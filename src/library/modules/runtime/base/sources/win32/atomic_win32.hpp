// Original: library/modules/runtime/base/sources/win32/atomic_win32.hpp
//
// Win32 atomics (Interlocked*).
#pragma once

#include "keen/base/types.hpp"
#include <stdint.h>

namespace keen
{
    uint32 atomicIncrementUint32(uint32* pValue); // 0x004B6780 (.h:9)
    uint32 atomicDecrementUint32(uint32* pValue); // 0x004B67A0 (.h:10)
    uint32 atomicAddUint32(uint32* pValue, uint32 value); // 0x004B67B0 (.h:11)
    uint32 atomicSubUint32(uint32* pValue, uint32 value); // 0x00563B00 (.h:12)
    int atomicAddSint32(int* pValue, int value); // 0x00BEDB90 (.h:13)
    uint64_t atomicSetUint64(uint64_t* pTarget, uint64_t value); // 0x0048CCF0 (.h:58)
    int64_t atomicSetSint64(int64_t* pTarget, int64_t value); // 0x00482B10 (.h:62)
    uint32 atomicCompareAndSetUint32(uint32* pValue, uint32 compare, uint32 newValue); // 0x004B67D0 (.h:76)
    uint64_t atomicCompareAndSetUint64(uint64_t* pValue, uint64_t compare, uint64_t newValue); // 0x00482B60 (.h:77)
    uint32 atomicSetUint32(uint32* pTarget, uint32 value); // 0x004B67F0 (.h:79)
}
