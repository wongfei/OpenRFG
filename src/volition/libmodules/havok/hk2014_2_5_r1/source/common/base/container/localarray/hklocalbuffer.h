// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalbuffer.h
//
// Havok 2014.2.5 (hand-written from the PDB instantiations, havok_impl.md HK0): hkFixedArray, a pointer and a size over
// storage owned elsewhere (hkUnionFind's parents, hkFreeList's sort buffer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

// size 0x8
template<typename T>
class hkFixedArray
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkFixedArray);
    const T& operator[](int i) const { return m_data.val()[i]; } // <int> 0x00FBEE80
    T& operator[](int i) { return m_data.val()[i]; } // <int> 0x00D683E0
    const T* begin() const { return m_data.val(); }
    T* begin() { return m_data.val(); } // <int> 0x00D683F0
public:
    hkFixedArray(); // <int> 0x00D68500
public:
    hkFixedArray(T* t, int n) : m_data(t), m_size(n) {}

public:
    hkPadSpu<T*> m_data; // 0x00
    hkPadSpu<int> m_size; // 0x04
};

// A fixed array on the LIFO (stack) allocator (Havok 2014.1 hkLocalBuffer; HK7). hkAllocateStack/hkDeallocateStack
// round the block up to 128 bytes.
template<typename T>
class hkLocalBuffer : public hkFixedArray<T>
{
public:
    explicit hkLocalBuffer(int n)
        : hkFixedArray<T>(static_cast<T*>(hkMemoryRouter::getInstance().stack().fastBlockAlloc(getStackBytes(n))), n)
    {
    }

    ~hkLocalBuffer()
    {
        hkMemoryRouter::getInstance().stack().fastBlockFree(this->m_data.val(), getStackBytes(this->m_size.val()));
    }

    static int getStackBytes(int n) { return (n * (int)sizeof(T) + 127) & ~127; }
};
