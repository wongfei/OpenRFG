// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaybundlecastinput.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1
// (hkpShapeRayBundleCastInput.inl): four rays, their end points stored transposed. Hand-written (not in the specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h"

class hkpRayShapeCollectionFilter;

// size 0x70
struct alignas(16) hkpShapeRayBundleCastInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpShapeRayBundleCastInput);

    hkpShapeRayBundleCastInput()
        : m_filterInfo(0)
        , m_userData(0)
        , m_rayShapeCollectionFilter(nullptr)
    {
    }

    hkFourTransposedPointsf m_from; // 0x00
    hkFourTransposedPointsf m_to; // 0x30
    unsigned int m_filterInfo; // 0x60
    unsigned long m_userData; // 0x64
    const hkpRayShapeCollectionFilter* m_rayShapeCollectionFilter; // 0x68
};
