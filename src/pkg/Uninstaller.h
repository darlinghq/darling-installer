#ifndef UNINSTALLER_H
#define UNINSTALLER_H
#include <memory>
#include <string>
#include "bom/BOMStore.h"

class Uninstaller
{
public:
	Uninstaller(const char* package);
	Uninstaller(std::shared_ptr<BOMStore> bom, const char* prefix);
	
	void uninstall();
private:
	void uninstall(uint32_t parentID, std::string path);
private:
	std::shared_ptr<BOMFilesBTree> m_tree;
	std::string m_prefix;
};

#endif /* UNINSTALLER_H */

