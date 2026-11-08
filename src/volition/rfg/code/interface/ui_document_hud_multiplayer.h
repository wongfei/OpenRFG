// Original: volition/rfg/code/interface/ui_document_hud_multiplayer.h
// Implementation: volition/rfg/code/interface/ui_document_hud_multiplayer.cpp
//
// The multiplayer HUD documents (Multi_doc_info per mode: rfg_ui_multiplayer_hud, rfg_ui_mp_hud_xp,
// rfg_ui_mp_hint_bar, rfg_ui_mp_scoreboard, siege, wrecking crew) and their events (kills with kill phrases, joins,
// backpacks, XP, stamps, objectives, the engineer HUD). Placement of the types inferred.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"

class color;

// UI.md UI5: a shared static of rfg.exe's interface unity (the multiplayer menus' message box), external here.
extern int Mp_msgbox_handle; // 0x016632E0
class object;
class player;
class vint_data_item;
struct weapon_info;

enum hud_multiplayer_docs
{
    HMD_NONE = -1,
    HMD_HINTS = 0,
    HMD_MAIN = 1,
    HMD_SIEGE = 2,
    HMD_KING_OF_THE_HILL = 3,
    HMD_XP = 4,
    HMD_SCOREBOARD = 5,
    HMD_INFO = 6,
    NUM_HUD_MULTIPLAYER_DOCS = 7,
};

// size 0x18
struct kill_phrase_token
{
    kill_phrase_token(const wchar_t* name_in); // 0x008970B0 (:150)

    const wchar_t* name; // 0x00
    int name_len; // 0x04
    const wchar_t* colored_replacement; // 0x08
    int colored_replacement_len; // 0x0C
    const wchar_t* replacement; // 0x10
    int replacement_len; // 0x14
};

// size 0x20
struct multi_game_engineer_hud
{
    const wchar_t* title; // 0x00
    bool flash_first_line; // 0x04
    const wchar_t* text[3]; // 0x08
    float text_alpha[3]; // 0x14
};

void unused_dcf_show_kill_message(); // 0x008970E0 (:480)
void hud_ui_multiplayer_enter(); // 0x00916D80 (:1762)
void hud_ui_multiplayer_exit(); // 0x00904BA0 (:1786)
void hud_ui_multiplayer_load_items(bool assets_only); // 0x00910A60 (:1792)
void hud_ui_multiplayer_unload_items(bool assets_only); // 0x008F4F70 (:1823)
void hud_ui_multiplayer_process(float dtime); // 0x008F50B0 (:1874)
void hud_ui_multiplayer_render(); // 0x008D8CE0 (:2030)
int hud_ui_multiplayer_get_doc_handle(hud_multiplayer_docs doc); // 0x008980A0 (:2053)
void hud_ui_multiplayer_show_stamp(const char* message); // 0x008980C0 (:2060)
void hud_ui_multiplayer_hide_stamp(); // 0x00898140 (:2072)
void hud_ui_multiplayer_trigger_objective(const wchar_t* title, const wchar_t* label, human_teams team); // 0x008B2AC0 (:2081)
void hud_ui_multiplayer_trigger_objective(const wchar_t* title, const wchar_t* label, const object* obj); // 0x008B2AF0 (:2088)
void hud_ui_multiplayer_trigger_objective(const wchar_t* title, const wchar_t* label, human_teams team, const color& col); // 0x00898180 (:2097)
void hud_ui_multiplayer_trigger_engineer(const multi_game_engineer_hud* hud_info); // 0x00898350 (:2120)
void hud_ui_multiplayer_hide_engineer(); // 0x00898500 (:2150)
void hud_ui_multiplayer_scoreboard_enter(bool sticky); // 0x008EA960 (:2161)
void hud_ui_multiplayer_scoreboard_exit(); // 0x008C6210 (:2198)
bool hud_ui_multiplayer_scoreboard_is_active(); // 0x00898540 (:2221)
void hud_ui_multiplayer_info_text_enter(); // 0x008EAAA0 (:2232)
void hud_ui_multiplayer_info_text_exit(); // 0x008C6290 (:2257)
bool hud_ui_multiplayer_info_text_is_active(); // 0x00898550 (:2269)
bool hud_ui_multiplayer_splash_screen_is_active(); // 0x008D8D30 (:2278)
void hud_ui_multiplayer_player_removed(player* pp); // 0x00898560 (:2284)
void hud_ui_multiplayer_player_died(player* victim, player* killer, damaged_by_types damage_type, const weapon_info* w_info); // 0x008EAB00 (:2289)
void hud_ui_multiplayer_add_player_died_message(const wchar_t* colored_buffer, const wchar_t* shadow_buffer, bool outline); // 0x008C62D0 (:2577)
void hud_ui_multiplayer_local_player_picked_up_backpack(); // 0x008EB530 (:2599)
void hud_ui_multiplayer_local_player_dropped_backpack(); // 0x00898570 (:2616)
void hud_ui_multiplayer_local_player_triggered_xp_event(const char* event_name, int xp_amount, bool bonus_event, int total_xp, int total_bonus_xp, bool leveled_up); // 0x00898580 (:2621)
void hud_ui_multiplayer_add_backpack_data_item(vint_data_item* item); // 0x008986E0 (:2643)
void hud_ui_multiplayer_process_backpack_data_item(vint_data_item* item); // 0x008D8D80 (:2665)
void hud_ui_multiplayer_player_joined(unsigned int game_data); // 0x008EB570 (:2712)
