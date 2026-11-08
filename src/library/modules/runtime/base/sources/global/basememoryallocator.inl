// Original: library/modules/runtime/base/sources/global/basememoryallocator.inl (included by basememoryallocator.hpp)
//
// BaseMemoryAllocator wraps an allocator policy with a mutex. This build does not track allocations: allocate/free
// leave m_allocatedSize, m_maxAllocatedSize and m_allocationCount at zero. Creation flag 0x40 silences the
// out-of-memory message. Requests are rounded up to the alignment; a size of 0 allocates 4 bytes.
// Bodies are from the <TlsfAllocator> instantiation; the <ZoneAllocatorAdapter> one compiles to the same bodies with the
// zone policy inlined (checked for orig_main.md OM1).
#pragma once

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    // <TlsfAllocator> 0x005D6A60 (:21), <ZoneAllocatorAdapter> 0x00506150
    template<typename TAllocator>
    BaseMemoryAllocator<TAllocator>::BaseMemoryAllocator()
    {
        m_memoryBlock.pStart = nullptr;
        m_memoryBlock.size = 0;
    }

    // <TlsfAllocator> 0x005D36E0, <ZoneAllocatorAdapter> 0x004D8410
    template<typename TAllocator>
    BaseMemoryAllocator<TAllocator>::~BaseMemoryAllocator()
    {
    }

    // <TlsfAllocator> 0x005D3710, <ZoneAllocatorAdapter> 0x005D6910
    template<typename TAllocator>
    bool BaseMemoryAllocator<TAllocator>::create(MemoryBlock block, uint32 creationFlags, const char* pName)
    {
        if (block.pStart == nullptr)
        {
            return false;
        }
        if (!m_allocator.create(block))
        {
            debug::nativePrintArgs("[base] Unable to create Allocator '%s'\n", pName);
            return false;
        }
        if (pName == nullptr)
        {
            pName = "<unnamed>";
        }
        copyString(m_name, sizeof(m_name), pName);
        m_flags = creationFlags;
        m_mutex.create(m_name);
        m_memoryBlock = block;
        m_allocatedSize = 0;
        m_maxAllocatedSize = 0;
        m_allocationCount = 0;
        return true;
    }

    // <TlsfAllocator> 0x005D37C0, <ZoneAllocatorAdapter> 0x005D3280
    template<typename TAllocator>
    MemoryBlock BaseMemoryAllocator<TAllocator>::destroy()
    {
        if (m_memoryBlock.pStart == nullptr)
        {
            return m_memoryBlock;
        }
        if (m_allocationCount != 0)
        {
            debug::nativePrintArgs("[base] Memory Allocator '%s' leaked %i Bytes of Memory in %i allocations!\n", m_name, m_allocatedSize, m_allocationCount);
        }
        m_allocatedSize = 0;
        m_maxAllocatedSize = 0;
        m_allocationCount = 0;
        m_mutex.destroy();
        m_allocator.destroy();
        const MemoryBlock block = m_memoryBlock;
        m_memoryBlock.pStart = nullptr;
        m_memoryBlock.size = 0;
        return block;
    }

    // <TlsfAllocator> 0x005D6AA0 (:134), <ZoneAllocatorAdapter> 0x005061A0
    template<typename TAllocator>
    void* BaseMemoryAllocator<TAllocator>::allocate(uint32 size, uint32 alignment, uint32 flags, const char* pName)
    {
        MutexLock lock(&m_mutex);
        if (size == 0)
        {
            size = 4;
        }
        void* pMemory = m_allocator.allocate((size - 1 + alignment) & ~(alignment - 1), alignment, flags);
        if (pMemory == nullptr && (m_flags & 0x40) == 0)
        {
            // The original formats the sizes with keen's thousands separator ("%,zu").
            debug::nativePrintArgs("[base] Allocator '%s' is out of memory! - Could not allocate additional %u Bytes of Memory for '%s'! (Total Size:%u Bytes Available: %u Bytes)\n",
                getName(), size, pName != nullptr ? pName : "", m_memoryBlock.size, m_memoryBlock.size - m_allocatedSize);
        }
        return pMemory;
    }

    // <TlsfAllocator> 0x005D3870, <ZoneAllocatorAdapter> 0x004D8450
    template<typename TAllocator>
    void BaseMemoryAllocator<TAllocator>::free(void* pAddress)
    {
        MutexLock lock(&m_mutex);
        m_allocator.free(pAddress);
    }

    template<typename TAllocator>
    bool BaseMemoryAllocator<TAllocator>::containsPointer(void* pAddress) const
    {
        return isAddressInMemoryBlock(m_memoryBlock, pAddress);
    }

    template<typename TAllocator>
    const void* BaseMemoryAllocator<TAllocator>::getBaseAddress() const
    {
        return m_memoryBlock.pStart;
    }

    // <TlsfAllocator> 0x005D38D0 (.h:55), <ZoneAllocatorAdapter> 0x004D96E0
    template<typename TAllocator>
    const uint32 BaseMemoryAllocator<TAllocator>::getTotalSize() const
    {
        return m_memoryBlock.size;
    }

    template<typename TAllocator>
    bool BaseMemoryAllocator<TAllocator>::isFlagSet(uint32 flag) const
    {
        return (m_flags & flag) != 0;
    }

    // <TlsfAllocator> 0x005D3700 (.h:58), <ZoneAllocatorAdapter> 0x004D8440
    template<typename TAllocator>
    const char* BaseMemoryAllocator<TAllocator>::getName() const
    {
        return m_name;
    }
}
