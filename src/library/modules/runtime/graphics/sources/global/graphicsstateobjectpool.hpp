// Original: library/modules/runtime/graphics/sources/global/graphicsstateobjectpool.hpp
// Implementation: library/modules/runtime/graphics/sources/global/graphicsstateobjectpool.cpp
//
// GraphicsStateObjectPool<T>, a pool plus cache per state object type, and CombinedGraphicsStateObjectPool (one pool per
// type) owned by the graphics system. GraphicsStateObjectPool<T> is written by hand; the addresses are of the
// <BlendState> instantiation.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/poolallocator.hpp"
#include "keen/graphics/vertexformat.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/direct3d_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/global/graphicsstateobjectcache.hpp"

namespace keen
{
    struct BlendParametersEx;
    struct DepthStencilParameters;
    class MemoryAllocator;
    struct RasterizerParameters;
    struct SamplerParameters;
    struct VertexAttributeDescription;
    struct VertexShader;

    // size 0x54
    template<typename T>
    class GraphicsStateObjectPool
    {
    public:
        GraphicsStateObjectPool(); // <BlendState> 0x00C6D490 (.h:26)
        void create(MemoryAllocator* pAllocator, uint32 capacity); // <BlendState> 0x00C6F8D0 (.h:31)
        void destroy(MemoryAllocator* pAllocator); // <BlendState> 0x00C71040 (.h:41)
        bool allocateObject(T** ppResult, uint32 hash); // <BlendState> 0x00C710A0 (.h:52)
        void addObjectReference(const T*);
        bool freeObject(const T* pObject); // <BlendState> 0x00C6F980 (.h:98)
        float getCacheHitRatio() const;
        void trace(const char* pObjectName); // <BlendState> 0x00C60600 (.h:121)
        uint32 getIndex(const T*);
        ~GraphicsStateObjectPool(); // <BlendState> 0x00C6D920

    private:
        PoolAllocator<T> m_allocator; // 0x00
        GraphicsStateObjectCache m_cache; // 0x18
        uint32 m_peakSize; // 0x48
        uint32 m_cacheHits; // 0x4C
        uint32 m_cacheRequests; // 0x50
    };

    // size 0x18
    struct CombinedGraphicsStateObjectPoolParameters
    {
        CombinedGraphicsStateObjectPoolParameters(); // 0x00C5BA20 (.h:146)

        uint32 stateObjectCapacity[6]; // 0x00
    };

    // size 0x1F8
    class CombinedGraphicsStateObjectPool
    {
    public:
        CombinedGraphicsStateObjectPool(); // 0x00C6EE00 (.h:163)
        void create(MemoryAllocator* pAllocator, const CombinedGraphicsStateObjectPoolParameters& parameters); // 0x00C70680 (:8)
        void destroy(MemoryAllocator* pAllocator); // 0x00C729D0 (:18)
        void trace(); // 0x00C64630 (:30)
        bool allocateBlendState(BlendState** ppState, const BlendParametersEx& parameters); // 0x00C71880 (.h:185)
        bool freeBlendState(const BlendState* pState); // 0x00C702C0 (.h:185)
        uint32 getIndexOfBlendState(BlendState*);
        bool allocateRasterizerState(RasterizerState** ppState, const RasterizerParameters& parameters); // 0x00C718B0 (.h:186)
        bool freeRasterizerState(const RasterizerState* pState); // 0x00C702D0 (.h:186)
        uint32 getIndexOfRasterizerState(RasterizerState*);
        bool allocateSamplerState(SamplerState** ppState, const SamplerParameters& parameters); // 0x00C718E0 (.h:187)
        bool freeSamplerState(const SamplerState* pState); // 0x00C702E0 (.h:187)
        uint32 getIndexOfSamplerState(SamplerState*);
        bool allocateDepthStencilState(DepthStencilState** ppState, const DepthStencilParameters& parameters); // 0x00C71910 (.h:188)
        bool freeDepthStencilState(const DepthStencilState* pState); // 0x00C702F0 (.h:188)
        uint32 getIndexOfDepthStencilState(DepthStencilState*);
        bool allocateVertexFormat(VertexFormat**, const VertexAttributeDescription*, uint32, const VertexAttributeDescription*, uint32);
        bool allocateVertexFormat(VertexFormat** ppVertexFormat, const VertexAttributeDescription* pAttributes, uint32 attributeCount); // 0x00C71940 (.h:193)
        bool freeVertexFormat(const VertexFormat* pVertexFormat); // 0x00C70300 (.h:216)
        void addVertexFormatReference(const VertexFormat* pVertexFormat); // 0x00C61470 (.h:220)
        bool allocateVertexInputBinding(VertexInputBinding** ppInputBinding, const VertexFormat* pVertexFormat, const VertexShader* pVertexShader, uint32 geometryModeMask); // 0x00C71A80 (.h:225)
        bool freeVertexInputBinding(const VertexInputBinding* pInputBinding); // 0x00C70310 (.h:243)
        ~CombinedGraphicsStateObjectPool(); // 0x00C6D980

    private:
        GraphicsStateObjectPool<BlendState> m_blendStatePool; // 0x00
        GraphicsStateObjectPool<RasterizerState> m_rasterizerStatePool; // 0x54
        GraphicsStateObjectPool<SamplerState> m_samplerStatePool; // 0xA8
        GraphicsStateObjectPool<DepthStencilState> m_depthStencilStatePool; // 0xFC
        GraphicsStateObjectPool<VertexFormat> m_vertexFormatPool; // 0x150
        GraphicsStateObjectPool<VertexInputBinding> m_vertexInputBindingPool; // 0x1A4
    };
}

#include "library/modules/runtime/graphics/sources/global/graphicsstateobjectpool.inl"
