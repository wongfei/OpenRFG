// Original: volition/rfg/code/objects/turret_internal.h
// Implementation: volition/rfg/code/objects/turret_internal.cpp
//
// Turret physics: the three components of a turret (base, middle, top; static meshes or vehicle components) built
// into Havok bodies held together by constraints with position and velocity motors that aim the turret, with the
// grip and weapon mount points.
#pragma once

#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class hkpConstraintInstance;
class hkpPositionConstraintMotor;
class hkpVelocityConstraintMotor;
class rl_mesh_instance;
struct static_mesh;
class turret;
class vehicle;

enum turret_components
{
    TURRET_COMPONENT_NONE = -1,
    TURRET_COMPONENT_BASE = 0,
    TURRET_COMPONENT_MIDDLE = 1,
    TURRET_COMPONENT_TOP = 2,
    NUM_TURRET_COMPONENTS = 3,
};

// size 0x28
struct turret_component_physics_info
{
    void clear(); // 0x00A20EF0 (:1494)

    static_mesh* smesh; // 0x00
    rl_mesh_instance* smesh_instance; // 0x04
    bool is_veh_component; // 0x08
    int veh_component_idx; // 0x0C
    int attach_tag_idx; // 0x10
    unsigned int attach_tag_offset; // 0x14
    unsigned int havok_handle; // 0x18
    hkpConstraintInstance* hconstraint; // 0x1C
    hkpPositionConstraintMotor* pos_motor; // 0x20
    hkpVelocityConstraintMotor* vel_motor; // 0x24
};

// size 0xF0
struct turret_physics
{
    void init(); // 0x00A44C00 (:1508)
    void clear_objects(); // 0x00A20F20 (:1530)
    turret_physics(); // 0x00A264A0

    unsigned int detour_request_handle; // 0x00
    unsigned int base_constraint_handle; // 0x04
    turret_component_physics_info components[3]; // 0x08
    bool grip_tags_valid; // 0x80
    int left_grip_tag_index; // 0x84
    int right_grip_tag_index; // 0x88
    turret_components secondary_weapon_attach_component; // 0x8C
    int secondary_weapon_attach_tag_idx; // 0x90
    unsigned int secondary_weapon_attach_offset; // 0x94
    float pitch; // 0x98
    float heading; // 0x9C
    float heading_sum; // 0xA0
    vector kill_angular_impulse; // 0xA4
    vector kill_linear_impulse; // 0xB0
    timestamp update_tag_time; // 0xBC
    vector last_local_tag_pos; // 0xC0
    matrix last_local_tag_orient; // 0xCC
};

// The original's inlined clamp, as its asm compares: below the minimum first, then not below the maximum.
inline float turret_clamp(float x, float minimum, float maximum)
{
    if (x - minimum < 0.0f)
    {
        x = minimum;
    }
    if (0.0f <= x - maximum)
    {
        x = maximum;
    }
    return x;
}

extern unsigned char Default_turret_physical_material_index; // 0x01694703, set by turret_system_init
extern unsigned char Default_turret_effect_material_index; // 0x01694784

bool turret_physics_constrain_to_floor(turret_physics* physics, turret* tp); // 0x00A96D60 (:367)
turret_physics* turret_physics_alloc(); // 0x00A74050 (:1551)
void turret_physics_free(turret_physics* physics); // 0x00A97020 (:1564)
bool turret_physics_stream_load(turret_physics* physics, turret* tp, vehicle* vp); // 0x00AB0830 (:1572)
void turret_physics_stream_unload(turret_physics* physics, turret* tp, vehicle* vp); // 0x00A44CD0 (:1611)
void turret_physics_children_streamed(turret_physics* physics, turret* tp); // 0x00A44D10 (:1626)
void turret_physics_kill_turret(turret_physics* physics, turret* tp); // 0x00A74080 (:1638)
void turret_physics_get_tag_world_coords(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index, vector& tag_pos, matrix* tag_orient); // 0x00A44D40 (:1666)
void turret_physics_get_local_bbox(turret_physics* physics, const turret* tp, vector& bmin, vector& bmax); // 0x00A44EC0 (:1714)
void turret_physics_cycle_tag(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index); // 0x00A451A0 (:1766)
void turret_physics_reset_cycle(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index); // 0x00A45380 (:1844)
void turret_physics_play_alt_effect(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index); // 0x00A20F80 (:1870)
bool turret_physics_process_door_weapon_ready(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index); // 0x00A453E0 (:1914)
bool turret_physics_process_door_weapon_unready(turret_physics* physics, const turret* tp, const vehicle* vp, int tag_index); // 0x00A45650 (:2010)
void turret_physics_transform_to_tag(turret_physics* physics, const vehicle* vp); // 0x00A21090 (:2104)
bool turret_physics_fix_pos_orient(turret_physics* physics, vehicle* vp); // 0x00A45870 (:2142)
void turret_physics_update_renderables(turret_physics* physics); // 0x00A45E30 (:2232)
void turret_physics_update_objects(turret_physics* physics, turret* tp, const vehicle* vp); // 0x00A21210 (:2248)
void turret_physics_aim_pos_motors(turret_physics* physics); // 0x00A21390 (:2295)
void turret_physics_aim_vel_motors(turret_physics* physics, float h_vel, float v_vel); // 0x00A21470 (:2323)
void turret_physics_update_heading(turret_physics* physics, turret* tp); // 0x00A216C0 (:2349)
void turret_physics_set_no_collide(turret_physics* physics, turret* tp, unsigned int object_handle); // 0x00A218D0 (:2384)
void turret_physics_set_control_type(turret_physics* physics, bool is_player, bool is_player_aimed); // 0x00A21940 (:2393)
