// Original: volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.h
// Implementation: volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp
//
// The wrecking crew scoreboard.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void ui_document_menu_wrecking_crew_scoreboard_init(); // 0x00917020 (:1377)
void ui_document_menu_wrecking_crew_scoreboard_enter(game_state prev_state); // 0x009074F0 (:1394)
void ui_document_menu_wrecking_crew_scoreboard_exit(game_state new_state); // 0x008EEE20 (:1456)
void ui_document_menu_wrecking_crew_scoreboard_process(); // 0x00912270 (:1495)
void ui_document_menu_wrecking_crew_scoreboard_handshake(rl_renderer*); // 0x008DBA70 (:1549)
void ui_document_menu_wrecking_crew_scoreboard_render(rl_renderer*); // 0x008F6AA0 (:1557)
