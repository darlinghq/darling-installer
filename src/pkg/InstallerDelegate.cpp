#include "InstallerDelegate.h"
#include <iostream>
#include <unistd.h>
#include <cstdio>

void ConsoleInstallerDelegate::message(Level level, const std::string& text)
{
	if (level == Warning)
		std::cerr << "installer: Warning: " << text << std::endl;
	else
		std::cout << "installer: " << text << std::endl;
}

void ConsoleInstallerDelegate::phase(const std::string& name, double percent)
{
	if (!m_verboseR)
		return;
	std::cout << "installer:PHASE:" << name << "\n";
	if (percent >= 0)
		std::cout << "installer:%" << percent << ".0\n";
}

// Only a terminal can be asked
bool ConsoleInstallerDelegate::confirmExternalProgram(const std::string& command)
{
	if (!isatty(STDIN_FILENO) || !isatty(STDERR_FILENO))
	{
		std::cerr << "installer: Warning: the package wants to run '" << command
				<< "', which needs your confirmation on a terminal; not running it\n";
		return false;
	}

	std::cerr << "installer: The package wants to run the program '" << command
			<< "' with the privileges of this installer. Allow? [y/N] " << std::flush;

	char answer[16];
	if (fgets(answer, sizeof(answer), stdin) == nullptr)
		return false;
	return answer[0] == 'y' || answer[0] == 'Y';
}
