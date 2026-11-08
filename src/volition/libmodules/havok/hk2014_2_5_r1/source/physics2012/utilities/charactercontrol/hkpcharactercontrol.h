// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/hkpcharactercontrol.h
//
// Havok 2014.2.5 (player_minimal.md P1): hkpSurfaceInfo, the result of hkpCharacterProxy::checkSupport. Layout from
// rfg.pdb (size 0x40), inline members of Havok 2014.1 (hkpCharacterControl.h). Hand-written: the PDB has no
// functions of this header (the constructor is inlined).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x40
struct alignas(16) hkpSurfaceInfo
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSurfaceInfo);

    enum SupportedState
    {
        UNSUPPORTED = 0,
        SLIDING = 1,
        SUPPORTED = 2,
    };

    hkpSurfaceInfo()
    {
        m_supportedState = SUPPORTED;
        m_surfaceNormal.setConstant<HK_QUADREAL_0010>();
        m_surfaceVelocity.setZero();
        m_surfaceDistanceExcess = 0.0f;
        m_surfaceIsDynamic.m_bool = 0;
    }

    SupportedState m_supportedState; // 0x00
    hkVector4f m_surfaceNormal; // 0x10
    hkVector4f m_surfaceVelocity; // 0x20
    float m_surfaceDistanceExcess; // 0x30
    hkBool m_surfaceIsDynamic; // 0x34
};
