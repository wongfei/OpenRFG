// Original: library/modules/runtime/resources/include/keen/resources/resourceallocationcontexts.hpp
// Implementation: library/modules/runtime/resources/sources/resourceallocationcontexts.cpp
//
// Resource allocation contexts: the allocators resources of a context are created with (ResourceContextDescriptor,
// context indices). Placement of the types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/autoarray.hpp"

namespace keen
{
    class MemoryAllocator;
    struct ResourceAllocationContext;

    enum ResourceContextIndex
    {
        ResourceContextIndex_Root = 255,
    };

    // size 0x8
    struct ResourceContextDescriptor
    {
        const ResourceAllocationContext* pAllocationContext; // 0x00
        uint8 parentIndex; // 0x04
        uint8 resourceTag; // 0x05
    };

    // size 0xC
    struct ResourceAllocationContext
    {
        ResourceAllocationContext(); // 0x00CDFB90
        ~ResourceAllocationContext(); // 0x00CDF1A0

        static const uint32 ResourceObjectAllocatorIndex; // 0x01340B28
        AutoArray<MemoryAllocator*> allocators; // 0x00
    };

    ResourceAllocationContext* createResourceAllocationContext(MemoryAllocator* pAllocator, uint32 allocatorCount); // 0x00CE02A0 (:11)
    void destroyResourceAllocationContext(ResourceAllocationContext* pContext, MemoryAllocator* pAllocator); // 0x00CE07F0 (:37)
}
