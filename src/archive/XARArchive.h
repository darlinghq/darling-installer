#ifndef XARARCHIVE_H
#define XARARCHIVE_H
#include "RandomAccessDirectory.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>

class XARArchive : public RandomAccessDirectory
{
public:
	XARArchive(Reader* reader);
	virtual ~XARArchive();
	virtual Reader* openFile(const std::string& path) override;
	virtual std::vector<std::string> listFiles() override;
private:
	Reader* openFile(xmlNodeSetPtr nodes);
	static void decompressTOC(const char* in, size_t inLength, char* out, size_t outLength);
	static int64_t extractXMLNumber(xmlXPathContextPtr context, const char* query);
	void loadTOC(const char* in, size_t inLength);
private:
	Reader* m_reader;
	xmlDocPtr m_toc;
	xmlXPathContextPtr m_context;
};

#endif

