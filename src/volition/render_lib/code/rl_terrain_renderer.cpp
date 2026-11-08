// Original: volition/render_lib/code/rl_terrain_renderer.cpp
//
// The terrain renderer and its render context.
//
// The context (createTerrainRenderContext) holds the constant buffers, shader variants, vertex formats, input
// bindings and samplers of the terrain passes, and the library handles of the .fxo_kg terrain shaders. It also
// registers code-built shaders that streamed meshes use: "kg_terrainrocks" (terrain stitch pieces),
// "rfg-terrainblend", "kg_substance" and "kg_substance_singledecal" and the "rfg-eosglass" pair are keen shader variants
// (game/build/.../shader/*_sv.cpp), not .fxo_kg files, so a material naming them only binds (rl_material_data_fixup)
// once the context exists. rl_terrain_renderer::init creates the context during game_render_system_init.
//
// The renderer draws the terrain lists the visibility pass fills (height mesh patches, low LOD patches, skirts,
// roads, landmarks) straight through the keen command writer with the context's pipelines, not through rl_shader
// materials: prerender sorts the height mesh patches into three LOD lists and their render blocks into material-sorted
// chunks; each pass (base, lighting, depth) then sets the terrain constant buffers (instance slot 6, material slot 2,
// roads slot 7), the variant pipeline and the material textures per chunk or instance.
//
// Notes (decompiled behaviour, kept as is):
// - The glass variants (glassVariants) are part of the context but are never created.
// - destroyTerrainRenderContext does not destroy the roads bindings and constant buffers, the stitch alpha and roads
//   vertex formats, or the substance and EOS glass variants, and the registered rl_shaders stay in the library.

#include "volition/render_lib/code/rl_terrain_renderer.h"

#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_height_mesh_instance.h"
#include "volition/render_lib/code/rl_landmark_lod_mesh_instance.h"
#include "volition/render_lib/code/rl_low_lod_mesh_instance.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_skirt_mesh_instance.h"
#include "volition/render_lib/code/rl_texture_defines.h"
#include "volition/render_lib/code/shader/terrainroadsshaderdata.hpp"
#include "volition/render_lib/code/shader/terrainshaderdata.hpp"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/ix.h"
#include "library/modules/runtime/graphics/sources/d3d11/rendergeometry_d3d11.hpp"

#include "keen/base/memoryallocator.hpp"
#include "keen/base/radixsort.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/graphicsstateobjects.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/rendergeometry.hpp"
#include "keen/graphics/vertexattribute.hpp"
#include "keen/math/matrix43.hpp"

#include <algorithm>
#include <math.h>
#include <string.h>

int g_terrainAnisotropyLevel = 4; // 0x0150A600
int g_terrainMinMipLevel; // 0x01B7BD3C

// Magic number: the geometry mode mask of every terrain input binding.
static const unsigned int TERRAIN_GEOMETRY_MODE_MASK = 7;

// A texture fixup of a kg shader: the texture name and the sampler slot.
static rl_shader_tex_fixup tex_fixup(const char* name, unsigned char sampler_slot)
{
    rl_shader_tex_fixup fixup;
    memset(&fixup, 0, sizeof(fixup));
    fixup.crc_value = (int)crc_stri(name, 0);
    fixup.constLocData.const_loc_disk = sampler_slot;
    return fixup;
}

// A constant fixup of a kg shader: the constant name, its location (in floats), rows and array size.
static rl_shader_const_fixup const_fixup(const char* name, unsigned char const_loc, unsigned char num_rows, unsigned char array_size)
{
    rl_shader_const_fixup fixup;
    fixup.crc_value = (int)crc_stri(name, 0);
    fixup.const_loc = const_loc;
    fixup.flags = 0;
    fixup.num_rows = num_rows;
    fixup.array_size = array_size;
    return fixup;
}

// A new rl_shader over the three technique pipelines (base, lighting, depth), registered under name.
static void register_kg_shader(const char* name, const keen::ShaderPipeline* pipeline0, const keen::ShaderPipeline* pipeline1, const keen::ShaderPipeline* pipeline2, const rl_shader_tex_fixup* tex_fixups, unsigned int num_tex,
                               const rl_shader_const_fixup* const_fixups, unsigned int num_consts, bool override_existing)
{
    const keen::ShaderPipeline* pipelines[3] = { pipeline0, pipeline1, pipeline2 };
    rl_shader* shader = rl_shader::create(); // inlined, not checked
    shader->initKgShader(pipelines, tex_fixups, num_tex, const_fixups, num_consts);
    rl_shader_library::get_instance()->register_shader(name, shader, override_existing);
}

// 0x004FF630 (:147)
// Border-addressed, wrapped and wrapped anisotropic linear samplers; the lookup maps the material sampler slots to
// them (slots 0-8 anisotropic, 9 wrapped, 13 and 15 border).
void createSamplerStates(TerrainRenderContext* pContext, keen::GraphicsSystem* pGraphicsSystem)
{
    keen::SamplerParameters parameters;
    memset(&parameters, 0, sizeof(parameters));
    parameters.maxLod = 1000.0f;
    parameters.minLod = (float)g_terrainMinMipLevel;
    parameters.addressW = keen::SamplerAddressMode_Wrap;
    parameters.comparisonFunction = keen::ComparisonFunction_Never;
    parameters.alphaKillEnabled = false;
    parameters.mipLodBias = 0.0f;
    parameters.maxAnisotropyLevel = 0;
    parameters.addressV = keen::SamplerAddressMode_Border;
    parameters.addressU = keen::SamplerAddressMode_Border;
    parameters.mipFilter = keen::SamplerFilterMode_Linear;
    parameters.magFilter = keen::SamplerFilterMode_Linear;
    parameters.minFilter = keen::SamplerFilterMode_Linear;
    pContext->pSamplerStates[0] = keen::graphics::createSamplerState(pGraphicsSystem, parameters);
    parameters.addressV = keen::SamplerAddressMode_Wrap;
    parameters.addressU = keen::SamplerAddressMode_Wrap;
    parameters.mipFilter = keen::SamplerFilterMode_Linear;
    parameters.magFilter = keen::SamplerFilterMode_Linear;
    parameters.minFilter = keen::SamplerFilterMode_Linear;
    pContext->pSamplerStates[1] = keen::graphics::createSamplerState(pGraphicsSystem, parameters);
    parameters.maxAnisotropyLevel = (unsigned int)g_terrainAnisotropyLevel;
    parameters.addressV = keen::SamplerAddressMode_Wrap;
    parameters.addressU = keen::SamplerAddressMode_Wrap;
    // The filter after linear is anisotropic.
    parameters.minFilter = (keen::SamplerFilterMode)(keen::SamplerFilterMode_Linear + (g_terrainAnisotropyLevel > 0 ? 1 : 0));
    parameters.magFilter = parameters.minFilter;
    parameters.mipFilter = parameters.minFilter;
    pContext->pSamplerStates[2] = keen::graphics::createSamplerState(pGraphicsSystem, parameters);
    for (int i = 0; i < 9; i++)
    {
        pContext->pSamplerStateLookup[i] = pContext->pSamplerStates[2];
    }
    pContext->pSamplerStateLookup[9] = pContext->pSamplerStates[1];
    pContext->pSamplerStateLookup[0xD] = pContext->pSamplerStates[0];
    pContext->pSamplerStateLookup[0xF] = pContext->pSamplerStates[0];
}

// 0x004D4310 (:185)
void destroySamplerStates(TerrainRenderContext* pContext, keen::GraphicsSystem* pGraphicsSystem)
{
    keen::graphics::destroySamplerState(pGraphicsSystem, pContext->pSamplerStates[0]);
    keen::graphics::destroySamplerState(pGraphicsSystem, pContext->pSamplerStates[1]);
    keen::graphics::destroySamplerState(pGraphicsSystem, pContext->pSamplerStates[2]);
}

// 0x00531BD0 (:192)
TerrainRenderContext* createTerrainRenderContext(keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem)
{
    TerrainRenderContext* pContext = (TerrainRenderContext*)pAllocator->allocate(sizeof(TerrainRenderContext), 4, 0, nullptr);
    pContext->pTerrainVertexConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x50);
    pContext->pTerrainFragmentConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x10);
    pContext->pHeightmeshMaterialVertexConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x40);
    pContext->pHeightmeshMaterialFragmentConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x70);
    pContext->pSidemapMaterialVertexConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x100);
    pContext->pSidemapMaterialFragmentConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x50);
    pContext->pRoadsVertexConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x60);
    pContext->pRoadsFragmentConstants = keen::graphics::createDynamicConstantBuffer(pGraphicsSystem, 0, 0x90);
    pContext->heightMeshVariants.create(pGraphicsSystem);
    pContext->sideMapVariants.create(pGraphicsSystem);
    pContext->lowLodVariants.create(pGraphicsSystem);
    pContext->roadsVariants.create(pGraphicsSystem);
    pContext->rocksVariants.create(pGraphicsSystem);
    pContext->blendVariants.create(pGraphicsSystem);

    // Vertex formats: the height mesh (16-bit xy position, packed normal); the stitch pieces (float position, packed
    // normal; with the alpha blend data also a tangent and four 16-bit uv sets); the low LOD mesh; the roads.
    keen::VertexAttributeDescription height_mesh_attributes[2] = {
        { keen::VertexAttributeId_Position, keen::VertexAttributeFormat_x16y16_sint, 0, 0 },
        { keen::VertexAttributeId_Normal, keen::VertexAttributeFormat_x8y8z8w8_unorm, 0, 0 },
    };
    pContext->pHeightMeshVertexFormat = keen::graphics::createVertexFormat(pGraphicsSystem, height_mesh_attributes, 2);
    keen::VertexAttributeDescription stitch_attributes[7] = {
        { keen::VertexAttributeId_Position, keen::VertexAttributeFormat_x32y32z32_float, 0, 0 },
        { keen::VertexAttributeId_Normal, keen::VertexAttributeFormat_x8y8z8w8_unorm, 0, 0 },
        { keen::VertexAttributeId_Tangent, keen::VertexAttributeFormat_x8y8z8w8_unorm, 0, 0 },
        { keen::VertexAttributeId_TexCoord0, keen::VertexAttributeFormat_x16y16_sint, 0, 0 },
        { keen::VertexAttributeId_TexCoord1, keen::VertexAttributeFormat_x16y16_sint, 0, 0 },
        { keen::VertexAttributeId_TexCoord2, keen::VertexAttributeFormat_x16y16_sint, 0, 0 },
        { keen::VertexAttributeId_TexCoord3, keen::VertexAttributeFormat_x16y16_sint, 0, 0 },
    };
    pContext->pHeightMeshStitchVertexFormat = keen::graphics::createVertexFormat(pGraphicsSystem, stitch_attributes, 2);
    pContext->pHeightMeshStitchAlphaVertexFormat = keen::graphics::createVertexFormat(pGraphicsSystem, stitch_attributes, 7);
    keen::VertexAttributeDescription low_lod_attributes[1] = {
        { keen::VertexAttributeId_Position, keen::VertexAttributeFormat_x16y16z16w16_sint, 0, 0 },
    };
    pContext->pLowLodVertexFormat = keen::graphics::createVertexFormat(pGraphicsSystem, low_lod_attributes, 1);
    keen::VertexAttributeDescription roads_attributes[4] = {
        { keen::VertexAttributeId_Position, keen::VertexAttributeFormat_x32y32z32_float, 0, 0 },
        { keen::VertexAttributeId_Normal, keen::VertexAttributeFormat_x8y8z8w8_unorm, 0, 0 },
        { keen::VertexAttributeId_Tangent, keen::VertexAttributeFormat_x8y8z8w8_unorm, 0, 0 },
        { keen::VertexAttributeId_TexCoord0, keen::VertexAttributeFormat_x16y16_sint, 0, 0 },
    };
    pContext->pRoadsVertexFormat = keen::graphics::createVertexFormat(pGraphicsSystem, roads_attributes, 4);

    // Input bindings: height mesh [technique][lod][stitch] (stitch 0/1/2 with the height mesh, stitch and stitch alpha
    // formats), side map [technique][lod][type], low LOD [technique], roads [technique][layer].
    for (unsigned int technique = 0; technique < 3; technique++)
    {
        for (unsigned int lod = 0; lod < 3; lod++)
        {
            const keen::VertexInputBinding** bindings = &pContext->pHeightMeshBindings[(technique * 3 + lod) * 3];
            bindings[0] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pContext->pHeightMeshVertexFormat, pContext->heightMeshVariants.getPipeline(technique, lod, 0), TERRAIN_GEOMETRY_MODE_MASK);
            bindings[1] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pContext->pHeightMeshStitchVertexFormat, pContext->heightMeshVariants.getPipeline(technique, lod, 1), TERRAIN_GEOMETRY_MODE_MASK);
            bindings[2] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pContext->pHeightMeshStitchAlphaVertexFormat, pContext->heightMeshVariants.getPipeline(technique, lod, 2), TERRAIN_GEOMETRY_MODE_MASK);
        }
    }
    for (unsigned int technique = 0; technique < 3; technique++)
    {
        for (unsigned int lod = 0; lod < 2; lod++)
        {
            const keen::VertexInputBinding** bindings = &pContext->pSideMapBindings[(technique * 2 + lod) * 2];
            bindings[0] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pContext->pHeightMeshVertexFormat, pContext->sideMapVariants.getPipeline(technique, lod, 0), TERRAIN_GEOMETRY_MODE_MASK);
            bindings[1] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pContext->pHeightMeshVertexFormat, pContext->sideMapVariants.getPipeline(technique, lod, 1), TERRAIN_GEOMETRY_MODE_MASK);
        }
    }
    for (unsigned int technique = 0; technique < 3; technique++)
    {
        pContext->pLowLodBindings[technique] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pContext->pLowLodVertexFormat, pContext->lowLodVariants.getPipeline(technique), TERRAIN_GEOMETRY_MODE_MASK);
    }
    for (unsigned int technique = 0; technique < 3; technique++)
    {
        for (unsigned int layer = 0; layer < 3; layer++)
        {
            pContext->pRoadsBindings[technique * 3 + layer] = keen::graphics::createVertexInputBinding(pGraphicsSystem, pContext->pRoadsVertexFormat, pContext->roadsVariants.getPipeline(layer, technique), TERRAIN_GEOMETRY_MODE_MASK);
        }
    }
    createSamplerStates(pContext, pGraphicsSystem);

    // The .fxo_kg terrain shaders (rl_shader_library::lookup, inlined).
    rl_shader_library* library = rl_shader_library::get_instance();
    pContext->terrainLowLodShaderHandle = library->lookup("rl_terrain_low_lod");
    pContext->heightmeshShaderHandle = library->lookup("rl_terrain_height_mesh");
    pContext->heightmeshLowLodShaderHandle = library->lookup("rl_terrain_height_mesh_lowlod");
    pContext->heightmeshBlendShaderHandle = library->lookup("rl_terrain_height_mesh_blend");
    pContext->sidemapMultiShaderHandle = library->lookup("rl_terrain_sidemap");
    pContext->sidemapSingleShaderHandle = library->lookup("rl_terrain_sidemap_single");
    pContext->roadShaderHandles[0] = library->lookup("rl_terrain_road_layer2");
    pContext->roadShaderHandles[1] = library->lookup("rl_terrain_road_layer3");
    pContext->roadShaderHandles[2] = library->lookup("rl_terrain_road_layer4");

    // The kg shaders: _s (unbatched) and _bs (batched) variants, _ms the third substance type. The texture fixups map
    // material texture names to sampler slots (13 SSAO, 15 shadow map/occlusion), the constant fixups the material
    // constants to float locations.
    {
        const rl_shader_tex_fixup tex[4] = {
            tex_fixup("Diffuse_Map", 0), tex_fixup("Normal_Map", 1), tex_fixup("SSAO_sampler", 0xD), tex_fixup("Shadow_map_sampler", 0xF),
        };
        const rl_shader_const_fixup consts[8] = {
            const_fixup("Diffuse_Color", 0, 1, 4), const_fixup("Specular_Color", 4, 1, 4), const_fixup("Diffuse_Map_TilingU", 8, 1, 1), const_fixup("Diffuse_Map_TilingV", 9, 1, 1),
            const_fixup("Normal_Map_Height", 0xA, 1, 1), const_fixup("Self_Illumination", 0xB, 1, 1), const_fixup("Specular_Power", 0xC, 1, 1), const_fixup("Specular_Alpha", 0xD, 1, 1),
        };
        keen::TerrainRocksVariants& rocks = pContext->rocksVariants;
        register_kg_shader("kg_terrainrocks_s", rocks.getPipeline(0, 0), rocks.getPipeline(0, 1), rocks.getPipeline(0, 2), tex, 4, consts, 8, false);
        register_kg_shader("kg_terrainrocks_bs", rocks.getPipeline(1, 0), rocks.getPipeline(1, 1), rocks.getPipeline(1, 2), tex, 4, consts, 8, false);
    }
    {
        const rl_shader_tex_fixup tex[6] = {
            tex_fixup("Diffuse_Map", 0), tex_fixup("Normal_Map", 1), tex_fixup("Diffuse_Map2", 2), tex_fixup("Normal_Map2", 3), tex_fixup("SSAO_sampler", 0xD), tex_fixup("Shadow_map_sampler", 0xF),
        };
        const rl_shader_const_fixup consts[8] = {
            const_fixup("Diffuse_Color", 0, 1, 4), const_fixup("Diffuse_Color2", 4, 1, 4), const_fixup("Normal_Map_Height", 8, 1, 1), const_fixup("Normal_Map_Height2", 9, 1, 1),
            const_fixup("Specular_Alpha", 0xA, 1, 1), const_fixup("Specular_Alpha2", 0xB, 1, 1), const_fixup("Specular_Power", 0xC, 1, 1), const_fixup("Specular_Power2", 0xD, 1, 1),
        };
        keen::TerrainBlendVariants& blend = pContext->blendVariants;
        register_kg_shader("rfg-terrainblend_s", blend.getPipeline(0, 0), blend.getPipeline(0, 1), blend.getPipeline(0, 2), tex, 6, consts, 8, true);
        register_kg_shader("rfg-terrainblend_bs", blend.getPipeline(1, 0), blend.getPipeline(1, 1), blend.getPipeline(1, 2), tex, 6, consts, 8, true);
    }
    pContext->substanceVariants.create(pGraphicsSystem);
    {
        const rl_shader_tex_fixup tex[7] = {
            tex_fixup("Diffuse_Map", 0), tex_fixup("Normal_Map", 1), tex_fixup("Specular_Map", 2), tex_fixup("Gloss_Map", 3), tex_fixup("Decal_Map", 4), tex_fixup("SSAO_sampler", 0xD),
            tex_fixup("Shadow_map_sampler", 0xF),
        };
        const rl_shader_const_fixup consts[1] = {
            const_fixup("SpecularAlpha__DecalAlpha__FresnelAlphaAndExponent", 0, 1, 4),
        };
        keen::SubstanceVariants& substance = pContext->substanceVariants;
        static const char* const names[2][3] = {
            { "kg_substance_s", "kg_substance_bs", "kg_substance_ms" },
            { "kg_substance_singledecal_s", "kg_substance_singledecal_bs", "kg_substance_singledecal_ms" },
        };
        for (unsigned int decal = 0; decal < 2; decal++)
        {
            for (unsigned int type = 0; type < 3; type++)
            {
                register_kg_shader(names[decal][type], substance.getPipeline(type, 0, decal), substance.getPipeline(type, 1, decal), substance.getPipeline(type, 2, decal), tex, 7, consts, 1, false);
            }
        }
    }
    pContext->eosGlassVariants.create(pGraphicsSystem);
    {
        const rl_shader_tex_fixup tex[5] = {
            tex_fixup("Normal_Map", 0), tex_fixup("Environment_Map", 1), tex_fixup("Reflection_Map", 2), tex_fixup("Diffuse_Map", 3), tex_fixup("occlusionSampler", 0xF),
        };
        const rl_shader_const_fixup consts[6] = {
            const_fixup("Specular_Color", 0, 1, 4), const_fixup("Diffuse_Map_TilingU", 8, 1, 1), const_fixup("Diffuse_Map_TilingV", 9, 1, 1),
            const_fixup("Normal_Map_Height", 0xA, 1, 1), const_fixup("Specular_Alpha", 0xB, 1, 1), const_fixup("Specular_Power", 0xC, 1, 1),
        };
        keen::EosGlassVariants& glass = pContext->eosGlassVariants;
        register_kg_shader("rfg-eosglass_s", glass.getPipeline(0, 0, 0), glass.getPipeline(0, 0, 1), glass.getPipeline(0, 0, 2), tex, 5, consts, 6, true);
        register_kg_shader("rfg-eosglass_bs", glass.getPipeline(1, 0, 0), glass.getPipeline(1, 0, 1), glass.getPipeline(1, 0, 2), tex, 5, consts, 6, true);
    }
    {
        const rl_shader_tex_fixup tex[6] = {
            tex_fixup("Normal_Map", 0), tex_fixup("Environment_Map", 1), tex_fixup("Reflection_Map", 2), tex_fixup("Diffuse_Map", 3), tex_fixup("Specular_Map", 4), tex_fixup("occlusionSampler", 0xF),
        };
        const rl_shader_const_fixup consts[6] = {
            const_fixup("Specular_Color", 0, 1, 4), const_fixup("Diffuse_Map_TilingU", 4, 1, 1), const_fixup("Diffuse_Map_TilingV", 5, 1, 1),
            const_fixup("Normal_Map_Height", 6, 1, 1), const_fixup("Specular_Alpha", 7, 1, 1), const_fixup("Specular_Power", 8, 1, 1),
        };
        keen::EosGlassVariants& glass = pContext->eosGlassVariants;
        register_kg_shader("rfg-eosglass-spec_s", glass.getPipeline(0, 1, 0), glass.getPipeline(0, 1, 1), glass.getPipeline(0, 1, 2), tex, 6, consts, 6, true);
        register_kg_shader("rfg-eosglass-spec_bs", glass.getPipeline(1, 1, 0), glass.getPipeline(1, 1, 1), glass.getPipeline(1, 1, 2), tex, 6, consts, 6, true);
    }
    return pContext;
}

// 0x004FF780 (:752)
void destroyTerrainRenderContext(TerrainRenderContext* pContext, keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem)
{
    for (int i = 0; i < 3; i++)
    {
        keen::graphics::destroySamplerState(pGraphicsSystem, pContext->pSamplerStates[i]);
    }
    pContext->roadsVariants.destroy(pGraphicsSystem);
    pContext->rocksVariants.destroy(pGraphicsSystem);
    pContext->blendVariants.destroy(pGraphicsSystem);
    pContext->lowLodVariants.destroy(pGraphicsSystem);
    pContext->sideMapVariants.destroy(pGraphicsSystem);
    pContext->heightMeshVariants.destroy(pGraphicsSystem);
    for (int i = 0; i < 3; i++)
    {
        keen::graphics::destroyVertexInputBinding(pGraphicsSystem, pContext->pLowLodBindings[i]);
    }
    for (int i = 0; i < 12; i++)
    {
        keen::graphics::destroyVertexInputBinding(pGraphicsSystem, pContext->pSideMapBindings[i]);
    }
    for (int i = 0; i < 27; i++)
    {
        keen::graphics::destroyVertexInputBinding(pGraphicsSystem, pContext->pHeightMeshBindings[i]);
    }
    keen::graphics::destroyVertexFormat(pGraphicsSystem, pContext->pLowLodVertexFormat);
    keen::graphics::destroyVertexFormat(pGraphicsSystem, pContext->pHeightMeshStitchVertexFormat);
    keen::graphics::destroyVertexFormat(pGraphicsSystem, pContext->pHeightMeshVertexFormat);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pSidemapMaterialFragmentConstants);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pSidemapMaterialVertexConstants);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pHeightmeshMaterialFragmentConstants);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pHeightmeshMaterialVertexConstants);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pTerrainFragmentConstants);
    keen::graphics::destroyDynamicConstantBuffer(pGraphicsSystem, pContext->pTerrainVertexConstants);
    pAllocator->free(pContext);
}

// ---- rl_iterrain_renderer / rl_terrain_renderer

// <rl_terrain_renderer::release> 0x004D4360 (:795)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_terrain_renderer)

extern bool Rl_blur_landmarks; // 0x01B7BD42, rl_scene_renderer.cpp
extern float RL_reserved_landmark_depth_range; // 0x0150A618, rl_scene_renderer.cpp
extern bool Rl_terrain_skirt_rendering_enable; // 0x014FA54D, rl_skirt_mesh_instance.cpp

// Magic numbers: the visible list sizes.
const int rl_terrain_renderer::VISIBLE_RENDERABLE_INSTANCES_MAX_DEFAULT = 1024; // 0x012BECE8
const int rl_terrain_renderer::VISIBLE_LOW_LOD_INSTANCES_MAX_DEFAULT = 100; // 0x012BECEC
const int rl_terrain_renderer::VISIBLE_LANDMARK_LOD_INSTANCES_MAX_DEFAULT = 100; // 0x012BECF0
rl_terrain_renderer* rl_terrain_renderer::ms_current_instance_p = nullptr; // 0x01B7BD44
int rl_terrain_renderer::ms_visible_renderable_instances_max = 1024; // 0x0150A60C
int rl_terrain_renderer::ms_visible_low_lod_instances_max = 100; // 0x0150A610
int rl_terrain_renderer::ms_visible_landmark_lod_instances_max = 100; // 0x0150A614

// The terrain low LOD meshes are drawn in the base pass.
bool RL_terrain_render_lowlod = false; // 0x01B7BD41
// The terrain draws through the context's chunk lists (on in retail); off, every instance draws itself.
bool g_terrainUseNewRenderer = true; // 0x014FA613
// The detail map parameters of the terrain materials: tiling, scale, bias and blend (0 when disabled).
float g_terrainDetailMapTiling = 1.3f; // 0x0150A5EC
float g_terrainDetailMapScale = 4.8f; // 0x0150A5F0
float g_terrainDetailMapBias = -0.035f; // 0x0150A5F4
float g_terrainDetailMapBlend = 0.72f; // 0x0150A5F8
bool g_terrainDetailMapEnable = true; // 0x0150A5FC
// Not read in retail.
bool g_terrainSpecularEnable = true; // 0x0150A5FD
// The alpha skirts are drawn (render_alpha_skirts).
bool g_terrainRenderAlphaSkirts = true; // 0x0150A5FE
// The height mesh LOD distances: patches completely within fade_start of the camera are high LOD, patches farther
// than fade_end are the combined LOD 2, the rest the blend LOD 1.
float Rl_terrain_fade_start = 140.0f; // 0x0150A604
float Rl_terrain_fade_end = 250.0f; // 0x0150A608
// Debug switch: every visible patch is high LOD in its renderable (off in retail).
bool Rl_terrain_force_high_lod = false; // 0x01B7BD40

// Magic numbers: the constant buffer slots of the terrain shaders (material 2, instance 6, roads 7).
static const unsigned int TERRAIN_MATERIAL_CONSTANTS_SLOT = 2;
static const unsigned int TERRAIN_INSTANCE_CONSTANTS_SLOT = 6;
static const unsigned int TERRAIN_ROADS_CONSTANTS_SLOT = 7;
// Magic numbers: the low LOD patch entities' class id (rl_low_lod_mesh_instance::m___class_id) and the landmark LOD's
// (rl_landmark_lod_mesh_instance::m___class_id), which the lists are filtered by.
static const unsigned int LOW_LOD_MESH_INSTANCE_CLASS_ID = 0x236D1D4A;
static const unsigned int LANDMARK_LOD_MESH_INSTANCE_CLASS_ID = 0x12323434;

// Not in the binary: the draw of one render block of a geometry from vertex stream 0 (keen
// writeDrawVertexBuffersCommand, inlined in every terrain draw). A block with no index count draws the whole geometry.
static void draw_render_block(keen::GraphicsCommandWriter* pCommandWriter, const keen::RenderGeometry& geometry, const rl_render_block& block)
{
    keen::VertexBufferData vertex_buffer;
    vertex_buffer.pBuffer = geometry.pVertexBuffer;
    vertex_buffer.vertexCount = geometry.vertexCount;
    vertex_buffer.offset = 0;
    const keen::IndexFormat index_format = keen::graphics::getIndexFormat(&geometry);
    keen::IndexBufferData index_buffer;
    index_buffer.pBuffer = geometry.pIndexBuffer;
    index_buffer.indexCount = geometry.indexCount;
    index_buffer.offset = block.start_index * (index_format == keen::IndexFormat_Uint16 ? 2 : 4);
    index_buffer.indexFormat = geometry.indexFormat;
    keen::PrimitiveData primitive;
    primitive.primitiveTopology = geometry.primitiveTopology;
    primitive.vertexDrawCount = block.num_indices;
    if (primitive.vertexDrawCount == 0)
    {
        primitive.vertexDrawCount = geometry.pIndexBuffer == nullptr ? geometry.vertexCount : geometry.indexCount;
    }
    pCommandWriter->writeDrawVertexBuffersCommand(keen::Matrix43(), &vertex_buffer, 1, &index_buffer, &primitive);
}

// Not in the binary: the material of a library handle (the matlib bucket lookup inlined in the chunk fills and
// draws).
static rl_material* terrain_material(unsigned int mat_h)
{
    return (rl_material*)rl_material_library_get_material(mat_h);
}

// Not in the binary: the instance tint (rgb, opacity) times the renderer's fade color, as the terrain shaders get it.
static keen::float4 terrain_tint(const rl_renderer& renderer, const vector& tint, float opacity)
{
    const rl_color_float& fade = renderer.get_fade_color();
    keen::float4 result;
    result.x = fade.red * tint.x;
    result.y = fade.green * tint.y;
    result.z = fade.blue * tint.z;
    result.w = fade.alpha * opacity;
    return result;
}

static keen::float4 make_float4(const vector& v, float w)
{
    keen::float4 result;
    result.x = v.x;
    result.y = v.y;
    result.z = v.z;
    result.w = w;
    return result;
}

// 0x004D4520 (:2286)
// Notes (decompiled behaviour, kept as is): the argument is ignored; the low LOD is always turned on.
void rl_terrain_renderer_set_enable_lowlod(bool enable)
{
    RL_terrain_render_lowlod = true;
}

// 0x004D4380
rl_iterrain_renderer::rl_iterrain_renderer()
{
}

// 0x004D4390
rl_iterrain_renderer::~rl_iterrain_renderer()
{
}

// 0x0051A520 (:803)
rl_terrain_renderer::rl_terrain_renderer()
{
    m_initialized = false;
    m_terrain_chunks_rendered = 0;
    m_scene_renderer_p = nullptr;
    m_depth_only_material = nullptr;
    m_depth_only_material_handle = 0xFFFFFFFF;
}

// 0x00533930 (:813)
// The depth only material (when created) unregistered and released; the lists free their storage. The render
// context is not destroyed here.
rl_terrain_renderer::~rl_terrain_renderer()
{
    if (m_depth_only_material != nullptr)
    {
        rl_material_library_unregister(m_depth_only_material);
        m_depth_only_material->release();
        m_depth_only_material = nullptr;
        m_depth_only_material_handle = 0xFFFFFFFF;
    }
}

// 0x00533A80 (:824)
// The render context, the landmark shaders (rl_simple_texture, rl_simple_filter and its uv_xform constant) and the
// visible lists (4096 skirts, 2048 alpha skirts, 4096 roads).
bool rl_terrain_renderer::init()
{
    m_pContext = createTerrainRenderContext(Init_params.m_pDefaultAllocator, Init_params.m_pKeenGraphicsSystem);
    rl_shader_library* library = rl_shader_library::get_instance();
    m_landmark_effect_index = library->lookup(crc_stri("rl_simple_texture", 0));
    m_landmark_filter_effect_index = library->lookup(crc_stri("rl_simple_filter", 0));
    m_landmark_filter_uv_xform_loc = library->get_shader_const_index(m_landmark_filter_effect_index, crc_stri("uv_xform", 0));
    m_visible_renderable_instances.init_dynamic(ms_visible_renderable_instances_max);
    m_visible_low_lod_instances.init_dynamic(ms_visible_low_lod_instances_max);
    m_visible_landmark_lod_instances.init_dynamic(ms_visible_landmark_lod_instances_max);
    // Magic numbers: the skirt, alpha skirt and road list sizes.
    m_visible_skirt_instances.init_dynamic(0x1000);
    m_visible_alpha_skirt_instances.init_dynamic(0x800);
    m_visible_road_instances.init_dynamic(0x1000);
    m_initialized = true;
    return true;
}

// 0x0051A7D0 (:850)
void rl_terrain_renderer::reset()
{
    m_visible_renderable_instances.remove_all();
    m_visible_low_lod_instances.remove_all();
    m_visible_landmark_lod_instances.remove_all();
    m_visible_skirt_instances.remove_all();
    m_visible_alpha_skirt_instances.remove_all();
    m_visible_road_instances.remove_all();
}

// 0x004D43C0 (:861)
// Front to back by camera distance, with a relative tolerance of 1e-4 (at least 1e-4 absolute).
static bool ftb_chunk_sort_predicate(const rl_renderable_instance* lhs, const rl_renderable_instance* rhs)
{
    const float rhs_distance = rhs->get_camera_distance_squared();
    const float lhs_distance = lhs->get_camera_distance_squared();
    float scale = fabsf(lhs_distance);
    if (scale - fabsf(rhs_distance) < 0.0f)
    {
        scale = fabsf(rhs_distance);
    }
    if (1.0f - scale >= 0.0f)
    {
        scale = 1.0f;
    }
    // Magic number: the relative tolerance.
    return lhs_distance < rhs_distance - scale * 0.0001f;
}

// Not in the binary: the box of a scene entity's world bounding volume (computed if dirty), inlined in prerender.
static void world_box(rl_scene_entity* entity_p, vector& bmin, vector& bmax)
{
    const rl_bvolume& bvolume = entity_p->get_world_bvolume();
    bmin.set(bvolume.m_sphere.m_position.x - bvolume.m_half_vec.x, bvolume.m_sphere.m_position.y - bvolume.m_half_vec.y, bvolume.m_sphere.m_position.z - bvolume.m_half_vec.z);
    bmax.set(bvolume.m_sphere.m_position.x + bvolume.m_half_vec.x, bvolume.m_sphere.m_position.y + bvolume.m_half_vec.y, bvolume.m_half_vec.z + bvolume.m_sphere.m_position.z);
}

// 0x0055D290 (:872)
// The frame's terrain lists from the visible instances:
// - every visible height mesh patch gets its camera distance (LOD 2 for now) and the list is sorted front to back;
// - each patch goes to the high LOD list (completely within Rl_terrain_fade_start of the camera), the combined LOD 2
//   list (farther than Rl_terrain_fade_end) or the blend LOD 1 list, and its render blocks become chunks of that LOD;
// - each LOD's chunks are radix sorted by their key (sidemap materials last, then by material);
// - the low LOD patches to draw: those whose high LOD subzone is not loaded, or not completely within the camera's
//   high LOD distance.
void rl_terrain_renderer::prerender(const rl_camera* camera_p, rl_renderer& renderer)
{
    if (!m_initialized)
    {
        return;
    }
    m_camera_position = camera_p->get_position();
    ms_current_instance_p = this;
    m_terrain_chunks_rendered = 0;
    for (int i = 0; i < m_visible_renderable_instances.size(); i++)
    {
        rl_renderable_instance* instance_p = m_visible_renderable_instances[i];
        vector bmin;
        vector bmax;
        world_box(instance_p, bmin, bmax);
        instance_p->set_camera_distance_squared(bbox_find_distance_squared(bmin, bmax, m_camera_position));
        instance_p->set_lod_level(Rl_terrain_force_high_lod ? 0 : 2);
    }
    std::sort(m_visible_renderable_instances.elements(), m_visible_renderable_instances.elements() + m_visible_renderable_instances.size(), ftb_chunk_sort_predicate);
    const vector camera_position = m_camera_position;
    const float fade_start = Rl_terrain_fade_start;
    const float fade_end_sq = Rl_terrain_fade_end * Rl_terrain_fade_end;
    m_combine_lod_list.resize(0);
    m_blend_lod_list.resize(0);
    m_high_lod_list.resize(0);
    m_chunkHeader.resize(0);
    m_chunksLod0.resize(0);
    m_chunksLod1.resize(0);
    m_chunksLod2.resize(0);
    farray<unsigned int,2048> keys_lod0;
    farray<unsigned int,2048> keys_lod1;
    farray<unsigned int,2048> keys_lod2;
    for (int i = 0; i < m_visible_renderable_instances.size(); i++)
    {
        rl_height_mesh_instance* instance_p = (rl_height_mesh_instance*)m_visible_renderable_instances[i];
        if (instance_p->get_leaf_class_id() == LOW_LOD_MESH_INSTANCE_CLASS_ID)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        world_box(instance_p, bmin, bmax);
        unsigned int lod_level;
        base_array<RenderChunk>* chunks_p;
        base_array<unsigned int>* keys_p;
        if (instance_p->get_camera_distance_squared() > fade_end_sq)
        {
            m_combine_lod_list.add(instance_p);
            chunks_p = &m_chunksLod2;
            keys_p = &keys_lod2;
            lod_level = 2;
        }
        else if (ix_box_completely_in_sphere(camera_position, fade_start, bmin, bmax))
        {
            m_high_lod_list.add(instance_p);
            chunks_p = &m_chunksLod0;
            keys_p = &keys_lod0;
            lod_level = 0;
        }
        else
        {
            m_blend_lod_list.add(instance_p);
            chunks_p = &m_chunksLod1;
            keys_p = &keys_lod1;
            lod_level = 1;
        }
        instance_p->set_lod_level(lod_level);
        heightMeshInstanceFillChunks(lod_level, instance_p, renderer, m_chunkHeader, *chunks_p, *keys_p);
    }
    m_highLodChunks.resize(0);
    m_blendLodChunks.resize(0);
    m_combineLodChunks.resize(0);
    // Magic number: room for the ranks of 2048 keys and their ping-pong buffer.
    unsigned int ranks[4097];
    base_array<RenderChunk>* const chunk_lists[3] = { &m_chunksLod0, &m_chunksLod1, &m_chunksLod2 };
    base_array<unsigned int>* const key_lists[3] = { &keys_lod0, &keys_lod1, &keys_lod2 };
    base_array<RenderChunk*>* const sorted_lists[3] = { &m_highLodChunks, &m_blendLodChunks, &m_combineLodChunks };
    for (int lod = 0; lod < 3; lod++)
    {
        base_array<RenderChunk>& chunks = *chunk_lists[lod];
        keen::RadixSort sorter(ranks, (unsigned int)chunks.size());
        sorter.sort(key_lists[lod]->elements());
        for (int i = 0; i < chunks.size(); i++)
        {
            sorted_lists[lod]->add(&chunks.elements()[ranks[i]]);
        }
    }
    m_low_lod_scene_list.resize(0);
    const float high_lod_far_clip = camera_p->get_high_lod_far_clip();
    for (int i = 0; i < m_visible_low_lod_instances.size(); i++)
    {
        rl_scene_entity* entity_p = m_visible_low_lod_instances[i];
        if (entity_p->get_leaf_class_id() != LOW_LOD_MESH_INSTANCE_CLASS_ID)
        {
            continue;
        }
        if (!((rl_low_lod_mesh_instance*)entity_p)->get_has_high_lod_loaded())
        {
            m_low_lod_scene_list.add(entity_p);
            continue;
        }
        vector bmin;
        vector bmax;
        world_box(entity_p, bmin, bmax);
        if (!ix_box_completely_in_sphere(camera_position, high_lod_far_clip, bmin, bmax))
        {
            m_low_lod_scene_list.add(entity_p);
        }
    }
}

// 0x004FF8D0 (:1052)
void rl_terrain_renderer::render(unsigned int vis_pass_flags)
{
}

// 0x0051A7F0 (:1063)
// The pass's alpha mode (base 5, lighting 1, depth 0), no current shader, and the CLB samplers of slots 13 and 15.
void beginPass(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_renderer& renderer, rl_primitive_state& oldState, rl_shader_technique_type pass, const rl_light*)
{
    // Magic numbers: the alpha mode (state bits 0-4) per technique.
    static const unsigned int pass_alpha_modes[3] = { 5, 1, 0 };
    rl_primitive_state state;
    state.set_bits((oldState.get_bits() & ~0x1Fu) | pass_alpha_modes[pass]);
    renderer.set_macro_state(state, false);
    renderer.clear_current_shader();
    renderer.flush_states();
    // Magic numbers: pixel slots 13 (SSAO) and 15 (shadow mask).
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0xD, pContext->pSamplerStateLookup[0xD]);
    pCommandWriter->writeSetFragmentShaderSamplerStateCommand(0xF, pContext->pSamplerStateLookup[0xF]);
}

// 0x0051A8F0 (:1081)
void endPass(rl_renderer& renderer, rl_primitive_state& state)
{
    renderer.set_macro_state(state, false);
    rl_material_library_clear_render_material(renderer);
}

// 0x004FF8E0 (:1087)
// The decals of every instance in the pass, sorted.
void renderDecals(rl_renderer& renderer, base_array<rl_renderable_instance*>& instances, unsigned int vis_pass_flags)
{
    for (int i = 0; i < instances.size(); i++)
    {
        rl_renderable_instance* instance_p = instances[i];
        if ((instance_p->get_vis_pass_flags() & vis_pass_flags) == 0)
        {
            continue;
        }
        rl_decal* decals[256];
        const int num_decals = instance_p->build_sorted_decal_list(decals, 256);
        if (num_decals > 0)
        {
            instance_p->render_decals(renderer, num_decals, decals);
        }
    }
}

// 0x00533CF0 (:1106)
// The height mesh LODs (unless only the low LOD), the low LOD patches (unless excluded), the skirts and the roads,
// then their decals.
void rl_terrain_renderer::render_base_pass(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags, rl_terrain_mode terrain_mode, bool skip_decals)
{
    if (!m_initialized)
    {
        return;
    }
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    rl_primitive_state saved_state = renderer.get_macro_state();
    beginPass(pCommandWriter, m_pContext, renderer, saved_state, rl_shader_technique_base, light_p);
    if (terrain_mode != RL_TERRAIN_ONLY_LOW_LOD)
    {
        render_pass(pCommandWriter, renderer, camera, rl_shader_technique_base, m_high_lod_list, skip_decals, vis_pass_flags);
        render_pass(pCommandWriter, renderer, camera, rl_shader_technique_base, m_blend_lod_list, skip_decals, vis_pass_flags);
        render_pass(pCommandWriter, renderer, camera, rl_shader_technique_base, m_combine_lod_list, skip_decals, vis_pass_flags);
    }
    if (RL_terrain_render_lowlod && terrain_mode != RL_TERRAIN_NO_LOW_LOD)
    {
        render_low_lod(pCommandWriter, renderer, camera, rl_shader_technique_base, m_low_lod_scene_list, vis_pass_flags);
    }
    render_skirts(pCommandWriter, renderer, camera, rl_shader_technique_base, m_visible_skirt_instances, skip_decals, false, vis_pass_flags);
    render_roads(pCommandWriter, renderer, camera, rl_shader_technique_base, m_visible_road_instances, skip_decals, vis_pass_flags);
    if (!skip_decals && g_terrainUseNewRenderer)
    {
        renderer.set_macro_state(saved_state, false);
        if (terrain_mode != RL_TERRAIN_ONLY_LOW_LOD)
        {
            renderDecals(renderer, m_high_lod_list, vis_pass_flags);
            renderDecals(renderer, m_blend_lod_list, vis_pass_flags);
            renderDecals(renderer, m_combine_lod_list, vis_pass_flags);
        }
        renderDecals(renderer, (base_array<rl_renderable_instance*>&)m_visible_skirt_instances, vis_pass_flags);
        renderDecals(renderer, (base_array<rl_renderable_instance*>&)m_visible_road_instances, vis_pass_flags);
    }
    renderer.set_macro_state(saved_state, false);
    rl_material_library_clear_render_material(renderer);
}

// 0x004D4470 (:1174)
void rl_terrain_renderer::render_conditional_pass(rl_renderer&, rl_camera&, unsigned int)
{
}

// 0x00533EA0 (:1179)
// The alpha (type 2) skirts in the base technique, then their decals.
void rl_terrain_renderer::render_alpha_skirts(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags)
{
    if (!g_terrainRenderAlphaSkirts)
    {
        return;
    }
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    rl_primitive_state saved_state = renderer.get_macro_state();
    beginPass(pCommandWriter, m_pContext, renderer, saved_state, rl_shader_technique_base, light_p);
    for (int i = 0; i < m_visible_alpha_skirt_instances.size(); i++)
    {
        rl_skirt_mesh_instance* instance_p = (rl_skirt_mesh_instance*)m_visible_alpha_skirt_instances[i];
        if ((instance_p->get_vis_pass_flags() & vis_pass_flags) != 0)
        {
            // Magic number: skirt type 2, alpha.
            heightMeshSkirtInstanceRender(pCommandWriter, m_pContext, rl_shader_technique_base, instance_p, renderer, 2);
        }
    }
    if (g_terrainUseNewRenderer)
    {
        renderer.set_macro_state(saved_state, false);
        renderDecals(renderer, (base_array<rl_renderable_instance*>&)m_visible_alpha_skirt_instances, vis_pass_flags);
    }
    renderer.set_macro_state(saved_state, false);
    rl_material_library_clear_render_material(renderer);
}

// 0x00533F70 (:1203)
// The landmarks with the standard shaders, the camera's clip range moved out to [far - 20, 8192] m; the camera
// projection and the eye position (relative to the render offset) are restored afterwards.
void rl_terrain_renderer::render_landmarks_internal(rl_renderer& renderer, rl_camera& camera)
{
    renderer.set_shader_type(shader_type_standard);
    renderer.set_shader_configuration(0);
    const float near_clip = camera.get_near_clip();
    const float far_clip = camera.get_far_clip();
    // Magic numbers: the landmark clip range.
    camera.set_near_clip(far_clip - 20.0f);
    camera.set_far_clip(8192.0f);
    camera.render_refresh(renderer);
    for (int i = 0; i < m_visible_landmark_lod_instances.size(); i++)
    {
        rl_scene_entity* entity_p = m_visible_landmark_lod_instances[i];
        if (entity_p->get_leaf_class_id() == LANDMARK_LOD_MESH_INSTANCE_CLASS_ID)
        {
            ((rl_landmark_lod_mesh_instance*)entity_p)->render(renderer, 0);
        }
    }
    camera.set_near_clip(near_clip);
    camera.set_far_clip(far_clip);
    if (camera.get_type() == rl_camera::RLCT_PERSPECTIVE)
    {
        camera.setup_perspective(renderer);
    }
    else
    {
        camera.setup_ortho(renderer);
    }
    renderer.set_eye_position(camera.get_position() - renderer.get_render_offset());
}

// 0x00534100 (:1235)
void rl_terrain_renderer::prerender_landmarks(rl_renderer& renderer, rl_camera& camera)
{
    if (Rl_blur_landmarks)
    {
        VLIB_CRITICAL_FAILURE();
    }
}

// 0x00534130 (:1341)
// The landmarks in the reserved far end of the depth range ([1 - reserve, 1]) with depth mode 4.
void rl_terrain_renderer::render_landmarks(rl_renderer& renderer, rl_camera& camera)
{
    if (!m_initialized)
    {
        return;
    }
    if (Rl_blur_landmarks)
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    renderer.set_viewport_z_range(1.0f - RL_reserved_landmark_depth_range, 1.0f);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state state;
    // Magic numbers: depth mode (bits 6-8) 4.
    state.set_bits(saved_state.get_bits() & 0xFFFFFF3F | 0x100);
    renderer.set_macro_state(state, false);
    render_landmarks_internal(renderer, camera);
    renderer.set_macro_state(saved_state, false);
    renderer.set_viewport_z_range(z_near, z_far);
}

// 0x00534240 (:1418)
// The high LOD patches, the skirts and the roads with the lighting technique, then their decals.
void rl_terrain_renderer::render_lighting_pass(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags, bool skip_decals)
{
    if (!m_initialized)
    {
        return;
    }
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    rl_primitive_state saved_state = renderer.get_macro_state();
    beginPass(pCommandWriter, m_pContext, renderer, saved_state, rl_shader_technique_lighting, light_p);
    render_pass(pCommandWriter, renderer, camera, rl_shader_technique_lighting, m_high_lod_list, skip_decals, vis_pass_flags);
    render_skirts(pCommandWriter, renderer, camera, rl_shader_technique_lighting, m_visible_skirt_instances, skip_decals, false, vis_pass_flags);
    render_roads(pCommandWriter, renderer, camera, rl_shader_technique_lighting, m_visible_road_instances, skip_decals, vis_pass_flags);
    if (!skip_decals && g_terrainUseNewRenderer)
    {
        renderer.set_macro_state(saved_state, false);
        renderDecals(renderer, m_high_lod_list, vis_pass_flags);
        renderDecals(renderer, (base_array<rl_renderable_instance*>&)m_visible_skirt_instances, vis_pass_flags);
        renderDecals(renderer, (base_array<rl_renderable_instance*>&)m_visible_road_instances, vis_pass_flags);
    }
    renderer.set_macro_state(saved_state, false);
    rl_material_library_clear_render_material(renderer);
}

// 0x00534360 (:1450)
// Depth only: the three height mesh LODs, the low LOD patches (all visible ones in a shadow pass, else the frame's
// low LOD list), the skirts and the roads.
void rl_terrain_renderer::render_depth(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags, bool isShadowPass)
{
    if (!m_initialized)
    {
        return;
    }
    keen::GraphicsCommandWriter* pCommandWriter = renderer.getCommandWriter();
    rl_primitive_state saved_state = renderer.get_macro_state();
    beginPass(pCommandWriter, m_pContext, renderer, saved_state, rl_shader_technique_depth_only, light_p);
    render_pass(pCommandWriter, renderer, camera, rl_shader_technique_depth_only, m_high_lod_list, true, vis_pass_flags);
    render_pass(pCommandWriter, renderer, camera, rl_shader_technique_depth_only, m_blend_lod_list, true, vis_pass_flags);
    render_pass(pCommandWriter, renderer, camera, rl_shader_technique_depth_only, m_combine_lod_list, true, vis_pass_flags);
    render_low_lod(pCommandWriter, renderer, camera, rl_shader_technique_depth_only, isShadowPass ? (base_array<rl_scene_entity*>&)m_visible_low_lod_instances : (base_array<rl_scene_entity*>&)m_low_lod_scene_list, vis_pass_flags);
    for (int i = 0; i < m_visible_skirt_instances.size(); i++)
    {
        rl_skirt_mesh_instance* instance_p = (rl_skirt_mesh_instance*)m_visible_skirt_instances[i];
        if ((instance_p->get_vis_pass_flags() & vis_pass_flags) == 0)
        {
            continue;
        }
        if (!g_terrainUseNewRenderer)
        {
            instance_p->render(renderer, camera, 2);
        }
        else
        {
            // Magic number: skirt type 1, opaque.
            heightMeshSkirtInstanceRender(pCommandWriter, m_pContext, rl_shader_technique_depth_only, instance_p, renderer, 1);
        }
    }
    render_roads(pCommandWriter, renderer, camera, rl_shader_technique_depth_only, m_visible_road_instances, true, vis_pass_flags);
    renderer.set_macro_state(saved_state, false);
    rl_material_library_clear_render_material(renderer);
}

// 0x004D4480 (:1493)
// Only the detail map parameters are set (the specular scale and power and the spec map flag are left as they are).
void setTweakableShaderConstants(TerrainRenderContext* pContext, rl_shader_technique_type techType, const rl_material_data* pMaterialData, unsigned int diffuseSlotStart, keen::float4& detailMaskParams, keen::float4& specularScale, keen::float4& specularPower, float* pEnableSpecMap)
{
    detailMaskParams.x = g_terrainDetailMapTiling;
    detailMaskParams.y = g_terrainDetailMapScale;
    detailMaskParams.z = g_terrainDetailMapBias;
    detailMaskParams.w = g_terrainDetailMapEnable ? g_terrainDetailMapBlend : 0.0f;
}

// Not in the binary: the material constants as float4 rows (constant_block, inlined in the constant setters).
static const keen::float4* material_constants(const rl_material_data* pMaterialData)
{
    return (const keen::float4*)pMaterialData->constant_block.to_pointer();
}

// 0x004FF980 (:1558)
// The height mesh material: vertex constants (the layer scale/offsets, material rows 4-7) and fragment constants
// (detail map, spec alpha and power from rows 8-9, diffuse colors from rows 0-3), then the variant pipeline of
// technique, LOD and skirt type, except for the depth only technique of a plain patch.
void setHeightMeshMaterialShaderConstants(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, unsigned int lodLevel, unsigned int skirtType, const rl_material_data* pMaterialData)
{
    const keen::float4* constants = material_constants(pMaterialData);
    keen::TerrainHeightmeshMaterialVertexShaderConstants* pVertex = (keen::TerrainHeightmeshMaterialVertexShaderConstants*)pCommandWriter->beginDynamicVertexConstants(pContext->pHeightmeshMaterialVertexConstants, TERRAIN_MATERIAL_CONSTANTS_SLOT);
    pVertex->Terrain_layer0_scale_offset = constants[4];
    pVertex->Terrain_layer1_scale_offset = constants[5];
    pVertex->Terrain_layer2_scale_offset = constants[6];
    pVertex->Terrain_layer3_scale_offset = constants[7];
    pCommandWriter->endDynamicVertexConstants();
    keen::TerrainHeightmeshMaterialFragmentShaderConstants* pFragment = (keen::TerrainHeightmeshMaterialFragmentShaderConstants*)pCommandWriter->beginDynamicFragmentConstants(pContext->pHeightmeshMaterialFragmentConstants, TERRAIN_MATERIAL_CONSTANTS_SLOT);
    pFragment->Terrain_layer_spec_alpha = constants[8];
    pFragment->Terrain_layer_spec_power = constants[9];
    keen::float4 specular_scale;
    keen::float4 specular_power;
    setTweakableShaderConstants(pContext, techType, pMaterialData, 0, pFragment->detailMapTilingScaleBiasBlend, specular_scale, specular_power, nullptr);
    pFragment->Diffuse_color_0 = constants[0];
    pFragment->Diffuse_color_1 = constants[1];
    pFragment->Diffuse_color_2 = constants[2];
    pFragment->Diffuse_color_3 = constants[3];
    pCommandWriter->endDynamicFragmentConstants();
    if (techType == rl_shader_technique_depth_only && skirtType == 0)
    {
        return;
    }
    const keen::ShaderPipeline* pPipeline = pContext->heightMeshVariants.getPipeline(techType, lodLevel, skirtType);
    pCommandWriter->writeSetShaderPipelineCommand(pPipeline, pContext->pHeightMeshBindings[(techType * 3 + lodLevel) * 3 + skirtType], true);
}

// 0x004FFC90 (:1600)
// The side map material (single, or multi when it uses the multi side map shader): vertex constants (one or two layer
// transforms), fragment constants (detail map, spec alpha and power, diffuse colors 0-1 of the type), then the variant
// pipeline of technique, LOD / 2 and type except for depth only.
void setSideMapMaterialShaderConstants(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, unsigned int lodLevel, const rl_material_data* pMaterialData)
{
    const unsigned int type = pMaterialData->shader_handle == pContext->sidemapMultiShaderHandle ? 1 : 0;
    const keen::float4* constants = material_constants(pMaterialData);
    void* pVertex = pCommandWriter->beginDynamicVertexConstants(pContext->pSidemapMaterialVertexConstants, TERRAIN_MATERIAL_CONSTANTS_SLOT);
    // Magic number: two layer transforms (0x40 bytes each) for the single side map, four for the multi side map.
    memcpy(pVertex, constants, (type + 1) * 0x80);
    pCommandWriter->endDynamicVertexConstants();
    keen::TerrainSidemapMaterialFragmentShaderConstants* pFragment = (keen::TerrainSidemapMaterialFragmentShaderConstants*)pCommandWriter->beginDynamicFragmentConstants(pContext->pSidemapMaterialFragmentConstants, TERRAIN_MATERIAL_CONSTANTS_SLOT);
    // Magic numbers: the spec alpha and power rows follow the type's transforms and diffuse colors.
    pFragment->Terrain_layer_spec_alpha = constants[type * 9 + 9];
    pFragment->Terrain_layer_spec_power = constants[type * 9 + 10];
    keen::float4 specular_scale;
    keen::float4 specular_power;
    setTweakableShaderConstants(pContext, techType, pMaterialData, 0, pFragment->detailMapTilingScaleBiasBlend, specular_scale, specular_power, nullptr);
    pFragment->Diffuse_color_0 = constants[type * 8 + 8];
    pFragment->Diffuse_color_1 = constants[type * 8 + 9];
    pCommandWriter->endDynamicFragmentConstants();
    if (techType == rl_shader_technique_depth_only)
    {
        return;
    }
    const keen::ShaderPipeline* pPipeline = pContext->sideMapVariants.getPipeline(techType, lodLevel >> 1, type);
    pCommandWriter->writeSetShaderPipelineCommand(pPipeline, pContext->pSideMapBindings[(techType * 2 + (lodLevel >> 1)) * 2 + type], true);
}

// 0x0051A910 (:1643)
// Every texture of the material in its stage, with the context's sampler of that stage; nothing for depth only.
void setMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, const rl_material_data* pMaterialData)
{
    if (pass == rl_shader_technique_depth_only)
    {
        return;
    }
    const rl_texture_desc* textures = pMaterialData->textures.to_pointer();
    for (int i = 0; i < (int)pMaterialData->num_textures; i++)
    {
        const unsigned int stage = textures[i].data.rt.texture_stage;
        const rl_dev_texture texture = get_dev_texture(textures[i].texture_handle);
        pCommandWriter->writeSetFragmentShaderSamplerStateCommand(stage, pContext->pSamplerStateLookup[stage]);
        pCommandWriter->writeSetFragmentShaderTextureCommand(stage, texture.pData);
    }
}

// 0x0051AA20 (:1670)
// Material type 0 is a height mesh material, 1 a side map material.
void heightMeshInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, unsigned int lodLevel, unsigned int materialType, const rl_material_data* pMaterialData)
{
    if (materialType == 0)
    {
        setHeightMeshMaterialShaderConstants(pCommandWriter, pContext, pass, lodLevel, 0, pMaterialData);
    }
    else
    {
        setSideMapMaterialShaderConstants(pCommandWriter, pContext, pass, lodLevel, pMaterialData);
    }
    setMaterial(pCommandWriter, pContext, pass, pMaterialData);
}

// 0x004FFF60 (:1684)
// The instance constants: translation and subzone offset (and the transform of skirts) for the vertex shader, the
// tint for the fragment shader.
void heightMeshInstanceSetInstance(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, const keen::float4& translation, const keen::float4& zoneOffset, const keen::float4& tintColor, const keen::GraphicsMatrix43* pTM)
{
    keen::TerrainVertexShaderConstants* pVertex = (keen::TerrainVertexShaderConstants*)pCommandWriter->beginDynamicVertexConstants(pContext->pTerrainVertexConstants, TERRAIN_INSTANCE_CONSTANTS_SLOT);
    pVertex->Translation = translation;
    pVertex->Terrain_subzone_offset = zoneOffset;
    if (pTM != nullptr)
    {
        pVertex->tm = *pTM;
    }
    pCommandWriter->endDynamicVertexConstants();
    keen::TerrainFragmentShaderConstants* pFragment = (keen::TerrainFragmentShaderConstants*)pCommandWriter->beginDynamicFragmentConstants(pContext->pTerrainFragmentConstants, TERRAIN_INSTANCE_CONSTANTS_SLOT);
    pFragment->Tint_color = tintColor;
    pCommandWriter->endDynamicFragmentConstants();
}

// 0x005344C0 (:1705)
// One chunk header (translation relative to the render offset, subzone offset, tint, geometry) and one chunk per
// render block of the patch's sub mesh, with its sort key: bit 31 the side map type (material slots 3 and up), bits
// 0-30 the material pointer / 2. Material slots 0-2 are the LOD materials (LOD 0 slot 0, else slot 1).
void heightMeshInstanceFillChunks(unsigned int, rl_height_mesh_instance* pInstance, rl_renderer& renderer, base_array<rl_terrain_renderer::RenderChunkHeader>& chunkHeaders, base_array<rl_terrain_renderer::RenderChunk>& chunks, base_array<unsigned int>& keys)
{
    rl_terrain_renderer::RenderChunkHeader* pHeader = chunkHeaders.add_in_place();
    const vector& render_offset = renderer.get_render_offset();
    const vector& target = pInstance->get_target_position();
    pHeader->translation = make_float4(vector(target.x - render_offset.x, target.y - render_offset.y, target.z - render_offset.z), 0.0f);
    pHeader->zoneOffset = make_float4(rl_terrain_subzone_offset(pInstance->get_position()), 0.0f);
    pHeader->tintColor = terrain_tint(renderer, pInstance->get_tint(), pInstance->get_opacity());
    const rl_mesh* mesh_p = (const rl_mesh*)pInstance->get_renderable();
    pHeader->pGeometry = &mesh_p->getRenderGeometry();
    pHeader->pInstance = pInstance;
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[pInstance->get_sub_mesh_index()];
    rl_terrain_renderer::RenderChunk* pChunks = nullptr;
    if (chunks.size() + submesh.num_render_blocks > chunks.capacity())
    {
        // Game trace only: the overflow writes through null below (as rfg.exe). The render offset follows the camera.
        GAME_TRACE("render", "terrain chunks overflow: %d + %d of %d, headers %d, render offset (%g %g %g), instance at (%g %g %g)", chunks.size(), (int)submesh.num_render_blocks, chunks.capacity(),
                   chunkHeaders.size(), render_offset.x, render_offset.y, render_offset.z, pInstance->get_position().x, pInstance->get_position().y, pInstance->get_position().z);
        VLIB_CRITICAL_FAILURE();
    }
    else
    {
        pChunks = chunks.elements() + chunks.size();
        chunks.resize(chunks.size() + submesh.num_render_blocks);
    }
    unsigned int* pKeys = nullptr;
    if (keys.size() + submesh.num_render_blocks > keys.capacity())
    {
        VLIB_CRITICAL_FAILURE();
    }
    else
    {
        pKeys = keys.elements() + keys.size();
        keys.resize(keys.size() + submesh.num_render_blocks);
    }
    rl_render_block* blocks = submesh.render_blocks.to_pointer();
    const int lod_level = pInstance->get_lod_level();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        unsigned int material_index = blocks[b].material_map_idx;
        // Magic number: material slots 0-2 are the LOD materials, 3 and up side maps.
        const bool is_sidemap = material_index >= 3;
        if (!is_sidemap)
        {
            material_index = lod_level < 1 ? lod_level : 1;
        }
        rl_material* material_p = terrain_material(pInstance->get_material_map()->get_material(material_index));
        pChunks[b].pHeader = pHeader;
        pChunks[b].materialType = is_sidemap ? 1 : 0;
        pChunks[b].pMaterial = material_p;
        pChunks[b].pRenderBlock = &blocks[b];
        pKeys[b] = (is_sidemap ? 0x80000000u : 0u) | ((unsigned int)material_p >> 1);
    }
}

// 0x0051AA80 (:1767)
// The chunk's instance constants, its material when it differs from the current one, and its render block.
const rl_material* heightMeshInstanceRenderChunk(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, unsigned int lodLevel, rl_terrain_renderer::RenderChunk* pChunk, const rl_material* pCurrentMaterial)
{
    const rl_terrain_renderer::RenderChunkHeader* pHeader = pChunk->pHeader;
    heightMeshInstanceSetInstance(pCommandWriter, pContext, pHeader->translation, pHeader->zoneOffset, pHeader->tintColor, nullptr);
    if (pChunk->pMaterial != pCurrentMaterial)
    {
        const rl_material_data* pMaterialData = pChunk->pMaterial->get_material_data();
        if (pChunk->materialType == 0)
        {
            setHeightMeshMaterialShaderConstants(pCommandWriter, pContext, techType, lodLevel, 0, pMaterialData);
        }
        else
        {
            setSideMapMaterialShaderConstants(pCommandWriter, pContext, techType, lodLevel, pMaterialData);
        }
        setMaterial(pCommandWriter, pContext, techType, pMaterialData);
    }
    draw_render_block(pCommandWriter, *pHeader->pGeometry, *pChunk->pRenderBlock);
    return pChunk->pMaterial;
}

// 0x0051AC10 (:1796)
// The patch's instance constants and all its render blocks, without materials.
void heightMeshInstanceRenderDepthOnly(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, unsigned int lodLevel, rl_height_mesh_instance* pInstance, rl_renderer& renderer)
{
    const vector& render_offset = renderer.get_render_offset();
    const vector& target = pInstance->get_target_position();
    const keen::float4 translation = make_float4(vector(target.x - render_offset.x, target.y - render_offset.y, target.z - render_offset.z), 0.0f);
    const keen::float4 zone_offset = make_float4(rl_terrain_subzone_offset(pInstance->get_position()), 0.0f);
    const keen::float4 tint = terrain_tint(renderer, pInstance->get_tint(), pInstance->get_opacity());
    heightMeshInstanceSetInstance(pCommandWriter, pContext, translation, zone_offset, tint, nullptr);
    const rl_mesh* mesh_p = (const rl_mesh*)pInstance->get_renderable();
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[pInstance->get_sub_mesh_index()];
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        draw_render_block(pCommandWriter, mesh_p->getRenderGeometry(), blocks[b]);
    }
}

// 0x0051B0D0 (:1867)
// The list's LOD comes from the list (high 0, blend 1, combined 2). Depth only draws each visible patch with the
// depth pipeline; the other techniques draw the LOD's sorted chunks, switching materials only when they change. With
// g_terrainUseNewRenderer off, every visible instance draws itself (with the depth only material as the override
// in a depth pass).
void rl_terrain_renderer::render_pass(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type techType, base_array<rl_renderable_instance*>& instance_list, bool skip_decals, unsigned int vis_pass_flags)
{
    if (!g_terrainUseNewRenderer)
    {
        renderer.set_shader_type(shader_type_terrain);
        renderer.set_shader_configuration(0);
        const bool use_depth_material = renderer.get_depth_only() && m_depth_only_material_handle != 0xFFFFFFFF;
        if (use_depth_material)
        {
            rl_material_library_set_override(m_depth_only_material_handle);
        }
        for (int i = 0; i < instance_list.size(); i++)
        {
            rl_renderable_instance* instance_p = instance_list[i];
            if ((instance_p->get_vis_pass_flags() & vis_pass_flags) != 0)
            {
                instance_p->render(renderer, camera, 0);
            }
        }
        if (use_depth_material)
        {
            rl_material_library_set_override(0xFFFFFFFF);
        }
        return;
    }
    unsigned int lod_level;
    if (&instance_list == &m_high_lod_list)
    {
        lod_level = 0;
    }
    else
    {
        lod_level = &instance_list != &m_blend_lod_list ? 2 : 1;
    }
    if (techType == rl_shader_technique_depth_only)
    {
        // Magic number: binding 18, the depth technique's LOD 0 plain patch (pHeightMeshBindings[(2 * 3 + 0) * 3 + 0]).
        pCommandWriter->writeSetShaderPipelineCommand(m_pContext->heightMeshVariants.getPipeline(rl_shader_technique_depth_only, 0, 0), m_pContext->pHeightMeshBindings[0x12], true);
        for (int i = 0; i < instance_list.size(); i++)
        {
            rl_height_mesh_instance* instance_p = (rl_height_mesh_instance*)instance_list[i];
            if ((instance_p->get_vis_pass_flags() & vis_pass_flags) != 0)
            {
                heightMeshInstanceRenderDepthOnly(pCommandWriter, m_pContext, lod_level, instance_p, renderer);
            }
        }
        return;
    }
    farray<RenderChunk*,2048>& chunks = lod_level == 0 ? m_highLodChunks : (lod_level == 1 ? m_blendLodChunks : m_combineLodChunks);
    const rl_material* current_material_p = nullptr;
    for (int i = 0; i < chunks.size(); i++)
    {
        RenderChunk* pChunk = chunks[i];
        if ((pChunk->pHeader->pInstance->get_vis_pass_flags() & vis_pass_flags) != 0)
        {
            current_material_p = heightMeshInstanceRenderChunk(pCommandWriter, m_pContext, techType, lod_level, pChunk, current_material_p);
        }
    }
}

// 0x0051B330 (:1939)
void lowLodInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, const rl_material_data* pMaterialData)
{
    setMaterial(pCommandWriter, pContext, pass, pMaterialData);
}

// 0x0051B340 (:1944)
// The patch translation (relative to the render offset) and every render block with its material's textures.
// Notes (decompiled behaviour, kept as is): only the translation of the instance constants is written, and a block
// whose material handle does not resolve dereferences a null material.
void lowLodInstanceRender(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, rl_low_lod_mesh_instance* pInstance, rl_renderer& renderer, rl_camera&, unsigned int)
{
    const rl_mesh* mesh_p = (const rl_mesh*)pInstance->get_renderable();
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[pInstance->get_sub_mesh_index()];
    keen::TerrainVertexShaderConstants* pVertex = (keen::TerrainVertexShaderConstants*)pCommandWriter->beginDynamicVertexConstants(pContext->pTerrainVertexConstants, TERRAIN_INSTANCE_CONSTANTS_SLOT);
    const vector& position = pInstance->get_position();
    const vector& render_offset = renderer.get_render_offset();
    pVertex->Translation = make_float4(vector(position.x - render_offset.x, position.y - render_offset.y, position.z - render_offset.z), 0.0f);
    pCommandWriter->endDynamicVertexConstants();
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        const rl_material* material_p = terrain_material(pInstance->get_material_map()->get_material(blocks[b].material_map_idx));
        setMaterial(pCommandWriter, pContext, pass, material_p->get_material_data());
        draw_render_block(pCommandWriter, mesh_p->getRenderGeometry(), blocks[b]);
    }
}

// 0x0051B6C0 (:1988)
// Untinted, with the low LOD pipeline of the technique.
void rl_terrain_renderer::render_low_lod(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type pass, base_array<rl_scene_entity*>& instances, unsigned int vis_pass_flags)
{
    keen::TerrainFragmentShaderConstants* pFragment = (keen::TerrainFragmentShaderConstants*)pCommandWriter->beginDynamicFragmentConstants(m_pContext->pTerrainFragmentConstants, TERRAIN_INSTANCE_CONSTANTS_SLOT);
    pFragment->Tint_color.x = 1.0f;
    pFragment->Tint_color.y = 1.0f;
    pFragment->Tint_color.z = 1.0f;
    pFragment->Tint_color.w = 1.0f;
    pCommandWriter->endDynamicFragmentConstants();
    pCommandWriter->writeSetShaderPipelineCommand(m_pContext->lowLodVariants.getPipeline(pass), m_pContext->pLowLodBindings[pass], true);
    for (int i = 0; i < instances.size(); i++)
    {
        rl_low_lod_mesh_instance* instance_p = (rl_low_lod_mesh_instance*)instances[i];
        if ((instance_p->get_vis_pass_flags() & vis_pass_flags) == 0)
        {
            continue;
        }
        if (!g_terrainUseNewRenderer)
        {
            instance_p->render(renderer, camera, 0);
        }
        else
        {
            lowLodInstanceRender(pCommandWriter, m_pContext, pass, instance_p, renderer, camera, 0);
        }
    }
}

// 0x0051B880 (:2021)
void heightMeshSkirtInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, unsigned int lodLevel, unsigned int skirtType, const rl_material_data* pMaterialData, const keen::float4& translation, const keen::float4& zoneOffset, const keen::float4& tintColor, const keen::GraphicsMatrix43* pTM)
{
    heightMeshInstanceSetInstance(pCommandWriter, pContext, translation, zoneOffset, tintColor, pTM);
    setHeightMeshMaterialShaderConstants(pCommandWriter, pContext, pass, lodLevel, skirtType, pMaterialData);
    setMaterial(pCommandWriter, pContext, pass, pMaterialData);
}

// 0x0051B8E0 (:2029)
// The skirt's sub mesh with the terrain material of its LOD (clamped to the map) and the skirt's transform (sub mesh
// offset times world, relative to the render offset, transposed) in the instance constants.
void heightMeshSkirtInstanceRender(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, rl_skirt_mesh_instance* pInstance, rl_renderer& renderer, unsigned int skirtType)
{
    const keen::float4 zone_offset = make_float4(rl_terrain_subzone_offset(pInstance->get_position()), 0.0f);
    const keen::float4 tint = terrain_tint(renderer, pInstance->get_tint(), pInstance->get_opacity());
    const int lod_level = pInstance->get_lod_level();
    const rl_material_map* material_map_p = pInstance->get_terrain_material_map();
    const int last_material = material_map_p->get_num_materials() - 1;
    const rl_material* material_p = terrain_material(material_map_p->get_material(lod_level < last_material ? lod_level : last_material));
    const rl_mesh* mesh_p = (const rl_mesh*)pInstance->get_renderable();
    const rl_mesh_data_runtime* mesh_data_p = mesh_p->get_mesh_data();
    const unsigned int submesh_index = pInstance->get_submesh_index();
    if (submesh_index >= (unsigned int)mesh_data_p->num_sub_meshes)
    {
        return;
    }
    const rl_submesh_data& submesh = mesh_data_p->sub_meshes[submesh_index];
    const matrix43 offset(submesh.offset_transform, IDENTITY_MATRIX);
    matrix43 transform;
    transform.multiply_two(offset, pInstance->get_world_transform());
    const vector& render_offset = renderer.get_render_offset();
    transform.m_translation.x -= render_offset.x;
    transform.m_translation.y -= render_offset.y;
    transform.m_translation.z -= render_offset.z;
    transform.transpose();
    keen::GraphicsMatrix43 tm;
    memcpy(&tm, &transform, sizeof(tm));
    keen::float4 translation;
    translation.x = 0.0f;
    translation.y = 0.0f;
    translation.z = 0.0f;
    translation.w = 0.0f;
    const rl_material_data* pMaterialData = material_p->get_material_data();
    heightMeshInstanceSetInstance(pCommandWriter, pContext, translation, zone_offset, tint, &tm);
    setHeightMeshMaterialShaderConstants(pCommandWriter, pContext, techType, lod_level, skirtType, pMaterialData);
    setMaterial(pCommandWriter, pContext, techType, pMaterialData);
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        draw_render_block(pCommandWriter, mesh_p->getRenderGeometry(), blocks[b]);
    }
}

// 0x0051BF10 (:2092)
// Opaque (type 1) or alpha (type 2) skirts. With g_terrainUseNewRenderer off, opaque skirts draw themselves.
void rl_terrain_renderer::render_skirts(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type techType, base_array<rl_scene_entity*>& instances, bool skipDecals, bool hasAlpha, unsigned int vis_pass_flags)
{
    const unsigned int render_flags = skipDecals ? 2 : 0;
    const unsigned int skirt_type = hasAlpha + 1;
    for (int i = 0; i < instances.size(); i++)
    {
        rl_skirt_mesh_instance* instance_p = (rl_skirt_mesh_instance*)instances[i];
        if ((instance_p->get_vis_pass_flags() & vis_pass_flags) == 0)
        {
            continue;
        }
        if (!g_terrainUseNewRenderer && !hasAlpha)
        {
            instance_p->render(renderer, camera, render_flags);
        }
        else
        {
            heightMeshSkirtInstanceRender(pCommandWriter, m_pContext, techType, instance_p, renderer, skirt_type);
        }
    }
}

// 0x005000F0 (:2119)
// The road material: vertex constants (translation, layer scale/offsets from rows 4-6, subzone offset and tile from
// rows 10-11), fragment constants (tint, detail map, spec alpha and power from rows 7-8, diffuse colors 0-3, edge
// fade from row 9), then the pipeline of its layer count (the road shader handles 2, 3, 4 layers) and technique.
void setRoadMeshShaderAndConstants(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, const rl_material_data* pMaterialData, const keen::float4& translation, const keen::float4& tintColor)
{
    const keen::float4* constants = material_constants(pMaterialData);
    keen::TerrainRoadsVertexShaderConstants* pVertex = (keen::TerrainRoadsVertexShaderConstants*)pCommandWriter->beginDynamicVertexConstants(pContext->pRoadsVertexConstants, TERRAIN_ROADS_CONSTANTS_SLOT);
    pVertex->Translation = translation;
    pVertex->Terrain_layer0_scale_offset = constants[4];
    pVertex->Terrain_layer1_scale_offset = constants[5];
    pVertex->Terrain_layer2_scale_offset = constants[6];
    pVertex->Road_subzone_offset = constants[10];
    pVertex->Tile = constants[11];
    pCommandWriter->endDynamicVertexConstants();
    keen::TerrainRoadsFragmentShaderConstants* pFragment = (keen::TerrainRoadsFragmentShaderConstants*)pCommandWriter->beginDynamicFragmentConstants(pContext->pRoadsFragmentConstants, TERRAIN_ROADS_CONSTANTS_SLOT);
    pFragment->Tint_color = tintColor;
    pFragment->Terrain_layer_spec_alpha = constants[7];
    pFragment->Terrain_layer_spec_power = constants[8];
    keen::float4 specular_scale;
    keen::float4 specular_power;
    setTweakableShaderConstants(pContext, techType, pMaterialData, 0, pFragment->detailMapTilingScaleBiasBlend, specular_scale, specular_power, nullptr);
    pFragment->Diffuse_color_0 = constants[0];
    pFragment->Diffuse_color_1 = constants[1];
    pFragment->Diffuse_color_2 = constants[2];
    pFragment->Diffuse_color_3 = constants[3];
    pFragment->Edge_fade__use_spec_map = constants[9];
    pCommandWriter->endDynamicFragmentConstants();
    unsigned int layer;
    if (pMaterialData->shader_handle == pContext->roadShaderHandles[0])
    {
        layer = 0;
    }
    else
    {
        layer = pMaterialData->shader_handle != pContext->roadShaderHandles[1] ? 2 : 1;
    }
    pCommandWriter->writeSetShaderPipelineCommand(pContext->roadsVariants.getPipeline(layer, techType), pContext->pRoadsBindings[techType * 3 + layer], true);
}

// 0x0051BFB0 (:2168)
void roadMeshInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, const rl_material_data* pMaterialData, const keen::float4& translation, const keen::float4& tintColor)
{
    setRoadMeshShaderAndConstants(pCommandWriter, pContext, pass, pMaterialData, translation, tintColor);
    setMaterial(pCommandWriter, pContext, pass, pMaterialData);
}

// 0x0051BFF0 (:2175)
// Every render block of the road's sub mesh with a material in the map, at the sub mesh offset relative to the render
// offset.
// Notes (decompiled behaviour, kept as is): a block whose material handle does not resolve dereferences a null
// material.
void roadMeshInstanceRender(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, rl_mesh_instance* pInstance, rl_renderer& renderer)
{
    const keen::float4 tint = terrain_tint(renderer, pInstance->get_tint(), pInstance->get_opacity());
    const rl_mesh* mesh_p = (const rl_mesh*)pInstance->get_renderable();
    const rl_submesh_data& submesh = mesh_p->get_mesh_data()->sub_meshes[pInstance->get_submesh_index()];
    const vector& position = pInstance->get_position();
    const vector& render_offset = renderer.get_render_offset();
    const keen::float4 translation = make_float4(vector(submesh.offset_transform.x + position.x - render_offset.x, submesh.offset_transform.y + position.y - render_offset.y, submesh.offset_transform.z + position.z - render_offset.z), 0.0f);
    const rl_material_map* material_map_p = pInstance->get_material_map();
    const rl_render_block* blocks = submesh.render_blocks.to_pointer();
    for (int b = 0; b < submesh.num_render_blocks; b++)
    {
        const unsigned int material_index = blocks[b].material_map_idx;
        if (material_index >= (unsigned int)material_map_p->get_num_materials())
        {
            continue;
        }
        const rl_material_data* pMaterialData = terrain_material(material_map_p->get_material(material_index))->get_material_data();
        setRoadMeshShaderAndConstants(pCommandWriter, pContext, pass, pMaterialData, translation, tint);
        setMaterial(pCommandWriter, pContext, pass, pMaterialData);
        draw_render_block(pCommandWriter, mesh_p->getRenderGeometry(), blocks[b]);
    }
}

// 0x0051C3F0 (:2230)
// With g_terrainUseNewRenderer off, the roads draw themselves.
void rl_terrain_renderer::render_roads(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type techType, base_array<rl_scene_entity*>& instances, bool skipDecals, unsigned int vis_pass_flags)
{
    const unsigned int render_flags = skipDecals ? 2 : 0;
    for (int i = 0; i < instances.size(); i++)
    {
        rl_mesh_instance* instance_p = (rl_mesh_instance*)instances[i];
        if ((instance_p->get_vis_pass_flags() & vis_pass_flags) == 0)
        {
            continue;
        }
        if (!g_terrainUseNewRenderer)
        {
            instance_p->render(renderer, camera, render_flags);
        }
        else
        {
            roadMeshInstanceRender(pCommandWriter, m_pContext, techType, instance_p, renderer);
        }
    }
}

// 0x0051A7A0 (rl_terrain_renderer.inl:23)
pool_list<rl_renderable_instance*>& rl_terrain_renderer::get_visible_renderable_instances()
{
    return m_visible_renderable_instances;
}

// 0x0051A7B0 (rl_terrain_renderer.inl:28)
pool_list<rl_scene_entity*>& rl_terrain_renderer::get_visible_low_lod_instances()
{
    return m_visible_low_lod_instances;
}

// 0x0051A7C0 (rl_terrain_renderer.inl:33)
pool_list<rl_scene_entity*>& rl_terrain_renderer::get_visible_landmark_lod_instances()
{
    return m_visible_landmark_lod_instances;
}

// 0x0051A750 (.h:106)
pool_list<rl_scene_entity*>& rl_terrain_renderer::get_visible_skirt_instances()
{
    return m_visible_skirt_instances;
}

// 0x0051A760 (.h:107)
pool_list<rl_scene_entity*>& rl_terrain_renderer::get_visible_alpha_skirt_instances()
{
    return m_visible_alpha_skirt_instances;
}

// 0x0051A770 (.h:108)
pool_list<rl_scene_entity*>& rl_terrain_renderer::get_visible_road_instances()
{
    return m_visible_road_instances;
}

// 0x0051A780 (rl_terrain_renderer.inl:2)
rl_scene_renderer* rl_terrain_renderer::get_scene_renderer() const
{
    return m_scene_renderer_p;
}

// 0x0051A790 (rl_terrain_renderer.inl:7)
void rl_terrain_renderer::set_scene_renderer(rl_scene_renderer* scene_renderer_p)
{
    m_scene_renderer_p = scene_renderer_p;
}

// 0x004D44F0 (:2255)
bool rl_terrain_renderer::is_landmark_z_range_enabled()
{
    return !Rl_blur_landmarks;
}

// 0x004D4500 (:2260)
float rl_terrain_renderer::get_landmark_z_range_reserve()
{
    return RL_reserved_landmark_depth_range;
}

// 0x004D4510 (:2265)
void rl_terrain_renderer::render_debug()
{
}
