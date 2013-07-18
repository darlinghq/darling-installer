#ifndef INFLATEREADER_H
#define INFLATEREADER_H
#include "Reader.h"
#include <zlib.h>

class InflateReader : public Reader
{
public:
	InflateReader(Reader* reader, uint64_t decompressedLength);
	virtual ~InflateReader();

	int32_t read(void* buf, int32_t count, uint64_t offset) override;
	uint64_t length() override;
private:
	Reader* m_reader;
	uint64_t m_decompressedLength;
};

#endif

