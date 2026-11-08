// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastcollectoroutput.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1
// (hkpShapeRayCastCollectorOutput.inl). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

// size 0x20
struct hkpShapeRayCastCollectorOutput
{
    // 0x007174A0 (hkpshaperaycastcollectoroutput.inl:16)
    hkpShapeRayCastCollectorOutput()
    {
        reset();
    }

    // 0x007174C0 (hkpshaperaycastcollectoroutput.inl:21)
    hkBool hasHit() const
    {
        return m_hitFraction < 1.0f;
    }

    // 0x00717480 (hkpshaperaycastcollectoroutput.inl:10)
    void reset()
    {
        m_hitFraction = 1.0f;
        m_extraInfo = -1;
    }

    hkVector4f m_normal; // 0x00
    float m_hitFraction; // 0x10
    int m_extraInfo; // 0x14
    int m_pad[2]; // 0x18
};
