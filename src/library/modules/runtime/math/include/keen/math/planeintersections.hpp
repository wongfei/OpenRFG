// Original: library/modules/runtime/math/include/keen/math/planeintersections.hpp
// Implementation: library/modules/runtime/math/sources/global/planeintersections.cpp
//
// Plane intersection helpers.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct Plane;
    struct Vector3;

    bool isPointOnPlane(const Vector3& point, const Plane& plane, float epsilon); // 0x00C52750 (:22)
    bool getPlaneRayIntersection(const Plane& plane, const Vector3& rayOrigin, const Vector3& rayDirection, Vector3* pPosition); // 0x00C52830 (:41)
    bool getPlaneLineIntersection(Vector3* pIntersection, const Plane& plane, const Vector3& p0, const Vector3& p1); // 0x00C52AD0 (:67)
    bool getThreePlanesIntersection(const Plane& plane0, const Plane& plane1, const Plane& plane2, Vector3* pPoint); // 0x00C52D60 (:109)
}
