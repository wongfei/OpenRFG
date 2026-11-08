// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/collide/linesegment/hklinesegmentutil.h
//
// Havok 2014.2.5 (havok_impl.md HK2): ported from Havok 2014.1 (Common/Base/Algorithm/Collide/LineSegment/hkLineSegmentUtil.h), checked against rfg.exe.
#ifndef HK_INTERNAL_LINE_SEGMENT_UTIL_H
#define HK_INTERNAL_LINE_SEGMENT_UTIL_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

class hkContactPoint;

class hkLineSegmentUtil
{
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkLineSegmentUtil);
        //
        //    EDGE EDGE
        //

        enum
        {
            CLSLS_POINTA_START = 1,
            CLSLS_POINTA_END   = 2,
            CLSLS_POINTB_START = 4,
            CLSLS_POINTB_END   = 8
        };
        struct ClosestLineSegLineSegResult
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkLineSegmentUtil::ClosestLineSegLineSegResult);
            // implicit constructor: 0x00DBEA60
            hkVector4 m_closestPointA;
            hkVector4 m_closestAminusClosestB;
            hkPadSpu<hkReal>  m_distanceSquared;
            hkSimdReal  m_t;
            hkSimdReal  m_u;
        };
        static int HK_CALL closestLineSegLineSeg( hkVector4Parameter A, hkVector4Parameter dA, hkVector4Parameter B, hkVector4Parameter dB, ClosestLineSegLineSegResult& result ); // 0x00FB7FB0
        static int HK_CALL capsuleCapsuleManifold( const  hkVector4* capsA, hkReal capsARadius, const hkVector4* capsB, hkReal capsBRadius,    hkContactPoint* pointsOut ); // 0x00FB8540

        //
        //    POINT EDGE
        //

        struct ClosestPointLineSegResult
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkLineSegmentUtil::ClosestPointLineSegResult);
            hkVector4 m_pointOnEdge;
        };
        static int HK_CALL closestPointLineSeg( hkVector4Parameter A, hkVector4Parameter B0, hkVector4Parameter B1, ClosestPointLineSegResult& result ); // 0x00FB8200

        //
        // Infinite line / infinite line
        //

        struct ClosestPointInfLineInfLineResult
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ClosestPointInfLineInfLineResult);
            // implicit constructor: 0x00D81D10
            hkPadSpu<hkReal> m_distanceSquared;
            hkPadSpu<hkReal> m_fractionA;
            hkPadSpu<hkReal> m_fractionB;
            hkVector4 m_closestPointA;
            hkVector4 m_closestPointB;
        };
        static hkResult HK_CALL closestPointInfLineInfLine( const hkVector4& A, const hkVector4& dA, const hkVector4& B, const hkVector4& dB, ClosestPointInfLineInfLineResult& result); // 0x00FB82B0

        //
        // Infinite line / plane
        //

        struct IntersectionInfLinePlaneResult
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkLineSegmentUtil::IntersectionInfLinePlaneResult);
            hkPadSpu<hkReal> m_fractionA;
            hkVector4 m_pointOnPlane;
        };
        static hkResult HK_CALL intersectionInfLinePlane( const hkVector4& A, const hkVector4& dA, const hkVector4& planeEquation, IntersectionInfLinePlaneResult& result); // 0x00FB8480

};

#endif // HK_INTERNAL_LINE_SEGMENT_UTIL_H
