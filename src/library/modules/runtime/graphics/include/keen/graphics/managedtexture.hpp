// Original: library/modules/runtime/graphics/include/keen/graphics/managedtexture.hpp
// Implementation: library/modules/runtime/graphics/sources/global/managedtexture.cpp
//
// ManagedTexture, a texture whose storage is (re)created on demand with a given description.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

namespace keen
{
    class GraphicsCommandWriter;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct TextureDescription;

    // size 0x40
    class ManagedTexture
    {
    public:
        bool create(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureDataAllocator, const TextureDescription& description, const char* pDebugName); // 0x00C6CE90 (:9)
        void destroy(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureDataAllocator); // 0x00C5DC00 (:39)
        TextureData* getGpuTextureData();
        const TextureData* getGpuTextureData() const;
        TextureData* getCpuTextureData();
        const TextureData* getCpuTextureData() const;
        void copyCpuToGpu(GraphicsCommandWriter* pCommandWriter, uint32 level); // 0x00C5DC70 (:45)
        void copyGpuToCpu(GraphicsCommandWriter* pCommandWriter, uint32 level); // 0x00C5DCA0 (:50)
        uint32 getWidth() const;
        uint32 getHeight() const;

    private:
        TextureData m_gpuTexture; // 0x00
        TextureData m_cpuTexture; // 0x20
    };
}
