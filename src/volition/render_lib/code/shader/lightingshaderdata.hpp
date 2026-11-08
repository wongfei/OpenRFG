// Original: volition/render_lib/code/shader/lightingshaderdata.hpp
//
// Constant buffer layout(s) shared with the shaders (LightingVertexShaderConstants, LightingFragmentShaderConstants);
// keen namespace. Constant buffer structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after
// members ending at 0x28; tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x40
    struct alignas(16) LightingVertexShaderConstants
    {
        float3 lightDirOrPos; // 0x00
        float __dummy0; // 0x0C
        float4 spotLightDir; // 0x10
        float4 attenuation; // 0x20
        float4 hotspot; // 0x30
    };

    // size 0x30
    struct alignas(16) LightingFragmentShaderConstants
    {
        float4 lightColor; // 0x00
        float4 attenuation; // 0x10
        float4 hotspot; // 0x20
    };
}
