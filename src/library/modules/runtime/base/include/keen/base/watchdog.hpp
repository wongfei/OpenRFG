// Original: library/modules/runtime/base/include/keen/base/watchdog.hpp
// Implementation: library/modules/runtime/base/sources/global/watchdog.cpp
//
// The watchdog thread that detects hangs.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace watchdog
    {
        void initalize(); // 0x00C0E260 (.h:50)
        void shutdown(); // 0x00C0E270 (.h:51)
    }
}
