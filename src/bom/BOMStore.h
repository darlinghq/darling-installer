#ifndef BOMSTORE_H
#define	BOMSTORE_H
#include "../archive/Reader.h"
#include "../archive/MemoryReader.h"
#include <vector>
#include <string>
#include <memory>
#include <stdint.h>
#include "bom.h"
#include "be.h"

class BOMStore
{
public:
	BOMStore(Reader* input);
	~BOMStore();
	
protected:
	// Checks the header and calls loadTOC()
	void loadStore();
	
	template <typename StructType>
	StructType* readData(const BOMLocator& loc)
	{
		return reinterpret_cast<StructType*>(&m_data[0] + be(loc.offset));
	}
	
	// Loads the table of contents which provides pointers to individual trees
	// (used by getTree)
	void loadTOC(const std::vector<uint8_t>& tocData);

	BOMNodeDescriptor* getNode(uint32_t nodeIndex);
	
	static const char* TREE_PATHS;
	static const char* TREE_SIZE64;
	BOMTreeHeaderRecord* getTree(const char* treeName);
private:
	struct TOCItem
	{
		uint32_t blockKey;
		std::string name;
	};
	
	std::vector<uint8_t> m_data;
	std::vector<TOCItem> m_toc;
	const BOMLocator* m_treeIndices = nullptr;
	const BOMTreeHeaderRecord* m_pathsTree = nullptr;
	const BOMTreeHeaderRecord* m_size64Tree = nullptr;
};

#endif	/* BOMSTORE_H */

