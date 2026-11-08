// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Matrix/hkMatrix3f.h), checked against rfg.exe.
#ifndef HK_MATH_MATH_H
// The math classes are declared together and their inline code follows them all (Havok's hkMath.h): including one
// of them includes them all.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#elif !defined(HK_MATH_MATRIX3f_H)
#define HK_MATH_MATRIX3f_H

namespace hkCompileError
{
    template <bool b> struct HK_MATRIX3f_FUNC_NOT_IMPLEMENTED;
    template <> struct HK_MATRIX3f_FUNC_NOT_IMPLEMENTED<true>{ };
}
#define HK_MATRIX3f_NOT_IMPLEMENTED HK_COMPILE_TIME_ASSERT2(N==0, HK_MATRIX3f_FUNC_NOT_IMPLEMENTED)

#if defined(HK_TINY_SPU_ELF)
#define HK_MATRIX3f_INLINE HK_ALWAYS_INLINE
#else
#define HK_MATRIX3f_INLINE HK_FORCE_INLINE
#endif

class hkMatrix3f
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMatrix3f);
        HK_DECLARE_POD_TYPE();

#ifndef HK_DISABLE_MATH_CONSTRUCTORS
        HK_ALWAYS_INLINE hkMatrix3f() {} // 0x00710680 (.h:38)
        HK_FORCE_INLINE hkMatrix3f( const hkMatrix3f& other );
#endif

        HK_FORCE_INLINE hkVector4f& getColumn(int i); // 0x0080C900 (hkmatrix3f.inl:11)
        HK_FORCE_INLINE const hkVector4f& getColumn(int i) const; // 0x0080C910 (hkmatrix3f.inl:16)
        template <int I> HK_MATRIX3f_INLINE const hkVector4f& getColumn() const;
        template <int I> HK_MATRIX3f_INLINE void setColumn(hkVector4fParameter c);
        template <int I> HK_MATRIX3f_INLINE hkVector4f& getColumn();
        HK_FORCE_INLINE void getRow( int row, hkVector4f& r) const; // 0x00DD3820
        template <int I> HK_FORCE_INLINE void getRow(hkVector4f& r) const;
        HK_FORCE_INLINE hkFloat32& operator() (int row, int col); // 0x00CF8840
        HK_FORCE_INLINE const hkFloat32& operator() (int row, int col) const; // 0x00CE8640
        template <int ROW, int COL> HK_FORCE_INLINE const hkSimdFloat32 getElement() const;
        template <int ROW, int COL> HK_FORCE_INLINE void setElement(hkSimdFloat32Parameter s);
        HK_FORCE_INLINE void setRows( hkVector4fParameter r0, hkVector4fParameter r1, hkVector4fParameter r2); // 0x00F8F650
        HK_FORCE_INLINE void getRows( hkVector4f& r0, hkVector4f& r1, hkVector4f& r2) const; // 0x00CFB5E0
        HK_FORCE_INLINE void setCols( hkVector4fParameter c0, hkVector4fParameter c1, hkVector4fParameter c2); // 0x0080C920 (hkmatrix3f.inl:129)
        HK_FORCE_INLINE void getCols( hkVector4f& c0, hkVector4f& c1, hkVector4f& c2) const; // 0x0080C950 (hkmatrix3f.inl:143)
        HK_FORCE_INLINE void setZero(); // 0x00B62D30 (hkmatrix3f.inl:383)
        HK_FORCE_INLINE void setIdentity(); // 0x00814090 (hkmatrix3f.inl:390)
        HK_FORCE_INLINE static const hkMatrix3f& HK_CALL getIdentity(); // 0x00EA8DD0
        bool isApproximatelyEqual( const hkMatrix3f& m, hkFloat32 epsilon=hkFloat32(1e-3f) ) const; // 0x00CF9B20
        bool isApproximatelyEqualSimd( const hkMatrix3f& m, hkSimdFloat32Parameter eps) const; // 0x00CF9BA0
        bool isSymmetric( hkFloat32 epsilon=hkFloat32(1e-3f) ) const; // 0x00CF9C00
        void setCrossSkewSymmetric( hkVector4fParameter r ); // 0x00CF93B0
        hkResult invert(hkFloat32 epsilon);
        HK_FORCE_INLINE void setInverseSymmetric(const hkMatrix3f& src);
        hkResult setInverse( const hkMatrix3f& src ); // 0x00CF9CB0
        HK_FORCE_INLINE void solveSymmetric(hkVector4fParameter b, hkVector4f& v);
        void invertSymmetric();
        hkSimdFloat32 frobeniusNormSqr() const ; // 0x00CF9FC0
        HK_FORCE_INLINE void _invertSymmetric();

        //    ///
        //void setRotatedDiagonalMatrix( const hkRotationf &r, const hkVector4f &diagonalMatrix3 );
        void transpose(); // 0x00CF9240
        void setTranspose( const hkMatrix3f& s ); // 0x00CF9700
        HK_FORCE_INLINE void _setTranspose( const hkMatrix3f& s ); // 0x00CF88F0
        void setMul( const hkMatrix3f& a, const hkMatrix3f& b ); // 0x00CFA3A0
        HK_FORCE_INLINE void setMul(hkQuaternionfParameter qa, const hkMatrix3f& b); // 0x00CF9790
        HK_FORCE_INLINE void _setMul( const hkMatrix3f& a, const hkMatrix3f& b ); // 0x00D6DE40
        void setMulInverse( const hkMatrix3f& a, const hkRotationf& b ); // 0x00CFA450
        void setMulInverseMul( const hkRotationf& a, const hkMatrix3f& b ); // 0x00CF98E0
        void setMul( hkSimdFloat32Parameter scale, const hkMatrix3f& a ); // 0x00CF9410
        HK_FORCE_INLINE void setDiagonalMul(hkVector4fParameter vDiag, const hkMatrix3f& a);
        HK_FORCE_INLINE void _setMul( hkSimdFloat32Parameter scale, const hkMatrix3f& a );
        HK_FORCE_INLINE void addDiagonal(hkSimdFloat32Parameter diag); // 0x00EAA190
        void setTransposeMul(const hkMatrix3f& a, const hkMatrix3f& b); // 0x00CF99C0
        void addMul( hkSimdFloat32Parameter scale, const hkMatrix3f& a ); // 0x00CF9AE0
        HK_FORCE_INLINE void _addMul( hkSimdFloat32Parameter scale, const hkMatrix3f& a ); // 0x00CFB7C0
        HK_FORCE_INLINE void _setSubDiag(const hkMatrix3f& A, hkVector4fParameter D);
        HK_FORCE_INLINE void _setSubDiag(const hkMatrix3f& A, hkSimdFloat32Parameter D);
        void changeBasis(const hkRotationf& t); // 0x00CFAD50
        void add( const hkMatrix3f& a ); // 0x00CF9440
        HK_FORCE_INLINE void _add( const hkMatrix3f& a ); // 0x01059EB0
        void sub( const hkMatrix3f& a ); // 0x00CF9470
        HK_FORCE_INLINE void _sub( const hkMatrix3f& a ); // 0x0104ADE0
        void mul( const hkMatrix3f& a); // 0x00CFA5A0
        void mul( hkSimdFloat32Parameter scale ); // 0x00CF9380
        HK_FORCE_INLINE void _mul( hkSimdFloat32Parameter scale ); // 0x00EA8DA0
        HK_FORCE_INLINE void operator= ( const hkMatrix3f& a ); // 0x00814070 (hkmatrix3f.inl:136)
        bool isOk() const; // 0x00CF9340
        HK_FORCE_INLINE void multiplyVector (hkVector4fParameter vectorIn, hkVector4f& resultOut) const; // 0x00EAA1D0
        const hkSimdFloat32 getDeterminant() const; // 0x00CF9E90
        hkResult diagonalizeSymmetric( hkRotationf& eigenVec, hkVector4f& eigenVal, int maxIter=20, hkFloat32 epsilon=HK_FLOAT_EPSILON ) const; // 0x00CFA5E0
        hkResult diagonalizeSymmetricWarmStart( hkMatrix3f& eigenVec, hkVector4f& eigenVal, int maxIter=20, hkFloat32 epsilon=HK_FLOAT_EPSILON ) const; // 0x00CFAD80
        void diagonalizeSymmetricApproximation( hkRotationf& eigenVec, hkVector4f& eigenVal, int maxIter=10 ) const; // 0x00CFA9E0
    public: // protected in Havok; public as the PDB-generated headers had it (Volition and the tests read the members)

        hkVector4f m_col0;
        hkVector4f m_col1;
        hkVector4f m_col2;
};

extern "C"
{
    // implementation of _setMul as out of line function with C-binding
    void hkMatrix3f_setMulMat3Mat3( hkMatrix3f* thisMatrix, const hkMatrix3f& aTb, const hkMatrix3f& bTc ); // 0x00CFA150
    void hkMatrix3f_invertSymmetric( hkMatrix3f& thisMatrix ); // 0x00CFA200
}

#endif //HK_MATH_MATRIX3f_H
