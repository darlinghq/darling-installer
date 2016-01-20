#ifndef BOMARRAY_H
#define	BOMARRAY_H
#include "be.h"
#include <stdint.h>

template <typename ArrayType>
class BOMArray
{
public:
	BOMArray(const void* arrayStart)
	: m_array(arrayStart)
	, m_arrayLength(be(*reinterpret_cast<const uint32_t*>(arrayStart)))
	{
	}
	
	uint32_t size() const { return m_arrayLength; }
	const ArrayType* itemAtOffset(size_t offset) const
	{
		return reinterpret_cast<const ArrayType*>(
				static_cast<const uint8_t*>(m_array)
					+ sizeof(uint32_t) + offset
				);
	}
private:
	const void* m_array;
	const uint32_t m_arrayLength;
};


#endif	/* BOMARRAY_H */

