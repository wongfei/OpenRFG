// Original: library/modules/runtime/animation/include/keen/animation/animationlve.hpp
//
// AnimationLve, a joint transform without scale (orientation and position).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/quaternion.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    // size 0x1C
    struct AnimationLve
    {
        AnimationLve(); // 0x00CBDD90

        Quaternion orientation; // 0x00
        Vector3 position; // 0x10
    };
}
