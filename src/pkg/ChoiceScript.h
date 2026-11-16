#ifndef CHOICESCRIPT_H
#define CHOICESCRIPT_H
#include <memory>
#include <string>
#include <vector>

// Evaluates the JavaScript of a Distribution file (choice conditions, check
// scripts) with JavaScriptCore, which is loaded at run time: it is an optional
// part of the system, and without it create() returns null.
//
// The script sees the Installer JavaScript objects (documented as "Installer JS"):
// `system` (version, compareVersions, files.fileExistsAtPath/plistAtPath/
// bundleAtPath, sysctl, env, propertiesOf, localizedString, applications.
// fromIdentifier, log), `my` (target.mountpoint/systemVersion/availableKilobytes,
// result.type/message/title) and `choices` (<id>.selected/enabled/visible/title/
// description/packages/packageUpgradeAction).
// Anything else is undefined and makes the expression fail.
class ChoiceScript
{
public:
	struct Package
	{
		std::string identifier, version;
	};
	struct ChoiceState
	{
		std::string id, title, description;
		bool selected, enabled, visible;
		std::vector<Package> packages;
		// clean, installed, upgrade, downgrade or mixed: how the packages relate to those installed
		std::string packageUpgradeAction;
	};

	static std::unique_ptr<ChoiceScript> create(const std::string& targetMountPoint);
	~ChoiceScript();

	// Runs a <script> element's text, defining functions for later expressions
	bool addScript(const std::string& source, std::string& error);
	// The state `choices.<id>` reports from now on
	void setChoices(const std::vector<ChoiceState>& choices);
	// Evaluates an expression; only the boolean true gives true
	bool evaluate(const std::string& expression, bool& result, std::string& error);
	// my.result.type / my.result.message as set by the last scripts
	std::string resultType();
	std::string resultMessage();

private:
	ChoiceScript();
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

#endif /* CHOICESCRIPT_H */
