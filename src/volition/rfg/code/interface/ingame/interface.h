// Original: volition/rfg/code/interface/ingame/interface.h
// Implementation: volition/rfg/code/interface/ingame/interface.cpp
//
// The in-game interface modes (pause, overlays, message boxes) and their mode functions; interface_process/render and
// the pause push.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

enum interface_modes
{
    INT_MODE_NONE = -1,
    INT_MODE_NORMAL = 0,
    INT_MODE_NO_CONTROL = 1,
    INT_MODE_MULTIPLAYER_REMOTE_CLIENT = 2,
    INT_MODE_PDA = 3,
    INT_MODE_PRE_RACING_IN_VEHICLE = 4,
    INT_MODE_RACING_WO_COMBAT_IN_VEHICLE = 5,
    INT_MODE_RACING_WO_COMBAT_STANDING = 6,
    INT_MODE_MULTI_PRE_RACING = 7,
    INT_MODE_MULTI_RACING = 8,
    INT_MODE_MULTI_RACING_GUNNER = 9,
    NUM_INT_MODES = 10,
};

// size 0x52C
struct int_mbox
{
    int_mbox(); // 0x008845D0

    char text[256]; // 0x00
    int x; // 0x100
    int y; // 0x104
    int w; // 0x108
    int h; // 0x10C
    int num_options; // 0x110
    char options[32][32]; // 0x114
    int options_x; // 0x514
    int options_offset; // 0x518
    int selected_option; // 0x51C
    int flags; // 0x520
    void (*callback)(int); // 0x524
    timestamp_realtime anim_time; // 0x528
};

// size 0x10
struct int_mode_funcs
{
    void (*enter)(); // 0x00
    void (*exit)(); // 0x04
    void (*process)(int); // 0x08
    void (*render)(); // 0x0C
};

extern interface_modes Interface_mode; // 0x01662E6C

void unused_dcf_letterbox(); // 0x008A6920 (:349)
void letterbox_init(); // 0x008A6B60 (:738)
void letterbox_process(); // 0x008A6BB0 (:758)
void letterbox_render(); // 0x008A6D20 (:802)
void interface_sound_init(); // 0x00884740 (:832)
void interface_system_init(); // 0x008A6DD0 (:851)
void interface_debug_mode_activate(); // 0x00884760 (:858)
void interface_debug_mode_deactivate(); // 0x00884770 (:863)
bool interface_debug_mode_active(); // 0x00884780 (:868)
bool interface_in_direct_travel_mode(); // 0x00884790 (:878)
void interface_set_mode(interface_modes mode); // 0x008847B0 (:895)
void interface_clear_modes(); // 0x008847D0 (:916)
interface_modes interface_get_pending_mode(); // 0x00884860 (:941)
interface_modes interface_get_mode(); // 0x00884870 (:946)
void interface_push_mode(interface_modes mode); // 0x00884880 (:951)
void interface_pop_mode(); // 0x008848A0 (:973)
int interface_find_mode(interface_modes mode); // 0x008848D0 (:993)
void interface_draw_horz_line_thin(int x, int y, int w, float alpha_mod); // 0x008A6E50 (:1025)
void interface_draw_horz_line_thick(int x, int y, int w, float alpha_mod); // 0x008A77D0 (:1164)
int interface_open_mbox_tight_loop(const char* text, int max_width, int num_options, const char** options, int flags, bool use_front); // 0x008BEAB0 (:1468)
void interface_open_mbox(const char* text, int max_width, int num_options, const char** options, void (*callback)(int), int flags); // 0x008A82B0 (:1620)
void interface_close_mbox(); // 0x00884900 (:1727)
void interface_render_mbox(); // 0x008BEE30 (:1738)
bool interface_is_mbox_open(); // 0x00884930 (:1747)
void interface_process(); // 0x008DD9D0 (:1752)
void interface_nongame_process(); // 0x008A8630 (:1871)
void interface_render(); // 0x008BEE40 (:1899)
void interface_level_init_post(); // 0x008A86A0 (:1934)
void interface_level_shutdown(); // 0x00884950 (:1943)
void interface_mark_mouse(int controller_id, int joy_id); // 0x008A86C0 (:1964)
void interface_process_mouse(int controller_id, int joy_id); // 0x008A8760 (:1982)
extern int Snd_scroll; // 0x01662E60
void interface_replace_special_font(char* str); // 0x00884960 (:2011)
void interface_letterbox_fade_in(bool immediate); // 0x00884AA0 (:2047)
void interface_letterbox_fade_out(bool immediate); // 0x00884AF0 (:2063)
void interface_letterbox_reset(); // 0x00884B40 (:2077)

// No line info in the PDB.
void interface_check_pending_mode(); // 0x008845F0 (:469)
void interface_process_mbox(); // 0x008A6970 (:552)
