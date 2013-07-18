#ifndef RANDOMACCESSDIRECTORY_H
#define RANDOMACCESSDIRECTORY_H
#include "Reader.h"
#include <string>
#include <vector>

class RandomAccessDirectory
{
public:
	virtual ~RandomAccessDirectory() {}

	virtual Reader* openFile(const std::string& path) = 0;
	virtual std::vector<std::string> listFiles() = 0;
};

#endif

