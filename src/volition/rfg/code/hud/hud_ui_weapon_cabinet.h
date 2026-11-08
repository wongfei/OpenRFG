// Original: volition/rfg/code/hud/hud_ui_weapon_cabinet.h
// Implementation: volition/rfg/code/hud/hud_ui_weapon_cabinet.cpp
//
// The weapon cabinet screen.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"

struct cfile;
class rl_renderer;
struct weapon_info;

// size 0x8
struct ui_weapon_list_item
{
    bool operator==(const ui_weapon_list_item& wli) const; // 0x0084B430 (:68)

    unsigned int weapon_id; // 0x00
    bool unlocked; // 0x04
};

void unused_dcf_show_weapon_cabinet(); // 0x0084B440 (:122)
ui_weapon_list_item hud_ui_weapon_cabinet_get_weapon(int relative_offset, bool skip_singularity); // 0x0085A0F0 (:164)
void hud_ui_weapon_cabinet_add_inventory_items(); // 0x0087CBC0 (:220)
void weapon_cabinet_get_ammo_box_ammo(const weapon_info* w_info, int& max_ammo, int& user_ammo, int& base_ammo, int& morale_ammo, int& final_ammo); // 0x0085A170 (:248)
void hud_ui_weapon_cabinet_create_weapon_info_data(); // 0x0084B4B0 (:319)
void hud_ui_weapon_cabinet_create_icon_data(); // 0x0084B540 (:345)
void hud_ui_weapon_cabinet_create_inventory_data(); // 0x0084B5E0 (:378)
void hud_ui_weapon_cabinet_create_animation_data(); // 0x0084B670 (:406)
void hud_ui_weapon_cabinet_create_hints_data(); // 0x0084B6D0 (:427)
void hud_ui_weapon_cabinet_create_data_items(); // 0x0084B750 (:451)
void hud_ui_weapon_cabinet_process_input(); // 0x0085A300 (:489)
void hud_ui_weapon_cabinet_process_weapon_info_data(); // 0x0085A850 (:685)
void hud_ui_weapon_cabinet_process_icon_data(); // 0x0085AC80 (:795)
void hud_ui_weapon_cabinet_process_inventory_data(); // 0x0085AD80 (:829)
void hud_ui_weapon_cabinet_process_animation_data(); // 0x0084B870 (:884)
void hud_ui_weapon_cabinet_proccess_hints(); // 0x0084B960 (:904)
void hud_ui_weapon_cabinet_process_info(); // 0x0085AF80 (:941)
void hud_ui_weapon_cabinet_init(); // 0x0087FEC0 (:957)
void hud_ui_weapon_cabinet_enter(game_state prev_state); // 0x0087CC90 (:971)
void hud_ui_weapon_cabinet_exit(game_state new_state); // 0x0085AFA0 (:1037)
void hud_ui_weapon_cabinet_handshake(rl_renderer*); // 0x0084BBD0 (:1161)
void hud_ui_weapon_cabinet_process(); // 0x0085B240 (:1167)
void hud_ui_weapon_cabinet_render(rl_renderer*); // 0x0084BBE0 (:1232)
void hud_ui_weapon_cabinet_level_init_pre(); // 0x0087FF10 (:1242)
void hud_ui_weapon_cabinet_tween_end_cb(); // 0x0084BC10 (:1253)
bool hud_ui_weapon_cabinet_show(bool from_ammo_box); // 0x008696A0 (:1259)
event_result hud_ui_weapon_cabinet_pick_up_cb(const event_data& in_event, notify_info* info); // 0x0087CDE0 (:1290)
void hud_ui_weapon_cabinet_add_weapon(const weapon_info* w_info, bool unlocked); // 0x00877200 (:1303)
bool hud_ui_weapon_cabinet_unlock_weapon(const weapon_info* w_info, bool unlocked); // 0x0085B3B0 (:1325)
void hud_ui_weapon_cabinet_reset(); // 0x0085B420 (:1354)
void hud_ui_weapon_cabinet_save(cfile* save_file); // 0x0085B440 (:1363)
void hud_ui_weapon_cabinet_load(cfile* save_file); // 0x00877260 (:1378)
bool hud_ui_weapon_cabinet_is_weapon_purchased(unsigned int weapon_unique_id); // 0x0085B4A0 (:1400)
