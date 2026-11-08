// Original: library/modules/runtime/graphics/include/keen/graphics/postprocessing_fxaa.hpp
// Implementation: library/modules/runtime/graphics/sources/global/postprocessingfxaa.cpp
//
// FXAA post-processing (FxaaContext). Placement of FxaaContext inferred.
#pragma once

#include "keen/base/types.hpp"
#include "game/build/win32/steam/master/shader/fxaashader_sv.hpp"

namespace keen
{
    struct BlendState;
    struct DepthStencilState;
    struct DynamicConstantBuffer;
    class GraphicsCommandWriter;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct RasterizerState;
    struct RenderTarget;
    struct SamplerState;
    struct TextureData;
    struct VertexInputBinding;

    // size 0x34
    struct FxaaContext
    {
        const BlendState* pBlendState; // 0x00
        const RasterizerState* pRasterizerState; // 0x04
        const DepthStencilState* pDepthStencilState; // 0x08
        const SamplerState* pSamplerState; // 0x0C
        const VertexInputBinding* pVertexInputBinding; // 0x10
        FxaaShaderVariants shaderVariants; // 0x14
        DynamicConstantBuffer* pConstantBuffer; // 0x30
    };

    FxaaContext* createFxaaContext(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem); // 0x00C73910 (postprocessingfxaa.cpp:23)
    void destroyFxaaContext(FxaaContext* pFxaaContext, MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem); // 0x00C70D10 (postprocessingfxaa.cpp:49)
    void renderFxaaEffect(FxaaContext* pFxaaContext, GraphicsCommandWriter* pCommandWriter, const TextureData* pInputBuffer, const RenderTarget* pOutputTarget, float inverseGamma); // 0x00C65640 (postprocessingfxaa.cpp:68)
}
