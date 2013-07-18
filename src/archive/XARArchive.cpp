#include "XARArchive.h"
#include "xar.h"
#include <stdexcept>
#include "zlib.h"
#include <memory>
#include <cstring>
#include "be.h"
#include <iostream>
#include <sstream>

XARArchive::XARArchive(Reader* reader)
	: m_reader(reader), m_toc(nullptr)
{
	xar_header hdr;
	std::unique_ptr<char[]> compressed, uncompressed;

	m_reader->read(&hdr, sizeof(hdr), 0);
	if (be(hdr.magic) != XAR_HEADER_MAGIC)
		throw std::runtime_error("Not a XAR file");

	compressed.reset(new char[be(hdr.toc_length_compressed)]);
	m_reader->read(compressed.get(), be(hdr.toc_length_compressed), be(hdr.size));

	uncompressed.reset(new char[be(hdr.toc_length_uncompressed)]);
	decompressTOC(compressed.get(), be(hdr.toc_length_compressed),
			uncompressed.get(), be(hdr.toc_length_uncompressed));

	compressed.reset();
	loadTOC(uncompressed.get(), be(hdr.toc_length_uncompressed));
}

XARArchive::~XARArchive()
{
	xmlXPathFreeContext(m_context);
	xmlFreeDoc(m_toc);
	delete m_reader;
}

static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim))
		elems.push_back(item);

	return elems;
}

static std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}

Reader* XARArchive::openFile(const std::string& path)
{
	std::vector<std::string> parts = split(path, '/');
	std::stringstream ss;
	xmlXPathObjectPtr xpathObj;
	Reader* retval = nullptr;

	if (parts.empty())
		return nullptr;

	ss << "/xar/toc";

	for (const std::string& p : parts)
	{
		if (!p.empty())
			ss << "/file[name='" << p << "']"; // TODO: escape file name
	}
	ss << "/data";

	m_context->node = nullptr;

	xpathObj = xmlXPathEvalExpression((const xmlChar*) ss.str().c_str(), m_context);
	if (xpathObj->nodesetval)
	{
		retval = openFile(xpathObj->nodesetval);
	}

	xmlXPathFreeObject(xpathObj);
	return retval;
}

Reader* XARArchive::openFile(xmlNodeSetPtr nodes)
{
	xmlXPathObjectPtr xpathObj;
	Reader* retval = nullptr;
	int64_t length, offset, size;

	if (!nodes->nodeNr)
		return nullptr;

	m_context->node = nodes->nodeTab[0];
	xpathObj = xmlXPathEvalExpression("string(encoding/@style)", m_context);

	if (!xpathObj->stringval)
	{
		xmlXPathFreeObject(xpathObj);
		throw std::runtime_error("Missing encoding for file!");
	}

	length = extractXMLNumber(m_context, "string(length)");
	offset = extractXMLNumber(m_context, "string(offset)");
	size = extractXMLNumber(m_context, "string(size)");

	if (strcmp(xpathObj->stringval, "application/octet-stream") == 0)
	{
		if (size != length)
			throw std::runtime_error("size != length for octet-stream");

		retval = new SubReader(m_reader, offset, size);
	}
	else if (strcmp(xpathObj->stringval, "application/x-gzip") == 0)
	{
		// TODO: deflate
	}

	xmlXPathFreeObject(xpathObj);
}

int64_t XARArchive::extractXMLNumber(xmlXPathContextPtr context, const char* query)
{
	xmlXPathObjectPtr xpathObj;
	int64_t rv = -1;

	xpathObj = xmlXPathEvalExpression(query, context);
	if (xpathObj && xpathObj->stringval)
		rv = std::stoll(xpathObj->stringval);

	xmlXPathFreeObject(xpathObj);
	return rv;
}

std::vector<std::string> XARArchive::listFiles()
{
}

void XARArchive::decompressTOC(const char* in, size_t inLength, char* out, size_t outLength)
{
	z_stream strm;
	int ret;
	
	memset(&strm, 0, sizeof(strm));

	ret = inflateInit(&strm);
	if (ret != Z_OK)
		throw std::runtime_error("deflateInit failed");

	strm.next_in = (Bytef*) in;
	strm.avail_in = inLength;
	strm.next_out = (Bytef*) out;
	strm.avail_out = outLength;

	while (true)
	{
		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret < 0)
			throw std::runtime_error("deflate error");
		else if (ret == Z_STREAM_END)
			break;
	}

	inflateEnd(&strm);
}

void XARArchive::loadTOC(const char* in, size_t inLength)
{
	m_toc = xmlParseMemory(in, inLength);
	if (!m_toc)
		throw std::runtime_error("Invalid XML data");
	m_context = xmlXPathNewContext(m_toc);
	std::cout << std::string(in, inLength) << std::endl;
}

