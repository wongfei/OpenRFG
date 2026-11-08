// Original: volition/rfg/code/interface/ui_document_hud_weapons.h
// Implementation: volition/rfg/code/interface/ui_document_hud_weapons.cpp
//
// The weapon HUD documents: reticles, weapon select, ammo and the help document.
#pragma once

#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/util/varray.h"

class player;

// size 0x24
struct weapon_info_set
{
    weapon_info_set(); // 0x00898C70

    bool m_show_weapons; // 0x00
    bool m_show_reticules; // 0x01
    int m_weapon_slot; // 0x04
    int m_prev_weapon_slot; // 0x08
    int m_update_ammo_slot; // 0x0C
    unsigned int m_last_weapon_info; // 0x10
    unsigned int m_current_weapon_info; // 0x14
    bool m_quick_switch; // 0x18
    vector2 m_weapon_screen_pos; // 0x1C
};

extern weapon_info_set Weapon_set; // 0x02C4CB3C (file static in the binary)

int hud_ui_weapons_determine_object_targeted(const player* const pp); // 0x008B2F50 (:549)
void hud_ui_weapons_create_data_items(); // 0x008DA700 (:1068)
void hud_ui_weapons_set_weapon_select_active(bool active); // 0x008992C0 (:1120)
bool hud_ui_weapons_is_weapon_select_active(); // 0x008992D0 (:1126)
void hud_ui_weapons_set_quick_switch(bool quick_switch); // 0x008992E0 (:1132)
void hud_ui_weapons_set_current_weapon_slot(int slot); // 0x008992F0 (:1138)
void hud_ui_weapons_set_previous_weapon_slot(int slot); // 0x00899300 (:1144)
void hud_ui_weapons_add_weapon_ammo_slot_update(int slot); // 0x00899310 (:1150)
void hud_ui_weapons_enter(); // 0x00899330 (:1157)
void hud_ui_weapons_load_items(bool reload_flag); // 0x00910E10 (:1170)
void hud_ui_weapons_unload_items(bool assets_only); // 0x008F5AB0 (:1190)
void hud_ui_weapons_process(float dtime); // 0x008F5B60 (:1227)
void hud_ui_weapons_render(); // 0x008EC080 (:1293)
void hud_ui_weapons_get_doc_handles(farray<int,16>& handles); // 0x008C6D90 (:1311)
