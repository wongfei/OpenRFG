// Original: volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.cpp
//
// The Wrecking Crew leaderboards (GS_WRECKING_CREW_SCOREBOARD, wrecking.md WC2) on rfg_ui_mp_leaderboard: one board per
// challenge map (the map's challenge leaderboard), filtered to everyone or friends, a page of 16 rows (14 in Arabic). The
// columns are the total and the three rounds. Retrieval runs through multi_core_platform_scoreboard_* with a loading box
// up, out of a 1 MB pool of level memory the state owns while it is up. Derived from ui_document_mp_scoreboard.cpp.
//
// Original bugs kept (commented): compare results are not a consistent ordering for a player on equal rank; a board change
// ignores the filter passed in; the board cycle steps map indices modulo the number of challenge modes and loops forever
// when no map has a challenge; the board entered with is chosen by map index, a cycled one by leaderboard id; a failed
// data item create reuses the previous handle.

#include "volition/rfg/code/interface/ui_document_menu_wrecking_crew_scoreboard.h"

#include <cstdio>
#include <cstdlib>

#include "keen/base/language.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_game/multi_game_friend.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

// ---- file-private types (rfg.pdb)
enum filter_mode_wcsco
{
    FILTER_WCSCO_NONE = 0,
    FILTER_WCSCO_FRIENDS = 1,
};

enum mp_menu_exit_actions_wcsco
{
    MMEA_WCSCO_POP = 0,
    MMEA_WCSCO_PUSH = 1,
    NUM_MP_WCSCO_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_wcsco
{
    MMS_WCSCO_LOADING = 0,
    MMS_WCSCO_LOADED = 1,
    MMS_WCSCO_UNLOADING = 2,
    MMS_WCSCO_UNLOADED = 3,
    NUM_MP_WCSCO_MENU_STATES = 4,
};

// size 0x8C
struct mp_match_scoreboard_shell_info_wcsco
{

    mp_menu_states_wcsco state; // 0x00
    mp_menu_states_wcsco pending_state; // 0x04
    mp_menu_states_wcsco prev_state; // 0x08
    mp_menu_exit_actions_wcsco exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_item_handles[6]; // 0x18
    int data_group_handle; // 0x30
    farray<int,16> group_item_handles; // 0x34
    unsigned int control_id; // 0x84
    int input_id; // 0x88
};
static_assert(sizeof(mp_match_scoreboard_shell_info_wcsco) == 0x8C, "mp_match_scoreboard_shell_info_wcsco");

enum my_leaderboard_state_wcsco
{
    MY_LEADERBOARD_WCSCO_UNKNOWN = 0,
    MY_LEADERBOARD_WCSCO_EXISTS = 1,
    MY_LEADERBOARD_WCSCO_DOESNT_EXIST = 2,
};

enum request_type_wcsco
{
    REQUEST_TYPE_WCSCO_NONE = 0,
    REQUEST_TYPE_WCSCO_FRIENDS = 1,
    REQUEST_TYPE_WCSCO_BOARD = 2,
};

enum scoreboard_hint_choices_wcsco
{
    WCSB_HINT_INVALID = -1,
    WCSB_HINT_ABORT = 0,
    WCSB_HINT_CYCLE_LEADERBOARD_LEFT = 1,
    WCSB_HINT_CYCLE_LEADERBOARD_RIGHT = 2,
    WCSB_HINT_CHANGE_FILTER = 3,
    WCSB_HINT_GOTO_ME = 4,
    WCSB_HINT_VIEW_GAMERCARD = 5,
    NUM_WCSB_HINTS = 6,
};

// ---- file-static globals
static const char* Ui_mp_shell_name_wcsco = {"rfg_ui_mp_leaderboard"}; // 0x01664C80
static const char* Menu_data_item_names_wcsco[6] = {"LeaderboardHighlightIndex", "LeaderboardHintInfo", "LeaderboardTitles", "LeaderboardHeaderInfo", "LeaderboardModeInfo", "LeaderboardScoreInfo"}; // 0x01664C84
static const char* Mode_colon_string_wcsco = {"MENU_MODE_COLON"}; // 0x01664C9C
static const char* Map_colon_string_wcsco = {"MENU_MAP_COLON"}; // 0x01664CA0
static const char* Back_string_wcsco = {"MENU_BACK"}; // 0x01664CA4
static const char* Cycle_leaderboard_string_wcsco = {"MENU_CYCLE_LEADERBOARD"}; // 0x01664CA8
static int Starting_index_wcsco = -1; // 0x01664CAC
// The board's request: the scoreboard index is set from the map (4 + the challenge's leaderboard id, see change_request).
// The column names are not read by this file (wc_scoreboard_process_data_item_info_text writes its own).
static scoreboard_info WC_scoreboard_info = {PST_WRECKING_CREW, 0, (platform_game_type_context)0, { // 0x01664CB0
    {PSCT_INT, "MENU_WC_TOTAL"},
    {PSCT_INT, "MENU_WC_ROUND 1"},
    {PSCT_INT, "MENU_WC_ROUND 2"},
    {PSCT_INT, "MENU_WC_ROUND 3"},
}};
static int Msgbox_handle_wcsco = -1; // 0x01664DCC
static scoreboard_hint_choices_wcsco Current_mouse_hint_choice_wcsco = WCSB_HINT_INVALID; // 0x01664DD0
static mp_match_scoreboard_shell_info_wcsco Mp_match_scoreboard_info_wcsco; // 0x01667588
// This file's copies of the header's hint object names (the same strings as ui_document_mp_scoreboard.cpp's).
static const char* kHintViewCard = {"row_highlight_button"}; // 0x01668598
static const char* kHintGroup5 = {"hint_5_group"}; // 0x0166859C
static const char* kHintGroup4 = {"hint_4_group"}; // 0x016685A0
static const char* kHintGroup3 = {"hint_3_group"}; // 0x016685A4
static const char* kHintGroup2 = {"hint_2_group"}; // 0x016685A8
static const char* kHintGroup1 = {"hint_1_group"}; // 0x016685AC
static const char* kGroup_name = {"row_group"}; // 0x0166869C
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
static int scoreboard_hint_config_wcsco[6]; // 0x02BF4310
static scoreboard_results Scoreboard_results_wcsco; // 0x02C00E30
static bool Goto_me_wcsco; // 0x02C080B3
static int Match_scoreboard_selection_wcsco; // 0x02C080D0
// The map whose challenge board is shown (an index into the map table).
static int Current_wc_scoreboard; // 0x02C080D4
static multi_player_uid* Scoreboard_friend_uid_list_wcsco; // 0x02C080D8
static int Friend_count_wcsco; // 0x02C080DC
static int Used_starting_index_wcsco; // 0x02C080E0
static unsigned char* Scoreboard_scratch_wcsco; // 0x02C080E4
static request_type_wcsco Request_type_wcsco; // 0x02C080E8
static my_leaderboard_state_wcsco My_leaderboard_state_wcsco; // 0x02C080EC
static int My_rank_wcsco; // 0x02C080F0
static filter_mode_wcsco Requested_filter_mode_wcsco; // 0x02C080F4
static filter_mode_wcsco Current_filter_mode_wcsco; // 0x02C080F8
static int scoreboard_num_hints_wcsco; // 0x02C08104
// The pool the results, the friend uids and the retrieval scratch come from (1 MB of level memory while the state is up).
static rfg_split_mempool WC_scoreboard_mempool; // 0x02C7E2D0

// ---- internal functions

// Port helper: the rows of a page, 16 (14 in Arabic, whose layout is taller); inlined everywhere.
static int scoreboard_page_size()
{
    return (Current_language != keen::LanguageId_Ar_Eg ? 2 : 0) + 14;
}

// Port helper: the page's first entry in the results (a friends board has them all, the others only the page); inlined.
static int scoreboard_page_offset()
{
    return Current_filter_mode_wcsco == FILTER_WCSCO_FRIENDS ? Starting_index_wcsco - 1 : 0;
}

// Port helper: the page's rows in the results; inlined.
static int scoreboard_page_rows()
{
    const int rows = Scoreboard_results_wcsco.num_entries - scoreboard_page_offset();
    return rows < scoreboard_page_size() ? rows : scoreboard_page_size();
}

// 0x008B6610 (:297)
static bool loading_box_cb_wcscb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (choice != MBC_ABORT && choice != MBC_FORCED_EXIT)
    {
        return false;
    }
    multi_core_platform_scoreboard_cancel_retrieve();
    multi_game_friend_cancel_all();
    Msgbox_handle_wcsco = -1;
    Mp_match_scoreboard_info_wcsco.exit_action = MMEA_WCSCO_POP;
    Mp_match_scoreboard_info_wcsco.pending_state = MMS_WCSCO_UNLOADING;
    return true;
}

// 0x0089D7D0 (:310)
// The start of my rank's page. Original quirk: a rank right after a page boundary gets one less (rank 17 gives 16).
static int get_my_starting_index_wcscb()
{
    const int page = scoreboard_page_size();
    const int rank = My_rank_wcsco - 1;
    return rank / page * page + (rank % page != 0 ? 1 : 0);
}

// 0x0089D800 (:323)
static bool request_pending_wcscb()
{
    return Request_type_wcsco != REQUEST_TYPE_WCSCO_NONE || Starting_index_wcsco < 0 || Requested_filter_mode_wcsco != Current_filter_mode_wcsco || My_leaderboard_state_wcsco == MY_LEADERBOARD_WCSCO_UNKNOWN;
}

// 0x008B6650 (:331)
static void select_my_player_wcscb()
{
    for (int i = 0; i < scoreboard_page_rows(); i++)
    {
        if (Scoreboard_results_wcsco.entries[scoreboard_page_offset() + i].player_uid.is_equal(My_player_uid))
        {
            Match_scoreboard_selection_wcsco = i;
            return;
        }
    }
}

// 0x0089D840 (:346)
// LTCG: scoreboard in ESI, filter in EAX. A board change takes the map's challenge leaderboard (none: nothing changes).
static void change_request_wcscb(int scoreboard, filter_mode_wcsco filter, int starting_index, bool goto_me)
{
    if (Current_wc_scoreboard == scoreboard)
    {
        if (filter == Current_filter_mode_wcsco && filter == Requested_filter_mode_wcsco)
        {
            Starting_index_wcsco = starting_index;
            Goto_me_wcsco = goto_me;
            return;
        }
        Requested_filter_mode_wcsco = filter;
    }
    else
    {
        const wrecking_crew_challenge_mode* mode = wrecking_crew_get_challenge_mode_by_index(wrecking_crew_get_map_challenge_id(scoreboard));
        if (mode == nullptr || mode->leaderboard_id == -1)
        {
            return;
        }
        // Magic number: the Wrecking Crew boards follow the four multiplayer ones.
        WC_scoreboard_info.scoreboard_index = mode->leaderboard_id + 4;
        // Original bug: a board change ignores the filter (the callers pass none): the request's filter stays.
        Current_filter_mode_wcsco = Requested_filter_mode_wcsco;
        Current_wc_scoreboard = scoreboard;
    }
    Goto_me_wcsco = false;
    Starting_index_wcsco = -1;
    Friend_count_wcsco = 0;
    My_leaderboard_state_wcsco = MY_LEADERBOARD_WCSCO_UNKNOWN;
}

// 0x00906E50 (:377)
static void show_loading_msgbox_wcsco()
{
    if (Msgbox_handle_wcsco == -1)
    {
        Msgbox_handle_wcsco = ui_add_msgbox(MBT_LOADING_CANCEL, "MENU_MSJS_JOIN_IN_PROGRESS", "", loading_box_cb_wcscb, false, false, nullptr, nullptr, false);
    }
}

// 0x008EE5F0 (:384)
static void hide_loading_msgbox_wcsco()
{
    if (Msgbox_handle_wcsco != -1)
    {
        ui_hide_msgbox(Msgbox_handle_wcsco, false);
        Msgbox_handle_wcsco = -1;
    }
}

// 0x0089D8D0 (:394)
// By rank, the unranked after them: first me, then by name.
static int sort_friends_wcsco(const void* entry1p, const void* entry2p)
{
    const scoreboard_entry* entry1 = (const scoreboard_entry*)entry1p;
    const scoreboard_entry* entry2 = (const scoreboard_entry*)entry2p;
    if (entry1->rank > 0)
    {
        if (entry2->rank > 0)
        {
            // Original bug: an equal rank compares as greater both ways, not a consistent ordering for qsort.
            return entry2->rank <= entry1->rank ? 1 : -1;
        }
        return -1;
    }
    if (entry2->rank > 0)
    {
        return 1;
    }
    if (entry1->player_uid.is_equal(My_player_uid))
    {
        return -1;
    }
    if (entry2->player_uid.is_equal(My_player_uid))
    {
        return 1;
    }
    // Magic number: 0x1E is player_name's length.
    return _v_strnicmp(entry1->player_name, entry2->player_name, 0x1E);
}

// 0x009071C0 (:423)
static void process_stats_request_wcsco()
{
    if (Request_type_wcsco == REQUEST_TYPE_WCSCO_NONE)
    {
        // My own row comes first, to know the rank (for friends that is the whole list).
        scoreboard_request request;
        bool retrieve = false;
        if (Requested_filter_mode_wcsco == FILTER_WCSCO_FRIENDS)
        {
            if (My_leaderboard_state_wcsco == MY_LEADERBOARD_WCSCO_UNKNOWN)
            {
                // Original: the friends request type is overwritten by the board request type below.
                Request_type_wcsco = REQUEST_TYPE_WCSCO_FRIENDS;
                request.set_uids(&WC_scoreboard_info, &My_player_uid, scoreboard_page_size(), Scoreboard_scratch_wcsco);
                request.request_type = SRT_FRIENDS;
                retrieve = true;
            }
        }
        else if (My_leaderboard_state_wcsco == MY_LEADERBOARD_WCSCO_UNKNOWN)
        {
            request.set_uids(&WC_scoreboard_info, &My_player_uid, 1, Scoreboard_scratch_wcsco);
            request.request_type = SRT_PIVOT_PLAYER;
            retrieve = true;
        }
        else if (Starting_index_wcsco == -1)
        {
            request.set(&WC_scoreboard_info, 1, scoreboard_page_size(), Scoreboard_scratch_wcsco);
            retrieve = true;
        }
        else if (Scoreboard_results_wcsco.num_entries >= 1 && (Starting_index_wcsco < Scoreboard_results_wcsco.entries[0].rank || Scoreboard_results_wcsco.entries[Scoreboard_results_wcsco.num_entries - 1].rank < Starting_index_wcsco))
        {
            // The page isn't in the results yet.
            if (Scoreboard_results_wcsco.total_entries_on_scoreboard < Starting_index_wcsco || Starting_index_wcsco < 2)
            {
                Starting_index_wcsco = 1;
            }
            request.set(&WC_scoreboard_info, Starting_index_wcsco, scoreboard_page_size(), Scoreboard_scratch_wcsco);
            retrieve = true;
        }
        if (retrieve)
        {
            multi_core_platform_scoreboard_retrieve_data(&Scoreboard_results_wcsco, &request);
            Request_type_wcsco = REQUEST_TYPE_WCSCO_BOARD;
        }
    }
    if (Request_type_wcsco == REQUEST_TYPE_WCSCO_BOARD)
    {
        const platform_op_status status = multi_core_platform_scoreboard_retrieve_state();
        if (status == POS_SUCCESS)
        {
            if (Requested_filter_mode_wcsco == FILTER_WCSCO_FRIENDS)
            {
                if (Scoreboard_results_wcsco.num_entries < 1)
                {
                    My_leaderboard_state_wcsco = MY_LEADERBOARD_WCSCO_DOESNT_EXIST;
                    My_rank_wcsco = 0;
                }
                else
                {
                    Friend_count_wcsco = Scoreboard_results_wcsco.num_entries;
                    Scoreboard_results_wcsco.total_entries_on_scoreboard = Scoreboard_results_wcsco.num_entries;
                    My_leaderboard_state_wcsco = MY_LEADERBOARD_WCSCO_EXISTS;
                    qsort(Scoreboard_results_wcsco.entries, Scoreboard_results_wcsco.num_entries, sizeof(scoreboard_entry), sort_friends_wcsco);
                    for (int i = 0; i < Scoreboard_results_wcsco.num_entries; i++)
                    {
                        if (My_player_uid.is_equal(Scoreboard_results_wcsco.entries[i].player_uid))
                        {
                            My_rank_wcsco = i + 1;
                            break;
                        }
                    }
                }
                Starting_index_wcsco = 1;
            }
            else if (My_leaderboard_state_wcsco == MY_LEADERBOARD_WCSCO_UNKNOWN)
            {
                if (Scoreboard_results_wcsco.num_entries < 1 || Scoreboard_results_wcsco.entries[0].rank < 1)
                {
                    My_leaderboard_state_wcsco = MY_LEADERBOARD_WCSCO_DOESNT_EXIST;
                }
                else
                {
                    My_leaderboard_state_wcsco = MY_LEADERBOARD_WCSCO_EXISTS;
                    My_rank_wcsco = Scoreboard_results_wcsco.entries[0].rank;
                }
            }
            else if (Scoreboard_results_wcsco.num_entries < 1)
            {
                Starting_index_wcsco = 0;
                Used_starting_index_wcsco = 0;
            }
            else if (Starting_index_wcsco == -1)
            {
                Starting_index_wcsco = Scoreboard_results_wcsco.entries[0].rank;
            }
            Current_filter_mode_wcsco = Requested_filter_mode_wcsco;
            Request_type_wcsco = REQUEST_TYPE_WCSCO_NONE;
        }
        else if (status == POS_FAILED)
        {
            if (My_leaderboard_state_wcsco == MY_LEADERBOARD_WCSCO_UNKNOWN)
            {
                My_leaderboard_state_wcsco = MY_LEADERBOARD_WCSCO_DOESNT_EXIST;
            }
            else
            {
                Starting_index_wcsco = 0;
                Used_starting_index_wcsco = 0;
            }
            Scoreboard_results_wcsco.num_entries = 0;
            Request_type_wcsco = REQUEST_TYPE_WCSCO_NONE;
        }
    }
    if (!request_pending_wcscb())
    {
        hide_loading_msgbox_wcsco();
        if (Goto_me_wcsco)
        {
            select_my_player_wcscb();
            Goto_me_wcsco = false;
        }
    }
    else
    {
        show_loading_msgbox_wcsco();
    }
}

// 0x008C8750 (:643)
static void scoreboard_load_global_data_items_wcsco()
{
    // Original bug: a failed create keeps the previous item's handle (0 for the first).
    unsigned int handle = 0;
    for (int i = 0; i < 6; i++)
    {
        vint_data_item* item = vint_data_item::create_new(Menu_data_item_names_wcsco[i]);
        if (item != nullptr)
        {
            switch (i)
            {
            case 0:
            {
                vint_variant_type types[1] = {VINT_PROP_TYPE_INT};
                item->set_template(types, 1);
                break;
            }
            case 2:
            {
                vint_variant_type types[2] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 2);
                break;
            }
            case 3:
            {
                vint_variant_type types[5] = {VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 5);
                break;
            }
            case 4:
            {
                vint_variant_type types[5] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_STRING};
                item->set_template(types, 5);
                item->set_element(0, vint_variant(false));
                item->set_element(1, vint_variant(Mode_colon_string_wcsco));
                item->set_element(2, vint_variant("MODE NAME"));
                item->set_element(3, vint_variant(Map_colon_string_wcsco));
                item->set_element(4, vint_variant("LEVEL NAME"));
                break;
            }
            case 5:
            {
                vint_variant_type types[3] = {VINT_PROP_TYPE_BOOL, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT};
                item->set_template(types, 3);
                item->set_element(0, vint_variant(false));
                break;
            }
            default:
                break;
            }
            handle = item->get_handle();
        }
        Mp_match_scoreboard_info_wcsco.data_item_handles[i] = (int)handle;
        ui_add_data_item(Mp_match_scoreboard_info_wcsco.doc_handle, (int)handle);
    }
}

// 0x008C8A00 (:749)
static void scoreboard_add_menu_option_data_item_wcsco()
{
    vint_data_item* item = vint_data_group::find(Mp_match_scoreboard_info_wcsco.data_group_handle)->add_data_item(nullptr);
    if (item != nullptr)
    {
        vint_variant_type types[6] = {
            VINT_PROP_TYPE_STRING, // name
            VINT_PROP_TYPE_BOOL, // selected
            VINT_PROP_TYPE_STRING,
            VINT_PROP_TYPE_STRING,
            VINT_PROP_TYPE_STRING,
            VINT_PROP_TYPE_STRING,
        };
        item->set_template(types, 6);
        Mp_match_scoreboard_info_wcsco.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008B6710 (:776)
static void scoreboard_remove_menu_option_data_item_wcsco()
{
    vint_data_group* group = vint_data_group::find(Mp_match_scoreboard_info_wcsco.data_group_handle);
    const int last = Mp_match_scoreboard_info_wcsco.group_item_handles.size() - 1;
    group->remove_data_item(Mp_match_scoreboard_info_wcsco.group_item_handles[last]);
    Mp_match_scoreboard_info_wcsco.group_item_handles.resize(last);
}

// 0x008C8AC0 (:786)
static void scoreboard_load_data_groups_wcsco()
{
    vint_data_group* group = vint_data_group::create_new("LeaderboardSetData");
    if (group != nullptr)
    {
        Mp_match_scoreboard_info_wcsco.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_match_scoreboard_info_wcsco.doc_handle, Mp_match_scoreboard_info_wcsco.data_group_handle);
    }
}

// 0x008C8AF0 (:804)
static void scoreboard_load_ui_information_wcsco()
{
    scoreboard_load_global_data_items_wcsco();
    scoreboard_load_data_groups_wcsco();
}

// 0x008DB8D0 (:573)
static void scoreboard_do_state_change_wcsco()
{
    do
    {
        Mp_match_scoreboard_info_wcsco.state = Mp_match_scoreboard_info_wcsco.pending_state;
        if (Mp_match_scoreboard_info_wcsco.pending_state == MMS_WCSCO_LOADED)
        {
            scoreboard_load_ui_information_wcsco();
        }
        else if (Mp_match_scoreboard_info_wcsco.pending_state == MMS_WCSCO_UNLOADING)
        {
            multi_core_platform_scoreboard_cancel_retrieve();
            multi_game_friend_cancel_all();
        }
        else
        {
            return;
        }
    }
    while (Mp_match_scoreboard_info_wcsco.state != Mp_match_scoreboard_info_wcsco.pending_state);
}

// 0x0089D940 (:609)
static void scoreboard_set_state(mp_menu_states_wcsco new_state)
{
    Mp_match_scoreboard_info_wcsco.pending_state = new_state;
}

// 0x0089D950 (:618)
// Note: the push target is the multiplayer live menu, as in the multiplayer leaderboards (never pushed: the exit pops).
static void scoreboard_init_state_wcsco()
{
    Mp_match_scoreboard_info_wcsco.state = MMS_WCSCO_UNLOADED;
    Mp_match_scoreboard_info_wcsco.pending_state = MMS_WCSCO_UNLOADED;
    Mp_match_scoreboard_info_wcsco.prev_state = MMS_WCSCO_UNLOADED;
    Mp_match_scoreboard_info_wcsco.exit_action = MMEA_WCSCO_POP;
    Mp_match_scoreboard_info_wcsco.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008B66D0 (:630)
static void scoreboard_init_info_wcsco()
{
    Mp_match_scoreboard_info_wcsco.group_item_handles.clear();
    for (int i = 0; i < 6; i++)
    {
        Mp_match_scoreboard_info_wcsco.data_item_handles[i] = -1;
    }
    Mp_match_scoreboard_info_wcsco.data_group_handle = -1;
}

// 0x008EE610 (:814)
// LTCG: the document handle in EDI.
static void scoreboard_unload_ui_information_wcsco()
{
    vint_doc_info* info = ui_get_doc_info(Mp_match_scoreboard_info_wcsco.doc_handle);
    if (info != nullptr)
    {
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    scoreboard_init_info_wcsco();
    multi_player_menu_system_close_message_box();
}

// 0x008EE6A0 (:823)
static void wc_scoreboard_process_data_item_hint_text(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info_wcsco.data_item_handles[1]);
    if (item == nullptr)
    {
        return;
    }
    for (int i = 1; i < 6; i++)
    {
        scoreboard_hint_config_wcsco[i] = WCSB_HINT_INVALID;
    }
    multi_player_menu_system_begin_hints(item);
    scoreboard_hint_config_wcsco[0] = WCSB_HINT_ABORT;
    scoreboard_num_hints_wcsco = 1;
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Back_string_wcsco), CBA_MENU_ABORT);
    multi_player_menu_system_add_hint_config(scoreboard_num_hints_wcsco, scoreboard_hint_config_wcsco, WCSB_HINT_CYCLE_LEADERBOARD_LEFT);
    // The left cycle button has no text of its own, the right one's stands for both.
    multi_player_menu_system_add_hint(localization_get_display_string_opt(""), CBA_MENU_L1);
    multi_player_menu_system_add_hint_config(scoreboard_num_hints_wcsco, scoreboard_hint_config_wcsco, WCSB_HINT_CYCLE_LEADERBOARD_RIGHT);
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Cycle_leaderboard_string_wcsco), CBA_MENU_R1);
    multi_player_menu_system_add_hint_config(scoreboard_num_hints_wcsco, scoreboard_hint_config_wcsco, WCSB_HINT_CHANGE_FILTER);
    multi_player_menu_system_add_hint(localization_get_display_string_opt("MENU_CHANGE_FILTER"), CBA_MENU_Y);
    if (!request_pending_wcscb() && (My_leaderboard_state_wcsco == MY_LEADERBOARD_WCSCO_EXISTS || (Current_filter_mode_wcsco == FILTER_WCSCO_FRIENDS && Scoreboard_results_wcsco.num_entries > 0)))
    {
        multi_player_menu_system_add_hint(localization_get_display_string_req("MENU_GOTO_ME", nullptr), CBA_MENU_X);
        multi_player_menu_system_add_hint_config(scoreboard_num_hints_wcsco, scoreboard_hint_config_wcsco, WCSB_HINT_GOTO_ME);
    }
    // The gamer card hint has no button text (the highlighted row's button).
    multi_player_menu_system_add_hint_config(scoreboard_num_hints_wcsco, scoreboard_hint_config_wcsco, WCSB_HINT_VIEW_GAMERCARD);
    multi_player_menu_system_end_hints();
}

// 0x008EE8C0 (:875)
// {the map's name and the filter, the selected row's rank or "--"}.
static void wc_scoreboard_process_data_item_titles(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info_wcsco.data_item_handles[2]);
    if (item == nullptr)
    {
        return;
    }
    wchar_t map_name[256] = L"Unnamed";
    const char* map_tag = wrecking_crew_get_map_display_name(Current_wc_scoreboard);
    if (map_tag != nullptr)
    {
        // A missing localization shows the tag itself.
        const wchar_t* localized = localization_get_display_string_req(map_tag, nullptr);
        if (localized != nullptr)
        {
            sprintf(map_name, L"%ls", localized);
        }
        else
        {
            sprintf(map_name, L"%hs", map_tag);
        }
    }
    wchar_t text[256];
    char encoded[768];
    const wchar_t* filter_name = localization_get_display_string_opt(Current_filter_mode_wcsco == FILTER_WCSCO_FRIENDS ? "MENU_SIT_FRIENDS" : "MENU_SIT_ALL");
    _snwprintf_safe(text, 0x100, L"%ls: %ls", map_name, filter_name);
    vint_encode_wide_text(encoded, 0x300, text);
    item->set_element(0, vint_variant((const char*)encoded));
    bool has_rank = false;
    if (Scoreboard_results_wcsco.num_entries > 0 && Match_scoreboard_selection_wcsco >= 0 && !request_pending_wcscb())
    {
        if (Match_scoreboard_selection_wcsco >= scoreboard_page_rows())
        {
            Match_scoreboard_selection_wcsco = scoreboard_page_rows() - 1;
        }
        const int rank = Scoreboard_results_wcsco.entries[scoreboard_page_offset() + Match_scoreboard_selection_wcsco].rank;
        if (rank > 0)
        {
            _snwprintf_safe(text, 0x100, L"%ls: %i", localization_get_display_string_opt("MENU_RANK"), rank);
            // Magic number: the original encodes the rank text into 0x100 of the 0x300 bytes.
            vint_encode_wide_text(encoded, 0x100, text);
            has_rank = true;
        }
    }
    if (!has_rank)
    {
        _snwprintf_safe(text, 0x100, L"%ls: --", localization_get_display_string_opt("MENU_RANK"));
        vint_encode_wide_text(encoded, 0x100, text);
    }
    item->set_element(1, vint_variant((const char*)encoded));
}

// 0x008EEC00 (:932)
// The column headers: name, total, then "round N" for the three rounds.
static void wc_scoreboard_process_data_item_info_text(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info_wcsco.data_item_handles[3]);
    if (item == nullptr)
    {
        return;
    }
    item->set_element(0, vint_variant("PLT_GAMERTAG"));
    item->set_element(1, vint_variant("MENU_WC_TOTAL"));
    const wchar_t* round_text = localization_get_display_string_opt("MENU_WC_ROUND");
    for (int round = 1; round <= 3; round++)
    {
        wchar_t text[32];
        char encoded[96];
        if (round_text != nullptr)
        {
            sprintf(text, L"%ls %d", round_text, round);
        }
        else
        {
            // A missing localization shows the tag itself.
            sprintf(text, L"%hs %d", "MENU_WC_ROUND", round);
        }
        vint_encode_wide_text(encoded, 0x60, text);
        item->set_element(round + 1, vint_variant((const char*)encoded));
    }
}

// 0x008C8B20 (:964)
static void wc_scoreboard_process_data_group_menu_options(float dtime)
{
    if (request_pending_wcscb())
    {
        return;
    }
    vint_data_group* group = vint_data_group::find(Mp_match_scoreboard_info_wcsco.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    char text[256];
    int num_rows = (int)group->get_num_data_items();
    if (scoreboard_page_rows() == 0)
    {
        // A single row with the message.
        if (num_rows == 0)
        {
            scoreboard_add_menu_option_data_item_wcsco();
            num_rows = 1;
        }
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info_wcsco.group_item_handles[0]);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant("MENU_NO_LEADERBOARD_RESULTS"));
            item->set_element(1, vint_variant(false));
            vint_encode_wide_text(text, 0x100, L"");
            for (int column = 0; column < 4; column++)
            {
                item->set_element(column + 2, vint_variant((const char*)text));
            }
        }
        for (; num_rows > 1; num_rows--)
        {
            scoreboard_remove_menu_option_data_item_wcsco();
        }
        return;
    }
    for (; num_rows < scoreboard_page_rows(); num_rows++)
    {
        scoreboard_add_menu_option_data_item_wcsco();
    }
    for (int i = 0; i < scoreboard_page_rows(); i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info_wcsco.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        const scoreboard_entry& entry = Scoreboard_results_wcsco.entries[scoreboard_page_offset() + i];
        vint_encode_wide_text(text, 0x100, entry.player_name);
        item->set_element(0, vint_variant((const char*)text));
        item->set_element(1, vint_variant(i == Match_scoreboard_selection_wcsco));
        for (int column = 0; column < 4; column++)
        {
            if (entry.columns[column].column_type == PSCT_INT)
            {
                sprintf(text, "%d", entry.columns[column].column_value.i_val);
            }
            else if (entry.columns[column].column_type == PSCT_FLOAT)
            {
                sprintf(text, "%.2f", (double)entry.columns[column].column_value.f_val);
            }
            else
            {
                text[0] = '\0';
            }
            item->set_element(column + 2, vint_variant((const char*)text));
        }
    }
    for (; num_rows > scoreboard_page_rows(); num_rows--)
    {
        scoreboard_remove_menu_option_data_item_wcsco();
    }
}

// 0x008EED60 (:1055)
static void wc_scoreboard_process_data_items()
{
    // dtime is unused by the functions (the original passes whatever is on the stack).
    wc_scoreboard_process_data_item_hint_text(0.0f);
    wc_scoreboard_process_data_item_titles(0.0f);
    wc_scoreboard_process_data_item_info_text(0.0f);
    wc_scoreboard_process_data_group_menu_options(0.0f);
}

// 0x008EED80 (:1066)
static void wc_scoreboard_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_match_scoreboard_info_wcsco.doc_handle))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_menu_wrecking_crew_scoreboard.cpp", 1066);
        [[fallthrough]];
    case ST_ERROR:
        ui_unload_document(Mp_match_scoreboard_info_wcsco.doc_handle, DFOT_NONE, 0.5f);
        Mp_match_scoreboard_info_wcsco.doc_handle = -1;
        scoreboard_set_state(MMS_WCSCO_UNLOADING);
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        scoreboard_set_state(MMS_WCSCO_LOADED);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_menu_wrecking_crew_scoreboard.cpp", 1066);
        break;
    }
}

// 0x008C8EF0 (:1098)
static void scoreboard_process_mouse_hints_wcsco()
{
    Current_mouse_hint_choice_wcsco = WCSB_HINT_INVALID;
    // Only with the input focus: the lock, else the newest request.
    int focus = Current_input_lock_id;
    if (focus == -1)
    {
        focus = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    if (Mp_match_scoreboard_info_wcsco.input_id != focus)
    {
        return;
    }
    const char* hints_1[3] = {kHintGroup1, kHintGroup2, kHintGroup3};
    const char* hints_2[3] = {kHintGroup4, kHintGroup5, kHintViewCard};
    const int hint_1 = ui_mouse_process_hints(hints_1, 3, Mp_match_scoreboard_info_wcsco.doc_handle, true);
    const int hint_2 = ui_mouse_process_hints(hints_2, 3, Mp_match_scoreboard_info_wcsco.doc_handle, true);
    if (hint_1 != -1)
    {
        Current_mouse_hint_choice_wcsco = (scoreboard_hint_choices_wcsco)scoreboard_hint_config_wcsco[hint_1];
    }
    if (hint_2 != -1)
    {
        Current_mouse_hint_choice_wcsco = (scoreboard_hint_choices_wcsco)scoreboard_hint_config_wcsco[hint_2 + 3];
    }
}

// 0x008DB930 (:1129)
// The hints, then (no message box up) a click on a row selects it. events_consumed is not used.
static void scoreboard_input_process_mouse_wcsco(bool& events_consumed)
{
    scoreboard_process_mouse_hints_wcsco();
    if (!mouse_just_moved() && !mouse_just_released(0))
    {
        return;
    }
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Mp_match_scoreboard_info_wcsco.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    const int num_rows = Mp_match_scoreboard_info_wcsco.group_item_handles.size();
    for (int i = 0; i < num_rows; i++)
    {
        char name[128];
        if (i == 0)
        {
            keen::formatString(name, 128, "%s", kGroup_name);
        }
        else
        {
            keen::formatString(name, 128, "%s%d", kGroup_name, i + 1);
        }
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(name, info->m_doc)))
        {
            if (i != Match_scoreboard_selection_wcsco && mouse_just_released(0))
            {
                multi_player_menu_system_set_menu_selection(i, Match_scoreboard_selection_wcsco, num_rows);
            }
            return;
        }
    }
}

// 0x00911920 (:1171)
// Up/down move the selection, past a page's end onto the next page; the shoulders cycle the challenge maps' boards,
// the triggers page, Y changes the filter, X goes to my rank. With nothing pending (and no move this frame): the
// highlight, the gamer card, back.
static void wc_scoreboard_process_loaded()
{
    process_stats_request_wcsco();
    wc_scoreboard_process_data_items();
    if (Match_scoreboard_selection_wcsco == -1 && Mp_match_scoreboard_info_wcsco.group_item_handles.size() > 0)
    {
        Match_scoreboard_selection_wcsco = 0;
    }
    Current_mouse_hint_choice_wcsco = WCSB_HINT_INVALID;
    bool events_consumed = false;
    scoreboard_input_process_mouse_wcsco(events_consumed);
    const int input_id = Mp_match_scoreboard_info_wcsco.input_id;
    // Magic numbers: the button indices of ui_get_button_info (0x20 the row button, 0xF Y, 0xE X, 1/5 up, 2/6 down, 0xB
    // back, 0x13/0x11 R2/L2, 0x12/0x10 R1/L1). No mouse wheel here (the multiplayer leaderboards have it).
    bool view_card = ui_get_button_info(0x20, input_id) > 0.0f;
    bool change_filter = ui_get_button_info(0xF, input_id) > 0.0f;
    bool goto_me = ui_get_button_info(0xE, input_id) > 0.0f;
    const bool up = ui_get_button_info(1, input_id) > 0.0f || ui_get_button_info(5, input_id) > 0.0f;
    const bool down = ui_get_button_info(2, input_id) > 0.0f || ui_get_button_info(6, input_id) > 0.0f;
    bool back = ui_get_button_info(0xB, input_id) > 0.0f;
    bool page_down = ui_get_button_info(0x13, input_id) > 0.0f;
    bool page_up = ui_get_button_info(0x11, input_id) > 0.0f;
    bool cycle_right = ui_get_button_info(0x12, input_id) > 0.0f;
    bool cycle_left = ui_get_button_info(0x10, input_id) > 0.0f;
    back = back || Current_mouse_hint_choice_wcsco == WCSB_HINT_ABORT;
    cycle_left = cycle_left || Current_mouse_hint_choice_wcsco == WCSB_HINT_CYCLE_LEADERBOARD_LEFT;
    cycle_right = cycle_right || Current_mouse_hint_choice_wcsco == WCSB_HINT_CYCLE_LEADERBOARD_RIGHT;
    change_filter = change_filter || Current_mouse_hint_choice_wcsco == WCSB_HINT_CHANGE_FILTER;
    goto_me = goto_me || Current_mouse_hint_choice_wcsco == WCSB_HINT_GOTO_ME;
    view_card = view_card || Current_mouse_hint_choice_wcsco == WCSB_HINT_VIEW_GAMERCARD;
    const int num_rows = Mp_match_scoreboard_info_wcsco.group_item_handles.size();
    if (Match_scoreboard_selection_wcsco == -1 && num_rows > 0)
    {
        Match_scoreboard_selection_wcsco = 0;
    }
    // A move skips the highlight, the gamer card and back on this frame.
    bool moved = false;
    // Magic numbers: Audio_info[1] up, [0] down, [11] no move, [10] next, [9] previous, [4] select, [5] back.
    if (up)
    {
        moved = true;
        page_down = false;
        if (Match_scoreboard_selection_wcsco == 0)
        {
            // The last row of the previous page.
            page_up = true;
            Match_scoreboard_selection_wcsco = scoreboard_page_size() - 1;
        }
        else
        {
            page_up = false;
            const int old_selection = Match_scoreboard_selection_wcsco;
            Match_scoreboard_selection_wcsco--;
            // Original: the selection always changes here, the "no move" sound is never played.
            audiolib_cue_play(Audio_info[Match_scoreboard_selection_wcsco == old_selection ? 11 : 1], nullptr, nullptr);
        }
    }
    else if (down)
    {
        moved = true;
        page_up = false;
        const int old_selection = Match_scoreboard_selection_wcsco;
        if (Match_scoreboard_selection_wcsco + 1 == num_rows)
        {
            // The first row of the next page.
            Match_scoreboard_selection_wcsco = 0;
            page_down = true;
        }
        else
        {
            page_down = false;
            Match_scoreboard_selection_wcsco++;
            audiolib_cue_play(Audio_info[Match_scoreboard_selection_wcsco == old_selection ? 11 : 0], nullptr, nullptr);
        }
    }
    const int num_modes = wrecking_crew_get_num_challenge_modes();
    if (cycle_right)
    {
        // The next map with a challenge. Original bugs: map indices are stepped modulo the number of challenge modes, and
        // the loop never ends when no map has a challenge.
        int scoreboard = (Current_wc_scoreboard + 1) % num_modes;
        while (wrecking_crew_get_map_challenge_id(scoreboard) == -1)
        {
            scoreboard = (scoreboard + 1) % num_modes;
        }
        if (scoreboard != Current_wc_scoreboard)
        {
            change_request_wcscb(scoreboard, FILTER_WCSCO_NONE, -1, false);
        }
        audiolib_cue_play(Audio_info[10], nullptr, nullptr);
    }
    else if (cycle_left)
    {
        int scoreboard = (Current_wc_scoreboard + num_modes - 1) % num_modes;
        while (wrecking_crew_get_map_challenge_id(scoreboard) == -1)
        {
            scoreboard = (num_modes - 1 + scoreboard) % num_modes;
        }
        if (scoreboard != Current_wc_scoreboard)
        {
            change_request_wcscb(scoreboard, FILTER_WCSCO_NONE, -1, false);
        }
        audiolib_cue_play(Audio_info[9], nullptr, nullptr);
    }
    else if (page_down && Starting_index_wcsco != 0)
    {
        int start = Starting_index_wcsco + scoreboard_page_size();
        // Past the end: back to the first page.
        if ((Current_filter_mode_wcsco == FILTER_WCSCO_NONE && start > Scoreboard_results_wcsco.total_entries_on_scoreboard) || (Current_filter_mode_wcsco == FILTER_WCSCO_FRIENDS && start > Friend_count_wcsco))
        {
            start = 1;
        }
        if (start != Starting_index_wcsco)
        {
            change_request_wcscb(Current_wc_scoreboard, Current_filter_mode_wcsco, start, false);
            audiolib_cue_play(Audio_info[10], nullptr, nullptr);
        }
    }
    else if (page_up && Starting_index_wcsco != 0)
    {
        const int page = scoreboard_page_size();
        int start = Starting_index_wcsco - page;
        if (start < 1)
        {
            // The last page's start.
            const int count = Current_filter_mode_wcsco == FILTER_WCSCO_FRIENDS ? Friend_count_wcsco : Scoreboard_results_wcsco.total_entries_on_scoreboard;
            start = (count - 1) + (1 - (count - 1) % page);
        }
        if (start != Starting_index_wcsco)
        {
            change_request_wcscb(Current_wc_scoreboard, Current_filter_mode_wcsco, start, false);
            audiolib_cue_play(Audio_info[9], nullptr, nullptr);
        }
    }
    else if (change_filter)
    {
        change_request_wcscb(Current_wc_scoreboard, Current_filter_mode_wcsco == FILTER_WCSCO_FRIENDS ? FILTER_WCSCO_NONE : FILTER_WCSCO_FRIENDS, -1, false);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    else if (goto_me && (Current_filter_mode_wcsco == FILTER_WCSCO_FRIENDS || My_leaderboard_state_wcsco == MY_LEADERBOARD_WCSCO_EXISTS))
    {
        change_request_wcscb(Current_wc_scoreboard, Current_filter_mode_wcsco, get_my_starting_index_wcscb(), true);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    if (moved || request_pending_wcscb())
    {
        return;
    }
    if (num_rows < 1)
    {
        Match_scoreboard_selection_wcsco = -1;
    }
    else
    {
        Match_scoreboard_selection_wcsco %= num_rows;
    }
    vint_data_item* highlight = vint_data_item::find((unsigned int)Mp_match_scoreboard_info_wcsco.data_item_handles[0]);
    if (highlight != nullptr)
    {
        highlight->set_element(0, vint_variant(scoreboard_page_rows() < 1 ? 0 : Match_scoreboard_selection_wcsco + 1));
    }
    if (view_card && scoreboard_page_rows() > 0)
    {
        const scoreboard_entry& entry = Scoreboard_results_wcsco.entries[scoreboard_page_offset() + Match_scoreboard_selection_wcsco];
        multi_player_menu_system_pull_up_player_option_message_box(entry.player_uid, entry.player_name, true);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
        return;
    }
    if (back)
    {
        Mp_match_scoreboard_info_wcsco.exit_action = MMEA_WCSCO_POP;
        scoreboard_set_state(MMS_WCSCO_UNLOADING);
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
}

// 0x0089D980 (:1365)
static void scoreboard_ui_pop_wcsco()
{
    Mp_match_scoreboard_info_wcsco.exit_action = MMEA_WCSCO_POP;
    Mp_match_scoreboard_info_wcsco.pending_state = MMS_WCSCO_UNLOADING;
}

// ---- external interface

// 0x00917020 (:1377)
void ui_document_menu_wrecking_crew_scoreboard_init()
{
    state_info info;
    info.enter = ui_document_menu_wrecking_crew_scoreboard_enter;
    info.exit = ui_document_menu_wrecking_crew_scoreboard_exit;
    info.process = ui_document_menu_wrecking_crew_scoreboard_process;
    info.handshake = ui_document_menu_wrecking_crew_scoreboard_handshake;
    info.render = ui_document_menu_wrecking_crew_scoreboard_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_WRECKING_CREW_SCOREBOARD, info);
}

// 0x009074F0 (:1394)
// The pool, the document, a control id and a locking input id; the selected map's board (map 0 when it has no challenge).
void ui_document_menu_wrecking_crew_scoreboard_enter(game_state prev_state)
{
    // Magic number: 1 MB of level memory for the results, the friend uids and the retrieval scratch.
    void* cpu_mem = Level_memmgr.alloc(0x100000, "WC memory CPU", 16);
    WC_scoreboard_mempool.init_rfg_split_mempool("WC mempool", cpu_mem, 0x100000, 4, nullptr, 0, 4, 8);
    scoreboard_init_state_wcsco();
    scoreboard_init_info_wcsco();
    Mp_match_scoreboard_info_wcsco.doc_handle = ui_load_document(Ui_mp_shell_name_wcsco, DFIT_NONE, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Mp_match_scoreboard_info_wcsco.control_id = ui_request_control_id();
    Mp_match_scoreboard_info_wcsco.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, true);
    // Magic number: room for 101 entries (a page is 16; the rest for a friends board).
    Scoreboard_results_wcsco.max_entries = 101;
    Scoreboard_results_wcsco.entries = (scoreboard_entry*)WC_scoreboard_mempool.static_mempool_base::alloc(101 * sizeof(scoreboard_entry), 0);
    Scoreboard_friend_uid_list_wcsco = (multi_player_uid*)WC_scoreboard_mempool.static_mempool_base::alloc(101 * sizeof(multi_player_uid), 0);
    // Magic number: 100 KB of scratch for the retrieval.
    Scoreboard_scratch_wcsco = (unsigned char*)WC_scoreboard_mempool.static_mempool_base::alloc(0x19000, 0);
    Request_type_wcsco = REQUEST_TYPE_WCSCO_NONE;
    Current_wc_scoreboard = Wrecking_crew_options.map_index;
    if (wrecking_crew_get_map_challenge_id(Wrecking_crew_options.map_index) == -1)
    {
        Current_wc_scoreboard = 0;
    }
    // Original bug: the board is the map index + 4 here, the challenge's leaderboard id + 4 once cycled
    // (change_request_wcscb), and map 0 is taken without checking it has a challenge.
    WC_scoreboard_info.scoreboard_index = Current_wc_scoreboard + 4;
    Starting_index_wcsco = -1;
    My_leaderboard_state_wcsco = MY_LEADERBOARD_WCSCO_UNKNOWN;
    Current_filter_mode_wcsco = FILTER_WCSCO_NONE;
    Requested_filter_mode_wcsco = FILTER_WCSCO_NONE;
    Friend_count_wcsco = 0;
    Goto_me_wcsco = false;
    // Magic number: 50 friend queries at once.
    multi_game_friend_init(&WC_scoreboard_mempool, 0x32);
}

// 0x008EEE20 (:1456)
void ui_document_menu_wrecking_crew_scoreboard_exit(game_state new_state)
{
    hide_loading_msgbox_wcsco();
    multi_core_platform_scoreboard_cancel_retrieve();
    multi_game_friend_cancel_all();
    Scoreboard_results_wcsco.max_entries = 0;
    Scoreboard_results_wcsco.entries = nullptr;
    Scoreboard_scratch_wcsco = nullptr;
    Scoreboard_friend_uid_list_wcsco = nullptr;
    scoreboard_unload_ui_information_wcsco();
    ui_relinquish_control_id(Mp_match_scoreboard_info_wcsco.control_id);
    ui_unload_document(Mp_match_scoreboard_info_wcsco.doc_handle, DFOT_NONE, 0.5f);
    Mp_match_scoreboard_info_wcsco.doc_handle = -1;
    ui_relinquish_input_id(Mp_match_scoreboard_info_wcsco.input_id);
    multi_game_lobby_clear_pushed_to_game();
    multi_game_friend_shutdown();
    Level_memmgr.free(WC_scoreboard_mempool.static_mempool_base::get_base());
    WC_scoreboard_mempool.split_mempool::clear();
}

// 0x00912270 (:1495)
void ui_document_menu_wrecking_crew_scoreboard_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    multi_game_friend_process();
    ui_add_for_processing(Mp_match_scoreboard_info_wcsco.doc_handle);
    switch (Mp_match_scoreboard_info_wcsco.state)
    {
    case MMS_WCSCO_LOADING:
        wc_scoreboard_process_loading();
        break;
    case MMS_WCSCO_LOADED:
        wc_scoreboard_process_loaded();
        break;
    case MMS_WCSCO_UNLOADING:
        scoreboard_unload_ui_information_wcsco();
        hide_loading_msgbox_wcsco();
        if (Mp_match_scoreboard_info_wcsco.exit_action == MMEA_WCSCO_POP)
        {
            gameseq_pop_state();
        }
        else if (Mp_match_scoreboard_info_wcsco.exit_action == MMEA_WCSCO_PUSH)
        {
            gameseq_push_state(Mp_match_scoreboard_info_wcsco.exit_push_target, true, true);
        }
        scoreboard_set_state(MMS_WCSCO_UNLOADED);
        break;
    case MMS_WCSCO_UNLOADED:
        scoreboard_set_state(MMS_WCSCO_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_match_scoreboard_info_wcsco.control_id, false);
}

// 0x008DBA70 (:1549)
void ui_document_menu_wrecking_crew_scoreboard_handshake(rl_renderer*)
{
    Mp_match_scoreboard_info_wcsco.prev_state = Mp_match_scoreboard_info_wcsco.state;
    if (Mp_match_scoreboard_info_wcsco.pending_state != Mp_match_scoreboard_info_wcsco.state)
    {
        scoreboard_do_state_change_wcsco();
    }
}

// 0x008F6AA0 (:1557)
// Drawn once loaded for a whole frame.
void ui_document_menu_wrecking_crew_scoreboard_render(rl_renderer*)
{
    if (Mp_match_scoreboard_info_wcsco.doc_handle == -1 || Mp_match_scoreboard_info_wcsco.control_id == 0)
    {
        return;
    }
    if (Mp_match_scoreboard_info_wcsco.state == MMS_WCSCO_LOADED && Mp_match_scoreboard_info_wcsco.prev_state == MMS_WCSCO_LOADED)
    {
        ui_add_for_rendering(Mp_match_scoreboard_info_wcsco.doc_handle);
    }
    ui_render(Mp_match_scoreboard_info_wcsco.control_id, false);
}
