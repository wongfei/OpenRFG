// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink_find_servers.cpp
//
// The system link server browser (GS_MULTIPLAYER_SYSLINK_FIND_SERVERS, UI.md UI5) on rfg_ui_mp_shell: the LAN search
// runs while the menu is loaded (multi_player_menu_system_process lists its results every 2 s); joining a server
// (multi_game_lobby_join) shows the "joining" loading box until the lobby session is ready (the game lobby is pushed)
// or ends ("could not join", back to the list).

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink_find_servers.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_string_processing.h"
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
#include "volition/rfg/code/multi/multi_core/include/multi_find_servers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <wchar.h>

// ---- file-private types (rfg.pdb)
enum mp_menu_exit_actions_servers
{
    MMEA_SE_POP = 0,
    MMEA_SE_PUSH = 1,
    NUM_MP_SE_MENU_EXIT_ACTIONS = 2,
};

enum mp_menu_states_servers
{
    MMS_SE_LOADING = 0,
    MMS_SE_LOADED = 1,
    MMS_SE_CONNECTING = 2,
    MMS_SE_UNLOADING = 3,
    MMS_SE_UNLOADED = 4,
    NUM_MP_SE_MENU_STATES = 5,
};

// size 0x74
struct mp_syslink_find_servers_shell_info
{

    mp_menu_states_servers state; // 0x00
    mp_menu_states_servers pending_state; // 0x04
    mp_menu_states_servers prev_state; // 0x08
    mp_menu_exit_actions_servers exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
};
static_assert(sizeof(mp_syslink_find_servers_shell_info) == 0x74, "mp_syslink_find_servers_shell_info");

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// const char* Mp_shell_doc_name (0x0166320C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_shell_doc_handle (0x0166325C) is used here too; placed in multi_player_menu_system.cpp.
// int Mp_msgbox_handle (0x016632E0) is used here too; placed in ui_document_hud_multiplayer.cpp.
static void sfs_process_data_item_hint_text(vint_data_item* vdi);
static void sfs_process_data_item_titles(vint_data_item* vdi);
static void sfs_process_data_item_info_text(vint_data_item* vdi);
static void sfs_process_data_item_selection_info(vint_data_item* vdi);
static void sfs_process_data_item_menu_type(vint_data_item* vdi);
static mp_shell_data_item_info Syslink_find_servers_data_items[5] = {
    {MPSDIT_HINT_INFO, sfs_process_data_item_hint_text},
    {MPSDIT_TITLES, sfs_process_data_item_titles},
    {MPSDIT_INFO_TEXT, sfs_process_data_item_info_text},
    {MPSDIT_SELECTION_INFO, sfs_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, sfs_process_data_item_menu_type},
}; // 0x01663CA8
static const char* Back_string_servers = {"MENU_BACK"}; // 0x01663CD0
static const char* Join_game_string_servers = {"MENU_JOIN_GAME"}; // 0x01663CD4
static const char* No_servers_found_string = {"MENU_NO_SERVERS_FOUND"}; // 0x01663CD8
static const char* Running_as_dedicated_string = {"MENU_RUNNING_AS_DEDICATED"}; // 0x01663CDC
static int Syslink_find_servers_selection = -1; // 0x01663CE0
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// bink_data Bink_info_main (0x01666148) is used here too; placed in main_menu.cpp.
// timestamp_realtime Server_search_ping_interval (0x01666274) is used here too; placed in multi_player_menu_system.cpp.
static mp_syslink_find_servers_shell_info Mp_syslink_find_servers_info; // 0x01666B28
// farray<unsigned int,16> Controller_slots (0x01667378) is used here too; placed in loading.cpp.
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
// multi_player_uid Playercard_player_uid (0x02BE9E14) is used here too; placed in multi_player_menu_system.cpp.
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
// mp_shell_data_item_info* Mp_shell_data_item_infos (0x02C0359C) is used here too; placed in multi_player_menu_system.cpp.
// int Num_mp_shell_data_item_infos (0x02C035A0) is used here too; placed in multi_player_menu_system.cpp.
// mp_message_box_type Mp_msgbox_type (0x02C035A8) is used here too; placed in multi_player_menu_system.cpp.
// vint_data_item* Hint_data_item (0x02C035B0) is used here too; placed in multi_player_menu_system.cpp.
// int Num_hints_added (0x02C035B4) is used here too; placed in multi_player_menu_system.cpp.
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

const multi_session_info* (*g_openrfgFindServersJoin)();

// ---- internal functions

static void sfs_load_ui_information();
static void sfs_unload_ui_information();

// 0x0090E6E0 (:167)
// Loaded: the data items (when coming from loading) and a new search; connecting: the "joining" box; unloading: the
// search stops and the exit action runs.
static void sfs_do_state_change()
{
    do
    {
        if (Mp_syslink_find_servers_info.pending_state == MMS_SE_LOADED)
        {
            const bool was_loading = Mp_syslink_find_servers_info.state == MMS_SE_LOADING;
            Mp_syslink_find_servers_info.state = Mp_syslink_find_servers_info.pending_state;
            if (was_loading)
            {
                sfs_load_ui_information();
            }
            multi_player_menu_system_start_searching_for_syslink_servers();
        }
        else if (Mp_syslink_find_servers_info.pending_state == MMS_SE_CONNECTING)
        {
            Mp_syslink_find_servers_info.state = Mp_syslink_find_servers_info.pending_state;
            ui_show_loading_screen(false, "MENU_MSJS_JOIN_IN_PROGRESS");
        }
        else
        {
            if (Mp_syslink_find_servers_info.pending_state != MMS_SE_UNLOADING)
            {
                Mp_syslink_find_servers_info.state = Mp_syslink_find_servers_info.pending_state;
                return;
            }
            // multi_player_menu_system_stop_searching_for_syslink_servers, inlined.
            Server_search_ping_interval.invalidate();
            Mp_syslink_find_servers_info.state = Mp_syslink_find_servers_info.pending_state;
            sfs_unload_ui_information();
            if (Mp_syslink_find_servers_info.exit_action == MMEA_SE_POP)
            {
                gameseq_pop_state();
            }
            else if (Mp_syslink_find_servers_info.exit_action == MMEA_SE_PUSH)
            {
                gameseq_push_state(Mp_syslink_find_servers_info.exit_push_target, true, true);
            }
            Mp_syslink_find_servers_info.pending_state = MMS_SE_UNLOADED;
        }
    } while (Mp_syslink_find_servers_info.state != Mp_syslink_find_servers_info.pending_state);
}

// 0x0088EAD0 (:229)
// LTCG: new_state in EAX.
static void sfs_set_state(mp_menu_states_servers new_state)
{
    Mp_syslink_find_servers_info.pending_state = new_state;
}

// 0x0088EAE0 (:238)
static void sfs_init_state()
{
    Mp_syslink_find_servers_info.state = MMS_SE_UNLOADED;
    Mp_syslink_find_servers_info.pending_state = MMS_SE_UNLOADED;
    Mp_syslink_find_servers_info.prev_state = MMS_SE_UNLOADED;
    Mp_syslink_find_servers_info.exit_action = MMEA_SE_POP;
    Mp_syslink_find_servers_info.exit_push_target = GS_MULTIPLAYER_LIVE;
}

// 0x008AE950 (:250)
static void sfs_init_info()
{
    Mp_syslink_find_servers_info.group_item_handles.clear();
    Mp_syslink_find_servers_info.data_group_handle = -1;
}

// 0x008C2650 (:258)
// A server row: {index, -1, name, -1, text, 3, false}.
static void sfs_add_data_group_item()
{
    if (Mp_syslink_find_servers_info.group_item_handles.size() >= Mp_syslink_find_servers_info.group_item_handles.capacity())
    {
        return;
    }
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_syslink_find_servers_info.data_group_handle);
    vint_data_item* item = group->add_data_item(nullptr);
    if (item == nullptr)
    {
        return;
    }
    vint_variant_type types[7] = {VINT_PROP_TYPE_INT, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_STRING, VINT_PROP_TYPE_INT, VINT_PROP_TYPE_BOOL};
    item->set_template(types, 7);
    Mp_syslink_find_servers_info.group_item_handles.add((int)item->get_handle());
}

// 0x008AE970 (:288)
static void sfs_remove_data_group_item()
{
    vint_data_group* group = vint_data_group::find((unsigned int)Mp_syslink_find_servers_info.data_group_handle);
    if (group != nullptr)
    {
        const int last = Mp_syslink_find_servers_info.group_item_handles.size() - 1;
        group->remove_data_item((unsigned int)Mp_syslink_find_servers_info.group_item_handles[last]);
        Mp_syslink_find_servers_info.group_item_handles.remove(last);
    }
}

// 0x008D4B60 (:299)
static void sfs_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_syslink_find_servers_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_syslink_find_servers_info.doc_handle, Mp_syslink_find_servers_info.data_group_handle);
    }
}

// 0x008D4B90 (:316)
static void sfs_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Syslink_find_servers_data_items, 5);
    sfs_load_data_groups();
}

// 0x008E59B0 (:326)
static void sfs_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    Mp_syslink_find_servers_info.group_item_handles.clear();
    Mp_syslink_find_servers_info.data_group_handle = -1;
    multi_player_menu_system_close_message_box();
}

// 0x008E5A40 (:335)
static void sfs_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi);
    multi_player_menu_system_add_hint(localization_get_display_string_req(Back_string_servers, nullptr), CBA_MENU_ABORT);
    if (Syslink_find_servers_selection >= 0)
    {
        multi_player_menu_system_add_hint(localization_get_display_string_req(Join_game_string_servers, nullptr), CBA_MENU_SELECT);
    }
    multi_player_menu_system_end_hints();
}

// 0x0088EB10 (:347)
static void sfs_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant("NETWORK_LOCAL_NAME"));
    vdi->set_element(1, vint_variant(Join_game_string_servers));
}

// 0x008E5B00 (:353)
// The selected server's name (and "running as dedicated"), else "no servers found".
static void sfs_process_data_item_info_text(vint_data_item* vdi)
{
    wchar_t text[512];
    const multi_server_info* server = Syslink_find_servers_selection >= 0 ? multi_fs_server_info((unsigned int)Syslink_find_servers_selection) : nullptr;
    if (server != nullptr)
    {
        wchar_t name[64];
        wcsncpy(name, server->name, 0x40);
        name[63] = L'\0';
        const wchar_t* dedicated = server->session_info.dedicated ? localization_get_display_string_req(Running_as_dedicated_string, nullptr) : L"";
        _snprintf_safe(text, 0x200, L"%ls\n%ls\n", name, dedicated);
    }
    else
    {
        wcsncpy(text, localization_get_display_string_req(No_servers_found_string, nullptr), 0x200);
        text[511] = L'\0';
    }
    char encoded[1536];
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    vdi->set_element(0, vint_variant(true));
    vdi->set_element(1, vint_variant((const char*)encoded));
    vdi->set_element(2, vint_variant(""));
    vdi->set_element(3, vint_variant(false));
    vdi->set_element(4, vint_variant(""));
}

// 0x0088EB80 (:379)
static void sfs_process_data_item_selection_info(vint_data_item* vdi)
{
    char image[32];
    char text[128];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(text, sizeof(text), "[image:%s]", image);
    vdi->set_element(0, vint_variant(Syslink_find_servers_selection));
    vdi->set_element(1, vint_variant((const char*)text));
    vdi->set_element(2, vint_variant(false));
}

// 0x0088EC70 (:392)
static void sfs_process_data_item_menu_type(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(0));
}

// 0x008C2730 (:397)
// One row per search result (16 at most), the extra rows removed.
static void sfs_process_data_group_menu_options()
{
    if (vint_data_group::find((unsigned int)Mp_syslink_find_servers_info.data_group_handle) == nullptr)
    {
        return;
    }
    int count = (int)s_lan_num_search_results;
    // Magic number: 16 rows at most.
    if (count > 15)
    {
        count = 16;
    }
    while (Mp_syslink_find_servers_info.group_item_handles.size() < count)
    {
        sfs_add_data_group_item();
    }
    char name[384];
    for (int i = 0; i < count; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_syslink_find_servers_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        vint_encode_wide_text(name, sizeof(name), multi_fs_server_info((unsigned int)i)->name);
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(-1));
        item->set_element(2, vint_variant((const char*)name));
        item->set_element(3, vint_variant(-1));
        item->set_element(4, vint_variant(" "));
        // Magic number: row type 3 (a plain option row of rfg_ui_mp_shell).
        item->set_element(5, vint_variant(3));
        item->set_element(6, vint_variant(false));
    }
    while (count < Mp_syslink_find_servers_info.group_item_handles.size())
    {
        sfs_remove_data_group_item();
    }
}

// 0x008C2980 (:449)
static void sfs_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    sfs_process_data_group_menu_options();
}

// 0x008F3820 (:458)
static void sfs_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_syslink_find_servers_info.doc_handle))
    {
    case ST_LOADING:
        return;
    case ST_LOADED:
        Mp_syslink_find_servers_info.pending_state = MMS_SE_LOADED;
        return;
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        // fall through
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Mp_syslink_find_servers_info.doc_handle);
        Mp_syslink_find_servers_info.pending_state = MMS_SE_UNLOADING;
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// The join of a server (sfs_input_selection_made's, and the command line's through the seam).
static void sfs_join(const multi_session_info* info)
{
    multi_game_lobby_join(info, nullptr, false, false);
    Mp_syslink_find_servers_info.exit_action = MMEA_SE_PUSH;
    Mp_syslink_find_servers_info.exit_push_target = GS_MULTIPLAYER_GAME_LOBBY;
    Mp_syslink_find_servers_info.pending_state = MMS_SE_CONNECTING;
    audiolib_cue_play(Audio_info[4], nullptr, nullptr);
}

// 0x008AE9D0 (:485)
// LTCG: events_consumed in ESI.
static void sfs_input_selection_made(bool& events_consumed)
{
    const multi_server_info* server = multi_fs_server_info((unsigned int)Syslink_find_servers_selection);
    if (server != nullptr)
    {
        sfs_join(&server->session_info);
    }
    else
    {
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
    }
    events_consumed = true;
}

// sfs_ui_pop with the back sound, inlined.
static void sfs_input_back()
{
    Mp_syslink_find_servers_info.exit_action = MMEA_SE_POP;
    Mp_syslink_find_servers_info.pending_state = MMS_SE_UNLOADING;
    audiolib_cue_play(Audio_info[5], nullptr, nullptr);
}

// 0x008AEA40 (:498)
// LTCG: hint in EAX, events_consumed in ECX (and ESI for the selection). Hint 0 back, 1 join.
static void sfs_input_hint_pressed(int hint, bool& events_consumed)
{
    if (hint == 0)
    {
        Mp_syslink_find_servers_info.pending_state = MMS_SE_UNLOADING;
        Mp_syslink_find_servers_info.exit_action = MMEA_SE_POP;
        events_consumed = true;
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        return;
    }
    if (hint == 1)
    {
        sfs_input_selection_made(events_consumed);
        events_consumed = true;
    }
}

// 0x008D4BD0 (:542)
// LTCG: events_consumed in EAX (EDI for the selection). Without a message box the hints first (back, join); the rows:
// hovering selects, a click joins.
static void sfs_input_process_mouse(bool& events_consumed)
{
    if (Utility_doc_handle == -1 || !ui_is_msgbox_active(-2))
    {
        int hint = -1;
        if (ui_mouse_over_hint(kHintText2, Mp_syslink_find_servers_info.doc_handle, true))
        {
            hint = 1;
        }
        if (ui_mouse_over_hint(kHintText1, Mp_syslink_find_servers_info.doc_handle, true))
        {
            hint = 0;
        }
        if (hint >= 0)
        {
            sfs_input_hint_pressed(hint, events_consumed);
            events_consumed = true;
            return;
        }
    }
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Syslink_find_servers_selection, Mp_syslink_find_servers_info.group_item_handles.size(), Mp_syslink_find_servers_info.input_id, Mp_syslink_find_servers_info.doc_handle, events_consumed);
    if (selection >= 0)
    {
        if (selection != Syslink_find_servers_selection)
        {
            multi_player_menu_system_set_menu_selection(selection, Syslink_find_servers_selection, Mp_syslink_find_servers_info.group_item_handles.size());
        }
        if (mouse_just_released(0))
        {
            sfs_input_selection_made(events_consumed);
        }
    }
}

// 0x008D4CE0 (:565)
// LTCG: reads ECX (the events flag's stack slot only).
static void sfs_process_input()
{
    // Original bug: rfg.exe never initializes the flag (its slot is ECX's top byte, pushed as stack space); false here.
    bool events_consumed = false;
    // Test seam (UI.md 2.7 item 2, off by default): the command line's server (a direct address or a search result).
    if (g_openrfgFindServersJoin != nullptr)
    {
        const multi_session_info* info = g_openrfgFindServersJoin();
        if (info != nullptr)
        {
            sfs_join(info);
            return;
        }
    }
    sfs_input_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    multi_player_menu_system_handle_menu_key_input(Syslink_find_servers_selection, Mp_syslink_find_servers_info.group_item_handles.size(), Mp_syslink_find_servers_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    // Magic numbers: button 10 select, 11 back.
    const float select = ui_get_button_info(10, Mp_syslink_find_servers_info.input_id);
    const float back = ui_get_button_info(11, Mp_syslink_find_servers_info.input_id);
    if (select > 0.0f)
    {
        sfs_input_selection_made(events_consumed);
        return;
    }
    if (back > 0.0f)
    {
        sfs_input_back();
    }
}

// 0x008D4DA0 (:590)
// The menu system runs here (the search results, the pong); the selection wraps into the list (-1 when empty).
static void sfs_process_loaded()
{
    multi_player_menu_system_process();
    sfs_process_data_items();
    if (Mp_syslink_find_servers_info.group_item_handles.size() < 1)
    {
        Syslink_find_servers_selection = -1;
    }
    else
    {
        Syslink_find_servers_selection = Syslink_find_servers_selection % Mp_syslink_find_servers_info.group_item_handles.size();
        if (Syslink_find_servers_selection < 0)
        {
            Syslink_find_servers_selection = 0;
        }
    }
    sfs_process_input();
}

// 0x0088ECA0 (:687)
// LTCG: new_state in EAX.
static void sfs_ui_push(game_state new_state)
{
    Mp_syslink_find_servers_info.exit_action = MMEA_SE_PUSH;
    Mp_syslink_find_servers_info.exit_push_target = new_state;
    Mp_syslink_find_servers_info.pending_state = MMS_SE_CONNECTING;
}

// 0x0088ECC0 (:694)
static void sfs_ui_pop()
{
    Mp_syslink_find_servers_info.exit_action = MMEA_SE_POP;
    Mp_syslink_find_servers_info.pending_state = MMS_SE_UNLOADING;
}

// ---- external interface

// 0x00917440 (:706)
void ui_document_mp_syslink_fs_init()
{
    state_info info;
    info.enter = ui_document_mp_syslink_fs_enter;
    info.exit = ui_document_mp_syslink_fs_exit;
    info.process = ui_document_mp_syslink_fs_process;
    info.handshake = ui_document_mp_syslink_fs_handshake;
    info.render = ui_document_mp_syslink_fs_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_SYSLINK_FIND_SERVERS, info);
}

// 0x0090E780 (:723)
void ui_document_mp_syslink_fs_enter(game_state prev_state)
{
    sfs_init_state();
    sfs_init_info();
    multi_player_menu_system_shell_load_doc(Mp_syslink_find_servers_info.doc_handle, false);
    Mp_syslink_find_servers_info.control_id = ui_request_control_id();
    Mp_syslink_find_servers_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
}

// 0x008E5CF0 (:746)
// Original: the shell document is not unloaded here (the next menu's load of rfg_ui_mp_shell finds it).
void ui_document_mp_syslink_fs_exit(game_state new_state)
{
    sfs_unload_ui_information();
    ui_relinquish_control_id(Mp_syslink_find_servers_info.control_id);
    ui_relinquish_input_id(Mp_syslink_find_servers_info.input_id);
}

// 0x009156F0 (:760)
// Connecting: the lobby session ended ("could not join", back to the list) or is ready (the game lobby is pushed).
void ui_document_mp_syslink_fs_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    ui_add_for_processing(Mp_syslink_find_servers_info.doc_handle);
    switch (Mp_syslink_find_servers_info.state)
    {
    case MMS_SE_LOADING:
        sfs_process_loading();
        break;
    case MMS_SE_LOADED:
        sfs_process_loaded();
        break;
    case MMS_SE_CONNECTING:
    {
        const multi_session_state state = Lobby_session->get_state();
        if (state == MSS_NONE)
        {
            ui_remove_loading_screen();
            multi_player_menu_system_pull_up_generic_message_box("", "MENU_COULD_NOT_JOIN");
            sfs_set_state(MMS_SE_LOADED);
        }
        else if (state == MSS_READY)
        {
            ui_remove_loading_screen();
            sfs_set_state(MMS_SE_UNLOADING);
        }
        break;
    }
    case MMS_SE_UNLOADED:
        sfs_set_state(MMS_SE_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_syslink_find_servers_info.control_id, false);
}

// 0x0090E870 (:805)
void ui_document_mp_syslink_fs_handshake(rl_renderer*)
{
    Mp_syslink_find_servers_info.prev_state = Mp_syslink_find_servers_info.state;
    if (Mp_syslink_find_servers_info.pending_state != Mp_syslink_find_servers_info.state)
    {
        sfs_do_state_change();
    }
}

// 0x008FC910 (:813)
void ui_document_mp_syslink_fs_render(rl_renderer*)
{
    if (Mp_syslink_find_servers_info.doc_handle == -1 || Mp_syslink_find_servers_info.control_id == 0)
    {
        return;
    }
    if (Mp_syslink_find_servers_info.state == MMS_SE_LOADED && Mp_syslink_find_servers_info.prev_state == MMS_SE_LOADED)
    {
        ui_add_for_rendering(Mp_syslink_find_servers_info.doc_handle);
    }
    ui_render(Mp_syslink_find_servers_info.control_id, false);
}
