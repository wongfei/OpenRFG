// Original: volition/render_lib/code/shader/decalshaderdata.hpp
//
// Constant buffer layout(s) shared with the shaders (DecalVertexShaderConstants); keen namespace. Constant buffer
// structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members ending at 0x28;
// tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x70
    struct alignas(16) DecalVertexShaderConstants
    {
        float3 xvec; // 0x00
        float xvec_Dummy0; // 0x0C
        float3 yvec; // 0x10
        float yvec_Dummy0; // 0x1C
        float3 zvec; // 0x20
        float zvec_Dummy0; // 0x2C
        float3 pos; // 0x30
        float pos_Dummy0; // 0x3C
        float4 tint; // 0x40
        float4 params1; // 0x50
        float4 params2; // 0x60
    };
}
