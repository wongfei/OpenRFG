// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.cpp
//
// Matchmaking (search and join).
#pragma once


struct multi_game_data;

extern bool Pushed_to_lobby; // 0x02BF2226

int compare_search_region(const void* lhs, const void* rhs); // 0x0088C650 (:1249)
void ui_document_mp_matchmaking_init(); // 0x00914DE0 (:1526)
bool multi_game_matchmaking_can_cancel(); // 0x0088C680 (:1545)
void multi_game_matchmaking_cancel(); // 0x0088C6B0 (:1550)
void ui_document_mp_matchmaking_add_party_player_data(const multi_game_data* pData); // 0x0088C6C0 (:1565)
void ui_document_mp_matchmaking_clear_party_player_data(); // 0x008ADCE0 (:1577)
