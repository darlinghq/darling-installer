#ifndef XAR_H
#define XAR_H
#include <stdint.h>

#define XAR_HEADER_MAGIC 'xar!'

#pragma pack(1)
struct xar_header
{
	uint32_t magic;
	uint16_t size;
	uint16_t version;
	uint64_t toc_length_compressed;
	uint64_t toc_length_uncompressed;
	uint32_t cksum_alg;
};
#pragma pack()

#endif

