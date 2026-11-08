// Original: volition/rfg/code/multi/multi_game/multi_game_character.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_character.cpp
//
// Multiplayer characters: the selectable characters per team, their human info and voice personas, display names and
// bitmaps, the player's selection (stored in the profile) and the character unlocks by experience. Placement of the
// character types inferred.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/varray.h"

class human_info;
class mempool_base;
class voice_persona_list;

enum mp_character_selection_types
{
    MP_SEL_FIRST = 0,
    MP_SEL_FFA = 0,
    MP_SEL_EDF = 1,
    MP_SEL_GUR = 2,
    NUM_MP_CHARACTER_SELECTION_TYPES = 3,
};

// size 0xD4
struct mp_game_character_info
{
    mp_game_character_info(); // 0x0096BC10

    const human_info* hip; // 0x00
    const char* display_name; // 0x04
    const char* bitmap_name; // 0x08
    const voice_persona_list* voice_persona; // 0x0C
    int unlock_level; // 0x10
    vector backpack_offsets[16]; // 0x14
};

// size 0x8
struct multi_xp_character_unlock_info
{
    unsigned int character_id; // 0x00
    int unlock_level; // 0x04
};

// size 0x28: a sledgehammer unlocked by experience (mp_weapon_unlockables.xtbl). Placement inferred.
struct mp_character_unlockable_weapon
{
    int unique_weapon_id; // 0x00
    int level_needed; // 0x04
    char image_name[32]; // 0x08
};

// The unlockable sledgehammers (placement inferred: parse_weapon_unlockables fills it).
extern dynamic_object_pool<mp_character_unlockable_weapon, 1> Mp_unlockable_weapon_pool; // 0x01692B08

// size 0xE0
class mp_character_selection_list
{
public:
    mp_character_selection_list(const char* _team_name, human_teams _team); // 0x009972A0 (:54)
    ~mp_character_selection_list(); // 0x009972D0

    const char* team_name; // 0x00
    human_teams team; // 0x04
    farray<mp_game_character_info*,50> list; // 0x08
};

void unused_dcf_reload_mp_character_table(); // 0x009F0830 (:102)
human_teams get_music_team_from_persona_name(const char* persona_name); // 0x009510D0 (:187)
void parse_weapon_unlockables(mempool_base& pool); // 0x009AE8A0 (:252)
void multi_game_character_init(mempool_base& pool); // 0x009DD2B0 (:360)
void multi_game_character_shutdown(); // 0x009DD320 (:378)
const human_info* multi_game_charcter_get_human_info(human_teams team, unsigned int player_selection_bundle); // 0x0097A7F0 (:383)
const voice_persona_list* multi_game_charcter_get_voice_persona_list(human_teams team, unsigned int player_selection_bundle); // 0x0097A860 (:399)
void multi_game_character_get_base_voice_persona_lists(int& num_voice_persona_lists, const voice_persona_list** vp_lists, int max_vp_lists); // 0x0097A8D0 (:415)
bool multi_game_player_is_locked(mp_character_selection_types selection_type, unsigned int player_selection_bundle); // 0x0097A910 (:425)
const char* multi_game_player_get_display_name(mp_character_selection_types selection_type, unsigned int player_selection_bundle); // 0x0097A970 (:431)
const char* multi_game_player_get_bitmap_name(mp_character_selection_types selection_type, unsigned int player_selection_bundle); // 0x0097A9C0 (:437)
const vector* multi_game_player_get_backpack_offset(human_teams team, unsigned int player_selection_bundle, int backpack_id); // 0x0097AA10 (:443)
void multi_game_character_modify_player_selection(mp_character_selection_types selection_type, unsigned int& player_selection_bundle, bool inc); // 0x0097AAA0 (:450)
unsigned int multi_game_character_get_profile_player_selection(); // 0x0097AB00 (:468)
void multi_game_data_store_profile_player_selection(unsigned int player_selection_bundle); // 0x0097AC00 (:492)
void multi_game_data_store_profile_sledge_selection(int weapon_unique_id); // 0x009510F0 (:511)
const char* multi_game_character_get_weapon_icon(int weapon_unique_id); // 0x00997310 (:519)
const char* multi_game_character_get_weapon_image(int weapon_unique_id); // 0x009973C0 (:531)
const char* multi_game_character_get_weapon_display_name(int weapon_unique_id); // 0x00951120 (:544)
void multi_game_character_get_xp_events(int& xp_event_count, multi_xp_character_unlock_info* event_infos, int max_multi_xp_character_unlock_infos); // 0x0097ACD0 (:552)
const char* multi_game_character_get_unlock_character_name(unsigned int character_id); // 0x0097AD20 (:565)
human_teams multi_game_character_get_my_base_team(); // 0x00997470 (:573)
human_teams multi_game_character_get_base_team(unsigned int player_selection_bundle); // 0x009974D0 (:590)
human_teams multi_game_character_get_base_team(const human_info* h_info); // 0x0097AD40 (:600)
