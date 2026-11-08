// Original: library/modules/runtime/math/include/keen/math/cone.hpp
// Implementation: library/modules/runtime/math/sources/global/cone.cpp
//
// Cone, an origin, a direction and a gradient (opening slope).
#pragma once

#include "keen/base/types.hpp"
#include "keen/math/vector3.hpp"

namespace keen
{
    // size 0x1C
    class Cone
    {
    public:
        Cone(const Vector3& originValue, const Vector3& directionValue, float gradientValue); // 0x00C3BA30 (:8)
        Cone();
        void create(const Vector3& originValue, const Vector3& directionValue, float gradientValue); // 0x00C3BA70 (:15)

        Vector3 origin; // 0x00
        Vector3 direction; // 0x0C
        float gradient; // 0x18
    };
}
