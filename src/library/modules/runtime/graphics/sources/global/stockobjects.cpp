// Original: library/modules/runtime/graphics/sources/global/stockobjects.cpp
//
// StockObjects: the immediate and copy shaders (from the shader file system), the three screen-quad/immediate vertex
// formats and a 4x4 white texture.

#include "library/modules/runtime/graphics/sources/global/stockobjects.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/global/shaderfactory.hpp"
#include "keen/graphics/texturedata.hpp"

namespace keen
{
    // 0x00C602B0 (:8)
    StockObjects::StockObjects()
    {
        for (uint32 i = 0; i < GraphicsStockVertexShaderId_Count; ++i)
        {
            m_pStockVertexShaders[i] = nullptr;
        }
        for (uint32 i = 0; i < GraphicsStockFragmentShaderId_Count; ++i)
        {
            m_pStockFragmentShaders[i] = nullptr;
        }
        for (uint32 i = 0; i < GraphicsStockVertexFormat_Count; ++i)
        {
            m_pFormats[i] = nullptr;
        }
    }

    static const VertexFormat* createStockVertexFormat(GraphicsSystem* pGraphicsSystem, const VertexAttributeDescription* pAttributes, uint32 attributeCount)
    {
        // createVertexFormat inlined.
        VertexFormat* pFormat;
        if (pGraphicsSystem->stateObjectPool.allocateVertexFormat(&pFormat, pAttributes, attributeCount) && !pFormat->fill(pAttributes, attributeCount))
        {
            pGraphicsSystem->stateObjectPool.freeVertexFormat(pFormat);
            return nullptr;
        }
        return pFormat;
    }

    // 0x00C72C10 (:24)
    void StockObjects::create(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureAllocator)
    {
        ShaderFactory* pShaderFactory = pGraphicsSystem->pShaderFactory;
        m_pStockVertexShaders[GraphicsStockVertexShaderId_Immediate] = graphics::createVertexShaderFromFile(pShaderFactory, "immediate.vso", "immediate.vso");
        m_pStockFragmentShaders[GraphicsStockFragmentShaderId_Immediate] = graphics::createFragmentShaderFromFile(pShaderFactory, "immediate_0.fso", "immediate_0.fso");
        m_pStockFragmentShaders[GraphicsStockFragmentShaderId_Immediate_VolumeSlice] = graphics::createFragmentShaderFromFile(pShaderFactory, "immediate_1.fso", "immediate_1.fso");
        m_pStockVertexShaders[GraphicsStockVertexShaderId_Copy] = graphics::createVertexShaderFromFile(pShaderFactory, "copy.vso", "copy.vso");
        m_pStockFragmentShaders[GraphicsStockFragmentShaderId_Copy] = graphics::createFragmentShaderFromFile(pShaderFactory, "copy.fso", "copy.fso");

        m_shaderPipelines[GraphicsStockShaderPipelineId_Immediate].pVertexShader = m_pStockVertexShaders[GraphicsStockVertexShaderId_Immediate];
        m_shaderPipelines[GraphicsStockShaderPipelineId_Immediate].pFragmentShader = m_pStockFragmentShaders[GraphicsStockFragmentShaderId_Immediate];
        m_shaderPipelines[GraphicsStockShaderPipelineId_Immediate_VolumeSlice].pVertexShader = m_pStockVertexShaders[GraphicsStockVertexShaderId_Immediate];
        m_shaderPipelines[GraphicsStockShaderPipelineId_Immediate_VolumeSlice].pFragmentShader = m_pStockFragmentShaders[GraphicsStockFragmentShaderId_Immediate_VolumeSlice];
        m_shaderPipelines[GraphicsStockShaderPipelineId_Copy].pVertexShader = m_pStockVertexShaders[GraphicsStockVertexShaderId_Copy];
        m_shaderPipelines[GraphicsStockShaderPipelineId_Copy].pFragmentShader = m_pStockFragmentShaders[GraphicsStockFragmentShaderId_Copy];

        // Pos2f is the first attribute of Pos2fTex2f.
        VertexAttributeDescription pos2fTex2f[2];
        pos2fTex2f[0].id = VertexAttributeId_Position;
        pos2fTex2f[0].format = VertexAttributeFormat_x32y32_float;
        pos2fTex2f[0].inputStreamIndex = 0;
        pos2fTex2f[0].instanceStepRate = 0;
        pos2fTex2f[1].id = VertexAttributeId_TexCoord0;
        pos2fTex2f[1].format = VertexAttributeFormat_x32y32_float;
        pos2fTex2f[1].inputStreamIndex = 0;
        pos2fTex2f[1].instanceStepRate = 0;
        m_pFormats[GraphicsStockVertexFormat_Pos2f] = createStockVertexFormat(pGraphicsSystem, pos2fTex2f, 1);
        m_pFormats[GraphicsStockVertexFormat_Pos2f_Tex2f] = createStockVertexFormat(pGraphicsSystem, pos2fTex2f, 2);

        VertexAttributeDescription immediate[3];
        immediate[0].id = VertexAttributeId_Position;
        immediate[0].format = VertexAttributeFormat_x32y32z32_float;
        immediate[0].inputStreamIndex = 0;
        immediate[0].instanceStepRate = 0;
        immediate[1].id = VertexAttributeId_Color0;
        immediate[1].format = VertexAttributeFormat_x8y8z8w8_unorm;
        immediate[1].inputStreamIndex = 0;
        immediate[1].instanceStepRate = 0;
        immediate[2].id = VertexAttributeId_TexCoord0;
        immediate[2].format = VertexAttributeFormat_x32y32_float;
        immediate[2].inputStreamIndex = 0;
        immediate[2].instanceStepRate = 0;
        m_pFormats[GraphicsStockVertexFormat_Immediate] = createStockVertexFormat(pGraphicsSystem, immediate, 3);

        // Magic numbers: a 4x4 R8G8B8A8 texture, all texels 0xFFFFFFFF.
        uint32 whiteTexels[16];
        for (uint32 i = 0; i < 16; ++i)
        {
            whiteTexels[i] = 0xffffffffu;
        }
        TextureDescription description;
        description.width = 4;
        description.height = 4;
        description.depth = 0;
        description.flags = TextureFlagMask_Bind_ShaderInput;
        description.type = TextureType_2d;
        description.format = PixelFormat_R8G8B8A8;
        description.lutFormat = 0;
        description.multiSampleType = 0;
        description.addressModeU = 0;
        description.addressModeV = 0;
        description.addressModeW = 0;
        description.levelCount = 1;
        description.cpuAccessMode = TextureAccessMode_None;
        description.gpuAccessMode = TextureAccessMode_Read;
        InitialTextureLevelData levelData;
        levelData.pData = whiteTexels;
        levelData.rowPitch = 16;
        levelData.slicePitch = 0;
        graphics::allocateTextureData(&m_textures[GraphicsStockTextureId_White], pGraphicsSystem, pTextureAllocator, description, &levelData, 1, "StockWhiteTexture");
    }

    // 0x00C70DF0 (:131)
    void StockObjects::destroy(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureAllocator)
    {
        graphics::freeTextureData(&m_textures[GraphicsStockTextureId_White], pGraphicsSystem, pTextureAllocator);
        for (uint32 i = 0; i < GraphicsStockShaderPipelineId_Count; ++i)
        {
            m_shaderPipelines[i].pVertexShader = nullptr;
            m_shaderPipelines[i].pFragmentShader = nullptr;
        }
        for (uint32 i = 0; i < GraphicsStockVertexShaderId_Count; ++i)
        {
            graphics::destroyVertexShader(pGraphicsSystem->pShaderFactory, m_pStockVertexShaders[i]);
            m_pStockVertexShaders[i] = nullptr;
        }
        for (uint32 i = 0; i < GraphicsStockFragmentShaderId_Count; ++i)
        {
            graphics::destroyFragmentShader(pGraphicsSystem->pShaderFactory, m_pStockFragmentShaders[i]);
            m_pStockFragmentShaders[i] = nullptr;
        }
        for (uint32 i = 0; i < GraphicsStockVertexFormat_Count; ++i)
        {
            if (m_pFormats[i] != nullptr)
            {
                pGraphicsSystem->stateObjectPool.freeVertexFormat(m_pFormats[i]);
            }
            m_pFormats[i] = nullptr;
        }
    }

    // 0x00C5B9F0 (.h:18)
    const ShaderPipeline* StockObjects::getShaderPipeline(GraphicsStockShaderPipelineId id)
    {
        return &m_shaderPipelines[id];
    }

    // 0x00C5BA00 (.h:22)
    const VertexFormat* StockObjects::getFormat(GraphicsStockVertexFormat id) const
    {
        return m_pFormats[id];
    }

    // 0x00C5BA10 (.h:23)
    const TextureData* StockObjects::getTexture(GraphicsStockTextureId id) const
    {
        return &m_textures[id];
    }
}
