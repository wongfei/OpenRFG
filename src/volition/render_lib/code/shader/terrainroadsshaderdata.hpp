// Original: volition/render_lib/code/shader/terrainroadsshaderdata.hpp
//
// Constant buffer layout(s) shared with the shaders (TerrainRoadsVertexShaderConstants,
// TerrainRoadsFragmentShaderConstants); keen namespace. Constant buffer structs are 16-byte aligned
// (rl_renderer::m_commonVertexData sits at 0x30 after members ending at 0x28; tools/hdrgen/aligncheck.py); alignas(16)
// is inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // size 0x60
    struct alignas(16) TerrainRoadsVertexShaderConstants
    {
        float4 Translation; // 0x00
        float4 Terrain_layer0_scale_offset; // 0x10
        float4 Terrain_layer1_scale_offset; // 0x20
        float4 Terrain_layer2_scale_offset; // 0x30
        float4 Road_subzone_offset; // 0x40
        float4 Tile; // 0x50
    };

    // size 0x90
    struct alignas(16) TerrainRoadsFragmentShaderConstants
    {
        float4 Tint_color; // 0x00
        float4 detailMapTilingScaleBiasBlend; // 0x10
        float4 Terrain_layer_spec_alpha; // 0x20
        float4 Terrain_layer_spec_power; // 0x30
        float4 Diffuse_color_0; // 0x40
        float4 Diffuse_color_1; // 0x50
        float4 Diffuse_color_2; // 0x60
        float4 Diffuse_color_3; // 0x70
        float4 Edge_fade__use_spec_map; // 0x80
    };
}
