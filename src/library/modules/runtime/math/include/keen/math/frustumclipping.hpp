// Original: library/modules/runtime/math/include/keen/math/frustumclipping.hpp
// Implementation: library/modules/runtime/math/sources/global/frustumclipping.cpp
//
// Clipping of a line against the frustum planes.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class Frustum;
    struct Line;

    bool clipLineOnFrustum(Line* pResult, const Line& line, const Frustum& frustum, uint32 planeMask); // 0x00C3DA70 (:27)
}
