#include "DistributionXml.h"
#include <stdexcept>
#include <cstring>
#import <Foundation/Foundation.h>

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

// A literal boolean ("true"/"false", surrounding whitespace allowed); anything
// else is a JavaScript expression.
static bool parseBoolean(const char* text, bool& value)
{
	std::string t(text);
	size_t b = t.find_first_not_of(" \t\r\n"), e = t.find_last_not_of(" \t\r\n");
	
	if (b == std::string::npos)
		return false;
	t = t.substr(b, e - b + 1);
	if (t == "true") { value = true; return true; }
	if (t == "false") { value = false; return true; }
	return false;
}

std::vector<std::string> DistributionXml::domains() const
{
	static const struct { const char* attribute; const char* domain; } kDomains[] = {
		{ "enable_localSystem", "LocalSystem" },
		{ "enable_currentUserHome", "CurrentUserHomeDirectory" },
	};
	std::vector<std::string> rv;
	
	m_context->node = nullptr;
	for (const auto& d : kDomains)
	{
		std::string xpath = std::string("string(/*/domains/@") + d.attribute + ")";
		xmlXPathObjectPtr obj = xmlXPathEvalExpression((const xmlChar*) xpath.c_str(), m_context);
		
		if (obj != nullptr)
		{
			if (obj->stringval && strcmp((char*) obj->stringval, "true") == 0)
				rv.push_back(d.domain);
			xmlXPathFreeObject(obj);
		}
	}
	return rv;
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
		
		// Initial state: start_selected (a plain boolean) if given, otherwise
		// "selected" when it is a literal boolean; the default is true.
		choice.selected = true;
		attrVal = xmlGetProp(node, (xmlChar*) "start_selected");
		if (attrVal != nullptr)
		{
			bool value;
			if (parseBoolean((char*) attrVal, value))
				choice.selected = value;
			else
				choice.unevaluated.push_back("start_selected");
		}
		else
		{
			attrVal = xmlGetProp(node, (xmlChar*) "selected");
			if (attrVal != nullptr)
			{
				bool value;
				if (parseBoolean((char*) attrVal, value))
					choice.selected = value;
				else
					choice.unevaluated.push_back("selected");
			}
		}

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
	pkg.active = true;
	pkg.activeUnevaluated = false;
	
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

		// pkg-refs sharing an id are merged; "active" may sit on any of them
		attrVal = xmlGetProp(node, (xmlChar*) "active");
		if (attrVal != nullptr)
		{
			bool value;
			if (parseBoolean((char*) attrVal, value))
				pkg.active = pkg.active && value;
			else
				pkg.activeUnevaluated = true;
		}

		if (pkg.path.empty() && node->children && node->children->type == XML_TEXT_NODE && !xmlIsBlankNode(node->children))
			pkg.path = (char*) node->children->content;

		NSString *nsPath = [NSString stringWithUTF8String: pkg.path.c_str()];
		pkg.path = [[nsPath stringByRemovingPercentEncoding] cStringUsingEncoding: NSUTF8StringEncoding];
	}

	xmlXPathFreeObject(xpathObj);
	return true;
}
