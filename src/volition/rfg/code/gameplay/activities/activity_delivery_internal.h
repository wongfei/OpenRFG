// Original: volition/rfg/code/gameplay/activities/activity_delivery_internal.h
//
// The delivery activity (a modal activity): its run state, best times and end effects, and the radio lines. Placement of
// delivery_radio_lines inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class base_delivery_definition_data;
class npc;
class object;
class object_delivery_node;
class player;

enum delivery_radio_lines
{
    DELIVERY_RADIO_LINE_NONE = -1,
    DELIVERY_RADIO_LINE_START = 0,
    DELIVERY_RADIO_LINE_START_RESPONSE = 1,
    DELIVERY_RADIO_LINE_WIN_RESPONSE = 2,
    NUM_DELIVERY_RADIO_LINES = 3,
};

// size 0x4A8
class activity_delivery_data : public activity_base_modal
{
public:
    activity_delivery_data(); // 0x0077E630 (.h:28)
    virtual void system_init(); // vtbl+0x1C 0x007A0F90 (activity_delivery.cpp:119)
    virtual void level_init(); // vtbl+0x24 0x00797120 (activity_delivery.cpp:129)
    virtual bool activity_initialize(const object* spawn_obj, unsigned int new_district_handle); // vtbl+0x2C 0x00797170 (activity_delivery.cpp:138)
    virtual bool activity_start(); // vtbl+0x38 0x007AD670 (activity_delivery.cpp:226)
    virtual void activity_end(); // vtbl+0x3C 0x007973B0 (activity_delivery.cpp:281)
    virtual void activity_kill(); // vtbl+0x40 0x007974C0 (activity_delivery.cpp:320)
    virtual const char* get_title() const; // vtbl+0x80 0x00758050 (activity_delivery.cpp:328)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00758060 (activity_delivery.cpp:342)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x007580B0 (activity_delivery.cpp:356)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x00797670 (activity_delivery.cpp:426)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x0076C510 (activity_delivery.cpp:495)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B7D20 (activity_delivery.cpp:609)
    virtual void activity_clean_up_once(); // vtbl+0x6C 0x007580F0 (activity_delivery.cpp:760)
    virtual void activity_do_frame_paused(); // vtbl+0x68 0x00758120 (activity_delivery.cpp:767)
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x00758140 (activity_delivery.cpp:780)
    virtual void record_victory(); // vtbl+0xB0 0x007819E0 (activity_delivery.cpp:810)
    virtual bool on_alert(const npc* np) const; // vtbl+0xA4 0x00758150 (activity_delivery.cpp:867)
    virtual bool is_player_cover_blown() const; // vtbl+0x9C 0x00758160 (activity_delivery.cpp:877)
    void activity_start_end_effect(unsigned int end_node); // 0x00758170 (activity_delivery.cpp:887)
    void activity_stop_end_effect(unsigned int end_node); // 0x007581C0 (activity_delivery.cpp:904)
    void play_radio_line(delivery_radio_lines line); // 0x00758200 (activity_delivery.cpp:950)
    void append_radio_line(delivery_radio_lines line, int time_delay); // 0x00758280 (activity_delivery.cpp:983)
    unsigned int get_remaining_time(); // 0x0076C6E0 (activity_delivery.cpp:918)
    void play_timer_low_sound(int, int);
    float get_destination_dist(); // 0x00754480 (.h:103)
    unsigned int get_start_node();
    unsigned int get_end_node();
    float get_best_time(); // 0x0076C7E0 (activity_delivery.cpp:1134)
    float get_current_time(); // 0x0076C830 (activity_delivery.cpp:1148)
    float get_target_time(); // 0x00758310 (activity_delivery.cpp:1161)
    float get_par_time(); // 0x00758320 (activity_delivery.cpp:1166)
    bool do_end_processing(bool player_died); // 0x0076C5B0 (activity_delivery.cpp:524)
    bool is_retry_ready(); // 0x007580C0 (activity_delivery.cpp:576)
    void give_rewards(); // 0x007B5E10 (activity_delivery.cpp:596)
private:
    void reset(); // 0x00781B40 (activity_delivery.cpp:1026)
    virtual void render_icons() const; // vtbl+0x74 0x007977F0 (activity_delivery.cpp:1062)
    virtual void render_regions() const; // vtbl+0x78 0x0076C780 (activity_delivery.cpp:1121)
    void activity_lost(); // 0x00758330 (activity_delivery.cpp:1172)
    void activity_stop_effect(); // 0x0076C8A0 (activity_delivery.cpp:1181)
    void activity_delivery_get_total_statistics(unsigned int& total_time, int& num_activities, int& num_completed); // 0x00781C20 (activity_delivery.cpp:1195)
    object_delivery_node* find_best_delivery_node(const player* pp); // 0x007974E0 (activity_delivery.cpp:362)
public:
    virtual ~activity_delivery_data(); // vtbl+0x00 0x00769100

private:
    const base_delivery_definition_data* delivery_info; // 0x44C
    unsigned int delivery_vehicle; // 0x450
    unsigned int delivery_start_node; // 0x454
    unsigned int delivery_end_node; // 0x458
    vector delivery_start_pos; // 0x45C
    vector delivery_end_pos; // 0x468
    float distance_to_end; // 0x474
    timestamp time_left_to_complete; // 0x478
    timestamp time_left_to_fail; // 0x47C
    timestamp post_death_failure_time; // 0x480
    int previous_time_left_ms; // 0x484
    timestamp time_between_distance_checks; // 0x488
    int previous_second; // 0x48C
    bool time_warning_first_time; // 0x490
    bool alert_set_green; // 0x491
    bool timer_started; // 0x492
    bool splash_screen_active; // 0x493
    bool queued_start_conversation; // 0x494
    int standard_rewards_count; // 0x498
    bool successfully_completed; // 0x49C
    bool activity_failed; // 0x49D
    bool end_processing_started; // 0x49E
    bool pending_retry; // 0x49F
    unsigned int end_node_effect; // 0x4A0
    voice_line_handle most_recent_radio_line; // 0x4A4
};

extern activity_delivery_data Delivery_activity_data; // 0x01652E00
