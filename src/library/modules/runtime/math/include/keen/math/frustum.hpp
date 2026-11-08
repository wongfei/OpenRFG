// Original: library/modules/runtime/math/include/keen/math/frustum.hpp
// Implementation: library/modules/runtime/math/sources/global/frustum.cpp
//
// Frustum, six planes plus helpers to build it from a camera (world matrix and projection), from a box or from points.
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/plane.hpp"

namespace keen
{
    struct AxisAlignedBox;
    struct Matrix43;
    struct Projection;
    struct Vector3;

    // size 0x60
    class Frustum
    {
    public:
        enum PlaneSide
        {
            PlaneSide_Left = 0,
            PlaneSide_Right = 1,
            PlaneSide_Top = 2,
            PlaneSide_Bottom = 3,
            PlaneSide_Near = 4,
            PlaneSide_Far = 5,
            PlaneSide_Count = 6,
        };

        enum PlaneMask
        {
            PlaneMask_Left = 1,
            PlaneMask_Right = 2,
            PlaneMask_Top = 4,
            PlaneMask_Bottom = 8,
            PlaneMask_Near = 16,
            PlaneMask_Far = 32,
            PlaneMask_All = 63,
            PlaneMask_AllSides = 15,
        };

        enum CornerPoint
        {
            CornerPoint_LeftNearTop = 0,
            CornerPoint_RightNearTop = 1,
            CornerPoint_RightNearBottom = 2,
            CornerPoint_LeftNearBottom = 3,
            CornerPoint_LeftFarTop = 4,
            CornerPoint_RightFarTop = 5,
            CornerPoint_RightFarBottom = 6,
            CornerPoint_LeftFarBottom = 7,
            CornerPoint_Count = 8,
        };

        void createFromAxisAlignedBox(const AxisAlignedBox& box); // 0x00C56640 (:29)
        void createFromPoints(const Vector3& leftTop, const Vector3& leftBottom, const Vector3& rightTop, const Vector3& rightBottom, const Vector3& origin, float nearPlaneDistance, float farPlaneDistance); // 0x00C49A80 (:64)
        void createFromPoints(const Vector3* points); // 0x00C488D0 (:54)
        bool getPoints(Vector3* points) const; // 0x00C56750 (:99)
        Plane getPlane(uint32 i) const; // 0x00C3BDF0 (.h:68)
        Plane* getPlanes();
        const Plane* getPlanes() const;
        void getMinimumMaximum(Vector3* pMinimum, Vector3* pMaximum, const Vector3* pCornerPoints) const; // 0x00C3C240 (:137)
        bool getMinimumMaximum(Vector3* pMinimum, Vector3* pMaximum) const; // 0x00C57230 (:175)
        bool containsPoint(const Vector3& point) const; // 0x00C3C170 (:15)
        void transform(const Matrix43& transform); // 0x00C56830 (:123)
        void createFromCameraParameters(const Matrix43& worldMatrix, const Projection& projection, float x0, float y0, float x1, float y1); // 0x00C4AA60 (:116)
        Frustum(); // 0x00CE4440

    protected:
        Plane m_planes[6]; // 0x00
    };

    void createFrustumPointsFromCameraParameters(Vector3* points, const Matrix43& worldMatrix, const Projection& projection, float x0, float y0, float x1, float y1); // 0x00C3CA40 (:188)
}
