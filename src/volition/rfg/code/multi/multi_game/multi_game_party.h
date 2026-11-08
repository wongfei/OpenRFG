// Original: volition/rfg/code/multi/multi_game/multi_game_party.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_party.cpp
//
// Parties: players who join lobbies together behind a leader (party lobby data, invites, kicks, lock down).
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"

struct connection_address;
class multi_connection;
struct multi_game_playlist;
class multi_player_uid;
struct pending_invite_info;
struct sender;

namespace kaiko
{
    struct GameLinkRemoteStation;
}

extern bool g_joinWithoutReservation; // 0x02FEB700, set by the server browser's join (handleJoinSession)

void multi_game_party_system_init(); // 0x0095F2A0 (:109)
void multi_game_party_system_shutdown(); // 0x0095F2B0 (:116)
void multi_game_party_lobby_data_update_received(const unsigned char* packet_data, unsigned short data_size, const connection_address& sender); // 0x009B1840 (:228)
void multi_game_party_lobby_data_update_request_received(const unsigned char* packet_data, unsigned short data_size, const connection_address& sender); // 0x009CE2A0 (:296)
void multi_game_party_system_process(); // 0x00A04CD0 (:318)
void multi_game_party_enable_party(); // 0x0099FE90 (:449)
void multi_game_party_disable_party(); // 0x0099FEF0 (:472)
bool multi_game_party_join_party(const pending_invite_info* pending_party_invite_info, bool invited_from_game, bool force_destruction); // 0x00A00AB0 (:492)
void multi_game_party_process_kicked(unsigned char* packet_data, const sender& from); // 0x00A00D00 (:598)
void multi_game_party_leave_party(bool left_by_choice); // 0x0099FF70 (:613)
void multi_game_party_reset_last_received_lobby_data_update(); // 0x00982070 (:640)
void multi_game_party_kick(const multi_player_uid& uid); // 0x009CE320 (:650)
bool multi_game_party_am_i_leader(bool party_must_exist); // 0x0095F2C0 (:665)
bool multi_game_party_left_party_by_choice(); // 0x0095F330 (:684)
void multi_game_party_clear_left_party_by_choice(); // 0x0095F340 (:689)
bool multi_game_party_in_party(); // 0x0095F350 (:694)
bool multi_game_party_all_clients_have_playlist(const multi_connection* const cp_list, const multi_game_playlist* const playlist); // 0x0095F3F0 (:727)
unsigned int multi_game_party_get_num_players_in_party(); // 0x0095F490 (:749)
bool multi_game_party_has_player(const multi_player_uid& uid); // 0x0095F4F0 (:767)
bool multi_game_party_get_player_name(const multi_player_uid& uid, wchar_t* out_name, unsigned int name_capacity); // 0x009820B0 (:794)
bool multi_game_party_is_player_leader(const multi_player_uid& uid); // 0x0095F5A0 (:824)
bool multi_game_party_get_leader(multi_player_uid* uid); // 0x0095F640 (:847)
unsigned int multi_game_party_get_players(multi_player_uid* out_uids, unsigned int capacity); // 0x009821B0 (:873)
bool multi_game_party_is_locked_down(); // 0x0095F6E0 (:904)
void multi_game_party_lockdown(); // 0x0095F6F0 (:916)
void multi_game_party_unlock(); // 0x0095F700 (:924)
bool multi_game_party_all_players_joined_lobby(); // 0x00982290 (:935)
bool multi_game_party_is_remote_station_in_my_party(const kaiko::GameLinkRemoteStation* pRemoteStation); // 0x0095F710 (:955)
session_invite_type multi_game_party_next_invite_type(session_invite_type invite_type); // 0x0095F830 (:979)
session_invite_type multi_game_party_prev_invite_type(session_invite_type invite_type); // 0x0095F840 (:988)
