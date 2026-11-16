#include "Installer.h"
#include <stdexcept>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <cstring>
#include <sstream>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include "archive/XARArchive.h"
#include "archive/FileReader.h"
#include "archive/GzipReader.h"
#include "archive/PbzxReader.h"
#include "archive/CPIOArchive.h"
#include "DistributionXml.h"
#include "PackageInfoXml.h"
#include "ReceiptsDb.h"
#include "Uninstaller.h"
#include <CoreFoundation/CFDate.h>
#include <dirent.h>
#include <fcntl.h>

Installer::Installer(const char* pkg, const char* target)
: m_pkg(pkg)
{
	std::shared_ptr<FileReader> file;
	struct stat st;
	char origCwd[PATH_MAX];
	
	if (::stat(pkg, &st) != 0 || !S_ISREG(st.st_mode))
		throw std::runtime_error(std::string("Error - the package path specified was invalid: '") + pkg + "'.");
	
	if (target != nullptr)
	{
		const char* path = target;
		
		// DomainKey
		if (strcmp(target, "LocalSystem") == 0)
			path = "/";
		
		if (path[0] != '/' || ::stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
			throw std::runtime_error(std::string("Error trying to locate volume at ") + target);
		m_target = path;
	}
	
	file.reset(new FileReader(pkg));
	m_xar.reset(new XARArchive(file));
	
	getcwd(origCwd, sizeof(origCwd));
	m_originalCwd = origCwd;
}

void Installer::loadChoices()
{
	if (m_choicesLoaded)
		return;
	m_choicesLoaded = true;
	
	std::shared_ptr<Reader> distributionFile(m_xar->openFile("Distribution"));
	if (!distributionFile)
		return;
	
	m_distribution = std::make_shared<DistributionXml>(distributionFile);
	m_choices = m_distribution->choices();
	evaluateChoiceExpressions();
}

// The script context of the Distribution; null if JavaScriptCore is not available
ChoiceScript* Installer::script()
{
	if (!m_scriptTried)
	{
		m_scriptTried = true;
		m_script = ChoiceScript::create(m_target.empty() ? "/" : m_target);
		
		if (m_script && m_distribution)
		{
			for (const std::string& source : m_distribution->scripts())
			{
				std::string error;
				if (!m_script->addScript(source, error))
					std::cerr << "installer: Warning: a <script> of the package failed: " << error << std::endl;
			}
		}
	}
	return m_script.get();
}

// Evaluates the JavaScript expressions of selected/enabled/visible. A choice can
// refer to other choices, so evaluate until the states settle.
void Installer::evaluateChoiceExpressions()
{
	bool any = false;
	for (const DistributionXml::Choice& c : m_choices)
		any = any || !c.expressions.empty();
	
	ChoiceScript* js = any ? script() : nullptr;
	if (js == nullptr)
		return;
	
	for (int pass = 0; pass < 5; pass++)
	{
		std::vector<ChoiceScript::ChoiceState> states;
		bool changed = false;
		
		for (const DistributionXml::Choice& c : m_choices)
			states.push_back({ c.id, c.selected, c.enabled, c.visible });
		js->setChoices(states);
		
		for (DistributionXml::Choice& c : m_choices)
		{
			for (const auto& e : c.expressions)
			{
				bool value, *field = e.first == "selected" ? &c.selected : (e.first == "enabled" ? &c.enabled : &c.visible);
				std::string error;
				
				if (!js->evaluate(e.second, value, error))
				{
					c.evalErrors[e.first] = error;
					continue;
				}
				c.evalErrors.erase(e.first);
				if (*field != value)
				{
					*field = value;
					changed = true;
				}
			}
		}
		if (!changed)
			break;
	}
	
	// what could not be evaluated keeps its default
	for (DistributionXml::Choice& c : m_choices)
	{
		c.unevaluated.clear();
		for (const auto& e : c.evalErrors)
			c.unevaluated.push_back(e.first);
	}
}

bool Installer::isPackageActive(const std::string& pkgId, const DistributionXml::PkgRef& ref)
{
	if (ref.activeExpression.empty())
		return ref.active;
	
	ChoiceScript* js = script();
	std::string error;
	bool value;
	
	if (js != nullptr)
	{
		std::vector<ChoiceScript::ChoiceState> states;
		for (const DistributionXml::Choice& c : m_choices)
			states.push_back({ c.id, c.selected, c.enabled, c.visible });
		js->setChoices(states);
		
		if (js->evaluate(ref.activeExpression, value, error))
			return value && ref.active;
	}
	std::cout << "installer: Warning: package " << pkgId << ": the JavaScript in 'active' is not evaluated"
			<< (error.empty() ? " (JavaScriptCore is not available)" : " (" + error + ")") << "; treating the package as active\n";
	return ref.active;
}

// <installation-check> and <volume-check>: a script returning false stops the
// installation, unless it sets my.result.type to Warning.
void Installer::runChecks()
{
	if (!m_distribution)
		return;
	
	const std::pair<const char*, std::string> checks[] = {
		{ "installation-check", m_distribution->installationCheck() },
		{ "volume-check", m_distribution->volumeCheck() },
	};
	for (const auto& check : checks)
	{
		if (check.second.empty())
			continue;
		
		ChoiceScript* js = script();
		std::string error;
		bool passed;
		
		if (js == nullptr || !js->evaluate(check.second, passed, error))
		{
			std::cout << "installer: Warning: the " << check.first << " script is not evaluated"
					<< (error.empty() ? " (JavaScriptCore is not available)" : " (" + error + ")") << "; continuing\n";
			continue;
		}
		if (passed)
			continue;
		
		std::string message = js->resultMessage();
		if (js->resultType() == "Warning")
			std::cout << "installer: Warning: " << message << "\n";
		else
			throw std::runtime_error(message.empty() ? std::string("The ") + check.first + " of the package failed" : message);
	}
}

void Installer::applyChoiceChanges(const std::vector<ChoiceChange>& changes)
{
	loadChoices();
	
	for (const ChoiceChange& change : changes)
	{
		// A change naming a choice the package lacks is ignored
		for (DistributionXml::Choice& c : m_choices)
		{
			if (c.id != change.identifier)
				continue;
			
			if (change.toggle)
				c.selected = !c.selected;
			else if (change.attribute == "selected")
				c.selected = change.value != 0;
			else if (change.attribute == "enabled")
				c.enabled = change.value != 0;
			else if (change.attribute == "visible")
				c.visible = change.value != 0;
			else if (change.attribute == "customLocation")
				std::cerr << "installer: Warning: customLocation for choice " << c.id << " is not supported\n";
		}
	}
}

static void indent(std::ostream& out, int depth)
{
	for (int i = 0; i < depth; i++)
		out << "    ";
}

static std::string xmlText(const std::string& s)
{
	std::string rv;
	for (char c : s)
	{
		switch (c)
		{
			case '&': rv += "&amp;"; break;
			case '<': rv += "&lt;"; break;
			case '>': rv += "&gt;"; break;
			default: rv += c;
		}
	}
	return rv;
}

static void plistHeader(std::ostream& out)
{
	out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n";
}

static std::string fileURL(const std::string& path, const std::string& fragment)
{
	std::string url = "file://localhost";
	static const char* hex = "0123456789ABCDEF";
	
	for (unsigned char c : path)
	{
		if (isalnum(c) || strchr("/-._~", c))
			url += c;
		else
		{
			url += '%';
			url += hex[c >> 4];
			url += hex[c & 15];
		}
	}
	return url + "#" + fragment;
}

const DistributionXml::Choice* Installer::findChoice(const std::string& id) const
{
	for (const DistributionXml::Choice& c : m_choices)
	{
		if (c.id == id)
			return &c;
	}
	return nullptr;
}

// 1: the choice (or all of its subchoices) is selected, 0: none, -1: some of them
int Installer::selectionState(const DistributionXml::Line& line) const
{
	const DistributionXml::Choice* c = findChoice(line.choice);
	
	if (line.children.empty())
		return (c != nullptr && c->selected) ? 1 : 0;
	return selectionState(line.children);
}

int Installer::selectionState(const std::vector<DistributionXml::Line>& lines) const
{
	bool any = false, all = true;
	
	for (const DistributionXml::Line& l : lines)
	{
		int state = selectionState(l);
		any = any || state != 0;
		all = all && state == 1;
	}
	return all ? 1 : (any ? -1 : 0);
}

void Installer::printChoice(std::ostream& out, int depth, const std::string& id, const std::string& title,
		bool enabled, bool visible, int selected, size_t kbytes, const std::vector<std::string>& packages,
		const std::vector<DistributionXml::Line>& children)
{
	indent(out, depth); out << "<dict>\n";
	indent(out, depth + 1); out << "<key>childItems</key>\n";
	if (children.empty())
	{
		indent(out, depth + 1); out << "<array/>\n";
	}
	else
	{
		indent(out, depth + 1); out << "<array>\n";
		for (const DistributionXml::Line& l : children)
			printLine(out, depth + 2, l);
		indent(out, depth + 1); out << "</array>\n";
	}
	indent(out, depth + 1); out << "<key>choiceIdentifier</key>\n";
	indent(out, depth + 1); out << "<string>" << xmlText(id) << "</string>\n";
	indent(out, depth + 1); out << "<key>choiceIsEnabled</key>\n";
	indent(out, depth + 1); out << (enabled ? "<true/>" : "<false/>") << "\n";
	indent(out, depth + 1); out << "<key>choiceIsSelected</key>\n";
	indent(out, depth + 1); out << "<integer>" << selected << "</integer>\n";
	indent(out, depth + 1); out << "<key>choiceIsVisible</key>\n";
	indent(out, depth + 1); out << (visible ? "<true/>" : "<false/>") << "\n";
	indent(out, depth + 1); out << "<key>choiceSizeInKilobytes</key>\n";
	indent(out, depth + 1); out << "<integer>" << kbytes << "</integer>\n";
	indent(out, depth + 1); out << "<key>choiceTitle</key>\n";
	indent(out, depth + 1); out << "<string>" << xmlText(title) << "</string>\n";
	indent(out, depth + 1); out << "<key>pathsOfActivePackagesInChoice</key>\n";
	if (packages.empty())
	{
		indent(out, depth + 1); out << "<array/>\n";
	}
	else
	{
		indent(out, depth + 1); out << "<array>\n";
		for (const std::string& p : packages)
		{
			indent(out, depth + 2); out << "<string>" << xmlText(p) << "</string>\n";
		}
		indent(out, depth + 1); out << "</array>\n";
	}
	indent(out, depth); out << "</dict>\n";
}

void Installer::printLine(std::ostream& out, int depth, const DistributionXml::Line& line)
{
	const DistributionXml::Choice* c = findChoice(line.choice);
	std::vector<std::string> packages;
	size_t kbytes = 0;
	
	if (c == nullptr)
		return;
	
	char* real = realpath(m_pkg, nullptr);
	std::string pkgPath = real ? real : m_pkg;
	free(real);
	
	for (const std::string& pkgId : c->pkgref)
	{
		DistributionXml::PkgRef ref;
		
		if (!m_distribution->package(pkgId, ref) || !isPackageActive(pkgId, ref))
			continue;
		
		std::string sub = ref.path;
		if (!sub.empty() && sub[0] == '#')
			sub.erase(0, 1);
		packages.push_back(fileURL(pkgPath, sub));
		kbytes += ref.installKbytes;
	}
	
	printChoice(out, depth, c->id, c->title.empty() ? c->id : c->title, c->enabled, c->visible,
			selectionState(line), kbytes, packages, line.children);
}

void Installer::printChoicesXML(std::ostream& out)
{
	loadChoices();
	plistHeader(out);
	
	if (!m_distribution)
	{
		out << "<array/>\n</plist>\n";
		return;
	}
	
	std::vector<DistributionXml::Line> outline = m_distribution->outline();
	
	out << "<array>\n";
	printChoice(out, 1, "__ROOT_CHOICE_IDENT_" + m_distribution->title(), m_distribution->title(), true, true,
			selectionState(outline), 0, {}, outline);
	out << "</array>\n</plist>\n";
}

static void collectLines(const std::vector<DistributionXml::Line>& lines, std::vector<std::string>& ids)
{
	for (const DistributionXml::Line& l : lines)
	{
		ids.push_back(l.choice);
		collectLines(l.children, ids);
	}
}

void Installer::printChoiceChanges(std::ostream& out)
{
	loadChoices();
	plistHeader(out);
	
	std::vector<std::string> ids;
	if (m_distribution)
		collectLines(m_distribution->outline(), ids);
	
	if (ids.empty())
	{
		out << "<array/>\n</plist>\n";
		return;
	}
	
	out << "<array>\n";
	for (const std::string& id : ids)
	{
		const DistributionXml::Choice* c = findChoice(id);
		if (c == nullptr)
			continue;
		
		struct { const char* attribute; bool isBool; int value; } attrs[] = {
			{ "visible", true, c->visible }, { "enabled", true, c->enabled }, { "selected", false, c->selected },
		};
		for (const auto& a : attrs)
		{
			out << "    <dict>\n"
				"        <key>attributeSetting</key>\n"
				"        " << (a.isBool ? (a.value ? "<true/>" : "<false/>") : (a.value ? "<integer>1</integer>" : "<integer>0</integer>")) << "\n"
				"        <key>choiceAttribute</key>\n"
				"        <string>" << a.attribute << "</string>\n"
				"        <key>choiceIdentifier</key>\n"
				"        <string>" << xmlText(id) << "</string>\n"
				"    </dict>\n";
		}
	}
	out << "</array>\n</plist>\n";
}

static std::string stripPackageSuffix(std::string name)
{
	size_t slash = name.rfind('/');
	if (slash != std::string::npos)
		name.erase(0, slash + 1);
	if (name.length() > 4 && name.compare(name.length() - 4, 4, ".pkg") == 0)
		name.erase(name.length() - 4);
	return name;
}

std::vector<std::string> Installer::domains()
{
	std::shared_ptr<Reader> distributionFile(m_xar->openFile("Distribution"));
	
	// Only a distribution can enable domains
	if (!distributionFile)
		return {};
	return DistributionXml(distributionFile).domains();
}

std::vector<Installer::PackageSummary> Installer::packageSummaries()
{
	std::vector<PackageSummary> rv;
	std::string subdir;
	
	loadChoices();
	if (!m_distribution)
	{
		m_subdir = "";
		std::shared_ptr<PackageInfoXml> info = loadPackageInfo();
		rv.push_back({ stripPackageSuffix(m_pkg), info->installKBytes(), info->requiresAuthentication() });
		return rv;
	}
	
	for (const DistributionXml::Choice& c : m_choices)
	{
		if (!c.selected)
			continue;
		
		for (const std::string& pkgId : c.pkgref)
		{
			DistributionXml::PkgRef ref;
			
			if (!m_distribution->package(pkgId, ref))
				throw std::runtime_error("Cannot find <pkg-ref> with id '" + pkgId + "'");
			if (!isPackageActive(pkgId, ref))
				continue;
			
			subdir = ref.path;
			if (!subdir.empty() && subdir[0] == '#')
				subdir.erase(0, 1);
			
			m_subdir = subdir.c_str();
			std::shared_ptr<PackageInfoXml> info = loadPackageInfo();
			rv.push_back({ stripPackageSuffix(subdir), info->installKBytes(), info->requiresAuthentication() });
		}
	}
	m_subdir = "";
	return rv;
}

void Installer::installPackage()
{
	if (m_target.empty())
		throw std::runtime_error("Must specify -target");
	
	// If "Distribution" file exists, then read it and find out choices enabled by default (normally located in subdirectories)
	// If no "Distribution" file exists, then there are no subdirectories and we proceed with installation
	loadChoices();
	if (m_distribution)
	{
		const std::shared_ptr<DistributionXml>& distribution = m_distribution;
		
		runChecks();
		
		std::cout << "installer: Package name is " << distribution->title() << std::endl;
		std::cout << "installer: Installing at base path " << m_target << std::endl;
		
		for (const DistributionXml::Choice& c : m_choices)
		{
			if (!c.selected)
				continue;
			
			std::string choiceTitle = c.title;
			// Fallback to using id as a replacement for title
			if (choiceTitle.empty())
				choiceTitle = c.id;
			
			for (const std::string& attr : c.unevaluated)
			{
				if (attr.find("selected") == std::string::npos)
					continue; // enabled/visible only matter to a user interface
				auto err = c.evalErrors.find(attr);
				std::cout << "installer: Warning: choice " << choiceTitle << ": the JavaScript in '" << attr
						<< "' is not evaluated" << (err == c.evalErrors.end() ? " (JavaScriptCore is not available)" : " (" + err->second + ")")
						<< "; treating the choice as selected\n";
			}
			
			std::cout << "installer: Installing selected choice " << choiceTitle << std::endl;
			
			for (const std::string& pkgId : c.pkgref)
			{
				DistributionXml::PkgRef pkgref;
				
				if (!distribution->package(pkgId, pkgref))
				{
					std::stringstream ss;
					ss << "Cannot find <pkg-ref> with id '" << pkgId << "'";
					throw std::runtime_error(ss.str());
				}
				
				if (!isPackageActive(pkgId, pkgref))
					continue;
				
				const char* path = pkgref.path.c_str();
				if (*path == '#')
					path++;
				installPayload(path);
			}
		}
	}
	else
	{
		installPayload();
	}
	
	chdir(m_originalCwd.c_str());
	
	std::cout << "installer: Installation complete\n";
}

std::string Installer::getSubdirFilePath(const char* fileName)
{
	std::string path;
	
	path = m_subdir;
	if (!path.empty())
		path += '/';
	path += fileName;
	
	return path;
}

std::shared_ptr<PackageInfoXml> Installer::loadPackageInfo()
{
	std::string path;
	std::shared_ptr<Reader> pkgInfoFile;
	
	path = getSubdirFilePath("PackageInfo");
	
	pkgInfoFile.reset(m_xar->openFile(path));
	if (!pkgInfoFile)
		throw std::runtime_error(path + " not found in .pkg");
	
	return std::make_shared<PackageInfoXml>(pkgInfoFile);
}

std::string Installer::getInstallLocation()
{
	std::string path, installLocation;
	
	path = m_target;
	if (path[path.length()-1] != '/') path += '/';
	
	installLocation = m_pkgInfo->installLocation();
	if (installLocation != "/")
		path += installLocation;
	if (path[path.length()-1] != '/') path += '/';
	
	return path;
}

void Installer::mkParentDirs(const char *fileName, mode_t mode)
{
	// Create directories so that it's possible to create the specified file,
	// in a manner similiar to `mkdir -p`

	char *path = strdup(fileName);

	for (char *pos = path; pos != nullptr; pos = strchr(pos + 1, '/'))
	{
		if (pos == path)
			continue;

		*pos = '\0';

		if (::mkdir(path, mode) != 0)
		{
			if (errno != EEXIST)
			{
				std::stringstream ss;
				ss << "Cannot mkdir " << path << ": " << strerror(errno);
				throw std::runtime_error(ss.str());
			}
		}

		*pos = '/';
	}

	free(path);
}

void Installer::extractPayload(const char* payloadFileName, std::string destinationDir)
{
	std::string path, name;
	std::shared_ptr<Reader> cpioFile;
	std::unique_ptr<CPIOArchive> cpio;
	struct stat st;
	size_t basePathLen;
	
	path = getSubdirFilePath(payloadFileName);
	
	cpioFile.reset(m_xar->openFile(path));

	if (!cpioFile)
		return; // Not an error - packages don't need to have a Payload

	if (GzipReader::isGzip(cpioFile))
		cpioFile.reset(new GzipReader(cpioFile));
	else if (PbzxReader::isPbzx(cpioFile))
		cpioFile.reset(new PbzxReader(cpioFile));
	
	cpio.reset(new CPIOArchive(cpioFile));
	
	if (destinationDir[destinationDir.length()-1] != '/')
		destinationDir += '/';
	path = destinationDir;
	basePathLen = destinationDir.length();
	
	while (cpio->next(name, st, cpioFile))
	{
		// The final path is a concatenation of target, pkgInfo->installLocation()
		// and name.
		if (!isSafeArchivePath(name))
			throw std::runtime_error("Refusing to install payload entry outside the destination: " + name);

		path.resize(basePathLen);
		path += name;
		
		mkParentDirs(path.c_str(), st.st_mode & 0777);

		if (S_ISDIR(st.st_mode))
		{
			if (::mkdir(path.c_str(), st.st_mode & 0777) == 0)
			{
				// Only directories created by this package get its ownership;
				// pre-existing ones (/usr, /Applications...) are left alone.
				applyOwner(path.c_str(), st, -1);
			}
			else if (errno != EEXIST)
			{
				std::stringstream ss;
				ss << "Cannot mkdir " << path << ": " << strerror(errno);
				throw std::runtime_error(ss.str());
			}
		}
		else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
		{
			std::cout << "installer: Warning: not installing a device node "
					<< path << std::endl;
		}
		else if (S_ISLNK(st.st_mode))
		{
			// The link target is the entry's data; it is not limited in length.
			std::string target(st.st_size, '\0');
			
			if (st.st_size == 0 || cpioFile->read(&target[0], st.st_size, 0) != st.st_size)
				throw std::runtime_error("Cannot read the symlink target of " + path);
			
			::unlink(path.c_str());
			if (::symlink(target.c_str(), path.c_str()) != 0)
			{
				std::stringstream ss;
				ss << "Cannot create symlink " << path << ": " << strerror(errno);
				throw std::runtime_error(ss.str());
			}
			applyOwner(path.c_str(), st, AT_SYMLINK_NOFOLLOW);
			applyMtime(path.c_str(), st, AT_SYMLINK_NOFOLLOW);
		}
		else
		{
			extractFile(cpioFile, path.c_str(), &st);
			applyMtime(path.c_str(), st, 0);
		}
	}
}

bool Installer::isSafeArchivePath(const std::string& name)
{
	// A payload entry is relative to the destination directory, so no
	// component may climb out of it.
	size_t pos = 0;
	
	while (pos <= name.length())
	{
		size_t end = name.find('/', pos);
		if (end == std::string::npos)
			end = name.length();
		if (name.compare(pos, end - pos, "..") == 0)
			return false;
		pos = end + 1;
	}
	return true;
}

void Installer::applyOwner(const char* path, const struct stat& st, int flags)
{
	// Changing ownership is only possible for root; a normal user keeps the
	// files it extracts.
	if (geteuid() != 0)
		return;
	if (::fchownat(AT_FDCWD, path, st.st_uid, st.st_gid, flags) != 0)
		std::cerr << "installer: Warning: cannot set the owner of " << path << ": " << strerror(errno) << std::endl;
}

void Installer::applyMtime(const char* path, const struct stat& st, int flags)
{
	struct timespec times[2];
	
	times[0].tv_sec = times[1].tv_sec = st.st_mtime;
	times[0].tv_nsec = times[1].tv_nsec = 0;
	::utimensat(AT_FDCWD, path, times, flags);
}

void Installer::extractFile(std::shared_ptr<Reader> reader, const char* dest, const struct stat* st)
{
	std::unique_ptr<char[]> buf(new char[4096]);
	uint64_t length, done = 0;
	int fd;

	::unlink(dest);

	// O_EXCL|O_NOFOLLOW: never write through a link that appeared at this path
	fd = ::open(dest, O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW, 0600);
	if (fd == -1)
	{
		std::stringstream ss;
		ss << "Cannot create file " << dest << ": " << strerror(errno);
		throw std::runtime_error(ss.str());
	}

	length = reader->length();
	while (done < length)
	{
		int rd, wr;
		int32_t toread;

		toread = std::min<int32_t>(4096, length-done);
		rd = reader->read(buf.get(), toread, done);

		if (rd != toread)
			throw std::runtime_error("Short read in extractFile");

		wr = ::write(fd, buf.get(), rd);
		if (wr != rd)
		{
			std::stringstream ss;
			ss << "Cannot write file " << dest << ": " << strerror(errno);
			throw std::runtime_error(ss.str());
		}

		done += rd;
	}

	if (st != nullptr)
	{
		// The owner goes first: changing it clears setuid/setgid bits.
		if (geteuid() == 0 && ::fchown(fd, st->st_uid, st->st_gid) != 0)
			std::cerr << "installer: Warning: cannot set the owner of " << dest << ": " << strerror(errno) << std::endl;
		::fchmod(fd, st->st_mode & 07777);
	}

	::close(fd);
}

static std::string concatPaths(std::string p1, std::string p2)
{
	std::string joined = p1 + "/" + p2;
	size_t pos = 0;
	
	while ((pos = joined.find("//", pos)) != std::string::npos)
	{
		joined.replace(pos, 2, "/");
	}
	
	return joined;
}

void Installer::runInstallStepScript(const char* scriptName)
{
	std::string script;
	int exitCode;
	
	script = m_pkgInfo->script(scriptName);
		
	if (!script.empty())
	{
		exitCode = runScript(script.c_str(), m_scriptsDir.c_str(), m_installLocation.c_str());
		if (exitCode != 0)
			throwScriptExitCode(exitCode);
	}
}

int Installer::installPayload(const char* subdir)
{
	std::string identifier, bomPath;
	char scriptsDir[] = "/tmp/installerXXXXXX";
	ReceiptsDb::InstalledPackageInfo installedPackageInfo;
	bool isUpgrade = false;
	std::shared_ptr<Reader> bomFile;

	m_subdir = subdir;
	
	m_pkgInfo = loadPackageInfo();
	identifier = m_pkgInfo->identifier();
	m_installLocation = m_pkgInfo->installLocation();
	
	std::cout << "installer: Installing package " << identifier
			<< " version " << m_pkgInfo->version()
			<< " (" << m_pkgInfo->installKBytes() << " KB)\n";

	if (m_verboseR)
	{
		std::cout << "installer:PHASE:Installing package...\n";
		std::cout << "installer:%10.0\n";
	}
	
	if (m_xar->containsFile(getSubdirFilePath("Scripts")))
	{
		if (!mkdtemp(scriptsDir))
			throw std::runtime_error("Cannot create temporary directory for scripts");
		
		// extract scripts
		extractPayload("Scripts", scriptsDir);
		chdir(scriptsDir);
		m_scriptsDir = scriptsDir;
		
		// TODO: InstallationCheck and VolumeCheck scripts
		// http://s.sudre.free.fr/Stuff/PackageMaker_Howto.html

		if (m_verboseR)
		{
			std::cout << "installer:PHASE:Running installer actions (preflight)...\n";
			std::cout << "installer:%20.0\n";
		}
		
		runInstallStepScript(PackageInfoXml::SCRIPT_PREFLIGHT);
	}
	else
		scriptsDir[0] = 0; // no scripts
	
	if (ReceiptsDb::getInstalledPackageInfo(m_pkgInfo->identifier().c_str(), installedPackageInfo))
	{
		std::cout << "installer: Uninstalling previous version "
				<< installedPackageInfo.version << std::endl;
		if (m_verboseR)
			std::cout << "installer:PHASE:Uninstalling previous version...\n";
		
		// uninstall
		auto bom = ReceiptsDb::getInstalledPackageBOM(m_pkgInfo->identifier().c_str());
		if (bom)
		{
			uninstall(bom,
					installedPackageInfo.prefixPath.c_str());
		}
		isUpgrade = true;
	}
	
	if (scriptsDir[0] != 0)
	{
		if (m_verboseR)
			std::cout << "installer:PHASE:Running installer actions (preinstall)...\n";

		// run preinstall script
		runInstallStepScript(isUpgrade ? PackageInfoXml::SCRIPT_PREUPGRADE : PackageInfoXml::SCRIPT_PREINSTALL);
	}

	std::cout << "installer: Extracting files\n";
	if (m_verboseR)
	{
		std::cout << "installer:PHASE:Extracting files...\n";
		std::cout << "installer:%50.0\n";
	}
	extractPayload("Payload", getInstallLocation());
	
	if (scriptsDir[0] != 0)
	{
		if (m_verboseR)
		{
			std::cout << "installer:PHASE:Running installer actions (postinstall)...\n";
			std::cout << "installer:%80.0\n";
		}

		// run postinstall script
		runInstallStepScript(isUpgrade ? PackageInfoXml::SCRIPT_POSTUPGRADE : PackageInfoXml::SCRIPT_POSTINSTALL);
		runInstallStepScript(PackageInfoXml::SCRIPT_POSTFLIGHT);
		
		// cleanup
		removeDirectory(scriptsDir);
	}

	if (m_verboseR)
	{
		std::cout << "installer:PHASE:Finishing the installation...\n";
		std::cout << "installer:%90.0\n";
	}
	
	// Write a plist about installed package
	installedPackageInfo.identifier = identifier;
	installedPackageInfo.installDate = CFAbsoluteTimeGetCurrent();
	installedPackageInfo.installProcessName = "installer";
	installedPackageInfo.packageFileName = getPackageName();
	installedPackageInfo.prefixPath = concatPaths(m_target, m_installLocation);
	installedPackageInfo.version = m_pkgInfo->version();
	
	ReceiptsDb::putInstalledPackageInfo(identifier.c_str(), installedPackageInfo);
	
	// Copy BOM file
	bomPath = ReceiptsDb::getInstalledPackageBOMPath(identifier.c_str());
	bomFile.reset(m_xar->openFile(getSubdirFilePath("Bom")));
	if (bomFile)
		extractFile(bomFile, bomPath.c_str());

	if (m_verboseR)
	{
		std::cout << "installer:PHASE:The software was successfully installed.\n";
		std::cout << "installer:%100.0\n";
	}

	return 0;
}

const char* Installer::getPackageName()
{
	if (m_subdir[0])
		return m_subdir;
	else
	{
		const char* p;
		
		p = strrchr(m_pkg, '/');
		if (p != nullptr)
			return p+1;
		else
			return m_pkg;
	}
}

void Installer::throwScriptExitCode(int exitCode)
{
	std::stringstream ss;
	ss << "The script has terminated with exit code " << exitCode;
	throw std::runtime_error(ss.str());
}

void Installer::uninstall(std::shared_ptr<BOMStore> bom, const char* prefix)
{
	Uninstaller u(bom, prefix);
	u.uninstall();
}

void Installer::removeDirectory(const char* dir)
{
	DIR* d;
	struct dirent* ent;
	
	d = opendir(dir);
	if (!d)
		return;
	
	while ((ent = readdir(d)) != nullptr)
	{
		std::string file;
		
		if (strcmp(ent->d_name, "..") == 0 || strcmp(ent->d_name, ".") == 0)
			continue;
			
		file = dir;
		file += '/';
		file += ent->d_name;
		
		if (ent->d_type == DT_DIR)
			removeDirectory(file.c_str());
		else
			unlink(file.c_str());
	}
	
	closedir(d);
	rmdir(dir);
}

int Installer::runScript(const char* scriptName, const char* scriptTempDir, const char* installLocation)
{
	int pipes[2];
	int pid;
	
	pipe(pipes);
	
	pid = fork();
	
	if (!pid)
	{
		int err;
		
		close(pipes[0]);
		fcntl(pipes[1], F_SETFD, FD_CLOEXEC);

		setenv("COMMAND_LINE_INSTALL", "1", 1);
		setenv("PACKAGE_PATH", m_pkg, 1);
		setenv("DSTVOLUME", m_target.c_str(), 1);
		setenv("DSTROOT", installLocation, 1);
		setenv("INSTALLER_TEMP", scriptTempDir, 1);
		setenv("TMPDIR", scriptTempDir, 1);

		execl(scriptName, scriptName, m_pkg, installLocation, m_target.c_str(), nullptr);

		err = errno;
		write(pipes[1], &err, sizeof(err));
		_exit(err);
		
		__builtin_unreachable();
	}
	else
	{
		int status, err, rd;
		
		close(pipes[1]);
		
		rd = read(pipes[0], &err, sizeof(err));
		close(pipes[0]);
		
		waitpid(pid, &status, 0);
		
		if (rd > 0)
		{
			// problem running the script
			std::stringstream ss;
			ss << "Cannot run script " << scriptName << ": " << strerror(err);
			throw std::runtime_error(ss.str());
		}
		else
		{
			// get script's exit code
			return WEXITSTATUS(status);
		}
	}
}
