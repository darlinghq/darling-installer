#ifndef BOMBTREE_H
#define	BOMBTREE_H
#include "bom.h"
#include <vector>
#include "BOMBTreeNode.h"

class BOMStore;

class BOMBTree
{
public:
	BOMBTree(std::shared_ptr<BOMStore> store, const BOMTreeHeaderRecord* treeHdr);
	virtual ~BOMBTree();
	
	enum CompareResult { Smaller = -1, Equal = 0, Greater = 1 };
	typedef CompareResult (*KeyComparator)(const void* k1, const void* k2);
	
	BOMBTreeNode findLeafNode(const void* key, KeyComparator compare, bool wildcard = false);
	std::vector<BOMBTreeNode> findLeafNodes(const void* key, KeyComparator compare);
private:
	
	BOMBTreeNode traverseTree(uint32_t nodeIndex, const void* key, KeyComparator compare, bool wildcard);
protected:
	std::shared_ptr<BOMStore> m_store;
	const BOMTreeHeaderRecord* m_header;
};

#endif	/* BOMTREE_H */

