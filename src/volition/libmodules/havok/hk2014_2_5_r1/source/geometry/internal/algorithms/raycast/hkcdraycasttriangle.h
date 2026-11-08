// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycasttriangle.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdray.h"

class hkFourTransposedPointsf;
class hkSimdFloat32;
class hkVector4f;
class hkVector4fComparison;
struct hkcdRayBundle;

namespace hkcdRayCastTriangle
{
    unsigned int sphereCastTriangle(const hkcdRay& ray, const hkSimdFloat32& radius, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkSimdFloat32* fractionOut, hkVector4f* planeOut); // 0x00EF4E70
    hkVector4fComparison hkcdSegmentBundleTriangleIntersect(const hkcdRayBundle& rayBundle, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, const hkSimdFloat32& tolerance, hkFourTransposedPointsf& normalsOut, hkVector4f& fractionsInOut); // 0x00EF5510
    hkcdRayCastResult safeUsingDynamicTolerance(const hkcdRay& ray, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkFlags<enum hkcdRayQueryFlags::Enum,unsigned int> flags, hkSimdFloat32& fractionInOut, hkVector4f& nonUnitNormalOut); // 0x00EF5890
    hkcdRayCastResult fastUsingZeroTolerance(const hkcdRay& ray, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkFlags<enum hkcdRayQueryFlags::Enum,unsigned int> flags, hkSimdFloat32& fractionInOut, hkVector4f& nonUnitNormalOut); // 0x00EF5D90
    // template instance: hkcdRayCastResult _rayTriangleImpl<1,1,1>(const hkcdRay& ray, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkFlags<enum hkcdRayQueryFlags::Enum,unsigned int> flags, hkSimdFloat32& fractionInOut, hkVector4f& nonUnitNormalOut); // 0x00EF6A50
    // template instance: hkcdRayCastResult _rayTriangleImpl<0,1,0>(const hkcdRay& ray, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, hkFlags<enum hkcdRayQueryFlags::Enum,unsigned int> flags, hkSimdFloat32& fractionInOut, hkVector4f& nonUnitNormalOut); // 0x00EF6890
}
