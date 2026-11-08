// Original: volition/render_lib/code/shader/instanceshaderdata.hpp
//
// Constant buffer layout(s) shared with the shaders (InstanceVertexShaderConstants, InstanceFragmentShaderConstants);
// keen namespace. Constant buffer structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after
// members ending at 0x28; tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicsmatrix.hpp"

namespace keen
{
    // size 0x50
    struct alignas(16) InstanceVertexShaderConstants
    {
        GraphicsMatrix43 objTM; // 0x00
        float4 instanceData; // 0x30
        float3 Terrain_subzone_offset; // 0x40
        float Terrain_subzone_offset_Dummy0; // 0x4C
    };

    // size 0x20
    struct alignas(16) InstanceFragmentShaderConstants
    {
        float4 tint; // 0x00
        float alphaTestRef; // 0x10
        uint32 doExplicitAlphaTest; // 0x14
        uint32 doExplicitAlphaTest_dummy0; // 0x18
        uint32 doExplicitAlphaTest_dummy1; // 0x1C
    };
}
