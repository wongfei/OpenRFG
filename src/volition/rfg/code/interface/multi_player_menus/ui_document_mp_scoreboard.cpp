// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.cpp
//
// The Steam leaderboards (GS_MULTIPLAYER_SCOREBOARD, stub_impl.md S7) on rfg_ui_mp_leaderboard: four boards of 16 rows a
// page, filtered to everyone or friends. Retrieval runs through multi_core_platform_scoreboard_* with a loading box up.
//
// Original bugs kept (commented): compare results are not a consistent ordering for a player on equal rank; a board change
// ignores the filter passed in; a failed data item create reuses the previous handle.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
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
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

// ---- file-private types (rfg.pdb)
enum filter_mode
{
    FILTER_NONE = 0,
    FILTER_FRIENDS = 1,
};

enum mp_menu_exit_actions_score
{
    MMEA_S_POP = 0,
    MMEA_S_PUSH = 1,
    NUM_MP_S_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_score
{
    MMS_S_LOADING = 0,
    MMS_S_LOADED = 1,
    MMS_S_UNLOADING = 2,
    MMS_S_UNLOADED = 3,
    NUM_MP_S_MENU_STATES = 4,
};

// size 0x8C
struct mp_match_scoreboard_shell_info
{

    mp_menu_states_score state; // 0x00
    mp_menu_states_score pending_state; // 0x04
    mp_menu_states_score prev_state; // 0x08
    mp_menu_exit_actions_score exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_item_handles[6]; // 0x18
    int data_group_handle; // 0x30
    farray<int,16> group_item_handles; // 0x34
    unsigned int control_id; // 0x84
    int input_id; // 0x88
};
static_assert(sizeof(mp_match_scoreboard_shell_info) == 0x8C, "mp_match_scoreboard_shell_info");

enum my_leaderboard_state
{
    MY_LEADERBOARD_UNKNOWN = 0,
    MY_LEADERBOARD_EXISTS = 1,
    MY_LEADERBOARD_DOESNT_EXIST = 2,
};

enum request_type
{
    REQUEST_TYPE_NONE = 0,
    REQUEST_TYPE_FRIENDS = 1,
    REQUEST_TYPE_BOARD = 2,
};

enum scoreboard_hint_choices
{
    MPSB_HINT_INVALID = -1,
    MPSB_HINT_ABORT = 0,
    MPSB_HINT_CYCLE_LEADERBOARD_LEFT = 1,
    MPSB_HINT_CYCLE_LEADERBOARD_RIGHT = 2,
    MPSB_HINT_CHANGE_FILTER = 3,
    MPSB_HINT_GOTO_ME = 4,
    MPSB_HINT_VIEW_GAMERCARD = 5,
    NUM_MPSB_HINTS = 6,
};

// ---- file-static globals
static const char* Ui_mp_shell_name_score = {"rfg_ui_mp_leaderboard"}; // 0x01663788
static const char* Menu_data_item_names_score[6] = {"LeaderboardHighlightIndex", "LeaderboardHintInfo", "LeaderboardTitles", "LeaderboardHeaderInfo", "LeaderboardModeInfo", "LeaderboardScoreInfo"}; // 0x0166378C
static const char* Mode_colon_string_score = {"MENU_MODE_COLON"}; // 0x016637A4
static const char* Map_colon_string_score = {"MENU_MAP_COLON"}; // 0x016637A8
static const char* Back_string_score = {"MENU_BACK"}; // 0x016637B0
static const char* Cycle_leaderboard_string = {"MENU_CYCLE_LEADERBOARD"}; // 0x016637B4
static int Starting_index = -1; // 0x016637BC
static const scoreboard_info Scoreboard_infos[4] = { // 0x016637C0
    {PST_XP, 0, (platform_game_type_context)0, {
        {PSCT_INT, "SCOREBOARD_TOTAL_XP"},
        {PSCT_INT, "SCOREBOARD_BONUSES"},
        {PSCT_FLOAT, "SCOREBOARD_XP_PER_GAME"},
        {PSCT_INT, "SCOREBOARD_HIGH_XP"},
    }},
    {PST_WINS, 1, (platform_game_type_context)0, {
        {PSCT_INT, "SCOREBOARD_WINS"},
        {PSCT_INT, "SCOREBOARD_LOSSES"},
        {PSCT_FLOAT, "SCOREBOARD_RATIO"},
        {PSCT_INT, "SCOREBOARD_STREAK"},
    }},
    {PST_KILLS, 2, (platform_game_type_context)0, {
        {PSCT_INT, "SCOREBOARD_KILLS"},
        {PSCT_INT, "SCOREBOARD_DEATHS"},
        {PSCT_INT, "SCOREBOARD_HIGH_KILLS"},
        {PSCT_INT, "SCOREBOARD_HIGH_DEATHS"},
    }},
    {PST_DESTRUCTION, 3, (platform_game_type_context)0, {
        {PSCT_INT, "SCOREBOARD_DAMAGE_DONE"},
        {PSCT_INT, "SCOREBOARD_REPAIR_DONE"},
        {PSCT_FLOAT, "SCOREBOARD_DAMAGE_PER_GAME"},
        {PSCT_INT, "SCOREBOARD_HIGH_DAMAGE"},
    }},
};
static int Msgbox_handle_score = -1; // 0x01663C30
static scoreboard_hint_choices Current_mouse_hint_choice_score = MPSB_HINT_INVALID; // 0x01663C34
static mp_match_scoreboard_shell_info Mp_match_scoreboard_info; // 0x01666990
static const char* kHintViewCard = {"row_highlight_button"}; // 0x01668658
static const char* kHintGroup5 = {"hint_5_group"}; // 0x0166865C
static const char* kHintGroup4 = {"hint_4_group"}; // 0x01668660
static const char* kHintGroup3 = {"hint_3_group"}; // 0x01668664
static const char* kHintGroup2 = {"hint_2_group"}; // 0x01668668
static const char* kHintGroup1 = {"hint_1_group"}; // 0x0166866C
static const char* kGroup_name = {"row_group"}; // 0x01668670
static int Delayed_selection = -1; // 0x01668708
static scoreboard_results Scoreboard_results; // 0x02BE7D80
static int scoreboard_hint_config[6]; // 0x02C00E54
static bool Goto_me; // 0x02C03641
static int Match_scoreboard_selection; // 0x02C03648
static int Current_scoreboard; // 0x02C0364C
static multi_player_uid* Scoreboard_friend_uid_list; // 0x02C03650
static int Friend_count; // 0x02C03654
static int Used_starting_index; // 0x02C03658
static unsigned char* Scoreboard_scratch; // 0x02C0365C
static request_type Request_type; // 0x02C03660
static my_leaderboard_state My_leaderboard_state; // 0x02C03664
static int My_rank; // 0x02C03668
static filter_mode Requested_filter_mode; // 0x02C0366C
static filter_mode Current_filter_mode; // 0x02C03670
static int scoreboard_num_hints; // 0x02C03674

// ---- internal functions

// Port helper: the page's first entry in the results (a friends board has them all, the others only the page); inlined.
static int scoreboard_page_offset()
{
    return Current_filter_mode == FILTER_FRIENDS ? Starting_index - 1 : 0;
}

// Port helper: the page's rows in the results; inlined.
static int scoreboard_page_rows()
{
    const int rows = Scoreboard_results.num_entries - scoreboard_page_offset();
    return rows > 15 ? 16 : rows;
}

// 0x008AE3C0 (:278)
static bool loading_box_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (choice != MBC_ABORT && choice != MBC_FORCED_EXIT)
    {
        return false;
    }
    multi_core_platform_scoreboard_cancel_retrieve();
    multi_game_friend_cancel_all();
    Msgbox_handle_score = -1;
    Mp_match_scoreboard_info.exit_action = MMEA_S_POP;
    Mp_match_scoreboard_info.pending_state = MMS_S_UNLOADING;
    return true;
}

// 0x0088D700 (:292)
// The start of my rank's page. Original quirk: a rank right after a page boundary gets one less (rank 17 gives 16).
static int get_my_starting_index()
{
    const int rank = My_rank - 1;
    return rank / 16 * 16 + (rank % 16 != 0 ? 1 : 0);
}

// 0x0088D730 (:305)
static bool request_pending()
{
    return Request_type != REQUEST_TYPE_NONE || Starting_index < 0 || Requested_filter_mode != Current_filter_mode || My_leaderboard_state == MY_LEADERBOARD_UNKNOWN;
}

// 0x008AE400 (:313)
static void select_my_player()
{
    for (int i = 0; i < scoreboard_page_rows(); i++)
    {
        if (Scoreboard_results.entries[scoreboard_page_offset() + i].player_uid.is_equal(My_player_uid))
        {
            Match_scoreboard_selection = i;
            return;
        }
    }
}

// 0x0088D770 (:334)
static void change_request(int scoreboard, filter_mode filter, int starting_index, bool goto_me)
{
    if (Current_scoreboard == scoreboard)
    {
        if (filter == Current_filter_mode && filter == Requested_filter_mode)
        {
            Starting_index = starting_index;
            Goto_me = goto_me;
            return;
        }
        Requested_filter_mode = filter;
    }
    else
    {
        // Original bug: a board change ignores the filter (the callers pass none): the request's filter stays.
        Current_filter_mode = Requested_filter_mode;
        Current_scoreboard = scoreboard;
    }
    Starting_index = -1;
    Friend_count = 0;
    My_leaderboard_state = MY_LEADERBOARD_UNKNOWN;
    Goto_me = false;
}

// 0x008FC170 (:357)
static void show_loading_msgbox()
{
    if (Msgbox_handle_score == -1)
    {
        Msgbox_handle_score = ui_add_msgbox(MBT_LOADING_CANCEL, "MENU_MSJS_JOIN_IN_PROGRESS", "", loading_box_cb, false, false, nullptr, nullptr, false);
    }
}

// 0x008E4760 (:364)
static void hide_loading_msgbox()
{
    if (Msgbox_handle_score != -1)
    {
        ui_hide_msgbox(Msgbox_handle_score, false);
        Msgbox_handle_score = -1;
    }
}

// 0x0088D7E0 (:374)
// By rank, the unranked after them: first me, then by name.
static int sort_friends(const void* entry1p, const void* entry2p)
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

// 0x008FC4E0 (:403)
static void process_stats_request()
{
    if (Request_type == REQUEST_TYPE_NONE)
    {
        // My own row comes first, to know the rank (for friends that is the whole list).
        scoreboard_request request;
        bool retrieve = false;
        const scoreboard_info* info = &Scoreboard_infos[Current_scoreboard];
        if (Requested_filter_mode == FILTER_FRIENDS)
        {
            if (My_leaderboard_state == MY_LEADERBOARD_UNKNOWN)
            {
                // Magic number: 16 uids, the page.
                request.set_uids(info, &My_player_uid, 16, Scoreboard_scratch);
                request.request_type = SRT_FRIENDS;
                retrieve = true;
            }
        }
        else if (My_leaderboard_state == MY_LEADERBOARD_UNKNOWN)
        {
            request.set_uids(info, &My_player_uid, 1, Scoreboard_scratch);
            request.request_type = SRT_PIVOT_PLAYER;
            retrieve = true;
        }
        else if (Starting_index == -1)
        {
            request.set(info, 1, 16, Scoreboard_scratch);
            retrieve = true;
        }
        else if (Scoreboard_results.num_entries >= 1 && (Starting_index < Scoreboard_results.entries[0].rank || Scoreboard_results.entries[Scoreboard_results.num_entries - 1].rank < Starting_index))
        {
            // The page isn't in the results yet.
            if (Scoreboard_results.total_entries_on_scoreboard < Starting_index || Starting_index < 2)
            {
                Starting_index = 1;
            }
            request.set(info, Starting_index, 16, Scoreboard_scratch);
            retrieve = true;
        }
        if (retrieve)
        {
            multi_core_platform_scoreboard_retrieve_data(&Scoreboard_results, &request);
            Request_type = REQUEST_TYPE_BOARD;
        }
    }
    if (Request_type == REQUEST_TYPE_BOARD)
    {
        const platform_op_status status = multi_core_platform_scoreboard_retrieve_state();
        if (status == POS_SUCCESS)
        {
            if (Requested_filter_mode == FILTER_FRIENDS)
            {
                if (Scoreboard_results.num_entries < 1)
                {
                    My_leaderboard_state = MY_LEADERBOARD_DOESNT_EXIST;
                    My_rank = 0;
                }
                else
                {
                    Friend_count = Scoreboard_results.num_entries;
                    Scoreboard_results.total_entries_on_scoreboard = Scoreboard_results.num_entries;
                    My_leaderboard_state = MY_LEADERBOARD_EXISTS;
                    qsort(Scoreboard_results.entries, Scoreboard_results.num_entries, sizeof(scoreboard_entry), sort_friends);
                    for (int i = 0; i < Scoreboard_results.num_entries; i++)
                    {
                        if (My_player_uid.is_equal(Scoreboard_results.entries[i].player_uid))
                        {
                            My_rank = i + 1;
                            break;
                        }
                    }
                }
                Starting_index = 1;
            }
            else if (My_leaderboard_state == MY_LEADERBOARD_UNKNOWN)
            {
                if (Scoreboard_results.num_entries < 1 || Scoreboard_results.entries[0].rank < 1)
                {
                    My_leaderboard_state = MY_LEADERBOARD_DOESNT_EXIST;
                }
                else
                {
                    My_leaderboard_state = MY_LEADERBOARD_EXISTS;
                    My_rank = Scoreboard_results.entries[0].rank;
                }
            }
            else if (Scoreboard_results.num_entries < 1)
            {
                Starting_index = 0;
                Used_starting_index = 0;
            }
            else if (Starting_index == -1)
            {
                Starting_index = Scoreboard_results.entries[0].rank;
            }
            Current_filter_mode = Requested_filter_mode;
            Request_type = REQUEST_TYPE_NONE;
        }
        else if (status == POS_FAILED)
        {
            if (My_leaderboard_state == MY_LEADERBOARD_UNKNOWN)
            {
                My_leaderboard_state = MY_LEADERBOARD_DOESNT_EXIST;
            }
            else
            {
                Starting_index = 0;
                Used_starting_index = 0;
            }
            Scoreboard_results.num_entries = 0;
            Request_type = REQUEST_TYPE_NONE;
        }
    }
    if (!request_pending())
    {
        hide_loading_msgbox();
        if (Goto_me)
        {
            select_my_player();
            Goto_me = false;
        }
    }
    else
    {
        show_loading_msgbox();
    }
}

// 0x008D3E70 (:628)
static void scoreboard_load_global_data_items()
{
    // Original bug: a failed create keeps the previous item's handle (0 for the first).
    unsigned int handle = 0;
    for (int i = 0; i < 6; i++)
    {
        vint_data_item* item = vint_data_item::create_new(Menu_data_item_names_score[i]);
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
                item->set_element(1, vint_variant(Mode_colon_string_score));
                item->set_element(2, vint_variant("MODE NAME"));
                item->set_element(3, vint_variant(Map_colon_string_score));
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
        Mp_match_scoreboard_info.data_item_handles[i] = (int)handle;
        ui_add_data_item(Mp_match_scoreboard_info.doc_handle, (int)handle);
    }
}

// 0x008C1730 (:734)
static void scoreboard_add_menu_option_data_item()
{
    vint_data_item* item = vint_data_group::find(Mp_match_scoreboard_info.data_group_handle)->add_data_item(nullptr);
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
        Mp_match_scoreboard_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008AE4C0 (:761)
static void scoreboard_remove_menu_option_data_item()
{
    vint_data_group* group = vint_data_group::find(Mp_match_scoreboard_info.data_group_handle);
    const int last = Mp_match_scoreboard_info.group_item_handles.size() - 1;
    group->remove_data_item(Mp_match_scoreboard_info.group_item_handles[last]);
    Mp_match_scoreboard_info.group_item_handles.resize(last);
}

// 0x008D4120 (:771) inlined
static void scoreboard_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("LeaderboardSetData");
    if (group != nullptr)
    {
        Mp_match_scoreboard_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_match_scoreboard_info.doc_handle, Mp_match_scoreboard_info.data_group_handle);
    }
}

// 0x008D4150 (:805)
static void scoreboard_load_ui_information()
{
    scoreboard_load_global_data_items();
    scoreboard_load_data_groups();
}

// 0x008E4780 (:558)
static void scoreboard_do_state_change()
{
    do
    {
        Mp_match_scoreboard_info.state = Mp_match_scoreboard_info.pending_state;
        if (Mp_match_scoreboard_info.pending_state == MMS_S_LOADED)
        {
            scoreboard_load_ui_information();
        }
        else if (Mp_match_scoreboard_info.pending_state == MMS_S_UNLOADING)
        {
            multi_core_platform_scoreboard_cancel_retrieve();
            multi_game_friend_cancel_all();
        }
        else
        {
            return;
        }
    }
    while (Mp_match_scoreboard_info.state != Mp_match_scoreboard_info.pending_state);
}

// 0x0088D850 (:594)
static void scoreboard_set_state(mp_menu_states_score new_state)
{
    Mp_match_scoreboard_info.pending_state = new_state;
}

// 0x0088D860 (:603) inlined
static void scoreboard_init_state()
{
    Mp_match_scoreboard_info.state = MMS_S_UNLOADED;
    Mp_match_scoreboard_info.pending_state = MMS_S_UNLOADED;
    Mp_match_scoreboard_info.prev_state = MMS_S_UNLOADED;
    Mp_match_scoreboard_info.exit_action = MMEA_S_POP;
    Mp_match_scoreboard_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AE480 (:615) inlined
static void scoreboard_init_info()
{
    Mp_match_scoreboard_info.group_item_handles.clear();
    for (int i = 0; i < 6; i++)
    {
        Mp_match_scoreboard_info.data_item_handles[i] = -1;
    }
    Mp_match_scoreboard_info.data_group_handle = -1;
}

// 0x008E47E0 (:815)
static void scoreboard_unload_ui_information()
{
    vint_doc_info* info = ui_get_doc_info(Mp_match_scoreboard_info.doc_handle);
    if (info != nullptr)
    {
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    scoreboard_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x008E4870 (:824)
static void scoreboard_process_data_item_hint_text(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info.data_item_handles[1]);
    if (item == nullptr)
    {
        return;
    }
    for (int i = 1; i < 6; i++)
    {
        scoreboard_hint_config[i] = MPSB_HINT_INVALID;
    }
    multi_player_menu_system_begin_hints(item);
    scoreboard_hint_config[0] = MPSB_HINT_ABORT;
    scoreboard_num_hints = 1;
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Back_string_score), CBA_MENU_ABORT);
    multi_player_menu_system_add_hint_config(scoreboard_num_hints, scoreboard_hint_config, MPSB_HINT_CYCLE_LEADERBOARD_LEFT);
    // The left cycle button has no text of its own, the right one's stands for both.
    multi_player_menu_system_add_hint(localization_get_display_string_opt(""), CBA_MENU_L1);
    multi_player_menu_system_add_hint_config(scoreboard_num_hints, scoreboard_hint_config, MPSB_HINT_CYCLE_LEADERBOARD_RIGHT);
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Cycle_leaderboard_string), CBA_MENU_R1);
    multi_player_menu_system_add_hint(localization_get_display_string_opt("MENU_CHANGE_FILTER"), CBA_MENU_Y);
    multi_player_menu_system_add_hint_config(scoreboard_num_hints, scoreboard_hint_config, MPSB_HINT_CHANGE_FILTER);
    if (!request_pending() && (My_leaderboard_state == MY_LEADERBOARD_EXISTS || (Current_filter_mode == FILTER_FRIENDS && Scoreboard_results.num_entries > 0)))
    {
        multi_player_menu_system_add_hint(localization_get_display_string_req("MENU_GOTO_ME", nullptr), CBA_MENU_X);
        multi_player_menu_system_add_hint_config(scoreboard_num_hints, scoreboard_hint_config, MPSB_HINT_GOTO_ME);
    }
    multi_player_menu_system_add_hint_config(scoreboard_num_hints, scoreboard_hint_config, MPSB_HINT_VIEW_GAMERCARD);
    multi_player_menu_system_end_hints();
}

// 0x008E4A90 (:876)
static void scoreboard_process_data_item_titles(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info.data_item_handles[2]);
    if (item == nullptr)
    {
        return;
    }
    wchar_t text[128];
    char encoded[384];
    const wchar_t* filter_name = localization_get_display_string_opt(Current_filter_mode == FILTER_FRIENDS ? "MENU_SIT_FRIENDS" : "MENU_SIT_ALL");
    const char* board_tag = multi_core_platform_scoreboard_get_display_name(Scoreboard_infos[Current_scoreboard].scoreboard_type, Scoreboard_infos[Current_scoreboard].scoreboard_context);
    const wchar_t* board_name = board_tag != nullptr ? localization_get_display_string_opt(board_tag) : nullptr;
    _snwprintf_safe(text, 128, L"%ls: %ls", board_name, filter_name);
    vint_encode_wide_text(encoded, 384, text);
    item->set_element(0, vint_variant(encoded));
    bool has_rank = false;
    if (Scoreboard_results.num_entries > 0 && Match_scoreboard_selection >= 0 && !request_pending())
    {
        if (Match_scoreboard_selection >= scoreboard_page_rows())
        {
            Match_scoreboard_selection = scoreboard_page_rows() - 1;
        }
        const int rank = Scoreboard_results.entries[scoreboard_page_offset() + Match_scoreboard_selection].rank;
        if (rank > 0)
        {
            _snwprintf_safe(text, 128, L"%ls: %i", localization_get_display_string_opt("MENU_RANK"), rank);
            has_rank = true;
        }
    }
    if (!has_rank)
    {
        _snwprintf_safe(text, 128, L"%ls: --", localization_get_display_string_opt("MENU_RANK"));
    }
    vint_encode_wide_text(encoded, 384, text);
    item->set_element(1, vint_variant(encoded));
}

// 0x0088D890 (:919)
static void scoreboard_process_data_item_info_text(float dtime)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info.data_item_handles[3]);
    if (item != nullptr)
    {
        item->set_element(0, vint_variant("PLT_GAMERTAG"));
        for (int i = 0; i < 4; i++)
        {
            item->set_element(i + 1, vint_variant(Scoreboard_infos[Current_scoreboard].column_info[i].display_name));
        }
    }
}

// 0x008C17F0 (:935)
static void scoreboard_process_data_group_menu_options(float dtime)
{
    if (request_pending())
    {
        return;
    }
    vint_data_group* group = vint_data_group::find(Mp_match_scoreboard_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    char text[256];
    int num_rows = group->get_num_data_items();
    if (scoreboard_page_rows() == 0)
    {
        // A single row with the message.
        if (num_rows == 0)
        {
            scoreboard_add_menu_option_data_item();
            num_rows = 1;
        }
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info.group_item_handles[0]);
        if (item != nullptr)
        {
            item->set_element(0, vint_variant("MENU_NO_LEADERBOARD_RESULTS"));
            item->set_element(1, vint_variant(false));
            vint_encode_wide_text(text, 256, L"");
            for (int column = 0; column < 4; column++)
            {
                item->set_element(column + 2, vint_variant(text));
            }
        }
        for (; num_rows > 1; num_rows--)
        {
            scoreboard_remove_menu_option_data_item();
        }
        return;
    }
    for (; num_rows < scoreboard_page_rows(); num_rows++)
    {
        scoreboard_add_menu_option_data_item();
    }
    for (int i = 0; i < scoreboard_page_rows(); i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_match_scoreboard_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        const scoreboard_entry& entry = Scoreboard_results.entries[scoreboard_page_offset() + i];
        vint_encode_wide_text(text, 256, entry.player_name);
        item->set_element(0, vint_variant(text));
        item->set_element(1, vint_variant(i == Match_scoreboard_selection));
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
            item->set_element(column + 2, vint_variant(text));
        }
    }
    for (; num_rows > scoreboard_page_rows(); num_rows--)
    {
        scoreboard_remove_menu_option_data_item();
    }
}

// 0x008F3230 (:1037)
static void scoreboard_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_match_scoreboard_info.doc_handle))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_scoreboard.cpp", 1037);
        ui_unload_document(Mp_match_scoreboard_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_match_scoreboard_info.doc_handle = -1;
        scoreboard_set_state(MMS_S_UNLOADING);
        break;
    case ST_ERROR:
        ui_unload_document(Mp_match_scoreboard_info.doc_handle, DFOT_NONE, 0.5f);
        Mp_match_scoreboard_info.doc_handle = -1;
        scoreboard_set_state(MMS_S_UNLOADING);
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        scoreboard_set_state(MMS_S_LOADED);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_scoreboard.cpp", 1037);
        break;
    }
}

// 0x008D4180 (:1107)
static void scoreboard_process_mouse_hints()
{
    Current_mouse_hint_choice_score = MPSB_HINT_INVALID;
    // Only with the input focus: the lock, else the newest request.
    int focus = Current_input_lock_id;
    if (focus == -1)
    {
        focus = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    if (Mp_match_scoreboard_info.input_id != focus)
    {
        return;
    }
    const char* hints_1[3] = {kHintGroup1, kHintGroup2, kHintGroup3};
    const char* hints_2[3] = {kHintGroup4, kHintGroup5, kHintViewCard};
    const int hint_1 = ui_mouse_process_hints(hints_1, 3, Mp_match_scoreboard_info.doc_handle, true);
    const int hint_2 = ui_mouse_process_hints(hints_2, 3, Mp_match_scoreboard_info.doc_handle, true);
    if (hint_1 != -1)
    {
        Current_mouse_hint_choice_score = (scoreboard_hint_choices)scoreboard_hint_config[hint_1];
    }
    if (hint_2 != -1)
    {
        Current_mouse_hint_choice_score = (scoreboard_hint_choices)scoreboard_hint_config[hint_2 + 3];
    }
}

// 0x008D4260 (:1140)
static void scoreboard_input_process_mouse()
{
    scoreboard_process_mouse_hints();
    if (!mouse_just_moved() && !mouse_just_released(0))
    {
        return;
    }
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Mp_match_scoreboard_info.doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    const int num_rows = Mp_match_scoreboard_info.group_item_handles.size();
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
            if (i != Match_scoreboard_selection && mouse_just_released(0))
            {
                multi_player_menu_system_set_menu_selection(i, Match_scoreboard_selection, num_rows);
            }
            return;
        }
    }
}

// 0x0090DAD0 (:1175)
static void scoreboard_process_input()
{
    Current_mouse_hint_choice_score = MPSB_HINT_INVALID;
    const int num_rows = Mp_match_scoreboard_info.group_item_handles.size();
    if (Match_scoreboard_selection == -1 && num_rows > 0)
    {
        Match_scoreboard_selection = 0;
    }
    if (Delayed_selection != -1)
    {
        Match_scoreboard_selection = Delayed_selection;
        Delayed_selection = -1;
    }
    scoreboard_input_process_mouse();
    const int input_id = Mp_match_scoreboard_info.input_id;
    // Magic numbers: the button indices of ui_get_button_info (0x10/0x12 L1/R1, 0x11/0x13 L2/R2).
    const bool view_card = ui_get_button_info(0x20, input_id) > 0.0f || Current_mouse_hint_choice_score == MPSB_HINT_VIEW_GAMERCARD;
    const bool change_filter = ui_get_button_info(0xF, input_id) > 0.0f || Current_mouse_hint_choice_score == MPSB_HINT_CHANGE_FILTER;
    const bool goto_me = ui_get_button_info(0xE, input_id) > 0.0f || Current_mouse_hint_choice_score == MPSB_HINT_GOTO_ME;
    const bool back = ui_get_button_info(0xB, input_id) > 0.0f || Current_mouse_hint_choice_score == MPSB_HINT_ABORT;
    bool page_down = ui_get_button_info(0x13, input_id) > 0.0f;
    bool page_up = ui_get_button_info(0x11, input_id) > 0.0f;
    const bool cycle_right = ui_get_button_info(0x12, input_id) > 0.0f || Current_mouse_hint_choice_score == MPSB_HINT_CYCLE_LEADERBOARD_RIGHT;
    const bool cycle_left = ui_get_button_info(0x10, input_id) > 0.0f || Current_mouse_hint_choice_score == MPSB_HINT_CYCLE_LEADERBOARD_LEFT;
    int mouse_x;
    int mouse_y;
    int mouse_wheel;
    mouse_get_deltas(mouse_x, mouse_y, mouse_wheel);
    const bool up = ui_get_button_info(1, input_id) > 0.0f || ui_get_button_info(5, input_id) > 0.0f || mouse_wheel > 0;
    const bool down = ui_get_button_info(2, input_id) > 0.0f || ui_get_button_info(6, input_id) > 0.0f || mouse_wheel < 0;
    // Past a page's end the selection continues on the next page.
    if (up)
    {
        if (Match_scoreboard_selection == 0)
        {
            Match_scoreboard_selection = num_rows - 1;
            page_up = true;
            // Magic number: the last row of a page, applied when the previous page has loaded.
            Delayed_selection = 15;
            if (Starting_index - 16 < 0)
            {
                // Back around to the last page, which is shorter.
                if (Current_filter_mode == FILTER_NONE)
                {
                    Match_scoreboard_selection = Scoreboard_results.total_entries_on_scoreboard % 16;
                }
                else if (Current_filter_mode == FILTER_FRIENDS)
                {
                    Match_scoreboard_selection = Friend_count == 0 ? -1 : Scoreboard_results.total_entries_on_scoreboard % Friend_count;
                }
            }
        }
        else
        {
            multi_player_menu_system_set_menu_selection(Match_scoreboard_selection - 1, Match_scoreboard_selection, num_rows);
        }
    }
    else if (down)
    {
        if (Match_scoreboard_selection < num_rows - 1)
        {
            multi_player_menu_system_set_menu_selection(Match_scoreboard_selection + 1, Match_scoreboard_selection, num_rows);
        }
        else
        {
            page_down = true;
            Match_scoreboard_selection = 0;
        }
    }
    // Magic number: the four boards of Scoreboard_infos.
    if (cycle_right)
    {
        change_request((Current_scoreboard + 1) % 4, FILTER_NONE, -1, false);
        audiolib_cue_play(Audio_info[10], nullptr, nullptr);
    }
    else if (cycle_left)
    {
        change_request((Current_scoreboard + 3) % 4, FILTER_NONE, -1, false);
        audiolib_cue_play(Audio_info[9], nullptr, nullptr);
    }
    else if (page_down && Starting_index != 0)
    {
        int start = Starting_index + 16;
        if ((Current_filter_mode == FILTER_NONE && start > Scoreboard_results.total_entries_on_scoreboard) || (Current_filter_mode == FILTER_FRIENDS && start > Friend_count))
        {
            start = 1;
        }
        if (start != Starting_index)
        {
            change_request(Current_scoreboard, Current_filter_mode, start, false);
            audiolib_cue_play(Audio_info[10], nullptr, nullptr);
        }
    }
    else if (page_up && Starting_index != 0)
    {
        int start = Starting_index - 16;
        if (start < 1)
        {
            // The last page's start.
            const int count = Current_filter_mode == FILTER_FRIENDS ? Friend_count : Scoreboard_results.total_entries_on_scoreboard;
            start = (count - 1) + (1 - (count - 1) % 16);
        }
        if (start != Starting_index)
        {
            change_request(Current_scoreboard, Current_filter_mode, start, false);
            audiolib_cue_play(Audio_info[9], nullptr, nullptr);
        }
    }
    else if (change_filter)
    {
        change_request(Current_scoreboard, Current_filter_mode == FILTER_FRIENDS ? FILTER_NONE : FILTER_FRIENDS, -1, false);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    else if (goto_me && (Current_filter_mode == FILTER_FRIENDS || My_leaderboard_state == MY_LEADERBOARD_EXISTS))
    {
        change_request(Current_scoreboard, Current_filter_mode, get_my_starting_index(), true);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    if (request_pending())
    {
        return;
    }
    if (num_rows < 1)
    {
        Match_scoreboard_selection = -1;
    }
    else if (num_rows < Match_scoreboard_selection)
    {
        Match_scoreboard_selection = num_rows - 1;
    }
    vint_data_item* highlight = vint_data_item::find((unsigned int)Mp_match_scoreboard_info.data_item_handles[0]);
    if (highlight != nullptr)
    {
        highlight->set_element(0, vint_variant(scoreboard_page_rows() < 1 ? 0 : Match_scoreboard_selection + 1));
    }
    if (view_card && scoreboard_page_rows() > 0)
    {
        const scoreboard_entry& entry = Scoreboard_results.entries[scoreboard_page_offset() + Match_scoreboard_selection];
        multi_player_menu_system_pull_up_player_option_message_box(entry.player_uid, entry.player_name, true);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
        return;
    }
    if (back)
    {
        Mp_match_scoreboard_info.exit_action = MMEA_S_POP;
        scoreboard_set_state(MMS_S_UNLOADING);
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
}

// 0x008E4D90 (:1026) inlined
static void scoreboard_process_data_items()
{
    // dtime is unused by the functions.
    scoreboard_process_data_item_hint_text(0.0f);
    scoreboard_process_data_item_titles(0.0f);
    scoreboard_process_data_item_info_text(0.0f);
    scoreboard_process_data_group_menu_options(0.0f);
}

// 0x0090E320 (:1357)
static void scoreboard_process_loaded()
{
    process_stats_request();
    scoreboard_process_data_items();
    scoreboard_process_input();
}

// 0x00917350 (:1536)
void ui_document_mp_scoreboard_init()
{
    state_info info;
    info.enter = ui_document_mp_scoreboard_enter;
    info.exit = ui_document_mp_scoreboard_exit;
    info.process = ui_document_mp_scoreboard_process;
    info.handshake = ui_document_mp_scoreboard_handshake;
    info.render = ui_document_mp_scoreboard_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_SCOREBOARD, info);
}

// 0x0090E340 (:1553)
void ui_document_mp_scoreboard_enter(game_state prev_state)
{
    scoreboard_init_state();
    scoreboard_init_info();
    Mp_match_scoreboard_info.doc_handle = ui_load_document(Ui_mp_shell_name_score, DFIT_NONE, 0.5f, DLT_NORMAL, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    Mp_match_scoreboard_info.control_id = ui_request_control_id();
    Mp_match_scoreboard_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, true);
    // Magic number: room for 101 entries (a page is 16; the rest for a friends board).
    Scoreboard_results.max_entries = 101;
    Scoreboard_results.entries = (scoreboard_entry*)MP_multi_purpose_mempool.alloc(101 * sizeof(scoreboard_entry), 0);
    Scoreboard_friend_uid_list = (multi_player_uid*)MP_multi_purpose_mempool.alloc(101 * sizeof(multi_player_uid), 0);
    // Magic number: 100 KB of scratch for the retrieval.
    Scoreboard_scratch = (unsigned char*)MP_multi_purpose_mempool.alloc(0x19000, 0);
    Starting_index = -1;
    Request_type = REQUEST_TYPE_NONE;
    Current_scoreboard = 0;
    My_leaderboard_state = MY_LEADERBOARD_UNKNOWN;
    Current_filter_mode = FILTER_NONE;
    Requested_filter_mode = FILTER_NONE;
    Friend_count = 0;
    Goto_me = false;
}

// 0x008F32D0 (:1595)
void ui_document_mp_scoreboard_exit(game_state new_state)
{
    hide_loading_msgbox();
    multi_core_platform_scoreboard_cancel_retrieve();
    multi_game_friend_cancel_all();
    Scoreboard_results.max_entries = 0;
    Scoreboard_results.entries = nullptr;
    Scoreboard_scratch = nullptr;
    Scoreboard_friend_uid_list = nullptr;
    MP_multi_purpose_mempool.clear();
    scoreboard_unload_ui_information();
    ui_relinquish_control_id(Mp_match_scoreboard_info.control_id);
    ui_unload_document(Mp_match_scoreboard_info.doc_handle, DFOT_NONE, 0.5f);
    Mp_match_scoreboard_info.doc_handle = -1;
    ui_relinquish_input_id(Mp_match_scoreboard_info.input_id);
    multi_game_lobby_clear_pushed_to_game();
}

// 0x009154A0 (:1624)
void ui_document_mp_scoreboard_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    multi_player_menu_system_process();
    ui_add_for_processing(Mp_match_scoreboard_info.doc_handle);
    switch (Mp_match_scoreboard_info.state)
    {
    case MMS_S_LOADING:
        scoreboard_process_loading();
        break;
    case MMS_S_LOADED:
        scoreboard_process_loaded();
        break;
    case MMS_S_UNLOADING:
        scoreboard_unload_ui_information();
        hide_loading_msgbox();
        if (Mp_match_scoreboard_info.exit_action == MMEA_S_POP)
        {
            gameseq_pop_state();
        }
        else if (Mp_match_scoreboard_info.exit_action == MMEA_S_PUSH)
        {
            gameseq_push_state(Mp_match_scoreboard_info.exit_push_target, true, true);
        }
        scoreboard_set_state(MMS_S_UNLOADED);
        break;
    case MMS_S_UNLOADED:
        scoreboard_set_state(MMS_S_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_match_scoreboard_info.control_id, false);
}

// 0x008E4DB0 (:1678)
void ui_document_mp_scoreboard_handshake(rl_renderer*)
{
    Mp_match_scoreboard_info.prev_state = Mp_match_scoreboard_info.state;
    if (Mp_match_scoreboard_info.pending_state != Mp_match_scoreboard_info.state)
    {
        scoreboard_do_state_change();
    }
}

// 0x008FC820 (:1686)
void ui_document_mp_scoreboard_render(rl_renderer*)
{
    if (Mp_match_scoreboard_info.doc_handle == -1 || Mp_match_scoreboard_info.control_id == 0)
    {
        return;
    }
    if (Mp_match_scoreboard_info.state == MMS_S_LOADED && Mp_match_scoreboard_info.prev_state == MMS_S_LOADED)
    {
        ui_add_for_rendering(Mp_match_scoreboard_info.doc_handle);
    }
    ui_render(Mp_match_scoreboard_info.control_id, false);
}
