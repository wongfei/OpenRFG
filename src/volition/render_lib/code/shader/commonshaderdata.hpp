// Original: volition/render_lib/code/shader/commonshaderdata.hpp
//
// Constant buffer layout(s) shared with the shaders (CommonVertexShaderConstants, CommonFragmentShaderConstants); keen
// namespace. Constant buffer structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members
// ending at 0x28; tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicsmatrix.hpp"

namespace keen
{
    // size 0x100
    struct alignas(16) CommonVertexShaderConstants
    {
        GraphicsMatrix44 projTM; // 0x00
        GraphicsMatrix44 viewTM; // 0x40
        GraphicsMatrix43 camRot; // 0x80
        float4 camPos; // 0xB0
        float4 time; // 0xC0
        float4 fogDistance; // 0xD0
        float3 renderOffset; // 0xE0
        float renderOffset_dummy0; // 0xEC
        float4 targetDimensions; // 0xF0
    };

    // size 0xB0
    struct alignas(16) CommonFragmentShaderConstants
    {
        float2 saturation; // 0x00
        float dummy0; // 0x08
        float dummy1; // 0x0C
        float4 time; // 0x10
        float4 ambient; // 0x20
        float4 backAmbient; // 0x30
        float4 fogColor; // 0x40
        float4 shadowFadeParams; // 0x50
        float4 targetDimensions; // 0x60
        float4 indirectLightParams; // 0x70
        float substanceSpecAlphaScale; // 0x80
        float substanceSpecPowerScale; // 0x84
        float substanceDiffuseScale; // 0x88
        float substanceFresnelAlphaScale; // 0x8C
        float substanceFresnelPowerScale; // 0x90
        float glassFresnelBias; // 0x94
        float glassFresnelPower; // 0x98
        float glassReflectionEnabled; // 0x9C
        float glassDirtEnabled; // 0xA0
        float dummy2; // 0xA4
        float dummy3; // 0xA8
        float dummy4; // 0xAC
    };
}
