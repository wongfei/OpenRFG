// Original: library/modules/runtime/base/sources/win32/profilingtimer_win32.hpp
//
// Win32 time stamp (QueryPerformanceCounter).
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

namespace keen
{
    namespace profiler
    {
        void getCurrentTimeStamp(uint64_t* pResult); // 0x00566B90 (.h:28)
    }
}
