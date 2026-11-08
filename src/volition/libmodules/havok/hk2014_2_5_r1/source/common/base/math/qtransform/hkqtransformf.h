// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/qtransform/hkqtransformf.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/QTransform/hkQTransformf.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_Q_TRANSFORMf_H)
#define HK_MATH_Q_TRANSFORMf_H

class hkQTransformf
{
public:

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkQTransformf);
    HK_DECLARE_REFLECTION();
    HK_FORCE_INLINE hkQTransformf() { }
    HK_FORCE_INLINE hkQTransformf(hkQuaternionfParameter q, hkVector4fParameter t);
    HK_FORCE_INLINE hkQTransformf(const hkQTransformf& qt);
    HK_FORCE_INLINE hkQTransformf(class hkFinishLoadedObjectFlag f);
    HK_FORCE_INLINE void set(hkQuaternionfParameter q, hkVector4fParameter t);
    HK_FORCE_INLINE void set(const hkTransformf& otherTransform);
    HK_FORCE_INLINE void setIdentity();
    HK_FORCE_INLINE void setZero();
    HK_FORCE_INLINE void add(const hkQTransformf& qa);
    HK_FORCE_INLINE static const hkQTransformf& HK_CALL getIdentity();
    HK_FORCE_INLINE const hkVector4f& getTranslation() const;
    HK_FORCE_INLINE void setTranslation(hkVector4fParameter t);
    HK_FORCE_INLINE const hkQuaternionf& getRotation() const;
    HK_FORCE_INLINE void setRotation(hkQuaternionfParameter q);
    void setInverse(const hkQTransformf& qt);
    HK_FORCE_INLINE void _setInverse(const hkQTransformf& qt);
    void setMul(const hkQTransformf& qt1, const hkQTransformf& qt2);
    HK_FORCE_INLINE void _setMul(const hkQTransformf& qt1, const hkQTransformf& qt2);
    void setMul(const hkTransformf& t1, const hkQTransformf& qt2);
    void setMul(const hkTransformf& t1, const hkTransformf& t2);
    void setMul(const hkQTransformf& qt1, const hkTransformf& t2);
    void setMulInverseMul(const hkQTransformf& qt1, const hkQTransformf& qt2);
    void setMulInverseMul(const hkTransformf& t1, const hkQTransformf& qt2);
    void setMulInverseMul(const hkQTransformf& qt1, const hkTransformf& t2);
    void setMulInverseMul(const hkTransformf& t1, const hkTransformf& t2);
    HK_FORCE_INLINE void _setMulInverseMul(const hkQTransformf& qt1, const hkQTransformf& qt2);
    void setMulMulInverse(const hkQTransformf &qt1, const hkQTransformf &qt2);
    HK_FORCE_INLINE void _setMulMulInverse(const hkQTransformf &qt1, const hkQTransformf &qt2);
    bool isApproximatelyEqual( const hkQTransformf& other, hkFloat32 epsilon=hkFloat32(1e-3f) ) const;
    HK_FORCE_INLINE bool _isApproximatelyEqual( const hkQTransformf& other, hkSimdFloat32Parameter epsilon) const;
    void setInterpolate4(const hkQTransformf& qtA, const hkQTransformf& qtB, hkSimdFloat32Parameter t);
    bool isOk() const;

public:

    hkQuaternionf m_rotation;
    hkVector4f m_translation;
};

#endif    // HK_MATH_Q_TRANSFORMf_H
