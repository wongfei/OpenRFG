// Original: library/modules/runtime/math/include/keen/math/orientedbox.hpp
// Implementation: library/modules/runtime/math/sources/global/orientedbox.cpp
//
// OrientedBox, a box with an orientation (center, axes, extents) and its eight corner points.
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    struct AxisAlignedBox;
    struct Matrix43;
    struct Plane;
    struct Sphere;

    // size 0x60
    struct OrientedBox
    {
        OrientedBox(const Vector3& p0, const Vector3& p1); // 0x00C4F8B0 (:14)
        OrientedBox(); // 0x00C5E5C0 (.h:41)
        OrientedBox& createFromPoints(const Vector3& p0, const Vector3& p1); // 0x00C43510 (:19)
        OrientedBox& createFromAxisAlignedBox(const AxisAlignedBox& aabb); // 0x00C436D0 (:38)
        void setPoint(uint32 index, const Vector3& p); // 0x00C436F0 (:43)
        const Vector3& getPoint(uint32 index) const; // 0x00C3A920 (.h:48)
        const Vector3* getPoints() const;
        OrientedBox& transform(const Matrix43& transformation, const OrientedBox& source); // 0x00C500E0 (:59)
        OrientedBox& transform(const Matrix43& transformation); // 0x00C4F8D0 (:49)
        void createBoundingPlanes(Plane* planes); // 0x00C50900 (:69)

    private:
        Vector3 m_points[8]; // 0x00
    };

    bool operator!=(const OrientedBox& lhs, const OrientedBox& rhs); // 0x00C51DB0 (:80)
    bool operator==(const OrientedBox& lhs, const OrientedBox& rhs); // 0x00C52070 (:92)
    void createBoundingSphere(Sphere* pSphere, const OrientedBox& box); // 0x00C52330 (:104)
}
