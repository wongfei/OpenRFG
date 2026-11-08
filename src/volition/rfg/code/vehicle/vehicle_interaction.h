// Original: volition/rfg/code/vehicle/vehicle_interaction.h
// Implementation: volition/rfg/code/vehicle/vehicle_interaction.cpp
//
// Vehicle interaction: a vi_instance pairs a human (vi_human_manager: the interaction, its logic and animation
// states, entry/exit data) with a vehicle (vi_vehicle_manager: doors and the seat waypoint), and is advanced by
// vehicle_interaction_process every frame. Each vehicle embeds one (vehicle::m_vi).
#pragma once

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_enums.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class human;
class matrix;
class object;
class vehicle;

// size 0x1
struct waypoint_usage
{
    int get_human_index() const; // 0x00B62F80 (.h:154)
    void set_human_index(const int human_index); // 0x00B62F90 (.h:155)
    bool is_path_reserved() const; // 0x00B62FA0 (.h:156)
    void set_free(); // 0x00B62FB0 (.h:157)

private:
    unsigned char human_ref : 4; // 0x00:0
public:
    unsigned char reg_on_release : 1; // 0x00:4
    unsigned char block_type : 3; // 0x00:5
};

// size 0x10
struct vi_door
{
    void initialize(); // 0x00B62F60 (.h:123)
    vi_door(); // 0x00B6F850

    timestamp open_close_ts; // 0x00
    int component_id; // 0x04
    vi_door_state state; // 0x08
    int wpi_check; // 0x0C
};

// size 0x10
class vi_enter_data
{
public:
    vi_enter_data(); // 0x005FB690 (.h:66)
    void initialize(); // 0x005DCCF0 (.h:72)

    vehicle_seat_index specified_seat; // 0x00
    vi_seat_selection_method seat_selection_method; // 0x04
    vehicle_seat_index destination_seat; // 0x08
    struct
    {
        unsigned char teleport : 1; // 0x00:0
        unsigned char walk : 1; // 0x00:1
        unsigned char minimal_processing : 1; // 0x00:2
        unsigned char reserve_seat_only : 1; // 0x00:3
        unsigned char use_pf_path : 1; // 0x00:4
        unsigned char teleport_to_door : 1; // 0x00:5
    } flags; // 0x0C
};

// size 0x1
class vi_exit_data
{
public:
    vi_exit_data(); // 0x005FB6B0 (.h:94)
    void initialize(); // 0x005DCD00 (.h:100)

    struct
    {
        unsigned char teleport : 1; // 0x00:0
        unsigned char maintain_position : 1; // 0x00:1
        unsigned char reexit : 1; // 0x00:2
        unsigned char last_exit_failed : 1; // 0x00:3
        unsigned char force_dive_exit : 1; // 0x00:4
    } flags; // 0x00
};

// size 0x68
class vi_human_manager
{
public:
    vi_human_manager(); // 0x00B884D0 (.h:221)
    void initialize(); // 0x00B80FF0 (:340)
    int effective_path_idx() const; // 0x00B62FC0 (.h:225)
    int effective_curr_wpi() const; // 0x00B71340 (.h:226)
    int effective_anim_wpi() const; // 0x00B62FD0 (.h:227)
    int path_dest_wpi() const; // 0x00B71360 (.h:228)
    ~vi_human_manager(); // 0x00B86CD0

    unsigned int handle; // 0x00
    vi_interaction interaction; // 0x04
    vi_correction_type pos_correction; // 0x08
    vector human_motion; // 0x0C
    vi_state_of_logic state; // 0x18
    farray<signed char,6> path_nodes; // 0x1C
    signed char transition_idx; // 0x34
    signed char cur_path_idx; // 0x35
    signed char last_path_idx; // 0x36
    timestamp wait_ts; // 0x38
    signed char door_comp_idx; // 0x3C
    vi_state_of_anim state_of_anim; // 0x40
    signed char animate_from_wpi; // 0x44
    signed char animate_dest_wpi; // 0x45
    int ride_state_handle; // 0x48
    vehicle_seat_index seat; // 0x4C
    timestamp manual_close_door_ts; // 0x50
    vi_enter_data enter_data; // 0x54
    vi_exit_data exit_data; // 0x64
    signed char door_idx : 4; // 0x65:0
    signed char reserve_next_node_only : 1; // 0x65:4
    signed char freeze_hands_next_frame : 1; // 0x65:5
    signed char being_hijacked : 1; // 0x65:6
    signed char use_vehicle_collision : 1; // 0x65:7
    signed char auto_connect_to_entry : 1; // 0x66:0
};

// size 0xB0
struct vi_vehicle_manager
{
    vi_vehicle_manager(); // 0x00B8F490
    ~vi_vehicle_manager(); // 0x00B888B0

    vehicle* vp; // 0x00
    farray<vi_door,6> doors; // 0x04
    farray<waypoint_usage,44> used_waypoints; // 0x74
};

// size 0x428
class vi_instance
{
public:
    vi_instance(); // 0x00B97EC0 (:372)
    int add_human(human* hp); // 0x00B8F4E0 (:377)
    void remove_human(int human_index); // 0x00BB19C0 (:438)
    void remove_human(human*);
    int find_human(const human* hp) const; // 0x00B81060 (:415)
    void initialize(vehicle* vp); // 0x00B81090 (:528)
    void passengers_destroy(); // 0x00BB1BD0 (:551)
    ~vi_instance(); // 0x00B95B60

    vi_vehicle_manager vvm; // 0x00
    signed char num_humans; // 0xB0
    farray<vi_human_manager,8> humans; // 0xB4
    timestamp refresh_raycasts_timestamp; // 0x404
    unsigned char reregister_paths : 1; // 0x408:0
    unsigned char process_state_again : 1; // 0x408:1
    unsigned char updated_exits : 1; // 0x408:2
    unsigned char post_stream_init : 1; // 0x408:3
    unsigned char exit_mode : 4; // 0x408:4
    farray<unsigned char,11> exit_costs; // 0x40C
};

// size 0x10
struct waypoint_node
{
    waypoint_node(); // 0x00B68580 (:148)

    int wpi; // 0x00
    int dest_ni; // 0x04
    float weight; // 0x08
    float ext_weight; // 0x0C
};

bool vec_equal_fast(vector v0, vector v1, float delta); // 0x00B684B0 (:90)
bool vi_collection_callback(object* hit_objp, unsigned int shape_key, const void* user_data); // 0x00B685E0 (:1431)
void path_to_door_stop(human* hp, vi_human_manager* hm); // 0x00B68650 (:2971)
void path_to_door_complete(human* hp, vi_human_manager* hm); // 0x00B81730 (:2980)
bool vehicle_interaction_request_enter_internal(human* hp, vehicle* vp, vi_enter_data* parameters); // 0x00BB7C60 (:6796)
bool vehicle_interaction_passengers_stuck(const vehicle* vp); // 0x00B81990 (:7074)
bool vehicle_interaction_request_enter(human* hp, vehicle* vp, vi_enter_data* parameters); // 0x00BB81F0 (:7100)
void vehicle_interaction_request_exit(human* hp, vi_exit_data* parameters); // 0x00BB6060 (:7307)
bool vehicle_interaction_play_custom_anim(human* hp, animation_action action, bool override_anim, bool break_ik); // 0x00B909D0 (:7359)
void vehicle_interaction_cancel_reservation(human* hp, vehicle* vp); // 0x00BB4990 (:7442)
bool vehicle_interaction_ragdoll(human* hp, unsigned int perp); // 0x00BB2670 (:7475)
bool vehicle_interaction_cancel(human* hp); // 0x00BB4A30 (:7515)
void vehicle_interaction_bail_all(vehicle* vp); // 0x00BB4B00 (:7587)
void vehicle_interaction_remove(human* hp); // 0x00BB2720 (:7624)
void vehicle_interaction_process(); // 0x00BBF660 (:7650)
bool vehicle_interaction_process_human_pre(human* hp); // 0x00B81A10 (:7797)
bool vehicle_interaction_teleport_human_positions(vehicle* vp); // 0x00BB34D0 (:8192)
bool vehicle_interaction_update_human_positions(vehicle* vp); // 0x00BB3560 (:8221)
bool human_in_protected_vehicle_pos(const human* hp); // 0x00B81DB0 (:8354)
bool human_can_quick_exit_vehicle(const human* hp); // 0x00B81E20 (:8375)
bool vehicle_interaction_get_seat_pos_orient(const vehicle* vp, vehicle_seat_index seat_idx, vector& pos, matrix* orient); // 0x00B90BC0 (:8425)
bool vehicle_interaction_ragdoll_fall_in_seat(human* hp, vehicle* vp); // 0x00BB8200 (:8449)
void vehicle_interaction_highlight_entry_points(vehicle* vp, float radius); // 0x00B68830 (:8518)

// 0x00B62F80 (.h:154)
inline int waypoint_usage::get_human_index() const
{
    return human_ref - 1;
}

// 0x00B62F90 (.h:155)
inline void waypoint_usage::set_human_index(const int human_index)
{
    human_ref = (unsigned char)(human_index + 1);
}

// 0x00B62FA0 (.h:156)
inline bool waypoint_usage::is_path_reserved() const
{
    return human_ref != 0;
}

// 0x00B62FB0 (.h:157)
inline void waypoint_usage::set_free()
{
    human_ref = 0;
}

// 0x005DCCF0 (.h:72)
inline void vi_enter_data::initialize()
{
    specified_seat = VSI_INVALID;
    seat_selection_method = VI_SEAT_SELECTION_METHOD_NONE;
    destination_seat = VSI_INVALID;
    *(unsigned char*)&flags = 0;
}

// 0x005DCD00 (.h:100)
inline void vi_exit_data::initialize()
{
    *(unsigned char*)&flags = 0;
}

// 0x00B62FC0 (.h:225)
inline int vi_human_manager::effective_path_idx() const
{
    return reserve_next_node_only != 0 ? cur_path_idx + 1 : cur_path_idx;
}

// 0x00B71340 (.h:226)
inline int vi_human_manager::effective_curr_wpi() const
{
    return cur_path_idx < 0 ? -1 : path_nodes[effective_path_idx()];
}

// 0x00B62FD0 (.h:227)
inline int vi_human_manager::effective_anim_wpi() const
{
    return reserve_next_node_only != 0 ? animate_dest_wpi : animate_from_wpi;
}

// 0x00B71360 (.h:228)
inline int vi_human_manager::path_dest_wpi() const
{
    return last_path_idx < 0 ? -1 : path_nodes[last_path_idx];
}

// 0x005FB690 (.h:66)
inline vi_enter_data::vi_enter_data()
{
    specified_seat = VSI_INVALID;
    seat_selection_method = VI_SEAT_SELECTION_METHOD_NONE;
    destination_seat = VSI_INVALID;
    *(unsigned char*)&flags = 0;
}

// 0x005FB6B0 (.h:94)
inline vi_exit_data::vi_exit_data()
{
    *(unsigned char*)&flags = 0;
}
