// Original: volition/rfg/code/hud/hud_icon.h
// Implementation: volition/rfg/code/hud/hud_icon.cpp
//
// HUD icons drawn in the world (player names, markers, backpacks) by hud_icon_render. Placement of hud_icons inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/color.h"

enum hud_icons
{
    HI_INVALID = -1,
    HI_ATTACK = 0,
    HI_DEFEND = 1,
    HI_TEAM_GUERRILLA = 2,
    HI_TEAM_EDF = 3,
    HI_STATUS_RING = 4,
    HI_MUTE = 5,
    HI_RING = 6,
    HI_FLAG = 7,
    HI_FLAG_HELD = 8,
    HI_LOCK = 9,
    HI_BAGMAN_FLAG_DROPPED = 10,
    HI_BAGMAN_FLAG_HELD = 11,
    HI_ENGINEER = 12,
    HI_STAR = 13,
    NUM_HUD_ICONS = 14,
};

// size 0x134
struct hud_icon
{
    hud_icon(); // 0x008669F0

    unsigned int obj; // 0x00
    bool visible; // 0x04
    wchar_t text[3][32]; // 0x06
    float text_scale; // 0xC8
    color text_color; // 0xCC
    int text_font; // 0xD0
    int bitmap_id; // 0xD4
    float icon_scale; // 0xD8
    color icon_color; // 0xDC
    bool display_distance; // 0xE0
    bool snap_to_edge; // 0xE1
    int fade_in_time_ms; // 0xE4
    int fade_out_time_ms; // 0xE8
    float fade_distance; // 0xEC
    int status_ring_bitmap_id; // 0xF0
    color status_ring_color; // 0xF4
    int status_ring_interval_ms; // 0xF8
    int status_ring_lifetime_ms; // 0xFC
    int status_ring_total_lifetime_ms; // 0x100
    float status_ring_max_scale; // 0x104
    bool remove_icon_when_obj_is_destroyed; // 0x108
    bool (*callback)(vector&, hud_icon*); // 0x10C
    void (*post_render_callback)(const vector&, hud_icon*); // 0x110
    const void* parameter; // 0x114
    struct
    {

        vector obj_pos_last_frame; // 0x00
        int status_ring_start_time; // 0x0C
        int status_ring_end_time; // 0x10
        int fade_start_time_ms; // 0x14
        bool first_frame_setup : 1; // 0x18:0
        bool visible_last_frame : 1; // 0x18:1
        bool use_status_rings : 1; // 0x18:2
        bool fading_in : 1; // 0x18:3
        bool fading_out : 1; // 0x18:4
    } internal_state; // 0x118
};

void hud_icon_system_init(); // 0x00843E90 (:59)
void hud_icon_level_init_post(); // 0x00843EE0 (:68)
void hud_icon_level_shutdown(); // 0x0087BBE0 (:78)
void hud_icon_render(); // 0x0087BCB0 (:93)
void hud_icon_add(unsigned int obj, int bitmap_id, float scale, const color& icon_color, bool display_distance, bool snap_to_edge); // 0x0087C8C0 (:386)
hud_icon* hud_icon_add(unsigned int obj); // 0x00876E40 (:421)
hud_icon* hud_icon_get(unsigned int obj); // 0x008685F0 (:439)
void hud_icon_enable_status_rings(unsigned int obj, bool enabled); // 0x00868660 (:452)
float hud_icon_get_fade_percent(hud_icon* icon); // 0x00843F70 (:469)
void hud_icon_remove(unsigned int obj); // 0x00876E80 (:486)
void hud_icon_remove_all(); // 0x00876EA0 (:494)
bool hud_icon_get_screen_pos(vector& screen_pos, const vector& world_pos, bool sticky, int bitmap_width, int bitmap_height, bool* behind_camera, bool* stickied); // 0x00843FF0 (:504)
int hud_icon_get_bitmap_id(hud_icons icon); // 0x00844880 (:628)
