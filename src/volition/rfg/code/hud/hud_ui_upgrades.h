// Original: volition/rfg/code/hud/hud_ui_upgrades.h
// Implementation: volition/rfg/code/hud/hud_ui_upgrades.cpp
//
// The upgrades screen.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_message_box.h"

class rl_renderer;

void unused_dcf_show_upgrades(); // 0x0084AF70 (:136)
bool hud_ui_upgrades_msgbox_cb(int mbox_handle, int selection_index, msgbox_choice choice); // 0x0087F5E0 (:153)
void hud_ui_upgrades_entry_set_viewed(int index, int curr_time, int start_time); // 0x00859680 (:219)
void hud_ui_upgrades_process_scroll_bar(); // 0x008596F0 (:234)
void hud_ui_upgrades_process_mouse_input(int& result, int& choice); // 0x008598A0 (:291)
void hud_ui_upgrades_process_input(); // 0x0087F700 (:375)
void hud_ui_upgrades_create_header_data(); // 0x0084AFC0 (:548)
void hud_ui_upgrades_create_button_data(); // 0x0084B060 (:577)
void hud_ui_upgrades_create_hints_data(); // 0x0084B120 (:617)
void hud_ui_upgrades_create_data_items(); // 0x0084B190 (:639)
void hud_ui_upgrades_process_header(); // 0x00859A00 (:647)
void hud_ui_upgrades_process_buttons(); // 0x00859D70 (:708)
void hud_ui_upgades_proccess_hints(); // 0x0084B210 (:760)
void hud_ui_upgrades_process_info(); // 0x00859FD0 (:783)
void hud_ui_upgrades_init(); // 0x00880750 (:797)
void hud_ui_upgrades_enter(game_state prev_state); // 0x0087FBE0 (:812)
void hud_ui_upgrades_exit(game_state new_state); // 0x0084B350 (:872)
void hud_ui_upgrades_handshake(rl_renderer*); // 0x0085A000 (:909)
void hud_ui_upgrades_process(); // 0x0087FD20 (:975)
void hud_ui_upgrades_render(rl_renderer*); // 0x0084B400 (:1063)
void hud_ui_upgrades_update_list(); // 0x0087C950 (:1073)
