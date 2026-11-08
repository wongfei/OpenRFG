// Original: library/modules/runtime/graphics/sources/global/stockobjects.hpp
// Implementation: library/modules/runtime/graphics/sources/global/stockobjects.cpp
//
// StockObjects, the built-in shaders, vertex formats and textures every graphics system creates (screen quads, the
// default white texture). The id enums are in graphicssystem.hpp. Placement of the stock vertex types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

namespace keen
{
    struct FragmentShader;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct VertexFormat;
    struct VertexShader;

    // size 0x8
    struct StockVertexPos2f
    {
        float2 position; // 0x00
    };

    // size 0x10
    struct StockVertexPos2fTex2f
    {
        float2 position; // 0x00
        float2 texCoord0; // 0x08
    };

    // size 0x58
    class StockObjects
    {
    public:
        StockObjects(); // 0x00C602B0 (:8)
        void create(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureAllocator); // 0x00C72C10 (:24)
        void destroy(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureAllocator); // 0x00C70DF0 (:131)
        const ShaderPipeline* getShaderPipeline(GraphicsStockShaderPipelineId id); // 0x00C5B9F0 (.h:18)
        const VertexShader* getVertexShader(GraphicsStockVertexShaderId) const;
        const FragmentShader* getFragmentShader(GraphicsStockFragmentShaderId) const;
        const VertexFormat* getFormat(GraphicsStockVertexFormat id) const; // 0x00C5BA00 (.h:22)
        const TextureData* getTexture(GraphicsStockTextureId id) const; // 0x00C5BA10 (.h:23)

    private:
        ShaderPipeline m_shaderPipelines[3]; // 0x00
        const VertexShader* m_pStockVertexShaders[2]; // 0x18
        const FragmentShader* m_pStockFragmentShaders[3]; // 0x20
        const VertexFormat* m_pFormats[3]; // 0x2C
        TextureData m_textures[1]; // 0x38
    };
}
