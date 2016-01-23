#ifndef RECEIPTSDB_H
#define RECEIPTSDB_H
#include <string>
#include <memory>
#include <chrono>
#include "bom/BOMStore.h"

namespace ReceiptsDb
{
	// Corresponds to a plist in RECEIPTS_DIR
	struct InstalledPackageInfo
	{
		std::string identifier, version, prefixPath;
		time_t installDate; // (2016-01-21T10:53:33Z)
		std::string packageFileName, installProcessName;
		// TODO: InstallDate (2016-01-21T10:53:33Z)
		// TODO: PackageFileName (dos2unixinstall.pkg)
		// TODO: InstallProcessName (installer)
	};
	
	bool getInstalledPackageInfo(const char* identifier, InstalledPackageInfo& info);
	std::shared_ptr<BOMStore> getInstalledPackageBOM(const char* identifier);
	
	// TODO: install package info, install bom, ...
	void putInstalledPackageInfo(const char* identifier, const InstalledPackageInfo& info);
}

#endif /* RECEIPTSDB_H */

