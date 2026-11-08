// Original: library/modules/runtime/animation/include/keen/animation/lveanimation.hpp
// Implementation: library/modules/runtime/animation/sources/global/lveanimation.cpp
//
// LveAnimation, a root (locomotion) animation: orientation and position keys sampled as state or delta. Placement of
// the handle and initialization types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    struct AnimationLve;
    struct LveAnimationData;
    struct LveAnimationInitializationData;
    struct LveOrientationKey;
    struct LvePositionKey;
    struct Quaternion;
    struct Vector3;

    // size 0xC
    class LveAnimation : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x4D4E414C,
        };

        enum
        {
            CurrentFileVersion = 4,
        };

        const LveAnimationData& getData() const;
        void getStartToEnd(Vector3*) const;
    private:
        LveAnimation(const LveAnimation&);
    protected:
        LveAnimation(uint32 key); // 0x00CBE680 (:13)
        bool initialize(const LveAnimationInitializationData* pInitData); // 0x00CBE6A0 (:18)
        void destroy(); // 0x00CBE6B0 (:25)
    private:
        LveAnimation& operator=(const LveAnimation&);
    public:
        ~LveAnimation(); // 0x00CBE9A0

    private:
        const LveAnimationData* m_pData; // 0x08
    };

    struct LveAnimationHandleType; // no definition in PDB

    // size 0x4
    struct LveAnimationInitializationData
    {
        const LveAnimationData* pLveData; // 0x00
    };

    namespace animation
    {
        void sampleLveStateAnimation(AnimationLve* pTargetLve, const LveAnimationData& animData, float time); // 0x00CC6700 (:30)
        void sampleLveDeltaAnimation(AnimationLve* pTargetLve, const LveAnimationData& animData, float startTime, float endTime); // 0x00CC6A20 (:110)
        void unpackLveOrientationKey(Quaternion* pOrientation, const LveOrientationKey& orientationKey); // 0x00CBE6C0 (:226)
        void unpackLvePositionKey(Vector3* pPosition, const LvePositionKey& positionKey); // 0x00CBE710 (:232)
    }
}
