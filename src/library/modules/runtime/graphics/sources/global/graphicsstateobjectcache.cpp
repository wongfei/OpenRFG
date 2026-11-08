// Original: library/modules/runtime/graphics/sources/global/graphicsstateobjectcache.cpp
//
// GraphicsStateObjectCache: state objects by description hash, reference counted.

#include "library/modules/runtime/graphics/sources/global/graphicsstateobjectcache.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"

namespace keen
{
    // 0x00C6C420 (:8)
    GraphicsStateObjectCache::GraphicsStateObjectCache()
    {
    }

    // 0x00C6A970
    GraphicsStateObjectCache::~GraphicsStateObjectCache()
    {
    }

    // 0x00C6F620 (:12)
    void GraphicsStateObjectCache::create(MemoryAllocator* pAllocator, uint32 capacity)
    {
        m_stateObjects.create(pAllocator, capacity, nullptr);
    }

    // 0x00C70640 (:17)
    void GraphicsStateObjectCache::destroy(MemoryAllocator* pAllocator)
    {
        m_stateObjects.destroy(pAllocator);
    }

    // 0x00C6F640 (:22)
    GraphicsStateObject* GraphicsStateObjectCache::acquireObject(uint32 hash) const
    {
        StateObjectMap::EntryList::ConstIterator it = m_stateObjects.find(hash);
        if (it == m_stateObjects.getEnd())
        {
            return nullptr;
        }
        GraphicsStateObject* pObject = it->value;
        ++pObject->refCount;
        return pObject;
    }

    // 0x00C70650 (:39)
    void GraphicsStateObjectCache::addNewObject(uint32 hash, GraphicsStateObject* pObject)
    {
        pObject->refCount = 1;
        pObject->hash = hash;
        m_stateObjects.insert(hash, pObject);
    }

    // 0x00C5D100 (:47)
    void GraphicsStateObjectCache::addObjectReference(const GraphicsStateObject* pObject)
    {
        ++const_cast<GraphicsStateObject*>(pObject)->refCount;
    }

    // 0x00C6F690 (:53)
    bool GraphicsStateObjectCache::releaseObject(const GraphicsStateObject* pObject)
    {
        // Notes (decompiled behaviour, kept as is): the reference count of the cached entry is decremented without
        // checking that the hash was found.
        StateObjectMap::EntryList::Iterator it = m_stateObjects.find(pObject->hash);
        if (--it->value->refCount == 0)
        {
            m_stateObjects.remove(it);
            return true;
        }
        return false;
    }

    // 0x00C60F20 (.h:32)
    uint32 GraphicsStateObjectCache::getUsedObjectCount() const
    {
        return m_stateObjects.getSize();
    }
}
