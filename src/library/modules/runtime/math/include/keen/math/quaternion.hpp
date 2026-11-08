// Original: library/modules/runtime/math/include/keen/math/quaternion.hpp
// Implementation: library/modules/runtime/math/sources/global/quaternion.cpp
//
// Quaternion, a rotation (x, y, z, w).
#pragma once

#include "keen/base/types.hpp"
#include <xmmintrin.h>

namespace keen
{
    struct Matrix33;
    struct Vector3;

    // size 0x10
    struct Quaternion : public float4
    {
        Quaternion(float inX, float inY, float inZ, float inR); // 0x00C75F20 (quaternion.inl:17)
        Quaternion(); // 0x00C3F4C0 (quaternion.inl:13)
        void createUnit(); // 0x00CBB520 (quaternion.inl:25)
        void fromMatrix(const Matrix33& source); // 0x00C53B70 (:93)
        void toMatrix(Matrix33* pDestination) const; // 0x00C3F540 (quaternion.inl:409)
        void set(__m128 value); // 0x00CBB5A0 (quaternion.inl:61)
        void set(float4 value); // 0x00CBB570 (quaternion.inl:53)
        void set(float inX, float inY, float inZ, float r); // 0x00CBB540 (quaternion.inl:45)
        float getModulus() const;
        float getModulusSquared() const;
        void multiply(Quaternion first, Quaternion second); // 0x00CBB5F0 (quaternion.inl:218)
        Quaternion& multiply(Quaternion other); // 0x00C75FF0 (quaternion.inl:189)
        void negate(); // 0x00C3F4D0 (quaternion.inl:239)
        void invert(); // 0x00C75F90 (quaternion.inl:88)
        void invert(Quaternion source); // 0x00C75F50 (quaternion.inl:80)
        void rotate(const Vector3&, const float);
        Quaternion& createRotation(const Vector3&, const float);
        void scale(float);
        float dot(Quaternion other) const; // 0x00C3F510 (quaternion.inl:283)
        void normalize();
        bool isEqual(Quaternion, float) const;
        bool isEqual(Quaternion) const;
        void getImag(Vector3*) const;
        float getReal() const;
        void interpolate(Quaternion first, Quaternion second, float position); // 0x00C44500 (:26)
        void slerp(Quaternion, Quaternion, float);
        void nlerp(Quaternion first, Quaternion second, float position); // 0x00CBEA30 (quaternion.inl:376)
        static Quaternion getUnit();

    };

    void toEulerXYZ(Vector3* pResult, const Quaternion& q); // 0x00C447B0 (:154)
}
