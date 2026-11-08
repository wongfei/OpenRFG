// Original: library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.cpp
//
// RenderTarget (up to 8 color views plus a depth/stencil view, with the textures behind them) and RenderTargetBuffer.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include "keen/graphics/pixelformat.hpp"

struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;

namespace keen
{
    struct TextureData;

    // size 0x8
    struct RenderTargetBuffer
    {
        RenderTargetBuffer(PixelFormat pixelFormat, TextureData* pTextureData); // 0x004B20E0 (rendertarget.hpp:24)
        RenderTargetBuffer(); // not in the PDB (inlined into RenderTarget::RenderTarget)

        PixelFormat format; // 0x00
        TextureData* pDataBuffer; // 0x04
    };

    // size 0x78
    struct RenderTarget
    {
        RenderTarget(); // 0x00C615C0

        ID3D11RenderTargetView* renderTargetViews[8]; // 0x00
        ID3D11DepthStencilView* pDepthBufferView; // 0x20
        RenderTargetBuffer colorBuffers[8]; // 0x24
        RenderTargetBuffer depthBuffer; // 0x64
        uint32 colorBufferCount; // 0x6C
        uint32 width; // 0x70
        uint32 height; // 0x74
    };
}
