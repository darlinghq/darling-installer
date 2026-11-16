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
		bool selected; // initially selected, i.e. to be installed
		// bool enabled; // whether the user can manipulate the selected status
		std::string id, title;
		std::vector<std::string> pkgref;
		// Attributes holding a JavaScript expression, which is not evaluated;
		// the choice keeps the documented default (selected) for these.
		std::vector<std::string> unevaluated;
	};
	struct PkgRef
	{
		size_t installKbytes;
		std::string path;
		bool active = true; // false: not installed even if its choice is selected
		bool activeUnevaluated = false; // "active" is a JavaScript expression
	};
	
	// Domains the package enables: LocalSystem, CurrentUserHomeDirectory
	std::vector<std::string> domains() const;
	std::vector<Choice> choices() const;
	bool package(const std::string& id, PkgRef& pkg) const;
private:
	mutable xmlDocPtr m_doc;
	mutable xmlXPathContextPtr m_context;
};

#endif /* DISTRIBUTIONXML_H */

