// Original: library/modules/runtime/math/include/keen/math/sphere.hpp
//
// Sphere, a center and a radius.
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    // size 0x10
    struct Sphere
    {
        Sphere(const Vector3&, float);
        Sphere(); // 0x00C3BE20 (.h:25)
        void merge(const Vector3& point); // 0x00C54220 (:35)
        void merge(const Sphere& other); // 0x00C53FF0 (:11)
        static const Sphere& get0();
        bool contains(const Vector3& point) const; // 0x00C44AE0 (:49)

        Vector3 center; // 0x00
        float radius; // 0x0C
    };
}
