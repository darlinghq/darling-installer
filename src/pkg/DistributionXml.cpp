#include "DistributionXml.h"
#include <stdexcept>
#include <cstring>

DistributionXml::DistributionXml(std::shared_ptr<Reader> file)
{
	std::unique_ptr<char[]> contents;
	size_t length = file->length();
	
	contents.reset(new char[length]);
	
	if (file->read(contents.get(), length, 0) != length)
		throw std::runtime_error("Short read in DistributionXml");
	
	m_doc = xmlParseMemory(contents.get(), length);
	if (!m_doc)
		throw std::runtime_error("Failed to parse Distribution XML");
	
	m_context = xmlXPathNewContext(m_doc);
}

DistributionXml::~DistributionXml()
{
	xmlXPathFreeContext(m_context);
	xmlFreeDoc(m_doc);
}

std::string DistributionXml::title() const
{
	xmlXPathObjectPtr xpathObj;
	std::string title;
	
	xpathObj = xmlXPathEvalExpression((const xmlChar*) "string(/*/title/text())", m_context);
	if (xpathObj->stringval)
		title = (char*) xpathObj->stringval;
	
	xmlXPathFreeObject(xpathObj);
	return title;
}

std::vector<DistributionXml::Choice> DistributionXml::choices() const
{
	std::vector<DistributionXml::Choice> rv;
	xmlXPathObjectPtr xpathObj;
	
	m_context->node = nullptr;
	xpathObj = xmlXPathEvalExpression((const xmlChar*) "/*/choice", m_context);
	
	for (int i = 0; xpathObj->nodesetval != nullptr && i < xpathObj->nodesetval->nodeNr; i++)
	{
		Choice choice;
		xmlNodePtr node;
		xmlChar* attrVal;
		xmlXPathObjectPtr pkgrefs;

		node = xpathObj->nodesetval->nodeTab[i];
		if (node->type != XML_ELEMENT_NODE)
			continue;

		attrVal = xmlGetProp(node, (xmlChar*) "title");
		if (attrVal) choice.title = (char*) attrVal;

		attrVal = xmlGetProp(node, (xmlChar*) "id");
		if (!attrVal) throw std::runtime_error("Element <choice> lacks 'id' attributte");
		choice.id = (char*) attrVal;
		
		attrVal = xmlGetProp(node, (xmlChar*) "selected");
		if (attrVal != nullptr && strcmp((char*) attrVal, "false") == 0)
			choice.selected = false;
		else
			choice.selected = true;

		m_context->node = node;
		pkgrefs = xmlXPathEvalExpression((const xmlChar*) "pkg-ref/@id", m_context);
		
		for (int j = 0; pkgrefs->nodesetval != nullptr && j < pkgrefs->nodesetval->nodeNr; j++)
		{
			xmlNodePtr attr;
			
			attr = pkgrefs->nodesetval->nodeTab[j];
			if (attr->type != XML_ATTRIBUTE_NODE)
				continue;
			
			choice.pkgref.push_back((char* ) attr->children->content);
		}

		xmlXPathFreeObject(pkgrefs);
		rv.push_back(choice);
	}
	
	xmlXPathFreeObject(xpathObj);
	return rv;
}

bool DistributionXml::package(const std::string& id, DistributionXml::PkgRef& pkg) const
{
	xmlXPathObjectPtr xpathObj;
	std::string xpath;
	
	xpath = "//pkg-ref[@id='";
	xpath += id;
	xpath += "']";
	
	pkg.installKbytes = 0;
	pkg.path.clear();
	
	m_context->node = nullptr;
	xpathObj = xmlXPathEvalExpression((const xmlChar*) xpath.c_str(), m_context);
	
	if (!xpathObj->nodesetval)
	{
		xmlXPathFreeObject(xpathObj);
		return false;
	}
	
	for (int i = 0; i < xpathObj->nodesetval->nodeNr; i++)
	{
		xmlNodePtr node;
		xmlChar* attrVal;
		
		node = xpathObj->nodesetval->nodeTab[i];
		if (node->type != XML_ELEMENT_NODE)
			continue;
		
		attrVal = xmlGetProp(node, (xmlChar*) "installKBytes");
		if (attrVal != nullptr)
			pkg.installKbytes = atoi((char*) attrVal);

		if (pkg.path.empty() && node->children && node->children->type == XML_TEXT_NODE && !xmlIsBlankNode(node->children))
			pkg.path = (char*) node->children->content;
	}
	
	xmlXPathFreeObject(xpathObj);
	return true;
}

