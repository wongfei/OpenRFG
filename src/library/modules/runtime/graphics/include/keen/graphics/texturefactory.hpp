// Original: library/modules/runtime/graphics/include/keen/graphics/texturefactory.hpp
// Implementation: library/modules/runtime/graphics/sources/global/texturefactory.cpp
//
// TextureFactory, the ResourceFactory for textures; TextureCreationContext holds its allocators and graphics system.
// Placement of TextureCreationContext inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/resources/resourcefactory.hpp"

namespace keen
{
    struct GraphicsSystem;
    class MemoryAllocator;
    class Resource;
    class Texture;
    struct TextureDescription;

    // size 0xC
    struct TextureCreationContext
    {
        TextureCreationContext();

        MemoryAllocator* pObjectAllocator; // 0x00
        MemoryAllocator* pTexMapAllocator; // 0x04
        GraphicsSystem* pGraphicsSystem; // 0x08
    };

    // size 0x10
    class TextureFactory : public ResourceFactory
    {
    public:
        TextureFactory(); // 0x00C60380 (:13)
        void create(GraphicsSystem* pGraphicsSystem, MemoryAllocator* pEmergencyAllocator); // 0x00C603B0 (:18)
        void destroy(); // 0x00C603D0 (:26)
        virtual uint32 getType() const; // vtbl+0x04 0x00C603E0 (:34)
        virtual Resource* createResource(MemoryAllocator* pAllocator, uint32 key); // vtbl+0x08 0x00C69900 (:39)
        virtual void destroyResource(MemoryAllocator* pAllocator, Resource* pResource); // vtbl+0x0C 0x00C6D2F0 (:45)
        virtual bool createEmergencyResource(Resource* pResource, const char* pFileName); // vtbl+0x20 0x00C6D3B0 (:65)
        virtual bool initializeResource(Resource* pResource, uint32 contentFormatKey, const void* pInitializationData, uint32 initializationDataSize); // vtbl+0x14 0x00C6D3D0 (:80)
        virtual void shutdownResource(Resource* pResource); // vtbl+0x1C 0x00C603F0 (:104)
        Texture* createTexture(MemoryAllocator* pAllocator, MemoryAllocator* pTextureDataAllocator, const TextureDescription& description); // 0x00C6D310 (:50)
        virtual ~TextureFactory(); // vtbl+0x00 0x00C603A0

    private:
        GraphicsSystem* m_pGraphicsSystem; // 0x08
        MemoryAllocator* m_pEmergencyAllocator; // 0x0C
    };
}
