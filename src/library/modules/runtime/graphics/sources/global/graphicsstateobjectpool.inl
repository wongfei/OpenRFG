// Inline part of library/modules/runtime/graphics/sources/global/graphicsstateobjectpool.hpp (included by it):
// GraphicsStateObjectPool<T>, a fixed pool of state objects plus the cache that shares them by description hash. The
// addresses are of the <BlendState> instantiation unless noted.
#pragma once

#include "keen/base/memoryallocator.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    // 0x00C6D490 (.h:26)
    template<typename T>
    GraphicsStateObjectPool<T>::GraphicsStateObjectPool()
    {
    }

    // 0x00C6D920
    template<typename T>
    GraphicsStateObjectPool<T>::~GraphicsStateObjectPool()
    {
    }

    // 0x00C6F8D0 (.h:31)
    template<typename T>
    void GraphicsStateObjectPool<T>::create(MemoryAllocator* pAllocator, uint32 capacity)
    {
        MemoryBlock poolMemory;
        poolMemory.size = capacity * sizeof(T);
        poolMemory.pStart = (uint8*)pAllocator->allocate(poolMemory.size, 16, 0, nullptr);
        m_allocator.create(poolMemory);
        m_cache.create(pAllocator, capacity);
        m_cacheHits = 0;
        m_cacheRequests = 0;
        m_peakSize = 0;
    }

    // 0x00C71040 (.h:41)
    template<typename T>
    void GraphicsStateObjectPool<T>::destroy(MemoryAllocator* pAllocator)
    {
        if (m_allocator.getSize() != 0)
        {
            debug::nativePrintArgs("Graphics State Object Leak detected!\n");
        }
        const MemoryBlock poolMemory = m_allocator.destroy();
        pAllocator->free(poolMemory.pStart);
        m_cache.destroy(pAllocator);
    }

    // 0x00C710A0 (.h:52)
    template<typename T>
    bool GraphicsStateObjectPool<T>::allocateObject(T** ppResult, uint32 hash)
    {
        // Returns true when a new object was allocated (the caller then creates the platform object), false when an
        // existing one was shared (its reference count was incremented).
        T* pObject = (T*)m_cache.acquireObject(hash);
        ++m_cacheRequests;
        if (pObject != nullptr)
        {
            *ppResult = pObject;
            ++m_cacheHits;
            return false;
        }
        pObject = m_allocator.allocateObject();
        m_cache.addNewObject(hash, pObject);
        *ppResult = pObject;
        const uint32 usedCount = m_cache.getUsedObjectCount();
        m_peakSize = usedCount < m_peakSize ? m_peakSize : usedCount;
        return true;
    }

    // <VertexFormat> 0x00C60640 (.h:93)
    template<typename T>
    void GraphicsStateObjectPool<T>::addObjectReference(const T* pObject)
    {
        ++const_cast<T*>(pObject)->refCount;
    }

    // 0x00C6F980 (.h:98)
    template<typename T>
    bool GraphicsStateObjectPool<T>::freeObject(const T* pObject)
    {
        if (!m_cache.releaseObject(pObject))
        {
            return false;
        }
        m_allocator.freeObject(const_cast<T*>(pObject));
        return true;
    }

    // 0x00C60600 (.h:121)
    template<typename T>
    void GraphicsStateObjectPool<T>::trace(const char* pObjectName)
    {
        // Empty in the shipped build.
    }
}
