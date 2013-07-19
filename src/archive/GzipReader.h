#ifndef GZIPREADER_H
#define GZIPREADER_H
#include "Reader.h"
#include <zlib.h>

class GzipReader : public Reader
{
public:
	GzipReader(Reader* reader);
	~GzipReader();
	int32_t read(void* buf, int32_t count, uint64_t offset) override;
	uint64_t length() override;
private:
	void readHeader(Reader* reader);
	void skipString(Reader* reader, uint32_t& inputPos);
private:
	Reader* m_reader;
};

#endif

