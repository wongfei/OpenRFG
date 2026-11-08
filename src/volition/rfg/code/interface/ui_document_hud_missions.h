// Original: volition/rfg/code/interface/ui_document_hud_missions.h
// Implementation: volition/rfg/code/interface/ui_document_hud_missions.cpp
//
// The single player mission and activity HUD (squad health, diversions).
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/vlib/code/util/varray.h"

class vint_data_item;

// size 0x8
struct squad_health_info
{
    squad_health_info(const squad_health_info& shi); // 0x00895960 (:61)
    squad_health_info(); // 0x00895940 (:60)

    unsigned int handle; // 0x00
    float health_pct; // 0x04
};

// size 0x10C
struct diversion_data
{
    // Defaults inferred: farray<diversion_data,8> needs a default constructor.
    diversion_data(bool is_ended = false, int key = 0, vint_data_item* vdi = nullptr, const char* diversion_name = nullptr, const char* diversion_value = nullptr); // 0x00895980 (:93)

    bool ended; // 0x00
    int diversion_key; // 0x04
    vint_data_item* data_item; // 0x08
    char name[128]; // 0x0C
    char value[128]; // 0x8C
};

void hud_ui_missions_complete_objective(); // 0x00895B20 (:297)
void hud_ui_mission_set_building_destroyed(const char* building_name, object_importance_level building_importance, bool show_icon); // 0x00895B80 (:308)
bool hud_ui_activity_interface_active(); // 0x00895ED0 (:454)
void hud_ui_missions_get_default_mission_info(char* mission_title, unsigned int mission_title_size, char* mission_caption, unsigned int mission_caption_size); // 0x00895EF0 (:460)
bool hud_ui_missions_get_mission_screen_active(); // 0x00896E70 (:1818)
void hud_ui_missions_add_objective(const char* caption); // 0x00896F80 (:1920)
const char* hud_ui_missions_get_objective(); // 0x00896FA0 (:1926)
void hud_ui_missions_remove_objective(); // 0x00896FB0 (:1932)
void hud_ui_missions_unload_items(bool assets_only); // 0x008F4C50 (:1938)
void hud_ui_missions_load_items(bool reload_flag); // 0x0090FF40 (:1976)
void hud_ui_missions_enter(); // 0x008B2590 (:1993)
void hud_ui_missions_process(float dtime); // 0x008F4D20 (:2008)
void hud_ui_missions_render(); // 0x008D8C80 (:2095)
void hud_ui_missions_get_doc_handles(farray<int,16>& handles); // 0x008C61D0 (:2107)
bool hud_ui_missions_set_timer(bool is_visible, const wchar_t* display_string); // 0x00896FD0 (:2118)
bool hud_ui_missions_set_hostages(int num_hostages, const unsigned int* handle_list); // 0x00897020 (:2140)
