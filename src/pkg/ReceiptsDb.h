#ifndef RECEIPTSDB_H
#define RECEIPTSDB_H
#include <string>
#include <memory>
#include <set>
#include "bom/BOMStore.h"

namespace ReceiptsDb
{
	// Corresponds to a plist in RECEIPTS_DIR
	struct InstalledPackageInfo
	{
		std::string identifier, version, prefixPath;
		double installDate; // AbsoluteTime
		std::string packageFileName, installProcessName;
	};
	
	bool getInstalledPackageInfo(const char* identifier, InstalledPackageInfo& info);
	bool getInstalledPackageInfo(const char* identifier, std::string& plist);
	std::shared_ptr<BOMStore> getInstalledPackageBOM(const char* identifier);
	
	void putInstalledPackageInfo(const char* identifier, const InstalledPackageInfo& info);
	std::string getInstalledPackageBOMPath(const char* identifier);
	
	std::set<std::string> getInstalledPackages();
	void removePackage(const char* identifier);
}

#endif /* RECEIPTSDB_H */

