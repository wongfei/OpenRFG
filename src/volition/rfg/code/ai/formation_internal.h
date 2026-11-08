// Original: volition/rfg/code/ai/formation_internal.h
// Implementation: volition/rfg/code/ai/formation_internal.cpp
//
// The formation objects: members with their slot and follow state, the formation itself, and the leader's ghost
// history points along which followers keep their spacing.
#pragma once

#include "volition/rfg/code/ai/formation.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class human;
class object;

// size 0x10
struct formation_position_information
{
    formation_position_information(vector offset, int index); // 0x005E9AC0 (:26)

    vector offset_from_leader; // 0x00
    int leader_index; // 0x0C
};

// size 0x24
struct member_follow_info
{
    member_follow_info(); // 0x005E9AE0 (:46)

    object* obj; // 0x00
    vector current_dir; // 0x04
    float current_speed; // 0x10
    vector move_dir; // 0x14
    bool current_is_valid; // 0x20
    bool move_dir_is_valid; // 0x21
};

// size 0x1C
struct ghost_history_point
{
    ghost_history_point(const vector& p, float d, const vector& v); // 0x005E99E0 (.h:38)
    ghost_history_point(); // 0x005E99D0 (.h:37)

    vector pos; // 0x00
    float dist; // 0x0C
    vector dir; // 0x10
};

// size 0x16C
struct ai_formation_member
{
    typedef circ_array<ghost_history_point,10> ghost_history_array;

    // size 0x1
    struct follower_flags
    {
        bool blocked : 1; // 0x00:0
        bool wants_wait : 1; // 0x00:1
        bool is_leader : 1; // 0x00:2
        bool obstruction_check : 1; // 0x00:3
    };

    void alloc(); // 0x006327A0 (:825)
    void add_ghost_point(const vector& p, float d, const vector& v); // 0x00632860 (:849)
    vector get_dir(const vector& current_pos, const vector& current_dir) const; // 0x006328C0 (:882)
    bool is_stuck() const; // 0x005E9A20 (.h:99)
    ai_formation_member(); // 0x0064B040

    unsigned int member_handle; // 0x00
    circ_array<ghost_history_point,10> ghost_history; // 0x04
    follow_information follow_info; // 0x124
    float max_dist_from_ghost_point; // 0x158
    timestamp obstruction_period; // 0x15C
    timestamp stuck_time; // 0x160
    follower_flags follow_flags; // 0x164
    float min_spacing; // 0x168
};

// size 0x164
class ai_formation
{
public:
    typedef farray<ai_formation_member*,15> member_list_array;
    typedef circ_array<vector,20> formation_history_array;

    void alloc(); // 0x00632AB0 (:919)
    void release(); // 0x0067B830 (:937)
    bool init(unsigned int new_handle, ai_formation_types new_formation_type, const object* leader, const object* follower); // 0x0067B930 (:965)
    bool add_follower(const object* follower); // 0x0067BA00 (:1005)
    bool remove_follower(unsigned int follower_handle); // 0x006A0D80 (:1036)
    unsigned int get_leader_handle() const; // 0x005E9A40 (.h:135)
    bool has_follower(unsigned int) const;
    int get_num_followers() const; // 0x00631E80 (.h:137)
    bool is_following(unsigned int follower_handle) const; // 0x0065E500 (.h:138)
    bool is_rigid_formation() const; // 0x005E9A50 (.h:139)
    bool is_flock_formation() const; // 0x005E9A70 (.h:140)
    bool is_follow_formation() const; // 0x005E9A80 (.h:141)
    bool do_frame(); // 0x006A0DD0 (:1060)
    const follow_information* get_follow_position(const object* follower) const; // 0x0065E970 (:1121)
    void request_wait(const object* follower, bool wait); // 0x0065E9C0 (:1142)
    bool wait_requested() const; // 0x00632AF0 (:1151)
    const object* get_member_requesting_wait() const; // 0x00632B30 (:1169)
    ai_formation_member* get_obstruction_check_member(int& max_time_since) const; // 0x00632B70 (:1197)
    void check_obstructions(ai_formation_member* member_info); // 0x0067BAA0 (:1222)
    int collect_followers(unsigned int* followers, int max_collect, bool (*callback)(unsigned int, void*), void* callback_data); // 0x00632BD0 (:1255)
    bool is_formation_member(unsigned int member_handle) const; // 0x0065E530 (.h:163)
private:
    int compact_member_list(); // 0x0065EBB0 (:1541)
    bool remove_derelict_members(); // 0x0067BBC0 (:1591)
    void update_ghost_history(); // 0x00633490 (:1720)
    void calculate_spacing(); // 0x00632CE0 (:1343)
    float get_min_spacing(const object* obj) const; // 0x00632CA0 (:1326)
    float get_human_base_distance(const human* hp) const; // 0x00632C30 (:1303)
    float get_object_base_distance(const object* obj) const; // 0x005E9D00 (:1288)
    int get_member_index(unsigned int member_handle) const; // 0x00632ED0 (:1487)
    bool add_member(const object* member); // 0x0065EA00 (:1380)
    void remove_follower_by_index(int member_index); // 0x0067BB60 (:1437)
    int get_leader_index(int member_index) const; // 0x005E9DA0 (:1455)
    void reorder_members(); // 0x00632F60 (:1627)
    unsigned int get_member_vehicle_handle(int member_index) const; // 0x00632F00 (:1510)
    void calculate_follower_positions(); // 0x00634C80 (:2186)
    void calculate_follower_positions_flock(); // 0x006336C0 (:1792)
    void calculate_follower_positions_rigid(); // 0x006348F0 (:2094)
    void calculate_follower_positions_follow(); // 0x00634070 (:1907)
    void calculate_follower_positions_follow_tight(); // 0x006342B0 (:1983)
public:
    ai_formation(); // 0x0064B000

    unsigned int handle; // 0x00
private:
    unsigned int leader_handle; // 0x04
    ai_formation_types formation_type; // 0x08
    float leader_to_follower_dist; // 0x0C
    float follower_to_follower_dist; // 0x10
    float follow_spacing_scale; // 0x14
    farray<ai_formation_member*,15> member_list; // 0x18
    circ_array<vector,20> formation_history; // 0x64
    float max_dist_from_history_point; // 0x15C
    timestamp reorder_time; // 0x160
};

float find_spline_dist(const vector& pos, int seg, vector* points, int num_points, int dir); // 0x005E9C20 (:793)
