// Original: volition/rfg/code/hud/hud_ui_district_liberation.h
// Implementation: volition/rfg/code/hud/hud_ui_district_liberation.cpp
//
// The district liberation screen. Placement of liberation_ui_type inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

enum liberation_ui_type
{
    LUT_LIBERATION = 0,
    LUT_MISSION_UNLOCKS = 1,
    LUT_ACTIVITY_UNLOCKS = 2,
    NUM_LIBERATION_UI_TYPES = 3,
};

void unused_dcf_show_district_liberation(); // 0x00868740 (:120)
void start_tally_sound(); // 0x008468A0 (:131)
void stop_tally_sound(); // 0x008468D0 (:142)
int hud_ui_district_liberation_process_mouse_input(); // 0x008468E0 (:148)
void hud_ui_district_liberation_process_input(); // 0x00846910 (:161)
void hud_ui_district_liberation_create_header_data(); // 0x00846A60 (:226)
void hud_ui_district_liberation_create_unlock_data(); // 0x00846AC0 (:247)
void hud_ui_district_liberation_create_control_data(); // 0x00846B40 (:271)
void hud_ui_district_liberation_create_hints_data(); // 0x00846BC0 (:293)
void hud_ui_district_liberation_create_data_items(); // 0x00846C20 (:314)
void hud_ui_district_liberation_process_header(); // 0x00856860 (:323)
void hud_ui_district_liberation_proccess_hints(); // 0x00846CD0 (:374)
void hud_ui_district_liberation_process_unlock_data(); // 0x00846DF0 (:398)
void hud_ui_district_liberation_process_control_data(); // 0x00856AA0 (:442)
void hud_ui_district_liberation_process_info(); // 0x00856D10 (:493)
void hud_ui_district_liberation_init(); // 0x00876FA0 (:508)
void hud_ui_district_liberation_enter(game_state prev_state); // 0x00846FB0 (:523)
void hud_ui_district_liberation_exit(game_state new_state); // 0x008687B0 (:568)
void hud_ui_district_liberation_handshake(rl_renderer*); // 0x00847090 (:602)
void hud_ui_district_liberation_process(); // 0x00856D30 (:642)
void hud_ui_district_liberation_render(rl_renderer*); // 0x00847120 (:729)
void hud_ui_district_liberation_do_frame(); // 0x00847160 (:739)
bool hud_ui_district_liberation_show(const int* update_idx_list, int num_idx, unsigned int district_handle, liberation_ui_type ui_type, int num_activities_completed); // 0x00856F80 (:749)
void hud_ui_district_liberation_level_shutdown(); // 0x008471A0 (:788)
void hud_ui_district_liberation_tween_done(); // 0x008471B0 (:793)
void hud_ui_district_liberation_main_text_done(); // 0x008471C0 (:800)
bool hud_ui_district_liberation_screen_active(); // 0x008471F0 (:810)
