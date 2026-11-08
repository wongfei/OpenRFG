// Original: library/modules/runtime/graphics/include/keen/graphics/indexformat.hpp
//
// Index buffer formats.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum IndexFormat
    {
        IndexFormat_Invalid = 0,
        IndexFormat_Uint16 = 1,
        IndexFormat_Uint32 = 2,
        IndexFormat_Count = 3,
    };
}
