#ifndef GZIPREADER_H
#define GZIPREADER_H
#include "Reader.h"
#include <zlib.h>
#include <memory>

class GzipReader : public Reader
{
public:
	GzipReader(std::shared_ptr<Reader> reader);
	~GzipReader();
	int32_t read(void* buf, int32_t count, uint64_t offset) override;
	uint64_t length() override;
	
	static bool isGzip(std::shared_ptr<Reader> reader);
private:
	void readHeader(std::shared_ptr<Reader> reader);
	void skipString(std::shared_ptr<Reader> reader, uint32_t& inputPos);
private:
	std::shared_ptr<Reader> m_reader;
};

#endif

