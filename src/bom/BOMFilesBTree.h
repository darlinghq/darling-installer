#ifndef BOMFILESBTREE_H
#define BOMFILESBTREE_H
#include "BOMBTree.h"
#include "bom.h"
#include <map>
#include <string>
#include "BOMPathElement.h"

class BOMFilesBTree : protected BOMBTree
{
public:
	using BOMBTree::BOMBTree;
	enum
	{
		// Listing this directory will output a single directory named '.' (the root)
		kAbstractRootParent = 0,
		// Refers to the root ('.') directory
		kRootDirectory
	};
	
	void listDirectory(uint32_t parentID, std::map<uint32_t, BOMPathElement>& contents);
	
	static CompareResult keyComparator(const BOMPathKey* k1, const BOMPathKey* k2);
};

#endif /* BOMFILESBTREE_H */

