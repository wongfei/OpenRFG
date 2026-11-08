// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1
// (hkpShapeRayCastInput.inl). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

class hkpCollidable;
class hkpRayShapeCollectionFilter;

// size 0x30
struct alignas(16) hkpShapeRayCastInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpShapeRayCastInput);

    // 0x00743450 (hkpshaperaycastinput.inl:15)
    hkpShapeRayCastInput()
        : m_filterInfo(0)
        , m_rayShapeCollectionFilter(nullptr)
        , m_collidable(nullptr)
        , m_userData(0)
    {
    }

    hkVector4f m_from; // 0x00
    hkVector4f m_to; // 0x10
    unsigned int m_filterInfo; // 0x20
    const hkpRayShapeCollectionFilter* m_rayShapeCollectionFilter; // 0x24
    const hkpCollidable* m_collidable; // 0x28
    unsigned long m_userData; // 0x2C
};
