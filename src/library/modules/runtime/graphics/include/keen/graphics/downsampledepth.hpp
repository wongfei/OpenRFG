// Original: library/modules/runtime/graphics/include/keen/graphics/downsampledepth.hpp
// Implementation: library/modules/runtime/graphics/sources/global/downsampledepth.cpp
//
// Depth buffer downsampling (DownsampleDepthContext, created per graphics system and used by the command buffer).
// Placement of the context inferred; the shader permutations are generated (game/build/.../shader/*_sv.hpp).
#pragma once

#include "keen/base/types.hpp"
#include "game/build/win32/steam/master/shader/copydepth_sv.hpp"
#include "game/build/win32/steam/master/shader/downsampledepth_sv.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "library/modules/runtime/graphics/sources/global/stockobjects.hpp"

namespace keen
{
    struct BlendState;
    struct DepthStencilState;
    struct DynamicConstantBuffer;
    struct GraphicsCommandBuffer;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct RasterizerState;
    struct SamplerState;
    struct TextureData;
    struct VertexInputBinding;

    // size 0x68
    struct DownsampleDepthContext
    {
        DownsampleDepthVariants shaders; // 0x00
        CopyDepthVariants copyDepthShader; // 0x1C
        DynamicConstantBuffer* pConstantBuffer; // 0x2C
        const RasterizerState* pRasterizerState; // 0x30
        const SamplerState* pDepthBufferSamplerState; // 0x34
        const VertexInputBinding* pVertexInputBinding; // 0x38
        const VertexInputBinding* pCopyVertexInputBinding; // 0x3C
        const BlendState* pDepthBlendState; // 0x40
        const DepthStencilState* pDepthDepthStencilState; // 0x44
        const BlendState* pStencilBlendState; // 0x48
        const DepthStencilState* pStencilDepthStencilState; // 0x4C
        StockVertexPos2f vertexData[3]; // 0x50
    };

    namespace graphics
    {
        DownsampleDepthContext* createDownsampleDepthContext(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, ColorWriteMask stencilChannelMask); // 0x00C72FD0 (:36)
        void destroyDownsampleDepthContext(DownsampleDepthContext* pContext, MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem); // 0x00C704F0 (:71)
        void writeDownsampleDepthContextCommands(GraphicsCommandBuffer* pCommandBuffer, DownsampleDepthContext* pContext, const TextureData* pSourceData, bool copyStencilPass, uint32 renderTargetWidth, uint32 renderTargetHeight); // 0x00C63560 (:96)
        void writeCopyDepthContextCommands(GraphicsCommandBuffer* pCommandBuffer, DownsampleDepthContext* pContext, const TextureData* pSourceData); // 0x00C639F0 (:132)
    }
}
