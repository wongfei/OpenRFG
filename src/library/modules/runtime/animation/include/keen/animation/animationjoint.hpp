// Original: library/modules/runtime/animation/include/keen/animation/animationjoint.hpp
//
// AnimationJoint, one joint of an animation pose (orientation, position, scale).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/quaternion.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    // size 0x28
    struct AnimationJoint
    {
        AnimationJoint(); // 0x00CC8FA0

        Quaternion orientation; // 0x00
        Vector3 position; // 0x10
        Vector3 scale; // 0x1C
    };
}
