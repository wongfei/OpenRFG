// Original: library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.cpp
//
// VertexShader, a D3D11 vertex shader plus its input signature (for input layouts).
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3d11.h>
#include "keen/base/array.hpp"

struct ID3D11VertexShader;

namespace keen
{
    class File;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct MemoryBlock;

    // size 0x18
    struct VertexShader
    {
        VertexShader(); // 0x00C63540 (:12)
        bool createFromRetainedMemoryBlock(MemoryBlock memBlock, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pScratchAllocator); // 0x00C5CE50 (:17)
        bool createFromFile(File* pFile, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pDataAllocator, MemoryAllocator*, MemoryAllocator*); // 0x00C6C170 (:26)
        void destroy(MemoryAllocator* pDataAllocator, MemoryAllocator*); // 0x00C6C3D0 (:114)
        void setDebugName(GraphicsSystem* pGraphicsSystem, const char* pDebugName); // 0x00C5CE60 (:104)
        ~VertexShader(); // 0x00C60CA0

        Array<uint8> shaderCode; // 0x00
        ID3D11VertexShader* pVertexShader; // 0x08
        Array<_D3D11_SIGNATURE_PARAMETER_DESC> inputSignature; // 0x0C
        uint32 inputSignatureHash; // 0x14
    };
}
