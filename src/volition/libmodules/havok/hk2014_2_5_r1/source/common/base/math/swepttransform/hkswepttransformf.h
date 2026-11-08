// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformf.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/SweptTransform/hkSweptTransformf.h), checked against rfg.exe.
#ifndef HK_MATH_SWEPTTRANSFORMf_H
#define HK_MATH_SWEPTTRANSFORMf_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

extern const class hkClass hkSweptTransformClass;


class hkSweptTransformf
{
    public:

        HK_DECLARE_REFLECTION(); // staticClass 0x00CE9FE0
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkSweptTransformf);
        HK_FORCE_INLINE hkSweptTransformf() { } // 0x00DC6BC0
    public:
        HK_FORCE_INLINE void _approxTransformAt( hkTime time, hkTransformf& transformOut ) const; // 0x00FBED80
        HK_FORCE_INLINE void _approxTransformAt( hkSimdFloat32Parameter time, hkTransformf& transformOut ) const; // 0x00FB97A0
        void approxTransformAt( hkTime time, hkTransformf& transformOut ) const; // 0x00FBECA0
        void approxTransformAt( hkSimdFloat32Parameter time, hkTransformf& transformOut ) const; // 0x00FBEBC0
        HK_FORCE_INLINE hkFloat32 getInterpolationValue( hkTime t ) const;
        HK_FORCE_INLINE hkSimdFloat32 getInterpolationValue( hkSimdFloat32Parameter t ) const; // 0x00CFCA20

            // get an interpolation value from a given time= t+deltaTimeAddon. This has a higher precision than getInterpolationValue
        HK_FORCE_INLINE hkFloat32 getInterpolationValueHiAccuracy( hkTime t, hkFloat32 deltaTimeAddon ) const;
        HK_FORCE_INLINE hkSimdFloat32 getInterpolationValueHiAccuracy( hkSimdFloat32Parameter t, hkSimdFloat32Parameter deltaTimeAddon ) const; // 0x00CFCA50
        HK_FORCE_INLINE hkTime getBaseTime() const;
        HK_FORCE_INLINE hkSimdFloat32 getBaseTimeSr() const; // 0x00CFCA00
        HK_FORCE_INLINE hkFloat32 getInvDeltaTime() const;
        HK_FORCE_INLINE hkSimdFloat32 getInvDeltaTimeSr() const; // 0x00B711D0 (hkswepttransformf.inl:26)
        void initSweptTransform( hkVector4fParameter position, hkQuaternionfParameter rotation ); // 0x00FBEB70
    public:
        hkVector4f m_centerOfMass0;
        hkVector4f m_centerOfMass1;
        hkQuaternionf m_rotation0;
        hkQuaternionf m_rotation1;
        hkVector4f m_centerOfMassLocal;
};
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformf.inl"

#endif //HK_MATH_SWEPTTRANSFORMf_H
