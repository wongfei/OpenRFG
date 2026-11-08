// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/common/hkpmaterial.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the surface material, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/common/hkpmaterial.h"

// 0x00DDEAC0: the response type is left to the owner (the rigid body sets it from its cinfo)
hkpMaterial::hkpMaterial()
{
    m_friction = 0.5f;
    m_restitution = 0.4f; // 0x3ECCCCCD
    m_rollingFrictionMultiplier.setZero();
}

// 0x00DDEAB0
hkpMaterial::hkpMaterial(hkFinishLoadedObjectFlag flag)
{
}
