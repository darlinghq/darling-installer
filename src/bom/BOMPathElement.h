#ifndef BOMPATHELEMENT_H
#define BOMPATHELEMENT_H
#include "bom.h"
#include "be.h"
#include "BOMStore.h"
#include <memory>
#include <string>
#include <ctime>
#include <stdexcept>
#include <stdint.h>

// Wrapper for BOMPathRecord
class BOMPathElement
{
public:
	BOMPathElement() {}
	
	BOMPathElement(const char* name, BOMPathRecord* record, uint64_t size64)
	: m_name(name), m_size64(size64), m_record(*record)
	{
		if (record->fileType == kBOMFileTypeSymlink)
			m_link = std::string(record->linkName, be(record->linkNameLength));
	}
	
	const std::string& name() const { return m_name; }
	BOMFileType type() const { return m_record.fileType; }
	bool isDirectory() const { return type() == kBOMFileTypeDirectory; }
	
	/* cpu_type_t */
	uint16_t architecture() const { return be(m_record.architecture); }
	
	uint16_t mode() const { return be(m_record.fileMode); }
	uint32_t uid() const { return be(m_record.uid); }
	uint32_t gid() const { return be(m_record.gid); }
	time_t mtime() const { return be(m_record.mtime); }
	uint32_t size32() const { return be(m_record.size32); }
	
	uint64_t size64() const { return m_size64; }
	
	uint32_t crc32() const
	{
		if (type() != kBOMFileTypeFile)
			throw std::logic_error("Cannot call crc32() on a non-file");
		
		return be(m_record.crc32);
	}
	
	uint32_t deviceNode() const
	{
		if (type() != kBOMFileTypeDevice)
			throw std::logic_error("Cannot call deviceNode() on a non-device");
		
		return be(m_record.devNode);
	}
	
	const std::string& linkTarget() const
	{
		if (type() != kBOMFileTypeSymlink)
			throw std::logic_error("Cannot call linkTarget() on a non-symlink");
		return m_link;
	}
private:
	std::string m_name, m_link;
	uint64_t m_size64;
	BOMPathRecord m_record;
};

#endif /* BOMPATHELEMENT_H */

