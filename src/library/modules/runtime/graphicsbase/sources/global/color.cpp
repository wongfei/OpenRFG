// Original: library/modules/runtime/graphicsbase/sources/global/color.cpp
//
// 32-bit color helpers (red in the low byte, alpha in the high byte). The channel accessors are inline in color.inl in
// the original. Channels are clamped to [0, 255] after float math.

#include "keen/graphics/color.hpp"

namespace keen
{
    static inline uint32 clampChannel(sint32 value)
    {
        if (value < 0)
        {
            return 0;
        }
        return value > 0xff ? 0xffu : (uint32)value;
    }

    // 0x004B2000 (color.inl:25)
    uint8 getColorR(uint32 value)
    {
        return (uint8)value;
    }

    // 0x004B2010 (color.inl:30)
    uint8 getColorG(uint32 value)
    {
        return (uint8)(value >> 8);
    }

    // 0x004B2020 (color.inl:35)
    uint8 getColorB(uint32 value)
    {
        return (uint8)(value >> 16);
    }

    // 0x004B2030 (color.inl:40)
    uint8 getColorA(uint32 value)
    {
        return (uint8)(value >> 24);
    }

    // 0x00CE2D40 (color.inl:53)
    uint32 setColorR(uint32 input, uint8 red)
    {
        return (input & 0xffffff00u) | red;
    }

    // 0x00CE2D60 (color.inl:58)
    uint32 setColorG(uint32 input, uint8 green)
    {
        return ((uint32)green << 8) | (input & 0xffff00ffu);
    }

    // 0x00CE2D80 (color.inl:63)
    uint32 setColorB(uint32 input, uint8 blue)
    {
        return ((uint32)blue << 16) | (input & 0xff00ffffu);
    }

    // 0x00CE2DA0 (color.inl:68)
    uint32 setColorA(uint32 input, uint8 alpha)
    {
        return ((uint32)alpha << 24) | (input & 0x00ffffffu);
    }

    // 0x00CE44E0 (:8)
    uint32 lerpColor(uint32 a, uint32 b, float position)
    {
        if (position == 0.0f)
        {
            return a;
        }
        if (position == 1.0f)
        {
            return b;
        }
        uint32 result = 0;
        for (uint32 shift = 0; shift < 32; shift += 8)
        {
            const float channelA = (float)((a >> shift) & 0xffu);
            const float channelB = (float)((b >> shift) & 0xffu);
            result |= clampChannel((sint32)((channelB - channelA) * position + channelA)) << shift;
        }
        return result;
    }

    // 0x00CE4690 (:51)
    uint32 scaleColor(uint32 colSrc, float scale)
    {
        uint32 result = 0;
        for (uint32 shift = 0; shift < 32; shift += 8)
        {
            result |= clampChannel((sint32)((float)((colSrc >> shift) & 0xffu) * scale)) << shift;
        }
        return result;
    }

    // 0x00CE2DC0 (:80)
    uint32 preMultiplyColorAlpha(uint32 sourceColor)
    {
        const uint32 alpha = sourceColor >> 24;
        const uint32 red = ((sourceColor & 0xffu) * alpha) / 0xff;
        const uint32 green = (((sourceColor >> 8) & 0xffu) * alpha) / 0xff;
        const uint32 blue = (((sourceColor >> 16) & 0xffu) * alpha) / 0xff;
        return (alpha << 24) | ((blue & 0xffu) << 16) | ((green & 0xffu) << 8) | (red & 0xffu);
    }

    // 0x00CE47A0 (:91)
    uint32 addColor(uint32 a, uint32 b)
    {
        uint32 result = 0;
        for (uint32 shift = 0; shift < 32; shift += 8)
        {
            uint32 sum = ((a >> shift) & 0xffu) + ((b >> shift) & 0xffu);
            if (sum > 0xff)
            {
                sum = 0xff;
            }
            result |= sum << shift;
        }
        return result;
    }

    // 0x00CE4870 (:118)
    uint32 mulColor(uint32 lhs, uint32 rhs)
    {
        uint32 result = 0;
        for (uint32 shift = 0; shift < 32; shift += 8)
        {
            uint32 product = (((lhs >> shift) & 0xffu) * ((rhs >> shift) & 0xffu)) / 0xff;
            if (product > 0xff)
            {
                product = 0xff;
            }
            result |= product << shift;
        }
        return result;
    }
}
