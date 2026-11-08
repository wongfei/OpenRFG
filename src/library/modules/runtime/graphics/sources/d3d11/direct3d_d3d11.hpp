// Original: library/modules/runtime/graphics/sources/d3d11/direct3d_d3d11.hpp
//
// The D3D11 state objects (BlendState, RasterizerState, DepthStencilState, SamplerState: a GraphicsStateObject plus the
// D3D11 state; VertexInputBinding: a VertexFormat bound to a shader input layout) and constant buffers. Placement
// inferred (the PDB records no inline code for them).
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include "keen/graphics/graphicsstateobjects.hpp"

struct ID3D11BlendState;
struct ID3D11Buffer;
struct ID3D11DepthStencilState;
struct ID3D11InputLayout;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;

namespace keen
{
    struct VertexFormat;

    // size 0xC
    struct BlendState : public GraphicsStateObject
    {
        ID3D11BlendState* pState; // 0x08
    };

    // size 0xC
    struct RasterizerState : public GraphicsStateObject
    {
        ID3D11RasterizerState* pState; // 0x08
    };

    // size 0xC
    struct DepthStencilState : public GraphicsStateObject
    {
        ID3D11DepthStencilState* pState; // 0x08
    };

    // size 0xC
    struct SamplerState : public GraphicsStateObject
    {
        ID3D11SamplerState* pState; // 0x08
    };

    // size 0x14
    struct VertexInputBinding : public GraphicsStateObject
    {
        const VertexFormat* pVertexFormat; // 0x08
        ID3D11InputLayout* pLayout; // 0x0C
        uint32 geometryModeMask; // 0x10
    };

    // size 0x8
    struct DynamicConstantBuffer
    {
        ID3D11Buffer* pBuffer; // 0x00
        uint32 sizeInBytes; // 0x04
    };

    // size 0x8
    struct StaticConstantBuffer
    {
        ID3D11Buffer* pBuffer; // 0x00
        uint32 sizeInBytes; // 0x04
    };
}
