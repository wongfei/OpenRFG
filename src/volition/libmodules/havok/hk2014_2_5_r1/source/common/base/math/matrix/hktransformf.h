// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Matrix/hkTransformf.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_TRANSFORMf_H)
#define HK_MATH_TRANSFORMf_H

class hkTransformf
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkTransformf);
        HK_DECLARE_POD_TYPE();

#ifndef HK_DISABLE_MATH_CONSTRUCTORS
        HK_FORCE_INLINE hkTransformf() { } // 0x0071A6F0 (.h:45)
        HK_FORCE_INLINE hkTransformf(const hkRotationf& r, hkVector4fParameter t);
        HK_FORCE_INLINE hkTransformf(const hkTransformf& r); // 0x00747700 (hktransformf.inl:58)
        HK_FORCE_INLINE hkTransformf(hkQuaternionfParameter q, hkVector4fParameter t); // 0x0080CA60 (hktransformf.inl:71)
#endif

        HK_FORCE_INLINE hkFloat32& operator() (int row, int column); // 0x00FB72E0
        HK_FORCE_INLINE const hkFloat32& operator() (int row, int column) const; // 0x00FB7300
        template <int ROW, int COL>
        HK_FORCE_INLINE const hkSimdFloat32 getElement() const;
        template <int ROW, int COL>
        HK_FORCE_INLINE void setElement(hkSimdFloat32Parameter s);
        HK_FORCE_INLINE void set(hkQuaternionfParameter q, hkVector4fParameter t); // 0x00B62D40 (hktransformf.inl:106)
        HK_FORCE_INLINE void set(const hkRotationf& r, hkVector4fParameter t); // 0x00CF8980
        HK_FORCE_INLINE void setIdentity(); // 0x008140F0 (hktransformf.inl:125)
        HK_FORCE_INLINE static const hkTransformf& HK_CALL getIdentity(); // 0x00742290 (hktransformf.inl:118)
        bool isApproximatelyEqual( const hkTransformf& t, hkFloat32 epsilon=hkFloat32(1e-3f) ) const; // 0x00CF8280
        hkBool32 isApproximatelyEqualSimd( const hkTransformf& t, hkSimdFloat32Parameter epsilon ) const; // 0x00CF82F0
        HK_FORCE_INLINE hkVector4f& getTranslation(); // 0x0080CA80 (hktransformf.inl:131)
        HK_FORCE_INLINE const hkVector4f& getTranslation() const; // 0x00710850 (hktransformf.inl:136)
        HK_FORCE_INLINE void setTranslation(hkVector4fParameter t); // 0x00742280 (hktransformf.inl:32)
        HK_FORCE_INLINE hkRotationf& getRotation(); // 0x0080CA50 (hktransformf.inl:37)
        HK_FORCE_INLINE const hkRotationf& getRotation() const; // 0x00710840 (hktransformf.inl:42)
        HK_FORCE_INLINE void setRotation(const hkRotationf& rotation); // 0x00821D40 (hktransformf.inl:47)
        HK_FORCE_INLINE void setRotation(hkQuaternionfParameter quatRotation); // 0x00CFC9E0
        void setInverse( const hkTransformf &t ); // 0x00CF7F00
        HK_FORCE_INLINE void _setInverse( const hkTransformf &t ); // 0x00CF89B0
        void setMul( const hkTransformf &t1, const hkTransformf &t2 ); // 0x00CF8550
        void setMul( const hkQsTransformf &t1, const hkTransformf &t2 ); // 0x00CF7FD0
        void setMulInverseMul( const hkTransformf& bTa, const hkTransformf &bTc ); // 0x00CF8100
        HK_FORCE_INLINE void _setMulInverseMul( const hkTransformf& bTa, const hkTransformf &bTc ); // 0x01043590
        void setMulEq( const hkTransformf& b ); // 0x00CF8610
        void get4x4ColumnMajor(hkFloat32* HK_RESTRICT p) const; // 0x00CF8360
        void get4x4ColumnMajor(hkDouble64* HK_RESTRICT p) const; // 0x00CF83C0
        void set4x4ColumnMajor(const hkFloat32* p); // 0x00CF8450
        void set4x4ColumnMajor(const hkDouble64* p); // 0x00CF84B0
        HK_FORCE_INLINE void setRows4( hkVector4fParameter r0, hkVector4fParameter r1, hkVector4fParameter r2, hkVector4fParameter r3); // 0x00E6C790
        void setMulMulInverse( const hkTransformf &a, const hkTransformf &b ); // 0x00CF86A0
        bool isOk() const; // 0x00CF7ED0
        HK_FORCE_INLINE hkVector4f& getColumn(int i); // 0x0080CA30 (hktransformf.inl:10)
        HK_FORCE_INLINE const hkVector4f& getColumn(int i) const; // 0x0080CA40 (hktransformf.inl:15)
        template <int I>
        HK_FORCE_INLINE const hkVector4f& getColumn() const;
        template <int I>
        HK_FORCE_INLINE void setColumn(hkVector4fParameter v);
    public: // protected in Havok; public as the PDB-generated headers had it (Volition and the tests read the members)

        hkRotationf m_rotation;
        hkVector4f m_translation;
};

#endif //HK_MATH_TRANSFORMf_H
