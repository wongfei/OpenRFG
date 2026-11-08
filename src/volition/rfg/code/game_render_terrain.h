// Original: volition/rfg/code/game_render_terrain.h
// Implementation: volition/rfg/code/game_render_terrain.cpp
//
// Creation and release of the terrain renderables of streamed zones: stitched pieces and roads (render side of
// terrain_stitch_instance_data / road_mesh_data), low LOD patches, subzone renderables with their side map materials,
// and the landmark LODs streamed in separately. Placement of terrain_lowlod_patch, terrain_landmark_lod and
// terrain_sidemap_data inferred (serialized terrain render data, only used here).
#pragma once

#include "volition/render_lib/code/rl_landmark_lod_mesh_instance.h"
#include "volition/render_lib/code/rl_low_lod_mesh_instance.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"

class object;
struct road_mesh_data;
struct terrain_data;
struct terrain_stitch_instance_data;
struct terrain_subzone_data;

// size 0x30
struct terrain_lowlod_patch
{
    et_ptr_offset<rl_low_lod_mesh_instance,0> m_renderable_p; // 0x00
    et_ptr_offset<rl_mesh_data,0> m_mesh_data_p; // 0x04
    et_ptr_offset<rl_mesh,0> m_mesh_p; // 0x08
    vector m_aabb_min; // 0x0C
    vector m_aabb_max; // 0x18
    vector m_position; // 0x24
};

// size 0x40
struct terrain_landmark_lod
{
    unsigned int m_sig; // 0x00
    unsigned int m_version; // 0x04
    vector m_aabb_min; // 0x08
    vector m_aabb_max; // 0x14
    vector m_position; // 0x20
    et_ptr_offset<rl_landmark_lod_mesh_instance,0> m_renderable_p; // 0x2C
    et_ptr_offset<rl_mesh_data,0> m_mesh_data_p; // 0x30
    et_ptr_offset<rl_mesh,0> m_mesh_p; // 0x34
    et_ptr_offset<rl_material,0> m_material_p; // 0x38
    et_ptr_offset<rl_material_map,0> m_material_map_p; // 0x3C
};

// size 0x8
struct terrain_sidemap_data
{
    et_ptr_offset<et_ptr_offset<rl_material,0>,0> materials; // 0x00
    et_ptr_offset<unsigned int,0> material_handles; // 0x04
};

extern bool R_stitch_batch; // 0x01668713
extern int Num_instanciated_stitch_meshes; // 0x01D59C30

void game_render_terrain_create_stitched_piece(terrain_stitch_instance_data* instance_data_p, terrain_data* terrain_data_p, object* objp, bool do_roads, terrain_subzone_data* subzone_data_p); // 0x005D9AC0 (:80)
void game_render_terrain_release_stitched_piece(terrain_stitch_instance_data* instance_data_p); // 0x005DA1F0 (:235)
void game_render_terrain_create_road_mesh(road_mesh_data* road_data_p, terrain_data* terrain_data_p); // 0x005DA320 (:268)
void game_render_terrain_release_road_mesh(road_mesh_data* road_data_p); // 0x005D6C70 (:296)
void game_render_terrain_create_low_lod_mesh(terrain_lowlod_patch* patch_data_p, rl_material_map* material_map_p, object* objp); // 0x005DA470 (:307)
void game_render_terrain_release_low_lod_mesh(terrain_lowlod_patch* patch_data_p); // 0x005D6CC0 (:330)
void game_render_terrain_level_init(); // 0x005D0C60 (:340)
void game_render_terrain_level_shutdown(); // 0x005D0C70 (:349)
void game_render_terrain_debug(); // 0x005D0C80 (:355)
terrain_lowlod_patch* game_render_terrain_load_lowlod(unsigned char* cpu_data_p, int& cpu_offset, unsigned char* gpu_data_p, int& gpu_offset, const char* pFilename); // 0x005DA720 (:379)
void game_render_terrain_create_subzone_renderables(terrain_subzone_data* subzone_data_p, rl_material_map* material_map_p, terrain_sidemap_data* sidemap_data); // 0x005DA9B0 (:411)
void game_render_terrain_release_subzone_renderables(terrain_subzone_data* subzone_data_p); // 0x005D6CE0 (:451)
terrain_landmark_lod* load_landmark_lod(unsigned char* cpu_data_p, int& cpu_offset, unsigned char* gpu_data_p, int& gpu_offset, const char* pName); // 0x005DACD0
void unload_landmark_lod(terrain_landmark_lod* landmark_header); // 0x005D6D20 (:531)
bool game_render_terrain_landmark_lod_stream(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x005DAEB0 (:553)
void game_render_terrain_landmark_lod_unstream(const char* filename, unsigned int srid); // 0x005D6D90 (:594)
