// Original: library/modules/runtime/base/include/keen/base/basememoryallocator.hpp
// Implementation: library/modules/runtime/base/sources/global/basememoryallocator.inl
#pragma once

#include "keen/base/memoryallocator.hpp"
#include "keen/base/mutex.hpp"

namespace keen
{
    // Thread-safe MemoryAllocator front end over a raw allocator policy
    // (TlsfAllocator, ZoneAllocatorAdapter, LowOverheadAllocator, ...).
    // Policy interface: create(MemoryBlock), destroy(), allocate(size, alignment, flags), free(void*).
    // size 0xD4 + sizeof(TAllocator) (0xE0 for TlsfAllocator, 0xE4 for ZoneAllocatorAdapter)
    template<typename TAllocator>
    class BaseMemoryAllocator : public MemoryAllocator
    {
    public:
        typedef BaseMemoryAllocator<TAllocator> ThisType;

    protected:
        BaseMemoryAllocator(const BaseMemoryAllocator<TAllocator>&);
    public:
        BaseMemoryAllocator(); // <TlsfAllocator> 0x005D6A60, <ZoneAllocatorAdapter> 0x00506150
        virtual ~BaseMemoryAllocator(); // vtbl+0x00 <TlsfAllocator> 0x005D36E0, <ZoneAllocatorAdapter> 0x004D8410
        bool create(MemoryBlock block, uint32 creationFlags, const char* pName); // <TlsfAllocator> 0x005D3710, <ZoneAllocatorAdapter> 0x005D6910
        MemoryBlock destroy(); // <TlsfAllocator> 0x005D37C0, <ZoneAllocatorAdapter> 0x005D3280
        virtual void* allocate(uint32 size, uint32 alignment, uint32 flags, const char* pName); // vtbl+0x04 <TlsfAllocator> 0x005D6AA0, <ZoneAllocatorAdapter> 0x005061A0
        virtual void free(void* pAddress); // vtbl+0x08 <TlsfAllocator> 0x005D3870, <ZoneAllocatorAdapter> 0x004D8450
        bool containsPointer(void*) const;
        const void* getBaseAddress() const;
        const uint32 getTotalSize() const; // <TlsfAllocator> 0x005D38D0, <ZoneAllocatorAdapter> 0x004D96E0
        bool isFlagSet(uint32) const;
        virtual const char* getName() const; // vtbl+0x0C <TlsfAllocator> 0x005D3700, <ZoneAllocatorAdapter> 0x004D8440

    protected:
        ThisType& operator=(const ThisType&);

        Mutex m_mutex; // 0x04
        char m_name[128]; // 0x3C
        TAllocator m_allocator; // 0xBC
        MemoryBlock m_memoryBlock; // 0xBC + sizeof(TAllocator)
        uint32 m_allocatedSize;
        uint32 m_maxAllocatedSize;
        uint32 m_allocationCount;
        uint32 m_flags;
    };
}

#include "../../../sources/global/basememoryallocator.inl"
