// Original: library/modules/runtime/math/include/keen/math/noise.hpp
// Implementation: library/modules/runtime/math/sources/global/noise.cpp
//
// Noise functions.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    class RandomNumberGenerator;
    struct Vector2;
    struct Vector3;

    namespace PerlinNoise
    {
        void initialize(RandomNumberGenerator randomNumberGenerator); // 0x00C4F630 (:51)
        void shutdown(); // 0x00C42B70 (:89)
        float noise1(float arg); // 0x00C42B80 (:104)
        float noise2(const Vector2& arg); // 0x00C434D0 (:200)
        float noise3(const Vector3& arg); // 0x00C434F0 (:205)
    }
}
