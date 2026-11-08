// Original: library/modules/runtime/base/sources/global/float16.cpp
//
// Partial: float16 -> float32 (vertex attribute decoding).

#include "keen/base/float16.hpp"

#include <string.h>

namespace keen
{
    // 0x00BEFE50 (:193)
    void convertFloat16ToFloat32(float* pTarget, uint16 value)
    {
        uint32 mantissa = value & 0x3ffu;
        uint32 exponent;
        if ((value & 0x7c00u) == 0)
        {
            if (mantissa == 0)
            {
                // Zero: the rebias below brings the exponent back to 0.
                exponent = (uint32)-0x70;
            }
            else
            {
                // Denormal: normalize the mantissa.
                exponent = 1;
                do
                {
                    mantissa <<= 1;
                    --exponent;
                }
                while ((mantissa & 0x400u) == 0);
                mantissa &= 0x3ffu;
            }
        }
        else
        {
            exponent = (value >> 10) & 0x1fu;
        }
        // Magic number: 0x70 = 127 - 15 rebiases the exponent.
        // Notes (decompiled behaviour, kept as is): infinities and NaNs (exponent 31) are not special-cased and become
        // large finite values.
        const uint32 bits = (((value & 0x8000u) << 3) | mantissa) << 13 | (exponent + 0x70) << 23;
        memcpy(pTarget, &bits, sizeof(bits));
    }
}
