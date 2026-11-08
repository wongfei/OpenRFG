// Original: library/modules/runtime/base/include/keen/base/memorytracker.hpp
// Implementation: library/modules/runtime/base/sources/global/memorytracker.cpp
//
// The memory tracker: allocation entries recorded per allocator for leak and usage reports. Placement of
// AllocationEntry inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum AllocationEntryUsage
    {
        AllocationEntryUsage_Generic = 0,
        AllocationEntryUsage_WeakReferences = 1,
    };

    // size 0x8
    struct AllocationEntry
    {
        void* pMemory; // 0x00
        sint16 allocatorIndex; // 0x04
        uint8 allocationUsage; // 0x06
        uint8 _pad0; // 0x07
        static const sint16 DebugAllocatorIndex; // 0x01346F88
    };

    namespace MemoryTracker
    {
        void update(); // 0x00C0E0E0 (.h:138)
    }
}
