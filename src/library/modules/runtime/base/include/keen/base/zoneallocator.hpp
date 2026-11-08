// Original: library/modules/runtime/base/include/keen/base/zoneallocator.hpp
#pragma once

#include "keen/base/memory.hpp"

#include <stdint.h>

namespace keen
{
    // Linear (bump) allocator; memory is only reclaimed by clear().
    // size 0xC
    class ZoneAllocator
    {
    public:
        ZoneAllocator(); // 0x004B2400
        bool create(MemoryBlock block); // 0x004DB850
        MemoryBlock destroy(); // 0x005D0AA0
        void* allocate(uint32 size, uint32 alignment); // 0x004DB870
        void clear(); // 0x004B2420
        uint32 getAllocatedSize() const;
        uint32 getCapacity() const;
        uint32 getRemainingCapacity() const;
        const void* getBaseAddress() const;

    private:
        MemoryBlock m_memory; // 0x00
        uint8* m_pCurrentAddress; // 0x08
    };

    // ---- inline members (no .cpp home)

    // 0x004B2400
    inline ZoneAllocator::ZoneAllocator()
    {
        m_memory.pStart = nullptr;
        m_memory.size = 0;
        m_pCurrentAddress = nullptr;
    }

    // 0x004DB850
    inline bool ZoneAllocator::create(MemoryBlock block)
    {
        if (block.pStart == nullptr)
        {
            return false;
        }
        m_memory = block;
        m_pCurrentAddress = m_memory.pStart;
        return true;
    }

    // 0x005D0AA0
    inline MemoryBlock ZoneAllocator::destroy()
    {
        const MemoryBlock block = m_memory;
        m_pCurrentAddress = nullptr;
        m_memory.pStart = nullptr;
        m_memory.size = 0;
        return block;
    }

    // 0x004DB870
    // nullptr when the zone is full. Magic number: at least 4 bytes are taken.
    inline void* ZoneAllocator::allocate(uint32 size, uint32 alignment)
    {
        if (size < 4)
        {
            size = 4;
        }
        uint8* pMemory = (uint8*)(((uintptr_t)m_pCurrentAddress + (alignment - 1)) & ~(uintptr_t)(alignment - 1));
        uint8* pLast = pMemory + size - 1;
        if (pLast < m_memory.pStart || (uint32)(pLast - m_memory.pStart) >= m_memory.size)
        {
            return nullptr;
        }
        m_pCurrentAddress = pMemory + size;
        return pMemory;
    }

    // 0x004B2420
    inline void ZoneAllocator::clear()
    {
        m_pCurrentAddress = m_memory.pStart;
    }
}
