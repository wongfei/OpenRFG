// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_options.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_options.cpp
//
// The game options of the host (map, mode, time and kill limits, the option tabs).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_game_options_init(); // 0x00914A90 (:3357)

// UI.md UI5: the state callbacks are file statics of rfg.exe's interface unity (multi_player_menu_system_init
// registers them), external here.
void ui_document_mp_game_options_enter(game_state prev_state); // 0x0090C2B0 (:3232)
void ui_document_mp_game_options_exit(game_state new_state); // 0x008F23D0 (:3263)
void ui_document_mp_game_options_process(); // 0x009149C0 (:3280)
void ui_document_mp_game_options_handshake(rl_renderer*); // 0x008F2480 (:3324)
void ui_document_mp_game_options_render(rl_renderer*); // 0x008FBCC0 (:3334)
