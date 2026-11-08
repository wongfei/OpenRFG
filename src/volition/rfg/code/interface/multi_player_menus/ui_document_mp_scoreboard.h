// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.cpp
//
// The scoreboard state (end of match and in-game).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_scoreboard_init(); // 0x00917350 (:1536)
void ui_document_mp_scoreboard_enter(game_state prev_state); // 0x0090E340 (:1553)
void ui_document_mp_scoreboard_exit(game_state new_state); // 0x008F32D0 (:1595)
void ui_document_mp_scoreboard_process(); // 0x009154A0 (:1624)
void ui_document_mp_scoreboard_handshake(rl_renderer*); // 0x008E4DB0 (:1678)
void ui_document_mp_scoreboard_render(rl_renderer*); // 0x008FC820 (:1686)
