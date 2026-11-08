// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.cpp
//
// The match results.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

// UI.md UI5: the results menu's state and the statics the experience tally (and, through rfg.exe's interface unity,
// controller_options.cpp) share; file statics and file-private types of ui_document_mp_results.cpp in rfg.pdb.
enum mp_menu_exit_actions_results
{
    MMEA_R_POP = 0,
    MMEA_R_PUSH = 1,
    NUM_MP_R_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_results
{
    MMS_R_INVALID = -1,
    MMS_R_LOADING = 0,
    MMS_R_LOADED = 1,
    MMS_R_EXITING = 2,
    MMS_R_UNLOADING = 3,
    MMS_R_UNLOADED = 4,
    NUM_MP_R_MENU_STATES = 5,
};

// size 0x8C
struct mp_match_results_shell_info
{

    mp_menu_states_results state; // 0x00
    mp_menu_states_results pending_state; // 0x04
    mp_menu_states_results prev_state; // 0x08
    mp_menu_exit_actions_results exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_item_handles[6]; // 0x18
    int data_group_handle; // 0x30
    farray<int,16> group_item_handles; // 0x34
    unsigned int control_id; // 0x84
    int input_id; // 0x88
};
static_assert(sizeof(mp_match_results_shell_info) == 0x8C, "mp_match_results_shell_info");

extern mp_match_results_shell_info Mp_match_results_info; // 0x01666900
extern timestamp_realtime Auto_continue_timestamp; // 0x0166698C
extern int Num_players; // 0x02BFD420
extern bool Entering_results; // 0x02C03636
extern bool No_matchmaking; // 0x02C03637
extern bool Pending_pop; // 0x02C03640

// Test seam (UI.md 2.7 item 2, off by default): asked each loaded frame; true continues as the back button does (the
// command line's results time ran out).
extern bool (*g_openrfgResultsContinue)();

class rl_renderer;

void ui_document_mp_results_init(); // 0x00917300 (:1386)
void ui_document_mp_results_enter(game_state prev_state); // 0x0090D8A0 (:1403)
void ui_document_mp_results_exit(game_state new_state); // 0x008F3160 (:1478)
void ui_document_mp_results_process(); // 0x009153F0 (:1502)
void ui_document_mp_results_handshake(rl_renderer*); // 0x008F3210 (:1540)
void ui_document_mp_results_render(rl_renderer*); // 0x008FC120 (:1548)
void ui_document_mp_results_next_page(); // 0x008AE2B0 (:1565)
void ui_document_mp_results_prev_page(); // 0x008AE330 (:1584)
void ui_document_mp_results_ui_pop(bool no_matchmaking); // 0x0088D6B0 (:1603)
void ui_document_mp_results_ui_set_pending_pop(); // 0x0088D6E0 (:1611)
bool ui_document_mp_results_pending_pop(); // 0x0088D6F0 (:1616)
