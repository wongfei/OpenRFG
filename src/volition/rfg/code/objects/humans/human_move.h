// Original: volition/rfg/code/objects/humans/human_move.h
// Implementation: volition/rfg/code/objects/humans/human_move.cpp
//
// Path following of humans (human::pf): the current path and action, stuck detection, detours, repulsors and path
// smoothing. Placement of human_path_data inferred.
#pragma once

#include <stdint.h>
#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class human;
struct nav_cell;
class object;
struct path_node;
class player;

// size 0x120
struct human_path_data
{
    human_path_data(); // 0x00A0F1E0

    human_path_actions action; // 0x00
    human_movement_states prev_state; // 0x04
    path_node* path; // 0x08
    timestamp shortcut_time; // 0x0C
    timestamp dest_in_hull_time; // 0x10
    vector start_pos; // 0x14
    uint64_t last_visited_nav_cell_handle; // 0x20
    timestamp last_visited_nav_cell_update_timer; // 0x28
    timestamp dest_check_timer; // 0x2C
    vector last_dest_check_pos; // 0x30
    int extended_stuck_cumulative; // 0x3C
    timestamp extended_stuck_timer; // 0x40
    vector extended_stuck_pos; // 0x44
    timestamp detour_hull_check_timer; // 0x50
    unsigned int problem_detour_handles[4]; // 0x54
    int num_problem_detours; // 0x64
    vector problem_detours_dest_pos; // 0x68
    unsigned char affected_by_repulsors; // 0x74
    unsigned char num_path_repulsors; // 0x75
    unsigned int path_repulsor_handles[8]; // 0x78
    timestamp path_repulsors_collection_time; // 0x98
    timestamp repulsion_field_timer; // 0x9C
    timestamp hole_repulsion_field_timer; // 0xA0
    vector eventual_goal_pos; // 0xA4
    float dist_until_recheck; // 0xB0
    unsigned char num_hole_repulsors; // 0xB4
    unsigned int hole_repulsors[8]; // 0xB8
    unsigned int last_cmp_hull; // 0xD8
    unsigned int transition_node_to_avoid; // 0xDC
    float movement_radius; // 0xE0
    float curve_distance; // 0xE4
    vector smoothPoint; // 0xE8
    vector center_of_circle; // 0xF4
    vector smooth_start_pos; // 0x100
    vector smooth_start_fvec; // 0x10C
    struct
    {
        unsigned char turn_right : 1; // 0x00:0
        unsigned char smoothing : 1; // 0x00:1
        unsigned char climbing_up : 1; // 0x00:2
    } flags; // 0x118
};

void player_move_set_anim_direct(player* pp, float jh, float jv, bool do_sprint, bool do_stop); // 0x00A9D380 (:207)
void human_move_play_land(human* hp); // 0x00ADC910 (:358)
void human_move_path_set_direct(human* hp, path_node* path); // 0x00A0AFA0 (:736)
int human_move_calculate_path(human* hp, const vector& dest, uint64_t end_nav_cell_hint, bool out_of_chunk); // 0x00A30C60 (:770)
int human_move_recalculate_path(human* hp); // 0x00A30CC0 (:787)
bool human_move_get_path_destination(human* hp, vector& v); // 0x00A0B000 (:807)
bool human_move_turn_to_target(human* hp, const vector& pnt); // 0x00A0B030 (:830)
void human_move_direct_travel_to(human* hp, vector& dest); // 0x00A30D30 (:863)
void human_move_force_direct_walk(human* hp, vector& dest, human_movement_states mstate, nav_cell* dest_nc); // 0x00A0B220 (:880)
bool human_move_should_ignore_object_direct(object* objp); // 0x00A0B380 (:899)
bool human_move_is_mode(human* hp, human_move_modes mode, human_move_submodes submode); // 0x00A0B3A0 (:930)
void human_move_system_init(); // 0x00A30DF0 (:941)
void human_move_level_init_post(); // 0x00A30E00 (:950)
void human_move_process(human* hp); // 0x00AE4C60 (:969)
bool human_move_is_crouching(const human* hp); // 0x00A0B3D0 (:1114)
void human_move_start_crouching(human* hp); // 0x00A30E10 (:1121)
bool crouch_to_stand_callback(object* hit_objp, void* vp); // 0x00A809B0 (:1151)
void human_move_stop_crouching(human* hp); // 0x00A809E0 (:1173)

// ---- human_move_fall.cpp, human_move_jump.cpp, human_move_knockdown.cpp (the PDB has no header of their own; the
// movement modes are called from human_move.cpp, so they are declared here, placement inferred)
void human_move_fall_system_init(); // 0x00A0B400 (human_move_fall.cpp:97)
void human_move_fall_level_init_post(); // 0x00A0B410 (human_move_fall.cpp:101)
void human_move_fall_update_speed(human* hp); // 0x00A80BB0 (human_move_fall.cpp:104)
void human_move_fall(human* hp); // 0x00AC0F50 (human_move_fall.cpp:137)
void human_move_fall_do_landing_damage(human* hp); // 0x00AD4D90 (human_move_fall.cpp:268)
float human_move_fall_get_safe_speed(const human* hp, bool allow_jump); // 0x00A30ED0 (human_move_fall.cpp:301)
float human_move_fall_get_fatal_speed(const human* hp, bool allow_jump); // 0x00A30F70 (human_move_fall.cpp:315)
void human_move_jump_system_init(); // 0x00A0B420 (human_move_jump.cpp:88)
void human_move_jump(human* hp); // 0x00A31270 (human_move_jump.cpp:92)
void human_move_knockdown(human* hp); // 0x00ADD4D0 (human_move_knockdown.cpp:91)
// human_move_pathfind.cpp (no header in the PDB either; placement inferred)
bool human_move_pathfind(human* hp); // 0x00AC9960 (human_move_pathfind.cpp:851)

// Fall speeds (m/s, tweak table items of human_move_fall.cpp)
extern float Player_safe_fall_speed; // 0x030269C4
extern float Player_fatal_fall_speed; // 0x030266B8
extern float Npc_safe_jumping_fall_speed; // 0x030266AC
extern float Npc_fatal_jumping_fall_speed; // 0x0301A224
extern float Npc_safe_fall_speed; // 0x03023688
extern float Npc_fatal_fall_speed; // 0x0301A23C

// 0x00A0F1E0: the timestamps construct themselves.
inline human_path_data::human_path_data()
{
}
