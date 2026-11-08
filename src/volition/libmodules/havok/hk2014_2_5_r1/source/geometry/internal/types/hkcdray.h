// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declarations with the inline code of Havok 2014.1 (hkcdRay.h/.inl).
// Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// Ray cast accuracy of the reciprocals and divisions (single precision builds).
#define HK_ACC_RAYCAST HK_ACC_MID

// size 0x1
struct hkcdRayQueryFlags
{
    enum Enum
    {
        NO_FLAGS = 0,
        DISABLE_BACK_FACING_TRIANGLE_HITS = 1,
        DISABLE_FRONT_FACING_TRIANGLE_HITS = 2,
        ENABLE_INSIDE_HITS = 4,
        ENABLE_EXTRA_TRIANGLE_BEVELPLANES = 8,
    };

    // 0x00D7BB30: all lanes set when the flag is set
    static void isFlagSet(hkFlags<enum Enum, unsigned int> flags, Enum flag, hkVector4fComparison& out)
    {
        out.set(hkVector4ComparisonMask::Mask(((0 - (flags.get() & flag)) >> 4) & hkVector4ComparisonMask::MASK_XYZW));
    }
};

// size 0x4
struct hkcdRayCastResult
{
    enum Enum
    {
        NO_HIT = 0,
        BACK_FACE_HIT = 1,
        FRONT_FACE_HIT = 2,
        INSIDE_HIT = 4,
    };

    explicit hkcdRayCastResult(int i) { m_value = (unsigned int)i; } // 0x00D7B900
    hkcdRayCastResult(Enum e) { m_value = (unsigned int)e; } // 0x00D7B8F0
    hkcdRayCastResult() {}
    unsigned int isHit() const { return m_value; } // 0x00D7B910
    unsigned int isInsideHit() const { return m_value & INSIDE_HIT; }
    unsigned int isOutsideHit() const { return INSIDE_HIT ^ (m_value & INSIDE_HIT); }
    unsigned int isFrontSideHit() const { return m_value & FRONT_FACE_HIT; }
    unsigned int isBackSideHit() const { return m_value & BACK_FACE_HIT; }
    unsigned int operator==(const Enum other) const { return m_value == (unsigned int)other; }
    unsigned int operator==(const hkcdRayCastResult other) const { return m_value == other.m_value; }
    unsigned int operator!=(const Enum other) const { return m_value != (unsigned int)other; }
    unsigned int operator!=(const hkcdRayCastResult other) const { return m_value != other.m_value; }

    unsigned int m_value; // 0x00
};

// size 0x30
// A ray: origin, direction with the fraction in w, and the reciprocal direction with the direction's sign mask in w.
struct hkcdRay
{
    void setEndPoints(const hkVector4f& start, const hkVector4f& end, const hkSimdFloat32& fraction = hkSimdFloat32::getConstant<HK_QUADREAL_1>()); // 0x00D7C0B0
    void setOriginDirection(const hkVector4f& origin, const hkVector4f& direction, const hkSimdFloat32& fraction = hkSimdFloat32::getConstant<HK_QUADREAL_1>()); // 0x00E3D620
    void getEndPoint(hkVector4f& vEndPoint) const { vEndPoint.setAddMul(m_origin, m_direction, getFraction()); } // 0x00D82640
    const hkVector4f& getDirection() const { return m_direction; } // 0x00D7B8E0
    const hkVector4f& getInverseDirection() const { return m_invDirection; }
    hkSimdFloat32 getFraction() const { return m_direction.getComponent<3>(); } // 0x00D82620
    void setFraction(const hkSimdFloat32& fraction) { m_direction.setComponent<3>(fraction); } // 0x00E3AA60

    hkVector4f m_origin; // 0x00
    hkVector4f m_direction; // 0x10
    hkVector4f m_invDirection; // 0x20
};

HK_FORCE_INLINE void hkcdRay_setDirection(hkVector4fParameter direction, hkSimdFloat32Parameter fraction, hkcdRay* HK_RESTRICT ray)
{
    hkVector4f directionV;
    directionV.setXYZ_W(direction, fraction);
    directionV.zeroIfTrue(directionV.equalZero()); // no negative zeros, so the reciprocals of zeros are positive
    hkVector4f invDirection; invDirection.setReciprocal<HK_ACC_RAYCAST, HK_DIV_SET_MAX>(directionV);
    invDirection.setInt24W(int(directionV.greaterEqualZero().getMask<hkVector4ComparisonMask::MASK_XYZ>()));
    ray->m_direction = directionV;
    ray->m_invDirection = invDirection;
}

inline void hkcdRay::setEndPoints(const hkVector4f& start, const hkVector4f& end, const hkSimdFloat32& fraction)
{
    hkVector4f direction; direction.setSub(end, start);
    hkcdRay_setDirection(direction, fraction, this);
    m_origin = start;
}

inline void hkcdRay::setOriginDirection(const hkVector4f& origin, const hkVector4f& direction, const hkSimdFloat32& fraction)
{
    hkcdRay_setDirection(direction, fraction, this);
    m_origin = origin;
}
