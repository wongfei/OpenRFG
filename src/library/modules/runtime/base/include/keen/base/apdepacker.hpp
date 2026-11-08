// Original: library/modules/runtime/base/include/keen/base/apdepacker.hpp
// Implementation: library/modules/runtime/base/sources/global/apdepacker.cpp
//
// aPLib decompression (apdepack), used for compressed resources.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    namespace compression
    {
        uint32 apdepack(const void* source, void* destination); // 0x00BEDFD0 (:59)
    }
}
