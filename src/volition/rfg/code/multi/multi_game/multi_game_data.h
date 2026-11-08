// Original: volition/rfg/code/multi/multi_game/multi_game_data.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_data.cpp
//
// The multiplayer player data: per player network handles, team, name, class and weapon choices, badge, level, lag and
// stats, kept on the server and sent to the clients (multi_game_data), and the destruction migration data. Placement of
// destruction_migration_data inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/time/timestamp.h"

class player;

// size 0x8
struct destruction_migration_data
{
    unsigned short last_applied_frame; // 0x00
    unsigned short last_sent_frame; // 0x02
    bool applied_destruction; // 0x04
    bool destroyed_general_movers_recvd; // 0x05
    bool last_destruction_frame_recvd; // 0x06
};

// size 0x188
struct multi_game_data
{
    multi_game_data(); // 0x0088C3E0

    multi_player_uid player_uid; // 0x00
    multi_player_uid leader_uid; // 0x0B
    int locale; // 0x18
    int language; // 0x1C
    unsigned char player_level; // 0x20
    human_teams team; // 0x24
    bool isMuted; // 0x28
    unsigned int player_selection_preference; // 0x2C
    unsigned int player_badge; // 0x30
    unsigned short backpack_mask; // 0x34
    unsigned int dlc_mask; // 0x38
    wchar_t display_name[64]; // 0x3C
    wchar_t display_name_uncompressed[64]; // 0xBC
    char desired_class[32]; // 0x13C
    int desired_hammer_unique_id; // 0x15C
    unsigned int desired_weapon_inv_item_info_handle; // 0x160
    destruction_migration_data dm_data; // 0x164
    unsigned short player_network_handle; // 0x16C
    bool reported_stats; // 0x16E
    bool bagman_dlc_unlocked; // 0x16F
    bool completed_single_player; // 0x170
    timestamp bad_lag_time; // 0x174
    bool in_use; // 0x178
    bool in_game; // 0x179
    bool timed_out; // 0x17A
    timestamp linger_time; // 0x17C
    multi_connection_disconnect_reason disconnect_reason; // 0x180
    unsigned int handle; // 0x184
};

void unused_dcf_mp_list_players(); // 0x00997640 (:88)
void unused_dcf_mp_long_name_local(); // 0x0097AF30 (:101)
void unused_dcf_mp_long_name_all(); // 0x00997710 (:112)
void multi_game_data_system_init(); // 0x00997AF0 (:176)
unsigned int multi_game_data_register_server_player(session_types session_type, const multi_player_uid& player_uid); // 0x009AEC70 (:190)
unsigned int multi_game_data_register_server_player(session_types session_type, unsigned int clone_from_handle); // 0x0097AFA0 (:243)
unsigned int multi_game_data_register_client_player(unsigned char* packet_data, int& packet_offset, session_types session_type); // 0x00997B40 (:292)
unsigned int multi_game_data_update_player_data(unsigned char* packet_data, int& packet_offset, bool light_update); // 0x009519E0 (:413)
void multi_game_data_clear_players(session_types session_type); // 0x0097B150 (:438)
void multi_game_data_append_my_data(unsigned char* packet_data, unsigned short& packet_size, session_types session_type); // 0x00997FC0 (:475)
void multi_game_data_append_player_data(unsigned char* packet_data, unsigned short& packet_size, unsigned int game_data_handle, session_types as_session_type, bool light_update); // 0x00951F40 (:581)
void multi_game_data_set_player_instance_network_handle(unsigned int game_data_handle, unsigned short net_handle); // 0x00951FC0 (:601)
void multi_game_data_unset_player_instance_network_handle(unsigned int game_data_handle); // 0x00951FF0 (:617)
unsigned short multi_game_data_get_player_instance_network_handle(unsigned int game_data_handle); // 0x00952020 (:632)
void multi_game_data_set_player_team(unsigned int game_data_handle, human_teams team); // 0x0097B290 (:645)
void multi_game_data_set_player_name(unsigned int game_data_handle, const wchar_t* name); // 0x00952060 (:666)
void multi_game_data_set_desired_class(unsigned int game_data_handle, const char* class_name); // 0x009520D0 (:681)
void multi_game_data_set_desired_weapon(unsigned int game_data_handle, unsigned int desired_weapon_inv_item_info_handle); // 0x00952120 (:693)
void multi_game_data_set_desired_hammer(unsigned int game_data_handle, int desired_hammer_unique_id); // 0x00952150 (:704)
void multi_game_data_set_player_select_bundle(unsigned int game_data_handle, unsigned int bundle); // 0x00952180 (:715)
void multi_game_data_set_badge(unsigned int game_data_handle, const unsigned int& badge); // 0x009521B0 (:726)
void multi_game_data_set_isMuted(unsigned int game_data_handle, bool isMuted); // 0x009521E0 (:737)
void multi_game_data_set_player_level(unsigned int game_data_handle, unsigned char level); // 0x00998460 (:748)
void multi_game_data_set_lag_timestamp(unsigned int game_data_handle); // 0x00952210 (:770)
int multi_game_data_lag_timestamp_time_since(unsigned int game_data_handle); // 0x00952240 (:781)
void multi_game_data_clear_lag_timestamp(unsigned int game_data_handle); // 0x00952270 (:791)
unsigned int multi_game_data_get_player_select_preferences(unsigned int game_data_handle); // 0x009522A0 (:802)
const multi_game_data* multi_game_data_get(unsigned int game_data_handle, bool include_in_game, bool include_lingering); // 0x009522D0 (:813)
const multi_game_data* multi_game_data_get_my_data(session_types session_type, bool include_in_game, bool include_lingering); // 0x0097B2E0 (:837)
const multi_game_data* multi_game_data_get_by_player_uid(session_types session_type, const multi_player_uid& player_uid); // 0x00998580 (:851)
void multi_game_data_set_my_handle(unsigned int game_data_handle); // 0x00952340 (:868)
unsigned int multi_game_data_get_my_handle(session_types session_type); // 0x00952360 (:875)
bool multi_game_data_is_my_handle(unsigned int game_data_handle); // 0x00952370 (:880)
const wchar_t* multi_game_data_get_my_name(); // 0x00998660 (:889)
const wchar_t* multi_game_data_get_my_name_uncompressed(); // 0x0097B310 (:900)
void multi_game_data_remove_player(unsigned int game_data_handle); // 0x00952390 (:936)
void multi_game_data_clone_set(session_types from, session_types to); // 0x009523E0 (:950)
void multi_game_data_clone_handle(unsigned int game_data_handle, session_types from, session_types to); // 0x00952460 (:960)
int multi_game_data_count_players(session_types session_type); // 0x009524C0 (:970)
void multi_game_data_set_leader(unsigned int game_data_handle, const multi_player_uid& leader); // 0x00952510 (:983)
const multi_player_uid* multi_game_data_get_leader(unsigned int game_data_handle); // 0x00952550 (:994)
const multi_player_uid* multi_game_data_get_player_uid(unsigned int game_data_handle); // 0x00952580 (:1003)
int multi_game_data_get_player_index(unsigned int game_data_handle); // 0x009525B0 (:1015)
int multi_game_data_get_my_index(session_types session_type); // 0x009525C0 (:1024)
player* multi_game_data_get_player_instance(unsigned int game_data_handle); // 0x009BDDD0 (:1031)
player* multi_game_data_get_player_instance(int player_index); // 0x009BDE40 (:1049)
bool multi_game_data_is_connected(unsigned int game_data_handle); // 0x00998690 (:1060)
void multi_game_data_set_reported_stats(unsigned int game_data_handle); // 0x009525E0 (:1084)
void multi_game_data_set_disconnect_reason(unsigned int game_data_handle, multi_connection_disconnect_reason reason); // 0x00952610 (:1094)
void multi_game_data_set_linger_time(unsigned int game_data_handle, int linger_time_ms); // 0x00952640 (:1104)
void multi_game_data_clear_linger_time(unsigned int game_data_handle); // 0x00952680 (:1114)
void multi_game_data_reset_in_game(session_types session_type); // 0x009526B0 (:1124)
void multi_game_data_set_in_game(unsigned int game_data_handle); // 0x009526E0 (:1133)
void multi_game_data_clear_migration_data(unsigned int game_data_handle); // 0x00952710 (:1143)
unsigned short multi_game_data_get_last_applied_frame(unsigned int game_data_handle); // 0x00952750 (:1159)
unsigned short multi_game_data_get_last_sent_frame(unsigned int game_data_handle); // 0x00952780 (:1169)
bool multi_game_data_get_applied_destruction(unsigned int game_data_handle); // 0x009527B0 (:1178)
bool multi_game_data_get_general_mover_destroyed_received(unsigned int game_data_handle); // 0x009527E0 (:1187)
bool multi_game_data_get_last_destruction_frame_received(unsigned int game_data_handle); // 0x00952810 (:1196)
void multi_game_data_set_last_applied_frame(unsigned int game_data_handle, unsigned short last_applied_frame); // 0x00952840 (:1205)
void multi_game_data_set_last_sent_frame(unsigned int game_data_handle, unsigned short last_sent_frame); // 0x00952870 (:1215)
void multi_game_data_set_applied_destruction(unsigned int game_data_handle, bool applied_destruction); // 0x009528A0 (:1224)
void multi_game_data_set_general_mover_destroyed_received(unsigned int game_data_handle); // 0x009528D0 (:1233)
unsigned int multi_game_data_handles_begin(session_types session_type, bool include_in_game, bool include_lingering); // 0x0097B3D0 (:1243)
unsigned int multi_game_data_handles_next(unsigned int player_handle, bool include_in_game, bool include_lingering); // 0x0097B460 (:1256)
unsigned int multi_game_data_handles_end(session_types session_type); // 0x00952900 (:1272)
const multi_game_data* multi_game_data_begin(session_types session_type, bool include_in_game, bool include_lingering); // 0x0097B500 (:1277)
const multi_game_data* multi_game_data_next(const multi_game_data* data, bool include_in_game, bool include_lingering); // 0x0097B540 (:1287)
const multi_game_data* multi_game_data_end(session_types session_type); // 0x00952910 (:1299)
void multi_game_data_system_shutdown(); // 0x00952920 (:1304)
