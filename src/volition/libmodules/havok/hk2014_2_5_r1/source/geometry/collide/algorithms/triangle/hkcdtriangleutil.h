// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/algorithms/triangle/hkcdtriangleutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkSimdFloat32;
class hkVector4f;

namespace hkcdTriangleUtil
{
    unsigned int isDegenerate(const hkVector4f& a, const hkVector4f& b, const hkVector4f& c, const hkSimdFloat32& tolerance); // 0x00FD0380
    void calcBarycentricCoordinates(const hkVector4f& vP, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkVector4f& result); // 0x00FD0520
    void calcBarycentricCoordinatesDeprecated(const hkVector4f& pos, const hkVector4f& t0, const hkVector4f& t1, const hkVector4f& t2, hkVector4f& result); // 0x00FD07A0
    int clipWithPlane(const hkVector4f& a, const hkVector4f& b, const hkVector4f& c, const hkVector4f& plane, const hkSimdFloat32& tolerance, hkVector4f* edgesOut); // 0x00FD0B00
    unsigned int checkForFlatConvexQuad(const hkVector4f& a, const hkVector4f& b, const hkVector4f& c, const hkVector4f& d, float tolerance); // 0x00FD0D30
    void calcUnitNormal(const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkVector4f& normal); // 0x00FD0FE0
    void calcNonUnitNormal(const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkVector4f& normal); // 0x00EFCD70
    void calcUnitPlaneEquation(const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkVector4f& planeOut); // 0x00FE3AE0
    void calcNonUnitPlaneEquation(const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkVector4f& planeOut); // 0x01010420
    hkSimdFloat32 calcDoubleArea(const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC); // 0x00FE3BA0
    void calcBarycentricCoordinatesNonDegenerate(const hkVector4f& vP, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkVector4f& result); // 0x00F10100
}
