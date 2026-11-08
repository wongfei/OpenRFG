// Original: library/modules/runtime/base/include/keen/base/systemtimer.hpp
// Implementation: library/modules/runtime/base/sources/global/systemtimer.cpp,
//                 library/modules/runtime/base/sources/win32/systemtimer_win32.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // Millisecond stopwatch.
    // size 0x4
    class SystemTimer
    {
    public:
        SystemTimer(); // 0x00BFB120
        void start(); // 0x00BF2DD0
        bool hasTimeElapsed(uint32 milliseconds) const; // 0x00BF2E20
        uint32 getElapsedTime() const; // 0x00BF2E80
        uint32 restart(); // 0x00BF2ED0
        static uint32 getCurrentMilliseconds(); // 0x00BEDCA0

    private:
        uint32 m_startTime; // 0x00
    };
}
