// Original: library/modules/runtime/math/include/keen/math/line.hpp
// Implementation: library/modules/runtime/math/sources/global/line.cpp
//
// Line, a line through two points, and the line distance and 2D segment intersection helpers. Placement of
// LineSegmentIntersectionType inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    struct Vector2;

    enum LineSegmentIntersectionType
    {
        LineSegmentIntersectionType_NoIntersection = 0,
        LineSegmentIntersectionType_Intersect = 1,
        LineSegmentIntersectionType_Collinear = 2,
    };

    // size 0x18
    struct Line
    {
        Line(const Vector3& point0, const Vector3& point1); // 0x00C3E9D0 (:9)
        Line();
        void set(const Vector3& point0, const Vector3& point1); // 0x00C3EA00 (:15)
        float getShortestDistanceToLine(const Line& otherLine) const; // 0x00C57650 (:21)

        Vector3 p0; // 0x00
        Vector3 p1; // 0x0C
    };

    float getShortestDistanceBetweenLines(const Line& firstLine, const Line& secondLine); // 0x00C4BEF0 (:28)
    void computeClosestPointsOnLines(Vector3* pPointOnFirstLine, Vector3* pPointOnSecondLine, const Line& firstLine, const Line& secondLine); // 0x00C3EA30 (:38)
    LineSegmentIntersectionType get2DLineSegmentIntersection(Vector2* pIntersection, float* pPositionOnA, float* pPositionOnB, const Line& a, const Line& b, float epsilon); // 0x00C4BFC0 (:85)
}
