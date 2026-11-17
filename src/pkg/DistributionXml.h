#ifndef DISTRIBUTIONXML_H
#define DISTRIBUTIONXML_H
#include "archive/Reader.h"
#include <memory>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <vector>
#include <map>
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
		bool enabled = true; // whether the user can manipulate the selected status
		bool visible = true;
		std::string id, title, description;
		std::vector<std::string> pkgref;
		// Attributes (selected, enabled, visible) holding a JavaScript expression:
		// the expression, and the names of those not (yet) evaluated, for which the
		// choice keeps the documented default (true).
		std::map<std::string, std::string> expressions;
		std::vector<std::string> unevaluated;
		std::map<std::string, std::string> evalErrors; // attribute -> why its script failed
	};
	struct PkgRef
	{
		size_t installKbytes;
		std::string path;
		std::string version; // the pkg-ref version attribute
		bool active = true; // false: not installed even if its choice is selected
		bool activeUnevaluated = false; // "active" is a JavaScript expression
		std::string activeExpression;
	};
	
	// Domains the package enables: LocalSystem, CurrentUserHomeDirectory
	std::vector<std::string> domains() const;
	
	// <options allow-external-scripts>: whether system.run/runOnce may launch programs
	bool allowExternalScripts() const;
	// The file named by <welcome>, <readme>, <license>, <conclusion> or <background>, and its MIME type
	bool resourceFile(const char* element, std::string& file, std::string& mimeType) const;
	// Text of the <script> elements, which define functions for the expressions
	std::vector<std::string> scripts() const;
	// The script expression of <installation-check> and <volume-check>; empty if absent
	std::string installationCheck() const;
	std::string volumeCheck() const;
	std::vector<Choice> choices() const;
	
	// The <choices-outline> tree: each line names a choice, child lines are its subchoices
	struct Line
	{
		std::string choice;
		std::vector<Line> children;
	};
	std::vector<Line> outline() const;
	bool package(const std::string& id, PkgRef& pkg) const;
private:
	mutable xmlDocPtr m_doc;
	mutable xmlXPathContextPtr m_context;
};

#endif /* DISTRIBUTIONXML_H */

