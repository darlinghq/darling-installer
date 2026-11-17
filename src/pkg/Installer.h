#ifndef INSTALLER_H
#define INSTALLER_H
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include "archive/XARArchive.h"
#include "PackageInfoXml.h"
#include "bom/BOMStore.h"
#include "DistributionXml.h"
#include "ChoicesXml.h"
#include "ChoiceScript.h"
#include "InstallerDelegate.h"
#include <iosfwd>

class Installer
{
public:
	// target may be null when the package is only inspected
	Installer(const char* pkg, const char* target = nullptr);
	~Installer();
	
	struct PackageSummary
	{
		std::string name;
		size_t installKBytes;
		bool requiresAuthentication;
	};
	// The packages installPackage() would install (all subpackages of a metapackage)
	std::vector<PackageSummary> packageSummaries();
	// Install choices (a Distribution): the changes file is described in installer(8)
	void applyChoiceChanges(const std::vector<ChoiceChange>& changes);
	void printChoicesXML(std::ostream& out);
	void printChoiceChanges(std::ostream& out);
	
	// Domains the package can be installed into, as accepted by -target
	std::vector<std::string> domains();
	
	// ---- For a user interface -------------------------------------------------
	
	// The title of the package (of the Distribution; the name of a plain package)
	std::string title();
	
	// A <welcome>, <readme>, <license>, <conclusion> or <background> of the Distribution:
	// its file from the best localization found in Resources
	struct Resource
	{
		std::string data, mimeType;
	};
	bool resource(const char* element, Resource& resource);
	void setPreferredLanguages(const std::vector<std::string>& languages) { m_languages = languages; }
	
	// The destination: a directory, or the domain LocalSystem
	bool setTarget(const std::string& target);
	const std::string& target() const { return m_target; }
	
	// The choices as the outline shows them; setChoiceSelected re-evaluates the conditions
	// that depend on the choice
	struct ChoiceNode
	{
		std::string id, title, description;
		bool selected, enabled, visible;
		int selectionState; // 1: selected, 0: not, -1: some of the subchoices
		size_t installKBytes;
		std::vector<ChoiceNode> children;
	};
	std::vector<ChoiceNode> choiceTree();
	void setChoiceSelected(const std::string& id, bool selected);
	// Space the selected packages need, in kilobytes
	size_t requiredKilobytes();
	
	// The result of the installation-check and volume-check scripts
	struct CheckResult
	{
		enum Type { Pass, Warn, Fatal } type;
		std::string message;
	};
	CheckResult checkInstallation();
	
	void installPackage();
	// Where messages go and who is asked; the default is the command line (the caller keeps the delegate alive)
	void setDelegate(InstallerDelegate* delegate) { m_delegate = delegate; }
	void enableVerboseR() { m_console.enableVerboseR(); }
private:
	void loadChoices();
	ChoiceScript* script();
	void evaluateChoiceExpressions();
	std::vector<ChoiceScript::ChoiceState> choiceStates();
	bool isPackageActive(const std::string& pkgId, const DistributionXml::PkgRef& ref);
	void runChecks();
	CheckResult evaluateChecks();
	ChoiceNode nodeForLine(const DistributionXml::Line& line);
	void setSelectedWithChildren(const DistributionXml::Line& line, bool selected);
	void info(const std::string& text);
	void warn(const std::string& text);
	void phase(const std::string& name, double percent);
	bool runResource(const std::string& program, const std::vector<std::string>& args, int& exitCode);
	bool confirmExternalProgram(const std::string& program, const std::vector<std::string>& args);
	bool extractResources();
	const DistributionXml::Choice* findChoice(const std::string& id) const;
	int selectionState(const DistributionXml::Line& line) const;
	int selectionState(const std::vector<DistributionXml::Line>& lines) const;
	void printLine(std::ostream& out, int depth, const DistributionXml::Line& line);
	void printChoice(std::ostream& out, int depth, const std::string& id, const std::string& title,
			bool enabled, bool visible, int selected, size_t kbytes, const std::vector<std::string>& packages,
			const std::vector<DistributionXml::Line>& children);
	std::shared_ptr<PackageInfoXml> loadPackageInfo();
	void mkParentDirs(const char *fileName, mode_t mode);
	void extractPayload(const char* payloadFileName, std::string destinationDir);
	int installPayload(const char* subdir = "");
	std::string getSubdirFilePath(const char* fileName);
	std::string getInstallLocation();
	void uninstall(std::shared_ptr<BOMStore> bom, const char* prefix);
	static void removeDirectory(const char* dir);
	void extractFile(std::shared_ptr<Reader> reader, const char* dest, const struct stat* st = nullptr);
	static bool isSafeArchivePath(const std::string& name);
	void applyOwner(const char* path, const struct stat& st, int flags);
	static void applyMtime(const char* path, const struct stat& st, int flags);
	
	void runInstallStepScript(const char* scriptName);
	int runScript(const char* scriptName, const char* scriptTempDir, const char* installLocation);
	static void throwScriptExitCode(int exitCode);
	
	// Return package name used for plist
	const char* getPackageName();
private:
	const char* m_pkg;
	std::string m_target;
	std::shared_ptr<XARArchive> m_xar;
	std::string m_originalCwd;
	
	// State for the currently installed sub-package
	const char* m_subdir = "";
	std::shared_ptr<PackageInfoXml> m_pkgInfo;
	std::string m_scriptsDir, m_installLocation;

	ConsoleInstallerDelegate m_console;
	InstallerDelegate* m_delegate = &m_console;
	bool m_choicesLoaded = false;
	std::shared_ptr<DistributionXml> m_distribution; // null for a package without Distribution
	std::vector<DistributionXml::Choice> m_choices;
	std::map<std::string, bool> m_userSelection; // choices the user (or a changes file) decided
	std::vector<std::string> m_languages = { "en", "English", "Base" };
	std::unique_ptr<ChoiceScript> m_script; // null without JavaScriptCore
	bool m_scriptTried = false;
	bool m_installing = false; // an installation, not an information query
	std::string m_resourcesDir; // the package's Resources, extracted for system.run
	std::set<std::string> m_allowedPrograms; // programs the user agreed to run
};

#endif /* INSTALLER_H */

