// Original: volition/rfg/code/vehicle/walker.h
// Implementation: volition/rfg/code/vehicle/walker.cpp
//
// walker, a legged vehicle (the mech): animated legs with foot IK (wik_joint per leg), ramming and jump jets.
// Placement of the small types inferred.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class hkpRigidBody;
struct multi_walker_state;
class walker;
enum walker_attack_types
{
    WALKER_ATTACK_NONE = 0,
    WALKER_ATTACK_THROW = 1,
    WALKER_ATTACK_SWEEP = 2,
    WALKER_ATTACK_SMASH = 3,
    NUM_WALKER_ATTACK_TYPES = 4,
};

enum walker_ik_joints
{
    WALKER_IK_JOINT_LEFT_LEG = 0,
    WALKER_IK_JOINT_RIGHT_LEG = 1,
    NUM_WALKER_IK_JOINTS = 2,
};

enum walker_control_scheme
{
    WCS_FACING_CAMERA = 0,
    WCS_CHARACTER_STYLE = 1,
};

enum walker_model_type
{
    WMT_SMALL = 0,
    WMT_FLAME = 1,
    WMT_CIVILIAN = 2,
};

// size 0x4
struct walker_flags
{
    unsigned int reduce_impact_momentum : 1; // 0x00:0
    unsigned int in_ramming_mode : 1; // 0x00:1
    unsigned int rotating_left : 1; // 0x00:2
    unsigned int rotating_right : 1; // 0x00:3
    unsigned int teleport_components : 1; // 0x00:4
    unsigned int is_mounting : 1; // 0x00:5
    unsigned int jump_jet_on : 1; // 0x00:6
    unsigned int jump_take_off : 1; // 0x00:7
};

// size 0x5
struct walker_mp_data
{
    unsigned char attack_1_id; // 0x00
    unsigned char attack_2_id; // 0x01
    unsigned char attack_3_id; // 0x02
    unsigned char attack_4_id; // 0x03
    unsigned char charge_id; // 0x04
};

// size 0x34
struct mp_x_collision_prediction_info
{
    mp_x_collision_prediction_info(); // 0x00B6D920

    timestamp check_time; // 0x00
    vector last_start_pos; // 0x04
    vector last_end_pos; // 0x10
    vector last_pos; // 0x1C
    vector last_vel; // 0x28
};

// size 0x34
class wik_joint
{
public:
    bool process(walker* wp, vector& target_pos); // 0x00B99F40 (:3228)
    void do_ik(walker* wp, vector& target_pos, matrix43* tmp_matrices); // 0x00B6E540 (:2968)
private:
    bool get_ik_pos(walker* wp, vector& ik_pos); // 0x00B996F0 (:3109)
public:
    wik_joint(); // 0x00B6D910

    int anchor_tag; // 0x00
    int joint_tag; // 0x04
    int ik_tag; // 0x08
    float ik_strength; // 0x0C
    float ik_rate_of_change; // 0x10
    float correction_amount; // 0x14
    vector cached_hit_point; // 0x18
    bool hit_point_valid; // 0x24
    float cur_foot_p; // 0x28
    float cur_foot_b; // 0x2C
    walker_ik_joints location; // 0x30
};

// size 0x70A8
class walker : public vehicle
{
public:
    // size 0x9C
    struct component_info
    {
        component_info(); // 0x00B6D900

        vector desired_pos; // 0x00
        matrix desired_orient; // 0x0C
        vector anim_pos; // 0x30
        matrix anim_orient; // 0x3C
        float component_error; // 0x60
        int has_children; // 0x64
        vector debug_pos; // 0x68
        matrix debug_orient; // 0x74
        timestamp collide_ts; // 0x98
    };

    // size 0x8
    struct resource_dependent_data
    {
        int left_foot_component_idx; // 0x00
        int right_foot_component_idx; // 0x04
    };

    walker(); // 0x00BA36F0 (:491)
    static const object_type get_type_static(); // 0x006C4210 (.h:101)
    static const object_sub_type get_subtype_static(); // 0x006C4220 (.h:101)
    static const char* get_classname_static(); // 0x00B630C0 (.h:101)
    static int get_bpo_index_static(); // 0x00B630D0 (.h:101)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00BA3740 (.h:101)
    static unsigned int get_classname_hash(); // 0x00B6D8F0 (:490)
    static int pool_remaining(); // 0x00B85300 (:490)
    virtual void pool_free(); // vtbl+0x80 0x00B996E0 (:490)
    virtual hkpRigidBody* get_supporting_rigid_body() const; // vtbl+0x88 0x00B6D930 (:496)
    virtual float get_energy_scale(); // vtbl+0x64 0x00B6D960 (:508)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00B94300 (:553)
    virtual bool stream(bool load); // vtbl+0x1C 0x00BB04B0 (:639)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00B85310 (:786)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00B85610 (:867)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00B854F0 (:845)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00BB4C60 (:905)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00B6DA70 (:919)
    virtual void destroy_notify(); // vtbl+0x2C 0x00BB4CE0 (:932)
protected:
    virtual void free_this(); // vtbl+0x28 0x00BB4C40 (:768)
public:
    virtual ~walker(); // vtbl+0x00 0x00BA3750

    object_stream_resource<resource_dependent_data> w_rdd; // 0x40B8
    walker_flags w_flags; // 0x40C4
    vector chassis_pos; // 0x40C8
    matrix chassis_orient; // 0x40D4
    unsigned int collision_model; // 0x40F8
    float hull_radius; // 0x40FC
    component_info comp_info[74]; // 0x4100
    int comp_transform_order[74]; // 0x6E18
    vector steering_vector; // 0x6F40
    float move_speed; // 0x6F4C
    float cur_rotation_speed; // 0x6F50
    float desired_move_speed; // 0x6F54
    vector input_velocity; // 0x6F58
    vector chassis_pos_last_frame; // 0x6F64
    vector actual_velocity; // 0x6F70
    float actual_velocity_mag; // 0x6F7C
    float vertical_anim_speed; // 0x6F80
    float y_offset; // 0x6F84
    vector mount_vector; // 0x6F88
    unsigned int mount_vehicle; // 0x6F94
    unsigned int ground_obj_handle; // 0x6F98
    unsigned int last_ground_obj_handle; // 0x6F9C
    vector ground_normal; // 0x6FA0
    vector ground_world_pos; // 0x6FAC
    timestamp ground_test_ts; // 0x6FB8
    float ground_impact_momentum; // 0x6FBC
    float ram_momentum; // 0x6FC0
    float ram_max_energy_used; // 0x6FC4
    float jump_jet_energy; // 0x6FC8
    float jump_jet_speed; // 0x6FCC
    float jump_jet_engage_time; // 0x6FD0
    int jump_jet_tag_idx; // 0x6FD4
    unsigned int jump_jet_effect; // 0x6FD8
    int jump_jet_foley; // 0x6FDC
    timestamp jump_jet_foley_disable; // 0x6FE0
    timestamp jump_timeout; // 0x6FE4
    float engine_load; // 0x6FE8
    timestamp time_since_last_supported; // 0x6FEC
    walker_model_type model_type; // 0x6FF0
    walker_control_scheme control_scheme; // 0x6FF4
    int block_movement_anim; // 0x6FF8
    block_movement_types block_movement_type; // 0x6FFC
    wik_joint ik_joints[2]; // 0x7000
    multi_sim_state<multi_walker_state>* mp_sim_state; // 0x7068
    walker_mp_data mp_data; // 0x706C
    mp_x_collision_prediction_info mp_collision_prediction; // 0x7074
};

// size 0xC
class obj_walker_props : public obj_prop_block
{
public:
    obj_walker_props(int, const vector&, const matrix&, bool);

};

const char* classname_walker(); // 0x00B6D8E0 (:490)
object* alloc_walker(); // 0x00B942F0 (:490)
int bpo_index_walker(); // 0x00B6D8D0 (:490)
void types_walker(signed char& type, signed char& subtype); // 0x00B6D8C0 (:490)
void walker_system_init(); // 0x00B6DA90 (:964)
void walker_get_velocity(const walker* wp, vector& vel); // 0x00B6DAD0 (:984)
bool walker_build(walker* wp); // 0x00BAD320 (:991)
void walker_steer(walker* wp, float stick_v, float stick_h); // 0x00B85850 (:1164)
void walker_process_movement_presim(walker* wp); // 0x00BB83A0 (:1424)
void walker_do_frame_presim(walker* wp); // 0x00BBA2B0 (:1946)
void walker_do_frame_postsim(walker* wp); // 0x00BA3760 (:2163)
void walker_blow_up(walker* wp); // 0x00B86530 (:2486)
bool walker_play_attack(walker* wp); // 0x00B6DBD0 (:2523)
bool walker_play_attack2(walker* wp); // 0x00B6DD20 (:2567)
bool walker_play_attack3(walker* wp); // 0x00B6DE70 (:2612)
bool walker_play_attack4(walker* wp); // 0x00B6DF90 (:2649)
void walker_play_VI_anim(walker* wp, int state, int exit_type); // 0x00B6E0B0 (:2687)
bool walker_charge(walker* wp); // 0x00B6E2D0 (:2832)
void walker_mount_vehicle(walker* wp, vehicle* vp); // 0x00B86650 (:2884)
walker_attack_types walker_get_attack_type(walker* wp); // 0x00B6E480 (:2921)
bool walker_should_ignore_collision_effect(const walker* const wp); // 0x00B6E500 (:2954)
bool walker_idx_is_foot(walker* wp, int idx); // 0x00B6EFB0 (:3352)

// 0x006C4210 (.h:101)
inline const object_type walker::get_type_static()
{
    return OT_VEHICLE;
}

// 0x006C4220 (.h:101)
inline const object_sub_type walker::get_subtype_static()
{
    return OT_SUB_VEHICLE_WALKER;
}
