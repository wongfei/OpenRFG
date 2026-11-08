// Original: volition/render_lib/code/rl_terrain_renderer.h
// Implementation: volition/render_lib/code/rl_terrain_renderer.cpp
//
// rl_terrain_renderer, draws the terrain (layers, LODs, render chunks) with its render context. Placement of the
// terrain mode enum, rl_iterrain_renderer and TerrainRenderContext inferred.
#pragma once

#include "game/build/win32/steam/master/shader/eosglass_sv.hpp"
#include "game/build/win32/steam/master/shader/glass_sv.hpp"
#include "game/build/win32/steam/master/shader/heightmesh_sv.hpp"
#include "game/build/win32/steam/master/shader/heightmeshlowlod_sv.hpp"
#include "game/build/win32/steam/master/shader/sidemap_sv.hpp"
#include "game/build/win32/steam/master/shader/substance_sv.hpp"
#include "game/build/win32/steam/master/shader/terrainblend_sv.hpp"
#include "game/build/win32/steam/master/shader/terrainroads_sv.hpp"
#include "game/build/win32/steam/master/shader/terrainrocks_sv.hpp"
#include "keen/base/types.hpp"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

struct TerrainRenderContext;
class rl_camera;
class rl_class_dynamic_info;
class rl_height_mesh_instance;
class rl_light;
class rl_low_lod_mesh_instance;
class rl_material;
struct rl_material_data;
class rl_mesh_instance;
class rl_primitive_state;
struct rl_render_block;
class rl_renderable_instance;
class rl_renderer;
class rl_scene_entity;
class rl_scene_renderer;
class rl_skirt_mesh_instance;

namespace keen
{
    struct DynamicConstantBuffer;
    class GraphicsCommandWriter;
    struct GraphicsMatrix43;
    struct GraphicsSystem;
    class MemoryAllocator;
    struct RenderGeometry;
    struct SamplerState;
    struct VertexFormat;
    struct VertexInputBinding;
}

enum rl_terrain_mode
{
    RL_TERRAIN_ALL = 0,
    RL_TERRAIN_NO_LOW_LOD = 1,
    RL_TERRAIN_ONLY_LOW_LOD = 2,
};

// size 0x8
class rl_iterrain_renderer : public rl_base_object
{
public:
    virtual bool init() = 0; // vtbl+0x08
    virtual void prerender(const rl_camera*, rl_renderer&) = 0; // vtbl+0x0C
    virtual void render(unsigned int) = 0; // vtbl+0x10
    virtual void render_depth(rl_renderer&, rl_camera&, const rl_light*, unsigned int, bool) = 0; // vtbl+0x14
    virtual void reset() = 0; // vtbl+0x18
    virtual void render_base_pass(rl_renderer&, rl_camera&, const rl_light*, unsigned int, rl_terrain_mode, bool) = 0; // vtbl+0x1C
    virtual void render_lighting_pass(rl_renderer&, rl_camera&, const rl_light*, unsigned int, bool) = 0; // vtbl+0x20
    virtual void render_conditional_pass(rl_renderer&, rl_camera&, unsigned int) = 0; // vtbl+0x24
    virtual void prerender_landmarks(rl_renderer&, rl_camera&) = 0; // vtbl+0x28
    virtual void render_landmarks(rl_renderer&, rl_camera&) = 0; // vtbl+0x2C
    virtual void render_alpha_skirts(rl_renderer&, rl_camera&, const rl_light*, unsigned int) = 0; // vtbl+0x30
    virtual pool_list<rl_renderable_instance*>& get_visible_renderable_instances() = 0; // vtbl+0x34
    virtual pool_list<rl_scene_entity*>& get_visible_low_lod_instances() = 0; // vtbl+0x38
    virtual pool_list<rl_scene_entity*>& get_visible_landmark_lod_instances() = 0; // vtbl+0x3C
    virtual pool_list<rl_scene_entity*>& get_visible_skirt_instances() = 0; // vtbl+0x40
    virtual pool_list<rl_scene_entity*>& get_visible_road_instances() = 0; // vtbl+0x44
    virtual pool_list<rl_scene_entity*>& get_visible_alpha_skirt_instances() = 0; // vtbl+0x48
    virtual rl_scene_renderer* get_scene_renderer() const = 0; // vtbl+0x4C
    virtual void set_scene_renderer(rl_scene_renderer*) = 0; // vtbl+0x50
    rl_iterrain_renderer(); // 0x004D4380
    virtual ~rl_iterrain_renderer(); // vtbl+0x00 0x004D4390

};

// size 0x3D308
class rl_terrain_renderer : public rl_iterrain_renderer
{
public:
    enum
    {
        MaxInstancesPerLodList = 1024,
        MaxChunksPerLodList = 2048,
    };

    // size 0x38
    struct RenderChunkHeader
    {
        keen::float4 translation; // 0x00
        keen::float4 zoneOffset; // 0x10
        keen::float4 tintColor; // 0x20
        const keen::RenderGeometry* pGeometry; // 0x30
        rl_renderable_instance* pInstance; // 0x34
    };

    // size 0x10
    struct RenderChunk
    {
        RenderChunkHeader* pHeader; // 0x00
        unsigned int materialType; // 0x04
        rl_render_block* pRenderBlock; // 0x08
        rl_material* pMaterial; // 0x0C
    };

protected:
    rl_terrain_renderer(); // 0x0051A520 (:803)
    virtual ~rl_terrain_renderer(); // vtbl+0x00 0x00533930 (:813)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_terrain_renderer* create(); // 0x005338F0 (:795)
    static bool can_create(); // 0x004D4350 (:795)
    virtual void release(); // vtbl+0x04 0x004D4360 (:795)
    virtual bool init(); // vtbl+0x08 0x00533A80 (:824)
    virtual void prerender(const rl_camera* camera_p, rl_renderer& renderer); // vtbl+0x0C 0x0055D290 (:872)
    virtual void render(unsigned int vis_pass_flags); // vtbl+0x10 0x004FF8D0 (:1052)
    virtual void render_depth(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags, bool isShadowPass); // vtbl+0x14 0x00534360 (:1450)
    virtual void reset(); // vtbl+0x18 0x0051A7D0 (:850)
    virtual void render_base_pass(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags, rl_terrain_mode terrain_mode, bool skip_decals); // vtbl+0x1C 0x00533CF0 (:1106)
    virtual void render_lighting_pass(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags, bool skip_decals); // vtbl+0x20 0x00534240 (:1418)
    virtual void render_conditional_pass(rl_renderer&, rl_camera&, unsigned int); // vtbl+0x24 0x004D4470 (:1174)
    virtual void prerender_landmarks(rl_renderer& renderer, rl_camera& camera); // vtbl+0x28 0x00534100 (:1235)
    virtual void render_landmarks(rl_renderer& renderer, rl_camera& camera); // vtbl+0x2C 0x00534130 (:1341)
    virtual void render_alpha_skirts(rl_renderer& renderer, rl_camera& camera, const rl_light* light_p, unsigned int vis_pass_flags); // vtbl+0x30 0x00533EA0 (:1179)
    virtual pool_list<rl_renderable_instance*>& get_visible_renderable_instances(); // vtbl+0x34 0x0051A7A0 (rl_terrain_renderer.inl:23)
    virtual pool_list<rl_scene_entity*>& get_visible_low_lod_instances(); // vtbl+0x38 0x0051A7B0 (rl_terrain_renderer.inl:28)
    virtual pool_list<rl_scene_entity*>& get_visible_landmark_lod_instances(); // vtbl+0x3C 0x0051A7C0 (rl_terrain_renderer.inl:33)
    virtual pool_list<rl_scene_entity*>& get_visible_skirt_instances(); // vtbl+0x40 0x0051A750 (.h:106)
    virtual pool_list<rl_scene_entity*>& get_visible_alpha_skirt_instances(); // vtbl+0x48 0x0051A760 (.h:107)
    virtual pool_list<rl_scene_entity*>& get_visible_road_instances(); // vtbl+0x44 0x0051A770 (.h:108)
    virtual rl_scene_renderer* get_scene_renderer() const; // vtbl+0x4C 0x0051A780 (rl_terrain_renderer.inl:2)
    virtual void set_scene_renderer(rl_scene_renderer* scene_renderer_p); // vtbl+0x50 0x0051A790 (rl_terrain_renderer.inl:7)
    int get_terrain_chunks_rendered() const;
    static bool is_landmark_z_range_enabled(); // 0x004D44F0 (:2255)
    static float get_landmark_z_range_reserve(); // 0x004D4500 (:2260)
    static void set_visible_renderable_instances_max(int);
    static int get_visible_renderable_instances_max();
protected:
    void render_skirts(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type techType, base_array<rl_scene_entity*>& instances, bool skipDecals, bool hasAlpha, unsigned int vis_pass_flags); // 0x0051BF10 (:2092)
    void render_roads(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type techType, base_array<rl_scene_entity*>& instances, bool skipDecals, unsigned int vis_pass_flags); // 0x0051C3F0 (:2230)
    virtual void render_pass(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type techType, base_array<rl_renderable_instance*>& instance_list, bool skip_decals, unsigned int vis_pass_flags); // vtbl+0x54 0x0051B0D0 (:1867)
    virtual void render_low_lod(keen::GraphicsCommandWriter* pCommandWriter, rl_renderer& renderer, rl_camera& camera, rl_shader_technique_type pass, base_array<rl_scene_entity*>& instances, unsigned int vis_pass_flags); // vtbl+0x58 0x0051B6C0 (:1988)
    void render_landmarks_internal(rl_renderer& renderer, rl_camera& camera); // 0x00533F70 (:1203)
    void render_debug(); // 0x004D4510 (:2265)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x015163E8
protected:
    bool m_initialized; // 0x08
    pool_list<rl_renderable_instance*> m_visible_renderable_instances; // 0x0C
    pool_list<rl_scene_entity*> m_visible_low_lod_instances; // 0x24
    pool_list<rl_scene_entity*> m_visible_landmark_lod_instances; // 0x3C
    pool_list<rl_scene_entity*> m_visible_skirt_instances; // 0x54
    pool_list<rl_scene_entity*> m_visible_alpha_skirt_instances; // 0x6C
    pool_list<rl_scene_entity*> m_visible_road_instances; // 0x84
    farray<rl_renderable_instance*,1024> m_high_lod_list; // 0x9C
    farray<rl_renderable_instance*,1024> m_blend_lod_list; // 0x10AC
    farray<rl_renderable_instance*,1024> m_combine_lod_list; // 0x20BC
    farray<RenderChunkHeader,2048> m_chunkHeader; // 0x30CC
    farray<RenderChunk,2048> m_chunksLod0; // 0x1F0DC
    farray<RenderChunk,2048> m_chunksLod1; // 0x270EC
    farray<RenderChunk,2048> m_chunksLod2; // 0x2F0FC
    farray<RenderChunk*,2048> m_highLodChunks; // 0x3710C
    farray<RenderChunk*,2048> m_blendLodChunks; // 0x3911C
    farray<RenderChunk*,2048> m_combineLodChunks; // 0x3B12C
    farray<rl_scene_entity*,100> m_low_lod_scene_list; // 0x3D13C
    unsigned int m_depth_only_material_handle; // 0x3D2DC
    rl_material* m_depth_only_material; // 0x3D2E0
private:
    TerrainRenderContext* m_pContext; // 0x3D2E4
    static const int VISIBLE_RENDERABLE_INSTANCES_MAX_DEFAULT; // 0x012BECE8
    static const int VISIBLE_LOW_LOD_INSTANCES_MAX_DEFAULT; // 0x012BECEC
    static const int VISIBLE_LANDMARK_LOD_INSTANCES_MAX_DEFAULT; // 0x012BECF0
    static rl_terrain_renderer* ms_current_instance_p; // 0x01B7BD44
    static int ms_visible_renderable_instances_max; // 0x0150A60C
    static int ms_visible_low_lod_instances_max; // 0x0150A610
    static int ms_visible_landmark_lod_instances_max; // 0x0150A614
    rl_scene_renderer* m_scene_renderer_p; // 0x3D2E8
    int m_terrain_chunks_rendered; // 0x3D2EC
    vector m_camera_position; // 0x3D2F0
    unsigned int m_landmark_effect_index; // 0x3D2FC
    unsigned int m_landmark_filter_effect_index; // 0x3D300
    int m_landmark_filter_uv_xform_loc; // 0x3D304
};

// size 0x728
struct TerrainRenderContext
{
    keen::DynamicConstantBuffer* pTerrainVertexConstants; // 0x00
    keen::DynamicConstantBuffer* pTerrainFragmentConstants; // 0x04
    keen::DynamicConstantBuffer* pHeightmeshMaterialVertexConstants; // 0x08
    keen::DynamicConstantBuffer* pHeightmeshMaterialFragmentConstants; // 0x0C
    keen::DynamicConstantBuffer* pSidemapMaterialVertexConstants; // 0x10
    keen::DynamicConstantBuffer* pSidemapMaterialFragmentConstants; // 0x14
    keen::DynamicConstantBuffer* pRoadsVertexConstants; // 0x18
    keen::DynamicConstantBuffer* pRoadsFragmentConstants; // 0x1C
    keen::HeightMeshVariants heightMeshVariants; // 0x20
    keen::SideMapVariants sideMapVariants; // 0x1D0
    keen::HeightMeshLowLodVariants lowLodVariants; // 0x290
    keen::TerrainRoadsVariants roadsVariants; // 0x2C0
    keen::TerrainRocksVariants rocksVariants; // 0x338
    keen::TerrainBlendVariants blendVariants; // 0x38C
    keen::SubstanceVariants substanceVariants; // 0x3E0
    keen::GlassVariants glassVariants; // 0x4D0
    keen::EosGlassVariants eosGlassVariants; // 0x548
    const keen::VertexFormat* pHeightMeshVertexFormat; // 0x5D8
    const keen::VertexFormat* pHeightMeshStitchVertexFormat; // 0x5DC
    const keen::VertexFormat* pHeightMeshStitchAlphaVertexFormat; // 0x5E0
    const keen::VertexFormat* pLowLodVertexFormat; // 0x5E4
    const keen::VertexFormat* pRoadsVertexFormat; // 0x5E8
    const keen::VertexInputBinding* pHeightMeshBindings[27]; // 0x5EC
    const keen::VertexInputBinding* pSideMapBindings[12]; // 0x658
    const keen::VertexInputBinding* pLowLodBindings[3]; // 0x688
    const keen::VertexInputBinding* pRoadsBindings[9]; // 0x694
    const keen::SamplerState* pSamplerStates[3]; // 0x6B8
    const keen::SamplerState* pSamplerStateLookup[16]; // 0x6C4
    unsigned int terrainLowLodShaderHandle; // 0x704
    unsigned int heightmeshShaderHandle; // 0x708
    unsigned int heightmeshLowLodShaderHandle; // 0x70C
    unsigned int heightmeshBlendShaderHandle; // 0x710
    unsigned int sidemapMultiShaderHandle; // 0x714
    unsigned int sidemapSingleShaderHandle; // 0x718
    unsigned int roadShaderHandles[3]; // 0x71C
};

void createSamplerStates(TerrainRenderContext* pContext, keen::GraphicsSystem* pGraphicsSystem); // 0x004FF630 (:147)
void destroySamplerStates(TerrainRenderContext* pContext, keen::GraphicsSystem* pGraphicsSystem); // 0x004D4310 (:185)
TerrainRenderContext* createTerrainRenderContext(keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem); // 0x00531BD0 (:192)
void destroyTerrainRenderContext(TerrainRenderContext* pContext, keen::MemoryAllocator* pAllocator, keen::GraphicsSystem* pGraphicsSystem); // 0x004FF780 (:752)
void beginPass(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_renderer& renderer, rl_primitive_state& oldState, rl_shader_technique_type pass, const rl_light*); // 0x0051A7F0 (:1063)
void endPass(rl_renderer& renderer, rl_primitive_state& state); // 0x0051A8F0 (:1081)
void renderDecals(rl_renderer& renderer, base_array<rl_renderable_instance*>& instances, unsigned int vis_pass_flags); // 0x004FF8E0 (:1087)
void setTweakableShaderConstants(TerrainRenderContext* pContext, rl_shader_technique_type techType, const rl_material_data* pMaterialData, unsigned int diffuseSlotStart, keen::float4& detailMaskParams, keen::float4& specularScale, keen::float4& specularPower, float* pEnableSpecMap); // 0x004D4480 (:1493)
void setHeightMeshMaterialShaderConstants(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, unsigned int lodLevel, unsigned int skirtType, const rl_material_data* pMaterialData); // 0x004FF980 (:1558)
void setSideMapMaterialShaderConstants(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, unsigned int lodLevel, const rl_material_data* pMaterialData); // 0x004FFC90 (:1600)
void setMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, const rl_material_data* pMaterialData); // 0x0051A910 (:1643)
void heightMeshInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, unsigned int lodLevel, unsigned int materialType, const rl_material_data* pMaterialData); // 0x0051AA20 (:1670)
void heightMeshInstanceSetInstance(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, const keen::float4& translation, const keen::float4& zoneOffset, const keen::float4& tintColor, const keen::GraphicsMatrix43* pTM); // 0x004FFF60 (:1684)
void heightMeshInstanceFillChunks(unsigned int, rl_height_mesh_instance* pInstance, rl_renderer& renderer, base_array<rl_terrain_renderer::RenderChunkHeader>& chunkHeaders, base_array<rl_terrain_renderer::RenderChunk>& chunks, base_array<unsigned int>& keys); // 0x005344C0 (:1705)
const rl_material* heightMeshInstanceRenderChunk(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, unsigned int lodLevel, rl_terrain_renderer::RenderChunk* pChunk, const rl_material* pCurrentMaterial); // 0x0051AA80 (:1767)
void heightMeshInstanceRenderDepthOnly(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, unsigned int lodLevel, rl_height_mesh_instance* pInstance, rl_renderer& renderer); // 0x0051AC10 (:1796)
void lowLodInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, const rl_material_data* pMaterialData); // 0x0051B330 (:1939)
void lowLodInstanceRender(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, rl_low_lod_mesh_instance* pInstance, rl_renderer& renderer, rl_camera&, unsigned int); // 0x0051B340 (:1944)
void heightMeshSkirtInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, unsigned int lodLevel, unsigned int skirtType, const rl_material_data* pMaterialData, const keen::float4& translation, const keen::float4& zoneOffset, const keen::float4& tintColor, const keen::GraphicsMatrix43* pTM); // 0x0051B880 (:2021)
void heightMeshSkirtInstanceRender(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, rl_skirt_mesh_instance* pInstance, rl_renderer& renderer, unsigned int skirtType); // 0x0051B8E0 (:2029)
void setRoadMeshShaderAndConstants(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type techType, const rl_material_data* pMaterialData, const keen::float4& translation, const keen::float4& tintColor); // 0x005000F0 (:2119)
void roadMeshInstanceSetMaterial(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, const rl_material_data* pMaterialData, const keen::float4& translation, const keen::float4& tintColor); // 0x0051BFB0 (:2168)
void roadMeshInstanceRender(keen::GraphicsCommandWriter* pCommandWriter, TerrainRenderContext* pContext, rl_shader_technique_type pass, rl_mesh_instance* pInstance, rl_renderer& renderer); // 0x0051BFF0 (:2175)
void rl_terrain_renderer_set_enable_lowlod(bool); // 0x004D4520 (:2286)
