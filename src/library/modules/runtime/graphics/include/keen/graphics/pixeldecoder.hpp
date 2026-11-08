// Original: library/modules/runtime/graphics/include/keen/graphics/pixeldecoder.hpp
//
// Pixel decoders used to read back texture data (one per pixel format) and getNativeRGBA32Color. Placement of the
// decoder structs inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct LockedTextureRectangle;

    // size 0x1
    struct PixelDecoder_Color
    {
        typedef uint32 PixelType;

        static uint32* getPixelDataPointer(uint32, uint32, const LockedTextureRectangle&);
        static uint32 fromColor(uint32);
        static uint32 toColor(uint32);

    };

    // size 0x1
    struct PixelDecoder_A8R8G8B8
    {
        typedef uint32 PixelType;

        static uint32* getPixelDataPointer(uint32, uint32, const LockedTextureRectangle&);
        static uint32 fromColor(uint32);
        static uint32 toColor(uint32);

    };

    // size 0x1
    struct PixelDecoder_B8G8R8A8
    {
        typedef uint32 PixelType;

        static uint32* getPixelDataPointer(uint32, uint32, const LockedTextureRectangle&);
        static uint32 fromColor(uint32);
        static uint32 toColor(uint32);

    };

    // size 0x1
    struct PixelDecoder_R8G8B8A8
    {
        typedef uint32 PixelType;

        static uint32* getPixelDataPointer(uint32, uint32, const LockedTextureRectangle&);
        static uint32 fromColor(uint32);
        static uint32 toColor(uint32);

    };

    // size 0x1
    struct PixelDecoder_R32F
    {
        typedef float PixelType;

        static float* getPixelDataPointer(uint32, uint32, const LockedTextureRectangle&);
        static float fromColor(uint32);
        static uint32 toColor(float);

    };

    // size 0x1
    struct PixelDecoder_A16B16G16R16F
    {
        // size 0x8
        struct PixelData
        {
            uint16 r; // 0x00
            uint16 g; // 0x02
            uint16 b; // 0x04
            uint16 a; // 0x06
        };

        struct PixelType; // no definition in PDB

        static PixelData* getPixelDataPointer(uint32, uint32, const LockedTextureRectangle&);
        static PixelData fromColor(uint32);
        static uint32 toColor(PixelData);

    };

    uint32 getNativeRGBA32Color(uint8 r, uint8 g, uint8 b, uint8 a); // 0x00C5CEA0 (.h:185)
}
