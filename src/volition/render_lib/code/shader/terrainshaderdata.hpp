// Original: volition/render_lib/code/shader/terrainshaderdata.hpp
//
// Constant buffer layout(s) shared with the shaders (TerrainVertexShaderConstants, TerrainFragmentShaderConstants,
// TerrainHeightmeshMaterialVertexShaderConstants, TerrainHeightmeshMaterialFragmentShaderConstants,
// TerrainSidemapMaterialVertexShaderConstants, TerrainSidemapMaterialFragmentShaderConstants); keen namespace.
// Constant buffer structs are 16-byte aligned (rl_renderer::m_commonVertexData sits at 0x30 after members ending at
// 0x28; tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/graphics/graphicsmatrix.hpp"

namespace keen
{
    // size 0x50
    struct alignas(16) TerrainVertexShaderConstants
    {
        float4 Translation; // 0x00
        float4 Terrain_subzone_offset; // 0x10
        GraphicsMatrix43 tm; // 0x20
    };

    // size 0x10
    struct alignas(16) TerrainFragmentShaderConstants
    {
        float4 Tint_color; // 0x00
    };

    // size 0x40
    struct alignas(16) TerrainHeightmeshMaterialVertexShaderConstants
    {
        float4 Terrain_layer0_scale_offset; // 0x00
        float4 Terrain_layer1_scale_offset; // 0x10
        float4 Terrain_layer2_scale_offset; // 0x20
        float4 Terrain_layer3_scale_offset; // 0x30
    };

    // size 0x70
    struct alignas(16) TerrainHeightmeshMaterialFragmentShaderConstants
    {
        float4 detailMapTilingScaleBiasBlend; // 0x00
        float4 Terrain_layer_spec_alpha; // 0x10
        float4 Terrain_layer_spec_power; // 0x20
        float4 Diffuse_color_0; // 0x30
        float4 Diffuse_color_1; // 0x40
        float4 Diffuse_color_2; // 0x50
        float4 Diffuse_color_3; // 0x60
    };

    // size 0x100
    struct alignas(16) TerrainSidemapMaterialVertexShaderConstants
    {
        GraphicsMatrix44 Terrain_layer0_xform; // 0x00
        GraphicsMatrix44 Terrain_layer1_xform; // 0x40
        GraphicsMatrix44 Terrain_layer2_xform; // 0x80
        GraphicsMatrix44 Terrain_layer3_xform; // 0xC0
    };

    // size 0x50
    struct alignas(16) TerrainSidemapMaterialFragmentShaderConstants
    {
        float4 detailMapTilingScaleBiasBlend; // 0x00
        float4 Terrain_layer_spec_alpha; // 0x10
        float4 Terrain_layer_spec_power; // 0x20
        float4 Diffuse_color_0; // 0x30
        float4 Diffuse_color_1; // 0x40
    };
}
