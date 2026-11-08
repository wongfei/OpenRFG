// Original: library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.cpp
//
// The D3D11 command functions. On D3D11 a GraphicsCommandBuffer executes every command at once on its device context
// (the immediate context, or a deferred context for a recorded buffer). Most of them are inline in
// graphicscommandbuffer_d3d11.hpp in the original (marked .h:N); the map/unmap and depth copy functions are here.
//
// Immediate geometry is written into the context's dynamic vertex buffer (no-overwrite maps, discarded when full).
// Quad lists are first written to quadBuffer and expanded into triangle lists when the command ends.

#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/direct3d_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/fragmentshader_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/geometryinstancing_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/occlusionquery_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/skinning_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/global/graphicscommandbuffer.hpp"
#include "keen/graphics/downsampledepth.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/graphics/vertexformat.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

namespace keen
{
    // 0x01341088
    static const D3D11_MAP s_d3dMapType[GraphicsMapType_Count] =
    {
        D3D11_MAP_READ, D3D11_MAP_WRITE, D3D11_MAP_READ_WRITE, D3D11_MAP_WRITE_DISCARD, D3D11_MAP_WRITE_NO_OVERWRITE,
    };

    static UINT getD3dMapFlags(uint32 mapFlags)
    {
        return (mapFlags & GraphicsMapFlagMask_NonBlocking) == GraphicsMapFlagMask_NonBlocking ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0;
    }

    static D3D11_CLEAR_FLAG getD3dDepthClearFlags(uint32 clearMask)
    {
        uint32 flags = (clearMask & 4) != 0 ? D3D11_CLEAR_DEPTH : 0;
        if ((clearMask & 8) != 0)
        {
            flags |= D3D11_CLEAR_STENCIL;
        }
        return (D3D11_CLEAR_FLAG)flags;
    }

    // 0x004B2590, 0x00C0E170, 0x00C5B5D0 (.h:122, three inlined copies): binds a render target (or none) with a full
    // viewport and scissor rectangle.
    static void writeSetRenderTargetCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderTarget* pRenderTarget)
    {
        ID3D11DeviceContext* pContext = pCommandBuffer->pContext;
        if (pRenderTarget == nullptr)
        {
            pContext->OMSetRenderTargets(0, nullptr, nullptr);
            pCommandBuffer->pCurrentRenderTarget = nullptr;
            return;
        }
        pContext->OMSetRenderTargets(pRenderTarget->colorBufferCount, pRenderTarget->renderTargetViews, pRenderTarget->pDepthBufferView);
        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = (float)pRenderTarget->width;
        viewport.Height = (float)pRenderTarget->height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        pContext->RSSetViewports(1, &viewport);
        D3D11_RECT scissor;
        scissor.left = 0;
        scissor.top = 0;
        scissor.right = pRenderTarget->width;
        scissor.bottom = pRenderTarget->height;
        pContext->RSSetScissorRects(1, &scissor);
        pCommandBuffer->pCurrentRenderTarget = pRenderTarget;
    }

    // The quad list expansion of endImmediateGeometryCommand: quad (0, 1, 2, 3) becomes triangles (0, 1, 2) and
    // (0, 2, 3), walking from the last quad to the first.
    static void expandQuadList(GraphicsCommandBuffer* pCommandBuffer, uint32 stride)
    {
        const sint32 vertexCount = (sint32)pCommandBuffer->immediateVertexCount;
        const sint32 quadVertexCount = (vertexCount / 6) * 4;
        sint32 triangleVertex = vertexCount - 6;
        sint32 quadVertex = quadVertexCount - 4;
        for (; triangleVertex > 0; triangleVertex -= 6, quadVertex -= 4)
        {
            uint8* pTarget = pCommandBuffer->pCurrentImmediateBuffer + triangleVertex * stride;
            const uint8* pSource = pCommandBuffer->quadBuffer + quadVertex * stride;
            memcpy(pTarget + stride * 5, pSource + stride * 3, stride);
            memcpy(pTarget + stride * 4, pSource + stride * 2, stride);
            memcpy(pTarget + stride * 3, pSource, stride);
            memcpy(pTarget + stride * 2, pSource + stride * 2, stride);
            memcpy(pTarget + stride, pSource + stride, stride);
            memcpy(pTarget, pSource, stride);
        }
        if (quadVertexCount > 0)
        {
            uint8* pTarget = pCommandBuffer->pCurrentImmediateBuffer;
            const uint8* pSource = pCommandBuffer->quadBuffer;
            memcpy(pTarget + stride * 5, pSource + stride * 3, stride);
            memcpy(pTarget + stride * 4, pSource + stride * 2, stride);
            memcpy(pTarget + stride * 3, pSource, stride);
            memcpy(pTarget + stride * 2, pSource + stride * 2, stride);
            memcpy(pTarget + stride, pSource + stride, stride);
            memcpy(pTarget, pSource, stride);
        }
    }

    namespace graphics
    {
        // 0x004B2470 (.h:84)
        void writeClearCommand(GraphicsCommandBuffer* pCommandBuffer, const ClearSettings& clearSettings)
        {
            const D3D11_CLEAR_FLAG depthFlags = getD3dDepthClearFlags(clearSettings.clearMask);
            ID3D11DepthStencilView* pDepthView = pCommandBuffer->pCurrentRenderTarget->pDepthBufferView;
            if (pDepthView != nullptr && depthFlags != 0)
            {
                pCommandBuffer->pContext->ClearDepthStencilView(pDepthView, depthFlags, clearSettings.depth, clearSettings.stencilValue);
            }
            if ((clearSettings.clearMask & 2) != 0)
            {
                float color[4];
                color[0] = (float)((clearSettings.color) & 0xffu) / 255.0f;
                color[1] = (float)((clearSettings.color >> 8) & 0xffu) / 255.0f;
                color[2] = (float)((clearSettings.color >> 16) & 0xffu) / 255.0f;
                color[3] = (float)((clearSettings.color >> 24) & 0xffu) / 255.0f;
                for (uint32 i = 0; i < pCommandBuffer->pCurrentRenderTarget->colorBufferCount; ++i)
                {
                    pCommandBuffer->pContext->ClearRenderTargetView(pCommandBuffer->pCurrentRenderTarget->renderTargetViews[i], color);
                }
            }
        }

        // 0x004B2680 (.h:155)
        void writeBeginRenderPassCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderTarget* pRenderTarget, RenderTargetPartFlagMask restoreFlags, const ClearSettings* pClearSettings)
        {
            writeSetRenderTargetCommand(pCommandBuffer, pRenderTarget);
            pCommandBuffer->pCurrentRenderTarget = pRenderTarget;
            if (pClearSettings == nullptr)
            {
                return;
            }
            // Notes (decompiled behaviour, kept as is): pRenderTarget is not checked for null when clearing.
            const D3D11_CLEAR_FLAG depthFlags = getD3dDepthClearFlags(pClearSettings->clearMask);
            if (pRenderTarget->pDepthBufferView != nullptr && depthFlags != 0)
            {
                pCommandBuffer->pContext->ClearDepthStencilView(pRenderTarget->pDepthBufferView, depthFlags, pClearSettings->depth, pClearSettings->stencilValue);
            }
            if ((pClearSettings->clearMask & 2) != 0)
            {
                // The channels are converted in double precision here.
                float color[4];
                color[0] = (float)((double)((pClearSettings->color) & 0xffu) / 255.0);
                color[1] = (float)((double)((pClearSettings->color >> 8) & 0xffu) / 255.0);
                color[2] = (float)((double)((pClearSettings->color >> 16) & 0xffu) / 255.0);
                color[3] = (float)((double)((pClearSettings->color >> 24) & 0xffu) / 255.0);
                for (uint32 i = 0; i < pCommandBuffer->pCurrentRenderTarget->colorBufferCount; ++i)
                {
                    pCommandBuffer->pContext->ClearRenderTargetView(pCommandBuffer->pCurrentRenderTarget->renderTargetViews[i], color);
                }
            }
        }

        // 0x004B2860 (.h:171)
        void writeEndRenderPassCommand(GraphicsCommandBuffer* pCommandBuffer, RenderTargetPartFlagMask resolveFlags, const RenderTarget* pRenderTarget)
        {
            // pRenderTarget is the render target to go back to (resolves are not needed on D3D11).
            writeSetRenderTargetCommand(pCommandBuffer, pRenderTarget);
        }

        // 0x004B2960 (.h:178)
        void writeSetViewportSettingsCommand(GraphicsCommandBuffer* pCommandBuffer, const ViewportSettings* pSettings)
        {
            D3D11_VIEWPORT viewport;
            if (pSettings == nullptr)
            {
                viewport.TopLeftX = 0.0f;
                viewport.TopLeftY = 0.0f;
                viewport.Width = (float)getRenderTargetWidth(pCommandBuffer->pCurrentRenderTarget);
                viewport.Height = (float)getRenderTargetHeight(pCommandBuffer->pCurrentRenderTarget);
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
            }
            else
            {
                viewport.TopLeftX = (float)pSettings->x;
                viewport.TopLeftY = (float)pSettings->y;
                viewport.Width = (float)pSettings->width;
                viewport.Height = (float)pSettings->height;
                viewport.MinDepth = pSettings->minDepth;
                viewport.MaxDepth = pSettings->maxDepth;
            }
            pCommandBuffer->pContext->RSSetViewports(1, &viewport);
        }

        // 0x004B2A70 (.h:202)
        void writeSetScissorRectangleCommand(GraphicsCommandBuffer* pCommandBuffer, const ScissorRectangle* pRectangle)
        {
            D3D11_RECT rectangle;
            if (pRectangle == nullptr)
            {
                rectangle.left = 0;
                rectangle.top = 0;
                rectangle.right = getRenderTargetWidth(pCommandBuffer->pCurrentRenderTarget);
                rectangle.bottom = getRenderTargetHeight(pCommandBuffer->pCurrentRenderTarget);
            }
            else
            {
                rectangle.left = pRectangle->x;
                rectangle.top = pRectangle->y;
                rectangle.right = pRectangle->x + pRectangle->width;
                rectangle.bottom = pRectangle->y + pRectangle->height;
            }
            pCommandBuffer->pContext->RSSetScissorRects(1, &rectangle);
        }

        // 0x004B2B00 (.h:222)
        void writeSetBlendStateCommand(GraphicsCommandBuffer* pCommandBuffer, const BlendState* pState)
        {
            const float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            pCommandBuffer->pContext->OMSetBlendState(pState->pState, blendFactor, 0xffffffffu);
        }

        // 0x004B2B60 (.h:237)
        void writeSetRasterizerStateCommand(GraphicsCommandBuffer* pCommandBuffer, const RasterizerState* pState)
        {
            pCommandBuffer->pContext->RSSetState(pState->pState);
        }

        // 0x004B2B80 (.h:246)
        void writeSetDepthStencilStateCommand(GraphicsCommandBuffer* pCommandBuffer, const DepthStencilState* pState, uint32 stencilRefValue)
        {
            pCommandBuffer->pContext->OMSetDepthStencilState(pState->pState, stencilRefValue);
        }

        // 0x004B2BA0 (.h:255)
        void writeSetFragmentShaderTextureCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 stageIndex, const TextureData* pTexture)
        {
            ID3D11ShaderResourceView* pView = nullptr;
            pCommandBuffer->pContext->PSSetShaderResources(stageIndex, 1, pTexture != nullptr ? &pTexture->pShaderView : &pView);
        }

        // 0x004B2BE0 (.h:273)
        void writeSetFragmentShaderSamplerStateCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 samplerIndex, const SamplerState* pSampler)
        {
            ID3D11SamplerState* pState = nullptr;
            pCommandBuffer->pContext->PSSetSamplers(samplerIndex, 1, pSampler != nullptr ? &pSampler->pState : &pState);
        }

        // 0x00C5B6C0 (.h:289)
        void writeSetVertexShaderTextureCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 stageIndex, const TextureData* pTexture)
        {
            ID3D11ShaderResourceView* pView = nullptr;
            pCommandBuffer->pContext->VSSetShaderResources(stageIndex, 1, pTexture != nullptr ? &pTexture->pShaderView : &pView);
        }

        // 0x00C5B700 (.h:305)
        void writeSetVertexShaderSamplerStateCommand(GraphicsCommandBuffer* pCommandBuffer, uint32 samplerIndex, const SamplerState* pSampler)
        {
            ID3D11SamplerState* pState = nullptr;
            pCommandBuffer->pContext->VSSetSamplers(samplerIndex, 1, pSampler != nullptr ? &pSampler->pState : &pState);
        }

        // 0x004B2C20 (.h:321)
        void writeSetShaderPipelineCommand(GraphicsCommandBuffer* pCommandBuffer, const ShaderPipeline* pPipeline, const VertexInputBinding* pInputBinding, bool ignoreCurrentlySetResources)
        {
            ID3D11DeviceContext* pContext = pCommandBuffer->pContext;
            if (pPipeline->pVertexShader == nullptr)
            {
                pContext->VSSetShader(nullptr, nullptr, 0);
            }
            else
            {
                pContext->VSSetShader(pPipeline->pVertexShader->pVertexShader, nullptr, 0);
                pContext->IASetInputLayout(pInputBinding->pLayout);
                pCommandBuffer->pCurrentlyBoundVertexFormat = pInputBinding->pVertexFormat;
            }
            pContext->PSSetShader(pPipeline->pFragmentShader != nullptr ? pPipeline->pFragmentShader->pPixelShader : nullptr, nullptr, 0);
        }

        // 0x004B2C90 (.h:362)
        void* beginImmediateGeometryCommand(GraphicsCommandBuffer* pCommandBuffer, PrimitiveType type, uint32 vertexCount, uint32 vertexStride)
        {
            if (type == PrimitiveType_QuadList)
            {
                type = PrimitiveType_TriangleList;
                vertexCount = (vertexCount >> 2) * 6;
                pCommandBuffer->quadlistImmediateCommand = true;
            }
            else
            {
                pCommandBuffer->quadlistImmediateCommand = false;
            }
            const uint32 size = vertexCount * vertexStride;
            // Magic number: at most the 64 KB of quadBuffer per command.
            if (size > sizeof(pCommandBuffer->quadBuffer))
            {
                return nullptr;
            }
            D3D11_MAPPED_SUBRESOURCE mapped;
            HRESULT result;
            if (pCommandBuffer->immediateVertexBufferOffset + size < pCommandBuffer->immediateVertexBufferSize)
            {
                result = pCommandBuffer->pContext->Map(pCommandBuffer->pImmediateVertexData, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
            }
            else
            {
                result = pCommandBuffer->pContext->Map(pCommandBuffer->pImmediateVertexData, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                if (SUCCEEDED(result))
                {
                    pCommandBuffer->immediateVertexBufferOffset = 0;
                }
            }
            if (FAILED(result))
            {
                debug::nativePrintArgs("[graphics] Could not map immediate vertex buffer! (%x)\n", result);
                return nullptr;
            }
            uint8* pData = (uint8*)mapped.pData + pCommandBuffer->immediateVertexBufferOffset;
            pCommandBuffer->immediateVertexDataStride = vertexStride;
            pCommandBuffer->immediatePrimitiveType = type;
            pCommandBuffer->immediateVertexCount = vertexCount;
            pCommandBuffer->pCurrentImmediateBuffer = pData;
            if (pCommandBuffer->quadlistImmediateCommand)
            {
                return pCommandBuffer->quadBuffer;
            }
            return pData;
        }

        // 0x004B2D60 (.h:425)
        void endImmediateGeometryCommand(GraphicsCommandBuffer* pCommandBuffer)
        {
            const uint32 stride = pCommandBuffer->immediateVertexDataStride;
            if (pCommandBuffer->quadlistImmediateCommand)
            {
                expandQuadList(pCommandBuffer, stride);
            }
            ID3D11DeviceContext* pContext = pCommandBuffer->pContext;
            pContext->Unmap(pCommandBuffer->pImmediateVertexData, 0);
            pContext->IASetVertexBuffers(0, 1, &pCommandBuffer->pImmediateVertexData, &stride, &pCommandBuffer->immediateVertexBufferOffset);
            pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
            pContext->IASetPrimitiveTopology(getPrimitiveTopologyFromPrimitiveType(pCommandBuffer->immediatePrimitiveType));
            pContext->Draw(pCommandBuffer->immediateVertexCount, 0);
            pCommandBuffer->immediateVertexBufferOffset += pCommandBuffer->immediateVertexCount * stride;
            pCommandBuffer->immediateVertexDataStride = 0;
        }

        // 0x004B2F30 (.h:488)
        void writeInvalidateTextureCacheCommand(GraphicsCommandBuffer* pCommandBuffer, const TextureData* pData)
        {
        }

        // 0x004B2F40 (.h:498)
        void writeCopyTextureDataCommand(GraphicsCommandBuffer* pCommandBuffer, TextureData* pTarget, const TextureData* pSource)
        {
            pCommandBuffer->pContext->CopyResource(pTarget->pTexture, pSource->pTexture);
        }

        // 0x00C5B740 (.h:506)
        void writeCopyTextureLevelDataCommand(GraphicsCommandBuffer* pCommandBuffer, TextureData* pTarget, uint32 targetLevel, const TextureData* pSource, uint32 sourceLevel)
        {
            pCommandBuffer->pContext->CopySubresourceRegion(pTarget->pTexture, targetLevel, 0, 0, 0, pSource->pTexture, sourceLevel, nullptr);
        }

        // 0x004B2F70 (.h:666)
        void* beginInstancingBatch(GraphicsCommandBuffer* pCommandBuffer, GeometryInstancingBatch* pBatch, GeometryInstancingBuffer* pInstancingBuffer, uint32 instanceCount, uint32 instanceDataSize)
        {
            const uint32 size = instanceCount * instanceDataSize;
            if (pInstancingBuffer->pVertexBuffer == nullptr || pInstancingBuffer->currentPosition + size >= pInstancingBuffer->size)
            {
                return nullptr;
            }
            D3D11_MAPPED_SUBRESOURCE mapped;
            const D3D11_MAP mapType = pInstancingBuffer->currentPosition == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
            if (FAILED(pCommandBuffer->pContext->Map(pInstancingBuffer->pVertexBuffer, 0, mapType, 0, &mapped)))
            {
                debug::nativePrintArgs("Could not map instancing buffer!\n");
                return nullptr;
            }
            pBatch->instanceCount = instanceCount;
            pBatch->pVertexBuffer = pInstancingBuffer->pVertexBuffer;
            pBatch->vertexBufferByteOffset = pInstancingBuffer->currentPosition;
            pBatch->vertexBufferStride = instanceDataSize;
            pInstancingBuffer->currentPosition += size;
            pInstancingBuffer->pContext = pCommandBuffer->pContext;
            return (uint8*)mapped.pData + pBatch->vertexBufferByteOffset;
        }

        // 0x004B3010 (.h:717)
        void endInstancingBatch(GraphicsCommandBuffer* pCommandBuffer, GeometryInstancingBuffer* pInstancingBuffer)
        {
            pInstancingBuffer->pContext->Unmap(pInstancingBuffer->pVertexBuffer, 0);
            pInstancingBuffer->pContext = nullptr;
        }

        // 0x00C5B780 (.h:727)
        void clearVertexShaderConstantBuffers(GraphicsCommandBuffer* pCommandBuffer)
        {
            ID3D11Buffer* buffers[8] = {};
            pCommandBuffer->pContext->VSSetConstantBuffers(0, 8, buffers);
        }

        // 0x004B3030 (.h:736)
        void clearTextureAndSamplerBindings(GraphicsCommandBuffer* pCommandBuffer)
        {
            ID3D11ShaderResourceView* views[16] = {};
            pCommandBuffer->pContext->PSSetShaderResources(0, 16, views);
            pCommandBuffer->pContext->VSSetShaderResources(0, 16, views);
        }

        // 0x004B30B0 (.h:744)
        void* beginDynamicConstantUpdate(GraphicsCommandBuffer* pCommandBuffer, DynamicConstantBuffer* pBuffer, uint32 shaderMask)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (FAILED(pCommandBuffer->pContext->Map(pBuffer->pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                return nullptr;
            }
            pCommandBuffer->pMappedConstantBuffer = pBuffer->pBuffer;
            return mapped.pData;
        }

        // 0x004B30F0 (.h:763)
        void endDynamicConstantUpdate(GraphicsCommandBuffer* pCommandBuffer, uint32 slotIndex, uint32 shaderMask)
        {
            pCommandBuffer->pContext->Unmap(pCommandBuffer->pMappedConstantBuffer, 0);
            if ((shaderMask & ShaderMask_Vertex) != 0)
            {
                pCommandBuffer->pContext->VSSetConstantBuffers(slotIndex, 1, &pCommandBuffer->pMappedConstantBuffer);
            }
            if ((shaderMask & ShaderMask_Fragment) != 0)
            {
                pCommandBuffer->pContext->PSSetConstantBuffers(slotIndex, 1, &pCommandBuffer->pMappedConstantBuffer);
            }
            pCommandBuffer->pMappedConstantBuffer = nullptr;
        }

        // 0x00C5B7D0 (.h:781)
        void clearFragmentShaderConstantBuffers(GraphicsCommandBuffer* pCommandBuffer)
        {
            ID3D11Buffer* buffers[8] = {};
            pCommandBuffer->pContext->PSSetConstantBuffers(0, 8, buffers);
        }

        // 0x004DB910 (.h:804)
        void writeDrawGeometryCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderGeometry* pGeometry, const Matrix43& worldMatrix, const SkinnedGeometryInstance* pSkinnedInstance)
        {
            // The world matrix is not used on D3D11 (shader constants carry the transforms).
            ID3D11DeviceContext* pContext = pCommandBuffer->pContext;
            ID3D11Buffer* pIndexBuffer = pGeometry->pIndexBuffer;
            const uint32 drawCount = pIndexBuffer != nullptr ? pGeometry->indexCount : pGeometry->vertexCount;
            if (pSkinnedInstance != nullptr)
            {
                // Skinned positions come from the skinning buffer (stream 0), the rest from the geometry (stream 1).
                ID3D11Buffer* buffers[2] = { pSkinnedInstance->pVertexBuffer, pGeometry->pVertexBuffer };
                const UINT strides[2] = { pCommandBuffer->pCurrentlyBoundVertexFormat->streamStride[0], pCommandBuffer->pCurrentlyBoundVertexFormat->streamStride[1] };
                const UINT offsets[2] = { pSkinnedInstance->skinBufferOffset + pGeometry->skinningBufferOffset, 0 };
                pContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
            }
            else
            {
                const UINT stride = pCommandBuffer->pCurrentlyBoundVertexFormat->streamStride[0];
                const UINT offset = 0;
                pContext->IASetVertexBuffers(0, 1, &pGeometry->pVertexBuffer, &stride, &offset);
            }
            pContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)pGeometry->primitiveTopology);
            if (pIndexBuffer != nullptr)
            {
                pContext->IASetIndexBuffer(pIndexBuffer, (DXGI_FORMAT)pGeometry->indexFormat, 0);
                pContext->DrawIndexed(drawCount, 0, 0);
            }
            else
            {
                pContext->Draw(drawCount, 0);
            }
        }

        // 0x004B3150 (.h:882)
        void writeDrawInstancedVertexBuffersCommand(GraphicsCommandBuffer* pCommandBuffer, const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData, const GeometryInstancingBatch* pInstancingBatch)
        {
            // Magic number: at most 7 geometry streams plus the instance stream.
            ID3D11Buffer* buffers[8];
            UINT strides[8];
            UINT offsets[8];
            for (uint32 i = 0; i < bufferCount; ++i)
            {
                buffers[i] = pVertexBuffers[i].pBuffer;
                strides[i] = pCommandBuffer->pCurrentlyBoundVertexFormat->streamStride[i];
                offsets[i] = pVertexBuffers[i].offset;
            }
            buffers[bufferCount] = pInstancingBatch->pVertexBuffer;
            offsets[bufferCount] = pInstancingBatch->vertexBufferByteOffset;
            strides[bufferCount] = pInstancingBatch->vertexBufferStride;
            ID3D11DeviceContext* pContext = pCommandBuffer->pContext;
            pContext->IASetVertexBuffers(0, bufferCount + 1, buffers, strides, offsets);
            pContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)pPrimitiveData->primitiveTopology);
            if (pIndexBuffer != nullptr && pIndexBuffer->pBuffer != nullptr)
            {
                pContext->IASetIndexBuffer(pIndexBuffer->pBuffer, (DXGI_FORMAT)pIndexBuffer->indexFormat, pIndexBuffer->offset);
                pContext->DrawIndexedInstanced(pPrimitiveData->vertexDrawCount, pInstancingBatch->instanceCount, 0, 0, 0);
            }
            else
            {
                pContext->DrawInstanced(pPrimitiveData->vertexDrawCount, pInstancingBatch->instanceCount, 0, 0);
            }
        }

        // 0x004B3280 (.h:924)
        void writeDrawVertexBuffersCommand(GraphicsCommandBuffer* pCommandBuffer, const Matrix43& worldMatrix, const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData)
        {
            // Magic number: at most 4 vertex streams.
            ID3D11Buffer* buffers[4];
            UINT strides[4];
            UINT offsets[4];
            for (uint32 i = 0; i < bufferCount; ++i)
            {
                buffers[i] = pVertexBuffers[i].pBuffer;
                strides[i] = pCommandBuffer->pCurrentlyBoundVertexFormat->streamStride[i];
                offsets[i] = pVertexBuffers[i].offset;
            }
            ID3D11DeviceContext* pContext = pCommandBuffer->pContext;
            pContext->IASetVertexBuffers(0, bufferCount, buffers, strides, offsets);
            pContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)pPrimitiveData->primitiveTopology);
            if (pIndexBuffer != nullptr && pIndexBuffer->pBuffer != nullptr)
            {
                pContext->IASetIndexBuffer(pIndexBuffer->pBuffer, (DXGI_FORMAT)pIndexBuffer->indexFormat, pIndexBuffer->offset);
                pContext->DrawIndexed(pPrimitiveData->vertexDrawCount, 0, 0);
            }
            else
            {
                pContext->Draw(pPrimitiveData->vertexDrawCount, 0);
            }
        }

        // 0x004B3380 (.h:984)
        void callCommandBuffer(GraphicsCommandBuffer* pCommandBuffer, RecordedCommandBuffer* pOtherBuffer)
        {
            if (pOtherBuffer->pCommandList != nullptr)
            {
                pCommandBuffer->pContext->ExecuteCommandList(pOtherBuffer->pCommandList, FALSE);
                pOtherBuffer->pCommandList->Release();
            }
            pOtherBuffer->pCommandList = nullptr;
        }

        // 0x004B33C0 (.h:996)
        void beginOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer, OcclusionQuery* pOcclusionQuery)
        {
            pCommandBuffer->pContext->Begin(pOcclusionQuery->pQuery);
        }

        // 0x004B33E0 (.h:1004)
        void endOcclusionQuery(GraphicsCommandBuffer* pCommandBuffer, OcclusionQuery* pOcclusionQuery)
        {
            pCommandBuffer->pContext->End(pOcclusionQuery->pQuery);
        }

        // 0x00C5B820 (.h:1012)
        bool pollOcclusionQueryResult(GraphicsCommandBuffer* pCommandBuffer, OcclusionQuery* pOcclusionQuery, uint64_t* pResult)
        {
            return pCommandBuffer->pContext->GetData(pOcclusionQuery->pQuery, pResult, sizeof(uint64_t), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
        }

        // 0x00C5B850 (:29)
        void* mapTextureData(MappedTextureInfo* pMappedData, GraphicsCommandBuffer* pCommandBuffer, TextureData* pTexture, uint32 level, GraphicsMapType mapType, uint32 mapFlags)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (FAILED(pCommandBuffer->pContext->Map(pTexture->pTexture, level, s_d3dMapType[mapType], getD3dMapFlags(mapFlags), &mapped)))
            {
                return nullptr;
            }
            pMappedData->rowPitch = mapped.RowPitch;
            pMappedData->slicePitch = mapped.DepthPitch;
            return mapped.pData;
        }

        // 0x00C5B8C0 (:50)
        void unmapTextureData(GraphicsCommandBuffer* pCommandBuffer, TextureData* pTexture, uint32 level)
        {
            pCommandBuffer->pContext->Unmap(pTexture->pTexture, level);
        }

        // 0x00C5B8E0 (:58)
        void* mapRenderGeometryVertexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (FAILED(pCommandBuffer->pContext->Map(pRenderGeometry->pVertexBuffer, 0, s_d3dMapType[mapType], getD3dMapFlags(mapFlags), &mapped)))
            {
                return nullptr;
            }
            return (uint8*)mapped.pData + startOffset;
        }

        // 0x00C5B940 (:81)
        void unmapRenderGeometryVertexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData)
        {
            pCommandBuffer->pContext->Unmap(pRenderGeometry->pVertexBuffer, 0);
        }

        // 0x00C5B960 (:89)
        void* mapRenderGeometryIndexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (FAILED(pCommandBuffer->pContext->Map(pRenderGeometry->pIndexBuffer, 0, s_d3dMapType[mapType], getD3dMapFlags(mapFlags), &mapped)))
            {
                return nullptr;
            }
            return (uint8*)mapped.pData + startOffset;
        }

        // 0x00C5B9C0 (:111)
        void unmapRenderGeometryIndexData(GraphicsCommandBuffer* pCommandBuffer, RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData)
        {
            pCommandBuffer->pContext->Unmap(pRenderGeometry->pIndexBuffer, 0);
        }

        // 0x00C6A5D0 (:158)
        void writeDownsampleDepthBufferCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderTarget* pTarget, const TextureData* pSourceData)
        {
            const RenderTarget* pPreviousTarget = pCommandBuffer->pCurrentRenderTarget;
            writeSetRenderTargetCommand(pCommandBuffer, pTarget);
            const uint32 width = pTarget != nullptr ? pTarget->width : 0;
            const uint32 height = pTarget != nullptr ? pTarget->height : 0;
            writeDownsampleDepthContextCommands(pCommandBuffer, pCommandBuffer->pDownsampleDepthContext, pSourceData, false, width, height);
            writeSetRenderTargetCommand(pCommandBuffer, pPreviousTarget);
        }

        // 0x00C6A7B0 (:172)
        void writeCopyDepthBufferCommand(GraphicsCommandBuffer* pCommandBuffer, const RenderTarget* pTarget, const TextureData* pSourceData)
        {
            const RenderTarget* pPreviousTarget = pCommandBuffer->pCurrentRenderTarget;
            writeSetRenderTargetCommand(pCommandBuffer, pTarget);
            writeCopyDepthContextCommands(pCommandBuffer, pCommandBuffer->pDownsampleDepthContext, pSourceData);
            writeSetRenderTargetCommand(pCommandBuffer, pPreviousTarget);
        }
    }
}
