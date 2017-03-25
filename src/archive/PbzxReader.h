#ifndef PBZXREADER_H
#define PBZXREADER_H
#include "ArchivedFileReader.h"
#include <stdint.h>
#include <lzma.h>

class PbzxReader : public ArchivedFileReader
{
public:
	PbzxReader(std::shared_ptr<Reader> reader);
	virtual ~PbzxReader();
	virtual int32_t read(void* buf, int32_t count, uint64_t offset) override;
	
	static bool isPbzx(std::shared_ptr<Reader> reader);
private:
	lzma_stream m_strm = LZMA_STREAM_INIT;
	uint64_t m_remainingRunLength = 0, m_lastFlags;
};

#endif /* PBZXREADER_H */

