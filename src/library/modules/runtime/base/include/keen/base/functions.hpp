// Original: library/modules/runtime/base/include/keen/base/functions.hpp
// Implementation: library/modules/runtime/base/sources/global/functions.cpp
//
// Generic helpers (abs, min/max, alignValue, approachValue, swap, constructor calls); function templates,
// instantiations listed as comments.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // template instance: float min<float>(float a, float b); // 0x00BED4F0 (.h:43)
    // template instance: float min<float>(float a, float b, float c); // 0x00CE2BB0 (.h:85)
    // template instance: float max<float>(float a, float b); // 0x009417D0 (.h:110)
    // template instance: float max<float>(float a, float b, float c); // 0x00C74290 (.h:152)
    // template instance: int abs<int>(int x); // 0x00BED530 (.h:226)
    // template instance: float abs<float>(float x); // 0x00C461E0 (.h:251)
    float clamp01(float x); // 0x00458960 (.h:293)
    // template instance: uint32 getNextPowerOfTwo<uint32>(uint32 x); // 0x00C59F00 (.h:802)
    int isqrt(int x); // 0x00BF8290 (:17)
    int extendSign(uint32 x, int signBitPosition); // 0x00BF0420 (:36)
    void clampCircular(float* pX, float* pY); // 0x00BF0440 (:56)
    float approachValue(float currentValue, float targetValue, float step); // 0x00BF04B0 (:87)
    void computeDomainTransformationValues(float* pScale, float* pOffset, float sourceMin, float sourceMax, float targetMin, float targetMax); // 0x00BF0550 (:99)
}
