// Original: volition/rfg/code/code_effects/glass.h
// Implementation: volition/rfg/code/code_effects/glass.cpp
//
// Breakable glass: a glass object is a pane (outline, plane, material) of a mover's mesh; impacts cut circles into it and
// clip shards from the pane's polygons, which become simple physics pieces (Glass_shard_pool, 750) until they settle and
// fade. The shattered pane is rebuilt as a dynamic mesh each frame. Glass tints come from glass.xtbl. The movers keep
// their glass objects in rfg_mover::m_glass_obj_list.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/render_lib/code/rl_mesh_data.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class rfg_mover;
class rl_color_float;
class rl_compound_mesh_instance;
class rl_material_map;
class rl_mesh;
class rl_skin_instance;

// size 0x64
struct glass_circle
{
    glass_circle(); // 0x006ECCC0 (:124)
    ~glass_circle(); // 0x006E5440

    float m_radius; // 0x00
    farray<vector2,10> m_points; // 0x04
};

// size 0x60
struct glass_poly
{
    glass_poly(); // 0x006EF570
    ~glass_poly(); // 0x006EB6F0

    farray<vector,3> m_points; // 0x00
    farray<vector2,3> m_uvs; // 0x34
    bool m_winding; // 0x5C
};

// size 0x98
struct glass_shard
{
    glass_shard(); // 0x006F27D0

    glass_poly m_poly; // 0x00
    vector m_velocity; // 0x60
    float m_rotation; // 0x6C
    float m_rand_variance; // 0x70
    vector m_rotation_axis; // 0x74
    vector m_centroid; // 0x80
    bool m_merged; // 0x8C
    glass_shard* next; // 0x90
    glass_shard* prev; // 0x94
};

// size 0x24
struct glass_pane
{
    bool encompassed(vector2 point, float radius); // 0x006E3D00 (:488)
    glass_pane(); // 0x006E5E40

    vector2 m_p0; // 0x00
    vector2 m_p1; // 0x08
    vector2 m_p2; // 0x10
    vector2 m_p3; // 0x18
    float m_thickness; // 0x20
};

// size 0x2A8
struct glass_obj
{
    void queue_shatter_effect(const vector& impact_point, const vector& impact_force); // 0x006E97B0 (:980)
    void create_shatter_effect(); // 0x006F3EB0 (:990)
    void update_render(); // 0x006F07C0 (:1378)
    void create_mesh(); // 0x006F28B0 (:540)
    void physics_init(); // 0x006E8EF0 (:833)
    glass_obj(); // 0x006EF530

    static const int SHARDS_MAX; // 0x012C70D8
    static const int MAX_SHARDS_PER_INSTANCE; // 0x012C70DC
    static const int MAX_INSTANCES; // 0x012C70E0
    rl_render_block m_render_block[5]; // 0x00
    rl_submesh_data m_submesh[5]; // 0x64
    rl_mesh_data_runtime m_mesh_data[5]; // 0x140
    glass_pane m_pane; // 0x1E0
    matrix43 m_world_xform; // 0x204
    vector m_impact_point; // 0x234
    vector m_impact_force; // 0x240
    float m_impact_time; // 0x24C
    const rl_material_map* m_material_map_p; // 0x250
    unsigned short m_material_map_index; // 0x254
    linked_list_p<glass_shard> m_shards; // 0x258
    int m_num_instances; // 0x260
    rl_mesh* m_mesh_p[5]; // 0x264
    rl_skin_instance* m_mesh_instance_p[5]; // 0x278
    void* m_vertex_stream_p; // 0x28C
    int m_vertex_alloc_size; // 0x290
    glass_obj* next; // 0x294
    glass_obj* prev; // 0x298
    glass_obj* mover_next; // 0x29C
    glass_obj* mover_prev; // 0x2A0
    rfg_mover* mover_p; // 0x2A4
};

extern bool Glass_enabled; // 0x01648F80
extern bool Glass_debug; // 0x01DEE4B6
extern bool Glass_debug_full; // 0x01DEE4B5
extern bool Glass_merge; // 0x01648D2B
extern int Glass_merge_rand; // 0x01648F7C
extern float Glass_impact_force_dir_percent; // 0x01648F50
extern float Glass_impact_force_scale; // 0x01648F54
extern float Glass_impact_force_min; // 0x01648F58
extern float Glass_impact_force_max; // 0x01648F5C
extern float Glass_rotation_min; // 0x01648F60
extern float Glass_rotation_max; // 0x01648F64
extern float Glass_rotation_scale; // 0x01648F74
extern float Glass_gravity; // 0x01648F68
extern float Glass_damping; // 0x01648F6C
extern float Glass_rotational_damping; // 0x01648F70
extern float Glass_exp; // 0x01648F78

void unused_dcf_glass_impact(); // 0x006E36F0 (:167)
glass_obj* glass_obj_create(rfg_mover* mover_p, int submesh_index, const matrix43& subinst_xform, const vector& impact_point, const vector& impact_force); // 0x006ECD70 (:246)
void glass_obj_release(glass_obj* glass_obj_p); // 0x006F0510 (:332)
bool shards_share_sides(glass_shard* shard_0_p, glass_shard* shard_1_p); // 0x006E8C80 (:815)
void glass_obj_do_frame(); // 0x006F6530 (:966)
void glass_get_tint(rl_compound_mesh_instance* reference_p, int submesh_index, rl_color_float& ret); // 0x006ED300 (:1492)
void parse_glass_table(); // 0x006E41F0 (:1580)
void unused_dcf_reload_glass_table(); // 0x006E4400 (:1644)
void glass_system_init(); // 0x006F1150 (:1650)
void glass_system_shutdown(); // 0x006E4410 (:1692)
void glass_level_init_pre(); // 0x006E4450 (:1708)
void glass_level_shutdown(); // 0x006F11B0 (:1712)
