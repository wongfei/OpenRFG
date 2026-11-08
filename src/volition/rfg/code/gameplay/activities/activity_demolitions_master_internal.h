// Original: volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h
// Implementation: volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp
//
// The Demolitions Master activity (a modal activity, one global instance): loading the level layers of a challenge,
// weapon and ammo limits, par time and goal checks, the interface strings, and the accept / reject flow. Movers retry
// their anchors only while it is not loading layers (Demolitions_master_activity.current_state).
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

enum activity_load_state;
class demolitions_master_level_data;
class object;
class object_demolitions_master_node;
class vehicle_info;

enum demolitions_master_states
{
    DM_STATE_INVALID = -1,
    DM_STATE_WAIT = 0,
    DM_STATE_READY = 1,
    DM_STATE_LOADING_LAYERS = 2,
    DM_STATE_PREGAME_BRIEFING = 3,
    DM_STATE_PREGAME_ACCEPT = 4,
    DM_STATE_PREGAME_REJECT = 5,
    DM_STATE_ACTIVE = 6,
    DM_STATE_COMPLETE = 7,
    DM_STATE_POSTGAME_BRIEFING = 8,
    DM_STATE_POSTGAME_REJECT = 9,
    DM_STATE_POSTGAME_DELAY = 10,
    DM_STATE_ENDING_SEQUENCE = 11,
    DM_STATE_SUCCESS = 12,
    DM_STATE_FAILURE = 13,
};

// size 0x518
class activity_demolitions_master : public activity_base_modal
{
public:
    activity_demolitions_master(); // 0x0077E990 (.h:53)
    virtual void system_init(); // vtbl+0x1C 0x0076CA10 (activity_demolitions_master.cpp:217)
    virtual void level_init(); // vtbl+0x24 0x0076CA20 (activity_demolitions_master.cpp:226)
    virtual bool activity_start(); // vtbl+0x38 0x007AD780 (activity_demolitions_master.cpp:279)
    void activity_restart();
    virtual void activity_end(); // vtbl+0x3C 0x00797AC0 (activity_demolitions_master.cpp:399)
    void do_activity_end(); // 0x0076CA70 (activity_demolitions_master.cpp:420)
    void activity_end_cancel(); // 0x00758510 (activity_demolitions_master.cpp:432)
    virtual void activity_kill(); // vtbl+0x40 0x00797B10 (activity_demolitions_master.cpp:451)
    void do_activity_kill(); // 0x007A1170 (activity_demolitions_master.cpp:475)
    void warp_player_on_end(); // 0x00797B60 (activity_demolitions_master.cpp:495)
    void do_post_activity_loading_process(bool force_darkness); // 0x00797C40 (activity_demolitions_master.cpp:518)
    void do_loading_process(bool layer_is_loaded, bool force_darkness); // 0x007A11C0 (activity_demolitions_master.cpp:553)
    virtual bool lock_alert_level() const; // vtbl+0xA0 0x007585C0 (activity_demolitions_master.cpp:698)
    virtual const char* get_title() const; // vtbl+0x80 0x007585D0 (activity_demolitions_master.cpp:710)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x007585E0 (activity_demolitions_master.cpp:725)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00758620 (activity_demolitions_master.cpp:743)
    const char* get_corner_hud_subtitle() const; // 0x00758600 (activity_demolitions_master.cpp:734)
    const int get_alt_interface_state() const; // 0x00758630 (activity_demolitions_master.cpp:749)
    const char* get_weapon_icon_name(int index); // 0x0076CB70 (activity_demolitions_master.cpp:765)
    const int get_weapon_ammo(int index); // 0x0076CBB0 (activity_demolitions_master.cpp:790)
    const char* get_info_title_1() const; // 0x00758660 (activity_demolitions_master.cpp:827)
    const char* get_info_title_2() const; // 0x00758680 (activity_demolitions_master.cpp:841)
    const char* get_description() const; // 0x007586C0 (activity_demolitions_master.cpp:887)
    const char* get_completion_text_part_1() const; // 0x00781EB0 (activity_demolitions_master.cpp:858)
    const char* get_completion_text_part_2() const; // 0x007586A0 (activity_demolitions_master.cpp:877)
    const int get_goal_time() const; // 0x007586E0 (activity_demolitions_master.cpp:900)
    const int get_display_time(); // 0x0076CC00 (activity_demolitions_master.cpp:910)
    const int get_par_time() const; // 0x00758700 (activity_demolitions_master.cpp:989)
    const int get_salvage_received(); // 0x00758720 (activity_demolitions_master.cpp:999)
    const int get_best_time() const; // 0x007587B0 (activity_demolitions_master.cpp:1021)
    const int get_stress_active_indicator() const; // 0x00781F90 (activity_demolitions_master.cpp:1037)
    const char* get_first_hint() const; // 0x007587D0 (activity_demolitions_master.cpp:1055)
    const char* get_second_hint() const; // 0x007587F0 (activity_demolitions_master.cpp:1065)
    void process_player_death(); // 0x00797D20 (activity_demolitions_master.cpp:1074)
    virtual bool activity_starting_completed() const; // vtbl+0x48 0x00782020 (activity_demolitions_master.cpp:1341)
    virtual activity_load_state activity_do_frame_loading(); // vtbl+0x54 0x00782010 (activity_demolitions_master.cpp:1334)
    virtual void activity_do_frame_starting(); // vtbl+0x5C 0x007A1900 (activity_demolitions_master.cpp:1350)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x007A1920 (activity_demolitions_master.cpp:1359)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007A1430 (activity_demolitions_master.cpp:1163)
    virtual void activity_do_frame_clean_up(); // vtbl+0x64 0x00797D40 (activity_demolitions_master.cpp:1102)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x00782060 (activity_demolitions_master.cpp:1388)
    virtual bool activity_clean_up_completed() const; // vtbl+0x44 0x00758810 (activity_demolitions_master.cpp:1091)
    virtual bool activity_initialize(const object* spawn_objp, unsigned int new_district_handle); // vtbl+0x2C 0x00798000 (activity_demolitions_master.cpp:1636)
    void stop_all_flares() const; // 0x007821B0 (activity_demolitions_master.cpp:1625)
    bool do_startup_initialization(); // 0x007A1970 (activity_demolitions_master.cpp:1693)
    bool finalize_activity_initialization(); // 0x007980F0 (activity_demolitions_master.cpp:1712)
    void cancel_activity_initialization(); // 0x007588B0 (activity_demolitions_master.cpp:1788)
    virtual void record_victory(); // vtbl+0xB0 0x007B6050 (activity_demolitions_master.cpp:1458)
    virtual void record_loss(); // vtbl+0xB4 0x00758870 (activity_demolitions_master.cpp:1480)
    virtual void render_icons() const; // vtbl+0x74 0x0076D0E0 (activity_demolitions_master.cpp:1798)
    bool is_building_destroyed() const; // 0x0076CF60 (activity_demolitions_master.cpp:1450)
    bool house_ready() const; // 0x007588D0 (activity_demolitions_master.cpp:1820)
    void do_spawn_post(const object_demolitions_master_node*);
    void reset(); // 0x00758460 (activity_demolitions_master.cpp:237)
    void begin_ending_sequence(bool ending_sequence_state, int delay_time_ms); // 0x007979B0 (activity_demolitions_master.cpp:372)
    void clean_area(); // 0x00797E20 (activity_demolitions_master.cpp:1578)
    void demolitions_master_accept_internal(); // 0x00781E30 (activity_demolitions_master.cpp:168)
    void demolitions_master_reject_internal(); // 0x0076C9D0 (activity_demolitions_master.cpp:194)
private:
    void reload_inventory(); // 0x00758540 (activity_demolitions_master.cpp:639)
    void restore_previous_player_vehicle(); // 0x0076CAB0 (activity_demolitions_master.cpp:658)
    bool activity_should_end(); // 0x007A0FE0 (activity_demolitions_master.cpp:308)
    void flag_demo_master_buildings_for_minimap_coloration(unsigned int target_handle, bool add_mark); // 0x0076CEB0 (activity_demolitions_master.cpp:1138)
public:
    virtual ~activity_demolitions_master(); // vtbl+0x00 0x007691B0

    unsigned int house_handle; // 0x44C
    unsigned int dm_node_handle; // 0x450
    unsigned int player_last_vehicle_handle; // 0x454
    unsigned int activity_vehicle_handle; // 0x458
    unsigned int district_handle; // 0x45C
    demolitions_master_level_data* base_demolitions_master_data; // 0x460
    vector house_center; // 0x464
    vector region_center; // 0x470
    vector player_start_end_position; // 0x47C
    matrix player_start_end_orient; // 0x488
    timestamp do_frame_polling_period; // 0x4AC
    timestamp spawn_polling_period; // 0x4B0
    timestamp time_remaining; // 0x4B4
    timestamp time_elapsed; // 0x4B8
    timestamp post_activity_timer; // 0x4BC
    timestamp out_of_range_timer; // 0x4C0
    timestamp out_of_ammo_timer; // 0x4C4
    timestamp loading_screen_timer; // 0x4C8
    bool loading_done; // 0x4CC
    bool loading_screen_shown; // 0x4CD
    bool began_startup; // 0x4CE
    bool player_info_reloaded; // 0x4CF
    bool player_is_ignored; // 0x4D0
    demolitions_master_states current_state; // 0x4D4
    int previous_second; // 0x4D8
private:
    bool pre_activity_unlimited_ammo_state; // 0x4DC
    const vehicle_info* saved_player_vehicle; // 0x4E0
    struct
    {
        bool activity_over; // 0x00
        bool ending_sequence_state; // 0x01
        int elapsed_time; // 0x04
        int par_time; // 0x08
        int remaining_time; // 0x0C
        int salvage_received; // 0x10
        char status_string1[16]; // 0x14
        char status_string2[16]; // 0x24
    } completion_stats; // 0x4E4
};

extern activity_demolitions_master Demolitions_master_activity; // 0x022296E8

bool dm_item_cb(object* hit_objp, unsigned int shape_key, const void* user_data); // 0x0076CFA0 (activity_demolitions_master.cpp:1488)
bool dm_vehicle_cb(object* hit_objp, unsigned int shape_key, const void* user_data); // 0x0076D030 (activity_demolitions_master.cpp:1526)
bool dm_general_mover_cb(object* hit_objp, unsigned int shape_key, const void* user_data); // 0x0076D080 (activity_demolitions_master.cpp:1548)
bool demolitions_master_has_interface(); // 0x007588E0 (activity_demolitions_master.cpp:1831)
const char* demolitions_master_get_corner_hud_subtitle(); // 0x00758910 (activity_demolitions_master.cpp:1846)
const int demolitions_master_get_display_time(); // 0x0076D1F0 (activity_demolitions_master.cpp:1851)
const int demolitions_master_get_par_time(); // 0x00758930 (activity_demolitions_master.cpp:1856)
const int demolitions_master_get_salvage_received(); // 0x00758950 (activity_demolitions_master.cpp:1861)
const int demolitions_master_get_stress_active_indicator(); // 0x00782200 (activity_demolitions_master.cpp:1866)
void demolitions_master_process_player_death(); // 0x00798310 (activity_demolitions_master.cpp:1871)
bool activity_demolitions_master_alert_allows_start(); // 0x0076D200 (activity_demolitions_master.cpp:1877)
bool activity_demolitions_master_start(const object_demolitions_master_node* dm_node); // 0x007A1A80 (activity_demolitions_master.cpp:1898)
void demolitions_master_accept(); // 0x00782210 (activity_demolitions_master.cpp:1932)
void demolitions_master_reject(); // 0x0076D230 (activity_demolitions_master.cpp:1938)
