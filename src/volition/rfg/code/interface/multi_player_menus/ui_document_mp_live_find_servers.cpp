// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.cpp
//
// The online server browser (GS_MULTIPLAYER_LIVE_FIND_SERVERS, stub_impl.md S7) on rfg_ui_mp_shell: the Steam lobby
// search (Live_server_browser) runs when the menu loads and on refresh, its results listing on the search's end; joining
// goes through the browser, which pushes this menu to its connecting state (lfs_ui_push).
//
// Original bugs kept (commented): the document stays loaded on exit; the events flag starts uninitialized in rfg.exe.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.h"

#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/multi_server_browser.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/util/varray.h"

#include <wchar.h>

// ---- file-private types (rfg.pdb)
enum mp_menu_exit_actions_live_servers
{
    MMEAL_SE_POP = 0,
    MMEAL_SE_PUSH = 1,
    NUM_MP_SEL_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_live_servers
{
    MMSL_SE_LOADING = 0,
    MMSL_SE_LOADED = 1,
    MMSL_SE_CONNECTING = 2,
    MMSL_SE_UNLOADING = 3,
    MMSL_SE_UNLOADED = 4,
    NUM_MP_SEL_MENU_STATES = 5,
};

// size 0x74
struct mp_live_find_servers_shell_info
{
    mp_menu_states_live_servers state; // 0x00
    mp_menu_states_live_servers pending_state; // 0x04
    mp_menu_states_live_servers prev_state; // 0x08
    mp_menu_exit_actions_live_servers exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
};
static_assert(sizeof(mp_live_find_servers_shell_info) == 0x74, "mp_live_find_servers_shell_info");

// ---- file-static globals
static void lfs_process_data_item_hint_text(vint_data_item* vdi);
static void lfs_process_data_item_titles(vint_data_item* vdi);
static void lfs_process_data_item_info_text(vint_data_item* vdi);
static void lfs_process_data_item_selection_info(vint_data_item* vdi);
static void lfs_process_data_item_menu_type(vint_data_item* vdi);
static mp_shell_data_item_info Live_find_servers_data_items[5] = { // 0x01663538
    {MPSDIT_HINT_INFO, lfs_process_data_item_hint_text},
    {MPSDIT_TITLES, lfs_process_data_item_titles},
    {MPSDIT_INFO_TEXT, lfs_process_data_item_info_text},
    {MPSDIT_SELECTION_INFO, lfs_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, lfs_process_data_item_menu_type},
};
static const char* Back_string_live_servers = {"MENU_BACK"}; // 0x01663560
static const char* Join_game_string_live_servers = {"MENU_JOIN_GAME"}; // 0x01663564
static const char* No_servers_found_live_string = {"MENU_NO_SERVERS_INTERNET_FOUND"}; // 0x01663568
static int Live_find_servers_selection = -1; // 0x01663570
static multi_gameplay_mode Live_server_gameplay_mode = (multi_gameplay_mode)-1; // 0x01663574
static int Live_server_accept_hint; // 0x01663578
static int Live_server_abort_hint; // 0x0166357C
static int Live_server_refresh_hint; // 0x01663580
static mp_live_find_servers_shell_info Mp_live_find_servers_info;
static int Live_server_num_hints;
static bool Live_server_search_running; // 0x02C035EF
static unsigned int Num_live_server_infos; // 0x02C03600
static ServerTypeFilter Live_server_type_filter; // 0x02C03604
// Magic number: 256 results (ServerBrowser's capacity).
static ServerInfo Live_server_infos[256]; // 0x02C10A00

// ---- internal functions
static void lfs_load_ui_information();
static void lfs_unload_ui_information();

// Port helper: a new search, the first row selected, as rfg.exe inlines it.
static void lfs_start_search()
{
    if (Live_server_browser.startSearch(0x100, Live_server_type_filter, Live_server_gameplay_mode, nullptr))
    {
        Live_find_servers_selection = 0;
        Live_server_search_running = true;
    }
}

// 0x0090C5C0
// Loaded: the data items (when coming from loading) and a search unless one runs; connecting: the "joining" box;
// unloading: the exit action.
static void lfs_do_state_change()
{
    do
    {
        const mp_menu_states_live_servers previous = Mp_live_find_servers_info.state;
        Mp_live_find_servers_info.state = Mp_live_find_servers_info.pending_state;
        if (Mp_live_find_servers_info.pending_state == MMSL_SE_LOADED)
        {
            if (previous == MMSL_SE_LOADING)
            {
                lfs_load_ui_information();
            }
            if (!Live_server_search_running)
            {
                lfs_start_search(); // inlined
            }
        }
        else if (Mp_live_find_servers_info.pending_state == MMSL_SE_CONNECTING)
        {
            ui_show_loading_screen(false, "MENU_MSJS_JOIN_IN_PROGRESS");
        }
        else
        {
            if (Mp_live_find_servers_info.pending_state != MMSL_SE_UNLOADING)
            {
                return;
            }
            lfs_unload_ui_information();
            if (Mp_live_find_servers_info.exit_action == MMEAL_SE_POP)
            {
                gameseq_pop_state();
            }
            else if (Mp_live_find_servers_info.exit_action == MMEAL_SE_PUSH)
            {
                gameseq_push_state(Mp_live_find_servers_info.exit_push_target, true, true);
            }
            Mp_live_find_servers_info.pending_state = MMSL_SE_UNLOADED;
        }
    } while (Mp_live_find_servers_info.state != Mp_live_find_servers_info.pending_state);
}

// 0x0088C1F0
static void lfs_init_state()
{
    Mp_live_find_servers_info.state = MMSL_SE_UNLOADED;
    Mp_live_find_servers_info.pending_state = MMSL_SE_UNLOADED;
    Mp_live_find_servers_info.prev_state = MMSL_SE_UNLOADED;
    Mp_live_find_servers_info.exit_action = MMEAL_SE_POP;
    Mp_live_find_servers_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AD850
static void lfs_init_info()
{
    Mp_live_find_servers_info.group_item_handles.clear();
    Mp_live_find_servers_info.data_group_handle = -1;
}

// 0x008C01E0
static void lfs_add_data_group_item()
{
    if (Mp_live_find_servers_info.group_item_handles.size() >= Mp_live_find_servers_info.group_item_handles.capacity())
    {
        return;
    }
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_live_find_servers_info.data_group_handle);
    vint_data_item* item = group->add_data_item(nullptr);
    if (item != nullptr)
    {
        const vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL};
        item->set_template(types, 7);
        Mp_live_find_servers_info.group_item_handles.add((int)item->get_handle());
    }
}

// 0x008AD870
static void lfs_remove_data_group_item()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_live_find_servers_info.data_group_handle);
    if (group != nullptr)
    {
        const int last = Mp_live_find_servers_info.group_item_handles.size() - 1;
        group->remove_data_item((unsigned int)Mp_live_find_servers_info.group_item_handles[last]);
        Mp_live_find_servers_info.group_item_handles.remove(last);
    }
}

// 0x008D2490
static void lfs_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_live_find_servers_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_live_find_servers_info.doc_handle, Mp_live_find_servers_info.data_group_handle);
    }
}

// 0x008D24C0
static void lfs_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Live_find_servers_data_items, 5);
    lfs_load_data_groups(); // inlined
}

// 0x008E2A50
static void lfs_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items(); // inlined
    Mp_live_find_servers_info.group_item_handles.clear();
    Mp_live_find_servers_info.data_group_handle = -1;
    multi_player_menu_system_close_message_box(); // inlined
}

// 0x008E2AE0
// Back; join on a selected server; refresh when no search runs.
static void lfs_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi); // inlined
    Live_server_accept_hint = -1;
    Live_server_abort_hint = -1;
    Live_server_refresh_hint = -1;
    Live_server_num_hints = 0;
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_live_servers, nullptr), CBA_MENU_ABORT);
    Live_server_abort_hint = Live_server_num_hints;
    Live_server_num_hints++;
    if (Live_find_servers_selection >= 0 && Live_find_servers_selection < (int)Num_live_server_infos)
    {
        multi_player_menu_system_add_hint(localization_get_display_string_req(Join_game_string_live_servers, nullptr), CBA_MENU_SELECT);
        Live_server_accept_hint = Live_server_num_hints;
        Live_server_num_hints++;
    }
    if (!Live_server_search_running)
    {
        multi_player_menu_system_add_hint(localization_get_display_string_req("MENU_REFRESH", nullptr), CBA_MENU_X);
        Live_server_refresh_hint = Live_server_num_hints;
        Live_server_num_hints++;
    }
    multi_player_menu_system_end_hints();
}

// 0x0088C220
static void lfs_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant("MENU_FIND_GAME"));
    vdi->set_element(1, vint_variant(Join_game_string_live_servers));
}

// 0x008E2C10
// The selected server: host, match type, level, game mode and players; else "no servers found".
static void lfs_process_data_item_info_text(vint_data_item* vdi)
{
    // Magic number: 512 characters of text.
    wchar_t text[512];
    const ServerInfo* info = nullptr;
    if (Live_find_servers_selection >= 0 && Live_find_servers_selection < (int)Num_live_server_infos && Live_server_infos[Live_find_servers_selection].isValid()) // inlined
    {
        info = &Live_server_infos[Live_find_servers_selection];
    }
    if (info != nullptr)
    {
        char players_label[128];
        char game_mode_label[128];
        char level[128];
        char game_mode[128];
        char match_type[128];
        char utf8[512];
        keen::convertUTF16ToUTF8(players_label, sizeof(players_label), (const uint16*)localization_get_display_string_req("MENU_PLAYERS", nullptr));
        keen::convertUTF16ToUTF8(game_mode_label, sizeof(game_mode_label), (const uint16*)localization_get_display_string_req("MENU_GAME_MODE", nullptr));
        keen::convertUTF16ToUTF8(level, sizeof(level), (const uint16*)localization_get_display_string_req(info->levelName, nullptr));
        keen::convertUTF16ToUTF8(game_mode, sizeof(game_mode), (const uint16*)localization_get_display_string_req(multi_gameplay_mode_get_display_name(info->gameMode), nullptr));
        keen::convertUTF16ToUTF8(match_type, sizeof(match_type), (const uint16*)localization_get_display_string_req(info->partyLobby ? "MENU_PARTY_LOBBY_CUSTOM_GAME" : "MENU_MATCHMAKING", nullptr));
        keen::formatString(utf8, sizeof(utf8), "%s\n \n%s\n \n%s\n%s: %s\n%s: %zu/%zu", info->hostPlayerName, match_type, level, game_mode_label, game_mode, players_label, info->numPlayers, info->maxPlayers);
        keen::convertUTF8ToUTF16((uint16*)text, sizeof(text), utf8);
    }
    else
    {
        wcsncpy(text, localization_get_display_string_req(No_servers_found_live_string, nullptr), 512);
        text[511] = L'\0';
    }
    // Magic number: 1536 bytes of encoded text.
    char encoded[1536];
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant((const char*)encoded));
    vdi->set_element(2, vint_variant(""));
    vdi->set_element(3, vint_variant(false));
    vdi->set_element(4, vint_variant(""));
}

// 0x0088C290
static void lfs_process_data_item_selection_info(vint_data_item* vdi)
{
    char image[32];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
    vdi->set_element(0, vint_variant(Live_find_servers_selection));
    vdi->set_element(1, vint_variant((const char*)image));
    vdi->set_element(2, vint_variant(false));
}

// 0x0088C350
static void lfs_process_data_item_menu_type(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(0));
}

// 0x008C02C0
// One row per server (16 at most) with the host's name.
static void lfs_process_data_group_menu_options()
{
    if (vint_data_group::find((unsigned int)Mp_live_find_servers_info.data_group_handle) == nullptr)
    {
        return;
    }
    const int count = (int)Num_live_server_infos > 15 ? 16 : (int)Num_live_server_infos;
    while (Mp_live_find_servers_info.group_item_handles.size() < count)
    {
        lfs_add_data_group_item();
    }
    for (int i = 0; i < count; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_live_find_servers_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(-1));
        item->set_element(2, vint_variant((const char*)Live_server_infos[i].hostPlayerName));
        item->set_element(3, vint_variant(-1));
        item->set_element(4, vint_variant(" "));
        // Magic number: row type 3 (a plain option row of rfg_ui_mp_shell).
        item->set_element(5, vint_variant(3));
        item->set_element(6, vint_variant(false));
    }
    while (count < Mp_live_find_servers_info.group_item_handles.size())
    {
        lfs_remove_data_group_item(); // inlined
    }
}

// 0x008C04F0
static void lfs_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    lfs_process_data_group_menu_options();
}

// 0x008F2770
static void lfs_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_live_find_servers_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_live_find_servers_info.pending_state = MMSL_SE_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Mp_live_find_servers_info.doc_handle); // inlined
        Mp_live_find_servers_info.pending_state = MMSL_SE_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x008AD8D0
// LTCG: events_consumed in ESI.
static void lfs_input_selection_made(bool& events_consumed)
{
    if (Live_find_servers_selection >= 0 && Live_find_servers_selection < (int)Num_live_server_infos)
    {
        Live_server_browser.joinLobby(Live_server_infos[Live_find_servers_selection]);
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    else
    {
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
    }
    events_consumed = true;
}

// 0x008AD930
// LTCG: hint in EAX, events_consumed in ECX (and ESI for the selection).
static void lfs_input_hint_pressed(int hint, bool& events_consumed)
{
    if (hint == Live_server_abort_hint)
    {
        Mp_live_find_servers_info.exit_action = MMEAL_SE_POP;
        Mp_live_find_servers_info.pending_state = MMSL_SE_UNLOADING;
        events_consumed = true;
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
    else if (hint == Live_server_accept_hint)
    {
        lfs_input_selection_made(events_consumed);
        events_consumed = true;
    }
    else if (hint == Live_server_refresh_hint && !Live_server_search_running)
    {
        lfs_start_search(); // inlined
    }
}

// 0x008D2500
static void lfs_input_process_mouse(bool& events_consumed)
{
    const int hint = multi_player_menu_system_input_mouse_hints(Mp_live_find_servers_info.doc_handle, Live_server_num_hints);
    if (hint >= 0)
    {
        lfs_input_hint_pressed(hint, events_consumed);
        events_consumed = true;
        return;
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Live_find_servers_selection, Mp_live_find_servers_info.group_item_handles.size(), Mp_live_find_servers_info.input_id, Mp_live_find_servers_info.doc_handle, events_consumed);
    if (selection >= 0)
    {
        if (selection != Live_find_servers_selection)
        {
            multi_player_menu_system_set_menu_selection(selection, Live_find_servers_selection, Mp_live_find_servers_info.group_item_handles.size());
        }
        if (mouse_just_released(0))
        {
            lfs_input_selection_made(events_consumed);
        }
    }
}

// 0x008D2590
// LTCG: reads ECX (the events flag's stack slot only).
static void lfs_process_input()
{
    // Original bug: rfg.exe never initializes the flag (its slot is ECX, pushed as stack space); false here.
    bool events_consumed = false;
    lfs_input_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    multi_player_menu_system_handle_menu_key_input(Live_find_servers_selection, Mp_live_find_servers_info.group_item_handles.size(), Mp_live_find_servers_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    // Magic numbers: button 10 select, 11 back, 14 refresh.
    const bool select = ui_get_button_info(10, Mp_live_find_servers_info.input_id) > 0.0f;
    const bool back = ui_get_button_info(11, Mp_live_find_servers_info.input_id) > 0.0f;
    const float refresh = ui_get_button_info(14, Mp_live_find_servers_info.input_id);
    if (select)
    {
        lfs_input_selection_made(events_consumed);
    }
    else if (back)
    {
        // Magic number: hint 0, the back hint (always the first added).
        lfs_input_hint_pressed(0, events_consumed);
    }
    if (refresh > 0.0f && !Live_server_search_running)
    {
        lfs_start_search(); // inlined
    }
}

// 0x008D26A0
static void lfs_process_loaded()
{
    multi_player_menu_system_process();
    lfs_process_data_items(); // inlined
    if (Mp_live_find_servers_info.group_item_handles.size() < 1)
    {
        Live_find_servers_selection = -1;
    }
    else
    {
        Live_find_servers_selection = Live_find_servers_selection % Mp_live_find_servers_info.group_item_handles.size();
        if (Live_find_servers_selection < 0)
        {
            Live_find_servers_selection = 0;
        }
    }
    lfs_process_input();
}

// ---- external interface

// 0x0088C380 (:686)
// The browser asks for the join: the menu connects, and pushes the target once the session is ready.
void lfs_ui_push(game_state new_state)
{
    Mp_live_find_servers_info.exit_action = MMEAL_SE_PUSH;
    Mp_live_find_servers_info.exit_push_target = new_state;
    Mp_live_find_servers_info.pending_state = MMSL_SE_CONNECTING;
}

// 0x009172B0 (:705)
void ui_document_mp_live_fs_init()
{
    state_info info;
    info.enter = ui_document_mp_live_fs_enter;
    info.exit = ui_document_mp_live_fs_exit;
    info.process = ui_document_mp_live_fs_process;
    info.handshake = ui_document_mp_live_fs_handshake;
    info.render = ui_document_mp_live_fs_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_LIVE_FIND_SERVERS, info);
}

// 0x0090C690 (:722)
void ui_document_mp_live_fs_enter(game_state prev_state)
{
    lfs_init_state();
    lfs_init_info();
    multi_player_menu_system_shell_load_doc(Mp_live_find_servers_info.doc_handle, false); // inlined
    Mp_live_find_servers_info.control_id = ui_request_control_id();
    Mp_live_find_servers_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false); // inlined
}

// 0x008E2F90 (:745)
void ui_document_mp_live_fs_exit(game_state new_state)
{
    lfs_unload_ui_information();
    ui_relinquish_control_id(Mp_live_find_servers_info.control_id); // inlined
    // Original bug kept: the document is not unloaded (Mp_live_find_servers_info.doc_handle stays).
    ui_relinquish_input_id(Mp_live_find_servers_info.input_id);
}

// 0x00914BF0 (:759)
// The search's results are taken when it ends.
void ui_document_mp_live_fs_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    ui_add_for_processing(Mp_live_find_servers_info.doc_handle);
    switch (Mp_live_find_servers_info.state)
    {
    case MMSL_SE_LOADING:
        lfs_process_loading();
        break;
    case MMSL_SE_LOADED:
        lfs_process_loaded();
        break;
    case MMSL_SE_CONNECTING:
    {
        const multi_session_state state = Lobby_session->get_state();
        if (state == MSS_NONE)
        {
            ui_remove_loading_screen();
            multi_player_menu_system_pull_up_generic_message_box("", "MENU_COULD_NOT_JOIN");
            Mp_live_find_servers_info.pending_state = MMSL_SE_LOADED;
        }
        else if (state == MSS_READY)
        {
            ui_remove_loading_screen();
            Mp_live_find_servers_info.pending_state = MMSL_SE_UNLOADING;
        }
        break;
    }
    case MMSL_SE_UNLOADED:
        Mp_live_find_servers_info.pending_state = MMSL_SE_LOADING;
        break;
    default:
        break;
    }
    ui_process(Mp_live_find_servers_info.control_id, false);
    if (Live_server_search_running)
    {
        const unsigned int found = Live_server_browser.getSearchResults(Live_server_infos, 0x100, &Live_server_search_running);
        if (!Live_server_search_running)
        {
            Num_live_server_infos = found;
        }
    }
    if ((int)Num_live_server_infos <= Live_find_servers_selection && Num_live_server_infos != 0)
    {
        Live_find_servers_selection = (int)Num_live_server_infos - 1;
    }
}

// 0x0090C780 (:819)
void ui_document_mp_live_fs_handshake(rl_renderer*)
{
    Mp_live_find_servers_info.prev_state = Mp_live_find_servers_info.state;
    if (Mp_live_find_servers_info.pending_state != Mp_live_find_servers_info.state)
    {
        lfs_do_state_change();
    }
}

// 0x008FBD60 (:827)
void ui_document_mp_live_fs_render(rl_renderer*)
{
    if (Mp_live_find_servers_info.doc_handle == -1 || Mp_live_find_servers_info.control_id == 0)
    {
        return;
    }
    if (Mp_live_find_servers_info.state == MMSL_SE_LOADED && Mp_live_find_servers_info.prev_state == MMSL_SE_LOADED)
    {
        ui_add_for_rendering(Mp_live_find_servers_info.doc_handle);
    }
    ui_render(Mp_live_find_servers_info.control_id, false);
}
