// Original: library/modules/runtime/graphics/include/keen/graphics/texturedescription.hpp
//
// TextureDescription (size, format, type, levels, flags) and the texture enums; the per-platform flag enums are all
// compiled in. Placement of the enums inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    enum TextureType
    {
        TextureType_1d = 0,
        TextureType_2d = 1,
        TextureType_3d = 2,
        TextureType_Cube = 3,
        TextureType_Count = 4,
    };

    enum TextureAddressMode
    {
        TextureAddressMode_Wrap = 0,
        TextureAddressMode_Mirror = 1,
        TextureAddressMode_Clamp = 2,
        TextureAddressMode_MirrorOnce = 3,
        TextureAddressMode_Count = 4,
    };

    enum TextureAccessMode
    {
        TextureAccessMode_None = 0,
        TextureAccessMode_Read = 1,
        TextureAccessMode_Write = 2,
        TextureAccessMode_ReadWrite = 3,
        TextureAccessMode_Count = 4,
    };

    enum TilingMode
    {
        TilingMode_None = 0,
        TilingMode_TwoVertical = 1,
        TilingMode_FourVertical = 2,
        TilingMode_TwoHorizontal = 3,
        TilingMode_FourHorizontal = 4,
    };

    enum TextureFlag
    {
        TextureFlag_Bind_ShaderInput = 0,
        TextureFlag_Bind_RenderTarget = 1,
        TextureFlag_Bind_DepthStencil = 2,
        TextureFlag_Bind_ShadowMap = 3,
        TextureFlag_Bind_VertexTexture = 4,
        TextureFlag_Bind_ResolveRenderTarget = 5,
        TextureFlag_AlphaIsPremultiplied = 6,
        TextureFlag_Platform_Start = 7,
    };

    enum TextureFlagMask
    {
        TextureFlagMask_Bind_ShaderInput = 1,
        TextureFlagMask_Bind_RenderTarget = 2,
        TextureFlagMask_Bind_DepthStencil = 4,
        TextureFlagMask_Bind_ShadowMap = 8,
        TextureFlagMask_Bind_VertexTexture = 16,
        TextureFlagMask_Bind_ResolveRenderTarget = 32,
        TextureFlagMask_AlphaIsPremultiplied = 64,
    };

    enum TextureFlagD3D11
    {
        TextureFlagD3D11_AlphaIsPremultiplied = 7,
        TextureFlagD3D11_AliasableFormat = 8,
    };

    enum TextureFlagMaskD3D11
    {
        TextureFlagWinMaskD3D11_AlphaIsPremultiplied = 128,
        TextureFlagWinMaskD3D11_AliasableFormat = 256,
    };

    enum TextureFlagPS3
    {
        TextureFlagPS3_BindZCull = 7,
        TextureFlagPS3_BindTile = 8,
        TextureFlagPS3_Swizzled = 9,
    };

    enum TextureFlagXB360
    {
        TextureFlagXB360_Tiled = 7,
        TextureFlagXB360_Expand = 8,
        TextureFlagXB360_CPU_CACHED_MEMORY = 9,
    };

    enum TextureFlagWiiU
    {
        TextureFlagWiiU_Tiling = 7,
        TextureFlagWiiU_EnableHiZ = 11,
    };

    enum TextureFlagPs4
    {
        TextureFlagPs4_Tilemode = 7,
        TextureFlagPs4_Bind_Displayout = 12,
        TextureFlagPs4_EnableHTile = 13,
    };

    enum TextureFlagMaskPs4
    {
        TextureFlagMaskPs4_Tilemode = 3968,
        TextureFlagMaskPs4_Bind_Displayout = 4096,
        TextureFlagMaskPs4_EnableHTile = 8192,
    };

    enum TextureFlagXBone
    {
        TextureFlagXBone_Tilemode = 7,
        TextureFlagXBone_UseESRAM = 12,
    };

    enum TextureFlagMaskXBone
    {
        TextureFlagMaskXBone_Tilemode = 3968,
        TextureFlagMaskXBone_UseESRAM = 4096,
    };

    enum TextureFlagSwitch
    {
        TextureFlagSwitch_BindDisplay = 7,
        TextureFlagSwitch_Compressible = 8,
        TextureFlagSwitch_LinearTiling = 9,
    };

    // size 0x12
    struct TextureDescription
    {
        TextureDescription(); // 0x004B2040 (.h:196)

        uint16 width; // 0x00
        uint16 height; // 0x02
        uint16 depth; // 0x04
        uint16 flags; // 0x06
        uint8 type; // 0x08
        uint8 format; // 0x09
        uint8 lutFormat; // 0x0A
        uint8 multiSampleType; // 0x0B
        uint8 addressModeU; // 0x0C
        uint8 addressModeV; // 0x0D
        uint8 addressModeW; // 0x0E
        uint8 levelCount; // 0x0F
        uint8 cpuAccessMode; // 0x10
        uint8 gpuAccessMode; // 0x11
    };
}
