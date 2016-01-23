#ifndef CPIOARCHIVE_H
#define CPIOARCHIVE_H
#include "SequentialAccessDirectory.h"
#include <memory>

class CPIOArchive : public SequentialAccessDirectory
{
public:
	CPIOArchive(std::shared_ptr<Reader> reader);
	~CPIOArchive();

	bool next(std::string& name, struct stat& st, std::shared_ptr<Reader>& reader) override;
private:
	std::shared_ptr<Reader> m_reader;
	uint64_t m_offset;
};

#endif

