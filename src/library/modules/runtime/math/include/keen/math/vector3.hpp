// Original: library/modules/runtime/math/include/keen/math/vector3.hpp
// Implementation: library/modules/runtime/math/sources/global/vector3.inl
//
// Vector3, a 3D float vector on top of keen::float3 (inline operations in sources/global/vector3.inl).
#pragma once

#include "keen/base/types.hpp"
#include <xmmintrin.h>

namespace keen
{
    struct Matrix33;
    struct Matrix43;
    struct Vector2;

    // size 0xC
    struct Vector3 : public float3
    {
        Vector3(float xValue, float yValue, float zValue); // 0x00485A90 (vector3.inl:46)
        Vector3(); // 0x00485A80 (vector3.inl:16)
        void set(__m128 pos); // 0x00CBB440 (vector3.inl:139)
        void set(float3 pos); // 0x004B1FC0 (vector3.inl:132)
        void set(float xValue, float yValue, float zValue); // 0x00C3A0B0 (vector3.inl:125)
        Vector3& setScaled(Vector3, Vector3);
        Vector3& setScaled(Vector3 other, float value); // 0x00C3A0E0 (vector3.inl:150)
        void clear(); // 0x00C3A090 (vector3.inl:111)
        Vector3& scale(Vector3);
        Vector3& scale(float value); // 0x00C3A140 (vector3.inl:175)
        Vector3& add(Vector3 a, Vector3 b); // 0x00C3A480 (vector3.inl:289)
        Vector3& add(Vector3 other); // 0x00C3A1A0 (vector3.inl:208)
        Vector3& addScaled(Vector3 a, Vector3 b, float value); // 0x00C3A280 (vector3.inl:235)
        Vector3& addScaled(Vector3 other, float value); // 0x00C3A200 (vector3.inl:227)
        Vector3& sub(Vector3 a, Vector3 b); // 0x00C3A4F0 (vector3.inl:298)
        Vector3& sub(Vector3 other); // 0x00C3A3A0 (vector3.inl:263)
        Vector3& subScaled(Vector3, Vector3, float);
        Vector3& subScaled(Vector3 other, float value); // 0x00C3A400 (vector3.inl:272)
        Vector3& createMinimum(Vector3 a, Vector3 b); // 0x00C3A560 (vector3.inl:329)
        Vector3& createMaximum(Vector3 a, Vector3 b); // 0x00C3A5E0 (vector3.inl:338)
        Vector3& div(Vector3);
        Vector3& lerp(Vector3 a, Vector3 b, float value); // 0x00C3A300 (vector3.inl:243)
        float getLength() const; // 0x00C34BB0 (vector3.inl:356)
        float getSquareLength() const; // 0x00C3A660 (vector3.inl:373)
        void setLength(float);
        float dotProduct(Vector3 other) const; // 0x00C3A690 (vector3.inl:410)
        Vector3& crossProduct(Vector3 first, Vector3 second); // 0x00C3A6C0 (vector3.inl:427)
        void normalize(Vector3 other); // 0x00C462C0 (vector3.inl:460)
        float normalize(); // 0x00C46220 (vector3.inl:448)
        float normalize0(); // 0x00C46360 (vector3.inl:476)
        float normalizeAndGetLength();
        Vector3& negate(Vector3 other); // 0x00C3A750 (vector3.inl:543)
        Vector3& negate(); // 0x00C5A120 (vector3.inl:534)
        bool isZero() const;
        bool isEqual(Vector3 other, float epsilon) const; // 0x00C46490 (vector3.inl:576)
        float get2DAngle() const;
        void set2DAngle(const float);
        void get2DVector(Vector2*) const;
        float get2DDirection() const; // 0x00C32920 (vector3.inl:623)
        void set2DDirection(const float);
        Vector3& mulMatrix(const Matrix33&);
        Vector3& mulMatrix(const Matrix43& matrix); // 0x00C46700 (vector3.inl:839)
        Vector3& mulMatrix(const Matrix33& matrix, Vector3 other); // 0x00C45530 (:37)
        Vector3& mulMatrix(const Matrix43& matrix, Vector3 other); // 0x00C45430 (:31)
        Vector3& getReflection(Vector3, Vector3);
        void reflect(Vector3);
        void extract(Vector3*, Vector3) const;
        Vector3& operator*=(float value); // 0x00C3A790 (vector3.inl:750)
        Vector3& operator/=(float);
        Vector3& operator+=(Vector3);
        Vector3& operator-=(Vector3);
        float& operator[](uint32 index); // 0x00C3A800 (vector3.inl:833)
        float operator[](uint32 index) const; // 0x00C3A7F0 (vector3.inl:827)
        static Vector3 getXAxis(); // 0x00C3A030 (vector3.inl:60)
        static Vector3 getYAxis(); // 0x00C3A050 (vector3.inl:72)
        static Vector3 getZAxis(); // 0x00C3A070 (vector3.inl:84)
        static Vector3 get0();
        static Vector3 getScaleUnit();

    };

    float dotProduct(Vector3 lhs, Vector3 rhs); // 0x00C3A810 (vector3.inl:849)
    float getSquareDistance(Vector3 lhs, Vector3 rhs); // 0x00C3A840 (vector3.inl:861)
    float getDistance(Vector3 lhs, Vector3 rhs); // 0x00C46810 (vector3.inl:866)
    bool operator==(Vector3 lhs, Vector3 rhs); // 0x00C468D0 (vector3.inl:881)
    bool operator!=(Vector3 lhs, Vector3 rhs); // 0x00C46B40 (vector3.inl:888)
    float operator*(Vector3 lhs, Vector3 rhs); // 0x00C3A8F0 (vector3.inl:903)

    // 0x00485A80 (vector3.inl:16)
    // Uninitialized.
    inline Vector3::Vector3()
    {
    }
}
