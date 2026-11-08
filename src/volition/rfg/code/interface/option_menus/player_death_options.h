// Original: volition/rfg/code/interface/option_menus/player_death_options.h
// Implementation: volition/rfg/code/interface/option_menus/player_death_options.cpp
//
// The menu shown when the player dies in single player (retry, load, quit).
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

void player_death_options_init(); // 0x00918C90 (:196)
void player_death_options_enter(game_state prev_state); // 0x0090FCC0 (:211)
void player_death_options_process_mouse_input(); // 0x009042A0 (:245)
void player_death_options_create_menu(); // 0x00904370 (:285)
void player_death_options_process(); // 0x009164A0 (:324)
void player_death_options_handshake(rl_renderer*); // 0x008930A0 (:387)
void player_death_options_render(rl_renderer*); // 0x00904640 (:392)
void player_death_options_exit(game_state new_state); // 0x008F4190 (:402)
int player_death_options_get_control_id(); // 0x008930B0 (:422)
int player_death_options_get_doc_handle(); // 0x008930C0 (:428)
void player_death_options_show_menu(bool show); // 0x008D7090 (:435)
