#include "CPIOArchive.h"
#include "cpio_odc.h"
#include "SubReader.h"
#include <cstring>
#include <memory>

CPIOArchive::CPIOArchive(std::shared_ptr<Reader> reader)
	: m_reader(reader), m_offset(0)
{
}

CPIOArchive::~CPIOArchive()
{
}

static int fromoct(char (&value)[6])
{
	return std::stoi(std::string(value, 6), nullptr, 8);
}

static long long fromoct(char (&value)[11])
{
	return std::stol(std::string(value, 11), nullptr, 8);
}

bool CPIOArchive::next(std::string& name, struct stat& st, std::shared_ptr<Reader>& reader)
{
	cpio_odc_header hdr;
	std::unique_ptr<char[]> filename;
	int fnlen;

	if (m_reader->read(&hdr, sizeof(hdr), m_offset) != sizeof(hdr))
		return false;
	if (memcmp(hdr.c_magic, CPIO_ODC_MAGIC, 6) != 0)
		return false;

	memset(&st, 0, sizeof(st));

	st.st_dev = fromoct(hdr.c_dev);
	st.st_ino = fromoct(hdr.c_ino);
	st.st_mode = fromoct(hdr.c_mode);
	st.st_uid = fromoct(hdr.c_uid);
	st.st_gid = fromoct(hdr.c_gid);
	st.st_nlink = fromoct(hdr.c_nlink);
	st.st_rdev = fromoct(hdr.c_rdev);
	st.st_mtime = fromoct(hdr.c_mtime);
	st.st_size = fromoct(hdr.c_filesize);

	fnlen = fromoct(hdr.c_namesize);
	filename.reset(new char[fnlen]);

	m_offset += sizeof(hdr);

	if (m_reader->read(filename.get(), fnlen, m_offset) != fnlen)
		return false;
	if (filename[fnlen-1] != 0)
		return false;

	if (strcmp(filename.get(), "TRAILER!!!") == 0)
		return false;

	name = filename.get();
	m_offset += fnlen;

	reader.reset(new SubReader(m_reader, m_offset, st.st_size));

	m_offset += st.st_size;
	return true;
}

