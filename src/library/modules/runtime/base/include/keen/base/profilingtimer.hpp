// Original: library/modules/runtime/base/include/keen/base/profilingtimer.hpp
// Implementation: library/modules/runtime/base/sources/win32/profilingtimer_win32.cpp
//
// The profiling timer (high resolution elapsed time).
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

namespace keen
{
    namespace profiler
    {
        uint32 getElapsedTimeInMicroSeconds(uint64_t startT, uint64_t endT); // 0x00566B40 (.h:25)
        bool initializeTimer(); // 0x00BF4760 (profilingtimer_win32.cpp:20)
        void shutdownTimer(); // 0x00BEDA00 (profilingtimer_win32.cpp:26)
        void updateTimer(); // 0x00BEDA10 (profilingtimer_win32.cpp:30)
        float getElapsedTimeInMilliseconds(uint64_t beginTime, uint64_t endTime); // 0x00BEDB30 (profilingtimer_win32.cpp:80)
    }
}
