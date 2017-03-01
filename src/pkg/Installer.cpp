#include "Installer.h"
#include <stdexcept>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <iostream>
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
: m_pkg(pkg), m_target(target)
{
	std::shared_ptr<FileReader> file;
	struct stat st;
	char origCwd[PATH_MAX];
	
	// DomainKey
	if (strcmp(target, "LocalSystem") == 0)
		target = "/";
	else if (target[0] != '/')
		throw std::runtime_error("Invalid target path");
	
	if (::stat(target, &st) != 0)
		throw std::runtime_error("Invalid target path");
	
	if (!S_ISDIR(st.st_mode))
		throw std::runtime_error("Invalid target path; not a directory");
	
	file.reset(new FileReader(pkg));
	m_xar.reset(new XARArchive(file));
	
	getcwd(origCwd, sizeof(origCwd));
	m_originalCwd = origCwd;
}

void Installer::installPackage()
{
	std::shared_ptr<Reader> distributionFile;
	
	// If "Distribution" file exists, then read it and find out choices enabled by default (normally located in subdirectories)
	// If no "Distribution" file exists, then there are no subdirectories and we proceed with installation
	if (Reader* distr = m_xar->openFile("Distribution"))
	{
		std::unique_ptr<DistributionXml> distribution;
		std::vector<DistributionXml::Choice> choices;
		
		distributionFile.reset(distr);
		distribution.reset(new DistributionXml(distributionFile));
		
		std::cout << "installer: Package name is " << distribution->title() << std::endl;
		
		choices = distribution->choices();
		for (const DistributionXml::Choice& c : choices)
		{
			if (!c.selected)
				continue;
			
			std::cout << "installer: Installing selected choice " << c.title << std::endl;
			
			for (const std::string& pkgId : c.pkgref)
			{
				DistributionXml::PkgRef pkgref;
				
				if (!distribution->package(pkgId, pkgref))
				{
					std::stringstream ss;
					ss << "Cannot find <pkg-ref> with id '" << pkgId << "'";
					throw std::runtime_error(ss.str());
				}
				
				installPayload(pkgref.path.c_str() + 1);
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
		throw std::runtime_error(path + " not found in .pkg");

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
		path.resize(basePathLen);
		path += name;
		
		// std::cout << path << std::endl;

		mkParentDirs(path.c_str(), st.st_mode & 0777);

		if (S_ISDIR(st.st_mode))
		{
			if (::mkdir(path.c_str(), st.st_mode & 0777) != 0)
			{
				if (errno != EEXIST)
				{
					std::stringstream ss;
					ss << "Cannot mkdir " << path << ": " << strerror(errno);
					throw std::runtime_error(ss.str());
				}
			}
		}
		else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
		{
			std::cout << "installer: Warning: not installing a device node "
					<< path << std::endl;
		}
		else if (S_ISLNK(st.st_mode))
		{
			char target[256];
			int rd;
			
			rd = cpioFile->read(target, sizeof(target)-1, 0);
			target[rd] = '\0';
			
			::unlink(path.c_str());
			if (::symlink(target, path.c_str()) != 0)
			{
				std::stringstream ss;
				ss << "Cannot create symlink " << path << ": " << strerror(errno);
				throw std::runtime_error(ss.str());
			}
		}
		else
		{
			extractFile(cpioFile, path.c_str());
			::chmod(path.c_str(), st.st_mode);
		}
	}
}

void Installer::extractFile(std::shared_ptr<Reader> reader, const char* dest)
{
	std::unique_ptr<char[]> buf(new char[4096]);
	uint64_t length, done = 0;
	int fd;

	::unlink(dest);

	fd = ::open(dest, O_CREAT|O_WRONLY, 0600);
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
		
		runInstallStepScript(PackageInfoXml::SCRIPT_PREFLIGHT);
	}
	else
		scriptsDir[0] = 0; // no scripts
	
	if (ReceiptsDb::getInstalledPackageInfo(m_pkgInfo->identifier().c_str(), installedPackageInfo))
	{
		std::cout << "installer: Uninstalling previous version "
				<< installedPackageInfo.version << std::endl;
		
		// uninstall
		uninstall(ReceiptsDb::getInstalledPackageBOM(m_pkgInfo->identifier().c_str()),
				installedPackageInfo.prefixPath.c_str());
		isUpgrade = true;
	}
	
	if (scriptsDir[0] != 0)
	{	
		// run preinstall script
		runInstallStepScript(isUpgrade ? PackageInfoXml::SCRIPT_PREUPGRADE : PackageInfoXml::SCRIPT_PREINSTALL);
	}

	std::cout << "installer: Extracting files\n";
	extractPayload("Payload", getInstallLocation());
	
	if (scriptsDir[0] != 0)
	{
		// run postinstall script
		runInstallStepScript(isUpgrade ? PackageInfoXml::SCRIPT_POSTUPGRADE : PackageInfoXml::SCRIPT_POSTINSTALL);
		runInstallStepScript(PackageInfoXml::SCRIPT_POSTFLIGHT);
		
		// cleanup
		removeDirectory(scriptsDir);
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
	if (!bomFile)
		throw std::runtime_error(getSubdirFilePath("Bom") + " not found in .pkg");
	extractFile(bomFile, bomPath.c_str());

	return 0;
}

const char* Installer::getPackageName()
{
	if (m_subdir[0])
		return m_subdir;
	else
	{
		char* p;
		
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
		setenv("DSTVOLUME", m_target, 1);
		setenv("DSTROOT", installLocation, 1);
		setenv("INSTALLER_TEMP", scriptTempDir, 1);
		setenv("TMPDIR", scriptTempDir, 1);

		execl(scriptName, scriptName, m_pkg, installLocation, m_target, nullptr);

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
			
			// Since we don't have /usr/bin/perl yet, we ignore errors like this one.
			std::cerr << ss.str() << std::endl;
			// throw std::runtime_error(ss.str());
			return 0;
		}
		else
		{
			// get script's exit code
			return WEXITSTATUS(status);
		}
	}
}
