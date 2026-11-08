// Original: library/modules/runtime/base/include/keen/base/poolallocator.hpp
// Implementation: library/modules/runtime/base/sources/global/poolallocator.inl
#pragma once

#include "keen/base/memory.hpp"

namespace keen
{
    class MemoryAllocator;

    // Fixed-size element pool with an intrusive free list threaded through unused slots.
    // size 0x18
    class BasePoolAllocator
    {
        BasePoolAllocator(const BasePoolAllocator&);
    public:
        BasePoolAllocator(); // 0x00BED6A0
        ~BasePoolAllocator(); // 0x00BED6B0
        bool create(MemoryBlock memoryBlock, uint32 elementSize, uint32 alignment); // 0x00C0A4D0
        MemoryBlock destroy(); // 0x00BED6C0
        uint32 allocateIndex(); // 0x004DB5A0
        void freeIndex(uint32 index); // 0x004DB5D0
        void* allocate(); // 0x004DB5F0
        void free(void* pItem); // 0x004DB630
        uint32 getSize() const; // 0x00BED6F0
        uint32 getElementSize() const;
        uint32 getCapacity() const; // 0x00BED700
        uint32 getIndexFromObject(const void* pObject) const; // 0x004B21C0
        const void* getObjectFromIndex(uint32 index) const; // 0x00BED730
        void* getObjectFromIndex(uint32 index); // 0x00BED710

    private:
        BasePoolAllocator& operator=(const BasePoolAllocator&);
        uint32 getNextFreeIndex(uint32 freeIndex); // 0x004B21D0
        void setNextFreeIndex(uint32 freeIndex, uint32 nextIndex); // 0x004B21E0

        MemoryBlock m_memoryBlock; // 0x00
        uint32 m_capacity; // 0x08
        uint32 m_size; // 0x0C
        uint32 m_elementSize; // 0x10
        uint32 m_firstFreeIndex; // 0x14
    };

    // Typed front end; newObject/deleteObject run constructors/destructors, allocateObject/freeObject do not.
    // size 0x18
    template<typename T>
    class PoolAllocator
    {
        PoolAllocator(const PoolAllocator<T>&);
    public:
        PoolAllocator(); // <FileSystemMountPoint> 0x00BF39B0
        ~PoolAllocator(); // <FileSystemMountPoint> 0x00BF39C0
        bool create(MemoryBlock memoryBlock); // <FileSystemMountPoint> 0x00C0C6E0
        MemoryBlock destroy(); // <FileSystemMountPoint> 0x00BF39D0
        bool createFromAllocator(MemoryAllocator*, uint32, const char*);
        void destroyFromAllocator(MemoryAllocator*);
        T* allocateObject(); // <FileSystemMountPoint> 0x00BFBE20
        void freeObject(T* pObject); // <FileSystemMountPoint> 0x00BFBE60
        T* newObject(); // <FileSystemMountPoint> 0x00C00C50
        void deleteObject(T* pObject); // <FileSystemMountPoint> 0x00C00CB0
        uint32 getIndexFromObject(const T* pObject) const; // <FileSystemMountPoint> 0x00BF3A00
        const T* getObjectFromIndex(uint32) const;
        T* getObjectFromIndex(uint32 index); // <FileSystemMountPoint> 0x00BF3A10
        uint32 getSize() const;
        uint32 getCapacity() const;
        bool isEmpty() const;
        bool isFull() const;

    private:
        PoolAllocator<T>& operator=(const PoolAllocator<T>&);

        BasePoolAllocator m_pool; // 0x00
    };
}

#include "../../../sources/global/poolallocator.inl"
