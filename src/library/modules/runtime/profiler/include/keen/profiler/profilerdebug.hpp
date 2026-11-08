// Original: library/modules/runtime/profiler/include/keen/profiler/profilerdebug.hpp
// Implementation: library/modules/runtime/profiler/sources/global/profilerdebug.cpp
//
// Debug drawing of profiler zones. Placement of ProfileZonePeakValue inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x104
    struct ProfileZonePeakValue
    {
        enum
        {
            FrameCount = 60,
        };

        ProfileZonePeakValue();

        uint32 maxPeakValue; // 0x00
        uint32 maxPeakAge; // 0x04
        uint32 minPeakValue; // 0x08
        uint32 minPeakAge; // 0x0C
        uint32 values[60]; // 0x10
        uint32 valuePos; // 0x100
    };

    void drawProfileZoneBar(const char*, int, int, int, uint32, ProfileZonePeakValue* pPeakValue, const char* pProcessorName); // 0x00C15610 (:141)
}
