#ifndef INSTALLER_H
#define INSTALLER_H
#include <memory>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include "archive/XARArchive.h"
#include "PackageInfoXml.h"
#include "bom/BOMStore.h"

class Installer
{
public:
	// target may be null when the package is only inspected
	Installer(const char* pkg, const char* target = nullptr);
	
	struct PackageSummary
	{
		std::string name;
		size_t installKBytes;
		bool requiresAuthentication;
	};
	// The packages installPackage() would install (all subpackages of a metapackage)
	std::vector<PackageSummary> packageSummaries();
	// Domains the package can be installed into, as accepted by -target
	std::vector<std::string> domains();
	
	void installPackage();
	void enableVerboseR() { m_verboseR = true; }
private:
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
};

#endif /* INSTALLER_H */

