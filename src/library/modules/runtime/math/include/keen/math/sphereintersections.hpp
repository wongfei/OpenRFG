// Original: library/modules/runtime/math/include/keen/math/sphereintersections.hpp
// Implementation: library/modules/runtime/math/sources/global/sphereintersections.cpp
//
// Sphere intersection helpers.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct AxisAlignedBox;
    class Cone;
    struct Matrix43;
    struct Sphere;
    struct Vector3;

    bool getSphereRayIntersection(const Sphere& sphere, const Vector3& rayOrigin, const Vector3& rayDirection, Vector3* pPosition); // 0x00C543A0 (:27)
    bool isSphereConeIntersecting(const Sphere& sphere, const Cone& cone); // 0x00C545D0 (:63)
    bool isSphereSphereIntersecting(const Sphere& sphereA, const Sphere& sphereB); // 0x00C54770 (:81)
    bool isSphereAxisAlignedBoxIntersecting(const Sphere& sphere, const AxisAlignedBox& box); // 0x00C44BB0 (:88)
    bool isSphereAxisAlignedBoxIntersecting(const Sphere& sphere, const AxisAlignedBox& box, const Matrix43& worldTransform); // 0x00C44D60 (:131)
}
