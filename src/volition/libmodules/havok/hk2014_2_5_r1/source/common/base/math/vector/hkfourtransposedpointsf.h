// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Vector/hkFourTransposedPointsf.h), checked against rfg.exe.
#ifndef HK_MATH_FOUR_TRANSPOSED_VECTORSf_H
#define HK_MATH_FOUR_TRANSPOSED_VECTORSf_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"

class hkcdVertex;

class hkFourTransposedPointsf
{
    public:

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkFourTransposedPointsf );
        HK_DECLARE_REFLECTION(); // staticClass 0x00CEB6A0
        HK_DECLARE_POD_TYPE();
    public:

        hkFourTransposedPointsf() // 0x00D7BA10
        {
            m_vertices[0].setZero();
            m_vertices[1].setZero();
            m_vertices[2].setZero();
        }
        HK_FORCE_INLINE void set(hkVector4fParameter vA, hkVector4fParameter vB, hkVector4fParameter vC, hkVector4fParameter vD); // 0x00D99140
        HK_FORCE_INLINE void extract(hkVector4f& vA, hkVector4f& vB, hkVector4f& vC, hkVector4f& vD) const; // 0x00D74710
        HK_FORCE_INLINE void extractWithW(hkVector4fParameter wABCD, hkVector4f& vA, hkVector4f& vB, hkVector4f& vC, hkVector4f& vD) const; // 0x00D99200
        HK_FORCE_INLINE void extract(int index, hkVector4f& vI) const; // 0x00D99280
        HK_FORCE_INLINE void setAll(hkVector4fParameter v); // 0x00EF5FF0
        HK_FORCE_INLINE void setSub(const hkFourTransposedPointsf& v, hkVector4fParameter a); // 0x00EF6020
        HK_FORCE_INLINE void setSub(hkVector4fParameter a, const hkFourTransposedPointsf& v);
        HK_FORCE_INLINE void setSub(const hkFourTransposedPointsf& v, const hkFourTransposedPointsf& a); // 0x00D7BA30
        HK_FORCE_INLINE void setAdd(const hkFourTransposedPointsf& v, const hkFourTransposedPointsf& a);
        HK_FORCE_INLINE void setMulC(const hkFourTransposedPointsf& v, hkVector4fParameter a);
        HK_FORCE_INLINE void setMulT(const hkFourTransposedPointsf& v, hkVector4fParameter a); // 0x00D7BA60
        HK_FORCE_INLINE void setMul(const hkFourTransposedPointsf& v, const hkFourTransposedPointsf& a);
        HK_FORCE_INLINE void setMul(const hkFourTransposedPointsf& a, hkSimdFloat32Parameter b);
        HK_FORCE_INLINE void setMin(const hkFourTransposedPointsf& v, const hkFourTransposedPointsf& a);
        HK_FORCE_INLINE void setMax(const hkFourTransposedPointsf& v, const hkFourTransposedPointsf& a);
        HK_FORCE_INLINE void horizontalMin( hkVector4f& minOut ) const;
        HK_FORCE_INLINE void horizontalMax( hkVector4f& maxOut ) const;
        HK_FORCE_INLINE void setOuterProduct(hkVector4fParameter b, hkVector4fParameter c);
        HK_FORCE_INLINE void addOuterProduct(hkVector4fParameter b, hkVector4fParameter c);
        HK_FORCE_INLINE void subOuterProduct(hkVector4fParameter b, hkVector4fParameter c);
        HK_FORCE_INLINE void dot3(hkVector4fParameter a, hkVector4f& dotOut) const; // 0x00EF6060
        HK_FORCE_INLINE void dot3(const hkFourTransposedPointsf& a, hkVector4f& dotOut) const; // 0x00D7BA90
        HK_FORCE_INLINE void setAddMul(const hkFourTransposedPointsf& v, hkVector4fParameter dir, hkVector4fParameter distances);

#if defined(HK_REAL_IS_FLOAT)
        HK_FORCE_INLINE void setAabbProjection(const hkFourTransposedPointsf& v, const hkAabb& aabb);
        HK_FORCE_INLINE void computeAabbDistanceSq(const hkAabb& aabb, hkVector4f& distancesOut) const;
#endif // defined(HK_REAL_IS_FLOAT)

        HK_FORCE_INLINE void setSubMul(const hkFourTransposedPointsf& v, hkVector4fParameter dir, hkVector4fParameter distances);
        HK_FORCE_INLINE void dot4xyz1(hkVector4fParameter a, hkVector4f& dotOut) const;
        HK_FORCE_INLINE void add(hkVector4fParameter a);
        HK_FORCE_INLINE void sub(hkVector4fParameter a);
        HK_FORCE_INLINE void mul(hkSimdFloat32Parameter a);
        HK_FORCE_INLINE void sub( const hkFourTransposedPointsf& v );
        HK_FORCE_INLINE void add( const hkFourTransposedPointsf& v );
        HK_FORCE_INLINE void mul( const hkFourTransposedPointsf& v );
        HK_FORCE_INLINE void setAddMulT(const hkFourTransposedPointsf& v, const hkFourTransposedPointsf& u, hkVector4fParameter a); // 0x00EF60A0
        HK_FORCE_INLINE void setSubMulT(const hkFourTransposedPointsf& v, const hkFourTransposedPointsf& u, hkVector4fParameter a);
        HK_FORCE_INLINE void addMulT(const hkFourTransposedPointsf& u, hkVector4fParameter a); // 0x00D7BAC0
        HK_FORCE_INLINE void subMulT(const hkFourTransposedPointsf& u, hkVector4fParameter a);
        HK_FORCE_INLINE void mulT(hkVector4fParameter a); // 0x00D7BB00
        HK_FORCE_INLINE void setCross(hkVector4fParameter n, const hkFourTransposedPointsf& v); // 0x00EF60E0
        HK_FORCE_INLINE void setCross(const hkFourTransposedPointsf& v, hkVector4fParameter n);
        HK_FORCE_INLINE void flipSigns(hkVector4fParameter a); // 0x00EF6140
        HK_FORCE_INLINE void setSelect(hkVector4fComparisonParameter mask, const hkFourTransposedPointsf& trueVecs, const hkFourTransposedPointsf& falseVecs); // 0x00EF6190
        void setTransformedInverseDir(const hkMatrix3f& m, const hkFourTransposedPointsf& v); // 0x00FBB040
        HK_FORCE_INLINE void _setTransformedInverseDir(const hkMatrix3f& m, const hkFourTransposedPointsf& v); // 0x00FBAEE0
        void setTransformedInversePos(const hkTransformf& m, const hkFourTransposedPointsf& v); // 0x00FBB280
        HK_FORCE_INLINE void _setTransformedInversePos(const hkTransformf& m, const hkFourTransposedPointsf& v); // 0x00FBB4D0
        void setRotatedDir(const hkMatrix3f& m, const hkFourTransposedPointsf& v); // 0x00FBB100
        HK_FORCE_INLINE void _setRotatedDir(const hkMatrix3f& m, const hkFourTransposedPointsf& v); // 0x00FBAFA0
        HK_FORCE_INLINE void normalize();
        static void HK_CALL copyVertexData(const hkVector4f* HK_RESTRICT sourceVerts, const int numVerts, hkArray<hkFourTransposedPointsf>& pointsOut); // 0x00FBB2E0
        #    if defined(HK_REAL_IS_FLOAT)
        static void HK_CALL getOriginalVertices( const hkFourTransposedPointsf* verts4in, int numVertices, hkcdVertex* verticesOut ); // 0x00FBB1A0
        #endif

        template <hkVectorPermutation::Permutation perm>
        HK_FORCE_INLINE void setPermutation(const hkFourTransposedPointsf& pts);
        HK_FORCE_INLINE void computeAverage(hkVector4f& avgOut) const;
    public:

        hkVector4f m_vertices[3];
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkfourtransposedpointsf.inl"

#endif    //    HK_MATH_FOUR_TRANSPOSED_VECTORSf_H
