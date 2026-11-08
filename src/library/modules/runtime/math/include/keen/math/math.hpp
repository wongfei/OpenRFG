// Original: library/modules/runtime/math/include/keen/math/math.hpp
// Implementation: library/modules/runtime/math/sources/global/math.cpp
//
// Interpolation helpers (cosine, B-spline). calculateBSplineInterpolation<T> is a function template; its instantiations
// are listed as comments.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    float cosineInterpolation(float a, float b, float f); // 0x00C3F020 (:26)
    float cosineInterpolation(float f); // 0x00C3F090 (:47)
    // template instance: void calculateBSplineInterpolation<Vector3>(Vector3* pResult, const Vector3* pCVs, float fraction); // 0x00C45E00 (:62)
    // template instance: void calculateBSplineInterpolation<float>(float* pResult, const float* pCVs, float fraction); // 0x00C3F0D0 (:85)
    float calculateBSplineTangent(float* cvs, float fraction); // 0x00C3F1E0 (:101)
}
