// Original: volition/rfg/code/multi/multi_game/multi_game_xp.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_xp.cpp
//
// Experience: experience events, levels and the unlocks per level (weapons, characters, badges). Placement of the xp enums
// inferred.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/time/timestamp.h"

class mempool_base;
struct sender;

enum multi_xp_events
{
    MXE_NONE = -1,
    MXE_KILL = 0,
    MXE_FINISH_MATCH = 1,
    MXE_WIN_MATCH = 2,
    MXE_DEATHMATCH_KILL = 3,
    MXE_DEATHMATCH_KILL_SOLO = 4,
    MXE_PICK_UP_BAG = 5,
    MXE_HOLD_BAG = 6,
    MXE_HOLD_BAG_SOLO = 7,
    MXE_HOLD_BAG_TEAM = 8,
    MXE_KILL_BAGMAN = 9,
    MXE_PROTECT_BAGMAN = 10,
    MXE_BAGMAN_KILL = 11,
    MXE_CAPTURE_FLAG = 12,
    MXE_CAPTURE_FLAG_TEAM = 13,
    MXE_KILL_FLAG_CARRIER = 14,
    MXE_PROTECT_FLAG_CARRIER = 15,
    MXE_PROTECT_FLAG = 16,
    MXE_PICK_UP_FLAG = 17,
    MXE_RETURN_FLAG = 18,
    MXE_DESTROY_KING_OF_THE_HILL_TARGET = 19,
    MXE_DESTROY_ASSIST_KING_OF_THE_HILL_TARGET = 20,
    MXE_REPAIR_KING_OF_THE_HILL_TARGET = 21,
    MXE_REPAIR_ASSIST_KING_OF_THE_HILL_TARGET = 22,
    MXE_HOLD_KING_OF_THE_HILL_TARGET = 23,
    MXE_PROTECT_KING_OF_THE_HILL_TARGET = 24,
    MXE_DESTROY_SIEGE_TARGET = 25,
    MXE_DESTROY_ASSIST_SIEGE_TARGET = 26,
    MXE_DESTROY_SIEGE_TARGET_TEAM = 27,
    MXE_PROTECT_SIEGE_TARGET = 28,
    MXE_DEFEND_SIEGE_TARGET = 29,
    MXE_DEMOLITION = 30,
    MXE_DEMOLITION_TEAM = 31,
    MXE_KILL_ENGINEER = 32,
    MXE_PROTECT_ENGINEER = 33,
    MXE_MULTIPLE_KILLS = 34,
    MXE_MELEE_KILL = 35,
    MXE_BARREL_KILL = 36,
    MXE_DEBRIS_KILL = 37,
    MXE_HEAL_TEAMMATE = 38,
    MXE_KILL_ASSIST = 39,
    MXE_KILL_STREAK = 40,
    MXE_CLIFF_KILL = 41,
    MXE_BUNNY_KILL = 42,
    MXE_KUNG_FU_KILL = 43,
    MXE_HEADSHOT_KILL = 44,
    MXE_STREAK_STOPPER = 45,
    MXE_GHOST_KILL = 46,
    MXE_STICKY_KILL = 47,
    MXE_DIRECT_HIT_KILL = 48,
    MXE_RETALIATION_KILL = 49,
    MXE_X_RAY_KILL = 50,
    MXE_STICKY_SAVE = 51,
    MXE_EASY_KILL = 52,
    MXE_REVENGE_KILL = 53,
    MXE_CHARGE_KILL = 54,
    MXE_SURVIVOR = 55,
    MXE_GROUP_KNOCKDOWN = 56,
    MXE_GOOD_TIMING = 57,
    MXE_ZOMBIE_KILL = 58,
    MXE_BACK_OFF_KILL = 59,
    MXE_FLYING_KILL = 60,
    MXE_RESCUE_KILL = 61,
    MXE_SYNCHRONIZED_KILL = 62,
    MXE_PACK_HUNTER = 63,
    MXE_BBQ = 64,
    MXE_LINED_UP = 65,
    MXE_FULL_CONTROL = 66,
    MXE_WHOS_NUMBER_ONE = 67,
    MXE_BACKWHAT = 68,
    MXE_FLAGMAN = 69,
    MXE_DOUBLEBACK = 70,
    MXE_CAMPER = 71,
    MXE_BLINDSIDE = 72,
    MXE_HAIL_MARY = 73,
    MXE_VERTIGO = 74,
    MXE_SHARPSHOOTER = 75,
    MXE_NO_PROBLEM = 76,
    MXE_SHAKEN_NOT_STIRRED = 77,
    MXE_TASTE_THE_RAINBOW = 78,
    MXE_DOING_IT_WRONG = 79,
    MXE_BACKFIRE = 80,
    MXE_DEATH_FROM_ABOVE = 81,
    MXE_DEATH_FROM_BELOW = 82,
    MXE_POWER_OVERWHELMING = 83,
    NUM_MULTI_XP_EVENTS = 84,
};

enum multi_xp_unlock_types
{
    INVALID_MULTI_XP_UNLOCK_TYPE = -1,
    MXUT_PLAYLIST = 0,
    MXUT_BONUS_XP_EVENT = 1,
    MXUT_WEAPON = 2,
    MXUT_CHARACTER = 3,
    MXUT_BADGE_ICON = 4,
    MXUT_BADGE_EMBLEM = 5,
    MXUT_BADGE_COLOR_SCHEME = 6,
    NUM_MULTI_XP_UNLOCK_TYPES = 7,
};

// size 0xC
struct multi_xp_unlock
{
    // size 0x4
    union unlock_data
    {
        multi_xp_events xp_event; // 0x00
        unsigned int playlist_id; // 0x00
        int weapon_unique_id; // 0x00
        unsigned int character_id; // 0x00
        unsigned int badge_id; // 0x00
    };

    int level_required; // 0x00
    multi_xp_unlock_types unlock_type; // 0x04
    unlock_data data; // 0x08
};

// size 0x74
class multi_xp_event
{
public:
    // The name's checksum; the repeat timers start unset (timestamp()); invalid until mp_xp.xtbl defines it
    // (multi_game_xp_init).
    multi_xp_event(const char* name) : name_crc(name, 0), valid(false) // 0x00942F00 (.h:150)
    {
    }

    checksum_stri name_crc; // 0x00
    const char* display_name; // 0x04
    const char* description; // 0x08
    int value; // 0x0C
    int repeat_delay_ms; // 0x10
    timestamp individual_repeat_delay_timers[16]; // 0x14
    timestamp team_repeat_delay_timers[2]; // 0x54
    bool has_parameter; // 0x5C
    float parameter; // 0x60
    bool bonus; // 0x64
    int minimum_level; // 0x68
    bool player_event; // 0x6C
    bool team_event; // 0x6D
    bool server_authoritative; // 0x6E
    bool bagman_locked; // 0x6F
    bool valid; // 0x70
};

void unused_dcf_mp_alter_xp(); // 0x00987360 (:443)
void multi_game_xp_init(mempool_base& pool); // 0x009A3570 (:475)
void multi_game_xp_shutdown(); // 0x00967540 (:667)
void multi_game_xp_match_init(); // 0x00967550 (:671)
void multi_game_xp_match_shutdown(); // 0x009E29A0 (:685)
float multi_game_xp_get_event_parameter(multi_xp_events event); // 0x009675A0 (:722)
bool multi_game_xp_is_bonus_event(multi_xp_events event); // 0x009675D0 (:734)
bool multi_game_xp_is_team_event(multi_xp_events event); // 0x009675F0 (:744)
const multi_xp_event* multi_game_xp_get_event_info(multi_xp_events event); // 0x00967610 (:754)
int multi_game_xp_get_my_xp(); // 0x00967630 (:766)
int multi_game_xp_get_my_bonus_xp(); // 0x00967640 (:771)
unsigned char multi_game_xp_get_my_level(); // 0x00967650 (:778)
unsigned char multi_game_xp_calculate_level(int xp); // 0x00967660 (:783)
int multi_game_xp_til_next_level(int xp); // 0x00967680 (:796)
int multi_game_xp_level_threshold(int level); // 0x009676B0 (:806)
int multi_game_xp_max_level(); // 0x009676D0 (:812)
const multi_xp_unlock* multi_game_xp_get_all_unlocks(int& num_unlocks); // 0x00987440 (:819)
const multi_xp_unlock* multi_game_xp_get_my_unlocks(int& num_unlocks); // 0x009676E0 (:826)
const multi_xp_unlock* multi_game_xp_get_unlocks_at_level(int& num_unlocks, int level); // 0x00967720 (:845)
const multi_xp_unlock* multi_game_xp_get_unlocks_for_level_range(int& num_unlocks, int level_start, int level_end); // 0x00967770 (:868)
int multi_game_xp_get_sledge_from_profile(); // 0x009677C0 (:889)
void multi_game_xp_trigger_event(human_teams team, multi_xp_events event, int repititions, bool from_server); // 0x009E2B10 (:914)
void multi_game_xp_trigger_event(unsigned int multi_game_data_handle, multi_xp_events event, int repititions, int weapon_id, int backpack_id, bool from_server); // 0x009E2CE0 (:974)
void multi_game_xp_process_event(unsigned char* packet_data, const sender& from); // 0x009E2F20 (:1039)
void multi_game_xp_process_leveled_up(unsigned char* packet_data, const sender& from); // 0x009E3060 (:1110)
int multi_game_xp_get_first_weapon_field(); // 0x00967830 (:1143)
bool multi_game_xp_weapon_is_locked(int sledge_unique_id); // 0x00967890 (:1157)
void multi_game_xp_increment_weapon_field(int& sledge_unique_id); // 0x009678E0 (:1170)
void multi_game_xp_decrement_weapon_field(int& sledge_unique_id); // 0x00967950 (:1191)
int multi_game_xp_get_xp_for_level(int level); // 0x009679C0 (:1212)
