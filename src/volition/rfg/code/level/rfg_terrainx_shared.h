// Original: volition/rfg/code/level/rfg_terrainx_shared.h
// Implementation: volition/rfg/code/level/rfg_terrainx_shared.cpp
//
// Terrain file formats, shared with the tools. Placement of the structures inferred (the PDB records no inline code for
// them).
// - .cterrain_pc / .gterrain_pc (one per territory cell, GSP_TERRAIN, terrain_load_terrain): terrain_header, the
//   texture, foliage mesh and stitch piece name lists (terrain_process_header), then terrain_data: the low LOD patches,
//   materials, the layer map, undergrowth, invisible barriers, path finding data and an et_ptr_offset per high LOD
//   subzone that is filled in while a subzone is loaded.
// - .ctmesh_pc / .gtmesh_pc (one per subzone, GSP_TERRAIN_HIGH_LOD, terrain_load_subzone): terrain_subzone_data with
//   the height mesh patches, decals, stitch pieces (terrain_stitch_instance_data, with their own physics) and road
//   meshes.
// All pointers in the data are et_ptr_offset (self-relative) and are resolved in place.
#pragma once

#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"
#include "volition/rfg/code/objects/pblock_decal.h"

class object;
class rl_mesh;
class rl_material;
class rl_material_map;
struct rl_mesh_data;
class rl_height_mesh_instance;
class rl_skirt_mesh_instance;
class rl_mesh_instance;
class rl_renderable_instance;
struct terrain_lowlod_patch;
struct terrain_sidemap_data;

// Path finding data of a territory cell (offsets in the file are absolute and turned into et_ptr_offsets on load).
// size 0x2C
struct terrain_pf_data
{
    int version; // 0x00
    int block_size; // 0x04
    et_ptr_offset<unsigned char,0> pf_data_block; // 0x08
    unsigned int pf_data_block_size; // 0x0C
    et_ptr_offset<unsigned char,0> pf_mopp_block; // 0x10
    unsigned int pf_mopp_block_size; // 0x14
    et_ptr_offset<unsigned char,0> vehicle_pf_data_block; // 0x18
    unsigned int vehicle_pf_data_block_size; // 0x1C
    et_ptr_offset<unsigned char,0> vehicle_pf_mopp_block; // 0x20
    unsigned int vehicle_pf_mopp_block_size; // 0x24
    unsigned int total_size; // 0x28
};

// size 0x8
struct invisible_barrier_data
{
    invisible_barrier_data();

    unsigned int cm_index; // 0x00
    unsigned int havok_handle; // 0x04
};

// A stitch piece of a territory cell with its own collision (version 29 and later).
// size 0x38
struct terrain_stitch_physics_instance
{
    vector m_position; // 0x00
    matrix m_rotation; // 0x0C
    unsigned int m_havok_handle; // 0x30
    et_ptr_offset<char,0> m_stitch_chunk_name; // 0x34
};

// size 0x18
struct terrain_occluder
{
    float min_y; // 0x00
    float max_y; // 0x04
    vector2 pt1; // 0x08
    vector2 pt2; // 0x10
};

// size 0x10
struct undergrowth_layer_model_data
{
    et_ptr_offset<char,0> model_name; // 0x00
    unsigned short model_probability; // 0x04
    unsigned char model_flags; // 0x06
    unsigned char scale_noise; // 0x07
    unsigned int color_1; // 0x08
    unsigned int color_2; // 0x0C
};

// size 0x1C
struct undergrowth_layer_data
{
    int num_models; // 0x00
    et_ptr_offset<undergrowth_layer_model_data,0> models; // 0x04
    float max_density; // 0x08
    float max_fade_dist; // 0x0C
    float min_fade_dist; // 0x10
    int placement_method; // 0x14
    int rand_seed; // 0x18
};

// size 0x4
struct undergrowth_cell_data
{
    unsigned short layer_data_index; // 0x00
    unsigned short num_layer_datas; // 0x02
};

// size 0xA
struct undergrowth_cell_layer_data
{
    unsigned char layer_index; // 0x00
    unsigned char density; // 0x01
    unsigned char bit_mask[8]; // 0x02
};

// size 0x18
struct single_undergrowth_data
{
    unsigned int mesh_index; // 0x00
    vector pos; // 0x04
    float scale; // 0x10
    float color_lerp; // 0x14
};

// size 0xC
struct single_undergrowth_cell_layer_data
{
    unsigned int num_single_ug; // 0x00
    et_ptr_offset<unsigned short,0> num_extra_models; // 0x04
    et_ptr_offset<single_undergrowth_data,0> single_ug; // 0x08
};

// size 0x24
struct terrain_header
{
    unsigned int signature; // 0x00
    unsigned int version; // 0x04
    unsigned int num_texture_name_strings; // 0x08
    unsigned int texture_names_size; // 0x0C
    unsigned int num_fmesh_names; // 0x10
    unsigned int fmesh_names_size; // 0x14
    unsigned int stitch_piece_names_size; // 0x18
    unsigned int num_stitch_piece_names; // 0x1C
    unsigned int num_stitch_pieces; // 0x20
};

// size 0x14
struct terrain_stitch_info
{
    terrain_stitch_info(); // 0x00922170

    vector2 m_bmin; // 0x00
    vector2 m_bmax; // 0x08
    et_ptr_offset<char,0> m_filename; // 0x10
};

// size 0x20
struct terrain_layer_map
{
    int res_x; // 0x00
    int res_y; // 0x04
    int bit_depth; // 0x08
    int data_size; // 0x0C
    et_ptr_offset<unsigned char,0> data_p; // 0x10
    int num_materials; // 0x14
    et_ptr_offset<et_ptr_offset<char,0>,0> material_names; // 0x18
    et_ptr_offset<int,0> material_index; // 0x1C
};

// size 0x30
struct terrain_renderable_data
{
    et_ptr_offset<rl_mesh_data,0> mesh_data_p; // 0x00
    et_ptr_offset<rl_mesh,0> renderable_p; // 0x04
    vector aabb_min; // 0x08
    vector aabb_max; // 0x14
    vector bsphere_position; // 0x20
    float bsphere_radius; // 0x2C
};

// size 0x68
struct terrain_stitch_instance_data
{
    // size 0x20
    struct skirt
    {
        et_ptr_offset<rl_skirt_mesh_instance,1> blend_renderable_instance_p; // 0x00
        et_ptr_offset<rl_skirt_mesh_instance,1> generated_renderable_instance_p; // 0x04
        int generated_skirt_submesh_idx; // 0x08
        vector position; // 0x0C
        unsigned int dead_data_1; // 0x18
        unsigned int dead_data_2; // 0x1C
    };

    et_ptr_offset<char,0> stitch_chunk_name; // 0x00
    int num_skirts; // 0x04
    et_ptr_offset<skirt,0> skirts_p; // 0x08
    int num_stitched_meshes; // 0x0C
    et_ptr_offset<et_ptr_offset<rl_mesh_instance,0>,0> stitched_meshes_p; // 0x10
    vector position; // 0x14
    matrix rotation; // 0x20
    unsigned int havok_handle; // 0x44
    unsigned char pad[32]; // 0x48
};

// size 0x28
struct road_mesh_data
{
    int m_num_mesh_instances; // 0x00
    et_ptr_offset<rl_material,0> m_material_p; // 0x04
    unsigned int m_material_handle; // 0x08
    et_ptr_offset<rl_material_map,0> m_material_map_p; // 0x0C
    et_ptr_offset<rl_mesh_data,0> m_mesh_data_p; // 0x10
    et_ptr_offset<rl_mesh,0> m_mesh_p; // 0x14
    et_ptr_offset<et_ptr_offset<rl_renderable_instance,0>,0> m_mesh_instances_p; // 0x18
    vector m_position; // 0x1C
};

// size 0x450
struct terrain_subzone_data
{
    // size 0x60
    struct patch
    {
        et_ptr_offset<rl_height_mesh_instance,0> instance_p; // 0x00
        vector position; // 0x04
        matrix rotation; // 0x10
        int submesh_index; // 0x34
        vector local_aabb_min; // 0x38
        vector local_aabb_max; // 0x44
        vector local_bsphere_position; // 0x50
        float local_bsphere_radius; // 0x5C
    };

    // size 0x2C
    struct decal
    {
        pblock_decal decal_data; // 0x00
        int num_terrain_patches; // 0x24
        et_ptr_offset<int,0> terrain_patch_indices; // 0x28
    };

    int subzone_index; // 0x00
    vector position; // 0x04
    int patch_count; // 0x10
    et_ptr_offset<patch,0> patches; // 0x14
    terrain_renderable_data renderable_data; // 0x18
    int num_decals; // 0x48
    et_ptr_offset<decal,0> decals; // 0x4C
    et_ptr_offset<rl_mesh_data,0> stitch_mesh_data_p; // 0x50
    et_ptr_offset<rl_mesh,1> stitch_renderable_p; // 0x54
    int num_stitch_pieces; // 0x58
    et_ptr_offset<terrain_stitch_instance_data,0> stitch_pieces; // 0x5C
    int num_road_decal_meshes; // 0x60
    et_ptr_offset<road_mesh_data,0> road_decal_meshes; // 0x64
    int m_header_version; // 0x68
    unsigned char pad[996]; // 0x6C
};

// size 0x428
struct terrain_data
{
    vector bmin; // 0x00
    vector bmax; // 0x0C
    int x_res; // 0x18
    int z_res; // 0x1C
    int m_num_occluders; // 0x20
    et_ptr_offset<terrain_occluder,0> m_occluders; // 0x24
    et_ptr_offset<rl_material_map,0> m_terrain_material_map_p; // 0x28
    et_ptr_offset<et_ptr_offset<rl_material,0>,0> m_terrain_materials; // 0x2C
    unsigned int m_num_terrain_materials; // 0x30
    unsigned int m_minimap_material_handle; // 0x34
    et_ptr_offset<rl_material,0> m_minimap_material_p; // 0x38
    et_ptr_offset<terrain_lowlod_patch,0> m_low_lod_patches_p; // 0x3C
    et_ptr_offset<rl_material,0> m_low_lod_material_p; // 0x40
    et_ptr_offset<rl_material_map,0> m_low_lod_material_map_p; // 0x44
    int m_num_subzones; // 0x48
    et_ptr_offset<et_ptr_offset<terrain_subzone_data,1>,0> m_subzones; // 0x4C
    et_ptr_offset<terrain_pf_data,0> m_pf_data_p; // 0x50
    terrain_layer_map m_layer_map; // 0x54
    unsigned int m_num_ug_layers; // 0x74
    et_ptr_offset<undergrowth_layer_data,0> m_ug_layers; // 0x78
    et_ptr_offset<undergrowth_cell_data,0> m_ug_cell_data; // 0x7C
    int m_num_ug_cell_layer_datas; // 0x80
    et_ptr_offset<undergrowth_cell_layer_data,0> m_ug_cell_layer_data; // 0x84
    et_ptr_offset<single_undergrowth_cell_layer_data,0> m_sug_cell_layer_data; // 0x88
    unsigned int m_stitch_piece_cm_index; // 0x8C
    int m_num_invisible_barriers; // 0x90
    et_ptr_offset<invisible_barrier_data,0> m_invisible_barriers; // 0x94
    unsigned int m_shape_handle; // 0x98
    int m_num_sidemap_materials; // 0x9C
    et_ptr_offset<terrain_sidemap_data,1> m_sidemap_data; // 0xA0
    et_ptr_offset<object,0> m_object_p_stub; // 0xA4
    et_ptr_offset<terrain_stitch_physics_instance,0> m_stitch_physics_instances_p; // 0xA8
    int m_num_stitch_physics_instances; // 0xAC
    object* m_object_p; // 0xB0
    unsigned int m_object_p_padding; // 0xB4
    char pad[880]; // 0xB8
};

bool terrain_process_header(const char* filename, unsigned char* data, int size, int& offset, terrain_header* header, char*& texture_filenames, char*& stitch_filenames, char*& fmesh_filenames, terrain_stitch_info*& stitch_infos); // 0x0091B390 (:21)
bool terrain_load_filenames_SLOW(const char* filename, unsigned char* data, int data_size, str_data_hash_table<unsigned char,1024,64,unsigned short>* texture_list_unique, str_data_hash_table<unsigned char,1024,64,unsigned short>* fmesh_list_unique, terrain_stitch_info*& stitch_infos, int& num_stitch_infos, vlib_platform plat); // 0x009314E0 (:76)
bool restricted_area_2d_point_in_polygon(const vector2& pt, const vector2* polygon, int num_pts); // 0x0091B4D0 (:173)
