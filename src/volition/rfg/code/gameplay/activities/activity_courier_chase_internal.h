// Original: volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h
//
// The courier chase activity (a modal activity): the courier definitions (Courier_definitions), the courier case, and
// the radio lines. Placement of the definitions and lines inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

enum activity_disable_reason;
class base_activity_data;
class human;
class object;
class object_courier_end_point;
class object_squad;
struct squad_definition;
class vehicle;
class vehicle_spawn_group_info;

enum courier_chase_radio_lines
{
    CTC_RL_NONE = -1,
    CTC_RL_START_MALE = 0,
    CTC_RL_START_FEMALE = 1,
    CTC_RL_COURIER_SPOTTED_PC = 2,
    CTC_RL_COURIER_SPOTTED_RES_MALE = 3,
    CTC_RL_COURIER_SPOTTED_RES_FEMALE = 4,
    CTC_RL_OUT_OF_RANGE_PC = 5,
    CTC_RL_OUT_OF_RANGE_RES_MALE = 6,
    CTC_RL_OUT_OF_RANGE_RES_FEMALE = 7,
    CTC_RL_DESTROYED_DATA_PC = 8,
    CTC_RL_DESTROYED_DATA_RES_MALE = 9,
    CTC_RL_DESTROYED_DATA_RES_FEMALE = 10,
    CTC_RL_CAPTURED_DATA_PC = 11,
    CTC_RL_CAPTURED_DATA_RES_MALE = 12,
    CTC_RL_CAPTURED_DATA_RES_FEMALE = 13,
    CTC_RL_TRANSMITTING_DATA_PC = 14,
    CTC_RL_TRANSMITTING_DATA_RES_MALE = 15,
    CTC_RL_TRANSMITTING_DATA_RES_FEMALE = 16,
    NUM_CTC_RADIO_LINES = 17,
};

// size 0x74
class courier_definition
{
public:
    const char* name; // 0x00
    const base_activity_data* base_data; // 0x04
    int reward; // 0x08
    int start_delay; // 0x0C
    float normal_speed; // 0x10
    float evade_speed; // 0x14
    float catch_up_speed; // 0x18
    float escape_speed; // 0x1C
    float vehicle_mass_multiplier; // 0x20
    float vehicle_health_multiplier; // 0x24
    const squad_definition* squad_def; // 0x28
    const vehicle_spawn_group_info* vehicle_group; // 0x2C
    voice_line_handle radio_lines[17]; // 0x30
};

// size 0x1CD0
class activity_courier_chase_data : public activity_base_modal
{
public:
    // size 0x2
    struct courier_flags
    {
        bool spawned : 1; // 0x00:0
        bool chased : 1; // 0x00:1
        bool escaping : 1; // 0x00:2
        bool allow_catch_up : 1; // 0x00:3
        bool at_destination : 1; // 0x00:4
        bool data_dropped : 1; // 0x00:5
        bool data_retrieved : 1; // 0x00:6
        bool completed : 1; // 0x00:7
        bool simulated_last_frame : 1; // 0x01:0
    };

    activity_courier_chase_data(); // 0x0077E770 (.h:20)
    virtual void system_init(); // vtbl+0x1C 0x00757C60 (activity_courier_chase.cpp:248)
    virtual void level_init(); // vtbl+0x24 0x00797040 (activity_courier_chase.cpp:266)
    virtual bool activity_start(); // vtbl+0x38 0x007AD560 (activity_courier_chase.cpp:636)
    virtual void activity_end(); // vtbl+0x3C 0x00797090 (activity_courier_chase.cpp:878)
    virtual void activity_kill(); // vtbl+0x40 0x007A8DF0 (activity_courier_chase.cpp:674)
    virtual void reinstate(activity_disable_reason disable_reason); // vtbl+0x0C 0x00781740 (activity_courier_chase.cpp:698)
    void play_radio_line(courier_chase_radio_lines courier_line, courier_chase_radio_lines to_follow_line); // 0x00757CB0 (activity_courier_chase.cpp:274)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x007B5D10 (activity_courier_chase.cpp:706)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B7AE0 (activity_courier_chase.cpp:732)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x007B1E00 (activity_courier_chase.cpp:1218)
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x007819B0 (activity_courier_chase.cpp:1209)
    virtual void render_icons() const; // vtbl+0x74 0x007817C0 (activity_courier_chase.cpp:885)
    virtual void render_regions() const; // vtbl+0x78 0x007818A0 (activity_courier_chase.cpp:950)
    virtual bool render_hud(int x_pos, int y_pos); // vtbl+0x70 0x0076BEC0 (activity_courier_chase.cpp:918)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00757D60 (activity_courier_chase.cpp:974)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00757DC0 (activity_courier_chase.cpp:1010)
private:
    void check_for_start(); // 0x007B5AD0 (activity_courier_chase.cpp:331)
    void check_for_end(); // 0x007B74A0 (activity_courier_chase.cpp:474)
    void activity_end_common(bool courier_escaped); // 0x007A8EC0 (activity_courier_chase.cpp:843)
    void activity_end_victory(); // 0x007B5D70 (activity_courier_chase.cpp:805)
    void activity_end_loss(bool courier_escaped); // 0x007AD640 (activity_courier_chase.cpp:824)
    void reset(); // 0x007816B0 (activity_courier_chase.cpp:300)
    void create_courier_case(const vector& case_pos); // 0x007B1C10 (activity_courier_chase.cpp:431)
public:
    vector get_courier_position() const; // 0x0076BFE0 (activity_courier_chase.cpp:1027)
    vehicle* get_courier_vehicle(); // 0x0076C080 (activity_courier_chase.cpp:1048)
    object_squad* get_courier_squad() const; // 0x0076BFA0 (activity_courier_chase.cpp:1016)
    float get_move_speed() const; // 0x00757DD0 (activity_courier_chase.cpp:1071)
    void update_destination(const vector& new_pos, bool simulated); // 0x0076C230 (activity_courier_chase.cpp:1155)
    void update_destination_ghost(bool simulated); // 0x0076C0F0 (activity_courier_chase.cpp:1098)
    void update_destination_patrol(); // 0x0076C190 (activity_courier_chase.cpp:1133)
    bool get_ghost_destination(const vector& my_pos, vector& dest_pos); // 0x0076C250 (activity_courier_chase.cpp:1165)
    bool is_at_current_destination() const; // 0x0076C2A0 (activity_courier_chase.cpp:1183)
    bool is_at_final_destination(); // 0x0076C300 (activity_courier_chase.cpp:1202)
    bool chased() const; // 0x00754500 (.h:115)
    bool escaping() const; // 0x00754510 (.h:117)
    bool allow_follower_catch_up() const; // 0x00754520 (.h:119)
    bool most_recent_courier_escaped() const; // 0x00754530 (.h:121)
    unsigned int get_courier_squad_handle() const; // 0x00754540 (.h:123)
    void scripted_activity_courier_escape(); // 0x00757D40 (activity_courier_chase.cpp:834)
    void make_courier_stuck(); // 0x00781780 (activity_courier_chase.cpp:726)
    void apply_vehicle_health_and_mass_attributes(); // 0x0076C340 (activity_courier_chase.cpp:1401)
private:
    const object_courier_end_point* pick_start_position();
    bool spawn_courier(const object_courier_end_point* start_point); // 0x00757E10 (activity_courier_chase.cpp:1385)
    void update_player_in_target_vehicle();
    void update_squad_vehicles();
    const human* find_or_create_attacker();
    void update_npc_beliefs();
public:
    virtual ~activity_courier_chase_data(); // vtbl+0x00 0x00769120

private:
    const courier_definition* courier_def; // 0x44C
    unsigned int courier_end_handle; // 0x450
    int activity_level; // 0x454
    courier_flags flags; // 0x458
    patrol_node_handle current_destination; // 0x45C
    unsigned int squad_handle; // 0x468
    farray<vector,512> courier_route; // 0x46C
    int next_visited_index; // 0x1C7C
    unsigned int patrol_handle; // 0x1C80
    unsigned int vehicle_handle; // 0x1C84
    unsigned int courier_case_handle; // 0x1C88
    vector patrol_end_pos; // 0x1C8C
    vector data_debug_location; // 0x1C98
    vector courier_stuck_pos; // 0x1CA4
    bool end_activity_next_frame; // 0x1CB0
    bool last_courier_escaped; // 0x1CB1
public:
    timestamp out_of_bounds_timestamp; // 0x1CB4
    timestamp escaping_timestamp; // 0x1CB8
    timestamp start_delay_timestamp; // 0x1CBC
    timestamp polling_period; // 0x1CC0
    timestamp courier_stuck_timestamp; // 0x1CC4
    unsigned int orders_handle; // 0x1CC8
    bool spawned_from_console; // 0x1CCC
    bool radio_gender_male; // 0x1CCD
};

extern activity_courier_chase_data Courier_chase_data; // 0x02242BA8
