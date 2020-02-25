#include <getopt.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include "pkg/Installer.h"

static void showHelp();
static void showVersion();

int main_installer(int argc, char** argv)
{
	const char* pkg = nullptr;
	const char* target = nullptr;
	const char* file = nullptr;
	bool volinfo = false, dominfo = false, pkginfo = false;
	bool verboseR = false;
	
	struct option installer_options[] = {
		{ "pkg", required_argument, 0, 0},
		{ "package", required_argument, 0, 0},
		{ "volinfo", no_argument, 0, 0},
		{ "dominfo", no_argument, 0, 0},
		{ "pkginfo", no_argument, 0, 0},
		{ "allowUntrusted", no_argument, 0, 0},
		{ "vers", no_argument, 0, 0},
		{ "target", required_argument, 0, 0},
		{ "file", required_argument, 0, 0},
		{ "help", no_argument, 0, 0},
		{ "verboseR", no_argument, 0, 0},
		{ nullptr, 0, 0, 0}
	};
	
	if (argc == 1)
		showHelp();
	
	try
	{
		while (true)
		{
			int c, option_index;

			c = getopt_long_only(argc, argv, "", installer_options, &option_index);

			if (c != 0)
				break;

			switch (option_index)
			{
				case 0: // pkg
				case 1: // package
					if (pkg != nullptr)
						throw std::runtime_error("-pkg/-package options cannot be repeated");
					pkg = optarg;
					break;
				case 2:
					volinfo = true;
					break;
				case 3:
					dominfo = true;
					break;
				case 4:
					pkginfo = true;
					break;
				case 5: // allowUntrusted
					break;
				case 6: // vers
					showVersion();
					break;
				case 7:
					target = optarg;
					break;
				case 8:
					file = optarg;
					break;
				case 9:
					showHelp();
					break;
				case 10:
					verboseR = true;
					break;
			}
		}
		
		if (optind < argc)
			showHelp();
		
		if (pkg == nullptr)
			throw std::runtime_error("-package argument is required");
		if (int(volinfo) + int(dominfo) + int(pkginfo) > 0)
			throw std::runtime_error("Cannot specify -volinfo, -dominfo or -pkginfo at the same time");
		
		if (volinfo)
		{
			// throw std::runtime_error("-volinfo is not implemented yet");
			// TODO
			std::cout << "/\n";
		}
		else if (dominfo)
		{
			// throw std::runtime_error("-dominfo is not implemented yet");
			std::cout << "LocalSystem\n";
		}
		else if (pkginfo)
		{
			throw std::runtime_error("-pkginfo is not implemented yet");
		}
		else if (file != nullptr)
		{
			throw std::runtime_error("-file is not implemented yet");
		}
		else
		{
			if (target == nullptr)
				throw std::runtime_error("Must specify -target");
			
			Installer installer(pkg, target);
			
			if (verboseR)
				installer.enableVerboseR();

			installer.installPackage();
		}
		
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
}

static void showHelp()
{
	std::cerr << "Usage: installer [-help] [-dominfo] [-volinfo] [-pkginfo] [-allowUntrusted]\n"
				 "                 -pkg <pathToPackage>\n"
                 "                 -target <[DomainKey|MountPoint]>\n";
	exit(1);
}

static void showVersion()
{
	std::cerr << "Darling installer version 0.1\n";
	exit(0);
}

