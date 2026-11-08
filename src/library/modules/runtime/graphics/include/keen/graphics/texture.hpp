// Original: library/modules/runtime/graphics/include/keen/graphics/texture.hpp
// Implementation: library/modules/runtime/graphics/sources/global/texture.cpp
//
// Texture, the texture resource (a Resource holding TextureData), and TextureHandleType (the ResourceHandle tag).
// Placement of TextureHandleType inferred.
#pragma once

#include "keen/base/types.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/resources/resource.hpp"

namespace keen
{
    struct GraphicsSystem;
    class MemoryAllocator;
    struct TextureDescription;
    struct TextureInitializationData;

    struct TextureHandleType; // no definition in PDB

    // size 0x28
    class Texture : public Resource
    {
    public:
        enum
        {
            ResourceClassType = 0x52545854,
        };

    private:
        Texture(const Texture&);
        Texture(uint32 key); // 0x00C60300 (:22)
    public:
        Texture();
        const TextureData* getData() const; // 0x00CC8400 (.h:49)
        TextureData* getData();
        uint32 getWidth() const;
        uint32 getHeight() const;
    protected:
        bool initialize(const TextureInitializationData* pInitializationData, GraphicsSystem* pGraphicsSystem, uint32 contentFormatKey); // 0x00C6D1C0 (:31)
        void destroy(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTexMapAllocator); // 0x00C60340 (:94)
        void createDummy(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTexMapAllocator); // 0x00C6D1F0 (:62)
        bool create(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTextureDataAllocator, const TextureDescription& description); // 0x00C6D190 (:26)
    private:
        ~Texture(); // 0x00C5B9E0 (.h:71)
        Texture& operator=(const Texture&);

        TextureData m_data; // 0x08
    };

    const Texture* getFromHandle(const TextureHandleType* handle); // 0x00CC8410 (.h:82)
}
