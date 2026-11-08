// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.cpp
//
// The Live (Steam online) menu.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_live_init(); // 0x00917260 (:802)
void ui_document_mp_live_enter(game_state prev_state); // 0x0090C3B0 (:819)
void ui_document_mp_live_exit(game_state new_state); // 0x008F26A0 (:880)
void ui_document_mp_live_process(); // 0x00914AE0 (:899)
void ui_document_mp_live_handshake(rl_renderer*); // 0x008F2750 (:942)
void ui_document_mp_live_render(rl_renderer*); // 0x008FBD10 (:950)
void ui_document_mp_live_ui_push(game_state new_state); // 0x0088C1C0 (:967)
