// Original: volition/rfg/code/ai/combat/ai_thrown_weapon_logic.h
// Implementation: volition/rfg/code/ai/combat/ai_thrown_weapon_logic.cpp
//
// Thrown weapon logic: computing a grenade or charge throw (direct or over cover) for a situation and the path of the
// thrown object.
#pragma once

#include "volition/rfg/code/objects/object_cover_node_enums.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

struct ai_belief;
class cover_node;
class human;
struct nav_cell;
class npc;
class weapon;
struct weapon_info;

enum ai_throw_types
{
    AI_THROW_TYPE_UNDEFINED = -1,
    AI_THROW_TYPE_LOB = 0,
    AI_THROW_TYPE_DIRECT = 1,
    AI_THROW_TYPE_BOUNCE = 2,
    AI_THROW_TYPE_ROLL = 3,
    NUM_AI_THROW_TYPES = 4,
};

enum ai_throw_situations
{
    AI_THROW_SITUATION_NORMAL = 0,
    AI_THROW_SITUATION_ACQUIRE_VISUAL = 1,
    AI_THROW_SITUATION_VEHICLE = 2,
    NUM_AI_THROW_SITUATIONS = 3,
};

// size 0xA0
struct ai_thrown_weapon_info
{
    // size 0x1
    struct ai_thrown_weapon_info_flags
    {
        bool start_on_navmesh : 1; // 0x00:0
        bool target_on_navmesh : 1; // 0x00:1
        bool half_navmesh_path : 1; // 0x00:2
        bool full_navmesh_path : 1; // 0x00:3
        bool direct_raycast : 1; // 0x00:4
        bool lob_raycast : 1; // 0x00:5
        bool wall_raycast : 1; // 0x00:6
    };

    ai_thrown_weapon_info(); // 0x00657270
    ~ai_thrown_weapon_info(); // 0x006572A0

    const npc* attacker; // 0x00
    const weapon_info* w_info; // 0x04
    float fuse_time; // 0x08
    float max_dist; // 0x0C
    float start_speed; // 0x10
    ai_throw_situations situation; // 0x14
    const human* target; // 0x18
    const ai_belief* target_belief; // 0x1C
    vector target_pos; // 0x20
    bool check_cover_nodes; // 0x2C
    const cover_node* target_cover_node; // 0x30
    cover_node_stances target_cover_stance; // 0x34
    cover_node_firing_positions target_cover_stepout; // 0x38
    float safe_dist_sq; // 0x3C
    human* closest_ally; // 0x40
    bool check_closest_ally; // 0x44
    vector throw_start_pos; // 0x48
    vector throw_end_pos; // 0x54
    vector throw_velocity; // 0x60
    ai_thrown_weapon_info_flags check_flags; // 0x6C
    ai_thrown_weapon_info_flags result_flags; // 0x6D
    nav_cell* target_nav_cell; // 0x70
    vector target_navmesh_pos; // 0x74
    farray<enum ai_throw_types,4> available_throw_types; // 0x80
};

bool ai_thrown_weapon_calculate_throw(vector& throw_velocity, const npc* np, const weapon* wp, const human* target, ai_throw_situations situation, bool lead_target, ai_throw_types* throw_type); // 0x00693570 (:1295)
