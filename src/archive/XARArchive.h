#ifndef XARARCHIVE_H
#define XARARCHIVE_H
#include "RandomAccessDirectory.h"
#include <memory>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <stdint.h>

class XARArchive : public RandomAccessDirectory
{
public:
	XARArchive(std::shared_ptr<Reader> reader);
	virtual ~XARArchive();
	virtual Reader* openFile(const std::string& path) override;
	virtual std::vector<std::string> listFiles() override;
	bool containsFile(const std::string& path);
private:
	std::string xpathForPath(const std::string& path);
	Reader* openFile(xmlNodeSetPtr nodes);
	static void decompressTOC(const char* in, size_t inLength, char* out, size_t outLength);
	static int64_t extractXMLNumber(xmlXPathContextPtr context, const char* query);
	void loadTOC(const char* in, size_t inLength);
private:
	std::shared_ptr<Reader> m_reader;
	xmlDocPtr m_toc;
	xmlXPathContextPtr m_context;
	uint64_t m_heapStart;
};

#endif

