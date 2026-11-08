// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdraybundle.h
//
// Havok 2014.2.5 (havok_impl.md HK3): four rays, their end points stored transposed. Ported from Havok 2014.1
// (hkcdRayBundle.h); the PDB layout is the same. Hand-written (not in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h"

// size 0x70
struct hkcdRayBundle
{
    HK_FORCE_INLINE void getDirection(hkFourTransposedPointsf& directionOut) const
    {
        directionOut.setSub(m_end, m_start);
    }

    hkFourTransposedPointsf m_start; // 0x00
    hkFourTransposedPointsf m_end; // 0x30
    hkVector4fComparison m_activeRays; // 0x60
};
