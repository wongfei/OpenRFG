// Original: library/modules/runtime/graphicsbase/include/keen/graphics/modelhierarchy.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/modelhierarchy.cpp
//
// ModelHierarchy (the joint tree of a model) and ModelPose (per-joint SIMD orientation, position and scale).
#pragma once

#include "keen/base/types.hpp"
#include <xmmintrin.h>

namespace keen
{
    struct Matrix43;

    // size 0x10
    struct ModelHierarchy
    {
        enum
        {
            InvalidParentIndex = 65535,
        };

        uint32* pJointNames; // 0x00
        uint16* pJoinParentIndices; // 0x04
        Matrix43* pSkinToJointMatrices; // 0x08
        uint32 jointCount; // 0x0C
    };

    // size 0x8
    struct ModelPose
    {
        __m128* pData; // 0x00
        uint32 alignedJointCount; // 0x04
    };

    namespace graphics
    {
        const __m128* getPoseOrientation(const ModelPose& modelPose); // 0x00CBB770 (.h:36)
        const __m128* getPosePosition(const ModelPose& modelPose); // 0x00CBB780 (.h:37)
        const __m128* getPoseScale(const ModelPose& modelPose); // 0x00CBB790 (.h:38)
        void convertPoseToMatrix(Matrix43* pTargetMatrix, const ModelPose& modelPose, uint32 jointIndex); // 0x00CE3590 (:11)
    }
}
