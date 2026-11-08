// Original: library/modules/runtime/base/sources/win32/systemtimer_win32.cpp
//
// keen::SystemTimer, a millisecond stopwatch over the performance counter (counter * 1000 / frequency, in 32 bits).
// The PDB also names global/systemtimer.cpp; which members live there is not recorded, so all are placed here.

#include "keen/base/systemtimer.hpp"

#include <windows.h>

namespace keen
{
    // The performance counter in milliseconds (inlined into every member).
    static uint32 queryMilliseconds()
    {
        LARGE_INTEGER frequency;
        LARGE_INTEGER counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        return (uint32)(counter.QuadPart * 1000 / frequency.QuadPart);
    }

    // 0x00BFB120
    SystemTimer::SystemTimer()
    {
        m_startTime = queryMilliseconds();
    }

    // 0x00BF2DD0
    void SystemTimer::start()
    {
        m_startTime = queryMilliseconds();
    }

    // 0x00BF2E20
    bool SystemTimer::hasTimeElapsed(uint32 milliseconds) const
    {
        return milliseconds < queryMilliseconds() - m_startTime;
    }

    // 0x00BF2E80
    uint32 SystemTimer::getElapsedTime() const
    {
        return queryMilliseconds() - m_startTime;
    }

    // 0x00BF2ED0
    uint32 SystemTimer::restart()
    {
        const uint32 now = queryMilliseconds();
        const uint32 elapsed = now - m_startTime;
        m_startTime = now;
        return elapsed;
    }

    // 0x00BEDCA0
    uint32 SystemTimer::getCurrentMilliseconds()
    {
        return queryMilliseconds();
    }
}
