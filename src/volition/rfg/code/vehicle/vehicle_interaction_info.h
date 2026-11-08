// Original: volition/rfg/code/vehicle/vehicle_interaction_info.h
// Implementation: volition/rfg/code/vehicle/vehicle_interaction_info.cpp
//
// Vehicle interaction tables: per vehicle the seats, the waypoints a human moves through to enter and exit them and
// the animated transitions between waypoints.
#pragma once

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/vlib/code/math/vector3.h"

class anim;
class anim_set;
class matrix;
struct vanim_play_parameters;
class vehicle;
class vehicle_interaction_waypoint_transition;
class vi_human_manager;

enum vehicle_interaction_actions
{
    VIA_TRANSITION = 0,
    VIA_VEHICLE_MOVEMENT = 1,
    VIA_OPEN_DOOR = 2,
    VIA_OPEN_DOOR_AND_EXTRACT = 3,
    VIA_SKIP_TO_NEXT_NODE = 4,
    VIA_TRANSITION_TYPE2 = 5,
    VIA_CLOSE_DOOR = 6,
    VIA_TRANSITION_CLOSED_DOOR = 7,
    NUM_VEHICLE_INTERACTION_ACTIONS = 8,
};

// size 0x24
struct vehicle_interaction_waypoint
{
    char* name; // 0x00
    vector local_pos; // 0x04
    float local_heading; // 0x10
    vehicle_seat_index seat_index; // 0x14
    char* parent_component_name; // 0x18
    int parent_component_index; // 0x1C
    unsigned char node_type : 4; // 0x20:0
    unsigned char short_range : 1; // 0x20:4
    unsigned char restrict_vertical_access : 1; // 0x20:5
    unsigned char last_choice : 1; // 0x20:6
    unsigned char eject_on_explosion : 1; // 0x20:7
};

// size 0xC
struct vehicle_transition_action
{
    vehicle_interaction_actions action; // 0x00
    animation_action anim_action; // 0x04
    animation_state anim_state; // 0x08
};

// size 0x20
class vehicle_interaction_info
{
public:
    const vehicle_interaction_waypoint_transition* const get_transition(signed char& idx, int start_waypoint_index, int destination_waypoint_index) const; // 0x00B68920 (:587)
    void get_waypoint_world_pos_orient(const vehicle* vp, int waypoint_id, vector& pos, matrix* orient, vector sub_uvec) const; // 0x00B82860 (:499)
    bool find_waypoint_path(int waypoint_id, int seat_id, int* path) const; // 0x00B688D0 (:405)
    bool is_auto_connected_transition(int start_waypoint_index, int destination_waypoint_index) const; // 0x00B688E0 (:476)
    bool is_auto_connected_transition_active(const vehicle* vp, int start_waypoint_index, int destination_waypoint_index, vector* precalc_dest_pos) const; // 0x00B91410 (:483)
    void get_closest_seat_for_ragdoll(vehicle_seat_index& seat_idx, vehicle* vp, vector& pos, float range_squared) const; // 0x00B83770 (:808)

    char* name; // 0x00
    bool is_initialized : 1; // 0x04:0
    bool player_prompt_only_at_entry : 1; // 0x04:1
    anim_set* animation_set; // 0x08
    int num_waypoints; // 0x0C
    vehicle_interaction_waypoint* waypoints; // 0x10
    int num_waypoint_transitions; // 0x14
    vehicle_interaction_waypoint_transition* transitions; // 0x18
    int num_seats; // 0x1C
};

// size 0x1C
class vehicle_interaction_waypoint_transition
{
public:
    bool play_animation(vehicle* vp, vi_human_manager* hm, vehicle_interaction_actions action) const; // 0x00B83220 (:642)

    signed char index; // 0x00
    signed char destination_index; // 0x01
    char* component_obstacle_name; // 0x04
    signed char component_obstacle_index; // 0x08
    int component_open_foley; // 0x0C
    int component_close_foley; // 0x10
    signed char num_interactions; // 0x14
    vehicle_transition_action* actions; // 0x18
};

extern int Num_vehicle_interaction_infos; // 0x0377004C
extern vehicle_interaction_info* Vehicle_interaction_infos; // 0x03770050

void vehicle_interaction_dlc_init(int dlc_id); // 0x00B827D0 (:367)
void vehicle_interaction_dlc_shutdown(); // 0x00B688A0 (:380)
void vehicle_interaction_system_init(); // 0x00B82800 (:388)
void vi_info_get_animation_space(vanim_play_parameters& vpp, vehicle* vp, vi_human_manager* hm, anim* animp); // 0x00B82FE0 (:607)
const char* vi_trace_anim(int anim_handle); // the game trace (game_trace.h), not in rfg.exe
