// Original: volition/rfg/code/interface/option_menus/ingame_options.h
// Implementation: volition/rfg/code/interface/option_menus/ingame_options.cpp
//
// The in-game options (Esc menu): resume, options, quit (with the quit confirmation boxes), exit to the main menu.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"

class rl_renderer;

bool ingame_options_should_show_save_game(); // 0x00890F30 (:172)
void ingame_options_start_auto_catch(bool retry); // 0x00900D00 (:245)
void ingame_options_destroy_menu(); // 0x008D6AA0 (:305)
void ingame_options_create_menu(bool animate_in); // 0x00917E90 (:591)
void ingame_options_init(); // 0x00919850 (:715)
void ingame_options_enter(game_state prev_state); // 0x0090F590 (:731)
void ingame_options_process_mouse_input(); // 0x009185B0 (:784)
void ingame_options_process(); // 0x00918720 (:822)
void ingame_options_exit_to_main_menu(); // 0x00890FB0 (:944)
void ingame_options_handshake(rl_renderer*); // 0x008AF5E0 (:993)
void ingame_options_render(rl_renderer*); // 0x00901480 (:1021)
void ingame_options_exit(game_state new_state); // 0x00918A70 (:1031)
void ingame_options_show_menu(bool show); // 0x008D6E10 (:1068)
bool ingame_options_is_active(); // 0x008910D0 (:1083)
int ingame_options_get_control_id(); // 0x008910E0 (:1089)
int ingame_options_get_doc_handle(); // 0x008910F0 (:1095)

// UI.md UI6: file statics of rfg.exe's interface unity (ingame_options.cpp), external since UI6: the options main menu
// reads the in-game document and menu.
extern int Doc_handle_ioptions; // 0x01664104
extern int Menu_handle_ioptions; // 0x01664108
extern int Control_id_ioptions; // 0x01664114
