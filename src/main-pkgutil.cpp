#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include "pkg/Uninstaller.h"
#include "pkg/ReceiptsDb.h"
#include "bom/BOMPathElement.h"
#include "bom/BOMFilesBTree.h"
#include <cstring>
#include <stdexcept>
#include <regex>
#include <map>
#include <CoreFoundation/CFDate.h>

enum class ListMode { ListAll, ListFiles, ListDirs };

static void showHelp();
static void listPackages(bool plist, std::regex* re);
static void listFiles(const char* packageId, ListMode mode);
static void showPackageInfo(const char* packageId, bool plist);

int main_pkgutil(int argc, char** argv)
{
	const char* volume = "/";
	const char* pkgs_reStr = nullptr;
	const char* packageId = nullptr;
	std::regex pkgs_re;
	bool pkgs = false, pkgs_plist = false, files = false, forget = false, pkginfo = false,
			pkginfo_plist = false;
	bool only_files = false, only_dirs = false;
	
	struct option pkgutil_options[] = {
		{ "volume", required_argument, 0, 0},
		{ "pkgs", optional_argument, 0, 0},
		{ "pkgs-plist", no_argument, 0, 0},
		{ "packages", no_argument, 0, 0},
		{ "files", required_argument, 0, 0},
		{ "only-files", no_argument, 0, 0},
		{ "only-dirs", no_argument, 0, 0},
		{ "forget", required_argument, 0, 0},
		{ "pkg-info", required_argument, 0, 0},
		{ "pkg-info-plist", required_argument, 0, 0},
		{ nullptr, 0, 0, 0}
	};
	if (argc < 2)
		showHelp();
	
	try
	{
		while (true)
		{
			int c, option_index;

			c = getopt_long_only(argc, argv, "", pkgutil_options, &option_index);

			if (c != 0)
				break;

			switch (option_index)
			{
				case 0: // volume
					volume = optarg;
					break;
				case 1: // pkgs
					if (pkgs)
						throw std::runtime_error("--packages/--pkgs specified multiple times");
					pkgs = true;
					
					if (optarg)
						pkgs_reStr = optarg;
					break;
				case 2: // pkgs-plist
					pkgs_plist = true;
					break;
				case 3: // packages
					if (pkgs)
						throw std::runtime_error("--packages/--pkgs specified multiple times");
					pkgs = true;
					break;
				case 4: // files
					if (files)
						throw std::runtime_error("--files specified multiple times");
					files = true;
					packageId = optarg;
					break;
				case 5:
					only_files = true;
					break;
				case 6:
					only_dirs = true;
					break;
				case 7:
					if (forget)
						throw std::runtime_error("--forget specified multiple times");
					forget = true;
					packageId = optarg;
					break;
				case 8:
					if (pkginfo)
						throw std::runtime_error("--pkg-info specified multiple times");
					pkginfo = true;
					packageId = optarg;
					break;
				case 9:
					if (pkginfo_plist)
						throw std::runtime_error("--pkg-info-plist specified multiple times");
					pkginfo_plist = true;
					packageId = optarg;
					break;
			}
		}
		
		if (optind < argc)
			showHelp();
		if (int(pkgs) + int(pkgs_plist) + int(files) + int(forget) + int(pkginfo) + int(pkginfo_plist) > 1)
			throw std::runtime_error("Multiple commands specified");
		
		if (pkgs_reStr != nullptr)
			pkgs_re = std::regex(pkgs_reStr);
		
		if (pkgs || pkgs_plist)
		{
			if (strcmp(volume, "/") != 0)
				throw std::runtime_error("Only volume '/' is supported");
			listPackages(pkgs_plist, pkgs_reStr ? &pkgs_re : nullptr);
		}
		else if (files)
		{
			ListMode mode;
			if (only_files && only_dirs)
				throw std::runtime_error("--only-files and --only-dirs are mutually exclusive");
			if (only_files)
				mode = ListMode::ListFiles;
			else if (only_dirs)
				mode = ListMode::ListDirs;
			else
				mode = ListMode::ListAll;
			
			listFiles(packageId, mode);
		}
		else if (forget)
		{
			ReceiptsDb::removePackage(packageId);
		}
		else if (pkginfo || pkginfo_plist)
		{
			showPackageInfo(packageId, pkginfo_plist);
		}
		else
			showHelp();
		
		return 0;
	}
	catch (const std::regex_error& e)
	{
		std::cerr << "An invalid regex expression was specified.\n";
		return 1;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}

void showHelp()
{
	std::cerr << "Usage: pkgutil [options] [commands]\n\n";
	std::cerr << "OPTIONS:\n"
			"\t--volume path          Perform operations on specified volume ('/' by default).\n"
			"\t--only-files           List only files (use with --files command).\n"
			"\t--only-dirs            List only directories (use with --files command).\n"
			"\n";
	std::cerr << "COMMANDS:\n"
			"\t--packages, --pkgs     List all installed package IDs.\n"
			"\t--pkgs=regex           List all installed package IDs that match the given regex.\n"
			"\t--pkgs-plist           List all installed package IDs, printed in plist format.\n"
			"\t--files pkgId          List files installed by package 'pkgId'.\n"
			"\t--forget pkgId         Remove package 'pkgId' from installer's database.\n"
			"\t--pkg-info pkgId       Show installation information on package 'pkgId'.\n"
			"\t--pkg-info-plist pkgId Show installation information on package 'pkgId' in plist format.\n";
	exit(1);
}

void listPackages(bool plist, std::regex* re)
{
	std::set<std::string> pkgs;
	
	pkgs = ReceiptsDb::getInstalledPackages();
	if (plist)
	{
		std::cout << R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>)";
	}
	
	for (const std::string& pkg : pkgs)
	{
		if (plist)
			std::cout << "\t<string>";
		
		if (!plist && re != nullptr)
		{
			if (!std::regex_match(pkg, *re))
				continue;
		}
		
		std::cout << pkg;
		
		if (plist)
			std::cout << "</string>";
		std::cout << std::endl;
	}
	
	if (plist)
		std::cout << "</array>\n</plist>";
}

static void listFiles(std::shared_ptr<BOMFilesBTree> tree, std::string path, ListMode mode, uint32_t parentId)
{
	std::map<uint32_t, BOMPathElement> files;
	const size_t pathLen = path.length();
	
	tree->listDirectory(parentId, files);
	
	for (const std::pair<uint32_t, BOMPathElement>& file : files)
	{
		path.resize(pathLen);
		path += '/';
		path += file.second.name();
		
		if (file.second.type() == kBOMFileTypeDirectory)
		{
			if (mode == ListMode::ListAll || mode == ListMode::ListDirs)
				std::cout << path << std::endl;
			listFiles(tree, path, mode, file.first);
		}
		else
		{
			if (mode == ListMode::ListAll || mode == ListMode::ListFiles)
				std::cout << path << std::endl;
		}
	}
}

void listFiles(const char* packageId, ListMode mode)
{
	ReceiptsDb::InstalledPackageInfo installedPackageInfo;
	std::shared_ptr<BOMFilesBTree> tree;
	
	if (!ReceiptsDb::getInstalledPackageInfo(packageId, installedPackageInfo))
		throw std::runtime_error("No such package is installed");
	
	tree = ReceiptsDb::getInstalledPackageBOM(packageId)->getFilesTree();
	
	// Remove / at end to avoid double /
	if (!installedPackageInfo.prefixPath.empty() && installedPackageInfo.prefixPath[installedPackageInfo.prefixPath.length()-1] == '/')
		installedPackageInfo.prefixPath.resize(installedPackageInfo.prefixPath.length()-1);
	
	listFiles(tree, installedPackageInfo.prefixPath, mode, BOMFilesBTree::kRootDirectory);
}

void showPackageInfo(const char* packageId, bool plist)
{
	if (!plist)
	{
		ReceiptsDb::InstalledPackageInfo installedPackageInfo;

		if (!ReceiptsDb::getInstalledPackageInfo(packageId, installedPackageInfo))
			throw std::runtime_error("No such package is installed");

		std::cout << "package-id: " << installedPackageInfo.identifier << std::endl;
		std::cout << "version: " << installedPackageInfo.version << std::endl;
		std::cout << "volume: " << "/" << std::endl;
		std::cout << "location: " << installedPackageInfo.prefixPath << std::endl;
		std::cout << "install-time: " << time_t(installedPackageInfo.installDate+kCFAbsoluteTimeIntervalSince1970) << std::endl;
	}
	else
	{
		std::string plist;
		
		if (!ReceiptsDb::getInstalledPackageInfo(packageId, plist))
			throw std::runtime_error("No such package is installed");
		
		std::cout << plist;
	}
}
