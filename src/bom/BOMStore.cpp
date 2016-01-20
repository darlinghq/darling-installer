#include "BOMStore.h"
#include "bom.h"
#include <stdexcept>
#include <iostream>
#include "be.h"
#include "BOMArray.h"

const char* BOMStore::TREE_PATHS = "Paths";
const char* BOMStore::TREE_SIZE64 = "Size64";

BOMStore::BOMStore(Reader* input)
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
	std::unique_ptr<BOMArray<BOMTOCItem>> tocData;

	if (m_reader->read(&hdr, sizeof (hdr), 0) != sizeof (hdr))
		throw std::runtime_error("Short read reading the header");
	if (be(hdr.magic1) != BOM_MAGIC1 || be(hdr.magic2) != BOM_MAGIC2)
		throw std::runtime_error("Invalid BOMStore magic");

	// read TOC
	tocData.reset(new BOMArray<BOMTOCItem>(readData<void>(hdr.toc)));

	m_treeIndices = readData<BOMLocator>(hdr->blockTables);

	loadTOC(tocData);
	for (const TOCItem& item : m_toc)
	{
		std::cout << "TOC: " << item.name << std::endl;

		if (item.name == "Paths") {
			m_pathsTree = readData<BOMTreeHeaderRecord>(m_treeIndices[item.blockKey]);
		}
	}
}

void BOMStore::loadTOC(const std::vector<uint8_t>& tocData)
{
	BOMArray<BOMTOCItem> array(&tocData[0]);

	const BOMTOCItem* item;
	size_t pos = 0;

	for (uint32_t i = 0; i < array.size(); i++)
	{
		TOCItem tocItem;

		item = array.itemAtOffset(pos);
		tocItem.blockKey = be(item->blockKey);
		tocItem.name = std::string(item->string, item->length);

		m_toc.push_back(tocItem);
		pos += sizeof (*item) + item->length;
	}
}

BOMNodeDescriptor* BOMStore::getNode(uint32_t nodeIndex)
{
	return readData<BOMNodeDescriptor>(m_treeIndices[nodeIndex]);
}

BOMTreeHeaderRecord* BOMStore::getTree(const char* treeName)
{
	for (const TOCItem& item : m_toc)
	{
		if (item.name == treeName)
			return readData<BOMTreeHeaderRecord>(m_treeIndices[be(item.blockKey)]);
	}
	return nullptr;
}
