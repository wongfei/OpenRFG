// Original: library/modules/runtime/graphics/sources/global/shaderfactory.hpp
// Implementation: library/modules/runtime/graphics/sources/global/shaderfactory.cpp
//
// ShaderFactory, creates and caches vertex/fragment shaders from compiled shader blobs.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/tlsfmemoryallocator.hpp"

namespace keen
{
    struct FileSystem;
    struct FragmentShader;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct MemoryBlock;
    struct VertexShader;

    // size 0x2AC
    struct ShaderFactory
    {
        ShaderFactory(); // 0x00C6E5F0
        ~ShaderFactory(); // 0x00C6A3B0

        FileSystem* pShaderFileSystem; // 0x00
        GraphicsSystem* pGraphicsSystem; // 0x04
        TlsfMemoryAllocator scratchAllocator; // 0x08
        TlsfMemoryAllocator shaderObjectAllocator; // 0xE8
        TlsfMemoryAllocator shaderProgramCodeAllocator; // 0x1C8
        MemoryAllocator* pShaderProgramCodeAllocator; // 0x2A8
    };

    namespace graphics
    {
        ShaderFactory* createShaderFactory(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, FileSystem* pShaderFileSystem, MemoryBlock shaderObjectMemory, MemoryBlock shaderProgramCodeMemory); // 0x00C6F790 (:27)
        void destroyShaderFactory(ShaderFactory* pShaderFactory, MemoryAllocator* pAllocator); // 0x00C6F850 (:60)
        const FragmentShader* createFragmentShaderFromRetainedMemoryBlock(ShaderFactory* pShaderFactory, MemoryBlock memBlock); // 0x00C66E90 (:75)
        const FragmentShader* createFragmentShaderFromFile(ShaderFactory* pShaderFactory, const char* pFileName, const char* pDebugName); // 0x00C6E6C0 (:99)
        void destroyFragmentShader(ShaderFactory* pShaderFactory, const FragmentShader* pFragmentShader); // 0x00C66EF0 (:133)
        const VertexShader* createVertexShaderFromRetainedMemoryBlock(ShaderFactory* pShaderFactory, MemoryBlock memBlock); // 0x00C6E820 (:143)
        const VertexShader* createVertexShaderFromFile(ShaderFactory* pShaderFactory, const char* pFileName, const char* pDebugName); // 0x00C6E8A0 (:167)
        void destroyVertexShader(ShaderFactory* pShaderFactory, const VertexShader* pVertexShader); // 0x00C6EA30 (:202)
        MemoryAllocator* getShaderCPUMemoryAllocator(ShaderFactory* pShaderFactory); // 0x00C5F640 (:213)
        MemoryAllocator* getShaderGPUMemoryAllocator(ShaderFactory* pShaderFactory); // 0x00C5F650 (:218)
    }
}
