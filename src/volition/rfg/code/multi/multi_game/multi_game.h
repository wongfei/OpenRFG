// Original: volition/rfg/code/multi/multi_game/multi_game.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game.cpp
//
// The multiplayer game layer: system and level init/shutdown, the per-frame process and render, the reserved network
// handles of level objects (flags, markers, movers, ladders, zones), chat, spectating, the player/network handle maps,
// the multiplayer interface memory (the stream2 allocators of the multiplayer UI) and the game end conditions.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/stream2/code/stream2_allocator.h"

class checksum_stri;
class item;
class matrix;
class mempool_base;
struct mp_transmit_list;
class multi_connection;
struct multi_game_data;
class multi_object_marker;
class multi_session;
class object;
class player;
class rfg_mover;
struct sender;
class vector;
class vehicle;

extern stream2_mempool_allocator Stream_mp_interface_preload_allocator; // 0x01691E28
extern stream2_slot_allocator Stream_mp_interface_slot_allocator; // 0x01691E34

void unused_dcf_remote_console(); // 0x009DBD40 (:199)
bool multi_game_allow_chat_say(); // 0x0094E3C0 (:299)
bool multi_game_allow_chat_teamsay(); // 0x0094E3E0 (:310)
void multi_game_chat_say(const char* text, const multi_game_data* game_data, human_teams team); // 0x00995470 (:324)
void multi_game_send_textchat_message(unsigned int sender_data_handle, char* text, human_teams team); // 0x009BB560 (:358)
void multi_game_process_textchat_packet(unsigned char* packet_data, const sender& from); // 0x009BB7B0 (:407)
void mp_game_input_process_chatmouse(); // 0x0094E400 (:430)
void dcf_say(); // 0x009BB950 (:487)
void dcf_team_say(); // 0x009BBA40 (:516)
// template instance: void add_reserved_handles<obj_zone>(); // 0x009D75E0 (:635)
// template instance: void add_reserved_handles<multi_object_flag>(); // 0x009D7740 (:635)
// template instance: void add_reserved_handles<ladder>(); // 0x009D7690 (:635)
// template instance: void add_reserved_handles<general_mover>(); // 0x009D7480 (:635)
// template instance: void add_reserved_handles<rfg_mover>(); // 0x009D73D0 (:635)
// template instance: void add_reserved_handles<multi_object_marker>(); // 0x009D7530 (:635)
void multi_game_setup_reserved_remote_objects(); // 0x009DBDB0 (:719)
void multi_game_system_init(); // 0x0094E660 (:740)
void multi_game_init(); // 0x009FCE20 (:749)
void multi_game_shutdown(); // 0x009FE480 (:824)
void multi_game_level_init_pre(); // 0x009FD380 (:870)
void multi_game_level_init_post(); // 0x009BBB30 (:897)
void multi_game_object_shutdown(); // 0x0094E670 (:902)
void multi_game_level_shutdown(); // 0x00A01220 (:914)
bool multi_game_should_clear_controls(); // 0x009AB9A0 (:956)
void multi_game_set_spectating(bool spectating); // 0x0094E6B0 (:1026)
bool multi_game_spectating(); // 0x0094E6C0 (:1031)
void multi_game_process(const mp_transmit_list& transmit_list, bool client_update); // 0x009DBDD0 (:1036)
void multi_game_render(); // 0x009BBB50 (:1123)
void multi_game_interface_memory_required(int* cpu_required, int* gpu_required); // 0x0094E6D0 (:1137)
void multi_game_interface_memory_init(mempool_base* pool); // 0x0094E700 (:1153)
void multi_game_interface_memory_shutdown(); // 0x0094E780 (:1169)
bool multi_game_interface_memory_initialized(); // 0x0094E7A0 (:1175)
player* player_from_network_handle(unsigned short net_handle); // 0x009BBB90 (:1180)
void multi_game_send_client_loaded_level(); // 0x009BBBD0 (:1187)
bool multi_game_in_gameplay(); // 0x0094E7B0 (:1210)
bool multi_game_havok_broadphase_defer_delete(object* objp); // 0x00995550 (:1302)
void multi_game_init_remote_game_objects(bool clear_allocators); // 0x009F7FD0 (:1369)
void multi_game_send_server_options(multi_session* session); // 0x009BBCA0 (:1382)
void multi_game_process_server_options(unsigned char* packet_data, const sender& from); // 0x009BBD60 (:1395)
void multi_game_player_damaged_rfg_mover(player* pp, rfg_mover* mover); // 0x00995680 (:1407)
void multi_game_player_damaged_rfg_mover(unsigned int multi_game_data_handle, rfg_mover* mover); // 0x00978CD0 (:1418)
void multi_game_player_repaired_rfg_mover(player* pp, rfg_mover* mover); // 0x009956C0 (:1433)
void multi_game_player_repaired_rfg_mover(unsigned int multi_game_data_handle, rfg_mover* mover); // 0x00978D00 (:1444)
unsigned int multi_game_time_since_player_damaged_rfg_mover(player* pp, const rfg_mover* mover); // 0x00995700 (:1462)
unsigned int multi_game_time_since_player_damaged_rfg_mover(unsigned int multi_game_data_handle, const rfg_mover* mover); // 0x00978D30 (:1473)
unsigned int multi_game_time_since_player_repaired_rfg_mover(player* pp, const rfg_mover* mover); // 0x00995740 (:1490)
unsigned int multi_game_time_since_player_repaired_rfg_mover(unsigned int multi_game_data_handle, const rfg_mover* mover); // 0x00978D70 (:1501)
player* multi_game_last_player_to_damage_rfg_mover(const rfg_mover* mover); // 0x009DBFB0 (:1515)
player* multi_game_last_player_to_repair_rfg_mover(const rfg_mover* mover); // 0x009DC050 (:1537)
int multi_game_get_item_respawn_delay(item* obj_itemp); // 0x0094E7C0 (:1559)
void multi_game_process_player_hit_someone(unsigned char* packet_data, const sender& from); // 0x0094E820 (:1576)
void multi_game_trigger_tooltip(player* pp, const char* tooltip_name); // 0x009DC0F0 (:1604)
void multi_game_trigger_tooltip(player* pp, const checksum_stri& tooltip_name_crc); // 0x009BBDA0 (:1610)
void multi_game_process_tooltip_packet(unsigned char* packet_data, const sender& from); // 0x009BBEE0 (:1637)
void multi_game_player_left(multi_connection* cp); // 0x009EFED0 (:1649)
void multi_game_migration_about_to_start(multi_session_base* sp); // 0x009BBF50 (:1654)
bool multi_game_migration_allow_migration(multi_session_base* sp); // 0x00978DB0 (:1662)
void multi_game_migration_just_occurred(multi_session_base* sp); // 0x009F80D0 (:1671)
void multi_game_migration_failed(multi_session_base* sp); // 0x009BBF80 (:1681)
void multi_game_process_remote_object_free_common(object* obj); // 0x009EFEE0 (:1701)
void multi_game_call_debug_render_frame(); // 0x0094E840 (:1723)
bool multi_game_in_joinable_mode(); // 0x0094E850 (:1741)
void multi_game_respawn_all_players(bool start_of_round); // 0x009DC120 (:1761)
void multi_game_set_player_default_inventory(player* pp); // 0x009BBFD0 (:1792)
int multi_game_get_remote_object_type(const object* obj); // 0x009BC200 (:1937)
bool multi_game_vehicle_is_controlled_by_local_player(const vehicle* vp); // 0x0094E860 (:1967)
bool multi_game_vehicle_is_uncontrolled(const vehicle* vp); // 0x0094E890 (:1975)
void multi_game_call_client_connection_denied_callback(multi_session_join_status reason); // 0x0094E8B0 (:1984)
void multi_game_pre_sim(); // 0x00978DD0 (:1992)
void multi_game_post_sim(); // 0x00978DE0 (:1998)
bool multi_game_player_has_unlimited_ammo(const player* pp); // 0x0094E8C0 (:2004)
bool multi_game_player_has_unlimited_magazine_ammo(const player* pp); // 0x0094E8D0 (:2014)
void multi_game_get_player_starting_position(int& player_start_index, vector& pos, matrix& orient); // 0x00995780 (:2025)
multi_object_marker* multi_game_get_camera_starting_position(); // 0x00995800 (:2043)
unsigned int multi_game_compress_team(human_teams team); // 0x0094E8F0 (:2074)
human_teams multi_game_decompress_team(unsigned int compressed_team); // 0x0094E900 (:2088)
const wchar_t* multi_game_debug_get_char_name_from_connection(const multi_connection* cp); // 0x00978E10 (:2095)
const char* multi_game_debug_get_char_name_from_connection_UTF8(const multi_connection* cp); // 0x00978E70 (:2110)
bool multi_game_is_booted_to_main_menu(); // 0x0094E910 (:2131)
void multi_game_process_remote_console_command(unsigned char* packet_data, const sender& from); // 0x0094E920 (:2137)
void multi_game_send_game_lobby_console_command(const char* console_string); // 0x009BC270 (:2147)
human_teams multi_game_get_my_team_music(); // 0x009AB9E0 (:2160)
bool multi_game_allow_sim_event_process(); // 0x00978EF0 (:2180)
void multi_game_setup_game_callbacks(); // 0x00978F30 (:2194)
void multi_game_jip_request(unsigned char* packet_datap, const sender& from); // 0x009DC280 (:2201)
void multi_game_jip_client_joined(unsigned char* packet_data, const sender& from); // 0x009958E0 (:2297)
