// Original: library/modules/runtime/animation/include/keen/animation/animation.hpp
// Implementation: library/modules/runtime/animation/sources/global/animation.cpp
//
// Animation, the base of the animation resources (joint, LVE and morph animations), its ResourceHandle tag and
// initialization data. Placement of the handle and initialization types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    struct AnimationInitializationData;
    class JointAnimation;
    struct JointAnimationData;
    struct JointAnimationHandleType;
    class LveAnimation;
    struct LveAnimationData;
    struct LveAnimationHandleType;
    class MorphAnimation;
    struct MorphAnimationData;
    struct MorphAnimationHandleType;
    struct SequenceData;
    class SequenceResource;
    struct SequenceResourceHandleType;

    // size 0x1C
    class Animation : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x4D4E4150,
        };

        enum
        {
            CurrentFileVersion = 3,
        };

        const JointAnimationHandleType* getJointAnimationHandle() const;
        const LveAnimationHandleType* getLveAnimationHandle() const;
        const SequenceResourceHandleType* getSequenceResourceHandle() const;
        const MorphAnimationHandleType* getMorphAnimationHandle() const;
        const JointAnimation* getJointAnimation() const;
        const LveAnimation* getLveAnimation() const;
        const SequenceResource* getSequenceResource() const;
        const MorphAnimation* getMorphAnimation() const;
        const JointAnimationData* getJointData() const;
        const LveAnimationData* getLveData() const;
        const SequenceData* getSequenceData() const;
        const MorphAnimationData* getMorphAnimationData() const;
        float getLength() const;
    private:
        Animation(const Animation&);
    protected:
        Animation(uint32 key); // 0x00CBB4A0 (:8)
        bool initialize(const AnimationInitializationData* pInitData, float length); // 0x00CBB4D0 (:18)
        void destroy(); // 0x00CBB500 (:30)
    private:
        Animation& operator=(const Animation&);
    public:
        ~Animation(); // 0x00CBE970

    private:
        const JointAnimationHandleType* m_jointAnimationHandle; // 0x08
        const LveAnimationHandleType* m_lveAnimationHandle; // 0x0C
        const SequenceResourceHandleType* m_sequenceResourceHandle; // 0x10
        const MorphAnimationHandleType* m_morphAnimationHandle; // 0x14
        float m_length; // 0x18
    };

    struct AnimationHandleType; // no definition in PDB

    // size 0x14
    struct AnimationInitializationData
    {
        const JointAnimationHandleType* jointAnimationHandle; // 0x00
        const LveAnimationHandleType* lveAnimationHandle; // 0x04
        const SequenceResourceHandleType* sequenceHandle; // 0x08
        const MorphAnimationHandleType* morphAnimationHandle; // 0x0C
        float length; // 0x10
    };
}
