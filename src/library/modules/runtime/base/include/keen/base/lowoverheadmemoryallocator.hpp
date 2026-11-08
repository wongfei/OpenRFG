// Original: library/modules/runtime/base/include/keen/base/lowoverheadmemoryallocator.hpp
// Implementation: library/modules/runtime/base/sources/global/lowoverheadmemoryallocator.cpp
//
// LowOverheadAllocator and its MemoryAllocator front end, an allocator with minimal per-block overhead.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/basememoryallocator.hpp"
#include "keen/base/memory.hpp"
#include "keen/base/slice.hpp"

namespace keen
{
    struct MemoryBlockInfo;

    // size 0xC
    class LowOverheadAllocator
    {
    public:
        // size 0x8
        struct FreeBlock
        {
            uint32 size; // 0x00
            FreeBlock* pNext; // 0x04
        };

        LowOverheadAllocator(); // 0x00BF0DC0 (:19)
        bool create(MemoryBlock memoryBlock); // 0x00BF0DD0 (:24)
        MemoryBlock destroy(); // 0x00BF0E10 (:42)
        void* allocate(uint32 size, uint32 alignment, uint32 flags); // 0x00BFEF60 (:47)
        void free(void* pMemory); // 0x00BF0E30 (:85)
        uint32 getAllocationSize(void* pMemory) const; // 0x00BF0EB0 (:135)
        void fillAllocationInfos(Slice<MemoryBlockInfo>* pInfos, uint32 startIndex) const; // 0x00BF8CA0 (:147)
        bool checkIntegrity() const; // 0x00BF8D00 (:173)
    private:
        void* allocateUnalignedMemory(uint32 size); // 0x00BF8D60 (:215)

        MemoryBlock m_memory; // 0x00
        FreeBlock* m_pFirstFree; // 0x08
    };

    // size 0xE0
    class LowOverheadMemoryAllocator : public BaseMemoryAllocator<LowOverheadAllocator>
    {
    };
}
