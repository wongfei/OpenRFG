// Original: volition/rfg/code/havok/havok_ragdoll.h
// Implementation: volition/rfg/code/havok/havok_ragdoll.cpp
//
// Ragdolls: a fixed set of ragdoll slots whose memory is added per level; a human binds to a slot
// (havok_ragdoll_bind_to_human, by ragdoll_priority) and its bones follow the ragdoll bodies until it is unbound.
// ragdoll_part names the bodies. supplemental_constraint is a simple spring that pulls a body towards a point.
// Placement of ragdoll_part, ragdoll_priority and the supplemental constraint types inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class character_instance;
struct collision_event;
class hkpConstraintInstance;
class hkpRigidBody;
class human;
class matrix;

enum ragdoll_part
{
    RAGDOLL_CLOSEST = -2,
    RAGDOLL_ROOT = -1,
    RAGDOLL_HEAD = 0,
    RAGDOLL_BODY = 1,
    RAGDOLL_SPINE = 2,
    RAGDOLL_UPPER_BODY = 3,
    RAGDOLL_UR_LEG = 4,
    RAGDOLL_UL_LEG = 5,
    RAGDOLL_LR_LEG = 6,
    RAGDOLL_LL_LEG = 7,
    RAGDOLL_UR_ARM = 8,
    RAGDOLL_UL_ARM = 9,
    RAGDOLL_LR_ARM = 10,
    RAGDOLL_LL_ARM = 11,
    NUM_RAGDOLL_BONES = 12,
};

enum ragdoll_priority
{
    RAGDOLL_PRIORITY_BULLET_DAMAGE = 0,
    RAGDOLL_PRIORITY_CREATE = 1,
    RAGDOLL_PRIORITY_EXPLOSIVE_DAMAGE = 1,
    RAGDOLL_PRIORITY_ALWAYS = 2,
};

enum supplemental_constraint_type
{
    SCT_POINT_POINT = 0,
    SCT_SIMPLE_SPRING = 1,
    NUM_SUPPLEMENTAL_CONSTRAINT_TYPES = 2,
};

// size 0x34
struct supplemental_constraint
{
    supplemental_constraint(); // 0x00811BD0

    supplemental_constraint_type m_type; // 0x00
    hkpRigidBody* m_body_a; // 0x04
    vector m_point_a; // 0x08
    vector m_point_b; // 0x14
    hkpRigidBody* m_body_b; // 0x20
    hkpConstraintInstance* m_constraint; // 0x24
    unsigned int handle; // 0x28
    unsigned int tag_index; // 0x2C
    struct
    {
        unsigned int active : 1; // 0x00:0
        unsigned int faked_body : 1; // 0x00:1
        unsigned int constraint_added : 1; // 0x00:2
    } m_flags; // 0x30
};

void havok_ragdoll_store_matrices(int idx, character_instance* cip); // 0x00812110 (:830)
void havok_ragdoll_recall_matrices(int idx, character_instance* cip); // 0x00812220 (:861)
void havok_ragdoll_init_relative_orients(int idx, character_instance* cip); // 0x0081C750 (:884)
unsigned int havok_ragdoll_get_collision_filter(int ragdoll_idx); // 0x0081CA60 (:962)
void havok_ragdoll_set_collision_filter(int ragdoll_idx, unsigned int collision); // 0x0081CA90 (:972)
void havok_ragdoll_get_pos(int ragdoll_idx, vector& pos, const ragdoll_part body_part); // 0x0081CB00 (:2184)
void havok_ragdoll_get_vel(int ragdoll_idx, vector& vel); // 0x0081CB80 (:2199)
void apply_simple_spring_constraint(supplemental_constraint* scp); // 0x00812300 (:2215)
void havok_collide_ragdoll(const collision_event* c_event); // 0x00825220 (:2291)
void havok_ragdoll_system_init(); // 0x00812430 (:2513)
void havok_ragdoll_level_init_pre(); // 0x0081CC10 (:2522)
void havok_ragdoll_level_shutdown(); // 0x0081CC80 (:2543)
int havok_ragdoll_get_ragdoll_size(); // 0x00812490 (:2558)
void havok_ragdoll_add_ragdoll_memory(int num_ragdolls, void* memory); // 0x00825990 (:2563)
void havok_ragdoll_remove_ragdoll_memory(); // 0x0082B370 (:2582)
void havok_ragdoll_emergency_cleanup(); // 0x0082B3C0 (:2600)
int havok_ragdoll_bind_to_human(const human* hp, bool immediate, ragdoll_priority priority, int trigger, bool add_constraints); // 0x0082FD30 (:2636)
bool havok_ragdoll_unbind_from_human(int ragdoll_idx); // 0x00825A20 (:2808)
void havok_ragdoll_unsync(int ragdoll_idx, const vector& pos, const matrix& orient, const vector& vel, float force_multiplier); // 0x00825A80 (:2833)
void havok_ragdoll_add_constraints_to_world(int ragdoll_idx); // 0x0081CCF0 (:2842)
bool havok_ragdoll_is_synced(int ragdoll_idx); // 0x008124A0 (:2851)
bool havok_ragdoll_is_added_to_world(int ragdoll_idx); // 0x008124D0 (:2856)
void havok_ragdoll_set_gravity(int ragdoll_idx, bool gravity); // 0x00825AF0 (:2862)
bool havok_ragdoll_did_hit_wall(int ragdoll_idx); // 0x00812500 (:2886)
void havok_ragdoll_set_hit_wall(int ragdoll_idx); // 0x00812530 (:2892)
bool havok_ragdoll_should_destroy(int ragdoll_idx); // 0x00812560 (:2900)
bool havok_ragdoll_is_simulating(int ragdoll_idx); // 0x0081CD30 (:2912)
void havok_ragdoll_set_velocity(int ragdoll_idx, const vector& velocity); // 0x0081CDA0 (:2934)
void havok_ragdoll_set_avg_velocity(int ragdoll_idx, const vector& velocity); // 0x0081CE50 (:2972)
void havok_ragdoll_set_pos(int ragdoll_idx, const vector& pos); // 0x0081CFE0 (:3024)
void havok_ragdoll_apply_velocity(int ragdoll_idx, const vector& velocity, bool cap_velocity); // 0x0081D100 (:3061)
void havok_ragdoll_apply_point_velocity(int ragdoll_idx, const vector& velocity, const vector& pos); // 0x0081D2B0 (:3103)
void havok_ragdoll_cast_ray(int ragdoll_idx, const vector& start_pos, const vector& end_pos, float impulse); // 0x0083C4F0 (:3142)
void havok_ragdoll_cast_saved_ray(int ragdoll_idx); // 0x00825BE0 (:3230)
void havok_ragdoll_apply_point_impulse(int ragdoll_idx, const vector& impulse, const vector& pos); // 0x0081D480 (:3240)
void havok_ragdoll_apply_impulse_to_single_bone(int ragdoll_idx, const vector& impulse, const vector& pos); // 0x0081D910 (:3328)
void havok_ragdoll_apply_angular_velocity(int ragdoll_idx, const vector& angular_velocity, unsigned int body_part_filter); // 0x0081DAB0 (:3382)
void havok_ragdoll_jerk(int ragdoll_idx, float jerk_scale); // 0x0081DC70 (:3454)
void havok_sync_ragdoll_to_anim(int ragdoll_idx, const vector& char_pos, const matrix& char_orient, const vector& vel, float pct, float force_multiplier, bool preserve_anim_velocities_very_slow, bool use_ragdoll_pos, bool dont_bash_positions); // 0x0081DDB0 (:3489)
void havok_sync_anim_to_ragdoll(int idx, const vector& char_pos, const matrix& char_orient); // 0x0081E6C0 (:3702)
void havok_blend_anim_to_ragdoll(int idx, const vector& char_pos, const matrix& char_orient, float blend_pct); // 0x0081F240 (:3846)
bool havok_ragdoll_is_active(int ragdoll_idx); // 0x00820580 (:4126)
void havok_ragdoll_set_ignore_property(int ragdoll_idx, unsigned int handle, int time_ms); // 0x008205D0 (:4160)
void havok_ragdoll_remove_ignore_property(int ragdoll_idx); // 0x00820690 (:4194)
void havok_ragdoll_reset_active_timer(int ragdoll_idx); // 0x00812590 (:4217)
void havok_ragdoll_add_to_world(unsigned int ragdoll_idx); // 0x0083E440 (:4236)
void havok_ragdoll_process_pre(); // 0x0083E470 (:4247)
void havok_ragdoll_process_post(); // 0x00825C50 (:4353)
void havok_ragdoll_deactivate(int ragdoll_idx); // 0x00820700 (:4436)
void havok_ragdoll_activate(int ragdoll_idx); // 0x00820760 (:4453)
void havok_ragdoll_get_local_bbox(vector& bmin, vector& bmax, int ragdoll_idx, const vector& world_pos, const matrix& world_orient); // 0x008261C0 (:4475)
int havok_ragdoll_constrain_to_point(int ragdoll_idx, ragdoll_part bone, const vector& pos); // 0x0083E750 (:4529)
int havok_ragdoll_constrain_to_tag(int ragdoll_idx, ragdoll_part bone, unsigned int object_handle, unsigned int tag_idx); // 0x0083EC10 (:4650)
void havok_ragdoll_remove_constraint(int ragdoll_idx, int which_constraint); // 0x00826420 (:4776)
void havok_ragdoll_remove_constraints(int ragdoll_idx, hkpRigidBody* body); // 0x00826450 (:4788)
bool havok_ragdoll_increment_flinches(int ragdoll_idx); // 0x008264B0 (:4807)
int havok_ragdoll_get_time(int ragdoll_idx); // 0x008125C0 (:4845)
int havok_collect_ragdolls_fast(unsigned int* collected, const int max_collected, const vector& center, const float radius); // 0x008265A0 (:4855)
