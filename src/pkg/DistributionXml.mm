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

static bool initialState(xmlNodePtr node, const char* name, DistributionXml::Choice& choice)
{
	std::string start = std::string("start_") + name;
	bool value;
	
	for (const std::string& attr : { start, std::string(name) })
	{
		xmlChar* text = xmlGetProp(node, (const xmlChar*) attr.c_str());
		if (text == nullptr)
			continue;
		
		bool literal = parseBoolean((char*) text, value);
		if (literal)
		{
			xmlFree(text);
			return value;
		}
		choice.unevaluated.push_back(attr);
		if (attr == name) // start_X is a plain boolean, only X holds an expression
			choice.expressions[attr] = (char*) text;
		xmlFree(text);
		break;
	}
	return true;
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

		attrVal = xmlGetProp(node, (xmlChar*) "description");
		if (attrVal) choice.description = (char*) attrVal;

		attrVal = xmlGetProp(node, (xmlChar*) "id");
		if (!attrVal) throw std::runtime_error("Element <choice> lacks 'id' attributte");
		choice.id = (char*) attrVal;
		
		// Initial state: start_X (a plain boolean) if given, otherwise the
		// attribute X when it is a literal boolean; the default is true.
		choice.selected = initialState(node, "selected", choice);
		choice.enabled = initialState(node, "enabled", choice);
		choice.visible = initialState(node, "visible", choice);

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
	pkg.version.clear();
	pkg.active = true;
	pkg.activeUnevaluated = false;
	pkg.activeExpression.clear();
	
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

		attrVal = xmlGetProp(node, (xmlChar*) "version");
		if (attrVal != nullptr)
			pkg.version = (char*) attrVal;

		// pkg-refs sharing an id are merged; "active" may sit on any of them
		attrVal = xmlGetProp(node, (xmlChar*) "active");
		if (attrVal != nullptr)
		{
			bool value;
			if (parseBoolean((char*) attrVal, value))
				pkg.active = pkg.active && value;
			else
			{
				pkg.activeUnevaluated = true;
				pkg.activeExpression = (char*) attrVal;
			}
		}

		if (pkg.path.empty() && node->children && node->children->type == XML_TEXT_NODE && !xmlIsBlankNode(node->children))
			pkg.path = (char*) node->children->content;

		NSString *nsPath = [NSString stringWithUTF8String: pkg.path.c_str()];
		pkg.path = [[nsPath stringByRemovingPercentEncoding] cStringUsingEncoding: NSUTF8StringEncoding];
	}

	xmlXPathFreeObject(xpathObj);
	return true;
}

static void readLines(xmlNodePtr parent, std::vector<DistributionXml::Line>& lines)
{
	for (xmlNodePtr n = parent->children; n != nullptr; n = n->next)
	{
		if (n->type != XML_ELEMENT_NODE || xmlStrcmp(n->name, (const xmlChar*) "line") != 0)
			continue;
		
		xmlChar* id = xmlGetProp(n, (const xmlChar*) "choice");
		if (id == nullptr)
			continue;
		
		DistributionXml::Line line;
		line.choice = (char*) id;
		xmlFree(id);
		readLines(n, line.children);
		lines.push_back(line);
	}
}

std::vector<DistributionXml::Line> DistributionXml::outline() const
{
	std::vector<Line> rv;
	xmlXPathObjectPtr obj;
	
	m_context->node = nullptr;
	obj = xmlXPathEvalExpression((const xmlChar*) "/*/choices-outline", m_context);
	if (obj != nullptr && obj->nodesetval != nullptr && obj->nodesetval->nodeNr > 0)
		readLines(obj->nodesetval->nodeTab[0], rv);
	if (obj != nullptr)
		xmlXPathFreeObject(obj);
	return rv;
}

std::vector<std::string> DistributionXml::scripts() const
{
	std::vector<std::string> rv;
	xmlXPathObjectPtr obj;
	
	m_context->node = nullptr;
	obj = xmlXPathEvalExpression((const xmlChar*) "/*/script", m_context);
	for (int i = 0; obj != nullptr && obj->nodesetval != nullptr && i < obj->nodesetval->nodeNr; i++)
	{
		xmlChar* text = xmlNodeGetContent(obj->nodesetval->nodeTab[i]);
		if (text != nullptr)
		{
			rv.push_back((char*) text);
			xmlFree(text);
		}
	}
	if (obj != nullptr)
		xmlXPathFreeObject(obj);
	return rv;
}

static std::string checkScript(xmlXPathContextPtr context, const char* xpath)
{
	std::string rv;
	
	context->node = nullptr;
	xmlXPathObjectPtr obj = xmlXPathEvalExpression((const xmlChar*) xpath, context);
	if (obj != nullptr)
	{
		if (obj->stringval)
			rv = (char*) obj->stringval;
		xmlXPathFreeObject(obj);
	}
	return rv;
}

bool DistributionXml::allowExternalScripts() const
{
	std::string v = checkScript(m_context, "string(/*/options/@allow-external-scripts)");
	// the schema reference says true/false, the system.run page says 'yes'
	return v == "true" || v == "yes";
}

std::string DistributionXml::installationCheck() const
{
	return checkScript(m_context, "string(/*/installation-check/@script)");
}

std::string DistributionXml::volumeCheck() const
{
	return checkScript(m_context, "string(/*/volume-check/@script)");
}
