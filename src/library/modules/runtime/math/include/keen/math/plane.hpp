// Original: library/modules/runtime/math/include/keen/math/plane.hpp
//
// Plane, a plane as normal and distance (a Vector4).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector4.hpp"

namespace keen
{
    struct Vector3;

    // size 0x10
    struct Plane
    {
        enum HalfSpace
        {
            HalfSpace_OnPlane = 0,
            HalfSpace_Positive = 1,
            HalfSpace_Negative = 2,
        };

        Plane(Vector3, Vector3, Vector3);
        Plane(Vector3, Vector3);
        Plane(Vector3, float);
        Plane(float, float, float, float);
        Plane(); // 0x00C3BC10 (.h:31)
        Plane& create(Vector3 a, Vector3 b, Vector3 c); // 0x00C485A0 (plane.inl:41)
        Plane& create(Vector3 point, Vector3 normal); // 0x00C3BC20 (plane.inl:34)
        Plane& create(Vector3, float);
        Plane& normalize(const Plane&);
        Plane& normalize();
        void reverse();
        void negate(); // 0x00C3BC90 (plane.inl:79)
        Vector3 getNormal() const; // 0x00C3BCC0 (plane.inl:84)
        HalfSpace getHalfSpace(Vector3 point, float epsilon) const; // 0x00C3BCE0 (plane.inl:89)
        float getDistance(Vector4) const;
        float getDistance(Vector3 point) const; // 0x00C3BDA0 (plane.inl:107)
        float getAbsoluteDistance(Vector3 point) const; // 0x00C48810 (plane.inl:117)
        void projectPoint(Vector3*, Vector3) const;
        static Plane getXYPlane();
        static Plane getXZPlane();
        static Plane getYZPlane();

        Vector4 data; // 0x00
    };
}
