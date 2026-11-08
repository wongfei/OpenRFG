// Original: volition/rfg/code/gameplay/activities/activity_area_defense_internal.h
//
// The area defense activity (a modal activity): its per-node definitions (activity_area_defense_info), the reinforcement
// waves, the active sub-states and the end conditions (success, player killed, player left the area). Placement of the
// enums inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

enum activity_load_state;
class base_activity_data;
struct cfile;
class district;
class npc;
class object;
class object_patrol;
class object_squad;
class player;
struct xml_element;

enum area_defense_active_states
{
    INVALID_AD_ACTIVE_STATE = -1,
    AD_ACTIVE_STATE_WAIT_PLAYER = 0,
    AD_ACTIVE_STATE_RUNNING = 1,
    AD_ACTIVE_STATE_WAIT_SPLASH_SCREEN = 2,
    AD_ACTIVE_STATE_CLEANUP = 3,
    AD_ACTIVE_STATE_END = 4,
    AD_ACTIVE_STATE_LOAD_LAYERS = 5,
    NUM_AD_ACTIVE_STATES = 6,
};

enum area_defense_complete_conditions
{
    AD_COMPLETE_SUCCESS = 0,
    AD_COMPLETE_PLAYER_KILLED = 1,
    AD_COMPLETE_LEFT_AREA = 2,
    NUM_AD_COMPLETE_CONDITIONS = 3,
};

enum area_defense_kill_types
{
    AD_KILL_VEHICLES = 0,
    AD_KILL_HUMANS = 1,
    NUM_AD_KILL_TYPES = 2,
};

// size 0x10
struct area_defense_reinforcement_info
{
    bool load(xml_element* xml_reinforcement_info); // 0x00756CF0 (activity_area_defense.cpp:525)
    const int resolve_continuous_attack_name() const; // 0x00769F00 (activity_area_defense.cpp:539)
    void reset(); // 0x00753E60 (.h:72)

    const char* continuous_attack_name; // 0x00
    int ca_index; // 0x04
    int kill_limit; // 0x08
    int time_limit; // 0x0C
};

// size 0x8C
struct activity_area_defense_info
{
    char* name; // 0x00
    const base_activity_data* base_data; // 0x04
    activity_standard_rewards standard_rewards; // 0x08
    float radius; // 0x14
    float radius_squared; // 0x18
    int required_kills; // 0x1C
    area_defense_kill_types kill_type; // 0x20
    area_defense_reinforcement_info reinforcements[5]; // 0x24
    int num_reinforcements; // 0x74
    const char* persona_name; // 0x78
    const char* start_line_name; // 0x7C
    const char* end_line_name; // 0x80
    const char* encourage_a_name; // 0x84
    const char* encourage_b_name; // 0x88
};

// size 0x47C
class activity_area_defense_data : public activity_base_modal
{
public:
    activity_area_defense_data(); // 0x0077DDB0 (.h:105)
    virtual void system_init(); // vtbl+0x1C 0x00796600 (activity_area_defense.cpp:551)
    virtual void level_init(); // vtbl+0x24 0x00769F20 (activity_area_defense.cpp:564)
    virtual const char* get_title() const; // vtbl+0x80 0x00756D60 (activity_area_defense.cpp:573)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00756D70 (activity_area_defense.cpp:587)
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x0077F9C0 (activity_area_defense.cpp:631)
    virtual bool activity_begin_start_up(); // vtbl+0x34 0x0077F980 (activity_area_defense.cpp:609)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x00796630 (activity_area_defense.cpp:646)
    virtual bool activity_start(); // vtbl+0x38 0x007ACD20 (activity_area_defense.cpp:674)
    virtual void activity_end(); // vtbl+0x3C 0x007966C0 (activity_area_defense.cpp:684)
    virtual void activity_kill(); // vtbl+0x40 0x007A8B50 (activity_area_defense.cpp:696)
    virtual void record_victory(); // vtbl+0xB0 0x007B55C0 (activity_area_defense.cpp:712)
    virtual void render_icons() const; // vtbl+0x74 0x00769F70 (activity_area_defense.cpp:751)
    virtual void activity_complete(); // vtbl+0x4C 0x0077FA70 (activity_area_defense.cpp:772)
    virtual activity_load_state activity_do_frame_loading(); // vtbl+0x54 0x007A8BC0 (activity_area_defense.cpp:1071)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x007A8BF0 (activity_area_defense.cpp:1083)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x007A8BA0 (activity_area_defense.cpp:939)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B5670 (activity_area_defense.cpp:1019)
    virtual void activity_do_frame_clean_up(); // vtbl+0x64 0x007ACD30 (activity_area_defense.cpp:955)
    void start_cleanup(); // 0x00756DE0 (activity_area_defense.cpp:963)
    void player_equipment_cleanup(); // 0x007A0890 (activity_area_defense.cpp:973)
    virtual bool activity_clean_up_completed() const; // vtbl+0x44 0x00756DB0 (activity_area_defense.cpp:949)
    virtual bool start_activity_spawn(const object* spawn_objp, const district* dp); // vtbl+0x08 0x007A05C0 (activity_area_defense.cpp:780)
    void warp_player_on_end(); // 0x007A05F0 (activity_area_defense.cpp:793)
    virtual bool on_alert(const npc* np) const; // vtbl+0xA4 0x00756E00 (activity_area_defense.cpp:1131)
    virtual void register_death(const object* target_obj, const object* attacker_obj); // vtbl+0xBC 0x00769FD0 (activity_area_defense.cpp:886)
    virtual void save_activity_state(cfile* save_file); // vtbl+0xD0 0x00756E10 (activity_area_defense.cpp:1140)
    virtual void load_activity_state(cfile* save_file); // vtbl+0xD4 0x00756E40 (activity_area_defense.cpp:1145)
    virtual void render_regions() const; // vtbl+0x78 0x0076A020 (activity_area_defense.cpp:905)
    void stop_all_flares() const; // 0x0077FB30 (activity_area_defense.cpp:1060)
    const player* player_in_area_defense_region(); // 0x0077FA90 (activity_area_defense.cpp:921)
    void process_completion_condition(area_defense_complete_conditions condition); // 0x00756FD0 (activity_area_defense.cpp:1533)
    bool is_running() const; // 0x00753E70 (.h:203)
protected:
    void reset(); // 0x00756E70 (activity_area_defense.cpp:1160)
    void test_reinforcement_conditions(); // 0x007B1440 (activity_area_defense.cpp:1191)
    void test_complete_conditions(); // 0x0076A0E0 (activity_area_defense.cpp:1284)
    bool is_inside_region(const vector& point) const; // 0x00756EE0 (activity_area_defense.cpp:1322)
    void do_frame_active_wait_player(); // 0x00756F30 (activity_area_defense.cpp:1328)
    void do_frame_active_running(); // 0x007B1610 (activity_area_defense.cpp:1366)
    void do_frame_cleanup(); // 0x007A8CD0 (activity_area_defense.cpp:1466)
    void do_frame_load_layers(); // 0x007A09F0 (activity_area_defense.cpp:1490)
    bool object_setup(); // 0x00757090 (activity_area_defense.cpp:1579)
    object_squad* spawn_squad(const object_patrol*);
    bool area_defense_ready() const; // 0x00753E90 (.h:236)
public:
    virtual ~activity_area_defense_data(); // vtbl+0x00 0x00768D30

protected:
    unsigned int area_defense_node_handle; // 0x44C
    area_defense_active_states active_state; // 0x450
    timestamp max_start_period; // 0x454
    timestamp area_defense_timestamp; // 0x458
    int reinforcement_level; // 0x45C
    bool start_continuous_attack; // 0x460
    bool processed_completion; // 0x461
    bool player_wins_activity; // 0x462
    bool began_startup; // 0x463
public:
    int vehicle_kills; // 0x464
    int human_kills; // 0x468
    const activity_area_defense_info* current_info; // 0x46C
    vector area_defense_center; // 0x470
};

extern activity_area_defense_data Area_defense_activity_data; // 0x01651960
