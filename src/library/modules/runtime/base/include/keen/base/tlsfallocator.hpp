// Original: library/modules/runtime/base/include/keen/base/tlsfallocator.hpp
// Implementation: library/modules/runtime/base/sources/global/tlsfallocator.cpp (wraps thirdparty TLSF)
#pragma once

#include "keen/base/memory.hpp"

namespace keen
{
    struct MemoryBlockInfo;
    template<typename T> class Slice;

    // Raw two-level segregated fit allocator over a fixed memory block. Not thread-safe; see TlsfMemoryAllocator.
    // size 0xC
    class TlsfAllocator
    {
    public:
        TlsfAllocator(); // 0x00BF2F30
        bool create(MemoryBlock memoryBlock); // 0x00C09E00
        MemoryBlock destroy(); // 0x00BF2F50
        void* allocate(uint32 size, uint32 alignment, uint32 flags); // 0x00BF2F80
        void free(void* pMemory); // 0x00BF2FB0
        uint32 getAllocationSize(const void* pMemory) const; // 0x00BF2FD0
        void fillAllocationInfos(Slice<MemoryBlockInfo>* pInfos, uint32 startIndex) const; // 0x00BFB370
        bool checkIntegrity() const; // 0x00BF2FE0
        const MemoryBlock& getMemoryBlock() const;
        static uint32 getAllocationOverhead(); // 0x00BF2F40

    private:
        MemoryBlock m_memoryBlock; // 0x00
        void* m_pPool; // 0x08
    };
}
