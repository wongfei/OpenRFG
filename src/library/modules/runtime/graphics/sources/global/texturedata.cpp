// Original: library/modules/runtime/graphics/sources/global/texturedata.cpp
//
// Platform independent texture helpers. TextureDescription's constructor is inline in texturedescription.hpp in the
// original.

#include "keen/graphics/texturedata.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/pixelformat.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

#include <string.h>

namespace keen
{
    // 0x004B2040 (texturedescription.hpp:196)
    TextureDescription::TextureDescription()
    {
        width = 0;
        height = 0;
        depth = 0;
        flags = TextureFlagMask_Bind_ShaderInput;
        type = TextureType_2d;
        format = PixelFormat_None;
        lutFormat = 0;
        multiSampleType = 0;
        levelCount = 0;
        cpuAccessMode = TextureAccessMode_None;
        gpuAccessMode = TextureAccessMode_Read;
        addressModeU = TextureAddressMode_Wrap;
        addressModeV = TextureAddressMode_Wrap;
        addressModeW = TextureAddressMode_Wrap;
    }

    namespace graphics
    {
        // 0x00C69850 (:106)
        void clearTextureData(TextureData* pTexture, GraphicsCommandWriter* pCommandWriter)
        {
            // Clears the top level only (the texture must be CPU writable).
            const uint32 bitsPerPixel = getBitsPerPixel(pTexture->description.format);
            const uint32 depth = pTexture->description.depth < 2 ? 1 : pTexture->description.depth;
            ID3D11DeviceContext* pContext = pCommandWriter->getBuffer()->pContext;
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(pContext->Map(pTexture->pTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)) && mapped.pData != nullptr)
            {
                memset(mapped.pData, 0, (pTexture->description.height * pTexture->description.width * depth * bitsPerPixel) >> 3);
                pCommandWriter->getBuffer()->pContext->Unmap(pTexture->pTexture, 0);
            }
        }
    }
}
