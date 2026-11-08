// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.h
// Implementation: volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.cpp
//
// The multiplayer statistics document. Placement of mp_stats_hint_choices inferred.
#pragma once

#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/vlib/code/util/varray.h"

class mempool_base;
class rl_renderer;

// UI.md UI5: the statistics menu's state, shared with the experience tally (file-private in rfg.pdb).
enum mp_menu_exit_actions_stats
{
    MMEA_ST_POP = 0,
    MMEA_ST_PUSH = 1,
    NUM_MP_ST_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_stats
{
    MMS_ST_LOADING = 0,
    MMS_ST_LOADED = 1,
    MMS_ST_UNLOADING = 2,
    MMS_ST_UNLOADED = 3,
    NUM_MP_ST_MENU_STATES = 4,
};

// size 0x90
struct mp_match_stats_shell_info
{

    mp_menu_states_stats state; // 0x00
    mp_menu_states_stats pending_state; // 0x04
    mp_menu_states_stats prev_state; // 0x08
    mp_menu_exit_actions_stats exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_item_handles[7]; // 0x18
    int data_group_handle; // 0x34
    farray<int,16> group_item_handles; // 0x38
    unsigned int control_id; // 0x88
    int input_id; // 0x8C
};
static_assert(sizeof(mp_match_stats_shell_info) == 0x90, "mp_match_stats_shell_info");

extern mp_match_stats_shell_info Mp_match_stats_info; // 0x01666A20

enum mp_stats_hint_choices
{
    MPS_HINT_INVALID = -1,
    MPS_HINT_ABORT = 0,
    MPS_HINT_CYCLE_LEADERBOARD_LEFT = 1,
    MPS_HINT_CYCLE_LEADERBOARD_RIGHT = 2,
    MPS_HINT_CYCLE_UNLOCKS_LEFT = 3,
    MPS_HINT_CYCLE_UNLOCKS_RIGHT = 4,
    MPS_HINT_CYCLE_PAGE_LEFT = 5,
    MPS_HINT_CYCLE_PAGE_RIGHT = 6,
    NUM_MPS_HINTS = 7,
};

mp_stats_hint_choices stats_internal_process_mouse(int input_id); // 0x008D4740 (:1321)
mp_stats_hint_choices mp_stats_external_process_mouse(int input_id); // 0x008D4960 (:1381)
void ui_document_mp_stats_init(); // 0x009173A0 (:1521)
void ui_document_mp_stats_enter(game_state prev_state); // 0x0090E4D0 (:1538)
void ui_document_mp_stats_exit(game_state new_state); // 0x008F3520 (:1565)
void ui_document_mp_stats_process(); // 0x009155B0 (:1584)
void ui_document_mp_stats_handshake(rl_renderer*); // 0x008F35B0 (:1615)
void ui_document_mp_stats_render(rl_renderer*); // 0x008FC870 (:1623)
void ui_document_mp_stats_multi_init(mempool_base& pool); // 0x0088E660 (:1640)
void ui_document_mp_stats_next_page(); // 0x0088E680 (:1646)
void ui_document_mp_stats_prev_page(); // 0x0088E6F0 (:1662)
void ui_document_mp_stats_ui_pop(); // 0x0088E760 (:1678)
