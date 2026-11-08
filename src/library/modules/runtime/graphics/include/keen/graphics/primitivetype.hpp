// Original: library/modules/runtime/graphics/include/keen/graphics/primitivetype.hpp
//
// Primitive topologies of draw commands.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum PrimitiveType
    {
        PrimitiveType_Invalid = -1,
        PrimitiveType_TriangleList = 0,
        PrimitiveType_TriangleStrip = 1,
        PrimitiveType_LineList = 2,
        PrimitiveType_LineStrip = 3,
        PrimitiveType_QuadList = 4,
        PrimitiveType_Count = 5,
    };
}
