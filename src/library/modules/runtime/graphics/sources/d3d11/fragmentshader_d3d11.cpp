// Original: library/modules/runtime/graphics/sources/d3d11/fragmentshader_d3d11.cpp
//
// FragmentShader on D3D11: a pixel shader created from the DXBC file contents, which are read into the scratch heap
// and dropped once the shader exists.

#include "library/modules/runtime/graphics/sources/d3d11/fragmentshader_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/base/autoarray.hpp"
#include "keen/base/file.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"

namespace keen
{
    // 0x00C5B080 (:12)
    FragmentShader::FragmentShader()
    {
        pPixelShader = nullptr;
    }

    // 0x00C5B090 (:17)
    bool FragmentShader::createFromRetainedMemoryBlock(MemoryBlock memBlock, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pScratchAllocator)
    {
        return false;
    }

    // 0x00C6D830 (:26)
    bool FragmentShader::createFromFile(File* pFile, GraphicsSystem* pGraphicsSystem, MemoryAllocator*, MemoryAllocator*, MemoryAllocator* pScratchAllocator)
    {
        if (!pFile->isOpen())
        {
            return false;
        }
        AutoArray<uint8> shaderCode;
        if (!shaderCode.create(pScratchAllocator, (uint32)pFile->getSize(), 16))
        {
            return false;
        }
        const uint32 codeSize = shaderCode.getSize();
        if (pFile->read(shaderCode.getStart(), codeSize) != codeSize)
        {
            return false;
        }
        if (FAILED(pGraphicsSystem->pDevice->CreatePixelShader(shaderCode.getStart(), codeSize, nullptr, &pPixelShader)))
        {
            debug::nativePrintArgs("[graphics] Could not create D3D pixel shader from shader data!\n");
            return false;
        }
        return true;
    }

    // 0x00C5B0A0 (:61)
    void FragmentShader::setDebugName(GraphicsSystem* pGraphicsSystem, const char* pDebugName)
    {
        pPixelShader->SetPrivateData(WKPDID_D3DDebugObjectName, getStringLength(pDebugName), pDebugName);
    }

    // 0x00C5B0D0 (:67)
    void FragmentShader::destroy(MemoryAllocator*, MemoryAllocator*)
    {
        if (pPixelShader != nullptr)
        {
            pPixelShader->Release();
            pPixelShader = nullptr;
        }
    }
}
