// Original: library/modules/runtime/base/sources/global/poolallocator.inl (included by poolallocator.hpp)
//
// BasePoolAllocator threads its free list through the unused slots: the first uint32 of a free slot is the index of
// the next free slot. Allocating when full returns null (allocateIndex returns the capacity). create() reports a too
// small block but still returns true (kept as in the original).
#pragma once

#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"

#include <new>

namespace keen
{
    // 0x00BED6A0 (:7)
    inline BasePoolAllocator::BasePoolAllocator()
    {
        m_memoryBlock.pStart = nullptr;
        m_memoryBlock.size = 0;
    }

    // 0x00BED6B0 (:12)
    inline BasePoolAllocator::~BasePoolAllocator()
    {
    }

    // 0x00C0A4D0 (:17)
    inline bool BasePoolAllocator::create(MemoryBlock memoryBlock, uint32 elementSize, uint32 alignment)
    {
        if (memoryBlock.pStart == nullptr || memoryBlock.size < elementSize)
        {
            debug::nativePrintArgs("[base] not enough memory given to BasePoolAllocator::create()!\n");
            return true;
        }
        if ((uint8*)((uint32)memoryBlock.pStart & ~(alignment - 1)) != memoryBlock.pStart)
        {
            debug::nativePrintArgs("[base] alignment error in BasePoolAllocator::create()!\n");
            return false;
        }
        const uint32 capacity = memoryBlock.size / elementSize;
        if (capacity == 0)
        {
            return false;
        }
        m_memoryBlock = memoryBlock;
        m_capacity = capacity;
        m_size = 0;
        m_elementSize = elementSize;
        m_firstFreeIndex = 0;
        for (uint32 i = 0; i < m_capacity; ++i)
        {
            setNextFreeIndex(i, i + 1);
        }
        return true;
    }

    // 0x00BED6C0 (:61)
    inline MemoryBlock BasePoolAllocator::destroy()
    {
        const MemoryBlock block = m_memoryBlock;
        m_memoryBlock.pStart = nullptr;
        m_memoryBlock.size = 0;
        m_firstFreeIndex = 0;
        m_size = 0;
        m_capacity = 0;
        return block;
    }

    // 0x004DB5A0 (:87)
    inline uint32 BasePoolAllocator::allocateIndex()
    {
        if (m_size >= m_capacity)
        {
            return m_capacity;
        }
        const uint32 index = m_firstFreeIndex;
        ++m_size;
        m_firstFreeIndex = getNextFreeIndex(index);
        return index;
    }

    // 0x004DB5D0 (:101)
    inline void BasePoolAllocator::freeIndex(uint32 index)
    {
        setNextFreeIndex(index, m_firstFreeIndex);
        --m_size;
        m_firstFreeIndex = index;
    }

    // 0x004DB5F0 (:110)
    inline void* BasePoolAllocator::allocate()
    {
        const uint32 index = allocateIndex();
        if (index >= m_capacity)
        {
            return nullptr;
        }
        return m_memoryBlock.pStart + m_elementSize * index;
    }

    // 0x004DB630 (:123)
    inline void BasePoolAllocator::free(void* pItem)
    {
        freeIndex(getIndexFromObject(pItem));
    }

    // 0x00BED6F0 (:130)
    inline uint32 BasePoolAllocator::getSize() const
    {
        return m_size;
    }

    inline uint32 BasePoolAllocator::getElementSize() const
    {
        return m_elementSize;
    }

    // 0x00BED700
    inline uint32 BasePoolAllocator::getCapacity() const
    {
        return m_capacity;
    }

    // 0x004B21C0
    inline uint32 BasePoolAllocator::getIndexFromObject(const void* pObject) const
    {
        return (uint32)((const uint8*)pObject - m_memoryBlock.pStart) / m_elementSize;
    }

    // 0x00BED730
    inline const void* BasePoolAllocator::getObjectFromIndex(uint32 index) const
    {
        if (index >= m_capacity)
        {
            return nullptr;
        }
        return m_memoryBlock.pStart + m_elementSize * index;
    }

    // 0x00BED710
    inline void* BasePoolAllocator::getObjectFromIndex(uint32 index)
    {
        if (index >= m_capacity)
        {
            return nullptr;
        }
        return m_memoryBlock.pStart + m_elementSize * index;
    }

    // 0x004B21D0
    inline uint32 BasePoolAllocator::getNextFreeIndex(uint32 freeIndex)
    {
        return *(uint32*)(m_memoryBlock.pStart + m_elementSize * freeIndex);
    }

    // 0x004B21E0
    inline void BasePoolAllocator::setNextFreeIndex(uint32 freeIndex, uint32 nextIndex)
    {
        *(uint32*)(m_memoryBlock.pStart + m_elementSize * freeIndex) = nextIndex;
    }

    // ---- PoolAllocator<T>

    // <FileSystemMountPoint> 0x00BF39B0
    template<typename T>
    PoolAllocator<T>::PoolAllocator()
    {
    }

    // <FileSystemMountPoint> 0x00BF39C0
    template<typename T>
    PoolAllocator<T>::~PoolAllocator()
    {
    }

    // <FileSystemMountPoint> 0x00C0C6E0
    template<typename T>
    bool PoolAllocator<T>::create(MemoryBlock memoryBlock)
    {
        return m_pool.create(memoryBlock, sizeof(T), alignof(T));
    }

    // <FileSystemMountPoint> 0x00BF39D0
    template<typename T>
    MemoryBlock PoolAllocator<T>::destroy()
    {
        return m_pool.destroy();
    }

    // <NativeFileDevice::FileStreamData> 0x00C0CE40
    template<typename T>
    bool PoolAllocator<T>::createFromAllocator(MemoryAllocator* pAllocator, uint32 capacity, const char* pName)
    {
        const uint32 size = capacity * sizeof(T);
        return create(createMemoryBlock(pAllocator->allocate(size, alignof(T), 0, pName), size));
    }

    // <NativeFileDevice::FileStreamData> 0x00BFBD10
    template<typename T>
    void PoolAllocator<T>::destroyFromAllocator(MemoryAllocator* pAllocator)
    {
        pAllocator->free(m_pool.destroy().pStart);
    }

    // <FileSystemMountPoint> 0x00BFBE20
    template<typename T>
    T* PoolAllocator<T>::allocateObject()
    {
        return (T*)m_pool.allocate();
    }

    // <FileSystemMountPoint> 0x00BFBE60
    template<typename T>
    void PoolAllocator<T>::freeObject(T* pObject)
    {
        m_pool.free(pObject);
    }

    // <FileSystemMountPoint> 0x00C00C50
    template<typename T>
    T* PoolAllocator<T>::newObject()
    {
        T* pObject = allocateObject();
        if (pObject != nullptr)
        {
            new (pObject) T();
        }
        return pObject;
    }

    // <FileSystemMountPoint> 0x00C00CB0
    template<typename T>
    void PoolAllocator<T>::deleteObject(T* pObject)
    {
        if (pObject != nullptr)
        {
            pObject->~T();
            m_pool.free(pObject);
        }
    }

    // <FileSystemMountPoint> 0x00BF3A00
    template<typename T>
    uint32 PoolAllocator<T>::getIndexFromObject(const T* pObject) const
    {
        return m_pool.getIndexFromObject(pObject);
    }

    template<typename T>
    const T* PoolAllocator<T>::getObjectFromIndex(uint32 index) const
    {
        return (const T*)m_pool.getObjectFromIndex(index);
    }

    // <FileSystemMountPoint> 0x00BF3A10
    template<typename T>
    T* PoolAllocator<T>::getObjectFromIndex(uint32 index)
    {
        return (T*)m_pool.getObjectFromIndex(index);
    }

    template<typename T>
    uint32 PoolAllocator<T>::getSize() const
    {
        return m_pool.getSize();
    }

    template<typename T>
    uint32 PoolAllocator<T>::getCapacity() const
    {
        return m_pool.getCapacity();
    }

    template<typename T>
    bool PoolAllocator<T>::isEmpty() const
    {
        return m_pool.getSize() == 0;
    }

    // <FileSystemDeviceEntry> 0x00BF39A0
    template<typename T>
    bool PoolAllocator<T>::isFull() const
    {
        return m_pool.getSize() == m_pool.getCapacity();
    }
}
