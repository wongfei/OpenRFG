// Original: library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.cpp
//
// TextureData on D3D11: 1D, 2D, 3D and cube textures with an optional shader resource view. Textures bound as render
// targets with the aliasable flag use typeless formats so that views can reinterpret them. Every created texture is
// counted in g_allocatedResources. The TextureData members marked (.h:N) are inline in texturedata_d3d11.hpp in the
// original.

#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicssystem_d3d11.hpp"
#include "keen/graphics/texturedata.hpp"
#include "keen/graphics/pixelformat.hpp"
#include "keen/base/atomic.hpp"
#include "keen/base/trace.hpp"

#include <stdlib.h>
#include <string.h>

namespace keen
{
    // 0x004B2070 (.h:39)
    TextureData::TextureData()
    {
        pTexture = nullptr;
        pShaderView = nullptr;
    }

    // 0x004B20A0 (.h:46)
    uint32 TextureData::getWidth() const
    {
        return description.width;
    }

    // 0x004B20B0 (.h:47)
    uint32 TextureData::getHeight() const
    {
        return description.height;
    }

    // 0x00C5B0F0 (.h:48)
    uint32 TextureData::getDepth() const
    {
        return description.depth;
    }

    // 0x004B20C0 (.h:49)
    uint32 TextureData::getLevelCount() const
    {
        return description.levelCount;
    }

    // 0x004B20D0 (.h:50)
    uint32 TextureData::getFormat() const
    {
        return description.format;
    }

    namespace graphics
    {
        // 0x00C5CC30 (:21)
        static uint32 getD3dCpuAccessFlags(uint32 cpuAccessMode)
        {
            switch (cpuAccessMode)
            {
            case TextureAccessMode_Read:
            case TextureAccessMode_ReadWrite:
                return D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            case TextureAccessMode_Write:
                return D3D11_CPU_ACCESS_WRITE;
            default:
                return 0;
            }
        }

        // 0x00C5CC60 (:42)
        static D3D11_USAGE getD3dUsage(uint32 cpuAccessMode)
        {
            switch (cpuAccessMode)
            {
            case TextureAccessMode_None:
                return D3D11_USAGE_DEFAULT;
            case TextureAccessMode_Read:
            case TextureAccessMode_ReadWrite:
                return D3D11_USAGE_STAGING;
            case TextureAccessMode_Write:
                return D3D11_USAGE_DYNAMIC;
            default:
                exit(1);
            }
        }

        // 0x00C5CCA0 (:64)
        static uint32 getD3dBindFlags(const TextureDescription& description)
        {
            uint32 bindFlags = 0;
            if ((description.flags & TextureFlagMask_Bind_RenderTarget) != 0)
            {
                bindFlags = D3D11_BIND_RENDER_TARGET;
            }
            else if ((description.flags & (TextureFlagMask_Bind_DepthStencil | TextureFlagMask_Bind_ShadowMap)) != 0)
            {
                bindFlags = D3D11_BIND_DEPTH_STENCIL;
            }
            if ((description.flags & TextureFlagMask_Bind_ShaderInput) != 0)
            {
                bindFlags |= D3D11_BIND_SHADER_RESOURCE;
            }
            return bindFlags;
        }

        // 0x00C63380 (:595)
        static bool fillD3dTextureInitData(D3D11_SUBRESOURCE_DATA* pD3dInitData, uint32 initDataCapacity, const TextureDescription& description, const void* pTextureData)
        {
            // The data holds every level of every face (6 for cube maps); each face starts 16-byte aligned. Levels
            // past initDataCapacity are not described.
            const uint8* pStart = (const uint8*)pTextureData;
            const uint8* pCurrent = pStart;
            const uint32 faceCount = description.type == TextureType_Cube ? 6 : 1;
            for (uint32 face = 0; face < faceCount; ++face)
            {
                uint32 width = description.width;
                uint32 height = description.height;
                uint32 depth = description.depth;
                pCurrent = pStart + (((uint32)(pCurrent - pStart) + 15) & ~15u);
                const uint32 levelCount = description.levelCount < initDataCapacity ? description.levelCount : initDataCapacity;
                for (uint32 level = 0; level < levelCount; ++level)
                {
                    pD3dInitData->pSysMem = pCurrent;
                    switch (description.format)
                    {
                    case PixelFormat_Bc1:
                    case PixelFormat_Bc1_Gamma:
                        // Notes (decompiled behaviour, kept as is): block formats get 0 pitches below 4 texels.
                        pD3dInitData->SysMemPitch = (width >> 2) * 8;
                        pD3dInitData->SysMemSlicePitch = (height >> 2) * pD3dInitData->SysMemPitch;
                        break;
                    case PixelFormat_Bc2:
                    case PixelFormat_Bc2_Gamma:
                    case PixelFormat_Bc3:
                    case PixelFormat_Bc3_Gamma:
                        pD3dInitData->SysMemPitch = (width >> 2) * 16;
                        pD3dInitData->SysMemSlicePitch = (height >> 2) * pD3dInitData->SysMemPitch;
                        break;
                    case PixelFormat_A8:
                    case PixelFormat_L8:
                        pD3dInitData->SysMemPitch = width;
                        pD3dInitData->SysMemSlicePitch = width * height;
                        break;
                    case PixelFormat_R32F:
                    case PixelFormat_Y16X16:
                    case PixelFormat_R8G8B8A8:
                    case PixelFormat_R8G8B8A8_Gamma:
                        pD3dInitData->SysMemPitch = width * 4;
                        pD3dInitData->SysMemSlicePitch = width * 4 * height;
                        break;
                    case PixelFormat_A16B16G16R16F:
                        pD3dInitData->SysMemPitch = width * 8;
                        pD3dInitData->SysMemSlicePitch = width * 8 * height;
                        break;
                    case PixelFormat_A32B32G32R32F:
                        pD3dInitData->SysMemPitch = width * 16;
                        pD3dInitData->SysMemSlicePitch = width * 16 * height;
                        break;
                    default:
                        return false;
                    }
                    pCurrent += pD3dInitData->SysMemSlicePitch * depth;
                    width = width >> 1 != 0 ? width >> 1 : 1;
                    height = height >> 1 != 0 ? height >> 1 : 1;
                    depth = depth >> 1 != 0 ? depth >> 1 : 1;
                    ++pD3dInitData;
                }
            }
            return true;
        }

        // 0x00C6B750 (:99)
        bool allocateTextureData(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTexMapAllocator, const TextureDescription& description, const InitialTextureLevelData* pInitialLevelData, uint32 initialLevelDataCount, const char* pDebugName)
        {
            pTexture->description = description;
            pTexture->pTexture = nullptr;
            pTexture->pShaderView = nullptr;
            ID3D11Device* pDevice = pGraphicsSystem->pDevice;
            const bool useTypelessFormat = (description.flags & TextureFlagMask_Bind_RenderTarget) != 0 && (description.flags & TextureFlagWinMaskD3D11_AliasableFormat) != 0;
            pTexture->d3dFormat = getD3dPixelFormat(description.format, useTypelessFormat);
            if (pTexture->d3dFormat == DXGI_FORMAT_UNKNOWN)
            {
                debug::nativePrintArgs("Unsupported pixel format '%0x' (%s) in allocateTextureData()\n", description.format, getPixelFormatInfo(description.format).pName);
                return false;
            }

            D3D11_SUBRESOURCE_DATA initData[TextureData::MaxInitDataLevelCount];
            for (uint32 i = 0; i < initialLevelDataCount; ++i)
            {
                initData[i].pSysMem = pInitialLevelData[i].pData;
                initData[i].SysMemPitch = pInitialLevelData[i].rowPitch;
                initData[i].SysMemSlicePitch = pInitialLevelData[i].slicePitch;
            }
            const D3D11_SUBRESOURCE_DATA* pInitData = pInitialLevelData != nullptr ? initData : nullptr;

            HRESULT result;
            switch (description.type)
            {
            case TextureType_1d:
            {
                D3D11_TEXTURE1D_DESC textureDesc;
                textureDesc.Width = description.width;
                textureDesc.MipLevels = description.levelCount;
                // Notes (decompiled behaviour, kept as is): ArraySize is left 0, so 1D textures cannot be created.
                textureDesc.ArraySize = 0;
                textureDesc.Format = pTexture->d3dFormat;
                textureDesc.Usage = getD3dUsage(description.cpuAccessMode);
                textureDesc.BindFlags = getD3dBindFlags(description);
                textureDesc.CPUAccessFlags = getD3dCpuAccessFlags(description.cpuAccessMode);
                textureDesc.MiscFlags = 0;
                result = pDevice->CreateTexture1D(&textureDesc, pInitData, (ID3D11Texture1D**)&pTexture->pTexture);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "CreateTexture1D");
                }
                break;
            }
            case TextureType_2d:
            case TextureType_Cube:
            {
                const bool isCube = description.type == TextureType_Cube;
                if (isCube && initialLevelDataCount % 6 != 0)
                {
                    debug::nativePrintArgs("[graphics] You have to specify all 6 cube faces!\n");
                    return false;
                }
                D3D11_TEXTURE2D_DESC textureDesc;
                memset(&textureDesc, 0, sizeof(textureDesc));
                textureDesc.Width = description.width;
                textureDesc.Height = description.height;
                textureDesc.MipLevels = description.levelCount;
                textureDesc.ArraySize = isCube ? 6 : 1;
                textureDesc.Format = pTexture->d3dFormat;
                textureDesc.SampleDesc.Count = 1;
                textureDesc.SampleDesc.Quality = 0;
                textureDesc.Usage = getD3dUsage(description.cpuAccessMode);
                textureDesc.BindFlags = getD3dBindFlags(description);
                textureDesc.CPUAccessFlags = getD3dCpuAccessFlags(description.cpuAccessMode);
                textureDesc.MiscFlags = isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
                result = pDevice->CreateTexture2D(&textureDesc, pInitData, (ID3D11Texture2D**)&pTexture->pTexture);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, isCube ? "Create Cube Texture" : "CreateTexture2D");
                }
                break;
            }
            case TextureType_3d:
            {
                D3D11_TEXTURE3D_DESC textureDesc;
                textureDesc.Width = description.width;
                textureDesc.Height = description.height;
                textureDesc.Depth = description.depth;
                textureDesc.MipLevels = description.levelCount;
                textureDesc.Format = pTexture->d3dFormat;
                textureDesc.Usage = getD3dUsage(description.cpuAccessMode);
                textureDesc.BindFlags = getD3dBindFlags(description);
                textureDesc.CPUAccessFlags = getD3dCpuAccessFlags(description.cpuAccessMode);
                textureDesc.MiscFlags = 0;
                result = pDevice->CreateTexture3D(&textureDesc, pInitData, (ID3D11Texture3D**)&pTexture->pTexture);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "CreateTexture3D");
                }
                break;
            }
            default:
                break;
            }
            return createTextureShaderView(pTexture, pGraphicsSystem, description, pDebugName);
        }

        // 0x00C5CCD0 (:250)
        void freeTextureData(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, MemoryAllocator* pTexMapAllocator)
        {
            if (pTexture->pShaderView != nullptr)
            {
                pTexture->pShaderView->Release();
                pTexture->pShaderView = nullptr;
            }
            if (pTexture->pTexture != nullptr)
            {
                pTexture->pTexture->Release();
                pTexture->pTexture = nullptr;
            }
            InterlockedDecrement((volatile LONG*)&g_allocatedResources);
        }

        // 0x00C6BD20 (:272)
        bool initializeTextureData(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, const TextureDescription& description, const void* pTextureData, const char* pDebugName)
        {
            pTexture->description = description;
            pTexture->pTexture = nullptr;
            pTexture->pShaderView = nullptr;
            ID3D11Device* pDevice = pGraphicsSystem->pDevice;
            pTexture->d3dFormat = getD3dPixelFormat(description.format, false);
            if (pTexture->d3dFormat == DXGI_FORMAT_UNKNOWN)
            {
                debug::nativePrintArgs("[graphics] Could not map pixel format %i (%s) to D3D Format!\n", description.format, getPixelFormatInfo(description.format).pName);
                return false;
            }
            D3D11_SUBRESOURCE_DATA initData[TextureData::MaxInitDataLevelCount];
            if (!fillD3dTextureInitData(initData, TextureData::MaxInitDataLevelCount, description, pTextureData))
            {
                debug::nativePrintArgs("[graphics] Could not fill D3D texture init data!\n");
                return false;
            }
            // Initialized textures are immutable to the CPU (default usage, no CPU access).
            HRESULT result;
            switch (description.type)
            {
            case TextureType_1d:
            {
                D3D11_TEXTURE1D_DESC textureDesc;
                textureDesc.Width = description.width;
                textureDesc.MipLevels = description.levelCount;
                textureDesc.ArraySize = 1;
                textureDesc.Format = pTexture->d3dFormat;
                textureDesc.Usage = D3D11_USAGE_DEFAULT;
                textureDesc.BindFlags = getD3dBindFlags(description);
                textureDesc.CPUAccessFlags = 0;
                textureDesc.MiscFlags = 0;
                result = pDevice->CreateTexture1D(&textureDesc, initData, (ID3D11Texture1D**)&pTexture->pTexture);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "CreateTexture1D");
                }
                break;
            }
            case TextureType_2d:
            case TextureType_Cube:
            {
                const bool isCube = description.type == TextureType_Cube;
                D3D11_TEXTURE2D_DESC textureDesc;
                memset(&textureDesc, 0, sizeof(textureDesc));
                textureDesc.Width = description.width;
                textureDesc.Height = description.height;
                textureDesc.MipLevels = description.levelCount;
                textureDesc.ArraySize = isCube ? 6 : 1;
                textureDesc.Format = pTexture->d3dFormat;
                textureDesc.SampleDesc.Count = 1;
                textureDesc.Usage = D3D11_USAGE_DEFAULT;
                textureDesc.BindFlags = getD3dBindFlags(description);
                textureDesc.CPUAccessFlags = 0;
                textureDesc.MiscFlags = isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
                result = pDevice->CreateTexture2D(&textureDesc, initData, (ID3D11Texture2D**)&pTexture->pTexture);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, isCube ? "Create Cube Texture" : "CreateTexture2D");
                }
                break;
            }
            case TextureType_3d:
            {
                D3D11_TEXTURE3D_DESC textureDesc;
                textureDesc.Width = description.width;
                textureDesc.Height = description.height;
                textureDesc.Depth = description.depth;
                textureDesc.MipLevels = description.levelCount;
                textureDesc.Format = pTexture->d3dFormat;
                textureDesc.Usage = D3D11_USAGE_DEFAULT;
                textureDesc.BindFlags = getD3dBindFlags(description);
                textureDesc.CPUAccessFlags = 0;
                textureDesc.MiscFlags = 0;
                result = pDevice->CreateTexture3D(&textureDesc, initData, (ID3D11Texture3D**)&pTexture->pTexture);
                if (FAILED(result))
                {
                    handleFatalHResult(pGraphicsSystem, result, "CreateTexture3D");
                }
                break;
            }
            default:
                break;
            }
            return createTextureShaderView(pTexture, pGraphicsSystem, description, pDebugName);
        }

        // 0x00C62F30 (:408)
        bool createTextureShaderView(TextureData* pTexture, GraphicsSystem* pGraphicsSystem, const TextureDescription& description, const char* pDebugName)
        {
            if ((description.flags & TextureFlagMask_Bind_ShaderInput) == 0)
            {
                InterlockedIncrement((volatile LONG*)&g_allocatedResources);
                return true;
            }
            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
            memset(&viewDesc, 0, sizeof(viewDesc));
            switch (description.type)
            {
            case TextureType_1d:
                viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                viewDesc.Texture1D.MipLevels = description.levelCount;
                viewDesc.Format = getD3dPixelFormat(description.format, false);
                break;
            case TextureType_2d:
                viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipLevels = description.levelCount;
                if ((description.flags & TextureFlagMask_Bind_RenderTarget) != 0)
                {
                    viewDesc.Texture2D.MipLevels = 1;
                    viewDesc.Format = getD3dPixelFormat(description.format, false);
                }
                else if ((description.flags & (TextureFlagMask_Bind_DepthStencil | TextureFlagMask_Bind_ShadowMap)) != 0)
                {
                    // Depth textures are read through their depth channel.
                    viewDesc.Texture2D.MipLevels = 1;
                    switch (description.format)
                    {
                    case PixelFormat_Depth16: viewDesc.Format = DXGI_FORMAT_R16_UNORM; break;
                    case PixelFormat_Depth24Stencil8: viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; break;
                    case PixelFormat_Depth32F: viewDesc.Format = DXGI_FORMAT_R32_FLOAT; break;
                    default:
                        debug::nativePrintArgs("[graphics] Currently only Depth16 and Depth24S8 depth render formats are supported!\n");
                        break;
                    }
                }
                else
                {
                    viewDesc.Format = getD3dPixelFormat(description.format, false);
                }
                break;
            case TextureType_3d:
                viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipLevels = description.levelCount;
                viewDesc.Format = getD3dPixelFormat(description.format, false);
                break;
            case TextureType_Cube:
                viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                viewDesc.TextureCube.MipLevels = description.levelCount;
                viewDesc.Format = getD3dPixelFormat(description.format, false);
                break;
            default:
                break;
            }
            const HRESULT result = pGraphicsSystem->pDevice->CreateShaderResourceView(pTexture->pTexture, &viewDesc, &pTexture->pShaderView);
            if (FAILED(result))
            {
                handleFatalHResult(pGraphicsSystem, result, "CreateShaderResourceView");
            }
            InterlockedIncrement((volatile LONG*)&g_allocatedResources);
            return true;
        }

        // 0x00C5CD10 (:526)
        DXGI_FORMAT getD3dPixelFormat(uint32 format, bool useTypeLessFormats)
        {
            switch (format)
            {
            case PixelFormat_Bc1: return DXGI_FORMAT_BC1_UNORM;
            case PixelFormat_Bc1_Gamma: return DXGI_FORMAT_BC1_UNORM_SRGB;
            case PixelFormat_Bc2: return DXGI_FORMAT_BC2_UNORM;
            case PixelFormat_Bc2_Gamma: return DXGI_FORMAT_BC2_UNORM_SRGB;
            case PixelFormat_Bc3: return DXGI_FORMAT_BC3_UNORM;
            case PixelFormat_Bc3_Gamma: return DXGI_FORMAT_BC3_UNORM_SRGB;
            case PixelFormat_A8: return DXGI_FORMAT_A8_UNORM;
            case PixelFormat_L8: return DXGI_FORMAT_R8_UNORM;
            case PixelFormat_R8G8: return useTypeLessFormats ? DXGI_FORMAT_R8G8_TYPELESS : DXGI_FORMAT_R8G8_UNORM;
            case PixelFormat_R32F: return DXGI_FORMAT_R32_FLOAT;
            case PixelFormat_Y16X16: return DXGI_FORMAT_R16G16_UNORM;
            case PixelFormat_R8G8B8A8: return useTypeLessFormats ? DXGI_FORMAT_R8G8B8A8_TYPELESS : DXGI_FORMAT_R8G8B8A8_UNORM;
            case PixelFormat_R8G8B8A8_Gamma: return useTypeLessFormats ? DXGI_FORMAT_R8G8B8A8_TYPELESS : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case PixelFormat_A16B16G16R16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case PixelFormat_A32B32G32R32F: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case PixelFormat_Depth16: return DXGI_FORMAT_R16_TYPELESS;
            case PixelFormat_Depth24Stencil8: return DXGI_FORMAT_R24G8_TYPELESS;
            case PixelFormat_Depth32F: return DXGI_FORMAT_R32_TYPELESS;
            case PixelFormat_R11G11B10F: return DXGI_FORMAT_R11G11B10_FLOAT;
            default: return DXGI_FORMAT_UNKNOWN;
            }
        }
    }
}
