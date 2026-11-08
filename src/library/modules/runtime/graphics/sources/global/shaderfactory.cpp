// Original: library/modules/runtime/graphics/sources/global/shaderfactory.cpp
//
// ShaderFactory: loads compiled shaders from the shader file system (paths are prefixed with "/shader") into shader
// objects allocated from a TLSF heap over the caller's shader object memory. A 1 MB scratch heap holds file contents
// while a shader is created. Creating shaders from retained memory blocks is not supported on D3D11: those functions
// allocate an object, fail and free it again.

#include "library/modules/runtime/graphics/sources/global/shaderfactory.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/fragmentshader_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.hpp"
#include "keen/base/file.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/trace.hpp"

#include <new>

namespace keen
{
    // Magic number: creation flags of the shader heaps (0x27, as passed by the shipped build).
    static const uint32 s_shaderAllocatorFlags = 0x27;

    // 0x00C6E5F0
    ShaderFactory::ShaderFactory()
    {
    }

    // 0x00C6A3B0
    ShaderFactory::~ShaderFactory()
    {
    }

    namespace graphics
    {
        // 0x00C6F790 (:27)
        ShaderFactory* createShaderFactory(MemoryAllocator* pAllocator, GraphicsSystem* pGraphicsSystem, FileSystem* pShaderFileSystem, MemoryBlock shaderObjectMemory, MemoryBlock shaderProgramCodeMemory)
        {
            void* pMemory = pAllocator->allocate(sizeof(ShaderFactory), 4, 0, nullptr);
            ShaderFactory* pShaderFactory = pMemory != nullptr ? new (pMemory) ShaderFactory() : nullptr;
            pShaderFactory->pShaderFileSystem = pShaderFileSystem;
            pShaderFactory->pGraphicsSystem = pGraphicsSystem;

            // Magic number: 1 MB of scratch memory for shader file contents.
            MemoryBlock scratchMemory;
            scratchMemory.size = 0x100000;
            scratchMemory.pStart = (uint8*)pAllocator->allocate(scratchMemory.size, 16, 0, "ShaderScratchMemory");
            pShaderFactory->scratchAllocator.create(scratchMemory, s_shaderAllocatorFlags, "ShaderScratch");
            pShaderFactory->shaderObjectAllocator.create(shaderObjectMemory, s_shaderAllocatorFlags, "ShaderObjects");
            if (shaderProgramCodeMemory.size != 0)
            {
                pShaderFactory->shaderProgramCodeAllocator.create(shaderProgramCodeMemory, s_shaderAllocatorFlags, "ShaderProgramCode");
                pShaderFactory->pShaderProgramCodeAllocator = &pShaderFactory->shaderProgramCodeAllocator;
            }
            else
            {
                pShaderFactory->pShaderProgramCodeAllocator = nullptr;
            }
            return pShaderFactory;
        }

        // 0x00C6F850 (:60)
        void destroyShaderFactory(ShaderFactory* pShaderFactory, MemoryAllocator* pAllocator)
        {
            pShaderFactory->shaderObjectAllocator.destroy();
            if (pShaderFactory->pShaderProgramCodeAllocator != nullptr)
            {
                pShaderFactory->shaderProgramCodeAllocator.destroy();
                pShaderFactory->pShaderProgramCodeAllocator = nullptr;
            }
            const MemoryBlock scratchMemory = pShaderFactory->scratchAllocator.destroy();
            pAllocator->free(scratchMemory.pStart);
            pShaderFactory->~ShaderFactory();
            pAllocator->free(pShaderFactory);
        }

        // 0x00C66E90 (:75)
        const FragmentShader* createFragmentShaderFromRetainedMemoryBlock(ShaderFactory* pShaderFactory, MemoryBlock memBlock)
        {
            MemoryAllocator* pObjectAllocator = &pShaderFactory->shaderObjectAllocator;
            void* pMemory = pObjectAllocator->allocate(sizeof(FragmentShader), 4, 0, nullptr);
            if (pMemory == nullptr)
            {
                debug::nativePrintArgs("[graphics] Out of memory! Could not allocate another fragment shader instance!\n");
                return nullptr;
            }
            FragmentShader* pFragmentShader = new (pMemory) FragmentShader();
            // FragmentShader::createFromRetainedMemoryBlock always fails on D3D11 (inlined to nothing).
            pObjectAllocator->free(pFragmentShader);
            debug::nativePrintArgs("[graphics] Could not create fragment shader from memory block!\n");
            return nullptr;
        }

        // 0x00C6E6C0 (:99)
        const FragmentShader* createFragmentShaderFromFile(ShaderFactory* pShaderFactory, const char* pFileName, const char* pDebugName)
        {
            FilePath path;
            path.createCombinedPath("/shader", pFileName);
            File file;
            if (!file.open(pShaderFactory->pShaderFileSystem, path.getCompletePath(), FileAccessMode_Read, Endianness_Little))
            {
                debug::nativePrintArgs("[graphics] Could not load fragment shader from file '%s'!\n", pFileName);
                return nullptr;
            }
            MemoryAllocator* pObjectAllocator = &pShaderFactory->shaderObjectAllocator;
            void* pMemory = pObjectAllocator->allocate(sizeof(FragmentShader), 4, 0, nullptr);
            if (pMemory == nullptr)
            {
                debug::nativePrintArgs("[graphics] Out of memory! Could not allocate another fragment shader instance while loading shader '%s'!\n", pFileName);
                return nullptr;
            }
            FragmentShader* pFragmentShader = new (pMemory) FragmentShader();
            if (!pFragmentShader->createFromFile(&file, pShaderFactory->pGraphicsSystem, pObjectAllocator, pShaderFactory->pShaderProgramCodeAllocator, &pShaderFactory->scratchAllocator))
            {
                pObjectAllocator->free(pFragmentShader);
                debug::nativePrintArgs("[graphics] Could not load fragment shader from file '%s'!\n", pFileName);
                return nullptr;
            }
            pFragmentShader->setDebugName(pShaderFactory->pGraphicsSystem, pDebugName);
            return pFragmentShader;
        }

        // 0x00C66EF0 (:133)
        void destroyFragmentShader(ShaderFactory* pShaderFactory, const FragmentShader* pFragmentShader)
        {
            if (pFragmentShader == nullptr)
            {
                return;
            }
            FragmentShader* pShader = const_cast<FragmentShader*>(pFragmentShader);
            pShader->destroy(&pShaderFactory->shaderObjectAllocator, pShaderFactory->pShaderProgramCodeAllocator);
            pShaderFactory->shaderObjectAllocator.free(pShader);
        }

        // 0x00C6E820 (:143)
        const VertexShader* createVertexShaderFromRetainedMemoryBlock(ShaderFactory* pShaderFactory, MemoryBlock memBlock)
        {
            MemoryAllocator* pObjectAllocator = &pShaderFactory->shaderObjectAllocator;
            void* pMemory = pObjectAllocator->allocate(sizeof(VertexShader), 4, 0, nullptr);
            if (pMemory == nullptr)
            {
                debug::nativePrintArgs("[graphics] Out of memory! Could not allocate another vertex shader instance!\n");
                return nullptr;
            }
            VertexShader* pVertexShader = new (pMemory) VertexShader();
            // VertexShader::createFromRetainedMemoryBlock always fails on D3D11 (inlined to nothing).
            pVertexShader->destroy(pObjectAllocator, pShaderFactory->pShaderProgramCodeAllocator);
            pObjectAllocator->free(pVertexShader);
            debug::nativePrintArgs("[graphics] Could not create vertex shader from memory block!\n");
            return nullptr;
        }

        // 0x00C6E8A0 (:167)
        const VertexShader* createVertexShaderFromFile(ShaderFactory* pShaderFactory, const char* pFileName, const char* pDebugName)
        {
            FilePath path;
            path.createCombinedPath("/shader", pFileName);
            File file;
            if (!file.open(pShaderFactory->pShaderFileSystem, path.getCompletePath(), FileAccessMode_Read, Endianness_Little))
            {
                debug::nativePrintArgs("[graphics] Could not load vertex shader from file '%s'!\n", path.getCompletePath());
                return nullptr;
            }
            MemoryAllocator* pObjectAllocator = &pShaderFactory->shaderObjectAllocator;
            void* pMemory = pObjectAllocator->allocate(sizeof(VertexShader), 4, 0, nullptr);
            if (pMemory == nullptr)
            {
                debug::nativePrintArgs("[graphics] Out of memory! Could not allocate another vertex shader instance while loading shader '%s'!\n", path.getCompletePath());
                return nullptr;
            }
            VertexShader* pVertexShader = new (pMemory) VertexShader();
            if (!pVertexShader->createFromFile(&file, pShaderFactory->pGraphicsSystem, pObjectAllocator, pShaderFactory->pShaderProgramCodeAllocator, &pShaderFactory->scratchAllocator))
            {
                pVertexShader->destroy(pObjectAllocator, pShaderFactory->pShaderProgramCodeAllocator);
                pObjectAllocator->free(pVertexShader);
                debug::nativePrintArgs("[graphics] Could not load vertex shader from file '%s'!\n", path.getCompletePath());
                return nullptr;
            }
            pVertexShader->setDebugName(pShaderFactory->pGraphicsSystem, pDebugName);
            return pVertexShader;
        }

        // 0x00C6EA30 (:202)
        void destroyVertexShader(ShaderFactory* pShaderFactory, const VertexShader* pVertexShader)
        {
            if (pVertexShader == nullptr)
            {
                return;
            }
            VertexShader* pShader = const_cast<VertexShader*>(pVertexShader);
            pShader->destroy(&pShaderFactory->shaderObjectAllocator, pShaderFactory->pShaderProgramCodeAllocator);
            pShaderFactory->shaderObjectAllocator.free(pShader);
        }

        // 0x00C5F640 (:213)
        MemoryAllocator* getShaderCPUMemoryAllocator(ShaderFactory* pShaderFactory)
        {
            return &pShaderFactory->shaderObjectAllocator;
        }

        // 0x00C5F650 (:218)
        MemoryAllocator* getShaderGPUMemoryAllocator(ShaderFactory* pShaderFactory)
        {
            return &pShaderFactory->shaderProgramCodeAllocator;
        }
    }
}
