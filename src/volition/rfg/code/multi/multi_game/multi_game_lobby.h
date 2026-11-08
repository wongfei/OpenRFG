// Original: volition/rfg/code/multi/multi_game/multi_game_lobby.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_lobby.cpp
//
// The lobby: creating and joining the lobby session, the lobby state machine (auto launch, voting and vetoes, best
// host), team assignment and pushing the players into the game. Placement of the lobby types inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"

class game_query;
struct multi_game_playlist_entry;
class multi_session_info;
class multi_session_info_base;
struct sender;

namespace kaiko
{
    struct GameLinkLobby;
}

enum multi_game_lobby_state_machine
{
    MGLSM_NONE = 0,
    MGLSM_CLIENT = 1,
    MGLSM_HOST = 2,
    MGLSM_WAITING_FOR_MIN_PLAYERS = 3,
    MGLSM_WAITING_FOR_MARGINALLY_FAIR_TEAMS = 4,
    MGLSM_WAITING_FOR_PLAYERS = 5,
    MGLSM_WAITING_FOR_BALANCED_TEAMS = 6,
    MGLSM_GAME_COUNTDOWN = 7,
    MGLSM_VETOED = 8,
    MGLSM_SELECTING_BEST_HOST = 9,
    MGLSM_COMMUNICATING_BEST_HOST = 10,
    MGLSM_MIGRATING_BEST_HOST = 11,
    MGLSM_STARTING_GAME = 12,
    MGLSM_IN_GAME = 13,
    MGLSM_GAME_OVER = 14,
    NUM_MULTI_GAME_LOBBY_STATE_MACHINES = 15,
};

class multi_session;

extern multi_session* Lobby_session; // 0x02FE6D40, the game lobby's session (placement inferred)

enum client_start_test_results
{
    CST_OK = 0,
    CST_NO_LEVEL = 1,
    CST_NO_STARTING_BACKPACK = 2,
    CST_NO_DEFAULT_BACKPACK = 3,
    CST_NO_MODE = 4,
    CST_NO_PLAYLIST = 5,
    CST_INVALID = 6,
    NUM_CLIENT_TEST_START_RESULTS = 7,
};

// size 0x20
struct team_assignment_player_info
{
    team_assignment_player_info& operator=(const team_assignment_player_info& __that); // 0x0097BA40

    multi_player_uid player_uid; // 0x00
    skill_info skill; // 0x10
};

// size 0x228
struct team_assignment_group
{
    team_assignment_group& operator=(const team_assignment_group& __that); // 0x0097BA80

    multi_player_uid leader_uid; // 0x00
    team_assignment_player_info players[16]; // 0x10
    int num_players; // 0x210
    skill_info aggregate_skill; // 0x218
};

// size 0x18
struct team_info
{
    int num_players; // 0x00
    skill_info skill; // 0x08
};

void unused_dcf_mp_auto_launch_force_next_state(); // 0x0097B8E0 (:231)
void unused_dcf_mp_auto_launch_set_timer(); // 0x0097B900 (:236)
void unused_dcf_mp_lobby_team_assignments_reset(); // 0x009C5F20 (:247)
void multi_game_lobby_init(); // 0x009C6440 (:1824)
void multi_game_lobby_shutdown(); // 0x009FD500 (:1845)
void multi_game_lobby_process(); // 0x00A01350 (:1860)
void multi_game_lobby_fill_session_info(multi_session_info* pInfo, const game_query* pGameQuery, const multi_player_uid& hostId, const kaiko::GameLinkLobby& lobbyId, bool syslink, bool private_lobby, bool dedicated, bool party_lobby); // 0x0097BC70 (:2246)
void multi_game_lobby_create(bool syslink, bool private_lobby, bool dedicated, bool party_lobby); // 0x009FF190 (:2278)
void multi_game_lobby_join(const multi_session_info* session_info, const multi_session_info_base* coordinator_id, bool reservedSlot, bool ignoreGameSeqState); // 0x009FF700 (:2317)
bool multi_game_lobby_pending_join(); // 0x00956A00 (:2482)
void multi_game_lobby_logging_reinited(); // 0x00999740 (:2487)
void multi_game_lobby_server_start_game(); // 0x009F0F10 (:2498)
void multi_game_lobby_client_start_game(); // 0x009F8130 (:2542)
void multi_game_lobby_leave(bool left_by_choice); // 0x009FFDD0 (:2577)
bool multi_game_lobby_left_by_choice(); // 0x00956A10 (:2588)
void multi_game_lobby_reset_leaving_state(); // 0x00956A20 (:2593)
client_start_test_results multi_game_lobby_can_start_game(); // 0x0097BD50 (:2599)
void multi_game_lobby_signal_game_start(); // 0x00A00290 (:2624)
void multi_game_lobby_signal_game_end(); // 0x00A002D0 (:2656)
bool multi_game_lobby_game_starting(); // 0x00956A30 (:2668)
void multi_game_lobby_push_to_game(); // 0x00999780 (:2681)
bool multi_game_lobby_pushed_to_game(); // 0x00956A50 (:2687)
void multi_game_lobby_clear_pushed_to_game(); // 0x00956A60 (:2692)
const multi_session_info* multi_game_lobby_get_game_session_info(); // 0x00956A70 (:2697)
void multi_game_lobby_process_best_host(unsigned char* packet_data, const sender& from); // 0x009C6550 (:2702)
void multi_game_lobby_process_update_state(unsigned char* packet_data, const sender& from); // 0x00A00760 (:2756)
multi_game_lobby_state_machine multi_game_lobby_get_state(); // 0x00956A80 (:2815)
int multi_game_lobby_get_time_left(); // 0x00956A90 (:2820)
bool multi_game_lobby_veto_allowed(); // 0x0097BEE0 (:2829)
bool multi_game_lobby_did_i_veto(); // 0x00956AB0 (:2834)
bool multi_game_lobby_can_i_veto(); // 0x0097BF30 (:2839)
void multi_game_lobby_veto_map(); // 0x009C66D0 (:2849)
void multi_game_lobby_process_veto(unsigned char* packet_data, const sender& from); // 0x00956AC0 (:2873)
int multi_game_lobby_get_vetoes_needed(); // 0x009997B0 (:2887)
int multi_game_lobby_get_veto_count(); // 0x00956B00 (:2898)
const multi_game_playlist_entry* multi_game_lobby_get_veto_entry(); // 0x00956B10 (:2903)
bool multi_game_lobby_waiting_for_user_launch(); // 0x00956B20 (:2909)
bool multi_game_lobby_has_game_settings(); // 0x00956B50 (:2930)
bool multi_game_lobby_has_player_list(); // 0x00956B80 (:2947)
bool multi_game_lobby_allow_team_changing(); // 0x00956B90 (:2967)
bool multi_game_lobby_is_in_lobby(); // 0x00956BD0 (:2994)
bool multi_game_lobby_is_in_party_lobby(); // 0x00956BF0 (:3005)
int multi_game_lobby_get_ready_up_count(); // 0x0097BFA0 (:3016)
int multi_game_lobby_get_ready_ups_needed(); // 0x009997E0 (:3021)
void multi_game_lobby_set_ready(bool readyness); // 0x009C67B0 (:3030)
bool multi_game_lobby_is_ready(); // 0x00956C10 (:3049)
bool multi_game_lobby_ready_allowed(); // 0x00956C20 (:3054)
bool multi_game_lobby_player_is_ready(const multi_player_uid& player_uid); // 0x00999840 (:3059)
bool multi_game_lobby_can_ready_up(); // 0x0097BFC0 (:3068)
void multi_game_lobby_process_ready(unsigned char* packet_data, const sender& from); // 0x009AFB60 (:3082)
int multi_game_lobby_get_team_disparity(); // 0x00956C50 (:3111)
void multi_game_lobby_assign_teams(); // 0x00999890 (:3117)
void multi_game_lobby_jip_accept(unsigned char* packet_data, const sender& from); // 0x00956C60 (:3151)
void multi_game_lobby_send_match_query_to_all(const game_query& matchQuery); // 0x009AFC60 (:3230)
void multi_game_lobby_process_match_query(unsigned char* packet_data, const sender& from); // 0x0097C050 (:3254)
void multi_game_lobby_send_playlist_request_to_host(const game_query& matchQuery); // 0x00956C70 (:3278)
void multi_game_lobby_process_playlist_request(unsigned char* packet_data, const sender& from); // 0x00956C80 (:3307)
void multi_game_lobby_set_actual_game_mode_map_etc(); // 0x009AFD40 (:3361)
void multi_game_lobby_process_invite_type(unsigned char* packet_data, const sender& from); // 0x00956C90 (:3375)
