#ifndef GZIP_H
#define GZIP_H
#include <stdint.h>

#pragma pack(1)
struct gzip_header
{
	uint8_t id1, id2;
	uint8_t method;
	uint8_t flags;
	uint32_t mtime;
	uint8_t extraFlags;
	uint8_t os;
};
#pragma pack()

#define GZIP_FHTEXT		1
#define GZIP_FHCRC		(1 << 1)
#define GZIP_FEXTRA		(1 << 2)
#define GZIP_FNAME		(1 << 3)
#define GZIP_FCOMMENT	(1 << 4)

#endif

