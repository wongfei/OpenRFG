// Original: library/modules/runtime/math/include/keen/math/axisalignedboxintersections.hpp
// Implementation: library/modules/runtime/math/sources/global/axisalignedboxintersections.cpp
//
// Box versus box, ray and triangle tests.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct AxisAlignedBox;
    struct Vector3;

    bool isAxisAlignedBoxIntersecting(const AxisAlignedBox& box0, const AxisAlignedBox& box1); // 0x00C3AEF0 (:13)
    bool getAxisAlignedBoxRayIntersection(Vector3* pPosition, const AxisAlignedBox& box, const Vector3& rayOrigin, const Vector3& rayDirection); // 0x00C47550 (:35)
    bool isAxisAlignedBoxTriangleIntersecting(const AxisAlignedBox& box, Vector3* triangleVertices); // 0x00C563D0 (:121)
    bool isAxisAlignedBoxInsideAnotherAxisAlignedBox(const AxisAlignedBox& smallBox, const AxisAlignedBox& bigBox, const float epsilon); // 0x00C3B140 (:336)
}
