// Original: library/modules/runtime/base/include/keen/base/float32.hpp
// Implementation: library/modules/runtime/base/sources/global/float32.cpp
//
// Float parsing and formatting (readFloat32, writeFloat32 with Float32WriteFormat).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum Float32WriteFormat
    {
        Float32WriteFormat_Hex = 0,
        Float32WriteFormat_HexShort = 1,
        Float32WriteFormat_Float = 2,
        Float32WriteFormat_FloatShort = 3,
    };

    uint32 writeFloat32(float value, char* pBuffer, uint32 capacity, Float32WriteFormat); // 0x00C08360 (:14)
    const char* readFloat32(float* pResult, const char* pBuffer); // 0x00BEFFD0 (:76)
}
