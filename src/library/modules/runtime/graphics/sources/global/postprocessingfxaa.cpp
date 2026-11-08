// Original: library/modules/runtime/graphics/sources/global/postprocessingfxaa.cpp
//
// FxaaContext: the FXAA pass that rl_scene_renderer::render_scene_part2 runs last in its post effect chain (g_fxaaEnabled
// is always true in rfg.exe), so it is the only writer of the back buffer. One screen-covering triangle samples the
// input texture into the output target with the fxaa shader (keenShaderCorrectGamma variant when inverseGamma > 0).
// rfg.exe inlines all the command writer calls of renderFxaaEffect.

#include "keen/graphics/postprocessing_fxaa.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/graphics/rendertarget.hpp"
#include "keen/base/memoryallocator.hpp"

#include <string.h>

namespace keen
{
    // 0x00C73910 (:23)
    FxaaContext* createFxaaContext(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem)
    {
        FxaaContext* pContext = (FxaaContext*)pAllocator->allocate(sizeof(FxaaContext), 4, 0, nullptr);
        pContext->shaderVariants.create(pGraphicsSystem);

        // Opaque copy into all channels of every target.
        BlendParametersEx blendParameters;
        blendParameters.blendOpColor = BlendOperation_None;
        blendParameters.sourceFactorColor = BlendFactor_One;
        blendParameters.destFactorColor = BlendFactor_Zero;
        blendParameters.blendOpAlpha = BlendOperation_None;
        blendParameters.sourceFactorAlpha = BlendFactor_One;
        blendParameters.destFactorAlpha = BlendFactor_Zero;
        for (uint32 i = 0; i < 8; ++i)
        {
            // Magic number: 0xF = RGBA.
            blendParameters.colorWriteMask[i] = 0xF;
        }
        pContext->pBlendState = graphics::createBlendState(pGraphicsSystem, blendParameters);
        // Magic number: three float4 of constants (see renderFxaaEffect).
        pContext->pConstantBuffer = graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x30);
        pContext->pDepthStencilState = graphics::createDepthStencilState(pGraphicsSystem, false, ComparisonFunction_Always, false);

        RasterizerParameters rasterizerParameters;
        rasterizerParameters.cullMode = CullMode_Back;
        rasterizerParameters.fillMode = FillMode_Solid;
        rasterizerParameters.windingOrder = WindingOrder_Ccw;
        rasterizerParameters.flags = RasterizerFlagMask_CullAgainstFrustum;
        rasterizerParameters.depthBias = 0.0f;
        rasterizerParameters.depthBiasSlopeScale = 0.0f;
        rasterizerParameters.depthBiasClamp = 0.0f;
        pContext->pRasterizerState = graphics::createRasterizerState(pGraphicsSystem, rasterizerParameters);

        SamplerParameters samplerParameters;
        memset(&samplerParameters, 0, sizeof(samplerParameters));
        // Original bug kept: the sampler parameters reuse the rasterizer parameters' stack slot and the memset starts
        // 4 bytes in, so mipLodBias keeps the bits of cullMode (2, a denormal float, effectively 0).
        const uint32 staleCullModeBits = (uint32)CullMode_Back;
        memcpy(&samplerParameters.mipLodBias, &staleCullModeBits, sizeof(staleCullModeBits));
        samplerParameters.minLod = 0.0f;
        // Magic number: max LOD 1000.
        samplerParameters.maxLod = 1000.0f;
        samplerParameters.maxAnisotropyLevel = 0;
        samplerParameters.minFilter = SamplerFilterMode_Linear;
        samplerParameters.magFilter = SamplerFilterMode_Linear;
        samplerParameters.mipFilter = SamplerFilterMode_Nearest;
        samplerParameters.addressU = SamplerAddressMode_Wrap;
        samplerParameters.addressV = SamplerAddressMode_Wrap;
        samplerParameters.addressW = SamplerAddressMode_Wrap;
        samplerParameters.comparisonFunction = ComparisonFunction_Never;
        samplerParameters.alphaKillEnabled = false;
        pContext->pSamplerState = graphics::createSamplerState(pGraphicsSystem, samplerParameters);

        pContext->pVertexInputBinding = graphics::createVertexInputBinding(pGraphicsSystem, pGraphicsSystem->stockObjects.getFormat(GraphicsStockVertexFormat_Pos2f_Tex2f), pContext->shaderVariants.getPipeline(0), GraphicsGeometryModeMask_Immediate);
        return pContext;
    }

    // 0x00C70D10 (:49)
    void destroyFxaaContext(FxaaContext* pFxaaContext, MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem)
    {
        graphics::destroyBlendState(pGraphicsSystem, pFxaaContext->pBlendState);
        graphics::destroyDepthStencilState(pGraphicsSystem, pFxaaContext->pDepthStencilState);
        graphics::destroyRasterizerState(pGraphicsSystem, pFxaaContext->pRasterizerState);
        graphics::destroySamplerState(pGraphicsSystem, pFxaaContext->pSamplerState);
        graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pFxaaContext->pConstantBuffer);
        graphics::destroyVertexInputBinding(pGraphicsSystem, pFxaaContext->pVertexInputBinding);
        pFxaaContext->shaderVariants.destroy(pGraphicsSystem);
        pAllocator->free(pFxaaContext);
    }

    // 0x00C65640 (:68)
    // Its own render pass on the output target (viewport and scissor of the whole target, no clear, no resolve).
    // Fragment constants: the FXAA "rcpFrameOpt" values 2 / size and 0.5 / size, 1 / size, then inverseGamma.
    void renderFxaaEffect(FxaaContext* pFxaaContext, GraphicsCommandWriter* pCommandWriter, const TextureData* pInputBuffer, const RenderTarget* pOutputTarget, float inverseGamma)
    {
        pCommandWriter->writeBeginRenderPassCommand(pOutputTarget, RenderTargetPartFlagMask_None, nullptr);
        const float width = (float)(pOutputTarget != nullptr ? pOutputTarget->width : 0u);
        const float height = (float)(pOutputTarget != nullptr ? pOutputTarget->height : 0u);

        float* pConstants = (float*)pCommandWriter->beginDynamicFragmentConstants(pFxaaContext->pConstantBuffer, 0);
        // Magic numbers: the FXAA 3 console constants (2, 0.5 and 1 texel steps).
        pConstants[0] = 2.0f / width;
        pConstants[1] = 2.0f / height;
        pConstants[2] = 0.5f / width;
        pConstants[3] = 0.5f / height;
        pConstants[4] = 1.0f / width;
        pConstants[5] = 1.0f / height;
        pConstants[6] = 0.0f;
        pConstants[7] = 0.0f;
        pConstants[8] = inverseGamma;
        pConstants[9] = 0.0f;
        pConstants[10] = 0.0f;
        pConstants[11] = 0.0f;
        pCommandWriter->endDynamicFragmentConstants();

        const ShaderPipeline* pPipeline = pFxaaContext->shaderVariants.getPipeline(inverseGamma > 0.0f ? 1 : 0);
        pCommandWriter->writeSetShaderPipelineCommand(pPipeline, pFxaaContext->pVertexInputBinding, false);
        pCommandWriter->writeSetBlendStateCommand(pFxaaContext->pBlendState);
        pCommandWriter->writeSetDepthStencilStateCommand(pFxaaContext->pDepthStencilState, 0);
        pCommandWriter->writeSetRasterizerStateCommand(pFxaaContext->pRasterizerState);
        pCommandWriter->writeSetFragmentShaderTextureCommand(0, pInputBuffer, pFxaaContext->pSamplerState);

        // One triangle that covers the screen: position (x, y) and texture coordinate (u, v) per vertex.
        float* pVertices = (float*)pCommandWriter->beginImmediateGeometryCommand(PrimitiveType_TriangleList, 3, sizeof(StockVertexPos2fTex2f));
        if (pVertices != nullptr)
        {
            const float vertices[12] =
            {
                -1.0f, 1.0f, 0.0f, 0.0f,
                -1.0f, -3.0f, 0.0f, 2.0f,
                3.0f, 1.0f, 2.0f, 0.0f,
            };
            for (uint32 i = 0; i < 12; ++i)
            {
                pVertices[i] = vertices[i];
            }
            pCommandWriter->endImmediateGeometryCommand();
        }
        pCommandWriter->writeEndRenderPassCommand(RenderTargetPartFlagMask_None);
    }
}
