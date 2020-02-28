#include "Uninstaller.h"
#include "ReceiptsDb.h"
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include "bom/BOMFilesBTree.h"

Uninstaller::Uninstaller(const char* package)
{
	ReceiptsDb::InstalledPackageInfo installedPackageInfo;
	
	if (!ReceiptsDb::getInstalledPackageInfo(package, installedPackageInfo))
		throw std::runtime_error("No such package is installed");
	
	m_prefix = installedPackageInfo.prefixPath;

	auto bom = ReceiptsDb::getInstalledPackageBOM(package);
	if (bom)
		m_tree = bom->getFilesTree();
}

Uninstaller::Uninstaller(std::shared_ptr<BOMStore> bom, const char* prefix)
: m_tree(bom->getFilesTree()), m_prefix(prefix)
{
}

void Uninstaller::uninstall()
{
	uninstall(BOMFilesBTree::kRootDirectory, m_prefix);
}

void Uninstaller::uninstall(uint32_t parentID, std::string path)
{
	if (!m_tree)
		return;
		
	// Walk the BOM tree and remove files
	std::map<uint32_t, BOMPathElement> files;
	const size_t pathLen = path.length();
	
	m_tree->listDirectory(parentID, files);
	
	for (const std::pair<uint32_t, BOMPathElement>& file : files)
	{
		path.resize(pathLen);
		path += '/';
		path += file.second.name();
		
		if (file.second.type() == kBOMFileTypeDirectory)
		{
			uninstall(file.first, path);
			
			// rmdir() is silent, we don't care if non-empty dirs cannot be removed
			::rmdir(path.c_str());
		}
		else
		{
			// std::cout << "Unlink: " << path << std::endl;
			if (::unlink(path.c_str()) == -1)
			{
				std::cerr << "Warning: cannot unlink " << path << std::endl;
			}
		}
	}
}
