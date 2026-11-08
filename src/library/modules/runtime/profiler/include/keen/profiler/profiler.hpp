// Original: library/modules/runtime/profiler/include/keen/profiler/profiler.hpp
// Implementation: library/modules/runtime/profiler/sources/global/profiler.cpp
//
// The profiler (zones per thread; compiled to stubs in the shipped game).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct GraphicsSystem;

    // size 0xC
    struct ProfilerSettings
    {
        ProfilerSettings(); // 0x00C0E2A0 (.h:46)

        uint32 maxThreadCount; // 0x00
        uint32 maxZoneCount; // 0x04
        uint32 maxZoneVisitCount; // 0x08
    };

    namespace profiler
    {
        void initialize(GraphicsSystem*, const ProfilerSettings&); // 0x00C0E2C0 (.h:288)
        void shutdown(); // 0x00C0E2D0 (.h:290)
        void beginFrame(); // 0x00C0E2E0 (.h:291)
    }
}
