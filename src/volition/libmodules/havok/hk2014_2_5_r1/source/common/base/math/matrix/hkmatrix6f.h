// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix6f.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Matrix/hkMatrix6f.h), checked against rfg.exe.
#ifndef HK_MATH_MATRIX6f_H
#define HK_MATH_MATRIX6f_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

#define FOR_BOTH_SUB_VECTORS(code) m_lin.code; m_ang.code;

class hkMatrix6f;

class hkVector8f
{
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkVector8f);
        hkVector4f m_lin;
        hkVector4f m_ang;
    public:
        HK_FORCE_INLINE void setAll(hkFloat32 x) { FOR_BOTH_SUB_VECTORS( setAll(x) ); }
        HK_FORCE_INLINE void setAll6(hkFloat32 x) { FOR_BOTH_SUB_VECTORS( setXYZ(x) ); }
        HK_FORCE_INLINE void setZero8() { FOR_BOTH_SUB_VECTORS( setZero() ); }
        HK_FORCE_INLINE void add8(const hkVector8f& a)
        {
            m_lin.add( a.m_lin );
            m_ang.add( a.m_ang );
        }
        HK_FORCE_INLINE void add6clobberW(const hkVector8f& a)
        {
            m_lin.addXYZ( a.m_lin );
            m_ang.addXYZ( a.m_ang );
        }
        HK_FORCE_INLINE void setSub8(const hkVector8f& a, const hkVector8f& b)
        {
            m_lin.setSub( a.m_lin, b.m_lin );
            m_ang.setSub( a.m_ang, b.m_ang );
        }
        HK_FORCE_INLINE void mul8(hkSimdFloat32Parameter a) { FOR_BOTH_SUB_VECTORS( mul(a) ); }
        HK_FORCE_INLINE void setNeg8() { m_lin.setNeg<4>(m_lin); m_ang.setNeg<4>(m_ang); }
        HK_FORCE_INLINE void addMul8(hkSimdFloat32Parameter r, const hkVector8f& a)
        {
            m_lin.addMul( r, a.m_lin );
            m_ang.addMul( r, a.m_ang );
        }
        HK_FORCE_INLINE void _setMul6(const hkMatrix6f& a, const hkVector8f& b );

};

class hkMatrix6f
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkMatrix6f);
        HK_FORCE_INLINE hkMatrix6f() { } // 0x010484A0
        HK_FORCE_INLINE void operator= ( const hkMatrix6f& a ) // 0x010485B0
        {
            m_m[0][0] = a.m_m[0][0];
            m_m[0][1] = a.m_m[0][1];
            m_m[1][0] = a.m_m[1][0];
            m_m[1][1] = a.m_m[1][1];
        }

        //    /// Gets read-write access to the specified element.
        //HK_FORCE_INLINE hkFloat32& operator() (int row, int col)
        //{
        //    HK_MATH_ASSERT(0xad67899d, row >= 0 && row < 6 && col >= 0 && col < 6, "Row and col of hkMatrix6f must be in the [0,5] range.");
        //    return m_m[row/3][col/3](row%3,col%3);
        //}

        //    /// Gets read-only access to the specified elements.
        //HK_FORCE_INLINE const hkFloat32& operator() (int row, int col) const
        //{
        //    HK_MATH_ASSERT(0xad67899d, row >= 0 && row < 6 && col >= 0 && col < 6, "Row and col of hkMatrix6f must be in the [0,5] range.");
        //    return m_m[row/3][col/3](row%3,col%3);
        //}

        HK_FORCE_INLINE void setZero() // 0x010484B0
        {
            m_m[0][0].setZero();
            m_m[0][1].setZero();
            m_m[1][0].setZero();
            m_m[1][1].setZero();
        }
        HK_FORCE_INLINE void setIdentity() // 0x01048630
        {
            m_m[0][0].setIdentity();
            m_m[0][1].setZero();
            m_m[1][0].setZero();
            m_m[1][1].setIdentity();
        }
        HK_FORCE_INLINE void setTranspose( const hkMatrix6f& s ) // 0x01059840
        {
            m_m[0][0].setTranspose( s.m_m[0][0] );
            m_m[0][1].setTranspose( s.m_m[1][0] );
            m_m[1][0].setTranspose( s.m_m[0][1] );
            m_m[1][1].setTranspose( s.m_m[1][1] );
        }
        HK_FORCE_INLINE void setMul( const hkMatrix6f& a, const hkMatrix6f& b ) // 0x01059EE0
        {
            hkMatrix3f tmp;
            // optimize ?
            for (int i = 0; i < 2; i++)
            {
                for (int j = 0; j < 2; j++)
                {
                    tmp      ._setMul( a.m_m[i][0], b.m_m[0][j] );
                    m_m[i][j]._setMul( a.m_m[i][1], b.m_m[1][j] );
                    m_m[i][j]._add( tmp );
                }
            }
        }
        HK_FORCE_INLINE void add( const hkMatrix6f& a ) // 0x0105A050
        {
            m_m[0][0]._add( a.m_m[0][0] );
            m_m[0][1]._add( a.m_m[0][1] );
            m_m[1][0]._add( a.m_m[1][0] );
            m_m[1][1]._add( a.m_m[1][1] );
        }
        HK_FORCE_INLINE void sub( const hkMatrix6f& a ) // 0x0105A110
        {
            m_m[0][0]._sub( a.m_m[0][0] );
            m_m[0][1]._sub( a.m_m[0][1] );
            m_m[1][0]._sub( a.m_m[1][0] );
            m_m[1][1]._sub( a.m_m[1][1] );
        }
        HK_FORCE_INLINE void mul( const hkMatrix6f& a)
        {
            hkMatrix6f temp;
            temp.setMul( *this, a );
            *this = temp;
        }
        HK_FORCE_INLINE void mul( hkSimdFloat32Parameter scale )
        {
            m_m[0][0]._setMul(scale, m_m[0][0]);
            m_m[0][1]._setMul(scale, m_m[0][1]);
            m_m[1][0]._setMul(scale, m_m[1][0]);
            m_m[1][1]._setMul(scale, m_m[1][1]);
        }
        void setInvert( const hkMatrix6f& in ); // 0x01059C80
    public:

        HK_ALIGN_FLOAT( hkMatrix3f m_m[2][2] );    // [row][column]

};

void hkVector8f::_setMul6(const hkMatrix6f& a, const hkVector8f& b )
{
    hkVector4f tmp[2];
    tmp[0] ._setRotatedDir( a.m_m[0][0], b.m_lin );
    m_lin._setRotatedDir( a.m_m[0][1], b.m_ang );
    tmp[1] ._setRotatedDir( a.m_m[1][0], b.m_lin );
    m_ang._setRotatedDir( a.m_m[1][1], b.m_ang );
    m_lin.add(tmp[0]);
    m_ang.add(tmp[1]);
}

extern "C"
{
    void HK_CALL hkMatrix6fAdd(hkMatrix6f& aOut, const hkMatrix6f& b); // 0x010598C0
    void HK_CALL hkMatrix6fSub(hkMatrix6f& aOut, const hkMatrix6f& b); // 0x01059980
    void HK_CALL hkMatrix6fSetMul(hkMatrix6f& out, const hkMatrix6f&a, const hkMatrix6f&b ); // 0x01059A40
    void HK_CALL hkMatrix6fSetMulV(hkVector8f& out, const hkMatrix6f&a, const hkVector8f& b); // 0x01059BB0
    void HK_CALL hkMatrix6fSetTranspose(hkMatrix6f& out, const hkMatrix6f&in ); // 0x01059880
    void HK_CALL hkMatrix6fSetInvert(hkMatrix6f& out, const hkMatrix6f& in ); // 0x01059EA0
}

#endif // HK_MATH_MATRIX6f_H
