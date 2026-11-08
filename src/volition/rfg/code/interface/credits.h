// Original: volition/rfg/code/interface/credits.h
// Implementation: volition/rfg/code/interface/credits.cpp
//
// The credits state (rfg_credits).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void credits_init(); // 0x00917170 (:591)
void credits_enter(game_state prev_state); // 0x009096B0 (:604)
void credits_process(); // 0x009127D0 (:745)
void credits_exit(game_state prev_state); // 0x008F09C0 (:848)
void credits_render(rl_renderer*); // 0x008F70C0 (:893)
void credits_handshake(rl_renderer*); // 0x00884490 (:902)
void credits_show(bool end_game, unsigned int warp_to_obj); // 0x008844A0 (:906)
bool credits_are_rolling(); // 0x008844E0 (:918)
void credits_set_up_credits_page(); // 0x008844F0 (:923)
