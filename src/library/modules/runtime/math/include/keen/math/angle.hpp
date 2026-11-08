// Original: library/modules/runtime/math/include/keen/math/angle.hpp
// Implementation: library/modules/runtime/math/sources/global/angle.inl
//
// Trigonometry wrappers and angle normalization/interpolation (sources/global/angle.inl).
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    float getSin(float value); // 0x004B1F80 (.h:47)
    float getCos(float value); // 0x004B1FA0 (.h:48)
    void getSinCos(float* pSin, float* pCos, float value); // 0x00C32800 (.h:49)
    float getACos(float value); // 0x00C39E30 (.h:50)
    float getASin(float value); // 0x00C39E50 (.h:51)
    float getTan(float value); // 0x00C39E70 (.h:52)
    float getAtan2(float y, float x); // 0x00C32830 (.h:53)
    float normalizeAngle(const float angle); // 0x00CE3CD0 (angle.inl:10)
    float lerpAngle(float a, float b, float position); // 0x00CE3D60 (angle.inl:150)
}
