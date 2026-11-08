// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.cpp
//
// The matchmaking options.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_change_matchmaking_init(); // 0x00917210 (:1234)
void ui_document_mp_change_matchmaking_enter(game_state prev_state); // 0x0090B370 (:1251)
void ui_document_mp_change_matchmaking_exit(game_state new_state); // 0x008E0550 (:1288)
void ui_document_mp_change_matchmaking_process(); // 0x00914690 (:1302)
void ui_document_mp_change_matchmaking_handshake(rl_renderer*); // 0x008F2040 (:1364)
void ui_document_mp_change_matchmaking_render(rl_renderer*); // 0x008FB3F0 (:1372)
