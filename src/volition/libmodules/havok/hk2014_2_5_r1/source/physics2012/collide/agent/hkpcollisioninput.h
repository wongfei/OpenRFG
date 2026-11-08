// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h
//
// Havok 2014.2.5: the PDB declaration with the inline members of Havok 2014.1 (hkpCollisionInput.h). Hand-maintained
// since HK5 (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkpCollisionDispatcher;
class hkpCollisionFilter;
class hkpConvexListFilter;

// size 0x50
struct hkpCollisionInput
{
    // size 0x30
    struct Aabb32Info
    {
        hkVector4f m_bitOffsetLow; // 0x00
        hkVector4f m_bitOffsetHigh; // 0x10
        hkVector4f m_bitScale; // 0x20
    };

    // 0x00D90890
    float getTolerance() const
    {
        return m_tolerance;
    }

    // 0x00D9EE70
    void setTolerance(float t)
    {
        m_tolerance = t;
    }

    // 0x00816310 (.h:35): only m_forceAcceptContactPoints is initialized
    hkpCollisionInput()
    {
        m_forceAcceptContactPoints = 0u;
    }

    hkPadSpu<hkpCollisionDispatcher*> m_dispatcher; // 0x00
    hkPadSpu<unsigned int> m_weldClosestPoints; // 0x04
    hkPadSpu<unsigned int> m_forceAcceptContactPoints; // 0x08
    hkPadSpu<float> m_tolerance; // 0x0C
    hkPadSpu<hkpCollisionFilter const*> m_filter; // 0x10
    hkPadSpu<hkpConvexListFilter const*> m_convexListFilter; // 0x14
    hkPadSpu<unsigned int> m_createPredictiveAgents; // 0x18
    Aabb32Info m_aabb32Info; // 0x20
};
