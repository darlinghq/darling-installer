#include "BOMFilesBTree.h"
#include <cstring>

void BOMFilesBTree::listDirectory(uint32_t parentID, std::map<uint32_t, BOMPathElement>& contents)
{
	std::vector<BOMBTreeNode> leaves;
	BOMPathKey key;
	
	contents.clear();
	
	key.parentItemID = htobe32(parentID);
	key.name[0] = 0;
	
	leaves = findLeafNodes(&key, BOMFilesBTree::KeyComparator(keyComparator));
	
	for (BOMBTreeNode& leaf : leaves)
	{
		BOMBTreeNode::RecordIterator<BOMPathKey> it;
		
		it = std::lower_bound(leaf.begin<BOMPathKey>(), leaf.end<BOMPathKey>(), key.parentItemID,
			[](const BOMPathKey* k, uint32_t v) {
				return be(k->parentItemID) < be(v);
		});
		
		while (it != leaf.end<BOMPathKey>())
		{
			BOMPathKey* lkey;
			BOMLeafDescriptor* ldesc;
			BOMPathRecord* ldata;
			
			lkey = *it;
			
			if (lkey->parentItemID != key.parentItemID)
				break;
			
			ldesc = leaf.getRecordData<BOMLeafDescriptor>(it.index());
			ldata = m_store->getBlockData<BOMPathRecord>(be(ldesc->recordID));
			
			// TODO: size64
			contents[be(ldesc->itemID)] = BOMPathElement(lkey->name, ldata, 0);
			++it;
		}
	}
}

BOMBTree::CompareResult BOMFilesBTree::keyComparator(const BOMPathKey* k1, const BOMPathKey* k2)
{
	if (be(k2->parentItemID) > be(k2->parentItemID))
		return Greater;
	else if (be(k2->parentItemID) < be(k2->parentItemID))
		return Smaller;
	else
		return (BOMBTree::CompareResult) strcmp(k1->name, k2->name);
}
