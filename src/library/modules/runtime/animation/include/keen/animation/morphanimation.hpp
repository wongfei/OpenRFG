// Original: library/modules/runtime/animation/include/keen/animation/morphanimation.hpp
// Implementation: library/modules/runtime/animation/sources/global/morphanimation.cpp
//
// MorphAnimation, an animation of morph target weights. Placement of the handle and initialization types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    struct MorphAnimationData;
    struct MorphAnimationInitializationData;

    // size 0xC
    class MorphAnimation : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x4D4E414D,
        };

        enum
        {
            CurrentFileVersion = 1,
        };

        const MorphAnimationData& getData() const;
        float getLength() const; // 0x00CBE7F0 (:26)
    private:
        MorphAnimation(const MorphAnimation&);
    protected:
        MorphAnimation(uint32 key); // 0x00CBE7B0 (:9)
        bool initialize(const MorphAnimationInitializationData* pInitData); // 0x00CBE7D0 (:14)
        void destroy(); // 0x00CBE7E0 (:21)
    private:
        MorphAnimation& operator=(const MorphAnimation&);
    public:
        ~MorphAnimation(); // 0x00CBE9B0

    private:
        const MorphAnimationData* m_pData; // 0x08
    };

    struct MorphAnimationHandleType; // no definition in PDB

    // size 0x4
    struct MorphAnimationInitializationData
    {
        const MorphAnimationData* pMorphData; // 0x00
    };

    namespace animation
    {
        void sampleMorphAnimation(float* pResult, uint32 numResults, const MorphAnimationData& animation, float time); // 0x00CC7140 (:31)
    }
}
