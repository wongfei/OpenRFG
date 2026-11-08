// Original: library/modules/runtime/animation/sources/global/animationcommandbuffer.hpp
// Implementation: library/modules/runtime/animation/sources/global/animationcommandbuffer.cpp
//
// AnimationCommandBuffer and the command execution: decompression of orientations/positions/scales (SIMD), blending
// scopes, matrix composition.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/animation/sources/global/animationcommand.hpp"
#include "keen/base/array.hpp"
#include "keen/base/sizedarray.hpp"

namespace keen
{
    struct AnimationJoint;
    struct AnimationLve;
    struct AnimationSystem;
    struct JointAnimationData;
    struct LveAnimationData;
    struct Matrix43;
    struct ModelHierarchy;
    struct ModelPose;

    // size 0x18
    struct AnimationCommandBuffer
    {
        AnimationCommandBuffer(); // 0x00CC7470
        ~AnimationCommandBuffer(); // 0x00CC7490

        AnimationSystem* pSystem; // 0x00
        Array<uint8> stackData; // 0x04
        SizedArray<AnimationCommand> commands; // 0x0C
    };

    namespace animation
    {
        void writeBeginJointAnimationCommand(AnimationCommandBuffer* pBuffer, const ModelHierarchy& modelHierarchy, const ModelPose& defaultPose, bool writePose); // 0x00CC74A0 (:85)
        void writeSampleJointAnimationCommand(AnimationCommandBuffer* pBuffer, const JointAnimationData& animationData, float time); // 0x00CC74D0 (:103)
        void writeAddJointAnimationsCommand(AnimationCommandBuffer* pBuffer, float weight); // 0x00CC7500 (:118)
        void writeAddJointAnimationsWithMultipleWeightsCommand(AnimationCommandBuffer* pBuffer, float* pWeights, uint32 capacity); // 0x00CC7550 (:148)
        void writeLerpJointAnimationsCommand(AnimationCommandBuffer* pBuffer, float weight); // 0x00CC7590 (:171)
        void writeLerpJointAnimationsWithMultipleWeightsCommand(AnimationCommandBuffer* pBuffer, float* pWeights, uint32 capacity); // 0x00CC75E0 (:201)
        void writeExportJointPoseToMatrixArrayCommand(AnimationCommandBuffer* pBuffer, Matrix43* pMatrices, uint32 capacity); // 0x00CC7620 (:224)
        void writeExportJointPoseToSkinningMatricesCommand(AnimationCommandBuffer* pBuffer, Matrix43* pMatrices, uint32 capacity, bool withSkinToBone); // 0x00CC7660 (:243)
        void writeExportJointPoseToJointArrayCommand(AnimationCommandBuffer* pBuffer, AnimationJoint* pJoints, uint32 capacity); // 0x00CC76A0 (:263)
        void writeEndJointAnimationCommand(AnimationCommandBuffer* pBuffer); // 0x00CC76E0 (:283)
        void writeBeginLveAnimationCommand(AnimationCommandBuffer* pBuffer); // 0x00CC7700 (:298)
        void writeSampleLveAnimationCommand(AnimationCommandBuffer* pBuffer, const LveAnimationData& animation, float startTime, float endTime); // 0x00CC7720 (:311)
        void writeLerpLveAnimationsCommand(AnimationCommandBuffer* pBuffer, float weight); // 0x00CC7760 (:323)
        void writeExportLveStateCommand(AnimationCommandBuffer* pBuffer, AnimationLve* pLveState); // 0x00CC7790 (:333)
        void writeExportLveDeltaCommand(AnimationCommandBuffer* pBuffer, AnimationLve* pLveDelta); // 0x00CC77D0 (:350)
        void writeEndLveAnimationCommand(AnimationCommandBuffer* pBuffer); // 0x00CC7810 (:367)
        void waitForAllCommandsToFinish(AnimationCommandBuffer* pBuffer); // 0x00CC7C70 (:1922)
    }
}
