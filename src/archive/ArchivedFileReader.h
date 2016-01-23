#ifndef ARCHIVEDFILEREADER_H
#define ARCHIVEDFILEREADER_H
#include "Reader.h"
#include <memory>
#include <zlib.h>

class ArchivedFileReader : public Reader
{
protected:
	ArchivedFileReader(std::shared_ptr<Reader> reader, int64_t size);
	virtual ~ArchivedFileReader();
public:
	uint64_t length() override;
	static Reader* create(const char* mimeType, std::shared_ptr<Reader> reader, int64_t length, int64_t offset, int64_t size);
protected:
	std::shared_ptr<Reader> m_reader;
	int64_t m_uncompressedLength;
	int64_t m_lastReadEnd, m_compressedOffset;
	char m_buffer[4096];
};

class ArchivedFileReader_Deflate : public ArchivedFileReader
{
public:
	ArchivedFileReader_Deflate(std::shared_ptr<Reader> reader, int64_t size, bool gzip = false);
	~ArchivedFileReader_Deflate();
	int32_t read(void* buf, int32_t count, uint64_t offset) override;
private:
	z_stream m_strm;
};

#endif

