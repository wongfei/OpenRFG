// Original: library/modules/runtime/base/include/keen/base/thread.hpp
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/win32/thread_win32.hpp"

namespace keen
{
    // Implemented in sources/win32/thread_win32.cpp.
    void sleepCurrentThreadMilliseconds(uint32 milliseconds); // 0x00BEDE80
    void sleepCurrentThreadMicroseconds(uint32 nanoseconds); // 0x00BEDE90
    uint32 getCurrentThreadId(); // 0x00BEDEB0
    Thread* getCurrentThread(); // 0x00BEDEC0
    uint32 getAvailableCpuHardwareThreadMask(); // 0x00BF4840
}
