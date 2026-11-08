// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.cpp
//
// The Live (Steam) server browser.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void lfs_ui_push(game_state new_state); // 0x0088C380 (:686)
void ui_document_mp_live_fs_init(); // 0x009172B0 (:705)
void ui_document_mp_live_fs_enter(game_state prev_state); // 0x0090C690 (:722)
void ui_document_mp_live_fs_exit(game_state new_state); // 0x008E2F90 (:745)
void ui_document_mp_live_fs_process(); // 0x00914BF0 (:759)
void ui_document_mp_live_fs_handshake(rl_renderer*); // 0x0090C780 (:819)
void ui_document_mp_live_fs_render(rl_renderer*); // 0x008FBD60 (:827)
