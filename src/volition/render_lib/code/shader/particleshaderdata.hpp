// Original: volition/render_lib/code/shader/particleshaderdata.hpp
//
// Constant buffer layout(s) shared with the shaders (ParticleVertexShaderConstants, ParticleFragmentShaderConstants);
// keen namespace. Constant buffer structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after
// members ending at 0x28; tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/types.hpp"
#include "keen/graphics/graphicsmatrix.hpp"

// size 0x2C; a 3x3 matrix with rows padded to HLSL constant registers (global namespace; placement inferred)
struct float3x3
{
    keen::float3 row0; // 0x00
    float __dummy0; // 0x0C
    keen::float3 row1; // 0x10
    float __dummy1; // 0x1C
    keen::float3 row2; // 0x20
};

namespace keen
{
    // size 0x90
    struct alignas(16) ParticleVertexShaderConstants
    {
        GraphicsMatrix44 invProjTM; // 0x00
        float4 zDimensions; // 0x40
        float3 effectOpacity; // 0x50
        float effectOpacity_Dummy0; // 0x5C
        float4 effectAmbient; // 0x60
        float4 effectMaterialTint; // 0x70
        float2 effectUvAnimTiling; // 0x80
        float effectUvAnimTiling_Dummy0; // 0x88
        float effectUvAnimTiling_Dummy1; // 0x8C
    };

    // size 0xA0
    struct alignas(16) ParticleFragmentShaderConstants
    {
        GraphicsMatrix44 invProjTM; // 0x00
        float4 zDimensions; // 0x40
        float3x3 colorCorrectionMatrix; // 0x50
        float colorCorrectionMatrix_Dummy0; // 0x7C
        float4 effectDiffuse; // 0x80
        float3 effectLightDir; // 0x90
        float effectLightDir_dummy0; // 0x9C
    };
}
