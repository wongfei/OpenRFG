// Original: volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h
// Implementation: volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.cpp
//
// The match flow shared by the modes: the match state machine (pregame, countdown, playing, round over, match over), the
// round winner and Multi_match_info, the mode callbacks dispatched to the current mode, round begin/end, the equipment
// loadout and the mode info. Destruction is disabled before the match starts. Placement of the match, status and mode
// types inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/vlib/code/time/timestamp.h"

struct hidden_challenge;
class multi_connection;
struct multi_game_data;
class player;
class player_start;
class rfg_mover;
struct sender;
struct weapon_info;

enum multi_game_match_states
{
    MGMS_NONE = -1,
    MGMS_PREGAME = 0,
    MGMS_COUNTDOWN = 1,
    MGMS_GAMEPLAY = 2,
    MGMS_ENDGAME_SYNC = 3,
    MGMS_WAIT_ON_SYNC = 4,
    MGMS_VIEWING_RESULTS = 5,
    NUM_MULTI_GAME_MATCH_STATES = 6,
};

enum multi_game_status
{
    MGS_NONE = -1,
    MGS_WIN = 0,
    MGS_TIE = 1,
    MGS_LOSE = 2,
    NUM_MULTI_GAME_STATUS = 3,
};

// size 0x4
union multi_round_winner
{
    unsigned int player_game_data_handle; // 0x00
    human_teams team; // 0x00
};

// size 0x14
struct multi_game_equipment_loadout
{
    bool replace_starting_weapon; // 0x00
    unsigned int starting_weapon; // 0x04
    unsigned int third_weapon; // 0x08
    unsigned int fourth_weapon; // 0x0C
    int starting_backpack_type; // 0x10
};

// size 0x1A4
struct multi_game_mode_info
{
    char name[32]; // 0x00
    char display_name[32]; // 0x20
    char description[128]; // 0x40
    char classes[4][32]; // 0xC0
    int num_classes; // 0x140
    char info_text[3][32]; // 0x144
};

// size 0x24
struct multi_match_info
{
    multi_game_match_states match_state; // 0x00
    bool setting_up_round; // 0x04
    bool synchronizing_endgame; // 0x05
    int countdown_time_ms; // 0x08
    int round_time_ms; // 0x0C
    unsigned char current_round; // 0x10
    unsigned char num_rounds; // 0x11
    multi_round_winner round_winner; // 0x14
    timestamp client_wait_on_sync_timestamp; // 0x18
    timestamp server_endgame_sync_wait_timestamp; // 0x1C
    bool use_default_time_of_day; // 0x20
    bool playing_match; // 0x21
    unsigned char max_num_players; // 0x22
};

extern multi_match_info Multi_match_info; // 0x01691DA0

void multi_game_mode_system_init(); // 0x0094D1F0 (:1071)
void multi_game_mode_system_process(); // 0x00A00EC0 (:1077)
void multi_game_mode_system_handshake(); // 0x00975BF0 (:1256)
void multi_game_mode_system_render(); // 0x009AB2C0 (:1268)
void multi_game_mode_enter_common(); // 0x009AB2F0 (:1368)
void multi_game_mode_exit_common(); // 0x009EA1B0 (:1373)
void multi_game_mode_level_init_pre_common(); // 0x00975C20 (:1378)
void multi_game_mode_level_init_post_common(); // 0x009AB300 (:1399)
void multi_game_mode_level_init_shutdown_common(); // 0x0094D200 (:1426)
bool multi_game_mode_process_common(); // 0x009FDDD0 (:1459)
void multi_game_mode_handshake_common(); // 0x0094D210 (:1580)
void multi_game_mode_render_common(); // 0x00992670 (:1586)
void multi_game_mode_add_player_common(player* pp); // 0x0094D220 (:1593)
void multi_game_mode_remove_player_common(player* pp); // 0x009BA2A0 (:1641)
void multi_game_mode_player_died_common(player*& victim, player*& killer, damaged_by_types& damage_type, const weapon_info*& w_info); // 0x009EA1C0 (:1680)
void multi_game_mode_player_respawn_common(player* pp, int player_start_index); // 0x009F7CD0 (:2137)
float multi_game_mode_adjust_respawn_weight_common(const player* pp, const player_start* pstart); // 0x0094D250 (:2249)
void multi_game_mode_building_damage_change_common(player* pp, rfg_mover* building, float damage_delta); // 0x00975C80 (:2254)
bool multi_game_mode_player_can_swap_weapons_common(const player* pp); // 0x0094D260 (:2276)
bool multi_game_mode_player_can_attack_common(const player* pp, const weapon_info* w_info, damaged_by_types attack_type); // 0x0094D270 (:2281)
void multi_game_mode_get_default_equipment_common(const player* pp, multi_game_equipment_loadout* loadout); // 0x0094D280 (:2286)
int multi_game_mode_get_time_ms_common(); // 0x00975D50 (:2302)
void multi_game_mode_countdown_begin_common(); // 0x00975D60 (:2307)
void multi_game_mode_round_begin_common_jip(multi_connection* cp, unsigned int game_data_handle); // 0x009DB740 (:2332)
void multi_game_mode_round_begin_common(); // 0x009EB660 (:2405)
void multi_game_mode_round_over_common(multi_round_winner winner); // 0x009FC970 (:2481)
bool multi_game_mode_is_round_over_common(); // 0x009AB460 (:2637)
bool multi_game_mode_get_round_winner_common(multi_round_winner* winner); // 0x009AB4E0 (:2673)
void multi_game_mode_server_migration_pre_common(); // 0x009EB7D0 (:2695)
void multi_game_mode_server_migration_post_common(); // 0x009AB540 (:2712)
void multi_game_mode_server_migration_update_clients_common(); // 0x009EB8B0 (:2733)
void multi_game_mode_hud_init_common(); // 0x00975D70 (:2762)
void multi_game_mode_process_mode_common(unsigned char* packet_data, const sender& from); // 0x009FE2C0 (:2773)
multi_game_status multi_game_mode_get_mode_status(bool for_current_round); // 0x009EB980 (:2872)
bool multi_game_mode_is_pregame(); // 0x0094D2D0 (:2974)
bool multi_game_mode_is_countdown(); // 0x0094D2E0 (:2979)
bool multi_game_mode_is_gameplay(); // 0x0094D2F0 (:2984)
bool multi_game_mode_is_gameplay_or_later(); // 0x0094D300 (:2989)
bool multi_game_mode_allows_class_selection(multi_gameplay_mode mode); // 0x00975DA0 (:2999)
bool multi_game_mode_all_players_ready(); // 0x00992800 (:3018)
bool multi_game_mode_setting_up_round(); // 0x0094D310 (:3066)
bool multi_game_mode_synching_endgame(); // 0x0094D320 (:3073)
bool multi_game_mode_is_viewing_results(); // 0x0094D330 (:3080)
bool multi_game_mode_is_endgame(); // 0x009BA480 (:3087)
bool multi_game_mode_has_teams(); // 0x00975DD0 (:3094)
int multi_game_mode_get_team_size(session_types session_type, human_teams team, unsigned int connecting_data_handle); // 0x00992870 (:3099)
int multi_game_mode_get_team_xp(session_types session_type, human_teams team, unsigned int connecting_data_handle); // 0x009929D0 (:3141)
human_teams multi_game_mode_get_opposite_team(human_teams team); // 0x0094D340 (:3163)
human_teams multi_game_mode_get_jip_disadvantaged_team(session_types session_type, unsigned int connecting_data_handle); // 0x00992AD0 (:3179)
human_teams multi_game_mode_get_disadvantaged_team(session_types session_type, unsigned int connecting_data_handle); // 0x00992C30 (:3240)
human_teams multi_game_mode_get_advantaged_team(session_types session_type); // 0x00992C70 (:3254)
bool multi_game_mode_team_is_full(session_types session_type, human_teams team); // 0x00992CB0 (:3268)
human_teams multi_game_mode_choose_appropriate_team(session_types session_type, human_teams preferred_team, human_teams current_team, multi_gameplay_mode mode, unsigned int connecting_data_handle); // 0x00992CD0 (:3273)
human_teams multi_game_mode_jip_choose_appropriate_team(session_types session_type, human_teams preferred_team, human_teams current_team, multi_gameplay_mode mode, unsigned int connecting_data_handle); // 0x00992D90 (:3321)
bool multi_game_mode_is_last_round(); // 0x009AB5D0 (:3370)
bool multi_game_mode_point_limit_reached(); // 0x00992E50 (:3419)
int multi_game_mode_num_players(); // 0x00975E00 (:3445)
const multi_game_mode_info* multi_game_mode_get_info(multi_gameplay_mode mode); // 0x0094D360 (:3452)
player* multi_game_mode_get_round_winner_player(); // 0x009DB910 (:3458)
human_teams multi_game_mode_get_round_winner_team(); // 0x0094D370 (:3472)
player* multi_game_mode_get_last_man_standing(); // 0x00992FC0 (:3482)
void multi_game_mode_report_stats_for_player(const multi_game_data* data); // 0x00975E10 (:3512)
void multi_game_mode_report_stats(); // 0x00975E50 (:3741)
void multi_game_mode_append_mode_specific_stats(unsigned char* packet_data, unsigned short& packet_size); // 0x0094D380 (:3803)
void multi_game_mode_read_mode_specific_stats(unsigned char* packet_data, int& packet_offset); // 0x0094D390 (:3808)
void multi_game_mode_get_mode_specific_stat_names(char** names, const int num_values, const int max_name_len); // 0x0094D3A0 (:3813)
void multi_game_mode_get_mode_specific_stat_values(char** values, const int num_values, const int max_value_len); // 0x0094D3B0 (:3818)
void multi_game_mode_common_calculate_skill_mismatch(); // 0x0094D3C0 (:3824)
void multi_game_mode_common_set_skill_mismatch(bool mismatch); // 0x0094D3D0 (:3858)
bool multi_game_mode_common_get_skill_mismatch(); // 0x0094D3E0 (:3863)
void multi_game_mode_hidden_challenge_complete(const hidden_challenge* challengep); // 0x0094D3F0 (:3868)
void multi_game_mode_common_report_loss_for_leaving_early(); // 0x00993040 (:3876)
