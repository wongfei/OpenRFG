// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hksmallarray.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): small array growth, always on the calling thread's heap. The new block is
// allocated, the elements copied, and the old block freed unless the array does not own it (DONT_DEALLOCATE_FLAG);
// the LOCKED_FLAG survives.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

#include <string.h>

namespace
{
struct SmallArrayWords
{
    char* m_data;
    unsigned short m_size;
    unsigned short m_capacityAndFlags;
};

void reallocate(SmallArrayWords* a, int numElem, int sizeElem)
{
    char* p = (char*)hkMemoryRouter::getInstance().heap().blockAlloc(numElem * sizeElem);
    memcpy(p, a->m_data, a->m_size * sizeElem);
    const unsigned short flags = a->m_capacityAndFlags;
    if ((flags & hkSmallArray<char>::DONT_DEALLOCATE_FLAG) == 0)
    {
        hkMemoryRouter::getInstance().heap().blockFree(a->m_data, sizeElem * (flags & hkSmallArray<char>::CAPACITY_MASK));
    }
    a->m_data = p;
    a->m_capacityAndFlags = (unsigned short)((a->m_capacityAndFlags & hkSmallArray<char>::LOCKED_FLAG) | numElem);
}
}

// 0x00FBE890 (:22)
void hkSmallArrayUtil::_reserve(void* array, int numElem, int sizeElem)
{
    reallocate((SmallArrayWords*)array, numElem, sizeElem);
}

// The inline hkDeallocateChunk of hkSmallArray::releaseMemory (thread heap).
void hkSmallArrayUtil::_free(void* data, int numBytes)
{
    hkMemoryRouter::getInstance().heap().blockFree(data, numBytes);
}

// 0x00FBE920 (:45)
// Doubles the capacity (1 for an empty array).
void hkSmallArrayUtil::_reserveMore(void* array, int sizeElem)
{
    SmallArrayWords* a = (SmallArrayWords*)array;
    reallocate(a, a->m_size == 0 ? 1 : a->m_size * 2, sizeElem);
}
