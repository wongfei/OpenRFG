// Original: library/modules/runtime/animation/sources/global/animationcommand.hpp
//
// The animation commands recorded into an AnimationCommandBuffer: sample/combine joint animations, LVE sampling and
// lerping, and the export commands (joints, matrices, skinning matrices, LVE state/delta). Placement of the command
// records inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct AnimationJoint;
    struct AnimationLve;
    struct JointAnimationData;
    struct LveAnimationData;
    struct Matrix43;
    struct ModelHierarchy;
    struct ModelPose;

    enum AnimationCommandType
    {
        AnimationCommandType_JointBegin = 0,
        AnimationCommandType_JointSample = 1,
        AnimationCommandType_JointSampleAndAdd = 2,
        AnimationCommandType_JointSampleAndLerp = 3,
        AnimationCommandType_JointAdd = 4,
        AnimationCommandType_JointAddWithMultipleWeights = 5,
        AnimationCommandType_JointLerp = 6,
        AnimationCommandType_JointLerpWithMultipleWeights = 7,
        AnimationCommandType_JointExportMatrix = 8,
        AnimationCommandType_JointExportSkinningMatrices = 9,
        AnimationCommandType_JointExportJoint = 10,
        AnimationCommandType_JointEnd = 11,
        AnimationCommandType_LveBegin = 12,
        AnimationCommandType_LveSample = 13,
        AnimationCommandType_LveLerp = 14,
        AnimationCommandType_LveExportState = 15,
        AnimationCommandType_LveExportDelta = 16,
        AnimationCommandType_LveEnd = 17,
    };

    // size 0x8
    struct SampleJointAnimationCommand
    {
        const JointAnimationData* pData; // 0x00
        float time; // 0x04
    };

    // size 0xC
    struct SampleAndCombineJointAnimationCommand
    {
        const JointAnimationData* pData; // 0x00
        float time; // 0x04
        float weight; // 0x08
    };

    // size 0xC
    struct BeginJointAnimationCommand
    {
        const ModelHierarchy* pHierarchy; // 0x00
        const ModelPose* pDefaultPose; // 0x04
        bool writeHierarchy; // 0x08
    };

    // size 0x4
    struct CombineJointAnimationCommand
    {
        float weight; // 0x00
    };

    // size 0x4
    struct CombineJointAnimationWithMultipleWeightsCommand
    {
        const float* pWeights; // 0x00
    };

    // size 0x4
    struct JointExportJointCommand
    {
        AnimationJoint* pJoints; // 0x00
    };

    // size 0x4
    struct JointExportMatricesCommand
    {
        Matrix43* pMatrices; // 0x00
    };

    // size 0x8
    struct JointExportSkinningMatricesCommand
    {
        Matrix43* pMatrices; // 0x00
        bool withSkinToBone; // 0x04
    };

    // size 0xC
    struct SampleLveAnimationCommand
    {
        const LveAnimationData* pData; // 0x00
        float times[2]; // 0x04
    };

    // size 0x4
    struct LerpLveAnimationCommand
    {
        float weight; // 0x00
    };

    // size 0x4
    struct LveExportLveDeltaCommand
    {
        AnimationLve* pLve; // 0x00
    };

    // size 0x4
    struct LveExportLveStateCommand
    {
        AnimationLve* pLve; // 0x00
    };

    // size 0x10
    struct AnimationCommand
    {
        AnimationCommandType type; // 0x00
        union
        {
            BeginJointAnimationCommand jointBegin; // 0x00
            SampleJointAnimationCommand jointSample; // 0x00
            SampleAndCombineJointAnimationCommand jointSampleAndCombine; // 0x00
            CombineJointAnimationCommand jointCombine; // 0x00
            CombineJointAnimationWithMultipleWeightsCommand jointCombineWeights; // 0x00
            JointExportMatricesCommand jointExportMatrix; // 0x00
            JointExportSkinningMatricesCommand jointExportSkinning; // 0x00
            JointExportJointCommand jointExportJoint; // 0x00
            SampleLveAnimationCommand lveSample; // 0x00
            LerpLveAnimationCommand lveLerp; // 0x00
            LveExportLveStateCommand lveExportState; // 0x00
            LveExportLveDeltaCommand lveExportDelta; // 0x00
        } data; // 0x04
    };
}
