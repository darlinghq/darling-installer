#include <iostream>
#include <cstdlib>
#include "pkg/Uninstaller.h"
#include "pkg/ReceiptsDb.h"
#include <cstring>
#include <stdexcept>

static void showHelp();

int main_uninstaller(int argc, char** argv)
{
	if (argc != 2)
		showHelp();
	
	try
	{
		if (strcmp(argv[1], "-list") == 0)
		{
			std::set<std::string> pkgs;
			
			pkgs = ReceiptsDb::getInstalledPackages();
			for (const std::string& pkg : pkgs)
				std::cout << pkg << std::endl;
		}
		else
		{
			Uninstaller u(argv[1]);
			u.uninstall();
			ReceiptsDb::removePackage(argv[1]);
			
			std::cout << "Package " << argv[1] << " has been successfully uninstalled.\n";
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
	std::cerr << "Usage: uninstaller [-list] <package>\n\n"
			" -list: Lists all packages installed\n\n"
			"In default operation mode, uninstalls the given `package'.\n";
	exit(1);
}
