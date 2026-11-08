// Original: library/modules/runtime/base/include/keen/base/memoryallocator.hpp
#pragma once

#include "keen/base/memory.hpp"

namespace keen
{
    // Abstract allocator interface. Every keen system takes a MemoryAllocator*.
    // size 0x4
    class MemoryAllocator
    {
    public:
        virtual ~MemoryAllocator() = 0; // vtbl+0x00 0x00BF0ED0
        virtual void* allocate(uint32 size, uint32 alignment, uint32 flags, const char* pName) = 0; // vtbl+0x04
        void free(const void* pAddress) // 0x00BED560 (.h)
        {
            free((void*)pAddress);
        }
        virtual void free(void* pAddress) = 0; // vtbl+0x08
        virtual const char* getName() const = 0; // vtbl+0x0C
        MemoryAllocator() // 0x004D9530 (.h)
        {
        }

        // 0x00 vfptr
    };

    // 0x00BED570 (.h:94)
    static inline MemoryBlock allocateMemoryBlock(MemoryAllocator* pAllocator, uint32 size, uint32 alignment, uint32 flags, const char* pName)
    {
        return createMemoryBlock(pAllocator->allocate(size, alignment, flags, pName), size);
    }

    // 0x00BED590 (.h:102)
    static inline void freeMemoryBlock(MemoryAllocator* pAllocator, MemoryBlock block)
    {
        pAllocator->free(block.pStart);
    }

    // Calls the destructor and returns the memory to pAllocator (instantiated for most keen systems, e.g. 0x00C04640 for FileSystem).
    template<typename T>
    void deleteObject(T* pObject, MemoryAllocator* pAllocator)
    {
        if (pObject != nullptr)
        {
            pObject->~T();
            pAllocator->free(pObject);
        }
    }

    // Memory block that remembers its allocator and frees itself on destruction.
    // size 0xC
    class AutoMemoryBlock : public MemoryBlock
    {
    public:
        AutoMemoryBlock(); // 0x00CDC350
        ~AutoMemoryBlock(); // 0x00CDD600
        bool create(MemoryAllocator* pAllocator, uint32 inSize, uint32 alignment, uint32 flags, const char* pName); // 0x00CDD640
        void destroy(); // 0x00CDC360

    private:
        MemoryAllocator* m_pAllocator; // 0x08
    };
}
