// Original: volition/render_lib/code/gaussblur.cpp
//
// Partial (stub_impl.md S3): the context's creation and destruction. The scene renderer creates it, but nothing in
// rfg.exe renders the blur (renderGaussBlurEffect has no callers), so the two render functions are not ported.

#include "volition/render_lib/code/gaussblur.hpp"

#include "keen/base/memoryallocator.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "keen/graphics/graphicssystem.hpp"

#include <string.h>

// 0x004DBDA0 (:36)
// The gaussblur shader with point clamp sampling, no depth test, no blending, no culling, and 0xB0 bytes of fragment
// constants (the sampling offsets and weights).
GaussBlurContext* createGaussBlurContext(keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem)
{
    GaussBlurContext* context = (GaussBlurContext*)pAllocator->allocate(sizeof(GaussBlurContext), 4, 0, nullptr);
    context->gaussBlurVariants.create(pGraphicsSystem);
    context->pFragmentShaderConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0xB0);

    keen::SamplerParameters sampler;
    memset(&sampler, 0, sizeof(sampler));
    sampler.minLod = 0.0f;
    sampler.maxLod = 1000.0f; // magic number: no LOD clamp
    sampler.maxAnisotropyLevel = 0;
    sampler.comparisonFunction = keen::ComparisonFunction_Never;
    sampler.alphaKillEnabled = false;
    sampler.addressV = keen::SamplerAddressMode_Clamp;
    sampler.addressW = keen::SamplerAddressMode_Clamp;
    sampler.minFilter = keen::SamplerFilterMode_Nearest;
    sampler.magFilter = keen::SamplerFilterMode_Nearest;
    sampler.mipFilter = keen::SamplerFilterMode_Nearest;
    sampler.addressU = keen::SamplerAddressMode_Clamp;
    context->pSamplerState = keen::graphics::createSamplerState(pGraphicsSystem, sampler);

    context->pDepthStencilState = keen::graphics::createDepthStencilState(pGraphicsSystem, false, keen::ComparisonFunction_Always, false);

    keen::BlendParametersEx blend;
    blend.blendOpColor = keen::BlendOperation_None;
    blend.sourceFactorColor = keen::BlendFactor_One;
    blend.destFactorColor = keen::BlendFactor_Zero;
    blend.blendOpAlpha = keen::BlendOperation_None;
    blend.sourceFactorAlpha = keen::BlendFactor_One;
    blend.destFactorAlpha = keen::BlendFactor_Zero;
    memset(blend.colorWriteMask, 0x0F, sizeof(blend.colorWriteMask)); // every channel of every target
    context->pBlendState = keen::graphics::createBlendState(pGraphicsSystem, blend);

    keen::RasterizerParameters rasterizer;
    rasterizer.cullMode = keen::CullMode_None;
    rasterizer.fillMode = keen::FillMode_Solid;
    rasterizer.windingOrder = keen::WindingOrder_Cw;
    rasterizer.flags = 0;
    rasterizer.depthBias = 0.0f;
    rasterizer.depthBiasSlopeScale = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    context->pRasterizerState = keen::graphics::createRasterizerState(pGraphicsSystem, rasterizer);

    const keen::VertexFormat* format = keen::graphics::getStockVertexFormat(pGraphicsSystem, keen::GraphicsStockVertexFormat_Pos2f_Tex2f);
    // magic number: geometry mode mask 8
    context->pInputBinding = keen::graphics::createVertexInputBinding(pGraphicsSystem, format, context->gaussBlurVariants.getPipeline(), 8);
    return context;
}

// 0x004DBF30 (:60)
void destroyGaussBlurContext(GaussBlurContext* pContext, keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem)
{
    keen::graphics::destroyVertexInputBinding(pGraphicsSystem, pContext->pInputBinding);
    keen::graphics::destroyRasterizerState(pGraphicsSystem, pContext->pRasterizerState);
    keen::graphics::destroyBlendState(pGraphicsSystem, pContext->pBlendState);
    keen::graphics::destroyDepthStencilState(pGraphicsSystem, pContext->pDepthStencilState);
    keen::graphics::destroySamplerState(pGraphicsSystem, pContext->pSamplerState);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pFragmentShaderConstants);
    pContext->gaussBlurVariants.destroy(pGraphicsSystem);
    pAllocator->free(pContext);
}
