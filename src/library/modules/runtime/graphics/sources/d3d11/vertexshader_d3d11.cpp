// Original: library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.cpp
//
// VertexShader on D3D11: the file contents are D3D11 shader bytecode (DXBC). The bytecode is kept for input layout
// creation, and the input signature is reflected and hashed (CRC-32 of each parameter's semantic name, semantic index,
// register, component type and mask, seeded with 42) so that vertex input bindings can be shared between shaders with
// the same signature.
//
// Built against the current Windows SDK: D3D11_SIGNATURE_PARAMETER_DESC has the later MinPrecision member (32 bytes
// instead of the 28 the shipped build allocated per parameter) and D3DReflect comes from d3dcompiler_47. The hashed
// fields are the same.

#include "library/modules/runtime/graphics/sources/d3d11/vertexshader_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/graphics/vertexshader.hpp"
#include "keen/base/crc32.hpp"
#include "keen/base/file.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

#include <d3d11shader.h>
#include <d3dcompiler.h>

namespace keen
{
    // 0x00C63540 (:12)
    VertexShader::VertexShader()
    {
        pVertexShader = nullptr;
    }

    // 0x00C60CA0
    VertexShader::~VertexShader()
    {
    }

    // 0x00C5CE50 (:17)
    bool VertexShader::createFromRetainedMemoryBlock(MemoryBlock memBlock, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pScratchAllocator)
    {
        return false;
    }

    // 0x00C6C170 (:26)
    bool VertexShader::createFromFile(File* pFile, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pDataAllocator, MemoryAllocator*, MemoryAllocator*)
    {
        if (!pFile->isOpen())
        {
            return false;
        }
        if (!shaderCode.create(pDataAllocator, (uint32)pFile->getSize(), 16))
        {
            return false;
        }
        const uint32 codeSize = shaderCode.getSize();
        if (pFile->read(shaderCode.getStart(), codeSize) != codeSize)
        {
            shaderCode.destroy(pDataAllocator);
            return false;
        }

        if (FAILED(pGraphicsSystem->pDevice->CreateVertexShader(shaderCode.getStart(), shaderCode.getSize(), nullptr, &pVertexShader)))
        {
            debug::nativePrintArgs("Could not create D3D vertex shader from shader data!\n");
            shaderCode.destroy(pDataAllocator);
            return false;
        }

        // Notes (decompiled behaviour, kept as is): the reflection interface is never released.
        ID3D11ShaderReflection* pReflection = nullptr;
        if (FAILED(D3DReflect(shaderCode.getStart(), shaderCode.getSize(), IID_ID3D11ShaderReflection, (void**)&pReflection)))
        {
            debug::nativePrintArgs("Could not get D3D vertex shader input signature from shader!\n");
            shaderCode.destroy(pDataAllocator);
            return false;
        }
        D3D11_SHADER_DESC shaderDesc;
        if (FAILED(pReflection->GetDesc(&shaderDesc)))
        {
            debug::nativePrintArgs("Could not get D3D vertex shader input signature from shader!\n");
            shaderCode.destroy(pDataAllocator);
            return false;
        }

        inputSignature.create(pDataAllocator, shaderDesc.InputParameters, 16);
        // Magic number: the signature hash is seeded with 42.
        inputSignatureHash = 42;
        for (uint32 i = 0; i < shaderDesc.InputParameters; ++i)
        {
            D3D11_SIGNATURE_PARAMETER_DESC& parameter = inputSignature[i];
            if (FAILED(pReflection->GetInputParameterDesc(i, &parameter)))
            {
                debug::nativePrintArgs("Could not get D3D vertex shader input signature from shader!\n");
                inputSignature.destroy(pDataAllocator);
                shaderCode.destroy(pDataAllocator);
                return false;
            }
            inputSignatureHash = addCrc32Value(inputSignatureHash, parameter.SemanticName);
            inputSignatureHash = addCrc32Value(inputSignatureHash, &parameter.SemanticIndex, 4);
            inputSignatureHash = addCrc32Value(inputSignatureHash, &parameter.Register, 4);
            inputSignatureHash = addCrc32Value(inputSignatureHash, &parameter.ComponentType, 4);
            inputSignatureHash = addCrc32Value(inputSignatureHash, &parameter.Mask, 1);
        }
        return true;
    }

    // 0x00C5CE60 (:104)
    void VertexShader::setDebugName(GraphicsSystem* pGraphicsSystem, const char* pDebugName)
    {
        pVertexShader->SetPrivateData(WKPDID_D3DDebugObjectName, getStringLength(pDebugName), pDebugName);
    }

    // 0x00C6C3D0 (:114)
    void VertexShader::destroy(MemoryAllocator* pDataAllocator, MemoryAllocator*)
    {
        if (pVertexShader != nullptr)
        {
            pVertexShader->Release();
            pVertexShader = nullptr;
        }
        inputSignature.destroy(pDataAllocator);
        shaderCode.destroy(pDataAllocator);
    }

    namespace graphics
    {
        // 0x00C5CE90 (:127)
        uint32 getVertexShaderInputSignatureHash(const VertexShader* pVertexShader)
        {
            return pVertexShader->inputSignatureHash;
        }
    }
}
