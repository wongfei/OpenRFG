// Original: library/modules/runtime/base/include/keen/base/ieee754.hpp
// Implementation: library/modules/runtime/base/sources/global/ieee754.cpp
//
// IEEE 754 classification of floats (NaN, infinities, denormals, signed zero).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    bool isZero(float f); // 0x00BF05C0 (:7)
    bool isNormalizedFloat(float f); // 0x00BF05E0 (:21)
    bool isNanFloat(float f); // 0x00BF0610 (:36)
    bool isPositiveInfiniteFloat(float f); // 0x00BF0640 (:45)
    bool isNegativeInfiniteFloat(float f); // 0x00BF0680 (:54)
    bool isDenormalizedFloat(float f); // 0x00BF06C0 (:63)
    bool isPositiveZeroFloat(float f); // 0x00BF06F0 (:77)
    bool isNegativeZeroFloat(float f); // 0x00BF0710 (:88)
}
