// Original: library/modules/runtime/graphics/sources/global/graphicssystem.cpp
//
// The platform independent part of the graphics system: GraphicsSystemBase (shader factory, state object pools and
// stock objects) and the shader/stock object accessors of the graphics API. Also defined here: the functions that are
// inline in graphicssystem.hpp, graphicssystembase.hpp and graphicsstateobjects.hpp in the original (marked .h:N).

#include "library/modules/runtime/graphics/sources/global/graphicssystembase.hpp"
#include "library/modules/runtime/graphics/sources/global/shaderfactory.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/base/crc32.hpp"

#include <string.h>

namespace keen
{
    // 0x00C6EF40 (graphicssystembase.hpp:15)
    GraphicsSystemBase::GraphicsSystemBase()
    {
        pShaderFactory = nullptr;
        pSystemAllocator = nullptr;
    }

    // 0x00C6D9C0
    GraphicsSystemBase::~GraphicsSystemBase()
    {
    }

    // 0x004DB490 (graphicsstateobjects.hpp:198)
    BlendParametersEx::BlendParametersEx()
    {
        blendOpColor = BlendOperation_None;
        sourceFactorColor = BlendFactor_Zero;
        destFactorColor = BlendFactor_Zero;
        blendOpAlpha = BlendOperation_None;
        sourceFactorAlpha = BlendFactor_Zero;
        destFactorAlpha = BlendFactor_Zero;
        for (uint32 i = 0; i < 8; ++i)
        {
            colorWriteMask[i] = ColorWriteMask_All;
        }
    }

    // 0x00C5A150 (graphicsstateobjects.hpp:210)
    uint32 BlendParametersEx::getHash() const
    {
        return getCrc32Value(this, sizeof(*this));
    }

    // 0x004DB4C0 (graphicsstateobjects.hpp:227)
    RasterizerParameters::RasterizerParameters()
    {
        cullMode = CullMode_Back;
        fillMode = FillMode_Solid;
        windingOrder = WindingOrder_Ccw;
        flags = RasterizerFlagMask_Default;
        depthBias = 0.0f;
        depthBiasSlopeScale = 0.0f;
        depthBiasClamp = 0.0f;
    }

    // 0x00C5A160 (graphicsstateobjects.hpp:239)
    uint32 RasterizerParameters::getHash() const
    {
        return getCrc32Value(this, sizeof(*this));
    }

    // 0x004DB500 (graphicsstateobjects.hpp:254)
    SamplerParameters::SamplerParameters()
    {
        // The padding is hashed with the rest, so the whole description is cleared first.
        memset(this, 0, sizeof(*this));
        mipLodBias = 0.0f;
        minLod = 0.0f;
        // Magic number: default maximum LOD.
        maxLod = 1000.0f;
        maxAnisotropyLevel = 0;
        minFilter = SamplerFilterMode_Linear;
        magFilter = SamplerFilterMode_Linear;
        mipFilter = SamplerFilterMode_Nearest;
        addressU = SamplerAddressMode_Wrap;
        addressV = SamplerAddressMode_Wrap;
        addressW = SamplerAddressMode_Wrap;
        comparisonFunction = ComparisonFunction_Never;
        alphaKillEnabled = false;
    }

    // 0x00C5A170 (graphicsstateobjects.hpp:272)
    uint32 SamplerParameters::getHash() const
    {
        return getCrc32Value(this, sizeof(*this));
    }

    // 0x004DB550 (graphicsstateobjects.hpp:293)
    DepthStencilParameters::DepthStencilParameters()
    {
        depthComparisonFunction = ComparisonFunction_Less;
        depthTestEnabled = true;
        depthWriteEnabled = true;
    }

    // 0x00C5A180 (graphicsstateobjects.hpp:301)
    uint32 DepthStencilParameters::getHash() const
    {
        return getCrc32Value(this, sizeof(*this));
    }

    // 0x004B2190 (graphicsstateobjects.hpp:308)
    DepthStencilParameters::StencilParameters::StencilParameters()
    {
        testEnabled = false;
        testFunc = ComparisonFunction_Always;
        // Magic number: all stencil bits are tested, none are written.
        testMask = 0xff;
        writeMask = 0;
        opFail = StencilOperation_Keep;
        opDepthFail = StencilOperation_Keep;
        opDepthPass = StencilOperation_Keep;
    }

    // 0x004B2340 (graphicssystem.hpp:53)
    ClearSettings::ClearSettings()
    {
    }

    // 0x004B2350 (graphicssystem.hpp:54)
    ClearSettings::ClearSettings(uint32 inColor, float inDepth, uint8 inStencilValue, uint32 inClearMask)
    {
        color = inColor;
        depth = inDepth;
        stencilValue = inStencilValue;
        clearMask = inClearMask;
    }

    // 0x00456D00 (graphicssystem.hpp:221)
    ShaderPipelineParameters::ShaderPipelineParameters(const VertexShader* pVertexShader, const FragmentShader* pFragmentShader)
    {
        this->pVertexShader = pVertexShader;
        this->pFragmentShader = pFragmentShader;
    }

    // 0x00C706F0 (:13)
    void createGraphicsSystemBase(GraphicsSystemBase* pGraphicsSystemBase, MemoryAllocator* pSystemAllocator, FileSystem* pShaderFileSystem, MemoryBlock shaderObjectMemoryBlock, MemoryBlock shaderCodeMemoryBlock, const CombinedGraphicsStateObjectPoolParameters& poolParameters)
    {
        pGraphicsSystemBase->pSystemAllocator = pSystemAllocator;
        // GraphicsSystem derives from GraphicsSystemBase at offset 0.
        pGraphicsSystemBase->pShaderFactory = graphics::createShaderFactory(pSystemAllocator, (GraphicsSystem*)pGraphicsSystemBase, pShaderFileSystem, shaderObjectMemoryBlock, shaderCodeMemoryBlock);
        pGraphicsSystemBase->stateObjectPool.create(pGraphicsSystemBase->pSystemAllocator, poolParameters);
    }

    // 0x00C73230 (:24)
    void createGraphicsSystemBaseObjects(GraphicsSystemBase* pGraphicsSystemBase, MemoryAllocator* pTextureAllocator)
    {
        pGraphicsSystemBase->stockObjects.create((GraphicsSystem*)pGraphicsSystemBase, pTextureAllocator);
    }

    // 0x00C72BD0 (:30)
    void destroyGraphicsSystemBase(GraphicsSystemBase* pGraphicsSystemBase, MemoryAllocator* pTextureAllocator)
    {
        pGraphicsSystemBase->stockObjects.destroy((GraphicsSystem*)pGraphicsSystemBase, pTextureAllocator);
        pGraphicsSystemBase->stateObjectPool.destroy(pGraphicsSystemBase->pSystemAllocator);
        graphics::destroyShaderFactory(pGraphicsSystemBase->pShaderFactory, pGraphicsSystemBase->pSystemAllocator);
    }

    namespace graphics
    {
        // 0x004B2380 (graphicssystem.hpp:269)
        uint32 getBackBufferWidth(const GraphicsSystem* pGraphicsSystem)
        {
            return getRenderTargetWidth(getBackBufferRenderTarget(pGraphicsSystem));
        }

        // 0x004B23A0 (graphicssystem.hpp:270)
        uint32 getBackBufferHeight(const GraphicsSystem* pGraphicsSystem)
        {
            return getRenderTargetHeight(getBackBufferRenderTarget(pGraphicsSystem));
        }

        // 0x004DB660 (graphicssystem.hpp:324)
        const RasterizerState* createRasterizerState(GraphicsSystem* pGraphicsSystem, CullMode cullMode, FillMode fillMode, WindingOrder windingOrder, uint32 flags)
        {
            RasterizerParameters parameters;
            parameters.cullMode = cullMode;
            parameters.fillMode = fillMode;
            parameters.windingOrder = windingOrder;
            parameters.flags = flags;
            return createRasterizerState(pGraphicsSystem, parameters);
        }

        // 0x004DB6D0 (graphicssystem.hpp:334)
        const BlendState* createBlendState(GraphicsSystem* pGraphicsSystem, BlendOperation blendOp, BlendFactor sourceFactor, BlendFactor destFactor, uint8 colorWriteMask0)
        {
            BlendParametersEx parameters;
            parameters.blendOpColor = blendOp;
            parameters.sourceFactorColor = sourceFactor;
            parameters.destFactorColor = destFactor;
            parameters.blendOpAlpha = blendOp;
            parameters.sourceFactorAlpha = sourceFactor;
            parameters.destFactorAlpha = destFactor;
            parameters.colorWriteMask[0] = colorWriteMask0;
            return createBlendState(pGraphicsSystem, parameters);
        }

        // 0x004DB760 (graphicssystem.hpp:369)
        const SamplerState* createSamplerState(GraphicsSystem* pGraphicsSystem, SamplerAddressMode addressU, SamplerAddressMode addressV, SamplerAddressMode addressW, SamplerFilterMode minFilter, SamplerFilterMode magFilter, SamplerFilterMode mipFilter)
        {
            SamplerParameters parameters;
            parameters.addressU = addressU;
            parameters.addressV = addressV;
            parameters.addressW = addressW;
            parameters.minFilter = minFilter;
            parameters.magFilter = magFilter;
            parameters.mipFilter = mipFilter;
            return createSamplerState(pGraphicsSystem, parameters);
        }

        // 0x004DB7E0 (graphicssystem.hpp:381)
        const DepthStencilState* createDepthStencilState(GraphicsSystem* pGraphicsSystem, bool enableDepthTest, ComparisonFunction depthCompareFunction, bool enableDepthWrite)
        {
            DepthStencilParameters parameters;
            parameters.depthComparisonFunction = depthCompareFunction;
            parameters.depthTestEnabled = enableDepthTest;
            parameters.depthWriteEnabled = enableDepthWrite;
            return createDepthStencilState(pGraphicsSystem, parameters);
        }

        // 0x00C6C450 (:39)
        const FragmentShader* createFragmentShaderFromRetainedMemoryBlock(GraphicsSystem* pGraphicsSystem, MemoryBlock memBlock)
        {
            return createFragmentShaderFromRetainedMemoryBlock(pGraphicsSystem->pShaderFactory, memBlock);
        }

        // 0x00C6F700 (:46)
        const FragmentShader* createFragmentShaderFromFile(GraphicsSystem* pGraphicsSystem, const char* pFileName, const char* pDebugName)
        {
            if (pDebugName == nullptr)
            {
                pDebugName = pFileName;
            }
            return createFragmentShaderFromFile(pGraphicsSystem->pShaderFactory, pFileName, pDebugName);
        }

        // 0x00C6C470 (:66)
        void destroyFragmentShader(GraphicsSystem* pGraphicsSystem, const FragmentShader* pFragmentShader)
        {
            destroyFragmentShader(pGraphicsSystem->pShaderFactory, pFragmentShader);
        }

        // 0x00C6F730 (:73)
        const VertexShader* createVertexShaderFromRetainedMemoryBlock(GraphicsSystem* pGraphicsSystem, MemoryBlock memBlock)
        {
            return createVertexShaderFromRetainedMemoryBlock(pGraphicsSystem->pShaderFactory, memBlock);
        }

        // 0x00C6F750 (:80)
        const VertexShader* createVertexShaderFromFile(GraphicsSystem* pGraphicsSystem, const char* pFileName, const char* pDebugName)
        {
            if (pDebugName == nullptr)
            {
                pDebugName = pFileName;
            }
            return createVertexShaderFromFile(pGraphicsSystem->pShaderFactory, pFileName, pDebugName);
        }

        // 0x00C6F780 (:101)
        void destroyVertexShader(GraphicsSystem* pGraphicsSystem, const VertexShader* pVertexShader)
        {
            destroyVertexShader(pGraphicsSystem->pShaderFactory, pVertexShader);
        }

        // 0x00C5D110 (:108)
        const VertexFormat* getStockVertexFormat(const GraphicsSystem* pGraphicsSystem, GraphicsStockVertexFormat formatId)
        {
            return pGraphicsSystem->stockObjects.getFormat(formatId);
        }

        // 0x00C5D120 (:115)
        const ShaderPipeline* getStockShaderPipeline(GraphicsSystem* pGraphicsSystem, GraphicsStockShaderPipelineId pipelineId)
        {
            return pGraphicsSystem->stockObjects.getShaderPipeline(pipelineId);
        }

        // 0x00C5D130 (:122)
        const TextureData* getStockTexture(const GraphicsSystem* pGraphicsSystem, GraphicsStockTextureId textureId)
        {
            return pGraphicsSystem->stockObjects.getTexture(textureId);
        }
    }
}
