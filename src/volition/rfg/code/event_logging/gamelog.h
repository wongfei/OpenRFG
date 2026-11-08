// Original: volition/rfg/code/event_logging/gamelog.h
// Implementation: volition/rfg/code/event_logging/gamelog.cpp
//
// The game log: telemetry events sent to a logging server (definitions_events/events.h), and the queries the events are
// filled from (time played, completion, difficulty, crcs of missions, activities, weapons and vehicles, the player's
// loadout).
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"

class activity_base;
class district;
class human;
class item;
struct item_info;
class mission_info;
struct multi_backpack_info;
class multi_connection;
struct multi_game_data;
class multi_object_backpack;
class object_mover;
class player;
struct sender;
class vector;
class vehicle;
class weapon;
struct weapon_info;

enum gamelog_complete_reasons
{
    GLCR_FAILED = 0,
    GLCR_SUCCESS = 1,
    GLCR_PLAYER_DIED = 2,
    GLCR_CANCEL = 3,
    GLCR_OUT_OF_TIME = 4,
    GLCR_VEHICLE_ABANDONED = 5,
    GLCR_VEHICLE_DESTROYED = 6,
    GLCR_ESCAPED = 7,
    GLCR_OUT_OF_RANGE = 8,
    GLCR_OUT_OF_AMMO = 9,
    GLCR_HOSTAGES_KILLED = 10,
    GLCR_BUILDING_DESTROYED = 11,
    GLCR_TARGET_GOAL_NOT_REACHED = 12,
    NUM_GAMELOG_COMPLETE_REASONS = 13,
};

enum gamelog_loadout_change_types
{
    GLLCT_WEAPON_PICKUP = 0,
    GLLCT_AMMO_BOX = 1,
    GLLCT_WEAPONS_CABINET = 2,
    NUM_GAMELOG_LOADOUT_CHANGE_TYPES = 3,
};

// size 0x10
struct gamelog_loadout
{
    unsigned int player_handle; // 0x00
    int item_crc[3]; // 0x04
};

void unused_dcf_gamelog_dump_crc_maps(); // 0x00750AF0 (:250)
void unused_dcf_gamelog_dump_crc_maps_mp(); // 0x0074F3A0 (:889)
void unused_dcf_gamelog_dump_crc_maps_wc(); // 0x0074F550 (:921)
void process_logging_setup_state(); // 0x0074F880 (:1236)
void gamelog_system_init(); // 0x007506B0 (:1379)
void gamelog_system_reinit(); // 0x0074F910 (:1428)
void gamelog_local_reinit(bool initial_settings, void (*initd_cb)()); // 0x0074F9A0 (:1474)
void gamelog_remote_reinit(); // 0x0074FA70 (:1518)
void gamelog_process(); // 0x0074FAA0 (:1538)
int gamelog_get_logging_time(); // 0x0074FB00 (:1555)
char gamelog_get_player_index(const human* hp); // 0x00750710 (:1560)
char gamelog_get_player_index(const multi_game_data* data); // 0x0074FB10 (:1582)
char gamelog_get_player_xp_level(const human* hp); // 0x00750780 (:1597)
char gamelog_get_player_xp_level(const player* pp); // 0x0074FB30 (:1607)
int gamelog_get_time_played(); // 0x0074FB60 (:1621)
char gamelog_get_pct_complete(); // 0x0074FB90 (:1634)
const vector& gamelog_get_human_pos(const human* hp); // 0x0074FBD0 (:1643)
int gamelog_get_weapon_crc(const weapon* w_inst); // 0x007507C0 (:1652)
int gamelog_get_weapon_crc(const weapon_info* w_info); // 0x0074FBF0 (:1661)
int gamelog_get_backpack_crc(const multi_object_backpack* backpack); // 0x007507E0 (:1670)
int gamelog_get_backpack_crc(const multi_backpack_info* backpack_info); // 0x0074FC00 (:1680)
int gamelog_get_item_crc(const item* item_inst); // 0x00750800 (:1689)
int gamelog_get_item_crc(const item_info* i_info); // 0x0074FC10 (:1698)
int gamelog_get_activity_crc(activity_base* activity); // 0x0074FC30 (:1708)
int gamelog_get_mission_crc(mission_info* mission); // 0x0074FC80 (:1736)
int gamelog_get_mover_crc(object_mover* mover); // 0x0074FCA0 (:1746)
int gamelog_get_vehicle_crc(vehicle* obj_vehicle); // 0x0074FCD0 (:1756)
int gamelog_get_district_crc(district* obj_district); // 0x0074FD00 (:1766)
int gamelog_get_locale(); // 0x0074FD30 (:1776)
int gamelog_get_language(); // 0x0074FD40 (:1785)
int gamelog_get_titleid(); // 0x0074FD50 (:1794)
char gamelog_get_difficulty(); // 0x0074FD60 (:1803)
int gamelog_get_player_checksum(); // 0x0074FD90 (:1815)
void gamelog_mark_mission_start_time(); // 0x0074FDA0 (:1820)
int gamelog_get_mission_duration(); // 0x0074FDD0 (:1825)
void gamelog_set_activity_failed_reason(gamelog_complete_reasons reason); // 0x0074FE10 (:1830)
gamelog_complete_reasons gamelog_get_activity_failed_reason(); // 0x0074FE20 (:1835)
void gamelog_mark_activity_start_time(activity_modes mode, bool is_modal); // 0x0074FE30 (:1840)
int gamelog_get_activity_duration(activity_modes mode); // 0x0074FE80 (:1851)
void gamelog_mark_vehicle_start_time(); // 0x0074FED0 (:1860)
int gamelog_get_vehicle_duration(); // 0x0074FF00 (:1865)
bool gamelog_has_logged_vehicle_entry(); // 0x0074FF50 (:1877)
void gamelog_mark_game_start_time(); // 0x0074FF60 (:1882)
int gamelog_get_game_duration(); // 0x0074FF80 (:1887)
void gamelog_store_current_loadout(human* hp); // 0x00750830 (:1893)
void gamelog_maybe_record_loadout_change(human* hp, gamelog_loadout_change_types change_type); // 0x00750840 (:1899)
void gamelog_send_logging_info(multi_connection* cp); // 0x0074FFA0 (:1919)
void gamelog_process_logging_info(unsigned char* packet_data, const sender& from); // 0x00750100 (:1939)
void gamelog_set_end_condition(const char* end_condition); // 0x00750230 (:1968)
int gamelog_get_end_condition_crc(); // 0x00750250 (:1974)
int gamelog_get_player_playing_crc(); // 0x00750260 (:1980)
void gamelog_campaign_start_logged(bool has_been_logged); // 0x007502E0 (:2002)
bool gamelog_has_campaign_start_been_logged(); // 0x007502F0 (:2007)
void gamelog_maybe_log_weapon_equipped(player* pp); // 0x00750300 (:2012)
