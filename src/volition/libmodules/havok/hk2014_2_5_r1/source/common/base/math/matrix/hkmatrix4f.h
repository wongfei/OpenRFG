// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix4f.h
//
// Havok 2014.2.5: ported from Havok 2014.1 (Common/Base/Math/Matrix/hkMatrix4f.h), checked against rfg.exe. Only the
// members hkMatrixDecompositionImpl uses; the rest of the class is not reached by the port.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_MATRIX4f_H)
#define HK_MATH_MATRIX4f_H

class hkMatrix4f
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMatrix4f);
        HK_DECLARE_POD_TYPE();

#ifndef HK_DISABLE_MATH_CONSTRUCTORS
        HK_FORCE_INLINE hkMatrix4f() { }
#endif

        HK_FORCE_INLINE void setDiagonal( hkVector4fParameter v ); // 0x00FB7250
        HK_FORCE_INLINE void getDiagonal( hkVector4f& v ) const; // 0x00FB7290
        HK_FORCE_INLINE void set(const hkTransformf& t); // 0x00FB7600
        HK_FORCE_INLINE void get(hkTransformf& t) const; // 0x00FB76E0
        void set4x4ColumnMajor(const hkFloat32* p); // 0x00FC37E0
        HK_FORCE_INLINE hkBool32 isApproximatelyIdentity( hkSimdFloat32Parameter epsilon ) const; // 0x00FB7410
        HK_FORCE_INLINE void setMul( const hkMatrix4f& a, const hkMatrix4f& b ); // 0x00FB7490
        HK_FORCE_INLINE void resetFourthRow(); // 0x00FB75A0

    protected:

        hkVector4f m_col0;
        hkVector4f m_col1;
        hkVector4f m_col2;
        hkVector4f m_col3;
};

#endif //HK_MATH_MATRIX4f_H
