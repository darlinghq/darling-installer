#ifndef BOM_H
#define BOM_H
#include <stdint.h>

#define BOM_MAGIC1 0x424f4d53 // "BOMS"
#define BOM_MAGIC2 0x746f7265 // "tore"

#pragma pack(push, 1)

struct BOMLocator
{
	uint32_t offset;
	uint32_t length;
};

struct BOMHeader
{
	uint32_t magic1;
	uint32_t magic2;
	uint32_t version;
	uint32_t blockCount;
	BOMLocator blockTables; // "admin" area
	BOMLocator toc; // toc (BOMTocItem)
};

// List of items is preceded by uint32_t count
struct BOMTOCItem
{
	uint32_t blockID; // index into BOMBlocks::blockLocations
	uint8_t length;
	char string[];
};

#define BOM_TREEHDR_MAGIC 0x74726565 // "tree"
struct BOMTreeHeaderRecord
{
	uint32_t magic;
	uint32_t version;
	uint32_t firstChildID;
	uint32_t blockSize;
	uint32_t leafRecords;
	bool skip; // Meaning not entirely understood. Setting to false causes lsbom to skip the item?!
};


enum BOMNodeType
{
	kBOMIndexNode = 0,
	kBOMLeafNode = 1,
};
struct BOMNodeDescriptor
{
	uint16_t nodeType; // enum BOMNodeType
	uint16_t numRecords;
	uint32_t fLink;
	uint32_t bLink;
}; // followed by BOMNodeRecordHeaders

struct BOMNodeRecordHeader
{
	// kBOMLeafNode: points to BOMLeafDescriptor
	// kBOMIndexNode: points to BOMNodeDescriptor
	uint32_t childID;
	uint32_t recordID; // the record contains a BOMPathKey (for the Files tree)
};

enum BOMFileType : uint8_t
{
	kBOMFileTypeFile = 1,
	kBOMFileTypeDirectory = 2,
	kBOMFileTypeSymlink = 3,
	kBOMFileTypeDevice = 4
};

struct BOMLeafDescriptor
{
	uint32_t itemID;
	uint32_t recordID; // record ID of BOMPathRecord (for the Files tree)
};

struct BOMPathRecord
{
	BOMFileType fileType;
	uint8_t reserved1;
	uint16_t architecture; // cpu_type_t?
	uint16_t fileMode;
	uint32_t uid;
	uint32_t gid;
	uint32_t mtime;
	uint32_t size32;
	uint8_t reserved2;
	
	union
	{
		uint32_t crc32; // 0x58
		uint32_t devNode;
	};
	
	uint32_t linkNameLength;
	char linkName[1];
};

// Key type used in the Files tree
struct BOMPathKey
{
	uint32_t parentItemID; // related to BOMLeafDescriptor::itemID
	char name[1];
};

struct BOMInfo
{
	uint32_t version;
	uint32_t pathNodeCount; // not sure
	// followed by an array of BOMArchInfo
};

struct BOMArchInfo
{
	uint32_t cpu_type; // cpu_type_t
	uint32_t cpu_subtype; // ? cpu_subtype_t
	uint32_t i3;
	uint32_t i4;
};

#pragma pack(pop)

#endif /* BOM_H */

