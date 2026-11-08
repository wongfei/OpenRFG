// Original: library/modules/runtime/base/include/keen/base/indexarray.hpp
// Implementation: library/modules/runtime/base/sources/global/indexarray.cpp
#pragma once

#include "keen/base/sizedarray.hpp"

namespace keen
{
    class MemoryAllocator;

    // Stack of free indices in [0, size).
    // size 0xC
    class IndexArray
    {
    public:
        IndexArray(); // 0x00BF8310
        ~IndexArray(); // 0x00BF8320
        bool create(MemoryAllocator* pAllocator, uint32 size); // 0x00BF8330
        void destroy(MemoryAllocator* pAllocator); // 0x00BFE460
        uint32 allocateIndex(); // 0x00BF83A0
        void freeIndex(uint32 index); // 0x00BF83B0
        bool isEmpty() const;
        bool isFull() const; // 0x00BF7940

    private:
        SizedArray<uint32> m_indices; // 0x00
    };
}
