// Original: library/modules/runtime/animation/include/keen/animation/jointanimation.hpp
// Implementation: library/modules/runtime/animation/sources/global/jointanimation.cpp
//
// JointAnimation, a skeletal animation resource (compressed joint tracks), and the sampling functions. Placement of
// the handle and initialization types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    struct AnimationJoint;
    struct AxisAlignedBox;
    struct JointAnimationData;
    struct JointAnimationInitializationData;
    struct ModelPose;

    // size 0xC
    class JointAnimation : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x4D4E414A,
        };

        enum
        {
            CurrentFileVersion = 7,
        };

        const JointAnimationData& getData() const;
    private:
        JointAnimation(const JointAnimation&);
    protected:
        JointAnimation(uint32 key); // 0x00CBE5E0 (:14)
        bool initialize(const JointAnimationInitializationData* pInitData); // 0x00CBE600 (:19)
        void destroy(); // 0x00CBE610 (:26)
    private:
        JointAnimation& operator=(const JointAnimation&);
    public:
        ~JointAnimation(); // 0x00CBE990

    private:
        const JointAnimationData* m_pData; // 0x08
    };

    struct JointAnimationHandleType; // no definition in PDB

    // size 0x4
    struct JointAnimationInitializationData
    {
        const JointAnimationData* pAnimationData; // 0x00
    };

    namespace animation
    {
        void sampleJointAnimation(AnimationJoint* pTargetJoints, uint32 jointCapacity, const JointAnimationData& animData, float inputTime); // 0x00CC3690 (:33)
        void sampleJointAnimationWithDefaultPose(AnimationJoint* pTargetJoints, uint32 jointCapacity, const JointAnimationData& animData, const ModelPose& defaultPose, float inputTime); // 0x00CC49D0 (:265)
        void calculateMovementBox(AxisAlignedBox* pTarget, const JointAnimationData& animData); // 0x00CC6220 (:558)
    }
}
