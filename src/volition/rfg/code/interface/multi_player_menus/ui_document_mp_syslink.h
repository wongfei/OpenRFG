// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink.cpp
//
// The system link menu (host or join a LAN game).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_syslink_init(); // 0x009173F0 (:771)
void ui_document_mp_syslink_enter(game_state prev_state); // 0x0090E5E0 (:788)
void ui_document_mp_syslink_exit(game_state new_state); // 0x008F3750 (:817)
void ui_document_mp_syslink_process(); // 0x00915630 (:834)
void ui_document_mp_syslink_handshake(rl_renderer*); // 0x008F3800 (:873)
void ui_document_mp_syslink_render(rl_renderer*); // 0x008FC8C0 (:881)

// Test seam (UI.md 2.7 item 2, off by default): each loaded frame, the entry the command line chooses (0 host, 1 find
// games), or -1 to leave the menu to the player.
extern int (*g_openrfgSyslinkSelection)();
