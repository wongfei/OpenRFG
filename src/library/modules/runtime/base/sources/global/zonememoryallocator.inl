// Original: library/modules/runtime/base/sources/global/zonememoryallocator.inl (included by zonememoryallocator.hpp)
//
// ZoneAllocatorAdapter counts allocations; when the count drops back to zero the whole zone is reset.
#pragma once

namespace keen
{
    // 0x004B2430
    inline ZoneAllocatorAdapter::ZoneAllocatorAdapter()
        : m_allocationCount(0)
    {
    }

    // 0x004B2450
    inline ZoneAllocatorAdapter::~ZoneAllocatorAdapter()
    {
    }

    // 0x005D39C0
    inline bool ZoneAllocatorAdapter::create(MemoryBlock memoryBlock)
    {
        if (!m_allocator.create(memoryBlock))
        {
            return false;
        }
        m_allocationCount = 0;
        return true;
    }

    // 0x005D0AC0
    inline void ZoneAllocatorAdapter::destroy()
    {
        m_allocator.destroy();
        m_allocationCount = 0;
    }

    // 0x004DB8C0
    inline void* ZoneAllocatorAdapter::allocate(uint32 size, uint32 alignment, uint32 flags)
    {
        void* pMemory = m_allocator.allocate(size, alignment);
        if (pMemory != nullptr)
        {
            ++m_allocationCount;
        }
        return pMemory;
    }

    // 0x004D8430
    inline void ZoneAllocatorAdapter::free(void* pMemory)
    {
        --m_allocationCount;
        if (m_allocationCount == 0)
        {
            m_allocator.clear();
        }
    }

    // 0x00516C70
    inline ZoneMemoryAllocator::ZoneMemoryAllocator()
    {
    }

    // 0x004F3710
    inline ZoneMemoryAllocator::~ZoneMemoryAllocator()
    {
    }
}
