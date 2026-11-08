// Original: library/modules/runtime/base/include/keen/base/memory.hpp
//
// The helpers are static inline functions of this header (PDB line info: memory.hpp).
#pragma once

#include "keen/base/types.hpp"

#include <string.h>

namespace keen
{
    // size 0x8
    struct MemoryBlock
    {
        uint8* pStart; // 0x00
        uint32 size; // 0x04
    };

    // 0x004595E0 (.h:64)
    static inline void fillMemoryWithZero(void* pMemory, uint32 size)
    {
        memset(pMemory, 0, size);
    }

    // 0x00BED490 (.h:139)
    static inline MemoryBlock clearMemoryBlock(MemoryBlock* pBlock)
    {
        pBlock->pStart = nullptr;
        pBlock->size = 0;
        return *pBlock;
    }

    // 0x00BED4B0 (.h:147)
    static inline MemoryBlock createMemoryBlock(void* pAddress, uint32 size)
    {
        MemoryBlock block;
        block.pStart = (uint8*)pAddress;
        block.size = size;
        return block;
    }

    // 0x00BED4C0 (.h:159)
    static inline bool isAddressInMemoryBlock(MemoryBlock block, const void* pAddress)
    {
        return block.pStart <= (const uint8*)pAddress && (uint32)((const uint8*)pAddress - block.pStart) < block.size;
    }

    // 0x00BED4E0
    static inline bool isMemoryBlockValid(MemoryBlock block)
    {
        return block.pStart != nullptr;
    }
}
