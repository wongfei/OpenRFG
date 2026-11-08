// Original: volition/rfg/code/gameplay/activities/activity_convoy_internal.h
//
// The convoy activity (a modal activity): the convoy vehicles and their member flags, the nodes and spawn infos it is
// built from, the convoy state (leader, squads) and the radio lines. Placement of the enums and spawn structs inferred.
#pragma once

#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

enum activity_disable_reason;
struct human_flags;
class navpoint;
class npc;
class object;
class object_convoy_end_point;
class object_squad;
class vehicle;
struct vehicle_flags;

enum convoy_radio_lines
{
    CONVOY_RADIO_LINE_NONE = -1,
    CONVOY_RL_START_F = 0,
    CONVOY_RL_START_M = 1,
    CONVOY_RL_CAPTURED_VEHICLE = 2,
    CONVOY_RL_CAPTURED_VEHICLE_RES_F = 3,
    CONVOY_RL_CAPTURED_VEHICLE_RES_M = 4,
    CONVOY_RL_DESTROYED_VEHICLE = 5,
    CONVOY_RL_DESTROYED_VEHICLE_RES_F = 6,
    CONVOY_RL_DESTROYED_VEHICLE_RES_M = 7,
    CONVOY_RL_DELIVERED_VEHICLE_F = 8,
    CONVOY_RL_DELIVERED_VEHICLE_M = 9,
    CONVOY_RL_ESCAPING_VEHICLE_F = 10,
    CONVOY_RL_ESCAPING_VEHICLE_M = 11,
    CONVOY_RL_FAIL_F = 12,
    CONVOY_RL_FAIL_M = 13,
    CONVOY_RL_DISTANCE_25_F = 14,
    CONVOY_RL_DISTANCE_25_M = 15,
    CONVOY_RL_DISTANCE_50_F = 16,
    CONVOY_RL_DISTANCE_50_M = 17,
    CONVOY_RL_DISTANCE_75_F = 18,
    CONVOY_RL_DISTANCE_75_M = 19,
    NUM_CONVOY_RADIO_LINES = 20,
};

enum convoy_state
{
    CONVOY_STATE_INVALID = -1,
    CONVOY_STATE_NO_SQUADS = 0,
    CONVOY_STATE_NO_LEADER = 1,
    CONVOY_STATE_RUNNING = 2,
    NUM_CONVOY_STATES = 3,
};

// size 0x4
struct convoy_member_flags
{
    void reset(); // 0x007546E0 (.h:69)

    unsigned int limited_vehicle_exit : 1; // 0x00:0
    unsigned int disallow_vehicle_exit : 1; // 0x00:1
    unsigned int convoy_vehicle_exit : 1; // 0x00:2
    unsigned int player_may_capture : 1; // 0x00:3
    unsigned int disable_for_player : 1; // 0x00:4
    unsigned int m_scripted_abandon : 1; // 0x00:5
};

// size 0xC
struct convoy_node_info
{
    const navpoint* node_navp; // 0x00
    road_spline_identifier node_road_id; // 0x04
};

// size 0x18
struct convoy_spawn_info
{
    convoy_spawn_info(); // 0x00A162F0

    vector spawn_pos; // 0x00
    vector spawn_direction; // 0x0C
};

// size 0x10
class convoy_vehicle_data
{
public:
    convoy_vehicle_data(); // 0x007691D0 (.h:18)
    void reset(); // 0x007546B0 (.h:48)
    bool in_use() const; // 0x007546D0 (.h:56)

    unsigned int squad_handle; // 0x00
    unsigned int vehicle_handle; // 0x04
    convoy_squad_roles role; // 0x08
    struct
    {
        unsigned int squad_killed : 1; // 0x00:0
        unsigned int vehicle_killed : 1; // 0x00:1
        unsigned int capture_vehicle : 1; // 0x00:2
        unsigned int left_convoy : 1; // 0x00:3
        unsigned int no_vehicle_turret : 1; // 0x00:4
        unsigned int simulated_last_frame : 1; // 0x00:5
    } convoy_veh_flags; // 0x0C
};

// size 0x59C
class activity_convoy_data : public activity_base_modal
{
public:
    activity_convoy_data(); // 0x0077EC20 (.h:80)
    virtual void system_init(); // vtbl+0x1C 0x007577D0 (activity_convoy.cpp:328)
    virtual void level_init(); // vtbl+0x24 0x0076A600 (activity_convoy.cpp:336)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x00780C50 (activity_convoy.cpp:1078)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B1B40 (activity_convoy.cpp:686)
    virtual void activity_do_frame_clean_up(); // vtbl+0x64 0x007AD020 (activity_convoy.cpp:635)
    virtual bool activity_clean_up_completed() const; // vtbl+0x44 0x0076A840 (activity_convoy.cpp:625)
    virtual bool activity_begin_start_up(); // vtbl+0x34 0x00780A50 (activity_convoy.cpp:607)
    void activity_do_frame_active_capture(); // 0x00796DA0 (activity_convoy.cpp:490)
    void activity_do_frame_active_destroy(); // 0x007A0E00 (activity_convoy.cpp:552)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x00796EA0 (activity_convoy.cpp:1233)
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x00780DA0 (activity_convoy.cpp:1134)
    virtual void record_victory(); // vtbl+0xB0 0x007B5A40 (activity_convoy.cpp:1203)
    virtual bool activity_start(); // vtbl+0x38 0x007B59A0 (activity_convoy.cpp:351)
    virtual void activity_end(); // vtbl+0x3C 0x00796D10 (activity_convoy.cpp:370)
    virtual void activity_kill(); // vtbl+0x40 0x00796D40 (activity_convoy.cpp:382)
    virtual void reinstate(activity_disable_reason disable_reason); // vtbl+0x0C 0x0076A660 (activity_convoy.cpp:403)
    void process_target_driver_flags(const vehicle* target_vehicle); // 0x0076A6A0 (activity_convoy.cpp:413)
    void set_activity_level(int);
    int get_activity_level() const; // 0x007546F0 (.h:132)
    void release_attack_squads(); // 0x007B1BB0 (activity_convoy.cpp:1420)
    bool convoy_was_attacked(const object_convoy* my_convoy); // 0x0076B630 (activity_convoy.cpp:1027)
    void play_radio_line(convoy_radio_lines convoy_line, convoy_radio_lines to_follow_line); // 0x007576D0 (activity_convoy.cpp:270)
    virtual const char* get_title() const; // vtbl+0x80 0x00757AF0 (activity_convoy.cpp:2237)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00757B00 (activity_convoy.cpp:2251)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00757B50 (activity_convoy.cpp:2277)
    bool vehicle_in_convoy(const vehicle* vp); // 0x007578B0 (activity_convoy.cpp:1761)
    int get_num_vehicles() const; // 0x0076BB10 (activity_convoy.cpp:1780)
    int get_num_spawn_vehicles() const; // 0x007691F0 (.h:161)
    float get_target_distance() const; // 0x00781280 (activity_convoy.cpp:1822)
    float get_target_health(); // 0x0076BBA0 (activity_convoy.cpp:1955)
    bool get_player_in_target_vehicle() const; // 0x00754700 (.h:170)
    void squad_left_convoy(unsigned int squad_handle); // 0x00757970 (activity_convoy.cpp:1936)
    void capture_target_left_convoy(); // 0x00757930 (activity_convoy.cpp:1922)
    void convert_all_squads(); // 0x00757900 (activity_convoy.cpp:1907)
    convoy_type get_convoy_type() const; // 0x007578E0 (activity_convoy.cpp:1897)
    virtual bool on_alert(const npc* np) const; // vtbl+0xA4 0x00757B80 (activity_convoy.cpp:2300)
    object_convoy* get_convoy() const; // 0x0076BDA0 (activity_convoy.cpp:2309)
    void register_player_hit(const vector& hit_location, unsigned int rfg_mover_handle); // 0x0076B4F0 (activity_convoy.cpp:964)
    void convoy_at_destination(); // 0x007AD460 (activity_convoy.cpp:2120)
private:
    void reset(); // 0x0076A570 (activity_convoy.cpp:304)
    virtual void render_icons() const; // vtbl+0x74 0x00781090 (activity_convoy.cpp:1677)
    virtual bool render_hud(int x_pos, int y_pos); // vtbl+0x70 0x007578A0 (activity_convoy.cpp:1753)
    const object_convoy_end_point* pick_start_position(); // 0x00780D40 (activity_convoy.cpp:1118)
    bool spawn_convoy(const object_convoy_end_point* start_point); // 0x00780FA0 (activity_convoy.cpp:1274)
    void find_message_points(const object_convoy* convoy); // 0x0076B6E0 (activity_convoy.cpp:1262)
    void update_player_in_target_vehicle(); // 0x0076BA10 (activity_convoy.cpp:1616)
    void update_squad_status(); // 0x0076B8C0 (activity_convoy.cpp:1515)
    void update_squad_vehicles(); // 0x0076B960 (activity_convoy.cpp:1554)
    void update_squad_orders(); // 0x007AD2C0 (activity_convoy.cpp:1489)
    void update_squad_orders_attack(convoy_vehicle_data& current_squad); // 0x007AD180 (activity_convoy.cpp:1449)
    void update_npc_beliefs(); // 0x0076B7F0 (activity_convoy.cpp:1373)
    void build_squad_list(const object_convoy* convoy); // 0x0076B720 (activity_convoy.cpp:1317)
    void target_vehicle_delivered(); // 0x0076BC00 (activity_convoy.cpp:1977)
    void destruction_convoy_destroyed(); // 0x00796F10 (activity_convoy.cpp:2027)
    void destruction_convoy_escaped(bool play_radio); // 0x007579C0 (activity_convoy.cpp:2014)
    void capture_convoy_destroyed(); // 0x00757A60 (activity_convoy.cpp:2177)
    void capture_convoy_escaped(); // 0x00757A00 (activity_convoy.cpp:2160)
    bool any_convoy_squad_alive() const; // 0x007577E0 (activity_convoy.cpp:532)
    bool convert_despawning_squad(const convoy_vehicle_data& squad_info, object_squad* squad); // 0x007AD320 (activity_convoy.cpp:2058)
    const convoy_definition* get_convoy_definition() const; // 0x00754710 (.h:255)
    bool convoy_target_destroyed() const; // 0x0076BCF0 (activity_convoy.cpp:2194)
    const object* get_target_object() const; // 0x00781040 (activity_convoy.cpp:1651)
    void update_target_vehicle_driver(); // 0x00780B30 (activity_convoy.cpp:880)
    void update_cached_vehicle_handle(); // 0x0076B5D0 (activity_convoy.cpp:1003)
    object_squad* get_target_squad() const; // 0x0076B4A0 (activity_convoy.cpp:944)
    void update_all_member_flags(); // 0x0076A890 (activity_convoy.cpp:707)
    void set_member_flags(const convoy_member_flags& flag_status, vehicle_flags& vflags); // 0x00757810 (activity_convoy.cpp:774)
    void set_member_flags(const convoy_member_flags& flag_status, human_flags& hflags); // 0x00757850 (activity_convoy.cpp:782)
    void set_ready_flags(); // 0x00780A90 (activity_convoy.cpp:791)
    void set_ready_flags_capture(); // 0x0076AAD0 (activity_convoy.cpp:803)
    void set_ready_flags_destroy(); // 0x0076AD40 (activity_convoy.cpp:819)
    void set_active_flags(); // 0x00780AE0 (activity_convoy.cpp:834)
    void set_active_flags_capture(); // 0x0076AFB0 (activity_convoy.cpp:846)
    void set_active_flags_destroy(); // 0x0076B230 (activity_convoy.cpp:864)
public:
    virtual ~activity_convoy_data(); // vtbl+0x00 0x00769210

private:
    timestamp squad_spawn_timeout; // 0x44C
    timestamp orders_update_period; // 0x450
    timestamp belief_prop_timer; // 0x454
    unsigned int my_convoy_handle; // 0x458
    unsigned int convoy_end_handle; // 0x45C
    int activity_level; // 0x460
    unsigned int radio_line_flags; // 0x464
    struct
    {
        unsigned int built_squad_list : 1; // 0x00:0
        unsigned int player_in_target_vehicle : 1; // 0x00:1
        unsigned int played_capture_lines : 1; // 0x00:2
        unsigned int start_next_frame : 1; // 0x00:3
        unsigned int added_safehouse_marker : 1; // 0x00:4
    } convoy_activity_flags; // 0x468
    unsigned int cached_vehicle_handle; // 0x46C
    float cached_vehicle_health; // 0x470
    convoy_vehicle_data squad_array[12]; // 0x474
    convoy_member_flags member_flag_status[4]; // 0x534
    farray<unsigned int,12> despawning_squad_list; // 0x544
    const convoy_definition* convoy_def; // 0x584
    int distance_25_message_node_index; // 0x588
    int distance_50_message_node_index; // 0x58C
    int distance_75_message_node_index; // 0x590
    timestamp victory_timer; // 0x594
public:
    bool responses_are_female; // 0x598
};

extern activity_convoy_data Convoy_activity_data; // 0x022451E0
