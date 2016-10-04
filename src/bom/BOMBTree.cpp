#include "BOMBTree.h"
#include "be.h"
#include <iostream>
#include <algorithm>

BOMBTree::BOMBTree(std::shared_ptr<BOMStore> store, const BOMTreeHeaderRecord* treeHdr)
: m_store(store), m_header(treeHdr)
{
}

BOMBTree::~BOMBTree()
{
}

BOMBTreeNode BOMBTree::findLeafNode(const void* key, KeyComparator compare, bool wildcard)
{
	return traverseTree(be(m_header->firstChildID), key, compare, wildcard);
}

std::vector<BOMBTreeNode> BOMBTree::findLeafNodes(const void* indexKey, KeyComparator compare)
{
	std::vector<BOMBTreeNode> rv;
	BOMBTreeNode current = findLeafNode(indexKey, compare, true);
	
	rv.push_back(current);
	
	while (current.forwardLink() != 0)
	{
		const void* key;
		
		current = BOMBTreeNode(m_store, current.forwardLink());
		
		key = current.getRecordKey<void>(0);
		
		if (compare(key, indexKey) > 0)
			break;
		
		rv.push_back(current);
	}
	
	return rv;
}

BOMBTreeNode BOMBTree::traverseTree(uint32_t nodeIndex, const void* indexKey, KeyComparator comp, bool wildcard)
{
	BOMBTreeNode node(m_store, nodeIndex);
	
	switch (node.kind())
	{
		case kBOMIndexNode:
		{
			int position;

			if (wildcard)
			{
				auto it = std::lower_bound(node.begin<void>(), node.end<void>(), indexKey, [=](const void* keyA, const void* keyB) {
					return comp(keyA, keyB) < 0;
				});

				position = it.index() - 1;
			}
			else
			{
				auto it = std::upper_bound(node.begin<void>(), node.end<void>(), indexKey, [=](const void* keyA, const void* keyB) {
					return comp(keyA, keyB) < 0;
				});

				position = it.index() - 1;
			}
			
			if (position < 0)
				position = 0;
			
			// recurse down
			return traverseTree(node.getRecordDataBlockId(position), indexKey, comp, wildcard);
		}
		case kBOMLeafNode:
			return node;
		default:
			std::cerr << "Invalid node kind: " << int(node.kind()) << std::endl;
	}
	
	return BOMBTreeNode();
}
