// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/skinnedmodelinstance.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/skinnedmodelinstance.cpp
//
// SkinnedModelInstance, an instance of a skinned model: the current pose (AnimationJoint/AnimationLve), joint matrices
// and the skinned geometry instances.
#pragma once

#include "keen/base/types.hpp"
#include "keen/animation/animationjoint.hpp"
#include "keen/animation/animationlve.hpp"
#include "keen/base/array.hpp"
#include "keen/math/matrix43.hpp"

namespace keen
{
    class MemoryAllocator;
    struct ModelHandleType;
    struct ModelPose;
    struct SkinnedGeometryInstance;
    struct SkinningBatch;

    // size 0x74
    class SkinnedModelInstance
    {
    public:
        SkinnedModelInstance(); // 0x00CC9AA0 (:26)
        void create(MemoryAllocator* pAllocator, const ModelHandleType* model, uint32 messageChannelCount, bool useBoundingBoxData); // 0x00CCC2A0 (:32)
        void destroy(MemoryAllocator* pAllocator); // 0x00CCBCB0 (:85)
        void rebuildMatrices(); // 0x00CC8940 (:211)
        void updateMatrices() const; // 0x00CC9BD0 (:170)
        const SkinnedGeometryInstance* updateSkinning(SkinningBatch* pSkinningBatch) const; // 0x00CCA7E0 (:216)
        void copyPose(const Matrix43* pJointMatrices, uint32 jointCount); // 0x00CC9AD0 (:97)
        void resetJointData(); // 0x00CC9B50 (:107)
        void setDefaultPose(const ModelPose& defaultPose); // 0x00CC9BB0 (:119)
        AnimationLve& getLveData();
        const Array<AnimationJoint>& getJointData() const;
        Array<AnimationJoint>& getJointData();
        int getJointIndexDataByNameCrc(uint32 trackNameCrc) const; // 0x00CC88B0 (:124)
        void setWorldTransform(const Matrix43& transform); // 0x00CC88E0 (:141)
        const Matrix43& getWorldTransform() const;
        const ModelHandleType* getModel() const;
        const Array<Matrix43>& getAttachmentMatrices() const; // 0x00CCBD20 (:146)
        const Array<Matrix43>& getRenderMatrices() const; // 0x00CCBD40 (:155)
        Array<Matrix43>& getRenderMatrices(); // 0x00CC8930 (:164)
        bool hasMorphTargets() const;
        void setMorphWeight(uint32 index, float value); // 0x00CCAA60 (:255)

    private:
        Matrix43 m_worldTranform; // 0x00
        const ModelHandleType* m_model; // 0x30
        AnimationLve m_lve; // 0x34
        Array<AnimationJoint> m_joints; // 0x50
        bool m_matricesDirty; // 0x58
        Array<Matrix43> m_matrices; // 0x5C
        Array<Matrix43> m_attachmentMatrices; // 0x64
        Array<float> m_morphWeights; // 0x6C
    };
}
