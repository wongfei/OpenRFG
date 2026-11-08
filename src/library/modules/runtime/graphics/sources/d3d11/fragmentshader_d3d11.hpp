// Original: library/modules/runtime/graphics/sources/d3d11/fragmentshader_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/fragmentshader_d3d11.cpp
//
// FragmentShader, a D3D11 pixel shader.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>

struct ID3D11PixelShader;

namespace keen
{
    class File;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct MemoryBlock;

    // size 0x4
    struct FragmentShader
    {
        FragmentShader(); // 0x00C5B080 (:12)
        bool createFromRetainedMemoryBlock(MemoryBlock memBlock, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pScratchAllocator); // 0x00C5B090 (:17)
        bool createFromFile(File* pFile, GraphicsSystem* pGraphicsSystem, MemoryAllocator*, MemoryAllocator*, MemoryAllocator* pScratchAllocator); // 0x00C6D830 (:26)
        void destroy(MemoryAllocator*, MemoryAllocator*); // 0x00C5B0D0 (:67)
        void setDebugName(GraphicsSystem* pGraphicsSystem, const char* pDebugName); // 0x00C5B0A0 (:61)

        ID3D11PixelShader* pPixelShader; // 0x00
    };
}
