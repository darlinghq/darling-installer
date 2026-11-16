#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/mount.h>
#include "pkg/Installer.h"
#include "pkg/ChoicesXml.h"

namespace
{
	// QUIRK: a wrong command line (no arguments, an unknown option, a missing -pkg or -target where
	// one is required) prints the usage text and exits with status 255; other failures exit with 1.
	// The messages ("installer: invalid option -x", "installer: Error a package must be specified.",
	// "installer: Error -target parameter required") are as observed. Not documented. Evidence:
	// black-box observation on macOS 26.7.3 (tier 2).
	// Tests: "unknown option: exit status 255", "a missing -target is reported", "bad target: exit status 1"
	struct UsageError : std::runtime_error
	{
		using std::runtime_error::runtime_error;
	};

	enum Option
	{
		OPT_PKG = 1000, OPT_TARGET, OPT_VOLINFO, OPT_DOMINFO, OPT_PKGINFO, OPT_ALLOW_UNTRUSTED,
		OPT_VERS, OPT_HELP, OPT_VERBOSE, OPT_VERBOSER, OPT_PLIST, OPT_DUMPLOG, OPT_LANG,
		// accepted so that they can be reported as unsupported instead of as unknown
		OPT_FILE, OPT_CONFIG, OPT_QUERY, OPT_LISTISO,
		OPT_SHOW_CHOICES, OPT_APPLY_CHOICES, OPT_SHOW_CHOICES_AFTER,
		OPT_SHOW_CHOICE_CHANGES,
	};

	const struct option kOptions[] = {
		{ "pkg", required_argument, nullptr, OPT_PKG },
		{ "package", required_argument, nullptr, OPT_PKG },
		{ "target", required_argument, nullptr, OPT_TARGET },
		{ "tgt", required_argument, nullptr, OPT_TARGET },
		{ "volinfo", no_argument, nullptr, OPT_VOLINFO },
		{ "dominfo", no_argument, nullptr, OPT_DOMINFO },
		{ "pkginfo", no_argument, nullptr, OPT_PKGINFO },
		{ "allowUntrusted", no_argument, nullptr, OPT_ALLOW_UNTRUSTED },
		{ "vers", no_argument, nullptr, OPT_VERS },
		{ "help", no_argument, nullptr, OPT_HELP },
		{ "verbose", no_argument, nullptr, OPT_VERBOSE },
		{ "verboseR", no_argument, nullptr, OPT_VERBOSER },
		{ "plist", no_argument, nullptr, OPT_PLIST },
		{ "dumplog", no_argument, nullptr, OPT_DUMPLOG },
		{ "lang", required_argument, nullptr, OPT_LANG },
		{ "file", required_argument, nullptr, OPT_FILE },
		{ "config", no_argument, nullptr, OPT_CONFIG },
		{ "query", required_argument, nullptr, OPT_QUERY },
		{ "listiso", no_argument, nullptr, OPT_LISTISO },
		{ "showChoicesXML", no_argument, nullptr, OPT_SHOW_CHOICES },
		{ "showChoiceChangesXML", no_argument, nullptr, OPT_SHOW_CHOICE_CHANGES },
		{ "applyChoiceChangesXML", required_argument, nullptr, OPT_APPLY_CHOICES },
		{ "showChoicesAfterApplyingChangesXML", required_argument, nullptr, OPT_SHOW_CHOICES_AFTER },
		{ nullptr, 0, nullptr, 0 }
	};

	struct Volume
	{
		std::string name, mountPoint, deviceNode, fsKind;
		unsigned long long size;
	};

	struct Output
	{
		bool plist = false, verbose = false;
	};

	void showUsage()
	{
		std::cerr <<
			"Usage: installer [-help] [-dominfo] [-volinfo] [-pkginfo] [-allowUntrusted] [-dumplog]\n"
			"                 [-verbose | -verboseR] [-vers] [-config] [-plist]\n"
			"                 [-file <pathToFile>] [-lang <ISOLanguageCode>] [-listiso]\n"
			"                 [-showChoicesXML] [-applyChoiceChangesXML <pathToFile>]\n"
			"                 [-showChoicesAfterApplyingChangesXML <pathtoFile>]\n"
			"                 -pkg <pathToPackage>\n"
			"                 -target <[DomainKey|MountPoint]>\n";
	}

	void showHelp()
	{
		std::cout <<
			"Usage: installer [options...] -pkg <pathToPackage> -target <device>\n"
			"\n"
			"Options:\n"
			"    [-dominfo]        Display the domains the package can be installed into.\n"
			"    [-volinfo]        Display the volumes the package can be installed onto.\n"
			"    [-pkginfo]        Display the packages that would be installed\n"
			"                      (for metapackages, the subpackages).\n"
			"    [-plist]          Format -dominfo, -volinfo and -pkginfo as an XML plist.\n"
			"    [-verbose]        Display detailed information.\n"
			"    [-verboseR]       Display detailed information with simplified progress output.\n"
			"    [-allowUntrusted] Accepted; package signatures are not checked.\n"
			"    [-dumplog]        Accepted.\n"
			"    [-lang <code>]    Accepted; only meaningful for system installs.\n"
			"    [-showChoicesXML] Print the install choices of the package as XML.\n"
			"    [-showChoiceChangesXML]\n"
			"                      Print the choice states as a choice changes file.\n"
			"    [-applyChoiceChangesXML <file>]\n"
			"                      Apply a choice changes file before installing.\n"
			"    [-showChoicesAfterApplyingChangesXML <file>]\n"
			"                      Print the choice states after applying the file (needs -target).\n"
			"    [-vers]           Display the version of this tool.\n"
			"    [-help]           Display this help.\n"
			"\n"
			"Device:\n"
			"    The -target <device> parameter is a domain returned by -dominfo or\n"
			"    the mount point of a volume, e.g. /Volumes/Untitled.\n"
			"\n"
			"Examples:\n"
			"    installer -pkg InstallMe.pkg -target /\n"
			"    installer -volinfo -pkg InstallMe.pkg\n"
			"    installer -pkginfo -pkg InstallMe.pkg\n";
	}

	void showVersion()
	{
		std::cout << "    Darling installer version 0.1\n";
	}

	std::string xmlEscape(const std::string& s)
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

	// A plist array of dictionaries with a single string key each
	void printPlist(const char* key, const std::vector<std::string>& values)
	{
		std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
			"<plist version=\"1.0\">\n";
		if (values.empty())
			std::cout << "<array/>\n";
		else
		{
			std::cout << "<array>\n";
			for (const std::string& v : values)
			{
				std::cout << "    <dict>\n"
					"        <key>" << key << "</key>\n"
					"        <string>" << xmlEscape(v) << "</string>\n"
					"    </dict>\n";
			}
			std::cout << "</array>\n";
		}
		std::cout << "</plist>\n\n";
	}

	std::string formatSize(unsigned long long bytes)
	{
		char buf[64];
		if (bytes >= 1000ULL * 1000 * 1000)
			snprintf(buf, sizeof(buf), "%.2f GB", bytes / 1e9);
		else
			snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1e6);
		return buf;
	}

	// The root volume and the writable volumes mounted under /Volumes
	std::vector<Volume> mountedVolumes()
	{
		std::vector<Volume> rv;
		struct statfs* fs;
		int count = getmntinfo(&fs, MNT_NOWAIT);

		for (int i = 0; i < count; i++)
		{
			std::string mp = fs[i].f_mntonname;
			bool isRoot = (mp == "/");

			if (!isRoot && (mp.compare(0, 9, "/Volumes/") != 0 || (fs[i].f_flags & MNT_RDONLY)))
				continue;

			Volume v;
			v.mountPoint = mp;
			v.name = isRoot ? "System" : mp.substr(mp.rfind('/') + 1);
			v.deviceNode = fs[i].f_mntfromname;
			if (v.deviceNode.compare(0, 5, "/dev/") == 0)
				v.deviceNode.erase(0, 5);
			v.fsKind = fs[i].f_fstypename;
			v.size = (unsigned long long) fs[i].f_blocks * fs[i].f_bsize;
			rv.push_back(v);
		}
		return rv;
	}

	// QUIRK: -dominfo prints nothing (plain), an empty array (-plist) or "NoDomainsAvailable"
	// (-verbose) for a package that enables no domains, instead of always listing LocalSystem.
	// Not documented. Evidence: black-box observation on macOS 26.7.3 (tier 2).
	// Test: "-dominfo lists nothing for a package without domains"
	void printDomains(const std::vector<std::string>& domains, const Output& out)
	{
		if (out.plist)
			printPlist("Domain", domains);
		else if (out.verbose)
		{
			std::cout << "\nDomain information:\n";
			if (domains.empty())
				std::cout << "NoDomainsAvailable\n";
			for (const std::string& d : domains)
				std::cout << d << "\n";
		}
		else
		{
			for (const std::string& d : domains)
				std::cout << d << "\n";
		}
	}

	void printVolumes(const std::vector<Volume>& volumes, const Output& out)
	{
		if (out.plist)
		{
			std::vector<std::string> mountPoints;
			for (const Volume& v : volumes)
				mountPoints.push_back(v.mountPoint);
			printPlist("MountPoint", mountPoints);
		}
		else if (out.verbose)
		{
			std::cout << "\nVolume information:\n";
			for (const Volume& v : volumes)
			{
				std::cout << "\nStatus          : Enabled\n"
					"Volume          : " << v.name << "\n"
					"Size            : " << formatSize(v.size) << "\n"
					"MountPoint      : " << v.mountPoint << "\n"
					"Device Node     : " << v.deviceNode << "\n"
					"FileSystem Kind : " << v.fsKind << "\n"
					"Message         :\n";
			}
		}
		else
		{
			for (const Volume& v : volumes)
				std::cout << v.mountPoint << "\n";
		}
	}

	void printPackages(const std::vector<Installer::PackageSummary>& pkgs, const Output& out)
	{
		if (out.plist)
		{
			std::vector<std::string> names;
			for (const auto& p : pkgs)
				names.push_back(p.name);
			printPlist("Package", names);
		}
		else if (out.verbose)
		{
			std::cout << "\nPackage information:\n";
			for (const auto& p : pkgs)
			{
				std::cout << "\nPackage           : " << p.name << "\n"
					"Status            : Enabled\n"
					"Will Restart      : NO\n"
					"Size              : " << p.installKBytes << " KB\n"
					"Must Authenticate : " << (p.requiresAuthentication ? "YES" : "NO") << "\n"
					"Title             : " << p.name << "\n"
					"Description       :\n"
					"Message           :\n";
			}
		}
		else
		{
			for (const auto& p : pkgs)
				std::cout << p.name << "\n";
		}
	}

	std::vector<ChoiceChange> readChanges(const char* path)
	{
		std::vector<ChoiceChange> changes;
		
		if (!readChoiceChanges(path, changes))
			throw std::runtime_error(std::string("choices file '") + path + "' either could not be found or was malformed.");
		return changes;
	}

	[[noreturn]] void unsupported(const char* option)
	{
		throw std::runtime_error(std::string("Option -") + option + " is not supported");
	}
}

int main_installer(int argc, char** argv)
{
	const char* pkg = nullptr;
	const char* target = nullptr;
	const char* applyChanges = nullptr;
	const char* showAfterChanges = nullptr;
	bool showChoices = false, showChoiceChanges = false;
	bool volinfo = false, dominfo = false, pkginfo = false;
	bool verboseR = false;
	Output out;

	try
	{
		if (argc == 1)
			throw UsageError("");

		opterr = 0;
		optind = 1;
		while (true)
		{
			int c = getopt_long_only(argc, argv, "", kOptions, nullptr);

			if (c == -1)
				break;

			switch (c)
			{
				case OPT_PKG:
					if (pkg != nullptr)
						throw std::runtime_error("-pkg/-package options cannot be repeated");
					pkg = optarg;
					break;
				case OPT_TARGET: target = optarg; break;
				case OPT_VOLINFO: volinfo = true; break;
				case OPT_DOMINFO: dominfo = true; break;
				case OPT_PKGINFO: pkginfo = true; break;
				case OPT_ALLOW_UNTRUSTED: case OPT_DUMPLOG: case OPT_LANG: break;
				case OPT_VERS:
					showVersion();
					return 0;
				case OPT_HELP:
					showHelp();
					return 0;
				case OPT_VERBOSE: out.verbose = true; break;
				case OPT_VERBOSER: verboseR = true; break;
				case OPT_PLIST: out.plist = true; break;
				case OPT_FILE: unsupported("file");
				case OPT_CONFIG: unsupported("config");
				case OPT_QUERY: unsupported("query");
				case OPT_LISTISO: unsupported("listiso");
				case OPT_SHOW_CHOICES: showChoices = true; break;
				case OPT_SHOW_CHOICE_CHANGES: showChoiceChanges = true; break;
				case OPT_APPLY_CHOICES: applyChanges = optarg; break;
				case OPT_SHOW_CHOICES_AFTER: showAfterChanges = optarg; break;
				default:
					// '?': unknown option or missing argument
					throw UsageError(std::string("invalid option ") + argv[optind - 1]);
			}
		}

		if (optind < argc)
			throw UsageError("");

		if (pkg == nullptr)
			throw UsageError("Error a package must be specified.");
		if (int(volinfo) + int(dominfo) + int(pkginfo) > 1)
			throw std::runtime_error("Cannot specify -volinfo, -dominfo or -pkginfo at the same time");

		if (showChoices || showChoiceChanges || showAfterChanges != nullptr)
		{
			if (showAfterChanges != nullptr && target == nullptr)
				throw UsageError("Error -target parameter required");
			
			Installer installer(pkg, target);
			
			if (showAfterChanges != nullptr)
				installer.applyChoiceChanges(readChanges(showAfterChanges));
			if (showChoices)
				installer.printChoicesXML(std::cout);
			else
				installer.printChoiceChanges(std::cout);
		}
		else if (volinfo || dominfo || pkginfo)
		{
			Installer installer(pkg);

			if (volinfo)
				printVolumes(mountedVolumes(), out);
			else if (dominfo)
				printDomains(installer.domains(), out);
			else
				printPackages(installer.packageSummaries(), out);
		}
		else
		{
			if (target == nullptr)
				throw std::runtime_error("Must specify -target");

			Installer installer(pkg, target);

			if (verboseR)
				installer.enableVerboseR();
			if (applyChanges != nullptr)
				installer.applyChoiceChanges(readChanges(applyChanges));

			installer.installPackage();
		}

		return 0;
	}
	catch (const UsageError& e)
	{
		if (e.what()[0])
			std::cerr << "installer: " << e.what() << std::endl;
		showUsage();
		return 255;
	}
	catch (const std::exception& e)
	{
		std::cerr << "installer: " << e.what() << std::endl;
		return 1;
	}
}
