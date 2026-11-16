#ifndef CHOICESXML_H
#define CHOICESXML_H
#include <string>
#include <vector>

// One entry of a choice changes file (see installer(8)): either a dictionary
// setting an attribute of a choice, or a bare identifier that toggles the choice.
struct ChoiceChange
{
	std::string identifier;
	std::string attribute; // selected, enabled, visible or customLocation
	long value = 0;
	std::string stringValue; // for customLocation
	bool toggle = false; // a bare choice identifier
};

// Reads the property list at path; false if it cannot be read or is not an array
// of dictionaries and strings.
bool readChoiceChanges(const char* path, std::vector<ChoiceChange>& changes);

#endif /* CHOICESXML_H */
