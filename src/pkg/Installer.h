#ifndef INSTALLER_H
#define INSTALLER_H
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include "archive/XARArchive.h"
#include "PackageInfoXml.h"
#include "bom/BOMStore.h"
#include "DistributionXml.h"
#include "ChoicesXml.h"
#include "ChoiceScript.h"
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
	
	void installPackage();
	void enableVerboseR() { m_verboseR = true; }
private:
	void loadChoices();
	ChoiceScript* script();
	void evaluateChoiceExpressions();
	std::vector<ChoiceScript::ChoiceState> choiceStates();
	bool isPackageActive(const std::string& pkgId, const DistributionXml::PkgRef& ref);
	void runChecks();
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
	static void extractFile(std::shared_ptr<Reader> reader, const char* dest, const struct stat* st = nullptr);
	static bool isSafeArchivePath(const std::string& name);
	static void applyOwner(const char* path, const struct stat& st, int flags);
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

	bool m_verboseR = false;
	bool m_choicesLoaded = false;
	std::shared_ptr<DistributionXml> m_distribution; // null for a package without Distribution
	std::vector<DistributionXml::Choice> m_choices;
	std::unique_ptr<ChoiceScript> m_script; // null without JavaScriptCore
	bool m_scriptTried = false;
	bool m_installing = false; // an installation, not an information query
	std::string m_resourcesDir; // the package's Resources, extracted for system.run
	std::set<std::string> m_allowedPrograms; // programs the user agreed to run
};

#endif /* INSTALLER_H */

