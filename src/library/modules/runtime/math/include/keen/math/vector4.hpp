// Original: library/modules/runtime/math/include/keen/math/vector4.hpp
// Implementation: library/modules/runtime/math/sources/global/vector4.inl
//
// Vector4, a 4D float vector on top of keen::float4 (inline operations in sources/global/vector4.inl).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct Matrix44;
    struct Vector3;

    // size 0x10
    struct Vector4 : public float4
    {
        Vector4(Vector3 xyz, float wValue); // 0x00C3BAB0 (vector4.inl:36)
        Vector4(float xValue, float yValue, float zValue, float wValue); // 0x00C3BAE0 (vector4.inl:58)
        Vector4(); // 0x004B1FF0 (vector4.inl:20)
        Vector4& set(Vector3 xyz, float wValue); // 0x00C3BB40 (vector4.inl:174)
        Vector4& set(float xValue, float yValue, float zValue, float wValue); // 0x00C3BB10 (vector4.inl:163)
        Vector4& setScaled(Vector4, float);
        Vector4& clear();
        Vector4& scale(Vector4);
        Vector4& scale(float value); // 0x00C3BB70 (vector4.inl:199)
        Vector4& add(Vector4, Vector4);
        Vector4& add(Vector4);
        Vector4& addScaled(Vector4, Vector4, float);
        Vector4& addScaled(Vector4, float);
        Vector4& sub(Vector4, Vector4);
        Vector4& sub(Vector4);
        Vector4& subScaled(Vector4, Vector4, float);
        Vector4& subScaled(Vector4, float);
        float getLength() const;
        float getSquareLength() const;
        void setLength(float);
        float dotProduct(Vector4) const;
        void normalize();
        void normalize0();
        float normalizeAndGetLength();
        Vector4& negate();
        bool isZero() const;
        bool isEqual(Vector4 other, float epsilon) const; // 0x00CE4100 (vector4.inl:567)
        Vector4& mulMatrix(const Matrix44& matrix, Vector4 other); // 0x00C45610 (:32)
        static Vector4 getXAxis();
        static Vector4 getYAxis();
        static Vector4 getZAxis();
        static Vector4 getWAxis();
        static Vector4 get0();
        Vector4& operator*=(float);
        Vector4& operator/=(float);
        Vector4& operator+=(Vector4);
        Vector4& operator-=(Vector4);

    protected:
        static const Vector4 XAxis;
        static const Vector4 YAxis;
        static const Vector4 ZAxis;
        static const Vector4 WAxis;
        static const Vector4 Null;
    };

    float dotProduct(Vector4 lhs, Vector4 rhs); // 0x00C3BBE0 (vector4.inl:624)
}
