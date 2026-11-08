// Original: library/modules/runtime/graphics/sources/global/downsampledepth.cpp
//
// DownsampleDepthContext: the shaders and states that copy a depth buffer into the current render target (as depth,
// or into the stencil channel) or downsample it, drawn as one screen-covering triangle from the immediate vertex
// buffer. The command buffer's writeDownsampleDepthBufferCommand/writeCopyDepthBufferCommand bind the target first.

#include "keen/graphics/downsampledepth.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/base/memoryallocator.hpp"

namespace keen
{
    namespace graphics
    {
        // 0x00C72FD0 (:36)
        DownsampleDepthContext* createDownsampleDepthContext(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, ColorWriteMask stencilChannelMask)
        {
            DownsampleDepthContext* pContext = (DownsampleDepthContext*)pAllocator->allocate(sizeof(DownsampleDepthContext), 4, 0, nullptr);
            pContext->shaders.create(pGraphicsSystem);
            pContext->copyDepthShader.create(pGraphicsSystem);
            // Magic number: one float4 of constants (the inverse source size).
            pContext->pConstantBuffer = createDynamicConstantBuffer(pGraphicsSystem, 0, 16);

            RasterizerParameters rasterizerParameters;
            rasterizerParameters.cullMode = CullMode_None;
            rasterizerParameters.fillMode = FillMode_Solid;
            rasterizerParameters.windingOrder = WindingOrder_Ccw;
            rasterizerParameters.flags = 0;
            pContext->pRasterizerState = createRasterizerState(pGraphicsSystem, rasterizerParameters);

            SamplerParameters samplerParameters;
            samplerParameters.minFilter = SamplerFilterMode_Nearest;
            samplerParameters.magFilter = SamplerFilterMode_Nearest;
            samplerParameters.mipFilter = SamplerFilterMode_Nearest;
            samplerParameters.addressU = SamplerAddressMode_Clamp;
            samplerParameters.addressV = SamplerAddressMode_Clamp;
            samplerParameters.addressW = SamplerAddressMode_Clamp;
            pContext->pDepthBufferSamplerState = createSamplerState(pGraphicsSystem, samplerParameters);

            // Magic number: 8 = GraphicsGeometryModeMask_Immediate.
            pContext->pVertexInputBinding = createVertexInputBinding(pGraphicsSystem, pGraphicsSystem->stockObjects.getFormat(GraphicsStockVertexFormat_Pos2f), pContext->shaders.getPipeline(0), GraphicsGeometryModeMask_Immediate);
            pContext->pCopyVertexInputBinding = createVertexInputBinding(pGraphicsSystem, pGraphicsSystem->stockObjects.getFormat(GraphicsStockVertexFormat_Pos2f_Tex2f), pContext->copyDepthShader.getPipeline(), GraphicsGeometryModeMask_Immediate);

            // Depth passes write no color; the stencil pass writes the given channels.
            BlendParametersEx blendParameters;
            blendParameters.blendOpColor = BlendOperation_None;
            blendParameters.sourceFactorColor = BlendFactor_One;
            blendParameters.destFactorColor = BlendFactor_Zero;
            blendParameters.blendOpAlpha = BlendOperation_None;
            blendParameters.sourceFactorAlpha = BlendFactor_One;
            blendParameters.destFactorAlpha = BlendFactor_Zero;
            blendParameters.colorWriteMask[0] = 0;
            pContext->pDepthBlendState = createBlendState(pGraphicsSystem, blendParameters);
            pContext->pDepthDepthStencilState = createDepthStencilState(pGraphicsSystem, true, ComparisonFunction_Always, true);
            blendParameters.colorWriteMask[0] = (uint8)stencilChannelMask;
            pContext->pStencilBlendState = createBlendState(pGraphicsSystem, blendParameters);
            pContext->pStencilDepthStencilState = createDepthStencilState(pGraphicsSystem, false, ComparisonFunction_Always, false);

            // One triangle that covers the screen.
            pContext->vertexData[0].position.x = -1.0f;
            pContext->vertexData[0].position.y = 1.0f;
            pContext->vertexData[1].position.x = -1.0f;
            pContext->vertexData[1].position.y = -3.0f;
            pContext->vertexData[2].position.x = 3.0f;
            pContext->vertexData[2].position.y = 1.0f;
            return pContext;
        }

        // 0x00C704F0 (:71)
        void destroyDownsampleDepthContext(DownsampleDepthContext* pContext, MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem)
        {
            pContext->shaders.destroy(pGraphicsSystem);
            pContext->copyDepthShader.destroy(pGraphicsSystem);
            destroySamplerState(pGraphicsSystem, pContext->pDepthBufferSamplerState);
            destroyRasterizerState(pGraphicsSystem, pContext->pRasterizerState);
            destroyVertexInputBinding(pGraphicsSystem, pContext->pVertexInputBinding);
            destroyVertexInputBinding(pGraphicsSystem, pContext->pCopyVertexInputBinding);
            destroyBlendState(pGraphicsSystem, pContext->pDepthBlendState);
            destroyDepthStencilState(pGraphicsSystem, pContext->pDepthDepthStencilState);
            destroyBlendState(pGraphicsSystem, pContext->pStencilBlendState);
            destroyDepthStencilState(pGraphicsSystem, pContext->pStencilDepthStencilState);
            destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pConstantBuffer);
            pAllocator->free(pContext);
        }

        static void bindDepthSource(GraphicsCommandBuffer* pCommandBuffer, DownsampleDepthContext* pContext, const TextureData* pSourceData)
        {
            writeSetFragmentShaderTextureCommand(pCommandBuffer, 0, pSourceData);
            writeSetFragmentShaderSamplerStateCommand(pCommandBuffer, 0, pContext->pDepthBufferSamplerState);
        }

        // 0x00C63560 (:96)
        void writeDownsampleDepthContextCommands(GraphicsCommandBuffer* pCommandBuffer, DownsampleDepthContext* pContext, const TextureData* pSourceData, bool copyStencilPass, uint32 renderTargetWidth, uint32 renderTargetHeight)
        {
            writeSetRasterizerStateCommand(pCommandBuffer, pContext->pRasterizerState);
            if (copyStencilPass)
            {
                writeSetBlendStateCommand(pCommandBuffer, pContext->pStencilBlendState);
                writeSetDepthStencilStateCommand(pCommandBuffer, pContext->pStencilDepthStencilState, 0);
            }
            else
            {
                writeSetBlendStateCommand(pCommandBuffer, pContext->pDepthBlendState);
                writeSetDepthStencilStateCommand(pCommandBuffer, pContext->pDepthDepthStencilState, 0);
            }
            writeSetShaderPipelineCommand(pCommandBuffer, pContext->shaders.getPipeline(copyStencilPass ? 1 : 0), pContext->pVertexInputBinding, false);

            float* pConstants = (float*)beginDynamicConstantUpdate(pCommandBuffer, pContext->pConstantBuffer, ShaderMask_Fragment);
            // Constants: the size of one target texel in [0, 1] texture space.
            pConstants[0] = 1.0f / (float)renderTargetWidth;
            pConstants[1] = 1.0f / (float)renderTargetHeight;
            pConstants[2] = 0.0f;
            pConstants[3] = 0.0f;
            endDynamicConstantUpdate(pCommandBuffer, 0, ShaderMask_Fragment);

            bindDepthSource(pCommandBuffer, pContext, pSourceData);
            float2* pVertices = (float2*)beginImmediateGeometryCommand(pCommandBuffer, PrimitiveType_TriangleList, 3, sizeof(StockVertexPos2f));
            pVertices[0] = pContext->vertexData[0].position;
            pVertices[1] = pContext->vertexData[1].position;
            pVertices[2] = pContext->vertexData[2].position;
            endImmediateGeometryCommand(pCommandBuffer);
            // Unbind the source so that it can be rendered to again.
            writeSetFragmentShaderTextureCommand(pCommandBuffer, 0, nullptr);
        }

        // 0x00C639F0 (:132)
        void writeCopyDepthContextCommands(GraphicsCommandBuffer* pCommandBuffer, DownsampleDepthContext* pContext, const TextureData* pSourceData)
        {
            writeSetRasterizerStateCommand(pCommandBuffer, pContext->pRasterizerState);
            writeSetBlendStateCommand(pCommandBuffer, pContext->pDepthBlendState);
            writeSetDepthStencilStateCommand(pCommandBuffer, pContext->pDepthDepthStencilState, 0);
            writeSetShaderPipelineCommand(pCommandBuffer, pContext->copyDepthShader.getPipeline(), pContext->pCopyVertexInputBinding, false);
            bindDepthSource(pCommandBuffer, pContext, pSourceData);
            // Position (x, y) and texture coordinate (u, v) per vertex.
            float* pVertices = (float*)beginImmediateGeometryCommand(pCommandBuffer, PrimitiveType_TriangleList, 3, sizeof(StockVertexPos2fTex2f));
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
            endImmediateGeometryCommand(pCommandBuffer);
            writeSetFragmentShaderTextureCommand(pCommandBuffer, 0, nullptr);
        }
    }
}
