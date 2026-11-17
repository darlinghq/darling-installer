#ifndef INSTALLERDELEGATE_H
#define INSTALLERDELEGATE_H
#include <string>

// Where an Installer reports what it does and asks the user. The command line tool
// prints; the Installer application shows the messages in its window.
// The methods are called on the thread that runs the installation.
class InstallerDelegate
{
public:
	enum Level { Info, Warning };

	virtual ~InstallerDelegate() {}

	// A line of the installation log (without the "installer: " prefix)
	virtual void message(Level level, const std::string& text) = 0;
	// A new phase of the installation; percent is the overall progress, or negative if unchanged
	virtual void phase(const std::string& name, double percent) { (void) name; (void) percent; }
	// The package wants to run a program of its Resources: may the installer? May block.
	virtual bool confirmExternalProgram(const std::string& command) = 0;
};

// The command line behavior: messages as "installer: ..." lines, a confirmation question
// on the terminal, and (with -verboseR) PHASE and percentage lines.
class ConsoleInstallerDelegate : public InstallerDelegate
{
public:
	void enableVerboseR() { m_verboseR = true; }

	void message(Level level, const std::string& text) override;
	void phase(const std::string& name, double percent) override;
	bool confirmExternalProgram(const std::string& command) override;
private:
	bool m_verboseR = false;
};

#endif /* INSTALLERDELEGATE_H */
