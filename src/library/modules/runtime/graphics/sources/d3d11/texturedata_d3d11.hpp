// Original: library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp
// Implementation: library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.cpp
//
// TextureData, a texture: its description, the D3D11 resource and shader resource view.
#pragma once

#include "keen/base/types.hpp"
#include <d3d11.h>
#include <d3d11.h>
#include "keen/graphics/texturedescription.hpp"

struct ID3D11Resource;
struct ID3D11ShaderResourceView;

namespace keen
{
    // size 0x20
    struct TextureData
    {
        enum
        {
            MaxMipLevelCount = 13,
            MaxInitDataLevelCount = 78,
        };

        TextureData(); // 0x004B2070 (.h:39)
        uint32 getWidth() const; // 0x004B20A0 (.h:46)
        uint32 getHeight() const; // 0x004B20B0 (.h:47)
        uint32 getDepth() const; // 0x00C5B0F0 (.h:48)
        uint32 getLevelCount() const; // 0x004B20C0 (.h:49)
        uint32 getFormat() const; // 0x004B20D0 (.h:50)
        DXGI_FORMAT getD3dFormat() const;
        uint32 getType() const;
        uint32 getAddressModeU() const;
        uint32 getAddressModeV() const;
        uint32 getAddressModeW() const;

        TextureDescription description; // 0x00
        ID3D11Resource* pTexture; // 0x14
        ID3D11ShaderResourceView* pShaderView; // 0x18
        DXGI_FORMAT d3dFormat; // 0x1C
    };
}
