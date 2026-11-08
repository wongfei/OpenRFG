// Original: library/modules/runtime/math/include/keen/math/vector2.hpp
//
// Vector2, a 2D float vector on top of keen::float2.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x8
    struct Vector2 : public float2
    {
        Vector2(float xValue, float yValue); // 0x00C39E90 (vector2.inl:31)
        Vector2(); // 0x00C328D0 (vector2.inl:15)
        void set(float xValue, float yValue); // 0x00C39EB0 (vector2.inl:86)
        Vector2& setScaled(Vector2, Vector2, Vector2);
        Vector2& setScaled(Vector2, Vector2);
        Vector2& setScaled(Vector2, float);
        void clear();
        Vector2& scale(Vector2);
        Vector2& scale(float value); // 0x00C74450 (vector2.inl:116)
        Vector2& add(Vector2 a, Vector2 b); // 0x00C39ED0 (vector2.inl:163)
        Vector2& add(float);
        Vector2& add(Vector2 other); // 0x00C74490 (vector2.inl:147)
        Vector2& addScaled(Vector2 a, Vector2 b, float value); // 0x00C5A0A0 (vector2.inl:197)
        Vector2& addScaled(Vector2, float);
        Vector2& sub(Vector2 a, Vector2 b); // 0x00C39F20 (vector2.inl:230)
        Vector2& sub(Vector2);
        Vector2& subScaled(Vector2, Vector2, float);
        Vector2& subScaled(Vector2, float);
        Vector2& div(Vector2);
        float getLength() const; // 0x00C60EF0 (vector2.inl:311)
        float getSquareLength() const; // 0x00C5A100 (vector2.inl:332)
        void setLength(float);
        float dotProduct(Vector2) const;
        void getNormal(Vector2*) const;
        void getPerpendicular(Vector2*) const;
        void normalize();
        void normalize0();
        void negate(Vector2);
        void negate();
        float getAngle() const;
        float getDirection() const;
        bool isZero() const;
        void setAngle(float);
        void setDirection(float angle); // 0x00C328E0 (vector2.inl:571)
        void rotate(float sina, float cosa); // 0x00C744D0 (vector2.inl:613)
        void rotate(float angle); // 0x00C76870 (vector2.inl:588)
        Vector2& createMinimum(Vector2, Vector2, Vector2);
        Vector2& createMinimum(Vector2 a, Vector2 b); // 0x00C39F70 (vector2.inl:860)
        Vector2& createMaximum(Vector2, Vector2, Vector2);
        Vector2& createMaximum(Vector2 a, Vector2 b); // 0x00C39FD0 (vector2.inl:876)
        bool isEqual(Vector2, float) const;
        float& operator[](const uint32);
        float operator[](const uint32) const;
        bool operator==(Vector2) const;
        bool operator!=(Vector2) const;
        float operator*(Vector2) const;
        Vector2& operator*=(float);
        Vector2& operator/=(float);
        Vector2& operator+=(Vector2);
        Vector2& operator-=(Vector2);
        static Vector2 getXAxis();
        static Vector2 getYAxis();
        static Vector2 get0();

    };
}
