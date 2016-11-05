#include "PbzxReader.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include "be.h"

PbzxReader::PbzxReader(std::shared_ptr<Reader> reader)
: ArchivedFileReader(reader, 0)
{
	if (!isPbzx(reader))
		throw std::runtime_error("Not a pbzx stream");
	
	if (reader->read(&m_lastFlags, sizeof(m_lastFlags), 4) != sizeof(m_lastFlags))
		throw std::runtime_error("Short read in PbzxReader ctor");
	
	m_lastFlags = be64toh(m_lastFlags);
	m_compressedOffset = 12;
	
	if (lzma_stream_decoder(&m_strm, UINT64_MAX, LZMA_CONCATENATED) != LZMA_OK)
		throw std::runtime_error("lzma init error");
}

PbzxReader::~PbzxReader()
{
	lzma_end(&m_strm);
}

bool PbzxReader::isPbzx(std::shared_ptr<Reader> reader)
{
	uint32_t magic;
	
	if (reader->read(&magic, sizeof(magic), 0) != sizeof(magic))
		return false;
	if (memcmp((char*) &magic, "pbzx", 4) != 0)
		return false;
	
	return true;
}

int32_t PbzxReader::read(void* buf, int32_t count, uint64_t offset)
{
	if (offset != m_lastReadEnd)
		return -1;
	
	uint32_t done = 0;

	m_strm.next_out = static_cast<uint8_t*>(buf);
	m_strm.avail_out = count;

	while (m_strm.avail_out > 0)
	{
		int status;
		
		if (!m_strm.avail_in)
		{
			if (!m_remainingRunLength)
			{
				// read the next run's header
				
				if (!(m_lastFlags & (1 << 24)))
					break; // EOF
				
				if (m_compressedOffset == m_reader->length())
					break; // EOF
				
				if (m_reader->read(&m_lastFlags, sizeof(m_lastFlags), m_compressedOffset) != sizeof(m_lastFlags))
					throw std::runtime_error("Short read in PbzxReader");
				m_compressedOffset += sizeof(m_lastFlags);
				m_lastFlags = be64toh(m_lastFlags);
				
				if (m_reader->read(&m_remainingRunLength, sizeof(m_remainingRunLength), m_compressedOffset) != sizeof(m_remainingRunLength))
					throw std::runtime_error("Short read in PbzxReader");
				m_compressedOffset += sizeof(m_remainingRunLength);
				m_remainingRunLength = be64toh(m_remainingRunLength);
			}
			
			int32_t rd = m_reader->read(m_buffer, std::min<uint32_t>(sizeof(m_buffer), m_remainingRunLength), m_compressedOffset);
			
			m_compressedOffset += rd;
			m_remainingRunLength -= rd;
			
			m_strm.next_in = reinterpret_cast<const uint8_t*>(m_buffer);
			m_strm.avail_in = rd;
		}
		
		status = lzma_code(&m_strm, LZMA_RUN);
		if (status != LZMA_OK)
			throw std::runtime_error("lzma decompression error");
	}
	
	done = count - m_strm.avail_out;
	m_lastReadEnd += done;
	
	return done;
}
