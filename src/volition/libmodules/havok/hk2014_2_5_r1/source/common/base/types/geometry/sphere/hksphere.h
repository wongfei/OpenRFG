// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h
//
// Havok 2014.2.5 (havok_impl.md HK3): a sphere, the radius in w. The PDB declaration with the inline code of Havok
// 2014.1 (hkSphere.inl). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

class hkClass;

// size 0x10
class hkSphere
{
public:
    static const hkClass& staticClass(); // 0x00CEA000
    hkSphere(const hkVector4f& pt, const hkSimdFloat32& radius) { m_pos.setXYZ_W(pt, radius); }
    hkSphere(const hkVector4f& pt, float radius) { m_pos = pt; m_pos(3) = radius; }
    hkSphere() {} // 0x00E6C250
    float getRadius() const { return m_pos(3); } // 0x00710890 (hksphere.inl:25)
    hkSimdFloat32 getRadiusSimdReal() const { return m_pos.getW(); } // 0x00D90870
    void setRadius(const hkSimdFloat32& newRadius) { m_pos.setW(newRadius); } // 0x00FDEA70
    void setRadius(float newRadius) { m_pos(3) = newRadius; }
    const hkVector4f& getPosition() const { return m_pos; } // 0x007108A0 (hksphere.inl:45)
    hkVector4f& getPositionAndRadius() { return m_pos; } // 0x00D751A0
    const hkVector4f& getPositionAndRadius() const { return m_pos; } // 0x00D75190
    void setPosition(const hkVector4f& newPos) { m_pos.setXYZ(newPos); } // 0x00FDEA90 (the radius stays)
    void setPositionAndRadius(const hkVector4f& newPos, const hkSimdFloat32& newRadius) { m_pos.setXYZ_W(newPos, newRadius); } // 0x00D7BD90
    void setPositionAndRadius(const hkVector4f& newPos) { m_pos = newPos; } // 0x00D81D00

public:
    hkVector4f m_pos; // 0x00 (w: the radius)
};
