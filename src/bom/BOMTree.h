#ifndef BOMTREE_H
#define	BOMTREE_H
#include "bom.h"

class BOMStore;

class BOMTree
{
public:
	BOMTree(BOMStore* store, const BOMTreeHeaderRecord* treeHdr);
	virtual ~BOMTree();
private:
	BOMStore* m_store;
	const BOMTreeHeaderRecord* m_header;
};

#endif	/* BOMTREE_H */

