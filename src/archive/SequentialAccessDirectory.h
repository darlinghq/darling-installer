#ifndef SEQUENTIALACCESSDIRECTORY_H
#define SEQUENTIALACCESSDIRECTORY_H
#include <string>
#include <sys/stat.h>
#include "Reader.h"

class SequentialAccessDirectory
{
public:
	virtual ~SequentialAccessDirectory() {}

	virtual bool next(std::string& name, struct stat& st, Reader** reader) = 0;
};

#endif

