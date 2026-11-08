// Original: library/modules/runtime/graphicsbase/include/keen/graphics/pixelformat.hpp
// Implementation: library/modules/runtime/graphicsbase/sources/global/pixelformat.cpp
//
// PixelFormat (stored as uint32 in texture descriptions) and the per-format info table (bits per pixel, gamma
// variants, names). Placement of PixelFormatInfo inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum PixelFormat
    {
        PixelFormat_None = 0,
        PixelFormat_Bc1 = 1,
        PixelFormat_Bc1_Gamma = 2,
        PixelFormat_Bc2 = 3,
        PixelFormat_Bc2_Gamma = 4,
        PixelFormat_Bc3 = 5,
        PixelFormat_Bc3_Gamma = 6,
        PixelFormat_Bc4 = 7,
        PixelFormat_Bc4_Gamma = 8,
        PixelFormat_Bc5 = 9,
        PixelFormat_Bc5_Gamma = 10,
        PixelFormat_Bc6H_SF16 = 11,
        PixelFormat_Bc6H_UF16 = 12,
        PixelFormat_Bc7 = 13,
        PixelFormat_Bc7_Gamma = 14,
        PixelFormat_Index4 = 15,
        PixelFormat_A4 = 16,
        PixelFormat_L4 = 17,
        PixelFormat_A8 = 18,
        PixelFormat_L8 = 19,
        PixelFormat_Index8 = 20,
        PixelFormat_L4A4 = 21,
        PixelFormat_G8R8 = 22,
        PixelFormat_R8G8 = 23,
        PixelFormat_R5G6B5 = 24,
        PixelFormat_R5G5B5A1 = 25,
        PixelFormat_R4G4B4A4 = 26,
        PixelFormat_R5G5B5A3 = 27,
        PixelFormat_Index14x2 = 28,
        PixelFormat_R8G8B8 = 29,
        PixelFormat_R8G8B8_Gamma = 30,
        PixelFormat_R32F = 31,
        PixelFormat_Y16X16 = 32,
        PixelFormat_A8R8G8B8 = 33,
        PixelFormat_A8R8G8B8_Gamma = 34,
        PixelFormat_X8R8G8B8 = 35,
        PixelFormat_X8R8G8B8_Gamma = 36,
        PixelFormat_A8B8G8R8 = 37,
        PixelFormat_A8B8G8R8_Gamma = 38,
        PixelFormat_R8G8B8A8 = 39,
        PixelFormat_R8G8B8A8_Gamma = 40,
        PixelFormat_A2R10G10B10F = 41,
        PixelFormat_A16B16G16R16F = 42,
        PixelFormat_A32B32G32R32F = 43,
        PixelFormat_Depth8 = 44,
        PixelFormat_Depth16 = 45,
        PixelFormat_Depth24Stencil8 = 46,
        PixelFormat_ETC1_RGB8 = 47,
        PixelFormat_ETC1_RGB8A4 = 48,
        PixelFormat_Pvrtc2 = 49,
        PixelFormat_Pvrtc4 = 50,
        PixelFormat_Pvrtc2A = 51,
        PixelFormat_Pvrtc4A = 52,
        PixelFormat_Ctx1 = 53,
        PixelFormat_DxN = 54,
        PixelFormat_Depth32F = 55,
        PixelFormat_Depth24FStencil8 = 56,
        PixelFormat_R11G11B10F = 57,
        PixelFormat_Depth32FStencil8 = 58,
        PixelFormat_Depth16Stencil8 = 59,
        PixelFormat_Count = 60,
        PixelFormat_Invalid = 60,
        PixelFormat_Native_RGBA32 = 39,
        PixelFormat_Native_RGBA32_Gamma = 40,
        PixelFormat_Default_Depth = 46,
        PixelFormat_Dxt1 = 1,
        PixelFormat_Dxt1_Gamma = 2,
        PixelFormat_Dxt23 = 3,
        PixelFormat_Dxt23_Gamma = 4,
        PixelFormat_Dxt45 = 5,
        PixelFormat_Dxt45_Gamma = 6,
    };

    // size 0x10
    struct PixelFormatInfo
    {
        const char* pName; // 0x00
        uint32 bitsPerPixel; // 0x04
        uint32 bitsPer4x4Block; // 0x08
        bool isInGammaSpace; // 0x0C
    };

    namespace graphics
    {
        const PixelFormatInfo& getPixelFormatInfo(uint32 format); // 0x00CE3920 (:78)
        uint32 getGammaFormat(uint32 format); // 0x00CE3940 (:89)
        uint32 getNonGammaFormat(uint32 format); // 0x00CE54D0 (:128)
        bool isGammaPixelFormat(uint32 format); // 0x00CE39F0 (:174)
        const char* getPixelFormatName(uint32 format); // 0x00CE3A10 (:179)
        uint32 getBitsPerPixel(uint32 format); // 0x00CE3A30 (:184)
    }
}
