// Original: library/modules/runtime/graphicsbase/include/keen/graphics/color.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/color.cpp
//
// 32-bit colors (uint32, 0xAARRGGBB): channel accessors (color.inl) and blending helpers.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    uint8 getColorR(uint32 value); // 0x004B2000 (color.inl:25)
    uint8 getColorG(uint32 value); // 0x004B2010 (color.inl:30)
    uint8 getColorB(uint32 value); // 0x004B2020 (color.inl:35)
    uint8 getColorA(uint32 value); // 0x004B2030 (color.inl:40)
    uint32 setColorR(uint32 input, uint8 red); // 0x00CE2D40 (color.inl:53)
    uint32 setColorG(uint32 input, uint8 green); // 0x00CE2D60 (color.inl:58)
    uint32 setColorB(uint32 input, uint8 blue); // 0x00CE2D80 (color.inl:63)
    uint32 setColorA(uint32 input, uint8 alpha); // 0x00CE2DA0 (color.inl:68)
    uint32 lerpColor(uint32 a, uint32 b, float position); // 0x00CE44E0 (:8)
    uint32 scaleColor(uint32 colSrc, float scale); // 0x00CE4690 (:51)
    uint32 preMultiplyColorAlpha(uint32 sourceColor); // 0x00CE2DC0 (:80)
    uint32 addColor(uint32 a, uint32 b); // 0x00CE47A0 (:91)
    uint32 mulColor(uint32 lhs, uint32 rhs); // 0x00CE4870 (:118)
}
