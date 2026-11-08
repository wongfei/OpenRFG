// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpointmaterial.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the getters of hkcontactpointmaterial.inl with out-of-line copies. Friction
// and the maximum impulse are hkUFloat8; restitution is a byte in 1/128 steps.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpointmaterial.h"

// 0x00DDCC40
hkContactPointMaterial::hkContactPointMaterial()
{
}

// 0x00E03980 (.inl:10)
float hkContactPointMaterial::getFriction() const
{
    return m_friction;
}

// 0x00E03820
float hkContactPointMaterial::getRestitution() const
{
    return (float)m_restitution * (1.0f / 128.0f);
}

// 0x00E30130 (.inl:54)
float hkContactPointMaterial::getMaxImpulsePerStep()
{
    return m_maxImpulse;
}
