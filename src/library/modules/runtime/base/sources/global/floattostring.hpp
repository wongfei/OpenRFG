// Original: library/modules/runtime/base/sources/global/floattostring.hpp
// Implementation: library/modules/runtime/base/sources/global/floattostring.cpp
//
// Float to string conversion (FloatOutputInfo). The big integer helper BigIntUint32<N> is in an anonymous namespace of
// floattostring.cpp. Placement of FloatOutputInfo inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0xC
    struct FloatOutputInfo
    {
        uint32 digitCount; // 0x00
        uint32 dotIndex; // 0x04
        bool isNegative; // 0x08
        bool isSpecial; // 0x09
    };

    bool printFloatToBuffer(FloatOutputInfo* pOutputInfo, char* pBuffer, uint32 capacity, float value, uint32 targetPrecision); // 0x00BFD690 (:133)
}
