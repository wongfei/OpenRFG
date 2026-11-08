// Original: library/modules/runtime/base/sources/win32/profilingtimer_win32.cpp
//
// The profiling timer: time stamps are CPU cycles (rdtsc); updateTimer calibrates the cycles per millisecond against
// the performance counter over the time since its previous call.

#include "keen/base/profilingtimer.hpp"

#include <intrin.h>
#include <windows.h>

namespace keen
{
    namespace profiler
    {
        static bool s_lastUpdateTimeValid; // 0x038B3C50
        static double s_lastUpdateTime; // 0x038B3C58, seconds
        static uint64_t s_lastUpdateCycleCount; // 0x038B3C60
        static double s_cyclesToMilliseconds; // 0x038B3C68

        // 0x00BF4760 (:20)
        bool initializeTimer()
        {
            updateTimer();
            return true;
        }

        // 0x00BEDA00 (:26)
        void shutdownTimer()
        {
        }

        // 0x00BEDA10 (:30)
        void updateTimer()
        {
            LARGE_INTEGER frequency;
            LARGE_INTEGER counter;
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&counter);
            const double time = (double)counter.QuadPart / (double)frequency.QuadPart;
            // cpuid serializes the pipeline before the cycle counter is read.
            int cpuInfo[4];
            __cpuid(cpuInfo, 0);
            const uint64_t cycleCount = __rdtsc();

            uint64_t elapsedCycles = 0u;
            double elapsedTime = 0.0;
            if (s_lastUpdateTimeValid)
            {
                elapsedCycles = cycleCount - s_lastUpdateCycleCount;
                elapsedTime = time - s_lastUpdateTime;
            }
            s_lastUpdateCycleCount = cycleCount;
            s_lastUpdateTime = time;
            // Magic numbers: at least one cycle and 0.1 ms, so the first call (no previous time) gives 0.1 ms per cycle.
            if (elapsedCycles == 0u)
            {
                elapsedCycles = 1u;
            }
            if (elapsedTime <= 0.0)
            {
                elapsedTime = 0.0001;
            }
            s_lastUpdateTimeValid = true;
            s_cyclesToMilliseconds = 1000.0 / ((double)elapsedCycles / elapsedTime);
        }

        // 0x00BEDB30 (:80)
        float getElapsedTimeInMilliseconds(uint64_t beginTime, uint64_t endTime)
        {
            if (beginTime < endTime)
            {
                return (float)((double)(endTime - beginTime) * s_cyclesToMilliseconds);
            }
            return 0.0f;
        }
    }
}
