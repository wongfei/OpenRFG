// Original: library/modules/runtime/base/sources/global/float32_implementation.hpp
// Implementation: library/modules/runtime/base/sources/global/float32_implementation.cpp
//
// Portable float implementation (pf::) used by float32.cpp.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace pf
    {
        float fromi(int a); // 0x00C59E90 (.h:28)
        float fromi(uint32 a); // 0x004B18C0 (.h:29)
        float fromf(float a); // 0x00BED300 (.h:30)
        int toi(float a); // 0x00BED310 (.h:33)
        float mul(float a, float b); // 0x00BED320 (.h:36)
        float div(float a, float b); // 0x00BED340 (.h:37)
        float sqr(float x); // 0x00C327A0 (.h:38)
        float abs(float x); // 0x007CAF30 (.h:63)
        float reciproc(float x); // 0x00C39E00 (.h:67)
        bool isEqual(float a, float b, float relativeEpsilon); // 0x00C46080 (.h:70)
        bool isZero(float a, float relativeEpsilon); // 0x00C46160 (.h:75)
        float saturate(float x); // 0x00CE3C80 (.h:100)
        float pow(float base, float exponent); // 0x00BF0040 (:10)
        float log(float value); // 0x00BF0060 (:15)
    }
}
