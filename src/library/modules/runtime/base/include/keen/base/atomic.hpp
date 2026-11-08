// Original: library/modules/runtime/base/include/keen/base/atomic.hpp
//
// Atomic helpers on top of the platform atomics (sources/win32/atomic_win32.hpp).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    uint32 atomicAddSize(uint32* pValue, uint32 value); // 0x004B6800 (.h:71)
}
