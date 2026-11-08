// Original: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h
// Implementation: volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.cpp
//
// Wrecking Crew (the destruction minigame): up to four players take turns destroying a map with a given weapon and
// backpack, scored by the mass they destroy, over rounds or challenges. It loads single zones like multiplayer; the world
// asks whether it is running, its state and which DLC bundle it uses, and is told about destroyed movers and building
// damage. The mode, map, weapon and backpack tables come from XML (wrecking_crew_parse_challenges); the HUD reads its
// texts and icon states. Placement of the info, status and challenge types inferred.
#pragma once

#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew_enums.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/interface/option_menus/controller_options.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

struct hud_wrecking_crew_barrel_text_set;
enum hud_wrecking_crew_icon_state;
struct hud_wrecking_crew_text_set;
class human_info;
struct multi_backpack_info;
class object_mover;
class player;
class rfg_mover;
struct weapon_info;
struct xml_element;

// size 0x90
struct wrecking_crew_mode_info
{
    const char* internal_name; // 0x00
    const char* display_name; // 0x04
    void (*match_begin)(); // 0x08
    void (*match_end)(); // 0x0C
    void (*round_begin)(); // 0x10
    int (*round_end)(); // 0x14
    int (*players_turn_begin)(); // 0x18
    int (*players_turn_end)(int); // 0x1C
    bool (*is_players_turn_over)(); // 0x20
    float (*get_current_players_score)(int); // 0x24
    bool (*can_player_attack)(const weapon_info*, damaged_by_types); // 0x28
    void (*player_attacked)(const weapon_info*, damaged_by_types); // 0x2C
    void (*player_activated_backpack)(); // 0x30
    void (*player_respawned)(); // 0x34
    void (*gameplay_process)(); // 0x38
    void (*render)(wrecking_crew_states); // 0x3C
    void (*map_is_being_restored)(); // 0x40
    void (*mover_destroyed)(const object_mover*); // 0x44
    void (*get_weapon_ammo)(int*, int*); // 0x48
    bool skip_round_begin; // 0x4C
    bool display_round_win_message; // 0x4D
    void (*get_player_description)(int, wchar_t*, int); // 0x50
    void (*get_score_text)(hud_wrecking_crew_text_set*, hud_wrecking_crew_text_set*, bool*); // 0x54
    void (*get_weapon_text)(wchar_t*, int, bool*, char*, int); // 0x58
    void (*get_backpack_text)(wchar_t*, int, bool*, char*, int); // 0x5C
    void (*get_turn_end_score_text)(hud_wrecking_crew_text_set*, hud_wrecking_crew_text_set*, bool*); // 0x60
    void (*get_turn_end_letters_text)(hud_wrecking_crew_text_set*); // 0x64
    void (*get_turn_end_message_text)(hud_wrecking_crew_text_set*); // 0x68
    void (*get_barrel_text)(hud_wrecking_crew_barrel_text_set*); // 0x6C
    bool (*get_veto_text)(hud_wrecking_crew_text_set*, bool*, hud_wrecking_crew_text_set*, bool*); // 0x70
    hud_wrecking_crew_icon_state (*get_weapon_icon_state)(); // 0x74
    hud_wrecking_crew_icon_state (*get_backpack_icon_state)(); // 0x78
    hud_wrecking_crew_icon_state (*get_time_icon_state)(); // 0x7C
    tracked_stats_indices games_played_overall_stat; // 0x80
    const char* info_text[3]; // 0x84
};

// size 0x40
struct wrecking_crew_map_info
{
    wrecking_crew_map_info(); // 0x00767280

    const char* file_name; // 0x00
    checksum_stri file_name_crc; // 0x04
    const char* display_name; // 0x08
    const char* img_name; // 0x0C
    const char* load_img_name; // 0x10
    int time_of_day_hour; // 0x14
    int time_of_day_minute; // 0x18
    int challenge_id; // 0x1C
    int dlc_id; // 0x20
    tracked_stats_indices games_played_overall_stat; // 0x24
    tracked_stats_indices games_played_mode_stats[5]; // 0x28
    bool locked; // 0x3C
};

// size 0x10
struct wrecking_crew_weapon_info
{
    unsigned int w_info_handle; // 0x00
    int rampage_time_penalty_ms; // 0x04
    float mayhem_attack_multiplier; // 0x08
    unsigned short mode_flags; // 0x0C
};

// size 0xC
struct wrecking_crew_backpack_info
{
    int backpack_type; // 0x00
    int rampage_time_penalty_ms; // 0x04
    unsigned short mode_flags; // 0x08
};

// size 0x24
struct wrecking_crew_player_status
{
    int player_index; // 0x00
    float turn_score; // 0x04
    float round_score; // 0x08
    float match_score; // 0x0C
    float turn_destruction_done; // 0x10
    float round_destruction_done; // 0x14
    float match_destruction_done; // 0x18
    int round_wins; // 0x1C
    bool had_a_turn_for_current_round; // 0x20
    bool enabled; // 0x21
};

// size 0x18
struct wrecking_crew_challenge_mode_round
{
    wrecking_crew_modes mode; // 0x00
    unsigned int winfo_handle; // 0x04
    int backpack_id; // 0x08
    wrecking_crew_walker_choices walker_choice; // 0x0C
    wrecking_crew_stress_modes stress_mode; // 0x10
    bool barrels; // 0x14
};

// size 0x50
struct wrecking_crew_challenge_mode
{
    wrecking_crew_challenge_mode(); // 0x0075F860

    checksum_stri name_crc; // 0x00
    wrecking_crew_challenge_mode_round rounds[3]; // 0x04
    int leaderboard_id; // 0x4C
};

// wrecking.md WC0 (rfg.pdb): the match options the menus write and the mode, the loading screen and the stress code
// read (Wrecking_crew_options). Placement of this and the next three types inferred: the PDB records no file for data
// types, and the globals that hold them are wrecking_crew.cpp's (address order) and read by other modules.
// size 0x2C
struct wrecking_crew_options
{
    wrecking_crew_modes mode; // 0x00
    int num_players; // 0x04
    int map_index; // 0x08
    int old_map_index; // 0x0C: the map whose menu image is released next (-1: none)
    int weapon_index; // 0x10
    int backpack_index; // 0x14
    wrecking_crew_walker_choices walker_choice; // 0x18
    wrecking_crew_stress_modes stress_mode; // 0x1C
    int rounds_to_win; // 0x20
    bool spawn_barrels; // 0x24
    bool free_play; // 0x25
    int current_challenge_id; // 0x28: -1 outside a challenge
};

// One of the three walkers of walker mode (Wrecking_crew_walkers). vehicle_type is looked up from vehicle_name by
// wrecking_crew_init.
// size 0xC
struct wrecking_crew_walker_info
{
    const char* display_name; // 0x00
    const char* vehicle_name; // 0x04
    int vehicle_type; // 0x08
};

// A selectable character of wrecking_crew.xtbl (Character_Element), with the backpack offsets multi_object_backpack
// reads for it.
// size 0xD0
struct wc_game_character_info
{
    wc_game_character_info(); // 0x00767270

    const human_info* hip; // 0x00
    const char* display_name; // 0x04
    const char* bitmap_name; // 0x08
    human_teams team; // 0x0C
    vector backpack_offsets[16]; // 0x10: magic number 16, one per backpack type
};

// A player's controls and character choice (Wc_player_infos); the controller options page reads and writes it while
// the character select is up or the mode runs.
// size 0xC14
struct wc_player_info
{
    wc_player_info(); // 0x00761030

    controller_options control_options; // 0x00
    int character_index; // 0x58
    control_config ctrl_config; // 0x5C
};

extern wrecking_crew_walker_info Wrecking_crew_walkers[3]; // 0x0165188C
extern wrecking_crew_options Wrecking_crew_options; // 0x016518CC
extern int Wrecking_crew_current_player; // 0x016518F8
extern int Wrecking_crew_current_round; // 0x016518FC
extern int Wrecking_crew_last_first_player; // 0x01651900
extern timestamp_percent Wrecking_crew_timer; // 0x01659F2C
extern farray<wc_game_character_info*, 50> Wrecking_crew_characters; // 0x01659F38
extern bool Wrecking_crew_player_attacked_this_frame; // 0x022296BC
extern bool Wrecking_crew_player_activated_backpack_this_frame; // 0x022296BD
extern wc_player_info Wc_player_infos[4]; // 0x0222AD70
// Defined with the character select document (its address is among that file's globals); the copy taken on enter
// that a cancel restores.
extern wc_player_info Old_wc_player_infos[4]; // 0x02C3D120

void unused_dcf_wc_change_timer(); // 0x0075F880 (:500)
void unused_dcf_wc_lock_map(); // 0x00790F40 (:507)
void unused_dcf_wc_unlock_map(); // 0x007767E0 (:514)
void unused_dcf_wc_give_stat_points(); // 0x0075F8B0 (:521)
Wc_mouse_click_result wrecking_crew_check_mouse_clicks(); // 0x00777080 (:1326)
bool wrecking_crew_behemoth_enabled(); // 0x0075FE90 (:1864)
void wrecking_crew_system_init(); // 0x0079EE20 (:1869)
void wrecking_crew_init(); // 0x007AC290 (:1885)
void wrecking_crew_reset_controls(); // 0x0075FEB0 (:2031)
void wrecking_crew_shutdown(); // 0x0075FFB0 (:2056)
void wrecking_crew_set_thread_ownership(); // 0x00760000 (:2073)
xml_element* wrecking_crew_parse_table(const char* name, int dlc_id); // 0x00760020 (:2081)
void wrecking_crew_close_table(xml_element* root, int dlc_id); // 0x007600A0 (:2103)
void wrecking_crew_match_begin(); // 0x007A69B0 (:2110)
int wrecking_crew_dlc_bundle_id(); // 0x007600B0 (:2155)
void wrecking_crew_level_init_pre(); // 0x007600D0 (:2161)
void wrecking_crew_level_init_post(); // 0x0079EEA0 (:2180)
void wrecking_crew_level_shutdown(); // 0x00777230 (:2233)
void wrecking_crew_process(); // 0x007A6B90 (:2274)
void wrecking_crew_handshake(); // 0x00760140 (:2638)
void wrecking_crew_render(); // 0x00760360 (:2703)
bool wrecking_crew_is_active(); // 0x00760440 (:2745)
wrecking_crew_modes wrecking_crew_get_active_mode(); // 0x00760450 (:2752)
bool wrecking_crew_challenge_in_progress(); // 0x00760470 (:2763)
int wrecking_crew_get_num_players(); // 0x00760490 (:2772)
void wrecking_crew_lock_map(const char* map_name); // 0x00777310 (:2777)
void wrecking_crew_unlock_map(const char* map_name); // 0x007604A0 (:2799)
int wrecking_crew_get_num_maps(); // 0x007604C0 (:2808)
const char* wrecking_crew_get_map_file_name(int map_index); // 0x007604D0 (:2815)
const checksum_stri& wrecking_crew_get_map_file_name_crc(int map_index); // 0x007604F0 (:2821)
const char* wrecking_crew_get_map_display_name(int map_index); // 0x00760520 (:2829)
const char* wrecking_crew_get_map_image_name(int map_index); // 0x00760540 (:2837)
const char* wrecking_crew_get_loading_screen_image_name(int map_index); // 0x00760560 (:2843)
int wrecking_crew_get_map_challenge_id(int map_index); // 0x00760580 (:2849)
bool wrecking_crew_is_map_locked(int map_index); // 0x007605A0 (:2855)
bool wrecking_crew_is_map_locked(const char* map_name); // 0x007605C0 (:2861)
const char* wrecking_crew_get_mode_internal_name(wrecking_crew_modes mode); // 0x007605E0 (:2871)
const char* wrecking_crew_get_mode_display_name(wrecking_crew_modes mode); // 0x00760600 (:2879)
int wrecking_crew_get_num_weapons(); // 0x00760620 (:2887)
int wrecking_crew_get_weapon_index(unsigned int winfo_handle); // 0x00760630 (:2892)
const wrecking_crew_weapon_info* wrecking_crew_get_weapon_info(int weapon_index); // 0x00760660 (:2904)
const wrecking_crew_weapon_info* wrecking_crew_get_current_weapon_info(); // 0x00760680 (:2910)
bool wrecking_crew_is_weapon_available_for_mode(int weapon_index, wrecking_crew_modes mode); // 0x00760690 (:2915)
bool wrecking_crew_can_veto_weapon(); // 0x007606D0 (:2921)
int wrecking_crew_get_num_backpacks(); // 0x00760760 (:2958)
const wrecking_crew_backpack_info* wrecking_crew_get_backpack_info(int backpack_index); // 0x00760770 (:2963)
int wrecking_crew_get_backpack_index(int backpack_id); // 0x00760790 (:2969)
const wrecking_crew_backpack_info* wrecking_crew_get_current_backpack_info(); // 0x007607C0 (:2981)
bool wrecking_crew_is_backpack_available_for_mode(int backpack_index, wrecking_crew_modes mode); // 0x007607D0 (:2986)
bool wrecking_crew_can_veto_backpack(); // 0x00760810 (:2992)
int wrecking_crew_get_walker_vehicle_type(wrecking_crew_walker_choices choice); // 0x00760880 (:3019)
const wrecking_crew_player_status* wrecking_crew_get_player_status(int player_index); // 0x007608A0 (:3027)
void wrecking_crew_enable_player(int player_index, bool enable); // 0x007608C0 (:3036)
void wrecking_crew_get_player_name(int player_index, wchar_t* buffer, int buffer_len); // 0x00777360 (:3042)
void wrecking_crew_get_player_description(int player_index, wchar_t* buffer, int buffer_len); // 0x00777460 (:3075)
void wrecking_crew_get_score_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score); // 0x007776A0 (:3138)
void wrecking_crew_get_weapon_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len); // 0x007608E0 (:3188)
void wrecking_crew_get_backpack_text(wchar_t* buffer, int buffer_len, bool* override_icon, char* icon, int icon_buffer_len); // 0x00760930 (:3205)
bool wrecking_crew_get_info_text(int info_entry, wchar_t* buffer, int buffer_len); // 0x00760980 (:3222)
void wrecking_crew_get_turn_end_text(hud_wrecking_crew_text_set* score_set, hud_wrecking_crew_text_set* high_score_set, bool* green_score, hud_wrecking_crew_text_set* letters_set, hud_wrecking_crew_text_set* message_set); // 0x007777F0 (:3247)
void wrecking_crew_get_barrel_text(hud_wrecking_crew_barrel_text_set* barrel_set); // 0x00760A00 (:3302)
bool wrecking_crew_get_veto_text(hud_wrecking_crew_text_set* weapon_set, bool* can_veto_weapon, hud_wrecking_crew_text_set* backpack_set, bool* can_veto_backpack); // 0x00760A30 (:3318)
hud_wrecking_crew_icon_state wrecking_crew_get_weapon_icon_state(); // 0x00760A80 (:3338)
hud_wrecking_crew_icon_state wrecking_crew_get_backpack_icon_state(); // 0x00760AC0 (:3360)
hud_wrecking_crew_icon_state wrecking_crew_get_time_icon_state(); // 0x00760B00 (:3382)
void wrecking_crew_spawn_players(); // 0x0079F1A0 (:3414)
player* wrecking_crew_get_current_player(); // 0x00760BA0 (:3444)
void wrecking_crew_respawn_player(); // 0x0079F290 (:3454)
int wrecking_crew_get_round_winner(); // 0x00760BC0 (:3497)
int wrecking_crew_get_match_winner(); // 0x00760BD0 (:3502)
float wrecking_crew_get_round_high_score(); // 0x00760BE0 (:3507)
int wrecking_crew_get_turn_time_left(); // 0x00760BF0 (:3512)
wrecking_crew_states wrecking_crew_get_state(); // 0x00760C10 (:3519)
bool wrecking_crew_player_attacked_this_frame(); // 0x00760C20 (:3524)
bool wrecking_crew_player_activated_backpack_this_frame(); // 0x00760C30 (:3529)
bool wrecking_crew_can_player_move(); // 0x00760C40 (:3536)
bool wrecking_crew_can_player_move_camera(); // 0x00760C60 (:3550)
bool wrecking_crew_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type); // 0x00760C90 (:3571)
bool wrecking_crew_can_player_activate_backpack(const multi_backpack_info* b_info); // 0x00760CF0 (:3600)
bool wrecking_crew_should_run_stress(); // 0x00760D10 (:3614)
bool wrecking_crew_has_unlimited_magazine_ammo(); // 0x00760D30 (:3623)
bool wrecking_crew_has_unlimited_reserve_ammo(); // 0x00760D50 (:3632)
void wrecking_crew_set_default_inventory(player* pp); // 0x00777960 (:3643)
void wrecking_crew_register_building_damage(const rfg_mover* mover, float mass_delta); // 0x00760D70 (:3703)
void wrecking_crew_mover_destroyed(const object_mover* mover); // 0x00760E40 (:3745)
void wrecking_crew_register_attack(const weapon_info* w_info, damaged_by_types attack_type, bool detonating_charges); // 0x00760E80 (:3768)
void wrecking_crew_register_backpack_activation(); // 0x00760ED0 (:3794)
void wrecking_crew_player_died(player* pp); // 0x00760F10 (:3815)
bool wrecking_crew_should_restart(); // 0x00760F70 (:3841)
void wrecking_crew_should_restart(bool restart); // 0x00760F80 (:3846)
void wrecking_crew_restart(); // 0x00760F90 (:3853)
bool wrecking_crew_is_map_valid(const char* map_name); // 0x00760FC0 (:3864)
int wrecking_crew_get_num_challenge_modes(); // 0x00760FE0 (:3871)
const wrecking_crew_challenge_mode* wrecking_crew_get_challenge_mode_by_index(int index); // 0x00760FF0 (:3879)
bool wrecking_crew_player_can_veto(); // 0x00761010 (:3887)
void wrecking_crew_force_session_end(); // 0x00761020 (:3895)
