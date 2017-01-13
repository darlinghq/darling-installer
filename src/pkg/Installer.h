#ifndef INSTALLER_H
#define INSTALLER_H
#include <memory>
#include <string>
#include <sys/types.h>
#include "archive/XARArchive.h"
#include "PackageInfoXml.h"
#include "bom/BOMStore.h"

class Installer
{
public:
	Installer(const char* pkg, const char* target);
	
	void installPackage();
private:
	std::shared_ptr<PackageInfoXml> loadPackageInfo();
	void mkParentDirs(const char *fileName, mode_t mode);
	void extractPayload(const char* payloadFileName, std::string destinationDir);
	int installPayload(const char* subdir = "");
	std::string getSubdirFilePath(const char* fileName);
	std::string getInstallLocation();
	void uninstall(std::shared_ptr<BOMStore> bom, const char* prefix);
	static void removeDirectory(const char* dir);
	static void extractFile(std::shared_ptr<Reader> reader, const char* dest);
	
	void runInstallStepScript(const char* scriptName);
	int runScript(const char* scriptName, const char* scriptTempDir, const char* installLocation);
	static void throwScriptExitCode(int exitCode);
	
	// Return package name used for plist
	const char* getPackageName();
private:
	const char* m_pkg;
	const char* m_target;
	std::shared_ptr<XARArchive> m_xar;
	std::string m_originalCwd;
	
	// State for the currently installed sub-package
	const char* m_subdir;
	std::shared_ptr<PackageInfoXml> m_pkgInfo;
	std::string m_scriptsDir, m_installLocation;
};

#endif /* INSTALLER_H */

