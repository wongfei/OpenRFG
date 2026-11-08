// Original: library/modules/runtime/graphicsbase/include/keen/graphics/colorhsv.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/colorhsv.cpp
//
// Conversions between RGB and HSV (float3) and interpolation in HSV space.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct float3;
    struct float4;

    void convertRgbToHsv(float3* pHsv, float red, float green, float blue); // 0x00CE2E40 (:8)
    void convertRgbToHsv(float3* pHsv, const float3& rgb); // 0x00CE2FE0 (:53)
    void convertColorToHsv(float3* pHsv, float* pAlpha, uint32 color); // 0x00CE3020 (:58)
    void convertHsvToRgb(float* pRed, float* pGreen, float* pBlue, const float3& hsv); // 0x00CE30B0 (:70)
    void convertHsvToRgb(float3* pRgb, const float3& hsv); // 0x00CE3200 (:142)
    uint32 convertHsvToColor(const float3& hsv, float alpha); // 0x00CE3220 (:149)
    void lerpHsv(float3* pHsv, const float3& hsvStart, const float3& hsvEnd, float position); // 0x00CE4990 (:157)
    void lerpRgbInHsvSpace(float3* pRgb, const float3& rgbStart, const float3& rgbEnd, float position); // 0x00CE4D20 (:166)
    void lerpRgbaInHsvSpace(float4* pRgba, const float4& rgbaStart, const float4& rgbaEnd, float position); // 0x00CE4DC0 (:178)
    uint32 lerpColorInHsvSpace(uint32 colorStart, uint32 colorEnd, float position); // 0x00CE4EE0 (:203)
}
