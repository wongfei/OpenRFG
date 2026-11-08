// Original: library/modules/runtime/base/include/keen/base/float16.hpp
// Implementation: library/modules/runtime/base/sources/global/float16.cpp
//
// Conversions between 16-bit and 32-bit floats.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    void convertFloat16ToFloat32(float* pTarget, uint16 value); // 0x00BEFE50 (:193)

    inline float convertFloat16ToFloat32(uint16 value) // 0x00C5B100 (.h:22)
    {
        float result;
        convertFloat16ToFloat32(&result, value);
        return result;
    }

    void convertFloat32ToFloat16(uint16* pTarget, float value); // 0x00BEFEC0 (:235)
}
