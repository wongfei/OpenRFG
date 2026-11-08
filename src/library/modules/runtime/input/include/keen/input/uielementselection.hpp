// Original: library/modules/runtime/input/include/keen/input/uielementselection.hpp
// Implementation: library/modules/runtime/input/sources/global/uielementselection.cpp
//
// Directional selection between UI element rectangles (cost of a transition by distance and angle).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct AxisAlignedRectangle;
    struct Plane;
    struct Vector2;

    // size 0xC
    struct RectangleTransitionCosts
    {
        RectangleTransitionCosts(float distance, float anglePenalty, uint32 timeOfLastVisit); // 0x00C74F10 (:131)
        RectangleTransitionCosts(); // 0x00C74EF0 (:122)
        bool operator<(const RectangleTransitionCosts& otherCosts) const; // 0x00C74F40 (:134)

        float distance; // 0x00
        float anglePenalty; // 0x04
        uint32 timeOfLastVisit; // 0x08
    };

    bool getNextUiElementRect(uint32* pRectangleIndex, const AxisAlignedRectangle* pRectangles, uint32 rectangleCount, uint32 currentRectangleIndex, const AxisAlignedRectangle& currentRectangle, const Vector2& direction, float azimuth, bool wrapAround, bool preferStraightDirection, bool useRectCenterAsFocusOrigin, const uint32* pLastVisitTimes, const Plane* pCullingPlane); // 0x00C7B990 (:169)
    void getSilhouettePoints(Vector2* pPoints, const AxisAlignedRectangle& rectangle, const Vector2& direction, bool useRectCenterAsFocusOrigin); // 0x00C77250 (:341)
}
