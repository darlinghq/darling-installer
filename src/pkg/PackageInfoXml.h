#ifndef PACKAGEINFOXML_H
#define PACKAGEINFOXML_H
#include "archive/Reader.h"
#include <memory>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <string>

class PackageInfoXml
{
public:
	PackageInfoXml(std::shared_ptr<Reader> file);
	~PackageInfoXml();
	
	std::string identifier() const;
	std::string version() const;
	
	// Install relative to what path
	std::string installLocation() const;
	
	// Can be installed to a different location than installLocation()?
	bool relocatable() const;
	
	size_t installKBytes() const;
	
	std::string preinstallScript() const;
	std::string postinstallScript() const;
private:
	std::string xpathString(const char* xpath) const;
private:
	mutable xmlDocPtr m_doc;
	mutable xmlXPathContextPtr m_context;
};

#endif /* PACKAGEINFOXML_H */

