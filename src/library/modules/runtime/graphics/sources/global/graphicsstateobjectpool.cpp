// Original: library/modules/runtime/graphics/sources/global/graphicsstateobjectpool.cpp
//
// CombinedGraphicsStateObjectPool: one GraphicsStateObjectPool per state object type. The hash of a state description
// is the CRC-32 of its bytes; a vertex format hashes its attribute descriptions, a vertex input binding combines its
// vertex format with the vertex shader's input signature and the geometry mode mask. The members marked (.h:N) are
// inline in graphicsstateobjectpool.hpp in the original.

#include "library/modules/runtime/graphics/sources/global/graphicsstateobjectpool.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.hpp"
#include "keen/base/crc32.hpp"

namespace keen
{
    // 0x00C5BA20 (.h:146)
    CombinedGraphicsStateObjectPoolParameters::CombinedGraphicsStateObjectPoolParameters()
    {
        // Default capacities: blend, rasterizer, sampler, depth/stencil, vertex format, vertex input binding.
        stateObjectCapacity[GraphicsStateObjectType_BlendState] = 0x100;
        stateObjectCapacity[GraphicsStateObjectType_RasterizerState] = 0x40;
        stateObjectCapacity[GraphicsStateObjectType_SamplerState] = 0x20;
        stateObjectCapacity[GraphicsStateObjectType_DepthStencilState] = 0x80;
        stateObjectCapacity[GraphicsStateObjectType_VertexFormat] = 0x40;
        stateObjectCapacity[GraphicsStateObjectType_VertexInputBinding] = 0x80;
    }

    // 0x00C6EE00 (.h:163)
    CombinedGraphicsStateObjectPool::CombinedGraphicsStateObjectPool()
    {
    }

    // 0x00C6D980
    CombinedGraphicsStateObjectPool::~CombinedGraphicsStateObjectPool()
    {
    }

    // 0x00C70680 (:8)
    void CombinedGraphicsStateObjectPool::create(MemoryAllocator* pAllocator, const CombinedGraphicsStateObjectPoolParameters& parameters)
    {
        m_blendStatePool.create(pAllocator, parameters.stateObjectCapacity[GraphicsStateObjectType_BlendState]);
        m_rasterizerStatePool.create(pAllocator, parameters.stateObjectCapacity[GraphicsStateObjectType_RasterizerState]);
        m_samplerStatePool.create(pAllocator, parameters.stateObjectCapacity[GraphicsStateObjectType_SamplerState]);
        m_depthStencilStatePool.create(pAllocator, parameters.stateObjectCapacity[GraphicsStateObjectType_DepthStencilState]);
        m_vertexFormatPool.create(pAllocator, parameters.stateObjectCapacity[GraphicsStateObjectType_VertexFormat]);
        m_vertexInputBindingPool.create(pAllocator, parameters.stateObjectCapacity[GraphicsStateObjectType_VertexInputBinding]);
    }

    // 0x00C729D0 (:18)
    void CombinedGraphicsStateObjectPool::destroy(MemoryAllocator* pAllocator)
    {
        m_blendStatePool.destroy(pAllocator);
        m_rasterizerStatePool.destroy(pAllocator);
        m_samplerStatePool.destroy(pAllocator);
        m_depthStencilStatePool.destroy(pAllocator);
        m_vertexFormatPool.destroy(pAllocator);
        m_vertexInputBindingPool.destroy(pAllocator);
    }

    // 0x00C64630 (:30)
    void CombinedGraphicsStateObjectPool::trace()
    {
        // Empty in the shipped build.
    }

    // 0x00C71880 (.h:185)
    bool CombinedGraphicsStateObjectPool::allocateBlendState(BlendState** ppState, const BlendParametersEx& parameters)
    {
        return m_blendStatePool.allocateObject(ppState, getCrc32Value(&parameters, sizeof(parameters)));
    }

    // 0x00C702C0 (.h:185)
    bool CombinedGraphicsStateObjectPool::freeBlendState(const BlendState* pState)
    {
        return m_blendStatePool.freeObject(pState);
    }

    // 0x00C718B0 (.h:186)
    bool CombinedGraphicsStateObjectPool::allocateRasterizerState(RasterizerState** ppState, const RasterizerParameters& parameters)
    {
        return m_rasterizerStatePool.allocateObject(ppState, getCrc32Value(&parameters, sizeof(parameters)));
    }

    // 0x00C702D0 (.h:186)
    bool CombinedGraphicsStateObjectPool::freeRasterizerState(const RasterizerState* pState)
    {
        return m_rasterizerStatePool.freeObject(pState);
    }

    // 0x00C718E0 (.h:187)
    bool CombinedGraphicsStateObjectPool::allocateSamplerState(SamplerState** ppState, const SamplerParameters& parameters)
    {
        return m_samplerStatePool.allocateObject(ppState, getCrc32Value(&parameters, sizeof(parameters)));
    }

    // 0x00C702E0 (.h:187)
    bool CombinedGraphicsStateObjectPool::freeSamplerState(const SamplerState* pState)
    {
        return m_samplerStatePool.freeObject(pState);
    }

    // 0x00C71910 (.h:188)
    bool CombinedGraphicsStateObjectPool::allocateDepthStencilState(DepthStencilState** ppState, const DepthStencilParameters& parameters)
    {
        return m_depthStencilStatePool.allocateObject(ppState, getCrc32Value(&parameters, sizeof(parameters)));
    }

    // 0x00C702F0 (.h:188)
    bool CombinedGraphicsStateObjectPool::freeDepthStencilState(const DepthStencilState* pState)
    {
        return m_depthStencilStatePool.freeObject(pState);
    }

    // 0x00C71940 (.h:193)
    bool CombinedGraphicsStateObjectPool::allocateVertexFormat(VertexFormat** ppVertexFormat, const VertexAttributeDescription* pAttributes, uint32 attributeCount)
    {
        uint32 hash = 0;
        for (uint32 i = 0; i < attributeCount; ++i)
        {
            hash = addCrc32Value(hash, &pAttributes[i], sizeof(VertexAttributeDescription));
        }
        return m_vertexFormatPool.allocateObject(ppVertexFormat, hash);
    }

    // 0x00C70300 (.h:216)
    bool CombinedGraphicsStateObjectPool::freeVertexFormat(const VertexFormat* pVertexFormat)
    {
        return m_vertexFormatPool.freeObject(pVertexFormat);
    }

    // 0x00C61470 (.h:220)
    void CombinedGraphicsStateObjectPool::addVertexFormatReference(const VertexFormat* pVertexFormat)
    {
        m_vertexFormatPool.addObjectReference(pVertexFormat);
    }

    // 0x00C71A80 (.h:225)
    bool CombinedGraphicsStateObjectPool::allocateVertexInputBinding(VertexInputBinding** ppInputBinding, const VertexFormat* pVertexFormat, const VertexShader* pVertexShader, uint32 geometryModeMask)
    {
        const uint32 hash = addCrc32Value(pVertexFormat->hash ^ pVertexShader->inputSignatureHash, &geometryModeMask, sizeof(geometryModeMask));
        return m_vertexInputBindingPool.allocateObject(ppInputBinding, hash);
    }

    // 0x00C70310 (.h:243)
    bool CombinedGraphicsStateObjectPool::freeVertexInputBinding(const VertexInputBinding* pInputBinding)
    {
        return m_vertexInputBindingPool.freeObject(pInputBinding);
    }
}
