// Original: library/modules/runtime/rendereffect/include/keen/rendereffect/fallbackrendereffect.hpp
// Implementation: library/modules/runtime/rendereffect/sources/global/fallbackrendereffect.cpp
//
// FallbackRenderEffect, the default effect for materials without a registered effect: simple textured/colored
// rendering with configurable fill, cull, blend and display mode (FallbackRenderEffect::DisplayMode). Placement of the
// context and nested types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "game/build/win32/steam/master/shader/fallback_sv.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/rendereffect/rendereffect.hpp"

namespace keen
{
    struct BlendState;
    struct DepthStencilState;
    struct DynamicConstantBuffer;
    class GraphicsCommandWriter;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct RasterizerState;
    struct RenderCommands;
    struct RenderContext;
    struct RenderEffectSystem;
    struct SamplerState;
    struct TextureHandleType;
    struct VertexFormat;
    struct VertexInputBinding;

    namespace FallbackRenderEffect
    {
        enum DisplayMode
        {
            DisplayMode_Modulate = 0,
            DisplayMode_ShowVertexColor = 1,
            DisplayMode_Count = 2,
        };

        // size 0x14
        struct MaterialData : public BaseMaterialData
        {
            enum
            {
                TypeSignatureCrc = 0x3B33F439,
            };

            static uint32 getAlignment();

            const TextureHandleType* materialTexture; // 0x08
            const TextureHandleType* normalTexture; // 0x0C
            bool normalMapEnabled; // 0x10
        };

        // size 0x14
        struct VertexData
        {
            enum
            {
                TypeSignatureCrc = -1828107067,
            };

            static uint32 getAlignment();

            float3 position; // 0x00
            uint32 texCoord0; // 0x0C
            uint32 color0; // 0x10
        };

        // size 0x30
        struct NormalMapVertexData
        {
            enum
            {
                TypeSignatureCrc = -1616802766,
            };

            static uint32 getAlignment();

            float3 position; // 0x00
            float3 normal; // 0x0C
            float4 tangentflip; // 0x18
            uint32 texCoord0; // 0x28
            uint32 color0; // 0x2C
        };

    }

    // size 0xF0
    struct FallbackRenderEffectContext
    {
        // size 0x8
        struct Binding
        {
            const VertexFormat* pVertexFormat; // 0x00
            const VertexInputBinding* pInputBinding; // 0x04
        };

        FallbackRenderEffectContext(); // 0x00CC92A0
        ~FallbackRenderEffectContext(); // 0x00CC8DC0

        const RasterizerState* pRasterizerStates[6]; // 0x00
        const BlendState* pBlendStates[3]; // 0x18
        const DepthStencilState* pDepthStencilState; // 0x24
        const SamplerState* pSamplerState; // 0x28
        FillMode fillMode; // 0x2C
        CullMode cullMode; // 0x30
        BlendMode blendMode; // 0x34
        FallbackRenderEffect::DisplayMode displayMode; // 0x38
        DynamicConstantBuffer* pInstanceDataBuffer; // 0x3C
        FallbackVariants shaderVariants; // 0x40
        GraphicsSystem* pGraphicsSystem; // 0xBC
        TextureData fallbackTextureData; // 0xC0
        const SamplerState* pFallbackSamplerState; // 0xE0
        SizedArray<Binding> inputBindings; // 0xE4
    };

    namespace FallbackRenderEffect
    {
        FallbackRenderEffectContext* createContext(RenderEffectSystem* pRenderEffectSystem, MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureDataAllocator); // 0x00CCC710 (:81)
        void destroyContext(FallbackRenderEffectContext* pEffectContext, RenderEffectSystem* pRenderEffectSystem, MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureDataAllocator); // 0x00CCBE60 (:157)
        uint32 getEffectId(); // 0x00CC84F0 (:193)
        void setFillMode(FallbackRenderEffectContext* pEffectContext, FillMode fillMode); // 0x00CC8500 (:198)
        void setCullMode(FallbackRenderEffectContext* pEffectContext, CullMode cullMode); // 0x00CC8510 (:203)
        void setBlendMode(FallbackRenderEffectContext* pEffectContext, BlendMode mode); // 0x00CC8520 (:208)
        void setDisplayMode(FallbackRenderEffectContext* pEffectContext, DisplayMode mode); // 0x00CC8530 (:213)
        void executeRenderCommands(GraphicsCommandWriter* pCommandWriter, const RenderCommands& commands, const RenderContext& renderContext, void* pEffectData); // 0x00CCAE60 (:236)
    }
}
