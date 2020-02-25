#include "PackageInfoXml.h"

const char* PackageInfoXml::SCRIPT_PREFLIGHT = "preflight";
const char* PackageInfoXml::SCRIPT_POSTFLIGHT = "postflight";
const char* PackageInfoXml::SCRIPT_PREINSTALL = "preinstall";
const char* PackageInfoXml::SCRIPT_PREUPGRADE = "preupgrade";
const char* PackageInfoXml::SCRIPT_POSTINSTALL = "postinstall";
const char* PackageInfoXml::SCRIPT_POSTUPGRADE = "postupgrade";

PackageInfoXml::PackageInfoXml(std::shared_ptr<Reader> file)
{
	std::unique_ptr<char[]> contents;
	size_t length = file->length();
	
	contents.reset(new char[length]);
	
	if (file->read(contents.get(), length, 0) != length)
		throw std::runtime_error("Short read in PackageInfoXml");
	
	m_doc = xmlParseMemory(contents.get(), length);
	if (!m_doc)
		throw std::runtime_error("Failed to parse PackageInfo XML");
	
	m_context = xmlXPathNewContext(m_doc);
}

PackageInfoXml::~PackageInfoXml()
{
	xmlXPathFreeContext(m_context);
	xmlFreeDoc(m_doc);
}

std::string PackageInfoXml::identifier() const
{
	return xpathString("string(/pkg-info/@identifier)");
}

std::string PackageInfoXml::version() const
{
	return xpathString("string(/pkg-info/@version)");
}

std::string PackageInfoXml::installLocation() const
{
	return xpathString("string(/pkg-info/@install-location)");
}

bool PackageInfoXml::relocatable() const
{
	std::string val = xpathString("string(/pkg-info/@relocatable)");
	return val == "true";
}
	
size_t PackageInfoXml::installKBytes() const
{
	std::string v = xpathString("string(/pkg-info/payload/@installKBytes)");
	if (!v.empty())
		return std::stoi(v);
	return 0;
}

std::string PackageInfoXml::script(const char* phase) const
{
	std::string xpath;
	
	xpath = "string(/pkg-info/scripts/";
	xpath += phase;
	xpath += "/@file)";
	
	return xpathString(xpath.c_str());
}

std::string PackageInfoXml::xpathString(const char* xpath) const
{
	xmlXPathObjectPtr xpathObj;
	std::string value;
	
	m_context->node = nullptr;
	xpathObj = xmlXPathEvalExpression((const xmlChar*) xpath, m_context);
	
	if (xpathObj == nullptr)
		return value;
	
	if (xpathObj->stringval)
		value = (char*) xpathObj->stringval;
	
	xmlXPathFreeObject(xpathObj);
	return value;
}
