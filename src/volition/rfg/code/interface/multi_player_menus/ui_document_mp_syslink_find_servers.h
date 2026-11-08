// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink_find_servers.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink_find_servers.cpp
//
// The system link server browser (the LAN search results).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_mp_syslink_fs_init(); // 0x00917440 (:706)
void ui_document_mp_syslink_fs_enter(game_state prev_state); // 0x0090E780 (:723)
void ui_document_mp_syslink_fs_exit(game_state new_state); // 0x008E5CF0 (:746)
void ui_document_mp_syslink_fs_process(); // 0x009156F0 (:760)
void ui_document_mp_syslink_fs_handshake(rl_renderer*); // 0x0090E870 (:805)
void ui_document_mp_syslink_fs_render(rl_renderer*); // 0x008FC910 (:813)

class multi_session_info;

// Test seam (UI.md 2.7 item 2, off by default): each loaded frame, the server the command line joins (a direct
// address, or a search result's session info), or null to leave the list to the player.
extern const multi_session_info* (*g_openrfgFindServersJoin)();
