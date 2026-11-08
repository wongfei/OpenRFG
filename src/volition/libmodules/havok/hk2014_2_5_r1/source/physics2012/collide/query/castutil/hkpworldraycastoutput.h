// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastoutput.h
//
// Havok 2014.2.5 (havok_impl.md HK3): a world ray cast hit (adds the root collidable). The PDB declaration with the
// inline code of Havok 2014.1 (hkpWorldRayCastOutput.inl). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"

class hkpCollidable;

// size 0x60
struct hkpWorldRayCastOutput : public hkpShapeRayCastOutput
{
    // 0x0080D820 (hkpworldraycastoutput.inl:17)
    hkpWorldRayCastOutput()
        : m_rootCollidable(nullptr)
    {
    }

    // 0x00B63570 (hkpworldraycastoutput.inl:21)
    hkBool hasHit() const
    {
        return m_rootCollidable != nullptr;
    }

    hkBool operator<(const hkpWorldRayCastOutput& b) const
    {
        return m_hitFraction < b.m_hitFraction;
    }

    // 0x0080D800 (hkpworldraycastoutput.inl:10)
    void reset()
    {
        m_rootCollidable = nullptr;
        hkpShapeRayCastOutput::reset();
    }

    const hkpCollidable* m_rootCollidable; // 0x50
};
