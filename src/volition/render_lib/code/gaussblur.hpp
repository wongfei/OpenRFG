// Original: volition/render_lib/code/gaussblur.hpp
// Implementation: volition/render_lib/code/gaussblur.cpp
//
// Separable Gaussian blur on keen graphics (GaussBlurContext). Placement of the context inferred.
#pragma once

#include "game/build/win32/steam/master/shader/gaussblur_sv.hpp"

class rl_sampling_offsets;

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
    struct float4;
}

// size 0x28
struct GaussBlurContext
{
    keen::GaussBlurVariants gaussBlurVariants; // 0x00
    keen::DynamicConstantBuffer* pFragmentShaderConstants; // 0x10
    const keen::VertexInputBinding* pInputBinding; // 0x14
    const keen::SamplerState* pSamplerState; // 0x18
    const keen::DepthStencilState* pDepthStencilState; // 0x1C
    const keen::BlendState* pBlendState; // 0x20
    const keen::RasterizerState* pRasterizerState; // 0x24
};

GaussBlurContext* createGaussBlurContext(keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem); // 0x004DBDA0 (:36)
void destroyGaussBlurContext(GaussBlurContext* pContext, keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem); // 0x004DBF30 (:60)
void renderSeperatedGaussBlurEffect(GaussBlurContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, const rl_sampling_offsets& samplingOffsetsAndWeights, const keen::float4& axis, const keen::TextureData* pInputTexture, const keen::RenderTarget* pOutputTarget); // 0x004DBF90 (:75)
void renderGaussBlurEffect(GaussBlurContext* pContext, keen::GraphicsCommandWriter* pCommandWriter, const rl_sampling_offsets& samplingOffsetsAndWeights, const keen::TextureData* pInputTexture, const keen::RenderTarget* pOutputTarget, const keen::RenderTarget* pIntermediateTarget); // 0x004DC770 (:101)
