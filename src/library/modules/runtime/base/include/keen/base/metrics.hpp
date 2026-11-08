// Original: library/modules/runtime/base/include/keen/base/metrics.hpp
// Implementation: library/modules/runtime/base/sources/global/metrics.cpp
//
// Metrics, run-time counters reported to the debug connection.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace Metrics
    {
        void shutdown(); // 0x00C0E280 (.h:81)
        void update(); // 0x00C0E290 (.h:83)
    }
}
