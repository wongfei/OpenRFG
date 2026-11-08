// Original: volition/anim_lib/code/anim_lib_spinebend.h
// Implementation: volition/anim_lib/code/anim_lib_spinebend.cpp
//
// Spine bending: distributes an aim heading and pitch over the spine, neck and head bones within per-bone limits
// (human_spinebend_data, objects/humans/human_spinebend.h), ramping towards the target.
#pragma once


struct human_spinebend_data;
class matrix43;

// size 0x18
struct spinebend_bone_info
{
    float max_pitch; // 0x00
    float min_pitch; // 0x04
    float max_heading; // 0x08
    float min_heading; // 0x0C
    float blend_pct; // 0x10
    unsigned short bone_heading_filter; // 0x14
    unsigned short bone_pitch_filter; // 0x16
};

// The spine bend chain (Bone_parent is declared in anim_lib.h). Placement inferred.
extern int Bone_children[9][3]; // 0x016F08B8
extern spinebend_bone_info Bone_info[9]; // 0x016F0928

float fl_ramp_to_value(float cur_value, float dest_value, float acceleration, float& cur_velocity, bool in_combat); // 0x00C8EF40 (:214)
void interp_bones(human_spinebend_data* sbd, int bone_idx, float& applied_heading, float& applied_pitch); // 0x00C8F410 (:288)
void bend_bones(human_spinebend_data* sbd, int bone_idx, float pitch_axis_heading); // 0x00C9A580 (:473)
void calculate_bone_limits(human_spinebend_data* sbd, matrix43* tmp_matrices); // 0x00C8FDE0 (:521)
bool process_bone_limits_sub(float& max_heading, float& min_heading, float& max_pitch, float& min_pitch, float& total_blend, human_spinebend_data* sbd, int bone_idx); // 0x00C901A0 (:622)
void process_bone_limits(human_spinebend_data* sbd); // 0x00C90590 (:714)
float calculate_angles(float desired_heading, float desired_pitch, human_spinebend_data* sbd, int bone_idx, unsigned int filter); // 0x00C905C0 (:720)
void process_spinebending(human_spinebend_data* sbd, float pitch, float heading, matrix43* tmp_matrices, float eye_pitch, float eye_heading, float implied_anim_heading); // 0x00C9A860 (:806)
void animlib_spinebend_process(human_spinebend_data* sbd, matrix43* tmp_matrices); // 0x00C9AB60 (:881)
