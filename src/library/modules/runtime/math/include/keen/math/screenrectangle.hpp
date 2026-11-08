// Original: library/modules/runtime/math/include/keen/math/screenrectangle.hpp
// Implementation: library/modules/runtime/math/sources/global/screenrectangle.cpp
//
// ScreenRectangle, a 2D rectangle as min and max corners (screen space).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector2.hpp"

namespace keen
{
    // size 0x10
    struct ScreenRectangle
    {
        ScreenRectangle(float x0, float x1, float y0, float y1); // 0x00C53EF0 (:14)
        ScreenRectangle(); // 0x00C53ED0 (:9)
        void set(float x0, float x1, float y0, float y1); // 0x00C448F0 (:19)
        void clear(); // 0x00C44920 (:28)
        void clamp(float minX, float minY, float maxX, float maxY); // 0x00C53F20 (:33)
        float getWidth() const;
        float getHeight() const;
        bool isEmpty() const; // 0x00C43710 (.h:29)
        bool contains(const ScreenRectangle& other) const; // 0x00C44940 (:41)
        bool contains(float, float) const;

        Vector2 min; // 0x00
        Vector2 max; // 0x08
    };

    bool getIntersection(ScreenRectangle* pTarget, const ScreenRectangle& first, const ScreenRectangle& second); // 0x00C449B0 (:48)
}
