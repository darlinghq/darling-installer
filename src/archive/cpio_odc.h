#ifndef CPIO_ODC
#define CPIO_ODC

#define CPIO_ODC_MAGIC "070707"

struct cpio_odc_header
{
	char c_magic[6];
	char c_dev[6];
	char c_ino[6];
	char c_mode[6];
	char c_uid[6];
	char c_gid[6];
	char c_nlink[6];
	char c_rdev[6];
	char c_mtime[11];
	char c_namesize[6];
	char c_filesize[11];
};

#endif

