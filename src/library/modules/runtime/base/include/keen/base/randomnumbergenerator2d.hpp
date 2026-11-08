// Original: library/modules/runtime/base/include/keen/base/randomnumbergenerator2d.hpp
// Implementation: library/modules/runtime/base/sources/global/randomnumbergenerator2d.cpp
//
// RandomNumberGenerator2D, a hash-based generator over 2D coordinates.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x1
    class RandomNumberGenerator2D
    {
    public:
        static float getRandomValue(uint32 x, uint32 y, float min, float max); // 0x00BF1420 (:26)
        static uint32 getRandomValue(uint32 x, uint32 y); // 0x00BF1400 (:12)
        static uint32 getRange(); // 0x00BF13F0 (:7)

    };
}
