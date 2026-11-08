// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Math/Vector/hkVector4Util.h), checked against rfg.exe.
#ifndef HK_MATH_VECTOR4_UTIL_H
#define HK_MATH_VECTOR4_UTIL_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkintvector.h"

class hkStringBuf;

template <typename FT>
class hkVector4UtilImpl
{
public:

    typedef hkVector4UtilImpl<FT> ThisType;
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ThisType);
    typedef typename hkRealTypes<FT>::VectorParameter VectorParameter;
    typedef typename hkRealTypes<FT>::ScalarParameter ScalarParameter;
    static HK_FORCE_INLINE void HK_CALL setNormalOfTriangle(typename hkRealTypes<FT>::Vector& result, typename hkRealTypes<FT>::VectorParameter a, typename hkRealTypes<FT>::VectorParameter b, typename hkRealTypes<FT>::VectorParameter c);
    static bool HK_CALL    computeAffineTransformFromTriangles(    typename hkRealTypes<FT>::VectorParameter sa, typename hkRealTypes<FT>::VectorParameter sb, typename hkRealTypes<FT>::VectorParameter sc,
                                                                typename hkRealTypes<FT>::VectorParameter da, typename hkRealTypes<FT>::VectorParameter db, typename hkRealTypes<FT>::VectorParameter dc,
                                                                typename hkRealTypes<FT>::Transform& transformOut);
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL atan2(typename hkRealTypes<FT>::ScalarParameter y, typename hkRealTypes<FT>::ScalarParameter x);
    static HK_FORCE_INLINE void HK_CALL atan2(typename hkRealTypes<FT>::VectorParameter y, typename hkRealTypes<FT>::VectorParameter x, typename hkRealTypes<FT>::Vector& result);
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL atan2(typename hkRealTypes<FT>::VectorParameter v);
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL linearAtan2Approximation(typename hkRealTypes<FT>::ScalarParameter y, typename hkRealTypes<FT>::ScalarParameter x);
    static HK_FORCE_INLINE void HK_CALL linearAtan2Approximation(typename hkRealTypes<FT>::VectorParameter y, typename hkRealTypes<FT>::VectorParameter x, typename hkRealTypes<FT>::Vector& result);
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL linearAtan2ApproximationRough(typename hkRealTypes<FT>::ScalarParameter y, typename hkRealTypes<FT>::ScalarParameter x);
    static HK_FORCE_INLINE void HK_CALL linearAtan2ApproximationRough(typename hkRealTypes<FT>::VectorParameter y, typename hkRealTypes<FT>::VectorParameter x, typename hkRealTypes<FT>::Vector& result);
    static HK_FORCE_INLINE void HK_CALL logApproximation(typename hkRealTypes<FT>::VectorParameter v, typename hkRealTypes<FT>::Vector& result);
    static HK_FORCE_INLINE void HK_CALL convertQuaternionToRotation( typename hkRealTypes<FT>::QuaternionParameter qi, typename hkRealTypes<FT>::Rotation& rotationOut );
    static void HK_CALL convertQuaternionToRotationNoInline( typename hkRealTypes<FT>::QuaternionParameter qi, typename hkRealTypes<FT>::Rotation& rotationOut );
    static void HK_CALL convert4QuaternionsToRotations( const typename hkRealTypes<FT>::Quaternion* quats, typename hkRealTypes<FT>::Rotation* r0Out, typename hkRealTypes<FT>::Rotation* r1Out, typename hkRealTypes<FT>::Rotation* r2Out, typename hkRealTypes<FT>::Rotation* r3Out );
    static HK_FORCE_INLINE void HK_CALL convertFromHalf( const hkHalf& a, const hkHalf& b, const hkHalf& c, const hkHalf& d, typename hkRealTypes<FT>::Vector& out );
    static HK_FORCE_INLINE void HK_CALL convertToUint16WithClip( typename hkRealTypes<FT>::VectorParameter in, typename hkRealTypes<FT>::VectorParameter offset, typename hkRealTypes<FT>::VectorParameter scale, typename hkRealTypes<FT>::VectorParameter min, typename hkRealTypes<FT>::VectorParameter max, hkIntUnion64& out);
    static void HK_CALL getFloatToInt16FloorCorrection(typename hkRealTypes<FT>::Pod& out);
    static HK_FORCE_INLINE void HK_CALL convertToUint32( typename hkRealTypes<FT>::VectorParameter in, typename hkRealTypes<FT>::VectorParameter offset, typename hkRealTypes<FT>::VectorParameter scale, hkUint32* out );
    static HK_FORCE_INLINE void HK_CALL convertToUint32WithClip( typename hkRealTypes<FT>::VectorParameter in, typename hkRealTypes<FT>::VectorParameter offset, typename hkRealTypes<FT>::VectorParameter scale, typename hkRealTypes<FT>::VectorParameter min, typename hkRealTypes<FT>::VectorParameter max, hkUint32* out);
    static void HK_CALL getFloatToInt32FloorCorrection(typename hkRealTypes<FT>::Pod& out);
    static HK_FORCE_INLINE void HK_CALL convertComparison(typename hkRealTypes<FT>::ComparisonParameter cin, hkVector4fComparison& cout);
    static HK_FORCE_INLINE void HK_CALL convertVector(typename hkRealTypes<FT>::VectorParameter vin, hkVector4f& vout);
    static HK_FORCE_INLINE void HK_CALL calculatePerpendicularVector(typename hkRealTypes<FT>::VectorParameter vectorIn, typename hkRealTypes<FT>::Vector& biVectorOut);
    template<bool INVERSE2>
    static HK_FORCE_INLINE void HK_CALL calculatePerpendicularNormalizedVectors(typename hkRealTypes<FT>::VectorParameter vectorIn, typename hkRealTypes<FT>::Vector& biVectorOut, typename hkRealTypes<FT>::Vector& bi2VectorOut);
    static HK_FORCE_INLINE void HK_CALL transpose(typename hkRealTypes<FT>::Vector& v0, typename hkRealTypes<FT>::Vector& v1, typename hkRealTypes<FT>::Vector& v2);
    static HK_FORCE_INLINE void HK_CALL transpose(typename hkRealTypes<FT>::Vector& v0, typename hkRealTypes<FT>::Vector& v1, typename hkRealTypes<FT>::Vector& v2, typename hkRealTypes<FT>::Vector& v3);
    static HK_FORCE_INLINE void HK_CALL transformPoints( const typename hkRealTypes<FT>::Transform& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL transformPoints( const typename hkRealTypes<FT>::QTransform& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL transformPoints( const typename hkRealTypes<FT>::QsTransform& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL mul4xyz1Points( const typename hkRealTypes<FT>::Transform& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL transformSpheres( const typename hkRealTypes<FT>::Transform& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL transformSpheres( const typename hkRealTypes<FT>::QsTransform& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL transformPlaneEquation( const typename hkRealTypes<FT>::Transform& t, typename hkRealTypes<FT>::VectorParameter planeIn, typename hkRealTypes<FT>::Vector& planeOut );
    static HK_FORCE_INLINE void HK_CALL transformPlaneEquations( const typename hkRealTypes<FT>::Transform& t, const typename hkRealTypes<FT>::Vector* planesIn, int numPlanes, typename hkRealTypes<FT>::Vector* planesOuts );
    static HK_FORCE_INLINE void HK_CALL transformAndScalePlane( const typename hkRealTypes<FT>::Transform& transform, const typename hkRealTypes<FT>::Vector& scale, typename hkRealTypes<FT>::Vector& planeInOut );
    static HK_FORCE_INLINE void HK_CALL scalePlane(const typename hkRealTypes<FT>::Vector& scale, typename hkRealTypes<FT>::Vector& planeInOut );
    static HK_FORCE_INLINE void HK_CALL translatePlane( const typename hkRealTypes<FT>::Vector& translation, typename hkRealTypes<FT>::Vector& planeInOut );
    static HK_FORCE_INLINE void HK_CALL rotatePoints( const typename hkRealTypes<FT>::Matrix3& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL rotateInversePoints( const typename hkRealTypes<FT>::Rotation& t, const typename hkRealTypes<FT>::Vector* vectorsIn, int numVectors, typename hkRealTypes<FT>::Vector* vectorsOut );
    static HK_FORCE_INLINE void HK_CALL setMul( const typename hkRealTypes<FT>::Matrix3& aTb, const typename hkRealTypes<FT>::Matrix3& bTc, typename hkRealTypes<FT>::Matrix3& aTcOut );
    static HK_FORCE_INLINE void HK_CALL setInverseMul( const typename hkRealTypes<FT>::Rotation& bTa, const typename hkRealTypes<FT>::Matrix3& bTc, typename hkRealTypes<FT>::Matrix3& aTcOut );
    static HK_FORCE_INLINE void HK_CALL _setMulInverseMul( const typename hkRealTypes<FT>::Transform& bTa, const typename hkRealTypes<FT>::Transform &bTc, typename hkRealTypes<FT>::Transform* tOut );
    static HK_FORCE_INLINE hkResult HK_CALL invert2x2Matrix(typename hkRealTypes<FT>::VectorParameter m, typename hkRealTypes<FT>::ScalarParameter tolerance, typename hkRealTypes<FT>::Vector& out);
    static HK_FORCE_INLINE void HK_CALL sinCos(typename hkRealTypes<FT>::VectorParameter r, typename hkRealTypes<FT>::Vector& sines, typename hkRealTypes<FT>::Vector& cosines);
    static HK_FORCE_INLINE void HK_CALL sinCos(typename hkRealTypes<FT>::VectorParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE void HK_CALL sinCos(typename hkRealTypes<FT>::ScalarParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE void HK_CALL sinCos(typename hkRealTypes<FT>::ScalarParameter r, typename hkRealTypes<FT>::Scalar& s, typename hkRealTypes<FT>::Scalar& c);
    static HK_FORCE_INLINE void HK_CALL sinCosApproximation(typename hkRealTypes<FT>::VectorParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE void HK_CALL sinCosApproximation(typename hkRealTypes<FT>::ScalarParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE void HK_CALL sinCosApproximation(typename hkRealTypes<FT>::ScalarParameter r, typename hkRealTypes<FT>::Scalar& s, typename hkRealTypes<FT>::Scalar& c);
    static HK_FORCE_INLINE void HK_CALL aSin(typename hkRealTypes<FT>::VectorParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE void HK_CALL aCos(typename hkRealTypes<FT>::VectorParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL aSin(typename hkRealTypes<FT>::ScalarParameter r);
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL aCos(typename hkRealTypes<FT>::ScalarParameter r);
    static HK_FORCE_INLINE void HK_CALL aSinAcos(typename hkRealTypes<FT>::VectorParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE void HK_CALL aSinAcos(typename hkRealTypes<FT>::ScalarParameter r, typename hkRealTypes<FT>::Vector& sc);
    static HK_FORCE_INLINE void HK_CALL aSinAcos(typename hkRealTypes<FT>::ScalarParameter r, typename hkRealTypes<FT>::Scalar& s, typename hkRealTypes<FT>::Scalar& c);
    static HK_FORCE_INLINE const typename hkRealTypes<FT>::Scalar HK_CALL toRange(typename hkRealTypes<FT>::ScalarParameter v, typename hkRealTypes<FT>::ScalarParameter radius, typename hkRealTypes<FT>::Scalar& k);
    static HK_FORCE_INLINE const typename hkRealTypes<FT>::Scalar HK_CALL toRange(typename hkRealTypes<FT>::ScalarParameter v, typename hkRealTypes<FT>::ScalarParameter radius, typename hkRealTypes<FT>::ScalarParameter diameter, typename hkRealTypes<FT>::Scalar& k);
    static HK_FORCE_INLINE const typename hkRealTypes<FT>::Scalar HK_CALL toRange(typename hkRealTypes<FT>::ScalarParameter v, typename hkRealTypes<FT>::ScalarParameter radius, typename hkRealTypes<FT>::ScalarParameter diameter, typename hkRealTypes<FT>::ScalarParameter diameterReciprocal, typename hkRealTypes<FT>::Scalar& k);
    static HK_FORCE_INLINE void HK_CALL dot3_3vs3( typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter b0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter b1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter b2, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot3_4vs4( typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter b0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter b1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter b2, typename hkRealTypes<FT>::VectorParameter a3, typename hkRealTypes<FT>::VectorParameter b3, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot4_4vs4( typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter b0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter b1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter b2, typename hkRealTypes<FT>::VectorParameter a3, typename hkRealTypes<FT>::VectorParameter b3, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot3_1vs3( typename hkRealTypes<FT>::VectorParameter vectorIn, typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot3_1vs4( typename hkRealTypes<FT>::VectorParameter vectorIn, typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter a3, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot4_1vs4( typename hkRealTypes<FT>::VectorParameter vectorIn, typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter a3, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot4_1vs3( typename hkRealTypes<FT>::VectorParameter vectorIn, typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot4xyz1_1vs4( typename hkRealTypes<FT>::VectorParameter vectorIn, typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter a1, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter a3, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot3_2vs2( typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter b0, typename hkRealTypes<FT>::VectorParameter b1, typename hkRealTypes<FT>::Vector& dotsOut);
    static HK_FORCE_INLINE void HK_CALL dot4xyz1_2vs2( typename hkRealTypes<FT>::VectorParameter a0, typename hkRealTypes<FT>::VectorParameter a2, typename hkRealTypes<FT>::VectorParameter b0, typename hkRealTypes<FT>::VectorParameter b1, typename hkRealTypes<FT>::Vector& dotsOut);
    template<int X>
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL dot3X( VectorParameter a0, VectorParameter a1, VectorParameter b0, VectorParameter b1, VectorParameter c0, VectorParameter c1);
    template<int X>
    static HK_FORCE_INLINE typename hkRealTypes<FT>::Scalar HK_CALL dot2X( VectorParameter a0, VectorParameter a1, VectorParameter b0, VectorParameter b1);
    static HK_FORCE_INLINE void HK_CALL cross_3vs1(    typename hkRealTypes<FT>::VectorParameter a, typename hkRealTypes<FT>::VectorParameter b, typename hkRealTypes<FT>::VectorParameter c, typename hkRealTypes<FT>::VectorParameter n,
                                                    typename hkRealTypes<FT>::Vector& an, typename hkRealTypes<FT>::Vector& bn, typename hkRealTypes<FT>::Vector& cn);
    static HK_FORCE_INLINE void HK_CALL cross_4vs1(    typename hkRealTypes<FT>::VectorParameter a, typename hkRealTypes<FT>::VectorParameter b, typename hkRealTypes<FT>::VectorParameter c, typename hkRealTypes<FT>::VectorParameter d, typename hkRealTypes<FT>::VectorParameter n,
                                                    typename hkRealTypes<FT>::Vector& an, typename hkRealTypes<FT>::Vector& bn, typename hkRealTypes<FT>::Vector& cn, typename hkRealTypes<FT>::Vector& dn);
    static HK_FORCE_INLINE void computeCyclicCrossProducts(    typename hkRealTypes<FT>::VectorParameter vA, typename hkRealTypes<FT>::VectorParameter vB, typename hkRealTypes<FT>::VectorParameter vC,
                                                            typename hkRealTypes<FT>::Vector& vAB, typename hkRealTypes<FT>::Vector& vBC, typename hkRealTypes<FT>::Vector& vCA);
    static HK_FORCE_INLINE void computeQuadEdgePlanes( typename hkRealTypes<FT>::VectorParameter vA, typename hkRealTypes<FT>::VectorParameter vB, typename hkRealTypes<FT>::VectorParameter vC, typename hkRealTypes<FT>::VectorParameter vD, typename hkRealTypes<FT>::Vector* normalOut, typename hkRealTypes<FT>::Transform* edgesOut );
    static HK_FORCE_INLINE void HK_CALL buildOrthonormal( typename hkRealTypes<FT>::VectorParameter dir, typename hkRealTypes<FT>::Matrix3& out );
    static HK_FORCE_INLINE void HK_CALL buildOrthonormal( typename hkRealTypes<FT>::VectorParameter dir, typename hkRealTypes<FT>::VectorParameter dir2, typename hkRealTypes<FT>::Matrix3& out );
    static HK_FORCE_INLINE const typename hkRealTypes<FT>::Scalar HK_CALL distToLineSquared( typename hkRealTypes<FT>::VectorParameter a, typename hkRealTypes<FT>::VectorParameter b, typename hkRealTypes<FT>::VectorParameter p );
    static HK_FORCE_INLINE void HK_CALL setMax44(typename hkRealTypes<FT>::VectorParameter a, typename hkRealTypes<FT>::VectorParameter b, typename hkRealTypes<FT>::VectorParameter c, typename hkRealTypes<FT>::VectorParameter d, typename hkRealTypes<FT>::Vector& out);
    static HK_FORCE_INLINE void HK_CALL setMin44(typename hkRealTypes<FT>::VectorParameter a, typename hkRealTypes<FT>::VectorParameter b, typename hkRealTypes<FT>::VectorParameter c, typename hkRealTypes<FT>::VectorParameter d, typename hkRealTypes<FT>::Vector& out);
    static HK_FORCE_INLINE void HK_CALL setXAtVectorMax( typename hkRealTypes<FT>::VectorParameter check, hkIntVector& iVinOut, typename hkRealTypes<FT>::Vector& v0InOut, typename hkRealTypes<FT>::Vector& v1InOut);
    static HK_FORCE_INLINE void HK_CALL setLength( typename hkRealTypes<FT>::ScalarParameter desiredLength, typename hkRealTypes<FT>::Vector& vectorInOut );
    static HK_FORCE_INLINE void HK_CALL setProjectedLength( typename hkRealTypes<FT>::ScalarParameter desiredLength, typename hkRealTypes<FT>::VectorParameter scaleMeasurePlane, typename hkRealTypes<FT>::ScalarParameter maxScale, typename hkRealTypes<FT>::Vector& vectorInOut );
    struct DistanceToPoint
    {
        typedef typename hkVector4UtilImpl<FT>::DistanceToPoint DistType;
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(DistType);
        HK_FORCE_INLINE DistanceToPoint( typename hkRealTypes<FT>::VectorParameter p ) : m_p(p) { }
        HK_FORCE_INLINE typename hkRealTypes<FT>::Pod operator() (typename hkRealTypes<FT>::VectorParameter p) const { return m_p.distanceToSquared(p).getReal(); }
        typename hkRealTypes<FT>::Vector m_p;
    };

    //
    //    conversions
    //

    // debug output (hkStringBuf): not implemented
    static const char* HK_CALL toString3(typename hkRealTypes<FT>::VectorParameter x, hkStringBuf& s, char separator=',');
    static const char* HK_CALL toString4(typename hkRealTypes<FT>::VectorParameter x, hkStringBuf& s, char separator=',');

    //
    //    compression
    //

    // packs a normalized quaternion into a single 4*8 bit integer. The error is roughly 0.01f per component
    static hkUint32 HK_CALL packQuaternionIntoInt32( typename hkRealTypes<FT>::VectorParameter qin);

    // unpack an 32 bit integer into quaternion. Note: The resulting quaternion is not normalized ( |q.length<4>()-1.0f| < 0.04f )
    static HK_FORCE_INLINE void HK_CALL unPackInt32IntoQuaternion( hkUint32 ivalue, typename hkRealTypes<FT>::Vector& qout );
    template<unsigned int i, unsigned int j, unsigned int k, unsigned int l>
    static HK_FORCE_INLINE void HK_CALL setPermutation2(hkVector4fParameter a, hkVector4fParameter b, hkVector4f & out);
    static HK_FORCE_INLINE void HK_CALL sortVectorIncreasing(typename hkRealTypes<FT>::Vector& v);
    static HK_FORCE_INLINE void HK_CALL sortVectorDecreasing(typename hkRealTypes<FT>::Vector& v);

};

#if HK_CONFIG_SIMD == HK_CONFIG_SIMD_ENABLED
#    if defined(HK_COMPILER_HAS_INTRINSICS_IA32)
#            include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/sse/hkssevector4util.inl"
#    elif defined(HK_PLATFORM_PS3_SPU) || defined(HK_PLATFORM_PS3_PPU)
#        include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/ps3/hkps3vector4util.inl"
#    elif defined(HK_PLATFORM_XBOX360)
#        include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/xbox360/hkxbox360vector4util.inl"
#    endif
#endif // HK_CONFIG_SIMD

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4util.inl"

typedef hkVector4UtilImpl<hkFloat32>  hkVector4fUtil;

#if defined(HK_REAL_IS_DOUBLE)
#else
typedef hkVector4fUtil hkVector4Util;
#endif

#endif // HK_MATH_VECTOR4_UTIL_H
