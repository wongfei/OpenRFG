// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.cpp
//
// The matchmaking menu (GS_MULTIPLAYER_SEARCH_MATCHMAKING, stub_impl.md S7) on rfg_ui_mp_shell: runs the search state
// machine (build the query list, search, try the best results, create a game when nothing is left), shows its progress
// and the party, and pushes the game lobby once a session is ready. Party members follow the leader into its lobby.
//
// Original bugs kept (commented): compare_search_region is not a consistent ordering; the document stays loaded on exit;
// sorted_player_list is never filled, so the player options box is unreachable.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_matchmaking.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

class rl_renderer;

// ---- file-private types (rfg.pdb)
enum mp_menu_states_mmaking
{
    MMS_MM_LOADING = 0,
    MMS_MM_LOADED = 1,
    MMS_MM_EXITING = 2,
    MMS_MM_UNLOADING = 3,
    MMS_MM_UNLOADED = 4,
    NUM_MP_MM_MENU_STATES = 5,
};

enum mp_menu_exit_action_mmaking
{
    MMEA_MM_POP = 0,
    MMEA_MM_PUSH = 1,
};

enum mp_matchmake_search_states
{
    MPMMSS_NOT_SEARCHING = 0,
    MPMMSS_WAITING_TO_SEARCH = 1,
    MPMMSS_CREATING_SEARCH = 2,
    MPMMSS_WAITING_ON_SEARCH = 3,
    MPMMSS_SETTING_UP = 4,
    MPMMSS_CONNECTING = 5,
    MPMMSS_CREATING_LOBBY = 6,
    MPMMSS_CANCELLING = 7,
    NUM_MP_MATCHMAKING_SEARCH_STATES = 8,
};

enum mp_matchmaking_hints_mmaking
{
    MM_HINT_INVALID = -1,
    MM_HINT_ABORT = 0,
};

// size 0xD8
struct mp_matchmaking_menu_info
{
    mp_menu_states_mmaking state; // 0x00
    mp_menu_states_mmaking pending_state; // 0x04
    mp_menu_states_mmaking prev_state; // 0x08
    mp_menu_exit_action_mmaking exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    mp_matchmake_search_states matchmaking_state; // 0x14
    float matchmaking_state_time; // 0x18
    int doc_handle; // 0x1C
    int data_group_handle; // 0x20
    farray<int,16> group_item_handles; // 0x24
    farray<unsigned int,16> sorted_player_list; // 0x74
    unsigned int control_id; // 0xC4
    int input_id; // 0xC8
    bool exit_msg_box_created; // 0xCC
    timestamp_realtime exit_time; // 0xD0
    unsigned int multi_purpose_mempool_bookmark; // 0xD4
};
static_assert(sizeof(mp_matchmaking_menu_info) == 0xD8, "mp_matchmaking_menu_info");

// ---- file-static globals
static void mm_process_data_item_hint_info(vint_data_item* vdi);
static void mm_process_data_item_titles(vint_data_item* vdi);
static void mm_process_data_item_info_text(vint_data_item* vdi);
static void mm_process_data_item_selection_info(vint_data_item* vdi);
static void mm_process_data_item_menu_type(vint_data_item* vdi);
static void mm_process_data_item_party_info(vint_data_item* vdi);
static void mm_process_data_item_tip_info(vint_data_item* vdi);
static mp_shell_data_item_info Matchmaking_data_items[7] = { // 0x01663584
    {MPSDIT_HINT_INFO, mm_process_data_item_hint_info},
    {MPSDIT_TITLES, mm_process_data_item_titles},
    {MPSDIT_INFO_TEXT, mm_process_data_item_info_text},
    {MPSDIT_SELECTION_INFO, mm_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, mm_process_data_item_menu_type},
    {MPSDIT_PARTY_INFO, mm_process_data_item_party_info},
    {MPSDIT_TIPS, mm_process_data_item_tip_info},
};
static const char* Leave_party_string_mmaking = {"MENU_LEAVE_PARTY"}; // 0x016635BC
static const char* Cancel_matchmaking_string = {"MENU_CANCEL_MATCHMAKING"}; // 0x016635C0
static const char* Matchmaking_string_mmaking = {"MENU_MATCHMAKING"}; // 0x016635C4
static const char* Players_string_mmaking = {"MENU_PLAYERS"}; // 0x016635F4
static const char* Working_string = {"MENU_WORKING"}; // 0x016635F8
static mp_matchmaking_hints_mmaking Current_mouse_hint_choice_mmaking = MM_HINT_INVALID; // 0x016635FC
static int Mouse_tip_clicked_mmaking = -1; // 0x01663600
static mp_matchmaking_menu_info Mp_matchmaking_info; // 0x016666F0
static timestamp_realtime Waiting_to_matchmake_timer; // 0x016667C8
static farray<multi_session_join_status,4> Connection_attempt_statuses; // 0x016667CC
static int Total_results_found; // 0x02BF2220
bool Pushed_to_lobby; // 0x02BF2226
static int Num_queries; // 0x02BF4340
static game_query* Search_queries; // 0x02BFD474
static bool s_partyPlayerDataActive[16]; // 0x02C00E0C
static int mm_hint_config[1]; // 0x02C00EFC
static int Num_connection_attempts; // 0x02C02B48
static game_match* Best_result; // 0x02C02CCC
static bool Cancelling_matchmaking; // 0x02C0360C
static bool Never_create_match; // 0x02C0360D
static int Mp_matchmaking_selection; // 0x02C03610
static int mm_num_hints; // 0x02C03614
static unsigned int s_partyPlayerDataCount; // 0x02C03618
static multi_game_data s_partyPlayerData[16]; // 0x02C7E3B0
static match_results_list<25> Search_results; // 0x02C9FC98

// ---- internal functions
static void mm_load_ui_information();
static void mm_unload_ui_information();
static void mm_process_matchmaking_waiting();
static void mm_process_matchmaking_search();
static void mm_process_matchmatcking_setting_up();
static void mm_process_matchmaking_connecting();
static void mm_process_matchmaking_creating();
static void mm_ui_push(game_state new_state);
static void mm_ui_pop();
static void mm_set_matchmaking_state(mp_matchmake_search_states new_state);

// 0x0088C3E0
// Only this file's s_partyPlayerData constructs these (the rest live in the session's pools); the timestamps' constructors
// invalidate them.
multi_game_data::multi_game_data()
{
}

// 0x008F2820 (:254)
static void mm_do_state_change()
{
    do
    {
        Mp_matchmaking_info.state = Mp_matchmaking_info.pending_state;
        switch (Mp_matchmaking_info.pending_state)
        {
        case MMS_MM_LOADING:
            Mp_matchmaking_info.exit_msg_box_created = false;
            return;
        case MMS_MM_LOADED:
            mm_load_ui_information();
            break;
        case MMS_MM_EXITING:
            Mp_matchmaking_info.exit_time.set(0);
            Mp_matchmaking_info.exit_msg_box_created = false;
            if (Lobby_session != nullptr)
            {
                Lobby_session->session_delete();
            }
            break;
        case MMS_MM_UNLOADING:
            if (Mp_matchmaking_info.exit_msg_box_created || Loading_screen_ref_cnt > 0)
            {
                ui_remove_loading_screen();
            }
            mm_unload_ui_information();
            if (Mp_matchmaking_info.exit_action == MMEA_MM_POP)
            {
                multi_player_menu_system_close_message_box();
                gameseq_pop_state();
            }
            else if (Mp_matchmaking_info.exit_action == MMEA_MM_PUSH)
            {
                gameseq_push_state(Mp_matchmaking_info.exit_push_target, true, true);
            }
            Mp_matchmaking_info.pending_state = MMS_MM_UNLOADED;
            break;
        default:
            return;
        }
    }
    while (Mp_matchmaking_info.state != Mp_matchmaking_info.pending_state);
}

// 0x0088C400 (:318)
static void mm_set_state(mp_menu_states_mmaking new_state)
{
    Mp_matchmaking_info.pending_state = new_state;
}

// 0x0088C410 (:327) inlined
static void mm_init_state()
{
    Mp_matchmaking_info.state = MMS_MM_UNLOADED;
    Mp_matchmaking_info.pending_state = MMS_MM_UNLOADED;
    Mp_matchmaking_info.prev_state = MMS_MM_UNLOADED;
}

// 0x008AD9C0 (:335) inlined
static void mm_init_info()
{
    Mp_matchmaking_info.matchmaking_state = MPMMSS_NOT_SEARCHING;
    Mp_matchmaking_info.matchmaking_state_time = 0.0f;
    Mp_matchmaking_info.group_item_handles.clear();
    Mp_matchmaking_info.sorted_player_list.clear();
    Mp_matchmaking_info.data_group_handle = -1;
}

// 0x008C0560 (:393)
// One party row of the player list.
static void mm_add_menu_option_data_item()
{
    vint_data_item* item = vint_data_group::find(Mp_matchmaking_info.data_group_handle)->add_data_item(nullptr);
    if (item != nullptr)
    {
        vint_variant_type types[8] = {
            VINT_PROP_TYPE_INT, // index
            VINT_PROP_TYPE_INT, // badge
            VINT_PROP_TYPE_STRING, // name
            VINT_PROP_TYPE_INT, // row type
            VINT_PROP_TYPE_STRING, // level
            VINT_PROP_TYPE_INT, // voice state
            VINT_PROP_TYPE_BOOL, // party leader
            VINT_PROP_TYPE_BOOL, // in the party
        };
        item->set_template(types, 8);
        Mp_matchmaking_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008AD9F0 (:423)
static void mm_remove_menu_option_data_item()
{
    vint_data_group* group = vint_data_group::find(Mp_matchmaking_info.data_group_handle);
    const int last = Mp_matchmaking_info.group_item_handles.size() - 1;
    group->remove_data_item(Mp_matchmaking_info.group_item_handles[last]);
    Mp_matchmaking_info.group_item_handles.resize(last);
}

// 0x008D26F0 (:437) inlined
static void mm_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_matchmaking_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_matchmaking_info.doc_handle, Mp_matchmaking_info.data_group_handle);
    }
}

// 0x008D2720 (:474)
static void mm_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Matchmaking_data_items, 7);
    mm_load_data_groups();
}

// 0x008D2760 (:483)
static void mm_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    mm_init_info();
}

// 0x0088C440 (:491)
static void mm_process_data_item_menu_type(vint_data_item* vdi)
{
    // Magic number: the shell's menu layout with a list and an info panel (rfg_ui_mp_shell).
    vdi->set_element(0, vint_variant(3));
}

// 0x008E3010 (:496)
static void mm_process_data_item_hint_info(vint_data_item* vdi)
{
    mm_hint_config[0] = MM_HINT_INVALID;
    mm_num_hints = 0;
    multi_player_menu_system_begin_hints(vdi);
    if (multi_game_matchmaking_can_cancel())
    {
        if (multi_game_party_in_party() && !multi_game_party_am_i_leader(false))
        {
            multi_player_menu_system_add_hint(localization_get_display_string_req(Leave_party_string_mmaking, nullptr), CBA_MENU_ABORT);
        }
        else
        {
            multi_player_menu_system_add_hint(localization_get_display_string_opt(Cancel_matchmaking_string), CBA_MENU_ABORT);
        }
    }
    multi_player_menu_system_add_hint_config(mm_num_hints, mm_hint_config, MM_HINT_ABORT);
    multi_player_menu_system_end_hints();
}

// 0x0088C480 (:518)
static void mm_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Matchmaking_string_mmaking));
    char title[128];
    const multi_game_playlist* playlist = multi_game_playlist_get_by_id(multi_game_preferences_get_ideal_query()->m_playlist_id);
    sprintf(title, "%s", playlist != nullptr ? playlist->display_name : "");
    vdi->set_element(1, vint_variant(title));
}

// 0x008D27D0 (:531)
static void mm_process_data_item_info_text(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(true));
    wchar_t status_text[256] = {};
    wchar_t found_text[256] = {};
    wchar_t attempt_text[256];
    wchar_t attempts_text[1024] = {};
    if (multi_game_party_in_party() && !multi_game_party_am_i_leader(false))
    {
        const char* text = Lobby_session->get_state() == MSS_NONE ? "MENU_WAIT_FOR_LEADER" : "MENU_CONNECT_TO_LEADER";
        _snprintf_safe(status_text, 256, L"%ls", localization_get_display_string_opt(text));
    }
    else
    {
        const char* text = nullptr;
        switch (Mp_matchmaking_info.matchmaking_state)
        {
        case MPMMSS_NOT_SEARCHING:
            text = "MENU_STARTING_SEARCH";
            break;
        case MPMMSS_WAITING_TO_SEARCH:
        case MPMMSS_CREATING_SEARCH:
        case MPMMSS_WAITING_ON_SEARCH:
            text = "MENU_SEARCHING_FOR_GAMES";
            break;
        case MPMMSS_SETTING_UP:
        case MPMMSS_CONNECTING:
            text = "MENU_CONNECTING_TO_GAME";
            break;
        case MPMMSS_CREATING_LOBBY:
            text = "MENU_CREATING_GAME";
            break;
        case MPMMSS_CANCELLING:
            text = "MENU_CANCELLING_SEARCH";
            break;
        default:
            break;
        }
        _snprintf_safe(status_text, 256, L"%ls", text != nullptr ? localization_get_display_string_opt(text) : nullptr);
        _snprintf_safe(found_text, 256, L"%ls %d", localization_get_display_string_opt("MENU_GAMES_FOUND"), Total_results_found);
    }
    const int num_statuses = Connection_attempt_statuses.size();
    for (int i = 0; i < num_statuses; i++)
    {
        // The attempts' numbers keep counting while the list only holds the last four.
        const int attempt = Num_connection_attempts - num_statuses + i + 1;
        const wchar_t* status = localization_get_display_string_opt(multi_player_menu_system_get_connection_status_string(Connection_attempt_statuses[i]));
        _snprintf_safe(attempt_text, 256, L"%ls %d:\r %ls", localization_get_display_string_opt("MENU_GAME"), attempt, status);
        wcscat(attempts_text, attempt_text);
        if (i < num_statuses - 1)
        {
            wcscat(attempts_text, L"\n");
        }
    }
    if (Mp_matchmaking_info.matchmaking_state == MPMMSS_CREATING_LOBBY)
    {
        _snprintf_safe(attempt_text, 256, L"\n%ls", localization_get_display_string_opt("MENU_CREATING_GAME"));
        wcscat(attempts_text, attempt_text);
    }
    wchar_t text[1536];
    char encoded[4608];
    _snprintf_safe(text, 1536, "%ls\n%ls\n%ls", status_text, found_text, attempts_text);
    vint_encode_wide_text(encoded, 4608, text);
    vdi->set_element(1, vint_variant(encoded));
    vdi->set_element(2, vint_variant(""));
    vdi->set_element(3, vint_variant(true));
    vdi->set_element(4, vint_variant(Working_string));
}

// 0x0088C540 (:600)
static void mm_process_data_item_selection_info(vint_data_item* vdi)
{
    char image_name[32];
    char image_text[128];
    controls_get_button_image_name(image_name, 32, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image_text, 128, "[image:%s]", image_name);
    vdi->set_element(0, vint_variant(Mp_matchmaking_selection));
    vdi->set_element(1, vint_variant(image_text));
    vdi->set_element(2, vint_variant(false));
}

// 0x008E30E0 (:613)
static void mm_process_data_item_party_info(vint_data_item* vdi)
{
    wchar_t text[32];
    char encoded[96];
    g_gameLinkMutex.lock(0);
    const unsigned int num_members = get_game_link_session()->getNumPartyMembers();
    _snprintf_safe(text, 32, L"%ls (%d)", localization_get_display_string_opt(Players_string_mmaking), num_members);
    vint_encode_wide_text(encoded, 96, text);
    vdi->set_element(0, vint_variant((int)num_members > 1));
    vdi->set_element(1, vint_variant(encoded));
    g_gameLinkMutex.unlock();
}

// 0x008ADA50 (:628)
static void mm_process_data_item_tip_info(vint_data_item* vdi)
{
    char l2_name[32];
    char r2_name[32];
    char l2_text[128];
    char r2_text[128];
    controls_get_button_image_name(l2_name, 32, CBA_MENU_L2, Player_controller_index);
    controls_get_button_image_name(r2_name, 32, CBA_MENU_R2, Player_controller_index);
    keen::formatString(l2_text, 128, "[image:%s]", l2_name);
    keen::formatString(r2_text, 128, "[image:%s]", r2_name);
    const loading_screen_tip_data* tip = Current_tip < 0 || Current_tip >= Tip_data.size() ? nullptr : &Tip_data[Current_tip];
    if (multi_game_lobby_has_game_settings() || tip == nullptr)
    {
        vdi->set_element(0, vint_variant(false));
        vdi->set_element(1, vint_variant(""));
        vdi->set_element(2, vint_variant(""));
        vdi->set_element(3, vint_variant(""));
        vdi->set_element(4, vint_variant(""));
        vdi->set_element(5, vint_variant(""));
    }
    else
    {
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant(l2_text));
        vdi->set_element(2, vint_variant(r2_text));
        vdi->set_element(3, vint_variant(tip->title));
        vdi->set_element(4, vint_variant(tip->text));
        vdi->set_element(5, vint_variant(tip->icon_image));
    }
}

// 0x008C0640 (:658)
// The rows of the party (the data handed over by ui_document_mp_matchmaking_add_party_player_data), minus who left.
static void mm_process_data_group_menu_options()
{
    if (!multi_game_party_in_party())
    {
        return;
    }
    vint_data_group* group = vint_data_group::find(Mp_matchmaking_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    unit4::NamedRemoteUserId members[16];
    const unsigned int num_members = get_game_link_session()->getPartyMembers(members, 16);
    for (int i = 0; i < 16; i++)
    {
        if (s_partyPlayerDataActive[i])
        {
            unsigned int member = 0;
            while (member < num_members && !s_partyPlayerData[i].player_uid.is_equal(members[member].id))
            {
                member++;
            }
            if (member >= num_members)
            {
                s_partyPlayerDataActive[i] = false;
            }
        }
    }
    unsigned int row = 0;
    for (int i = 0; i < 16; i++)
    {
        if (!s_partyPlayerDataActive[i])
        {
            continue;
        }
        const multi_game_data& data = s_partyPlayerData[i];
        while ((unsigned int)group->get_num_data_items() <= row)
        {
            mm_add_menu_option_data_item();
        }
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_matchmaking_info.group_item_handles[row]);
        if (item != nullptr)
        {
            char name[192];
            char level[4];
            vint_encode_wide_text(name, 192, data.display_name);
            _snprintf_safe(level, 4, "%d", data.player_level);
            level[3] = '\0';
            const multi_voice_talk_state talk_state = multi_voice_get_icon_talk_state(data.player_uid);
            bool is_leader = false;
            bool in_party = false;
            if (multi_game_party_in_party())
            {
                is_leader = multi_game_party_is_player_leader(data.player_uid);
                in_party = multi_game_party_has_player(data.player_uid);
            }
            item->set_element(0, vint_variant((int)row + 1));
            item->set_element(1, vint_variant((int)data.player_badge));
            item->set_element(2, vint_variant(name));
            // Magic number: the row type the shell's player list draws for a party member (rfg_ui_mp_shell).
            item->set_element(3, vint_variant(2));
            item->set_element(4, vint_variant(level));
            item->set_element(5, vint_variant((int)talk_state));
            item->set_element(6, vint_variant(is_leader));
            item->set_element(7, vint_variant(in_party));
        }
        row++;
    }
    while (row < (unsigned int)group->get_num_data_items())
    {
        mm_remove_menu_option_data_item();
    }
    g_gameLinkMutex.unlock();
}

// 0x008F2900 (:760)
static void mm_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_matchmaking_info.doc_handle))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_matchmaking.cpp", 760);
        multi_player_menu_system_shell_unload_doc(Mp_matchmaking_info.doc_handle);
        mm_set_state(MMS_MM_UNLOADING);
        break;
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Mp_matchmaking_info.doc_handle);
        mm_set_state(MMS_MM_UNLOADING);
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        mm_set_state(MMS_MM_LOADED);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_matchmaking.cpp", 760);
        break;
    }
}

// 0x008D2D00 (:824)
static void mm_input_process_mouse(bool& events_consumed)
{
    Mouse_tip_clicked_mmaking = -1;
    if ((Utility_doc_handle == -1 || !ui_is_msgbox_active(-2)) && ui_mouse_over_hint(kHintText1, Mp_matchmaking_info.doc_handle, true))
    {
        Current_mouse_hint_choice_mmaking = (mp_matchmaking_hints_mmaking)mm_hint_config[0];
        return;
    }
    const char* tip_names[2] = {"tips_button_1", "tips_button_2"};
    const int tip = ui_mouse_process_hints(tip_names, 2, Mp_matchmaking_info.doc_handle, true);
    if (tip != -1)
    {
        Mouse_tip_clicked_mmaking = tip;
        return;
    }
    const int num_rows = Mp_matchmaking_info.group_item_handles.size();
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Mp_matchmaking_selection, num_rows, Mp_matchmaking_info.input_id, Mp_matchmaking_info.doc_handle, events_consumed);
    if (selection >= 0)
    {
        if (selection != Mp_matchmaking_selection)
        {
            multi_player_menu_system_set_menu_selection(selection, Mp_matchmaking_selection, num_rows);
        }
        if (mouse_just_released(0))
        {
            events_consumed = true;
        }
    }
}

// 0x0090C7A0 (:859)
static void mm_process_input()
{
    bool events_consumed = false;
    Current_mouse_hint_choice_mmaking = MM_HINT_INVALID;
    mm_input_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    multi_player_menu_system_handle_menu_key_input(Mp_matchmaking_selection, Mp_matchmaking_info.group_item_handles.size(), Mp_matchmaking_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    const int input_id = Mp_matchmaking_info.input_id;
    // Magic numbers: the button indices of ui_get_button_info (0x20 options, 11 back, 0x11/0x13 the tip buttons).
    const bool options = ui_get_button_info(0x20, input_id) > 0.0f;
    const bool back = ui_get_button_info(11, input_id) > 0.0f;
    const bool tip_back = ui_get_button_info(0x11, input_id) > 0.0f || Mouse_tip_clicked_mmaking == 0;
    const bool tip_forward = ui_get_button_info(0x13, input_id) > 0.0f || Mouse_tip_clicked_mmaking == 1;
    if (options && Mp_matchmaking_selection >= 0)
    {
        // Original bug: nothing adds to sorted_player_list, so mm_process_loaded keeps the selection at -1 and this is unreachable.
        const multi_game_data* player = multi_game_data_get(Mp_matchmaking_info.sorted_player_list[Mp_matchmaking_selection], false, true);
        if (player != nullptr && !player->linger_time.valid() && !multi_player_menu_system_has_open_message_box())
        {
            multi_player_menu_system_pull_up_player_option_message_box(player->player_uid, player->display_name, true);
            audiolib_cue_play(Audio_info[4], nullptr, nullptr);
        }
        else
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
        return;
    }
    if (back || Current_mouse_hint_choice_mmaking == MM_HINT_ABORT)
    {
        multi_player_menu_system_pull_up_leave_confirm_message_box();
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        return;
    }
    if (tip_forward && !multi_game_lobby_has_game_settings())
    {
        loading_screen_tip_cycle(true);
        audiolib_cue_play(Audio_info[10], nullptr, nullptr);
        return;
    }
    if (tip_back && !multi_game_lobby_has_game_settings())
    {
        loading_screen_tip_cycle(false);
        audiolib_cue_play(Audio_info[9], nullptr, nullptr);
    }
}

// 0x0090C990 (:916)
static void mm_process_loaded()
{
    multi_player_menu_system_shell_process_data_items();
    mm_process_data_group_menu_options();
    mm_process_input();
    const int num_players = Mp_matchmaking_info.sorted_player_list.size();
    if (num_players < 1)
    {
        Mp_matchmaking_selection = -1;
    }
    else
    {
        Mp_matchmaking_selection = Mp_matchmaking_selection % num_players;
        if (Mp_matchmaking_selection < 0)
        {
            Mp_matchmaking_selection = 0;
        }
    }
    bool cancel = false;
    if (!multi_game_matchmaking_can_cancel())
    {
        Cancelling_matchmaking = false;
    }
    else
    {
        cancel = Cancelling_matchmaking;
    }
    if (cancel || multi_game_lobby_left_by_choice())
    {
        mm_ui_pop();
    }
}

// 0x008E3200 (:1036)
static void mm_process_matchmaking()
{
    Mp_matchmaking_info.matchmaking_state_time += Frametime_real;
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    switch (Mp_matchmaking_info.matchmaking_state)
    {
    case MPMMSS_NOT_SEARCHING:
        if (link->isPartyEnabled() && !link->isPartyOwner())
        {
            mm_process_matchmaking_waiting();
        }
        else if (Lobby_session->get_state() == MSS_NONE)
        {
            if (!Cancelling_matchmaking && !multi_game_lobby_left_by_choice())
            {
                if (Pushed_to_lobby)
                {
                    // Back from a lobby: wait a little so a party doesn't rejoin the same game at once.
                    mm_set_matchmaking_state(MPMMSS_WAITING_TO_SEARCH);
                }
                else
                {
                    mm_set_matchmaking_state(MPMMSS_CREATING_SEARCH);
                }
            }
        }
        else if (Lobby_session->get_state() == MSS_READY || Lobby_session->get_state() == MSS_REGISTERED)
        {
            Lobby_session->session_delete();
        }
        break;
    case MPMMSS_WAITING_TO_SEARCH:
        if (!Waiting_to_matchmake_timer.valid() || Waiting_to_matchmake_timer.elapsed())
        {
            mm_set_matchmaking_state(MPMMSS_CREATING_SEARCH);
        }
        break;
    case MPMMSS_WAITING_ON_SEARCH:
        mm_process_matchmaking_search();
        break;
    case MPMMSS_SETTING_UP:
        mm_process_matchmatcking_setting_up();
        break;
    case MPMMSS_CONNECTING:
        mm_process_matchmaking_connecting();
        break;
    case MPMMSS_CREATING_LOBBY:
        mm_process_matchmaking_creating();
        break;
    default:
        break;
    }
    g_gameLinkMutex.unlock();
}

// 0x0090CA60 (:1093)
static void mm_process_exit()
{
    if (Lobby_session == nullptr || Lobby_session->get_state() == MSS_NONE)
    {
        mm_set_state(MMS_MM_UNLOADING);
        return;
    }
    // Magic number: 250 ms of deleting before the disconnect screen shows.
    if (Lobby_session->get_state() == MSS_DELETING && !Mp_matchmaking_info.exit_msg_box_created && Mp_matchmaking_info.exit_time.time_since() > 250)
    {
        ui_show_loading_screen(false, "MENU_DISCONNECT");
        Mp_matchmaking_info.exit_msg_box_created = true;
    }
}

// 0x008D2DF0 (:1115)
static void mm_ui_push(game_state new_state)
{
    Mp_matchmaking_info.exit_action = MMEA_MM_PUSH;
    Mp_matchmaking_info.exit_push_target = new_state;
    mm_set_matchmaking_state(MPMMSS_WAITING_TO_SEARCH);
    mm_set_state(MMS_MM_UNLOADING);
    Waiting_to_matchmake_timer.invalidate();
}

// 0x008D2E50 (:1124)
static void mm_ui_pop()
{
    multi_core_platform_set_game_mode_context(PGMC_MULTIPLAYER);
    multi_core_platform_set_game_type_context(PGTC_PUBLIC);
    mm_set_matchmaking_state(MPMMSS_NOT_SEARCHING);
    Mp_matchmaking_info.exit_action = MMEA_MM_POP;
    mm_set_state(MMS_MM_EXITING);
    Waiting_to_matchmake_timer.invalidate();
}

// 0x008D2EA0 (:1134)
// A party member: follows the leader's lobby, and leaves the party when the leader doesn't get one in 5 s.
static void mm_process_matchmaking_waiting()
{
    switch (Lobby_session->get_state())
    {
    case MSS_NONE:
        if (multi_game_lobby_is_in_lobby())
        {
            if (!Waiting_to_matchmake_timer.valid())
            {
                // Magic number: 5 s for the leader.
                Waiting_to_matchmake_timer.set(5000);
                return;
            }
            if (!Waiting_to_matchmake_timer.elapsed())
            {
                return;
            }
            multi_game_party_leave_party(false);
        }
        Waiting_to_matchmake_timer.invalidate();
        break;
    case MSS_READY:
    case MSS_STARTING:
    case MSS_IN_GAME:
        if (Lobby_session->get_info()->party_lobby)
        {
            mm_ui_pop();
        }
        else
        {
            Pushed_to_lobby = true;
            mm_ui_push(GS_MULTIPLAYER_GAME_LOBBY);
        }
        break;
    default:
        break;
    }
}

// 0x008D2F50 (:1173)
static void mm_process_matchmaking_search()
{
    multi_core_matchmake_system_process();
    if (multi_core_matchmake_async_find_complete())
    {
        Total_results_found += Search_results.num_results;
        Best_result = nullptr;
        multi_game_data_clear_players(ST_GAME);
        mm_set_matchmaking_state(MPMMSS_SETTING_UP);
    }
}

// 0x008D2F90 (:1188)
// Next step once the playlist and DLC mask are set: join the best result, else the next query, else make a game.
static void mm_process_matchmatcking_setting_up()
{
    if (multi_core_platform_get_user_playlist_state() == POS_IN_PROGRESS || multi_core_platform_get_user_dlc_mask_state() == POS_IN_PROGRESS)
    {
        return;
    }
    if (Best_result != nullptr)
    {
        mm_set_matchmaking_state(MPMMSS_CONNECTING);
    }
    else if (Num_queries > 1)
    {
        // The next query moves to the front, the one above searches on queries[0].
        Num_queries--;
        Search_queries[0].copy(Search_queries[Num_queries]);
        mm_set_matchmaking_state(MPMMSS_WAITING_ON_SEARCH);
    }
    else if (!Never_create_match && !multi_game_spectating() && multi_core_platform_get_nat_type() != PNT_RESTRICTED)
    {
        mm_set_matchmaking_state(MPMMSS_CREATING_LOBBY);
    }
    else
    {
        mm_set_matchmaking_state(MPMMSS_WAITING_TO_SEARCH);
    }
}

// 0x008D3100 (:1207)
static void mm_process_matchmaking_connecting()
{
    switch (Lobby_session->get_state())
    {
    case MSS_NONE:
        Connection_attempt_statuses[Connection_attempt_statuses.size() - 1] = Lobby_session->get_last_join_status();
        multi_game_data_clear_players(ST_GAME);
        mm_set_matchmaking_state(MPMMSS_SETTING_UP);
        break;
    case MSS_READY:
    case MSS_STARTING:
    case MSS_IN_GAME:
        if (!Waiting_to_matchmake_timer.valid() || Waiting_to_matchmake_timer.elapsed())
        {
            Pushed_to_lobby = true;
            Connection_attempt_statuses[Connection_attempt_statuses.size() - 1] = Lobby_session->get_last_join_status();
            mm_ui_push(GS_MULTIPLAYER_GAME_LOBBY);
        }
        break;
    default:
        break;
    }
}

// 0x008D31C0 (:1229)
static void mm_process_matchmaking_creating()
{
    switch (Lobby_session->get_state())
    {
    case MSS_NONE:
        multi_game_data_clear_players(ST_GAME);
        mm_set_matchmaking_state(MPMMSS_WAITING_TO_SEARCH);
        break;
    case MSS_READY:
    case MSS_STARTING:
    case MSS_IN_GAME:
        if (!Waiting_to_matchmake_timer.valid() || Waiting_to_matchmake_timer.elapsed())
        {
            Pushed_to_lobby = true;
            mm_ui_push(GS_MULTIPLAYER_GAME_LOBBY);
        }
        break;
    default:
        break;
    }
}

// 0x0088C650 (:1249)
int compare_search_region(const void* lhs, const void* rhs)
{
    // Original bug: a regional lhs is always "less", also against another regional query, so this isn't a consistent
    // ordering for qsort; regional searches still end up first.
    if (!((const game_query*)lhs)->m_world_wide_search)
    {
        return -1;
    }
    return ((const game_query*)rhs)->m_world_wide_search ? 0 : 1;
}

// 0x008C0A00 (:1266)
// rfg.exe has the recursive calls as a loop (tail calls) and inlines this into its callers.
static void mm_set_matchmaking_state(mp_matchmake_search_states new_state)
{
    Mp_matchmaking_info.matchmaking_state = new_state;
    Mp_matchmaking_info.matchmaking_state_time = 0.0f;
    switch (new_state)
    {
    case MPMMSS_NOT_SEARCHING:
        Total_results_found = 0;
        Connection_attempt_statuses.clear();
        Num_connection_attempts = 0;
        break;
    case MPMMSS_WAITING_TO_SEARCH:
        Total_results_found = 0;
        Connection_attempt_statuses.clear();
        Num_connection_attempts = 0;
        // Magic number: up to 5 s, so that players searching at once spread out.
        Waiting_to_matchmake_timer.set(int_rand_range(0, 5000));
        break;
    case MPMMSS_CREATING_SEARCH:
    {
        // Magic number: 16 is the capacity allocated in enter.
        multi_game_preferences_create_query_list(Search_queries, &Num_queries, 16);
        game_query temp;
        for (int i = 0; i < Num_queries; i++)
        {
            const int other = int_rand_range(i, Num_queries - 1);
            if (other != i)
            {
                temp.game_query_base::copy(Search_queries[i]);
                Search_queries[i].copy(Search_queries[other]);
                Search_queries[other].copy(temp);
            }
        }
        qsort(Search_queries, Num_queries, sizeof(game_query), compare_search_region);
        if (Num_queries >= 1)
        {
            mm_set_matchmaking_state(MPMMSS_WAITING_ON_SEARCH);
        }
        else
        {
            mm_set_matchmaking_state(Never_create_match ? MPMMSS_WAITING_TO_SEARCH : MPMMSS_CREATING_LOBBY);
        }
        break;
    }
    case MPMMSS_WAITING_ON_SEARCH:
        multi_game_preferences_set_current_query(Search_queries);
        multi_core_matchmake_find_match_async(Search_results.num_results, Search_results.list, 25, Search_queries);
        break;
    case MPMMSS_SETTING_UP:
    {
        // A failed best result is dropped: the last result takes its place.
        if (Best_result != nullptr && Search_results.num_results > 0)
        {
            Search_results.num_results--;
            Best_result->copy(Search_results.results[Search_results.num_results]);
        }
        if (Search_results.num_results == 0)
        {
            Best_result = nullptr;
            const multi_game_playlist* playlist = multi_game_playlist_get_by_id(multi_game_preferences_get_current_query()->m_playlist_id);
            if (playlist != nullptr)
            {
                multi_core_platform_set_user_playlist(playlist->playlist_id.get_crc());
                multi_core_platform_set_game_type_context(playlist->game_type);
                multi_core_platform_set_game_mode_context(playlist->mode_type);
            }
            break;
        }
        Best_result = &Search_results.results[0];
        for (int i = 1; i < Search_results.num_results; i++)
        {
            game_match* game = &Search_results.results[i];
            if (multi_game_preferences_calculate_score(game) > multi_game_preferences_calculate_score(Best_result))
            {
                Best_result = game;
            }
        }
        // The list keeps the last four attempts.
        if (Connection_attempt_statuses.size() < Connection_attempt_statuses.capacity())
        {
            Connection_attempt_statuses.add(MSJS_INVALID);
        }
        else
        {
            for (int i = 0; i < Connection_attempt_statuses.size() - 1; i++)
            {
                Connection_attempt_statuses[i] = Connection_attempt_statuses[i + 1];
            }
            Connection_attempt_statuses[Connection_attempt_statuses.size() - 1] = MSJS_INVALID;
        }
        Num_connection_attempts++;
        const multi_game_playlist* playlist = multi_game_playlist_get_by_id(multi_game_preferences_get_current_query()->m_playlist_id);
        if (playlist != nullptr)
        {
            multi_core_platform_set_user_playlist(playlist->playlist_id.get_crc());
            multi_core_platform_set_game_mode_context(playlist->mode_type);
            multi_core_platform_set_game_type_context(playlist->game_type);
        }
        else
        {
            mm_set_matchmaking_state(MPMMSS_SETTING_UP);
        }
        break;
    }
    case MPMMSS_CONNECTING:
        // Magic number: the lobby shows for at least 1 s before the push.
        Waiting_to_matchmake_timer.set(1000);
        g_joinedGameLobbyViaInvitation = false;
        multi_game_lobby_join(Best_result, nullptr, false, false);
        break;
    case MPMMSS_CREATING_LOBBY:
        // Magic number: as for connecting.
        Waiting_to_matchmake_timer.set(1000);
        multi_game_lobby_create(false, false, false, false);
        break;
    default:
        break;
    }
}

// 0x0090CAC0 (:1399)
void ui_document_mp_matchmaking_enter(game_state prev_state)
{
    multi_game_lobby_reset_leaving_state();
    mm_init_state();
    Pushed_to_lobby = false;
    Cancelling_matchmaking = false;
    mm_init_info();
    multi_player_menu_system_shell_load_doc(Mp_matchmaking_info.doc_handle, false);
    Mp_matchmaking_info.control_id = ui_request_control_id();
    Mp_matchmaking_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Current_tip = int_rand_range(0, Tip_data.size() - 1);
    loading_screen_tip_cycle(true);
    Mp_matchmaking_info.multi_purpose_mempool_bookmark = MP_multi_purpose_mempool.mark();
    // Magic numbers: room for 16 queries of 0x1C bytes each; alignment 0 is the pool's default.
    Search_queries = (game_query*)MP_multi_purpose_mempool.alloc(16 * sizeof(game_query), 0);
    for (int i = 0; i < 16; i++)
    {
        new (&Search_queries[i]) game_query();
    }
    Num_queries = 0;
}

// 0x008D3270 (:1437)
void ui_document_mp_matchmaking_exit(game_state new_state)
{
    // Original bug: the document stays loaded on exit.
    mm_unload_ui_information();
    ui_relinquish_control_id(Mp_matchmaking_info.control_id);
    ui_relinquish_input_id(Mp_matchmaking_info.input_id);
    MP_multi_purpose_mempool.restore_to_mark(Mp_matchmaking_info.multi_purpose_mempool_bookmark);
    console_disable_chat_window();
    Ui_text_chat_enabled = false;
}

// 0x00914CF0 (:1455)
void ui_document_mp_matchmaking_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    multi_player_menu_system_process();
    // Pulled into a lobby by something else (an invite): leave for it.
    if (Mp_matchmaking_info.matchmaking_state != MPMMSS_CREATING_LOBBY && Mp_matchmaking_info.matchmaking_state != MPMMSS_CONNECTING && Lobby_session->get_state() == MSS_READY && !Lobby_session->get_info()->party_lobby)
    {
        mm_ui_push(GS_MULTIPLAYER_GAME_LOBBY);
        return;
    }
    ui_add_for_processing(Mp_matchmaking_info.doc_handle);
    switch (Mp_matchmaking_info.state)
    {
    case MMS_MM_LOADING:
        mm_process_loading();
        break;
    case MMS_MM_LOADED:
        mm_process_matchmaking();
        mm_process_loaded();
        break;
    case MMS_MM_EXITING:
        mm_process_exit();
        break;
    case MMS_MM_UNLOADED:
        mm_set_state(MMS_MM_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_matchmaking_info.control_id, false);
}

// 0x008F29B0 (:1495)
void ui_document_mp_matchmaking_handshake(rl_renderer*)
{
    Mp_matchmaking_info.prev_state = Mp_matchmaking_info.state;
    if (Mp_matchmaking_info.pending_state != Mp_matchmaking_info.state)
    {
        mm_do_state_change();
    }
}

// 0x008FBDB0 (:1503)
void ui_document_mp_matchmaking_render(rl_renderer*)
{
    if (Mp_matchmaking_info.doc_handle == -1 || Mp_matchmaking_info.control_id == 0 || multi_game_lobby_pushed_to_game())
    {
        return;
    }
    if (Mp_matchmaking_info.state == MMS_MM_LOADED && Mp_matchmaking_info.prev_state == MMS_MM_LOADED)
    {
        ui_add_for_rendering(Mp_matchmaking_info.doc_handle);
    }
    ui_render(Mp_matchmaking_info.control_id, false);
}

// 0x00914DE0 (:1526)
void ui_document_mp_matchmaking_init()
{
    Pushed_to_lobby = false;
    state_info info;
    info.enter = ui_document_mp_matchmaking_enter;
    info.exit = ui_document_mp_matchmaking_exit;
    info.process = ui_document_mp_matchmaking_process;
    info.handshake = ui_document_mp_matchmaking_handshake;
    info.render = ui_document_mp_matchmaking_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_SEARCH_MATCHMAKING, info);
}

// 0x0088C680 (:1545)
bool multi_game_matchmaking_can_cancel()
{
    return !(Mp_matchmaking_info.pending_state > MMS_MM_EXITING && (Mp_matchmaking_info.exit_action == MMEA_MM_PUSH || Mp_matchmaking_info.exit_push_target == GS_MULTIPLAYER_GAME_LOBBY));
}

// 0x0088C6B0 (:1550)
void multi_game_matchmaking_cancel()
{
    Cancelling_matchmaking = true;
}

// 0x0088C6C0 (:1565)
void ui_document_mp_matchmaking_add_party_player_data(const multi_game_data* pData)
{
    if (s_partyPlayerDataCount < 16)
    {
        memmove((void*)&s_partyPlayerData[s_partyPlayerDataCount], pData, sizeof(multi_game_data));
        s_partyPlayerDataActive[s_partyPlayerDataCount] = true;
        s_partyPlayerDataCount++;
    }
}

// 0x008ADCE0 (:1577)
void ui_document_mp_matchmaking_clear_party_player_data()
{
    for (int i = 0; i < 16; i++)
    {
        s_partyPlayerDataActive[i] = false;
    }
    s_partyPlayerDataCount = 0;
}
