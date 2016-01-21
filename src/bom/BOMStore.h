#ifndef BOMSTORE_H
#define	BOMSTORE_H
#include "../archive/Reader.h"
#include <vector>
#include <string>
#include <memory>
#include <stdint.h>
#include "bom.h"
#include "be.h"
#include "BOMArray.h"

class BOMFilesBTree;

class BOMStore : public std::enable_shared_from_this<BOMStore>
{
public:
	BOMStore(std::shared_ptr<Reader> input);
	~BOMStore();
	
	std::shared_ptr<BOMFilesBTree> getFilesTree();
	
protected:
	// Checks the header and calls loadTOC()
	void loadStore();
	
	template <typename StructType>
	StructType* readData(BOMLocator loc)
	{
		return reinterpret_cast<StructType*>(&m_data[0] + be(loc.offset));
	}
	
	// Loads the table of contents which provides pointers to individual trees
	// (used by getTree)
	void loadTOC(BOMArray<BOMTOCItem>* array);
	void loadBOMInfo();

	BOMNodeDescriptor* getNode(uint32_t nodeIndex);
	
	template <typename BlockType>
	BlockType* getBlockData(int blockID)
	{
		return readData<BlockType>(m_treeIndices[blockID]);
	}
	
	static const char* CONTENT_BOMINFO;
	static const char* CONTENT_HLINDEX;
	static const char* CONTENT_VINDEX;
	template <typename ContentType>
	ContentType* getContent(const char* treeName)
	{
		for (const TOCItem& item : m_toc)
		{
			if (item.name == treeName)
				return getBlockData<ContentType>(item.blockID);
		}
		return nullptr;
	}
	
	static const char* TREE_PATHS;
	static const char* TREE_SIZE64;
	BOMTreeHeaderRecord* getTree(const char* treeName);

private:
	struct TOCItem
	{
		uint32_t blockID;
		std::string name;
	};
	
	std::vector<uint8_t> m_data;
	std::vector<TOCItem> m_toc;
	BOMArray<BOMLocator> m_treeIndices;
	const BOMTreeHeaderRecord* m_pathsTree = nullptr;
	const BOMTreeHeaderRecord* m_size64Tree = nullptr;
	
	friend class BOMBTreeNode;
	friend class BOMFilesBTree;
};

#endif	/* BOMSTORE_H */

