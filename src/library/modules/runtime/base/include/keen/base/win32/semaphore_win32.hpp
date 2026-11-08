// Original: library/modules/runtime/base/include/keen/base/win32/semaphore_win32.hpp
// Implementation: library/modules/runtime/base/sources/win32/semaphore_win32.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    class Semaphore
    {
    public:
        enum
        {
            TimeOutInfinite = -1,
        };

        Semaphore(); // 0x00BEDBB0
        ~Semaphore(); // 0x00BEDBC0
        void create(const char* pDebugname, int initialValue, int maxValue); // 0x00C074E0
        void destroy(); // 0x00BEDBD0
        void incrementValue(int value); // 0x00BEDC20
        bool tryDecrementValue(uint32 timeOutInMS); // 0x00BEDBF0

    private:
        void* m_semaphoreHandle; // 0x00
        int m_value; // 0x04
    };
}
