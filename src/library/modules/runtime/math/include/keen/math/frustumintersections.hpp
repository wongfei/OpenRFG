// Original: library/modules/runtime/math/include/keen/math/frustumintersections.hpp
// Implementation: library/modules/runtime/math/sources/global/frustumintersections.cpp
//
// Frustum versus box, sphere, rectangle, point and frustum tests (used by keen culling).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct AxisAlignedBox;
    class Frustum;
    struct Matrix43;
    struct Sphere;
    struct Vector3;

    bool isFrustumAxisAlignedBoxIntersecting(const Frustum& frustum, const AxisAlignedBox& box); // 0x00C3DDC0 (:15)
    bool isFrustumAxisAlignedBoxIntersecting(const Frustum& frustum, const AxisAlignedBox& box, const Matrix43& transform); // 0x00C4AAF0 (:44)
    bool isFrustumIntersectingRectangle(const Frustum& frustum, const Vector3* rectanglePoints); // 0x00C3E440 (:141)
    bool isFrustumIntersectingSphere(const Frustum& frustum, const Sphere& sphere); // 0x00C3E820 (:190)
    bool isFrustumIntersectingSpherePrecise(const Frustum& frustum, const Sphere& sphere, const Vector3* points); // 0x00C4B980 (:203)
    bool isPointInsideFrustum(const Frustum& frustum, const Vector3& testPoint); // 0x00C3E900 (:251)
    bool isFrustumAxisAlignedBoxIntersectingPrecise(const Frustum& frustum, const AxisAlignedBox& box); // 0x00C572A0 (:266)
    bool isFrustumIntersectingFrustum(const Frustum& frustum, const Frustum& testFrustum); // 0x00C57440 (:297)
}
