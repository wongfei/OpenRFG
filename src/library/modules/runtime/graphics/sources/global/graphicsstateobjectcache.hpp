// Original: library/modules/runtime/graphics/sources/global/graphicsstateobjectcache.hpp
// Implementation: library/modules/runtime/graphics/sources/global/graphicsstateobjectcache.cpp
//
// GraphicsStateObjectCache, a hash map from state description hash to the shared state object.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/hashmap.hpp"
#include "keen/base/hashmaptraits.hpp"

namespace keen
{
    struct GraphicsStateObject;
    class MemoryAllocator;

    // size 0x30
    class GraphicsStateObjectCache
    {
    public:
        typedef HashMap<uint32,GraphicsStateObject*,DefaultHashmapTraits<uint32,GraphicsStateObject*> > StateObjectMap;

        GraphicsStateObjectCache(); // 0x00C6C420 (:8)
        void create(MemoryAllocator* pAllocator, uint32 capacity); // 0x00C6F620 (:12)
        void destroy(MemoryAllocator* pAllocator); // 0x00C70640 (:17)
        GraphicsStateObject* acquireObject(uint32 hash) const; // 0x00C6F640 (:22)
        void addNewObject(uint32 hash, GraphicsStateObject* pObject); // 0x00C70650 (:39)
        void addObjectReference(const GraphicsStateObject* pObject); // 0x00C5D100 (:47)
        bool releaseObject(const GraphicsStateObject* pObject); // 0x00C6F690 (:53)
        uint32 getUsedObjectCount() const; // 0x00C60F20 (.h:32)
        uint32 getCapacity() const;
        ~GraphicsStateObjectCache(); // 0x00C6A970

    private:
        HashMap<uint32,GraphicsStateObject*,DefaultHashmapTraits<uint32,GraphicsStateObject*> > m_stateObjects; // 0x00
    };
}
