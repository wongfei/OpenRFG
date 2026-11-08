// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_lobby.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_lobby.cpp
//
// The game lobby: the player list, ready states, the host's start and the lobby chat.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_game_lobby_init(); // 0x00914970 (:2023)

// UI.md UI5: the state callbacks are file statics of rfg.exe's interface unity (multi_player_menu_system_init
// registers them), external here.
void ui_document_mp_game_lobby_enter(game_state prev_state); // 0x0090C170 (:1893)
void ui_document_mp_game_lobby_exit(game_state new_state); // 0x008BFF20 (:1929)
void ui_document_mp_game_game_lobby_process(); // 0x009148C0 (:1951)
void ui_document_mp_game_lobby_handshake(rl_renderer*); // 0x0090C290 (:1986)
void ui_document_mp_game_lobby_render(rl_renderer*); // 0x008FBA80 (:1996)

// Test seam (UI.md 2.7 item 2, off by default): asked by the lobby's input each loaded frame; true launches as the
// start button does (the host's command line launch once its options are set and its players are in).
extern bool (*g_openrfgGameLobbyLaunch)();
