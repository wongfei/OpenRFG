// Original: library/modules/runtime/base/include/keen/base/zonememoryallocator.hpp
// Implementation: library/modules/runtime/base/sources/global/zonememoryallocator.inl
#pragma once

#include "keen/base/basememoryallocator.hpp"
#include "keen/base/zoneallocator.hpp"

namespace keen
{
    // Adapts ZoneAllocator to the BaseMemoryAllocator policy interface; free() only counts allocations.
    // size 0x10
    class ZoneAllocatorAdapter
    {
    public:
        ZoneAllocatorAdapter(); // 0x004B2430
        ~ZoneAllocatorAdapter(); // 0x004B2450
        bool create(MemoryBlock memoryBlock); // 0x005D39C0
        void destroy(); // 0x005D0AC0
        void* allocate(uint32 size, uint32 alignment, uint32 flags); // 0x004DB8C0
        void free(void* pMemory); // 0x004D8430

    private:
        ZoneAllocator m_allocator; // 0x00
        uint32 m_allocationCount; // 0x0C
    };

    // size 0xE4
    class ZoneMemoryAllocator : public BaseMemoryAllocator<ZoneAllocatorAdapter>
    {
    public:
        ZoneMemoryAllocator(); // 0x00516C70
        virtual ~ZoneMemoryAllocator(); // vtbl+0x00 0x004F3710
    };
}

#include "../../../sources/global/zonememoryallocator.inl"
