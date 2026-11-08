// Original: volition/rfg/code/geom/cloth_simulation_shared.h
// Implementation: volition/rfg/code/geom/cloth_simulation_shared.cpp
//
// The cloth simulation data shared with the tools: the streamed cloth file (nodes, rods, collision primitives, offsets
// fixed up to pointers on load) and its preload. Placement of the data types inferred.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/endian_type.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool;
class mempool_base;

// size 0x24
struct simulated_node_info
{
    signed char bone_index; // 0x00
    signed char parent_node_index; // 0x01
    signed char gravity_link; // 0x02
    signed char unused; // 0x03
    unsigned int collide; // 0x04
    float wind_multiplier; // 0x08
    vector pos; // 0x0C
    vector local_space_pos; // 0x18
};

// size 0x10
struct simulated_node_link_info
{
    unsigned short node_index_1; // 0x00
    unsigned short node_index_2; // 0x02
    unsigned int collide; // 0x04
    float length; // 0x08
    float stretch_len; // 0x0C
};

// size 0x34
struct cloth_sim_collision_primitive_info
{
    int bone_index; // 0x00
    int is_capsule; // 0x04
    float radius; // 0x08
    float height; // 0x0C
    vector pos; // 0x10
    vector axis; // 0x1C
    vector local_space_pos; // 0x28
};

// size 0x34
struct cloth_sim_collision_primitive
{
    bool valid; // 0x00
    vector last_frame_pos; // 0x04
    matrix last_frame_orient; // 0x10
};

// size 0x54
struct cloth_sim_info
{
    int version; // 0x00
    char name[28]; // 0x04
    int num_passes; // 0x20
    float air_resistance; // 0x24
    float wind_multiplier; // 0x28
    float wind_const; // 0x2C
    float gravity_multiplier; // 0x30
    float object_velocity_inheritance; // 0x34
    int num_anchor_nodes; // 0x38
    int num_simulated_nodes; // 0x3C
    int num_node_links; // 0x40
    int num_colliders; // 0x44
    et_ptr_offset<simulated_node_info,0> nodes; // 0x48
    et_ptr_offset<simulated_node_link_info,0> node_links; // 0x4C
    et_ptr_offset<cloth_sim_collision_primitive_info,0> colliders; // 0x50
};

void cloth_sim_preload_all_shared(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, int dlc_id); // 0x007FBC20 (:21)
