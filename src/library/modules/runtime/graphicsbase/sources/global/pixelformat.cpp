// Original: library/modules/runtime/graphicsbase/sources/global/pixelformat.cpp
//
// Pixel format names, sizes and gamma flags. The table is as shipped, including its inconsistencies (kept as is):
// Bc2_Gamma has its bitsPer4x4Block value in bitsPerPixel, Bc5 has no block size, entry 55 (Depth32F) is named
// "PixelFormat_Depth32" with no size, and Depth32FStencil8/Depth16Stencil8 carry 5 and 3 bits per pixel.

#include "keen/graphics/pixelformat.hpp"

namespace keen
{
    static const PixelFormatInfo s_pixelFormatInfos[PixelFormat_Count] =
    {
        { "PixelFormat_None", 0, 0, false },
        { "PixelFormat_Bc1", 0, 64, false },
        { "PixelFormat_Bc1_Gamma", 0, 64, true },
        { "PixelFormat_Bc2", 0, 128, false },
        { "PixelFormat_Bc2_Gamma", 128, 0, true },
        { "PixelFormat_Bc3", 0, 128, false },
        { "PixelFormat_Bc3_Gamma", 0, 128, true },
        { "PixelFormat_Bc4", 0, 64, false },
        { "PixelFormat_Bc4_Gamma", 0, 64, true },
        { "PixelFormat_Bc5", 0, 0, false },
        { "PixelFormat_Bc5_Gamma", 0, 128, true },
        { "PixelFormat_Bc6H_SF16", 0, 128, false },
        { "PixelFormat_Bc6H_UF16", 0, 128, false },
        { "PixelFormat_Bc7", 0, 128, false },
        { "PixelFormat_Bc7_Gamma", 0, 128, true },
        { "PixelFormat_Index4", 4, 0, false },
        { "PixelFormat_A4", 4, 0, false },
        { "PixelFormat_L4", 4, 0, false },
        { "PixelFormat_A8", 8, 0, false },
        { "PixelFormat_L8", 8, 0, false },
        { "PixelFormat_Index8", 8, 0, false },
        { "PixelFormat_L4A4", 8, 0, false },
        { "PixelFormat_G8R8", 16, 0, false },
        { "PixelFormat_R8G8", 16, 0, false },
        { "PixelFormat_R5G6B5", 16, 0, false },
        { "PixelFormat_R5G5B5A1", 16, 0, false },
        { "PixelFormat_R4G4B4A4", 16, 0, false },
        { "PixelFormat_R5G5B5A3", 16, 0, false },
        { "PixelFormat_Index14x2", 0, 0, false },
        { "PixelFormat_R8G8B8", 24, 0, false },
        { "PixelFormat_R8G8B8_Gamma", 24, 0, true },
        { "PixelFormat_R32F", 32, 0, false },
        { "PixelFormat_Y16X16", 32, 0, false },
        { "PixelFormat_A8R8G8B8", 32, 0, false },
        { "PixelFormat_A8R8G8B8_Gamma", 32, 0, true },
        { "PixelFormat_X8R8G8B8", 32, 0, false },
        { "PixelFormat_X8R8G8B8_Gamma", 32, 0, true },
        { "PixelFormat_A8B8G8R8", 32, 0, false },
        { "PixelFormat_A8B8G8R8_Gamma", 32, 0, true },
        { "PixelFormat_R8G8B8A8", 32, 0, false },
        { "PixelFormat_R8G8B8A8_Gamma", 32, 0, true },
        { "PixelFormat_A2R10G10B10F", 64, 0, false },
        { "PixelFormat_A16B16G16R16F", 64, 0, false },
        { "PixelFormat_A32B32G32R32F", 128, 0, false },
        { "PixelFormat_Depth8", 8, 0, false },
        { "PixelFormat_Depth16", 16, 0, false },
        { "PixelFormat_Depth24Stencil8", 32, 0, false },
        { "PixelFormat_ETC1_RGB8", 0, 0, false },
        { "PixelFormat_ETC1_RGB8A4", 0, 0, false },
        { "PixelFormat_Pvrtc2", 0, 0, false },
        { "PixelFormat_Pvrtc4", 0, 0, false },
        { "PixelFormat_Pvrtc2A", 0, 0, false },
        { "PixelFormat_Pvrtc4A", 0, 0, false },
        { "PixelFormat_Ctx1", 0, 0, false },
        { "PixelFormat_DxN", 0, 0, false },
        { "PixelFormat_Depth32", 0, 0, false },
        { "PixelFormat_Depth24FStencil8", 32, 0, false },
        { "PixelFormat_R11G11B10F", 32, 0, false },
        { "PixelFormat_Depth32FStencil8", 5, 0, false },
        { "PixelFormat_Depth16Stencil8", 3, 0, false },
    };

    static PixelFormatInfo s_invalidPixelFormatInfo = { "PixelFormat_Invalid", 0, 0, false };

    namespace graphics
    {
        // 0x00CE3920 (:78)
        const PixelFormatInfo& getPixelFormatInfo(uint32 format)
        {
            if (format == PixelFormat_Invalid)
            {
                return s_invalidPixelFormatInfo;
            }
            return s_pixelFormatInfos[format];
        }

        // 0x00CE3940 (:89)
        uint32 getGammaFormat(uint32 format)
        {
            switch (format)
            {
            case PixelFormat_Bc1: return PixelFormat_Bc1_Gamma;
            case PixelFormat_Bc2: return PixelFormat_Bc2_Gamma;
            case PixelFormat_Bc3: return PixelFormat_Bc3_Gamma;
            case PixelFormat_Bc4: return PixelFormat_Bc4_Gamma;
            case PixelFormat_Bc5: return PixelFormat_Bc5_Gamma;
            case PixelFormat_Bc7: return PixelFormat_Bc7_Gamma;
            case PixelFormat_A8R8G8B8: return PixelFormat_A8R8G8B8_Gamma;
            case PixelFormat_X8R8G8B8: return PixelFormat_X8R8G8B8_Gamma;
            case PixelFormat_A8B8G8R8: return PixelFormat_A8B8G8R8_Gamma;
            case PixelFormat_R8G8B8A8: return PixelFormat_R8G8B8A8_Gamma;
            default: return format;
            }
        }

        // 0x00CE54D0 (:128)
        uint32 getNonGammaFormat(uint32 format)
        {
            if (!getPixelFormatInfo(format).isInGammaSpace)
            {
                return format;
            }
            switch (format)
            {
            case PixelFormat_Bc1_Gamma: return PixelFormat_Bc1;
            case PixelFormat_Bc2_Gamma: return PixelFormat_Bc2;
            case PixelFormat_Bc3_Gamma: return PixelFormat_Bc3;
            case PixelFormat_Bc4_Gamma: return PixelFormat_Bc4;
            case PixelFormat_Bc5_Gamma: return PixelFormat_Bc5;
            case PixelFormat_Bc7_Gamma: return PixelFormat_Bc7;
            case PixelFormat_A8R8G8B8_Gamma: return PixelFormat_A8R8G8B8;
            case PixelFormat_X8R8G8B8_Gamma: return PixelFormat_X8R8G8B8;
            case PixelFormat_A8B8G8R8_Gamma: return PixelFormat_A8B8G8R8;
            case PixelFormat_R8G8B8A8_Gamma: return PixelFormat_R8G8B8A8;
            default:
                // Notes (decompiled behaviour, kept as is): gamma formats without a linear twin (R8G8B8_Gamma) map
                // to PixelFormat_None.
                return PixelFormat_None;
            }
        }

        // 0x00CE39F0 (:174)
        bool isGammaPixelFormat(uint32 format)
        {
            if (format == PixelFormat_Invalid)
            {
                return false;
            }
            return s_pixelFormatInfos[format].isInGammaSpace;
        }

        // 0x00CE3A10 (:179)
        const char* getPixelFormatName(uint32 format)
        {
            if (format == PixelFormat_Invalid)
            {
                return "PixelFormat_Invalid";
            }
            return s_pixelFormatInfos[format].pName;
        }

        // 0x00CE3A30 (:184)
        uint32 getBitsPerPixel(uint32 format)
        {
            if (format == PixelFormat_Invalid)
            {
                return 0;
            }
            return s_pixelFormatInfos[format].bitsPerPixel;
        }
    }
}
