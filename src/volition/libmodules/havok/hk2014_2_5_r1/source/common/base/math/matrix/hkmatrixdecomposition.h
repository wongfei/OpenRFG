// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrixdecomposition.h
//
// Havok 2014.2.5: ported from Havok 2014.1 (Common/Base/Math/Matrix/hkMatrixDecomposition.h), checked against rfg.exe.
// The Matrix3 overload of decomposeMatrix and polarDecompose are not reached by the port.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATRIX_DECOMPOSITION_UTIL_H)
#define HK_MATRIX_DECOMPOSITION_UTIL_H

template <typename FT>
struct hkMatrixDecompositionImpl
{
    // size 0xF0
    struct Decomposition
    {
        typename hkRealTypes<FT>::Vector m_translation;     // +0x00
        typename hkRealTypes<FT>::Quaternion m_rotation;    // +0x10
        typename hkRealTypes<FT>::Vector m_scale;           // +0x20

        typename hkRealTypes<FT>::Matrix4 m_scaleAndSkew;   // +0x30: M = T*R * scaleAndSkew
        typename hkRealTypes<FT>::Matrix4 m_skew;           // +0x70: M = T*R*S * skew

        typename hkRealTypes<FT>::Rotation m_basis;         // +0xB0: may flip; if m_flips is false this is m_rotation

        hkBool m_hasScale;                                  // +0xE0: S != (1,1,1)
        hkBool m_hasSkew;                                   // +0xE1: T*R*S != M
        hkBool m_flips;                                     // +0xE2: det(M) < 0
    };

    static void HK_CALL decomposeMatrix(const typename hkRealTypes<FT>::Matrix4& matrixIn, Decomposition& decomposition);
    static void HK_CALL decompose4x4ColTransform(const typename hkRealTypes<FT>::Pod* matrixIn, Decomposition& decomposition);
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrixdecomposition.inl"

typedef hkMatrixDecompositionImpl<hkFloat32> hkMatrixfDecomposition;
typedef hkMatrixfDecomposition hkMatrixDecomposition;

#endif // HK_MATRIX_DECOMPOSITION_UTIL_H
