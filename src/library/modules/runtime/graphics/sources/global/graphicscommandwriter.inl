// Original: library/modules/runtime/graphics/sources/global/graphicscommandwriter.inl
//
// The GraphicsCommandWriter command functions (inline in the original; included by graphicscommandwriter.cpp here).
// They forward to the D3D11 command functions of graphicscommandbuffer_d3d11.hpp, skipping redundant blend,
// rasterizer and depth/stencil state changes.
#pragma once

#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/direct3d_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/global/graphicscommandbuffer.hpp"
#include "keen/graphics/vertexformat.hpp"

namespace keen
{
    // 0x004B34C0 (:30)
    void GraphicsCommandWriter::writeClearCommand(const ClearSettings& clearSettings)
    {
        if (clearSettings.clearMask != 0)
        {
            graphics::writeClearCommand(m_pBuffer, clearSettings);
        }
    }

    // 0x004B35E0 (:49)
    void GraphicsCommandWriter::writeBeginRenderPassCommand(const RenderTarget* pRenderTarget, RenderTargetPartFlagMask restoreFlags, const ClearSettings* pClearSettings)
    {
        graphics::writeBeginRenderPassCommand(m_pBuffer, pRenderTarget, restoreFlags, pClearSettings);
        invalidateState();
        m_pRenderTarget = pRenderTarget;
        m_renderPassStack[m_renderPassStackIndex] = pRenderTarget;
        ++m_renderPassStackIndex;
    }

    // 0x004B37F0 (:67)
    void GraphicsCommandWriter::writeEndRenderPassCommand(RenderTargetPartFlagMask resolveFlags)
    {
        --m_renderPassStackIndex;
        m_renderPassStack[m_renderPassStackIndex] = nullptr;
        m_pRenderTarget = m_renderPassStackIndex == 0 ? nullptr : m_renderPassStack[m_renderPassStackIndex - 1];
        graphics::writeEndRenderPassCommand(m_pBuffer, resolveFlags, m_pRenderTarget);
        invalidateState();
    }

    // 0x004B3910 (:135)
    void GraphicsCommandWriter::writeSetViewportSettingsCommand(const ViewportSettings* pSettings)
    {
        graphics::writeSetViewportSettingsCommand(m_pBuffer, pSettings);
    }

    // 0x004B3A20 (:140)
    void GraphicsCommandWriter::writeSetScissorRectangleCommand(const ScissorRectangle* pRectangle)
    {
        graphics::writeSetScissorRectangleCommand(m_pBuffer, pRectangle);
    }

    // 0x004B3AB0 (:145)
    void GraphicsCommandWriter::writeSetBlendStateCommand(const BlendState* pState)
    {
        if (m_pBlendState != pState)
        {
            graphics::writeSetBlendStateCommand(m_pBuffer, pState);
            m_pBlendState = pState;
        }
    }

    // 0x004B3B20 (:160)
    void GraphicsCommandWriter::writeSetRasterizerStateCommand(const RasterizerState* pState)
    {
        if (m_pRasterizerState != pState)
        {
            graphics::writeSetRasterizerStateCommand(m_pBuffer, pState);
            m_pRasterizerState = pState;
        }
    }

    // 0x004B3B50 (:174)
    void GraphicsCommandWriter::writeSetFragmentShaderSamplerStateCommand(uint32 samplerIndex, const SamplerState* pState)
    {
        m_fragmentShaderSamplerStates[samplerIndex] = pState;
        graphics::writeSetFragmentShaderSamplerStateCommand(m_pBuffer, samplerIndex, pState);
    }

    // 0x004B3BA0 (:189)
    void GraphicsCommandWriter::writeSetDepthStencilStateCommand(const DepthStencilState* pState, uint32 stencilRefValue)
    {
        if (m_pDepthStencilState != pState || m_stencilRefValue != stencilRefValue)
        {
            graphics::writeSetDepthStencilStateCommand(m_pBuffer, pState, stencilRefValue);
            m_pDepthStencilState = pState;
            m_stencilRefValue = stencilRefValue;
        }
    }

    // 0x004B3BE0 (:205)
    void GraphicsCommandWriter::writeSetFragmentShaderTextureCommand(uint32 samplerIndex, const TextureData* pTexture)
    {
        m_fragmentShaderTextures[samplerIndex] = pTexture;
        graphics::writeSetFragmentShaderTextureCommand(m_pBuffer, samplerIndex, pTexture);
    }

    // 0x004B3C30 (:220)
    void GraphicsCommandWriter::clearAllTextureStages()
    {
        graphics::clearTextureAndSamplerBindings(m_pBuffer);
    }

    // 0x00C5C580 (:225)
    void GraphicsCommandWriter::clearAllVertexConstants()
    {
        graphics::clearVertexShaderConstantBuffers(m_pBuffer);
        for (uint32 i = 0; i < 8; ++i)
        {
            m_currentStaticVertexConstantBuffers[i] = nullptr;
        }
    }

    // 0x004DBB40 (:237)
    void* GraphicsCommandWriter::beginDynamicVertexConstants(DynamicConstantBuffer* pConstantBuffer, uint32 slotIndex)
    {
        m_mappedConstantBufferSlotIndex = slotIndex;
        return graphics::beginDynamicConstantUpdate(m_pBuffer, pConstantBuffer, ShaderMask_Vertex);
    }

    // 0x004DBB90 (:243)
    void GraphicsCommandWriter::endDynamicVertexConstants()
    {
        const uint32 slotIndex = m_mappedConstantBufferSlotIndex;
        m_currentStaticVertexConstantBuffers[slotIndex] = nullptr;
        graphics::endDynamicConstantUpdate(m_pBuffer, slotIndex, ShaderMask_Vertex);
    }

    // 0x00C5C600 (:249)
    void GraphicsCommandWriter::clearAllFragmentConstants()
    {
        graphics::clearFragmentShaderConstantBuffers(m_pBuffer);
        for (uint32 i = 0; i < 8; ++i)
        {
            m_currentStaticFragmentConstantBuffers[i] = nullptr;
        }
    }

    // 0x004B3CB0 (:262)
    void* GraphicsCommandWriter::beginDynamicConstantUpdate(DynamicConstantBuffer* pConstantBuffer, uint32 shaderMask)
    {
        return graphics::beginDynamicConstantUpdate(m_pBuffer, pConstantBuffer, shaderMask);
    }

    // 0x004B3D00 (:279)
    void GraphicsCommandWriter::endDynamicConstantUpdate(uint32 slotIndex, uint32 shaderMask)
    {
        if ((shaderMask & ShaderMask_Fragment) != 0)
        {
            m_currentStaticFragmentConstantBuffers[slotIndex] = nullptr;
        }
        if ((shaderMask & ShaderMask_Vertex) != 0)
        {
            m_currentStaticVertexConstantBuffers[slotIndex] = nullptr;
        }
        graphics::endDynamicConstantUpdate(m_pBuffer, slotIndex, shaderMask);
    }

    // 0x004B3D80 (:307)
    void* GraphicsCommandWriter::beginDynamicFragmentConstants(DynamicConstantBuffer* pConstantBuffer, uint32 slotIndex)
    {
        m_mappedConstantBufferSlotIndex = slotIndex;
        return graphics::beginDynamicConstantUpdate(m_pBuffer, pConstantBuffer, ShaderMask_Fragment);
    }

    // 0x004B3DD0 (:313)
    void GraphicsCommandWriter::endDynamicFragmentConstants()
    {
        const uint32 slotIndex = m_mappedConstantBufferSlotIndex;
        m_currentStaticFragmentConstantBuffers[slotIndex] = nullptr;
        graphics::endDynamicConstantUpdate(m_pBuffer, slotIndex, ShaderMask_Fragment);
    }

    // 0x004B3E20 (:369)
    void* GraphicsCommandWriter::mapTextureData(MappedTextureInfo* pMappedData, TextureData* pTexture, uint32 level, GraphicsMapType mapType, uint32 mapFlags)
    {
        return graphics::mapTextureData(pMappedData, m_pBuffer, pTexture, level, mapType, mapFlags);
    }

    // 0x004B3E50 (:374)
    void GraphicsCommandWriter::unmapTextureData(TextureData* pTexture, uint32 level)
    {
        graphics::unmapTextureData(m_pBuffer, pTexture, level);
    }

    // 0x004B3E70 (:379)
    void* GraphicsCommandWriter::mapRenderGeometryVertexData(RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData)
    {
        return graphics::mapRenderGeometryVertexData(m_pBuffer, pRenderGeometry, mapType, mapFlags, startOffset, numBytes, pMappedData);
    }

    // 0x004B3EA0 (:384)
    void GraphicsCommandWriter::unmapRenderGeometryVertexData(RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData)
    {
        graphics::unmapRenderGeometryVertexData(m_pBuffer, pRenderGeometry, pMappedData);
    }

    // 0x00C5C680 (:389)
    void* GraphicsCommandWriter::mapRenderGeometryIndexData(RenderGeometry* pRenderGeometry, GraphicsMapType mapType, uint32 mapFlags, uint32 startOffset, uint32 numBytes, MappedRenderGeometryInfo* pMappedData)
    {
        return graphics::mapRenderGeometryIndexData(m_pBuffer, pRenderGeometry, mapType, mapFlags, startOffset, numBytes, pMappedData);
    }

    // 0x00C5C6E0 (:394)
    void GraphicsCommandWriter::unmapRenderGeometryIndexData(RenderGeometry* pRenderGeometry, MappedRenderGeometryInfo* pMappedData)
    {
        graphics::unmapRenderGeometryIndexData(m_pBuffer, pRenderGeometry, pMappedData);
    }

    // 0x004B3EC0 (:399)
    void GraphicsCommandWriter::writeInvalidateTextureCacheCommand(const TextureData* pData)
    {
        graphics::writeInvalidateTextureCacheCommand(m_pBuffer, pData);
    }

    // 0x004DBBE0 (:404)
    void GraphicsCommandWriter::writeDrawGeometryCommand(const RenderGeometry* pGeometry, const Matrix43& worldMatrix, const SkinnedGeometryInstance* pSkinnedInstance)
    {
        graphics::writeDrawGeometryCommand(m_pBuffer, pGeometry, worldMatrix, pSkinnedInstance);
    }

    // 0x004B3ED0 (:424)
    void GraphicsCommandWriter::writeDrawVertexBuffersCommand(const Matrix43& worldMatrix, const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData)
    {
        graphics::writeDrawVertexBuffersCommand(m_pBuffer, worldMatrix, pVertexBuffers, bufferCount, pIndexBuffer, pPrimitiveData);
    }

    // 0x004B3FC0 (:443)
    void GraphicsCommandWriter::writeDrawInstancedVertexBuffersCommand(const VertexBufferData* pVertexBuffers, uint32 bufferCount, const IndexBufferData* pIndexBuffer, const PrimitiveData* pPrimitiveData, const GeometryInstancingBatch* pInstancingBatch)
    {
        graphics::writeDrawInstancedVertexBuffersCommand(m_pBuffer, pVertexBuffers, bufferCount, pIndexBuffer, pPrimitiveData, pInstancingBatch);
    }

    // 0x004B4100 (:456)
    void GraphicsCommandWriter::writeSetShaderPipelineCommand(const ShaderPipeline* pPipeline, const VertexInputBinding* pInputBinding, bool ignoreCurrentlySetResources)
    {
        graphics::writeSetShaderPipelineCommand(m_pBuffer, pPipeline, pInputBinding, ignoreCurrentlySetResources);
        m_pVertexInputBinding = pInputBinding;
        m_pShaderPipeline = pPipeline;
    }

    // 0x00C5C700 (:474)
    void GraphicsCommandWriter::clearAllConstantBuffers()
    {
        clearAllVertexConstants();
        clearAllFragmentConstants();
    }

    // 0x00C5C7E0 (:484)
    void GraphicsCommandWriter::writeSetVertexShaderTextureCommand(uint32 stageIndex, const TextureData* pTexture)
    {
        // Notes (decompiled behaviour, kept as is): stages past 16 are only skipped when unbinding.
        if (stageIndex < 16 || pTexture != nullptr)
        {
            graphics::writeSetVertexShaderTextureCommand(m_pBuffer, stageIndex, pTexture);
            m_vertexShaderTextures[stageIndex] = pTexture;
        }
    }

    // 0x00C5C830 (:503)
    void GraphicsCommandWriter::writeSetVertexShaderSamplerStateCommand(uint32 samplerIndex, const SamplerState* pState)
    {
        graphics::writeSetVertexShaderSamplerStateCommand(m_pBuffer, samplerIndex, pState);
        m_vertexShaderSamplerStates[samplerIndex] = pState;
    }

    // 0x004B4190 (:518)
    void* GraphicsCommandWriter::beginImmediateGeometryCommand(PrimitiveType type, uint32 vertexCount, uint32 vertexStride)
    {
        return graphics::beginImmediateGeometryCommand(m_pBuffer, type, vertexCount, vertexStride);
    }

    // 0x004B4260 (:535)
    void GraphicsCommandWriter::endImmediateGeometryCommand()
    {
        graphics::endImmediateGeometryCommand(m_pBuffer);
    }

    // 0x004B4430 (:548)
    void* GraphicsCommandWriter::beginInstancingBatch(GeometryInstancingBatch* pBatch, GeometryInstancingBuffer* pInstancingBuffer, uint32 instanceCount, uint32 instanceDataSize)
    {
        return graphics::beginInstancingBatch(m_pBuffer, pBatch, pInstancingBuffer, instanceCount, instanceDataSize);
    }

    // 0x004B44D0 (:554)
    void GraphicsCommandWriter::endInstancingBatch(GeometryInstancingBuffer* pInstancingBuffer)
    {
        graphics::endInstancingBatch(m_pBuffer, pInstancingBuffer);
    }

    // 0x004B44F0 (:575)
    void GraphicsCommandWriter::beginOcclusionQuery(OcclusionQuery* pOcclusionQuery)
    {
        graphics::beginOcclusionQuery(m_pBuffer, pOcclusionQuery);
    }

    // 0x004B4510 (:581)
    void GraphicsCommandWriter::endOcclusionQuery(OcclusionQuery* pOcclusionQuery)
    {
        graphics::endOcclusionQuery(m_pBuffer, pOcclusionQuery);
    }

    // 0x004B4530 (:647)
    void GraphicsCommandWriter::writeDrawFullscreenQuadCommandPos2fTex2f(float u0, float v0, float u1, float v1)
    {
        // One triangle that covers the screen: (-1, 1), (-1, -3), (3, 1).
        // Notes (decompiled behaviour, kept as is): the far texture coordinates are u1 * 2 and v1 * 2, which equals the
        // linear extrapolation only when u0 and v0 are 0.
        const uint32 stride = m_screenQuadVertexFormats[GraphicsStockVertexFormat_Pos2f_Tex2f]->streamStride[0];
        float* pVertices = (float*)graphics::beginImmediateGeometryCommand(m_pBuffer, PrimitiveType_TriangleList, 3, stride);
        if (pVertices == nullptr)
        {
            return;
        }
        pVertices[0] = -1.0f;
        pVertices[1] = 1.0f;
        pVertices[2] = u0;
        pVertices[3] = v0;
        pVertices[4] = -1.0f;
        pVertices[5] = -3.0f;
        pVertices[6] = u0;
        pVertices[7] = (float)((double)v1 * 2.0);
        pVertices[8] = 3.0f;
        pVertices[9] = 1.0f;
        pVertices[10] = (float)((double)u1 * 2.0);
        pVertices[11] = v0;
        graphics::endImmediateGeometryCommand(m_pBuffer);
    }

    // 0x004B4840 (:714)
    void GraphicsCommandWriter::writeCopyTextureDataCommand(TextureData* pTarget, const TextureData* pSource)
    {
        graphics::writeCopyTextureDataCommand(m_pBuffer, pTarget, pSource);
    }

    // (inlined in rl_scene_renderer::render_scene_part2)
    void GraphicsCommandWriter::writeCopyDepthBufferCommand(const RenderTarget* pTarget, const TextureData* pSourceData)
    {
        graphics::writeCopyDepthBufferCommand(m_pBuffer, pTarget, pSourceData);
    }

    // 0x00C5C870 (:719)
    void GraphicsCommandWriter::writeCopyTextureLevelDataCommand(TextureData* pTarget, uint32 targetLevel, const TextureData* pSource, uint32 sourceLevel)
    {
        graphics::writeCopyTextureLevelDataCommand(m_pBuffer, pTarget, targetLevel, pSource, sourceLevel);
    }
}
