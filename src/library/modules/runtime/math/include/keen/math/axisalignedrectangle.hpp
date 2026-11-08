// Original: library/modules/runtime/math/include/keen/math/axisalignedrectangle.hpp
// Implementation: library/modules/runtime/math/sources/global/axisalignedrectangle.cpp
//
// AxisAlignedRectangle, a 2D rectangle as position and size; clampPointToRectangle.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct Matrix43;
    struct Vector2;

    // size 0x10
    struct AxisAlignedRectangle
    {
        AxisAlignedRectangle* set(float inX, float inY, float inWidth, float inHeight); // 0x004D4780 (axisalignedrectangle.inl:12)
        AxisAlignedRectangle* clear();
        AxisAlignedRectangle* createEnclosingRectangle(const AxisAlignedRectangle& rect0, const AxisAlignedRectangle& rect1); // 0x00C3B510 (:23)
        AxisAlignedRectangle* createAlignedRectangle(const AxisAlignedRectangle& alignmentBoundary, float sourceWidth, float sourceHeight, float horizontalAlignment, float verticalAlignment); // 0x00C3B6F0 (:35)
        AxisAlignedRectangle* clipToRect(const AxisAlignedRectangle& outerRectangle); // 0x00C3B770 (:45)
        AxisAlignedRectangle* move(float deltaX, float deltaY); // 0x004D47B0 (axisalignedrectangle.inl:29)
        AxisAlignedRectangle* transform(const Matrix43&);
        AxisAlignedRectangle* grow(float, float);
        AxisAlignedRectangle* shrink(float, float);
        AxisAlignedRectangle* insertPoint(float pointX, float pointY); // 0x00C3B440 (axisalignedrectangle.inl:87)
        AxisAlignedRectangle* insertRect(AxisAlignedRectangle);
        bool isEmpty() const; // 0x00C5CA40 (axisalignedrectangle.inl:77)
        bool containsPoint(float, float) const;

        float x; // 0x00
        float y; // 0x04
        float width; // 0x08
        float height; // 0x0C
    };

    void clampPointToRectangle(Vector2* pPoint, const AxisAlignedRectangle& rect); // 0x00C484F0 (:11)
}
