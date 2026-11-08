// Original: volition/rfg/code/interface/main_menu_idle.h
// Implementation: volition/rfg/code/interface/main_menu_idle.cpp
//
// The main menu's idle (attract) video.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void main_menu_idle_init(); // 0x009171C0 (:62)
void main_menu_idle_enter(game_state prev_state); // 0x008CF5E0 (:77)
void main_menu_idle_exit(game_state new_state); // 0x008BF9B0 (:112)
void main_menu_idle_process(); // 0x00914580 (:134)
void main_menu_idle_handshake(rl_renderer*); // 0x00889A30 (:169)
void main_menu_idle_render(rl_renderer*); // 0x00889A40 (:174)
