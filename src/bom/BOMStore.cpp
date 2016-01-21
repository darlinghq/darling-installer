#include "BOMStore.h"
#include "bom.h"
#include <stdexcept>
#include <iostream>
#include "be.h"
#include "BOMArray.h"
#include "BOMFilesBTree.h"

const char* BOMStore::TREE_PATHS = "Paths";
const char* BOMStore::TREE_SIZE64 = "Size64";
const char* BOMStore::CONTENT_BOMINFO = "BomInfo";
const char* BOMStore::CONTENT_HLINDEX = "HLIndex"; // hardlink index
const char* BOMStore::CONTENT_VINDEX = "VIndex";

BOMStore::BOMStore(std::shared_ptr<Reader> input)
{
	m_data.resize(input->length());
	
	if (input->read(&m_data[0], input->length(), 0) != input->length())
		throw std::runtime_error("Short read of BOM data");
	
	loadStore();
}

BOMStore::~BOMStore()
{
}

void BOMStore::loadStore()
{
	BOMHeader* hdr;
	BOMArray<BOMTOCItem> tocData;

	hdr = reinterpret_cast<BOMHeader*>(&m_data[0]);
	if (be(hdr->magic1) != BOM_MAGIC1 || be(hdr->magic2) != BOM_MAGIC2)
		throw std::runtime_error("Invalid BOMStore magic");

	// read TOC
	tocData.set(readData<void>(hdr->toc));

	m_treeIndices.set(readData<BOMLocator>(hdr->blockTables));

	loadTOC(&tocData);
	// loadBOMInfo();
	
	// std::cout << "<tree>\n";
	// dumpPaths();
}

void BOMStore::loadTOC(BOMArray<BOMTOCItem>* array)
{
	const BOMTOCItem* item;
	size_t pos = 0;

	for (uint32_t i = 0; i < array->size(); i++)
	{
		TOCItem tocItem;

		item = array->itemAtOffset(pos);
		tocItem.blockID = be(item->blockID);
		tocItem.name = std::string(item->string, item->length);

		m_toc.push_back(tocItem);
		pos += sizeof (*item) + item->length;
	}
}

void BOMStore::loadBOMInfo()
{
	const BOMInfo* bomInfo;
	BOMArray<BOMArchInfo> elems;
	
	bomInfo = getContent<BOMInfo>(CONTENT_BOMINFO);
	elems.set(bomInfo+1); // array follows after struct BOMInfo
	
	for (uint32_t i = 0; i < elems.size(); i++)
	{
		const BOMArchInfo* elem;
		
		elem = elems.itemAtIndex(i);
		std::cout << "BOMInfoElement[" << i << "]:\n";
		std::cout << "\ti1 = " << be(elem->cpu_type) << std::endl;
		std::cout << "\ti2 = " << be(elem->cpu_subtype) << std::endl;
		std::cout << "\ti3 = " << be(elem->i3) << std::endl;
		std::cout << "\ti4 = " << be(elem->i4) << std::endl;
	}
}

/*
void BOMStore::dumpPaths()
{
	BOMTreeHeaderRecord* tree;
	BOMNodeDescriptor *rootNode, *node;
	
	tree = getTree(TREE_PATHS);
	rootNode = readData<BOMNodeDescriptor>(m_treeIndices[be(tree->firstChildID)]);
	
	//dumpNode(rootNode, be(tree->firstChildID), 1);
}

void BOMStore::dumpNode(BOMNodeDescriptor* node, int id, int indentLevel)
{
	BOMNodeRecordHeader* records;
	std::string indent(indentLevel, ' ');
	
	records = reinterpret_cast<BOMNodeRecordHeader*>(node+1);
	std::cout << indent << "<node " << "id='" << id
			<< "' type='" << be(node->nodeType)
			<< "' fLink='" << be(node->fLink)
			<< "' bLink='" << be(node->bLink) << "'>\n";
	
	for (int i = 0; i < be(node->numRecords); i++)
	{
		BOMPathKey* key;
		
		std::string indent(indentLevel+1, ' ');
		
		std::cout << indent << "<record id='" << be(records[i].recordID) << "'>\n";
		indent = std::string(indentLevel+2, ' ');
		
		key = readData<BOMPathKey>(m_treeIndices[be(records[i].recordID)]);
		std::cout << indent << "<parent>" <<be(key->parentRecordID) << "</parent>\n";
		std::cout << indent << "<string>" << key->name << "</string>\n";
		
		if (be(node->nodeType) == kBOMIndexNode)
		{
			BOMNodeDescriptor* childNode;
			
			childNode = readData<BOMNodeDescriptor>(m_treeIndices[be(records[i].childID)]);
			dumpNode(childNode, be(records[i].childID), indentLevel+2);
		}
		else // leaf
		{
			BOMPathRecord* pathRecord;
			
			pathRecord = readData<BOMPathRecord>(m_treeIndices[be(records[i].childID)]);
		}
		
		indent = std::string(indentLevel+1, ' ');
		std::cout << indent << "</record>\n";
	}
	
	std::cout << indent << "</node>\n";
}
*/

BOMNodeDescriptor* BOMStore::getNode(uint32_t nodeIndex)
{
	return readData<BOMNodeDescriptor>(m_treeIndices[nodeIndex]);
}

std::shared_ptr<BOMFilesBTree> BOMStore::getFilesTree()
{
	return std::make_shared<BOMFilesBTree>(shared_from_this(), getTree(TREE_PATHS));
}

BOMTreeHeaderRecord* BOMStore::getTree(const char* treeName)
{
	BOMTreeHeaderRecord* rec = getContent<BOMTreeHeaderRecord>(treeName);
	if (rec != nullptr && be(rec->magic) != BOM_TREEHDR_MAGIC)
		throw std::runtime_error("Invalid tree magic");
	return rec;
}
