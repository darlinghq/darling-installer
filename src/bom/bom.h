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
	uint32_t blockKey; // index into BOMBlocks::blockLocations
	uint8_t length;
	char string[];
};

struct BOMTreeHeaderRecord
{
	uint32_t magic;
	uint32_t version;
	uint32_t firstChildKey;
	uint32_t blockSize;
	uint32_t leafRecords;
	bool skip; // Meaning not entirely understood. Setting to false causes lsbom to skip the item?!
};


enum // BOMNodeType
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
}; // followed by BOMNodeRecords

struct BOMNodeRecordHeader
{
	uint32_t childKey;
	uint32_t recordKey;
};

enum // BOMFileType
{
	kBOMFileTypeFile = 1,
	kBOMFileTypeDirectory = 2,
	kBOMFileTypeSymlink = 3,
	kBOMFileTypeDevice = 4
};

struct BOMPathRecord
{
	uint8_t fileType; // enum BOMFileType
	uint8_t reserved1;
	uint16_t architecture; // NXArch
	uint16_t fileMode;
	uint32_t uid;
	uint32_t gid;
	uint32_t mtime;
	uint32_t size32;
	uint8_t reserved2;
	
	union
	{
		uint32_t crc32;
		uint32_t devNode;
	};
	
	uint32_t linkNameLength;
	char linkName[1];
};

#pragma pack(pop)

#endif /* BOM_H */

