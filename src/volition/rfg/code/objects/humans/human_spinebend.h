// Original: volition/rfg/code/objects/humans/human_spinebend.h
// Implementation: volition/rfg/code/objects/humans/human_spinebend.cpp
//
// Spine bending: turns the spine, head and eye bones of a human toward points of interest (spinebend_interest) on top
// of the animation, per bone with heading/pitch limits and velocities (spinebend_bone_state, up to 9 bones). Placement
// of the types inferred.
// human_spinebend_data is over-aligned: human places it at 0x480 after ik_joints ends at 0x478, so its alignment is 8 or
// 16 (the PDB does not record alignment). alignas(16) is inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class human;

enum spinebend_interest
{
    SI_CHOOSE_NEW_MAIN_FOCUS_PRIMARY = 0,
    SI_CHOOSE_NEW_MAIN_FOCUS = 1,
    SI_MAIN_FOCUS_PRIMARY = 2,
    SI_MAIN_FOCUS_RANDOM = 3,
    SI_HOLDING_FOCUS = 4,
    SI_LOCAL_SCANNING = 5,
    SI_PANNING_START = 6,
    SI_PANNING = 7,
    SI_PANNING_GLANCE = 8,
    SI_PANNING_FOLLOW = 9,
    SI_QUICK_GLANCE = 10,
    SI_FOLLOW_EYES = 11,
};

// size 0x4
struct spinebend_flags
{
    unsigned int disabled : 1; // 0x00:0
    unsigned int track_object : 1; // 0x00:1
    unsigned int sleep_mode : 1; // 0x00:2
    unsigned int sleep_mode_ready : 1; // 0x00:3
    unsigned int no_spinebending_last_frame : 1; // 0x00:4
    unsigned int do_spinebend_process : 1; // 0x00:5
    unsigned int zero_pitch : 1; // 0x00:6
    unsigned int zero_heading : 1; // 0x00:7
    unsigned int zero_eye_pitch : 1; // 0x00:8
    unsigned int zero_eye_heading : 1; // 0x00:9
    unsigned int force_update : 1; // 0x00:10
    unsigned int is_player : 1; // 0x00:11
    unsigned int delay_processing : 1; // 0x00:12
    unsigned int blending_anims : 1; // 0x00:13
    unsigned int spinebend_teleport : 1; // 0x00:14
    unsigned int linear_interpolation : 1; // 0x00:15
    unsigned int aim_at_pos : 1; // 0x00:16
    unsigned int pan_dir : 1; // 0x00:17
};

// size 0x60
struct spinebend_bone_state
{
    float heading; // 0x00
    float pitch; // 0x04
    float desired_heading; // 0x08
    float desired_pitch; // 0x0C
    float last_heading; // 0x10
    float last_pitch; // 0x14
    float anim_last_bone_heading; // 0x18
    float anim_last_bone_pitch; // 0x1C
    float anim_heading_change; // 0x20
    float anim_pitch_change; // 0x24
    float max_children_heading; // 0x28
    float min_children_heading; // 0x2C
    float max_children_pitch; // 0x30
    float min_children_pitch; // 0x34
    float children_total_blend; // 0x38
    float max_pitch; // 0x3C
    float min_pitch; // 0x40
    float max_heading; // 0x44
    float min_heading; // 0x48
    float heading_vel; // 0x4C
    float pitch_vel; // 0x50
    int bone_index; // 0x54
    int rig_bone_parent; // 0x58
    struct
    {
        unsigned int fast_mode : 1; // 0x00:0
    } flags; // 0x5C
};

// size 0x410, alignment inferred
struct alignas(16) human_spinebend_data
{
    human_spinebend_data(); // 0x00A0F230

    spinebend_flags flags; // 0x00
    unsigned int target_object_handle; // 0x04
    vector target_point; // 0x08
    vector last_lookat_vec; // 0x14
    spinebend_interest main_interest; // 0x20
    spinebend_interest head_interest; // 0x24
    spinebend_interest eyes_interest; // 0x28
    spinebend_interest last_eyes_interest; // 0x2C
    vector head_dest_vec; // 0x30
    vector eyes_dest_vec; // 0x3C
    float lookat_priority; // 0x48
    float eye_lookat_priority; // 0x4C
    timestamp head_orient_ts; // 0x50
    timestamp main_interest_ts; // 0x54
    timestamp eyes_orient_ts; // 0x58
    timestamp focus_target_ts; // 0x5C
    float pan_heading; // 0x60
    timestamp pan_ts; // 0x64
    timestamp update_timestamp; // 0x68
    float remaining_heading; // 0x6C
    float lower_body_rotation; // 0x70
    float lower_body_heading_speed; // 0x74
    float spinebend_bone_rotation; // 0x78
    float spinebend_bone_last_heading; // 0x7C
    float root_heading_change; // 0x80
    float root_heading_last_frame; // 0x84
    int spinebend_bone_index; // 0x88
    float turn_speed; // 0x8C
    float heading_drift; // 0x90
    float pitch_drift; // 0x94
    unsigned int bone_filter; // 0x98
    float bone_max_angle_multiplier; // 0x9C
    int aim_handle; // 0xA0
    vector state_controller_movement; // 0xA4
    spinebend_bone_state bone_state[9]; // 0xB0
};

unsigned int determine_spinebend_filter(human* hp); // 0x00A82D70 (:445)
void human_spinebend_reset(human* hp); // 0x00A315B0 (:539)
void human_spinebend_reset_target(human* hp); // 0x00A0CE00 (:590)
void human_spinebend_disable(human* hp); // 0x00A0CE40 (:602)
void human_spinebend_enable(human* hp); // 0x00A0CE50 (:615)
bool human_spinebend_is_enabled(human* hp); // 0x00A0CE60 (:628)
void human_spinebend_rotate_orient(human* hp, float heading_change); // 0x00A0CE70 (:636)
void human_spinebend_pre_process(human* hp); // 0x00AB7480 (:1059)
void human_spinebend_post_transform(human* hp); // 0x00AB78F0 (:1210)

// 0x00A0F230: the timestamps construct themselves.
inline human_spinebend_data::human_spinebend_data()
{
}
