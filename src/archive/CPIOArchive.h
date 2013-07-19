#ifndef CPIOARCHIVE_H
#define CPIOARCHIVE_H
#include "SequentialAccessDirectory.h"

class CPIOArchive : public SequentialAccessDirectory
{
public:
	CPIOArchive(Reader* reader);
	~CPIOArchive();

	bool next(std::string& name, struct stat& st, Reader** reader) override;
private:
	Reader* m_reader;
	uint64_t m_offset;
};

#endif

