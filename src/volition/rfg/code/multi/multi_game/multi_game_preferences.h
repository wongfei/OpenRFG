// Original: volition/rfg/code/multi/multi_game/multi_game_preferences.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_preferences.cpp
//
// Matchmaking preferences: the player's playlist history (played and vetoed entries) and the preferred queries.
#pragma once


class game_match;
class game_query;
class multi_connection;
struct multi_game_playlist_entry;
struct sender;

void multi_game_prefereces_system_init(); // 0x00983920 (:127)
void multi_game_preferences_read_from_profile(); // 0x009601E0 (:140)
void multi_game_preferences_write_to_profile(); // 0x00960280 (:153)
void multi_game_prefereces_system_shutdown(); // 0x00960320 (:167)
void multi_game_preferences_create_query_list(game_query* dest_queries, int* num_dest_queries, const int max_dest_queries); // 0x009839A0 (:172)
void multi_game_preferences_create_any_playlist_query_list(game_query* dest_queries, unsigned int* dest_max_party_sizes, int* num_dest_queries, const int max_dest_queries); // 0x00983B40 (:223)
void multi_game_preferences_get_random_query(game_query* pResultQuery, unsigned int maxPartySize); // 0x00983CC0 (:262)
int multi_game_preferences_calculate_score(const game_match* game); // 0x00983DE0 (:301)
void multi_game_preferences_set_ideal_query(const game_query* new_query); // 0x00960330 (:315)
void multi_game_preferences_set_current_query(const game_query* new_query); // 0x00960350 (:320)
const game_query* multi_game_preferences_get_ideal_query(); // 0x00960370 (:325)
const game_query* multi_game_preferences_get_current_query(); // 0x00960380 (:330)
void multi_game_preferences_mark_played_entry(unsigned int playlist_id); // 0x00960390 (:335)
void multi_game_preferences_mark_vetoed_entry(unsigned int playlist_id); // 0x00960480 (:347)
void multi_game_preferences_reset_total_history(); // 0x00960570 (:359)
void multi_game_preferences_reset_player_history(int idx); // 0x009605C0 (:371)
void multi_game_preferences_apply_my_history(int idx); // 0x00960670 (:381)
void multi_game_preferences_clear_players_history(unsigned int player_idx); // 0x00960760 (:391)
void multi_game_preferences_read_players_history(unsigned char* packet_data, const sender& from); // 0x00960810 (:401)
void multi_game_preferences_send_my_history(multi_connection* cp); // 0x009D2320 (:419)
const multi_game_playlist_entry* multi_game_preferences_choose_random_preferred_playlist_entry(const multi_game_playlist_entry* veto); // 0x00960EC0 (:436)
