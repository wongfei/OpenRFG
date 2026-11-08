// Original: library/modules/runtime/graphics/sources/global/graphicscommandwriter.cpp
//
// GraphicsCommandWriter, the front end the renderers write commands through. It remembers the bound state objects,
// shaders, textures and samplers (unknown state is 0xFFFFFFFF), keeps a stack of render passes and forwards the
// commands to the GraphicsCommandBuffer. The command functions are in graphicscommandwriter.inl (inline in the
// original), included at the end of this file.

#include "keen/graphics/graphicscommandwriter.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/screencapture_d3d11.hpp"

namespace keen
{
    // Unknown (not yet written) sampler state: forces the next write through.
    static const SamplerState* const s_pUnknownSamplerState = (const SamplerState*)0xffffffffu;

    // 0x00C64440 (:9)
    GraphicsCommandWriter::GraphicsCommandWriter()
    {
        m_pBuffer = nullptr;
        m_pGraphicsSystem = nullptr;
        m_renderPassStackIndex = 0;
        for (uint32 i = 0; i < 4; ++i)
        {
            m_renderPassStack[i] = nullptr;
        }
        invalidateState();
    }

    // 0x00C64510 (:28)
    void GraphicsCommandWriter::beginWriting(GraphicsCommandBuffer* pTarget, GraphicsSystem* pGraphicsSystem)
    {
        m_pBuffer = pTarget;
        m_pGraphicsSystem = pGraphicsSystem;
        m_screenQuadVertexFormats[GraphicsStockVertexFormat_Pos2f] = m_pGraphicsSystem->stockObjects.getFormat(GraphicsStockVertexFormat_Pos2f);
        m_screenQuadVertexFormats[GraphicsStockVertexFormat_Pos2f_Tex2f] = m_pGraphicsSystem->stockObjects.getFormat(GraphicsStockVertexFormat_Pos2f_Tex2f);
        m_screenQuadVertexFormats[GraphicsStockVertexFormat_Immediate] = m_pGraphicsSystem->stockObjects.getFormat(GraphicsStockVertexFormat_Immediate);
        m_pRenderTarget = getBackBufferRenderTarget();
        invalidateState();
    }

    // 0x00C5D020 (:51)
    void GraphicsCommandWriter::endWriting()
    {
        m_pBuffer = nullptr;
        m_pGraphicsSystem = nullptr;
    }

    // 0x00C5D030 (:59)
    void GraphicsCommandWriter::invalidateState()
    {
        for (uint32 i = 0; i < 8; ++i)
        {
            m_currentStaticVertexConstantBuffers[i] = nullptr;
            m_currentStaticFragmentConstantBuffers[i] = nullptr;
        }
        m_pBlendState = nullptr;
        m_pRasterizerState = nullptr;
        m_pDepthStencilState = nullptr;
        m_pShaderPipeline = nullptr;
        m_pVertexInputBinding = nullptr;
        for (uint32 i = 0; i < 16; ++i)
        {
            m_vertexShaderTextures[i] = (const TextureData*)s_pUnknownSamplerState;
            m_vertexShaderSamplerStates[i] = s_pUnknownSamplerState;
            m_fragmentShaderSamplerStates[i] = s_pUnknownSamplerState;
            m_fragmentShaderTextures[i] = (const TextureData*)s_pUnknownSamplerState;
        }
    }

    // 0x004B3400 (.h:22)
    GraphicsCommandBuffer* GraphicsCommandWriter::getBuffer()
    {
        return m_pBuffer;
    }

    // 0x008A67D0 (.h:29)
    void GraphicsCommandWriter::writeClearCommand(uint32 inColor, float inDepth, uint8 inStencilValue, uint32 inClearMask)
    {
        writeClearCommand(ClearSettings(inColor, inDepth, inStencilValue, inClearMask));
    }

    // 0x004DBAC0 (.h:48)
    void GraphicsCommandWriter::writeSetFragmentShaderTextureCommand(uint32 stageIndex, const TextureData* pTexture, const SamplerState* pState)
    {
        // Notes (decompiled behaviour, kept as is): no caching, both bindings are always written.
        m_fragmentShaderSamplerStates[stageIndex] = pState;
        graphics::writeSetFragmentShaderSamplerStateCommand(m_pBuffer, stageIndex, pState);
        m_fragmentShaderTextures[stageIndex] = pTexture;
        graphics::writeSetFragmentShaderTextureCommand(m_pBuffer, stageIndex, pTexture);
    }

    // 0x00C624D0 (.h:56)
    void GraphicsCommandWriter::writeSetVertexShaderTextureCommand(uint32 stageIndex, const TextureData* pTexture, const SamplerState* pState)
    {
        graphics::writeSetVertexShaderSamplerStateCommand(m_pBuffer, stageIndex, pState);
        m_vertexShaderSamplerStates[stageIndex] = pState;
        writeSetVertexShaderTextureCommand(stageIndex, pTexture);
    }

    // 0x004B3410 (.h:170)
    const RenderTarget* GraphicsCommandWriter::getCurrentRenderTarget() const
    {
        return m_pRenderTarget;
    }

    // 0x00C62550 (.h:171)
    const RenderTarget* GraphicsCommandWriter::getBackBufferRenderTarget() const
    {
        return graphics::getBackBufferRenderTarget(m_pGraphicsSystem);
    }
}

#include "library/modules/runtime/graphics/sources/global/graphicscommandwriter.inl"
