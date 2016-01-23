#include "ArchivedFileReader.h"
#include <cassert>
#include <bzlib.h>
#include <cstring>
#include <memory>
#include <stdexcept>
#include "SubReader.h"

class ArchivedFileReader_Bzip2 : public ArchivedFileReader
{
public:
	ArchivedFileReader_Bzip2(std::shared_ptr<Reader> reader, int64_t size);
	~ArchivedFileReader_Bzip2();
	int32_t read(void* buf, int32_t count, uint64_t offset) override;
private:
	bz_stream m_strm;
};

ArchivedFileReader::ArchivedFileReader(std::shared_ptr<Reader> reader, int64_t size)
	: m_reader(reader), m_uncompressedLength(size),
	m_lastReadEnd(0), m_compressedOffset(0)
{
}

ArchivedFileReader::~ArchivedFileReader()
{
}

uint64_t ArchivedFileReader::length()
{
	return m_uncompressedLength;
}

Reader* ArchivedFileReader::create(const char* mimeType, std::shared_ptr<Reader> reader, int64_t length, int64_t offset, int64_t size)
{
	Reader* r = new SubReader(reader, offset, length);

	if (strcmp(mimeType, "application/octet-stream") == 0)
	{
		assert(length == size);
		return r;
	}
	else if (strcmp(mimeType, "application/x-gzip") == 0)
	{
		return new ArchivedFileReader_Deflate(std::shared_ptr<Reader>(r), size);
	}
	else if (strcmp(mimeType, "application/x-bzip2") == 0)
	{
		return new ArchivedFileReader_Bzip2(std::shared_ptr<Reader>(r), size);
	}
	else
	{
		delete r;
		return nullptr;
	}
}

ArchivedFileReader_Deflate::ArchivedFileReader_Deflate(std::shared_ptr<Reader> reader, int64_t size, bool gzip)
	: ArchivedFileReader(reader, size)
{
	memset(&m_strm, 0, sizeof(m_strm));
	
	if (!gzip)
		inflateInit(&m_strm);
	else
		inflateInit2(&m_strm, 16+MAX_WBITS);
}

ArchivedFileReader_Deflate::~ArchivedFileReader_Deflate()
{
	inflateEnd(&m_strm);
}

int32_t ArchivedFileReader_Deflate::read(void* buf, int32_t count, uint64_t offset)
{
	if (offset != m_lastReadEnd)
	{
		return -1;
	}

	uint32_t done;

	m_strm.next_out = (Bytef*) buf;
	m_strm.avail_out = count;

	while (m_strm.avail_out > 0)
	{
		int status;

		if (!m_strm.avail_in)
		{
			int32_t rd = m_reader->read(m_buffer, sizeof(m_buffer), m_compressedOffset);
			m_compressedOffset += rd;
			m_strm.next_in = (Bytef*) m_buffer;
			m_strm.avail_in = rd;
		}

		status = inflate(&m_strm, Z_NO_FLUSH);
		if (status < 0)
			throw std::runtime_error("inflate error");
		else if (status == Z_STREAM_END)
			break;
	}

	done = count - m_strm.avail_out;
	m_lastReadEnd += done;

	return done;
}

ArchivedFileReader_Bzip2::ArchivedFileReader_Bzip2(std::shared_ptr<Reader> reader, int64_t size)
	: ArchivedFileReader(reader, size)
{
	memset(&m_strm, 0, sizeof(m_strm));
	BZ2_bzDecompressInit(&m_strm, 0, 0);
}

ArchivedFileReader_Bzip2::~ArchivedFileReader_Bzip2()
{
	BZ2_bzDecompressEnd(&m_strm);
}

int32_t ArchivedFileReader_Bzip2::read(void* buf, int32_t count, uint64_t offset)
{
	if (offset != m_lastReadEnd)
	{
		return -1;
	}

	uint32_t done;

	m_strm.next_out = (char*) buf;
	m_strm.avail_out = count;

	while (m_strm.avail_out > 0)
	{
		int status;

		if (!m_strm.avail_in)
		{
			int32_t rd = m_reader->read(m_buffer, sizeof(m_buffer), m_compressedOffset);
			m_compressedOffset += rd;
			m_strm.next_in = m_buffer;
			m_strm.avail_in = rd;
		}

		status = BZ2_bzDecompress(&m_strm);
		if (status < 0)
			throw std::runtime_error("bunzip2 error");
		else if (status == BZ_STREAM_END)
			break;
	}

	done = count - m_strm.avail_out;
	m_lastReadEnd += done;

	return done;
}

