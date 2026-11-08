// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarrayutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): array storage growth. The array is seen as its three words (data, size,
// capacity and flags). Storage the array does not own (DONT_DEALLOCATE_FLAG, the sign bit) is copied into a new
// buffer and left alone; owned storage is reallocated. The allocator may hand out more than asked, so the capacity is
// what it returned divided by the element size.
//
// Notes (decompiled behaviour, kept as is):
// - _reduce passes the capacity word with its flag bits to bufRealloc as the old size.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

#include <string.h>

namespace
{
struct ArrayWords
{
    char* m_data;
    int m_size;
    int m_capacityAndFlags;
};
}

// 0x00CE8E00 (:17)
hkResult hkArrayUtil::_reserve(hkMemoryAllocator& mem, void* array, int numElem, int sizeElem)
{
    ArrayWords* a = (ArrayWords*)array;
    int bytes = numElem * sizeElem;
    void* p;
    if (a->m_capacityAndFlags < 0)
    {
        p = mem.bufAlloc(bytes);
        if (p != nullptr)
        {
            memcpy(p, a->m_data, a->m_size * sizeElem);
        }
    }
    else
    {
        p = mem.bufRealloc(a->m_data, (a->m_capacityAndFlags & hkArrayBase<char>::CAPACITY_MASK) * sizeElem, bytes);
    }
    a->m_data = (char*)p;
    if (p == nullptr)
    {
        a->m_capacityAndFlags = 0;
        return hkResult(HK_FAILURE);
    }
    a->m_capacityAndFlags = bytes / sizeElem;
    return hkResult(HK_SUCCESS);
}

// 0x00CE8EA0 (:55)
// Doubles the capacity (1 for an empty array).
void hkArrayUtil::_reserveMore(hkMemoryAllocator& mem, void* array, int sizeElem)
{
    ArrayWords* a = (ArrayWords*)array;
    const int numElem = a->m_size == 0 ? 1 : a->m_size * 2;
    int bytes = numElem * sizeElem;
    if (a->m_capacityAndFlags >= 0)
    {
        a->m_data = (char*)mem.bufRealloc(a->m_data, (a->m_capacityAndFlags & hkArrayBase<char>::CAPACITY_MASK) * sizeElem, bytes);
        a->m_capacityAndFlags = bytes / sizeElem;
        return;
    }
    char* p = (char*)mem.bufAlloc(bytes);
    memcpy(p, a->m_data, a->m_size * sizeElem);
    a->m_data = p;
    a->m_capacityAndFlags = bytes / sizeElem;
}

// 0x00CE8F30 (:84)
// Shrinks the storage to requestedCapacity, back into the inplace buffer when there is one and the elements fit.
void hkArrayUtil::_reduce(hkMemoryAllocator& mem, void* array, int sizeElem, char* inplaceMem, int requestedCapacity)
{
    ArrayWords* a = (ArrayWords*)array;
    if (inplaceMem != nullptr && a->m_size < requestedCapacity)
    {
        memcpy(inplaceMem, a->m_data, a->m_size * sizeElem);
        mem.bufFree(a->m_data, (a->m_capacityAndFlags & hkArrayBase<char>::CAPACITY_MASK) * sizeElem);
        a->m_capacityAndFlags = requestedCapacity | hkArrayBase<char>::DONT_DEALLOCATE_FLAG;
        a->m_data = inplaceMem;
        return;
    }
    int bytes = sizeElem * requestedCapacity;
    a->m_data = (char*)mem.bufRealloc(a->m_data, a->m_capacityAndFlags * sizeElem, bytes); // see the notes
    a->m_capacityAndFlags = bytes / sizeElem;
}
