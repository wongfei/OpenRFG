// Original: volition/rfg/code/gamewide/gamestate_quick_pause.h
// Implementation: volition/rfg/code/gamewide/gamestate_quick_pause.cpp
//
// The quick pause game state (GS_QUICK_PAUSE): the game state callbacks registered by gamestate_quick_pause_init.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void gamestate_quick_pause_init(); // 0x007E8FB0 (:28)
void gamestate_quick_pause_enter(game_state prev_state); // 0x007CD3D0 (:45)
void gamestate_quick_pause_exit(game_state new_state); // 0x007CD420 (:65)
void gamestate_quick_pause_process(); // 0x007D9180 (:87)
void gamestate_quick_pause_handshake(rl_renderer* pRenderer); // 0x007C0570 (:100)
void gamestate_quick_pause_render(rl_renderer* pRenderer); // 0x007C0580 (:105)
