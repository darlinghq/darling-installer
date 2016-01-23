#ifndef SEQUENTIALACCESSDIRECTORY_H
#define SEQUENTIALACCESSDIRECTORY_H
#include <string>
#include <memory>
#include <sys/stat.h>
#include "Reader.h"

class SequentialAccessDirectory
{
public:
	virtual ~SequentialAccessDirectory() {}

	virtual bool next(std::string& name, struct stat& st, std::shared_ptr<Reader>& reader) = 0;
};

#endif

