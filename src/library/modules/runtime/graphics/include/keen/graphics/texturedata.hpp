// Original: library/modules/runtime/graphics/include/keen/graphics/texturedata.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.cpp
//
// Texture creation data (initial level data) and the texture functions of the backend (create, destroy, update). The
// D3D11 TextureData is defined in sources/d3d11/texturedata_d3d11.hpp. Placement of the types inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/texturedescription.hpp"

enum DXGI_FORMAT;

namespace keen
{
    class GraphicsCommandWriter;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct TextureData;

    // size 0xC
    struct InitialTextureLevelData
    {
        const void* pData; // 0x00
        uint32 rowPitch; // 0x04
        uint32 slicePitch; // 0x08
    };

    // size 0x18
    struct TextureInitializationData
    {
        TextureDescription description; // 0x00
        void* pFirstLevelData; // 0x14
    };

    namespace graphics
    {
        bool allocateTextureData(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTexMapAllocator, const TextureDescription& description, const InitialTextureLevelData* pInitialLevelData, uint32 initialLevelDataCount, const char* pDebugName); // 0x00C6B750 (texturedata_d3d11.cpp:99)
        void freeTextureData(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTexMapAllocator); // 0x00C5CCD0 (texturedata_d3d11.cpp:250)
        bool initializeTextureData(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, const TextureDescription& description, const void* pTextureData, const char* pDebugName); // 0x00C6BD20 (texturedata_d3d11.cpp:272)
        bool createTextureShaderView(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, const TextureDescription& description, const char* pDebugName); // 0x00C62F30 (texturedata_d3d11.cpp:408)
        DXGI_FORMAT getD3dPixelFormat(uint32 format, bool useTypeLessFormats); // 0x00C5CD10 (texturedata_d3d11.cpp:526)
        void clearTextureData(TextureData* pTexture, GraphicsCommandWriter* pCommandWriter); // 0x00C69850 (:106)
    }
}
