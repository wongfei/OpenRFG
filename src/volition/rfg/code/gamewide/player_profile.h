// Original: volition/rfg/code/gamewide/player_profile.h
// Implementation: volition/rfg/code/gamewide/player_profile.cpp
//
// The player profile: the per-controller profile data (multiplayer experience, class and playlist preferences and their
// history, tool tips, the stats buffer, completion and cheat unlocks), read and written through the save data system. Placement of the profile parts inferred.
#pragma once

// size 0x50
struct multi_game_preferences_history
{
    unsigned int playlist_history[10]; // 0x00
    unsigned int veto_history[10]; // 0x28
};

// size 0x82
struct new_player_profile_data
{
    bool imported_save_data; // 0x00
    bool cheats_unlocked[2][64]; // 0x01
    bool game_completed_dlc; // 0x81
};

// size 0xDC0
struct player_profile_data
{
    void reset(); // 0x007E9D90 (:11)

    int mp_xp; // 0x00
    int mp_bonus_xp; // 0x04
    unsigned int mp_playlist_id; // 0x08
    unsigned int mp_ffa_player_crc; // 0x0C
    unsigned int mp_edf_player_crc; // 0x10
    unsigned int mp_gur_player_crc; // 0x14
    unsigned int mp_badge; // 0x18
    multi_game_preferences_history mp_prefs_history; // 0x1C
    int sledge_unique_id; // 0x6C
    unsigned char tool_tip_info[128]; // 0x70
    bool game_completed; // 0xF0
    bool wc_maps_unlocked; // 0xF1
    bool golden_hammer_unlocked; // 0xF2
    unsigned short stats_buffer_size; // 0xF4
    unsigned char stats_buffer[2250]; // 0xF6
    new_player_profile_data new_data; // 0x9C0
    unsigned char padding[894]; // 0xA42
};

extern player_profile_data Last_profile_data_from_save_data; // 0x02279080 (game_save_generic.cpp in rfg.pdb; player_profile.cpp since UI4)

void loaded_new_profile(); // 0x007EEC30 (:19)
bool player_profile_init(); // 0x007E9DB0 (:28)
player_profile_data* player_profile_get(const char* const calling_system_name, int controller_index); // 0x007C5950 (:35)
int player_profile_save(const char* const calling_system_name, int controller_index); // 0x007D24A0 (:43)
void player_profile_process(); // 0x007C5960 (:53)
bool player_profile_busy(int controller_index); // 0x007C5970 (:57)
bool player_profile_last_save_successful(int controller_index); // 0x007C5980 (:63)
void player_profile_set_thread_ownership(const char* const calling_system_name); // 0x007C5990 (:68)
