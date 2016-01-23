#ifndef DISTRIBUTIONXML_H
#define DISTRIBUTIONXML_H
#include "archive/Reader.h"
#include <memory>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <vector>
#include <string>

class DistributionXml
{
public:
	DistributionXml(std::shared_ptr<Reader> file);
	~DistributionXml();
	
	std::string title() const;
	
	struct Choice
	{
		bool selected; // to be installed, can also be JavaScript :-/
		// bool enabled; // whether the user can manipulate the selected status
		std::string id, title;
		std::vector<std::string> pkgref;
	};
	struct PkgRef
	{
		size_t installKbytes;
		std::string path;
	};
	
	std::vector<Choice> choices() const;
	bool package(const std::string& id, PkgRef& pkg) const;
private:
	mutable xmlDocPtr m_doc;
	mutable xmlXPathContextPtr m_context;
};

#endif /* DISTRIBUTIONXML_H */

