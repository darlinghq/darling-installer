#include "GzipReader.h"
#include <stdexcept>
#include <cstring>
#include <endian.h>
#include "gzip.h"
#include "ArchivedFileReader.h"
#include "SubReader.h"

GzipReader::GzipReader(Reader* reader)
	: m_reader(nullptr)
{
	readHeader(reader);
}

GzipReader::~GzipReader()
{
	delete m_reader;
}

bool GzipReader::isGzip(Reader* reader)
{
	gzip_header hdr;
	
	if (reader->read(&hdr, sizeof(hdr), 0) != sizeof(hdr))
		return false;
	if (hdr.id1 != 0x1f || hdr.id2 != 0x8b)
		return false;
	
	return true;
}

void GzipReader::readHeader(Reader* reader)
{
	gzip_header hdr;
	uint16_t extraLen;
	uint32_t uncompressedLength;
	uint32_t inputPos;

	if (reader->read(&hdr, sizeof(hdr), 0) != sizeof(hdr))
		throw std::runtime_error("Cannot read gzip header");
	if (hdr.id1 != 0x1f || hdr.id2 != 0x8b)
		throw std::runtime_error("Invalid gzip header");

	inputPos = sizeof(hdr);

	if (hdr.flags & GZIP_FEXTRA)
	{
		if (reader->read(&extraLen, 2, inputPos) != 2)
			throw std::runtime_error("Read error");
		
		inputPos += 2 + le16toh(extraLen);
	}

	if (hdr.flags & GZIP_FNAME)
		skipString(reader, inputPos);

	if (hdr.flags & GZIP_FCOMMENT)
		skipString(reader, inputPos);

	if (hdr.flags & GZIP_FHCRC)
		inputPos += 2;

	if (reader->read(&uncompressedLength, 4, reader->length()-4) != 4)
		throw std::runtime_error("Read error");
	uncompressedLength = le32toh(uncompressedLength);

	m_reader = new ArchivedFileReader_Deflate(
			new SubReader(reader, inputPos, reader->length()-inputPos-8),
			uncompressedLength
	);
}

void GzipReader::skipString(Reader* reader, uint32_t& inputPos)
{
	char c;

	do
	{
		if (reader->read(&c, 1, inputPos++) != 1)
			throw std::runtime_error("Read error");
	}
	while (c != 0);
}

int32_t GzipReader::read(void* buf, int32_t count, uint64_t offset)
{
	return m_reader->read(buf, count, offset);
}

uint64_t GzipReader::length()
{
	return m_reader->length();
}

