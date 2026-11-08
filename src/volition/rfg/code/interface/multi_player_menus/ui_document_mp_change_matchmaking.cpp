// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.cpp
//
// The playlist list in front of matchmaking (GS_MULTIPLAYER_CHANGE_MATCHMAKING, stub_impl.md S7) on rfg_ui_mp_shell:
// shows every playlist with its players, modes and maps; accepting stores it as the ideal query and in the profile,
// tells a party to follow (PT_GO_MATCHMAKING) and pushes the search. Y opens the Steam chat overlay.
//
// Original bugs kept (commented): a missing level or mode name crashes the playlist info; the document stays loaded on
// exit; Menu_options overflows past 16 playlists.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

// ---- file-private types (rfg.pdb)
enum mp_menu_states
{
    MMS_LOADING = 0,
    MMS_LOADED = 1,
    MMS_WAITING_FOR_PARTY_TO_GO_MATCHMAKING = 2,
    MMS_UNLOADING = 3,
    MMS_UNLOADED = 4,
    NUM_MP_MENU_STATES = 5,
};

enum mp_menu_exit_action
{
    MMEA_POP = 0,
    MMEA_PUSH = 1,
};

enum mp_change_matchmaking_hint_choices
{
    CMM_HINT_INVALID = -1,
    CMM_HINT_ABORT = 0,
    CMM_HINT_CHAT = 1,
};

// size 0x4
struct menu_option_info
{
    const multi_game_playlist* playlist; // 0x00
};

// size 0x78
struct mp_change_matchmaking_menu_info
{
    mp_menu_states state; // 0x00
    mp_menu_states pending_state; // 0x04
    mp_menu_states prev_state; // 0x08
    mp_menu_exit_action exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> group_item_handles; // 0x1C
    unsigned int control_id; // 0x6C
    int input_id; // 0x70
    unsigned int selection_tween_handle; // 0x74
};
static_assert(sizeof(mp_change_matchmaking_menu_info) == 0x78, "mp_change_matchmaking_menu_info");

// ---- file-static globals
static void cmm_process_data_item_hint_text(vint_data_item* vdi);
static void cmm_process_data_item_titles(vint_data_item* vdi);
static void cmm_process_data_item_selection_info(vint_data_item* vdi);
static void cmm_process_data_item_matching_hint(vint_data_item* vdi);
static void cmm_process_data_item_playlist_info(vint_data_item* vdi);
static void cmm_process_data_item_info_text(vint_data_item* vdi);
static void cmm_process_data_item_menu_type(vint_data_item* vdi);
static mp_change_matchmaking_hint_choices Current_mouse_hint_choice = CMM_HINT_INVALID; // 0x016632E4
static mp_shell_data_item_info Change_matchmaking_data_items[7] = { // 0x016632E8
    {MPSDIT_HINT_INFO, cmm_process_data_item_hint_text},
    {MPSDIT_TITLES, cmm_process_data_item_titles},
    {MPSDIT_SELECTION_INFO, cmm_process_data_item_selection_info},
    {MPSDIT_MATCHING_HINT, cmm_process_data_item_matching_hint},
    {MPSDIT_PLAYLIST_INFO, cmm_process_data_item_playlist_info},
    {MPSDIT_INFO_TEXT, cmm_process_data_item_info_text},
    {MPSDIT_MENU_TYPE, cmm_process_data_item_menu_type},
};
static const char* Change_playlist_string = {"MENU_CHANGE_PLAYLIST"}; // 0x01663328
static const char* Cancel_string = {"MENU_CANCEL"}; // 0x0166332C
static const char* Players_string = {"MENU_PLAYERS"}; // 0x01663330
static const char* Max_party_size_string = {"MENU_MAX_PARTY_SIZE"}; // 0x01663334
static const char* Modes_string = {"MENU_MODES"}; // 0x01663338
static const char* Maps_string = {"MENU_MAPS"}; // 0x0166333C
static const char* Unknown_string = {"MENU_UNKNOWN"}; // 0x01663344
static const char* No_playlist_string = {"MENU_NO_PLAYLIST_SELECTED"}; // 0x01663348
static const char* Chat_string = {"MENU_JOIN_CHAT"}; // 0x0166334C
static int s_partyHostGoMatchmakingTimeout = 1000; // 0x01663350 milliseconds
static farray<menu_option_info,16> Menu_options; // 0x016662D0
static mp_change_matchmaking_menu_info Mp_change_matchmaking_info; // 0x01666320
static timestamp_realtime s_waitingForPartyToGoMatchmakingTimeout; // 0x01666398
static int cmm_hint_config[2]; // 0x02BF2168
static int Num_menu_options; // 0x02BF4344
static int cmm_num_hints; // 0x02C035C0
static int Change_matchmaking_selection; // 0x02C035C4

// ---- internal functions
static void cmm_load_ui_information();
static void cmm_unload_ui_information();
static void cmm_ui_push(game_state new_state);
static void cmm_ui_pop();

// 0x008DF580 (:188)
static bool option_sort_less_than_or_equal_to(const menu_option_info& lhs, const menu_option_info& rhs)
{
    if (lhs.playlist == rhs.playlist)
    {
        return true;
    }
    if (lhs.playlist->required_xp_level != rhs.playlist->required_xp_level)
    {
        return lhs.playlist->required_xp_level <= rhs.playlist->required_xp_level;
    }
    if (lhs.playlist->max_players != rhs.playlist->max_players)
    {
        return lhs.playlist->max_players <= rhs.playlist->max_players;
    }
    const wchar_t* lhs_name = localization_does_tag_exist(lhs.playlist->display_name) ? localization_get_display_string_req(lhs.playlist->display_name, nullptr) : L"1";
    const wchar_t* rhs_name = localization_does_tag_exist(rhs.playlist->display_name) ? localization_get_display_string_req(rhs.playlist->display_name, nullptr) : L"1";
    return wcscmp(lhs_name, rhs_name) <= 0;
}

// 0x008F1D20 (:224)
static void cmm_do_state_change()
{
    do
    {
        Mp_change_matchmaking_info.state = Mp_change_matchmaking_info.pending_state;
        switch (Mp_change_matchmaking_info.pending_state)
        {
        case MMS_LOADED:
        {
            cmm_load_ui_information();
            Change_matchmaking_selection = 0;
            // The profile's playlist is the first selection.
            const player_profile_data* profile = player_profile_get("matchmaking", Player_controller_index == -1 ? 0 : Player_controller_index);
            if (profile != nullptr)
            {
                const multi_game_playlist* playlist = multi_game_playlist_get_by_id(profile->mp_playlist_id);
                if (playlist != nullptr)
                {
                    for (int i = 0; i < Menu_options.size(); i++)
                    {
                        if (Menu_options[i].playlist == playlist)
                        {
                            Change_matchmaking_selection = i;
                            break;
                        }
                    }
                }
            }
            break;
        }
        case MMS_WAITING_FOR_PARTY_TO_GO_MATCHMAKING:
            s_waitingForPartyToGoMatchmakingTimeout.set(s_partyHostGoMatchmakingTimeout);
            if (Lobby_session->get_state() != MSS_NONE && Lobby_session->get_slots_used() > 1)
            {
                // The packet with the playlist, big endian (the writers are inlined in rfg.exe).
                const unsigned int playlist_id = Menu_options[Change_matchmaking_selection].playlist->playlist_id.get_crc();
                unsigned char packet[1200];
                packet[0] = PT_GO_MATCHMAKING;
                unsigned int size = Multi_packets[PT_GO_MATCHMAKING].group_id != 0 ? 5 : 3; // in-order packets carry a group header
                packet[size] = (unsigned char)(playlist_id >> 24);
                packet[size + 1] = (unsigned char)(playlist_id >> 16);
                packet[size + 2] = (unsigned char)(playlist_id >> 8);
                packet[size + 3] = (unsigned char)playlist_id;
                size += 4;
                // The size field doesn't count the three bytes of type and size.
                packet[1] = (unsigned char)((size - 3) >> 8);
                packet[2] = (unsigned char)(size - 3);
                multi_io_send_to_all_reliable(Lobby_session, packet, size);
            }
            else
            {
                cmm_ui_push(GS_MULTIPLAYER_SEARCH_MATCHMAKING);
            }
            break;
        case MMS_UNLOADING:
            cmm_unload_ui_information();
            if (Mp_change_matchmaking_info.exit_action == MMEA_POP)
            {
                gameseq_pop_state();
                party_lobby_reset_party_state();
                // Without a party the party lobby below goes as well.
                if (!Multi_partied)
                {
                    gameseq_pop_state();
                }
            }
            else if (Mp_change_matchmaking_info.exit_action == MMEA_PUSH)
            {
                multi_game_lobby_leave(true);
                gameseq_push_state(Mp_change_matchmaking_info.exit_push_target, true, true);
            }
            Mp_change_matchmaking_info.pending_state = MMS_UNLOADED;
            break;
        case MMS_UNLOADED:
            Mp_change_matchmaking_info.exit_action = MMEA_POP;
            Mp_change_matchmaking_info.exit_push_target = GS_MULTIPLAYER_LIVE;
            Mp_change_matchmaking_info.group_item_handles.clear();
            Mp_change_matchmaking_info.data_group_handle = -1;
            Mp_change_matchmaking_info.selection_tween_handle = (unsigned int)-1;
            return;
        default:
            return;
        }
    }
    while (Mp_change_matchmaking_info.state != Mp_change_matchmaking_info.pending_state);
}

// 0x0088A750 (:308)
static void cmm_set_state(mp_menu_states new_state)
{
    Mp_change_matchmaking_info.pending_state = new_state;
}

// 0x0088A760 (:317) inlined
static void cmm_init_state()
{
    Mp_change_matchmaking_info.state = MMS_UNLOADED;
    Mp_change_matchmaking_info.pending_state = MMS_UNLOADED;
    Mp_change_matchmaking_info.prev_state = MMS_UNLOADED;
}

// 0x008AC900 (:325) inlined
static void cmm_init_info()
{
    Mp_change_matchmaking_info.exit_action = MMEA_POP;
    Mp_change_matchmaking_info.exit_push_target = GS_MULTIPLAYER_LIVE;
    Mp_change_matchmaking_info.group_item_handles.clear();
    Mp_change_matchmaking_info.data_group_handle = -1;
    Mp_change_matchmaking_info.selection_tween_handle = (unsigned int)-1;
}

// 0x0088A780 (:337) inlined
static void cmm_destroy_selection_tween()
{
    if (Mp_change_matchmaking_info.selection_tween_handle != (unsigned int)-1)
    {
        vint_data_item* tween = vint_data_item::find(Mp_change_matchmaking_info.selection_tween_handle);
        if (tween != nullptr)
        {
            tween->destroy();
        }
        Mp_change_matchmaking_info.selection_tween_handle = (unsigned int)-1;
    }
}

// 0x008BFC20 (:348)
static void cmm_load_menu_option_data_items()
{
    vint_data_group* group = vint_data_group::find(Mp_change_matchmaking_info.data_group_handle);
    for (int i = 0; i < Num_menu_options; i++)
    {
        vint_data_item* item = group->add_data_item(nullptr);
        if (item != nullptr)
        {
            vint_variant_type types[9] = {
                VINT_PROP_TYPE_INT, // index
                VINT_PROP_TYPE_INT,
                VINT_PROP_TYPE_STRING, // name
                VINT_PROP_TYPE_INT,
                VINT_PROP_TYPE_STRING,
                VINT_PROP_TYPE_INT, // row type
                VINT_PROP_TYPE_BOOL,
                VINT_PROP_TYPE_BOOL,
                VINT_PROP_TYPE_BOOL, // locked
            };
            item->set_template(types, 9);
            Mp_change_matchmaking_info.group_item_handles.add((int)item->get_handle());
        }
    }
}

// 0x008CFD20 (:381) inlined
static void cmm_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Mp_change_matchmaking_info.data_group_handle = group->get_handle();
        ui_add_data_group(Mp_change_matchmaking_info.doc_handle, Mp_change_matchmaking_info.data_group_handle);
        cmm_load_menu_option_data_items();
    }
}

// 0x008CFD50 (:417)
static void cmm_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Change_matchmaking_data_items, 7);
    cmm_load_data_groups();
}

// 0x008DF660 (:426)
static void cmm_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    cmm_destroy_selection_tween();
    multi_player_menu_system_close_message_box();
}

// 0x0088A7B0 (:435)
static void cmm_process_data_item_matching_hint(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(false));
}

// 0x0088A7E0 (:463)
static void cmm_process_data_item_selection_info(vint_data_item* vdi)
{
    char image_name[32];
    char image_text[128];
    controls_get_button_image_name(image_name, 32, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image_text, 128, "[image:%s]", image_name);
    vdi->set_element(0, vint_variant(Change_matchmaking_selection));
    vdi->set_element(1, vint_variant(image_text));
    vdi->set_element(2, vint_variant(false));
}

// 0x008DF700 (:476)
static void cmm_process_data_item_hint_text(vint_data_item* vdi)
{
    multi_player_menu_system_begin_hints(vdi);
    cmm_hint_config[1] = CMM_HINT_INVALID;
    cmm_hint_config[0] = CMM_HINT_ABORT;
    cmm_num_hints = 1;
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Cancel_string), CBA_MENU_ABORT);
    multi_player_menu_system_add_hint_config(cmm_num_hints, cmm_hint_config, CMM_HINT_CHAT);
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Chat_string), CBA_MENU_Y);
    multi_player_menu_system_end_hints();
}

// 0x008AC930 (:497)
static void cmm_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(Change_playlist_string));
    vdi->set_element(1, vint_variant(Menu_options[Change_matchmaking_selection].playlist->display_name));
}

// 0x008DF7E0 (:503)
static void cmm_process_data_item_playlist_info(vint_data_item* vdi)
{
    mempool_mark_scope scope(&Common_mempool);
    const multi_game_playlist* playlist = Change_matchmaking_selection < 0 ? nullptr : Menu_options[Change_matchmaking_selection].playlist;
    if (playlist == nullptr)
    {
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant(Players_string));
        vdi->set_element(2, vint_variant(Unknown_string));
        vdi->set_element(3, vint_variant(Max_party_size_string));
        vdi->set_element(4, vint_variant(Unknown_string));
        vdi->set_element(5, vint_variant(Modes_string));
        vdi->set_element(6, vint_variant(Unknown_string));
        vdi->set_element(7, vint_variant(Maps_string));
        vdi->set_element(8, vint_variant(Unknown_string));
        vdi->set_element(9, vint_variant(No_playlist_string));
        return;
    }
    char players[16];
    char party_size[16];
    if (playlist->min_players == playlist->max_players)
    {
        sprintf(players, "%d", playlist->min_players);
    }
    else
    {
        sprintf(players, "%d-%d", playlist->min_players, playlist->max_players);
    }
    _snprintf_safe(party_size, 16, "%d", playlist->get_max_party_size());
    bool show_lists = true;
    const char* modes_text;
    const char* maps_text;
    if (playlist->m_query_type == QT_QUICKMATCH)
    {
        modes_text = "MENU_ALL_MODES";
        maps_text = "MENU_ALL_MAPS";
        show_lists = false;
    }
    else
    {
        // The modes and the different levels of the entries (a ring).
        const multi_level_info** levels = (const multi_level_info**)Common_mempool.alloc(multi_game_settings_num_levels() * 4, 0);
        int num_levels = 0;
        // Magic number: 8 is MGPM_NUM_MODES.
        bool modes[8] = {};
        const multi_game_playlist_entry* entry = playlist->entries;
        while (entry != nullptr)
        {
            modes[entry->mode] = true;
            int level = 0;
            while (level < num_levels && entry->level != levels[level])
            {
                level++;
            }
            if (level >= num_levels && num_levels < multi_game_settings_num_levels())
            {
                levels[num_levels] = entry->level;
                num_levels++;
            }
            entry = entry->next;
            if (entry == playlist->entries)
            {
                break;
            }
        }
        // Selection sort by name.
        for (int i = 0; i < num_levels - 1; i++)
        {
            int lowest = i;
            for (int j = i + 1; j < num_levels; j++)
            {
                if (v_stricmp(levels[j]->display_name, levels[lowest]->display_name) < 0)
                {
                    lowest = j;
                }
            }
            if (lowest != i)
            {
                const multi_level_info* swap = levels[i];
                levels[i] = levels[lowest];
                levels[lowest] = swap;
            }
        }
        // Original bug: a name without a localized string is a null pointer here, for the levels and the modes.
        unsigned int maps_length = 1;
        for (int i = 0; i < num_levels; i++)
        {
            maps_length += (unsigned int)wcslen(localization_get_display_string_opt(levels[i]->display_name)) + 1;
        }
        wchar_t* maps_wide = (wchar_t*)Common_mempool.alloc(maps_length * 2, 0);
        maps_wide[0] = L'\0';
        for (int i = 0; i < num_levels; i++)
        {
            wcscat(maps_wide, localization_get_display_string_opt(levels[i]->display_name));
            wcscat(maps_wide, L"\n");
        }
        unsigned int modes_length = 1;
        for (int i = 0; i < 8; i++)
        {
            if (modes[i])
            {
                modes_length += (unsigned int)wcslen(localization_get_display_string_opt(multi_gameplay_mode_get_display_name((multi_gameplay_mode)i))) + 1;
            }
        }
        wchar_t* modes_wide = (wchar_t*)Common_mempool.alloc(modes_length * 2, 0);
        modes_wide[0] = L'\0';
        for (int i = 0; i < 8; i++)
        {
            if (modes[i])
            {
                wcscat(modes_wide, localization_get_display_string_opt(multi_gameplay_mode_get_display_name((multi_gameplay_mode)i)));
                wcscat(modes_wide, L"\n");
            }
        }
        // Magic number: three bytes for each character encoded.
        char* maps_encoded = (char*)Common_mempool.alloc(maps_length * 3, 0);
        char* modes_encoded = (char*)Common_mempool.alloc(modes_length * 3, 0);
        vint_encode_wide_text(maps_encoded, maps_length * 3, maps_wide);
        vint_encode_wide_text(modes_encoded, modes_length * 3, modes_wide);
        modes_text = modes_encoded;
        maps_text = maps_encoded;
    }
    vdi->set_element(0, vint_variant(show_lists));
    vdi->set_element(1, vint_variant(Players_string));
    vdi->set_element(2, vint_variant(players));
    vdi->set_element(3, vint_variant(Max_party_size_string));
    vdi->set_element(4, vint_variant(party_size));
    vdi->set_element(5, vint_variant(Modes_string));
    vdi->set_element(6, vint_variant(modes_text));
    vdi->set_element(7, vint_variant(Maps_string));
    vdi->set_element(8, vint_variant(maps_text));
    vdi->set_element(9, vint_variant(playlist->description));
}

// 0x008AC9B0 (:659)
static void cmm_process_data_item_info_text(vint_data_item* vdi)
{
    if (Change_matchmaking_selection >= 0 && Menu_options[Change_matchmaking_selection].playlist != nullptr && Menu_options[Change_matchmaking_selection].playlist->m_query_type == QT_QUICKMATCH)
    {
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant("MENU_ANY_PLAYLIST_DESC"));
        vdi->set_element(2, vint_variant((const char*)nullptr));
        vdi->set_element(3, vint_variant(false));
        vdi->set_element(4, vint_variant((const char*)nullptr));
    }
    else
    {
        vdi->set_element(0, vint_variant(false));
    }
}

// 0x0088A8D0 (:678)
static void cmm_process_data_item_menu_type(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant((int)0));
}

// 0x008E01E0 (:683)
static void cmm_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find(Mp_change_matchmaking_info.data_group_handle);
    if (group == nullptr)
    {
        return;
    }
    const int num_items = group->get_num_data_items();
    for (int i = 0; i < num_items; i++)
    {
        vint_data_item* item = vint_data_item::find((unsigned int)Mp_change_matchmaking_info.group_item_handles[i]);
        if (item == nullptr)
        {
            continue;
        }
        bool locked = false;
        int num_unlocks = 0;
        const multi_xp_unlock* unlocks = multi_game_xp_get_all_unlocks(num_unlocks);
        for (int u = 0; u < num_unlocks; u++)
        {
            if (unlocks[u].unlock_type == MXUT_PLAYLIST && unlocks[u].data.playlist_id == Menu_options[i].playlist->playlist_id.get_crc())
            {
                locked = multi_game_xp_get_my_level() < unlocks[u].level_required;
                break;
            }
        }
        wchar_t name[128];
        wchar_t text[128];
        char encoded[384];
        const char* display_name = Menu_options[i].playlist->display_name;
        const wchar_t* localized = display_name != nullptr ? localization_get_display_string_opt(display_name) : nullptr;
        if (localized != nullptr)
        {
            wcsncpy(name, localized, 128);
            name[127] = L'\0';
        }
        else
        {
            copy_to_wide(name, display_name, 128);
        }
        _snprintf_safe(text, 128, L"%ls", name);
        vint_encode_wide_text(encoded, 384, text);
        item->set_element(0, vint_variant(i + 1));
        item->set_element(1, vint_variant(-1));
        item->set_element(2, vint_variant(encoded));
        item->set_element(3, vint_variant(-1));
        item->set_element(4, vint_variant(" "));
        // Magic number: the row type the shell draws for a plain option (rfg_ui_mp_shell).
        item->set_element(5, vint_variant(3));
        item->set_element(6, vint_variant(false));
        item->set_element(7, vint_variant(false));
        item->set_element(8, vint_variant(locked));
    }
}

// 0x008E0540 (:748) inlined
static void cmm_process_data_items()
{
    multi_player_menu_system_shell_process_data_items();
    cmm_process_data_group_menu_options();
}

// 0x008F1F90 (:757)
static void cmm_process_loading()
{
    switch (ui_get_doc_asset_state(Mp_change_matchmaking_info.doc_handle))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_change_matchmaking.cpp", 757);
        multi_player_menu_system_shell_unload_doc(Mp_change_matchmaking_info.doc_handle);
        cmm_set_state(MMS_UNLOADING);
        break;
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Mp_change_matchmaking_info.doc_handle);
        cmm_set_state(MMS_UNLOADING);
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        cmm_set_state(MMS_LOADED);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_change_matchmaking.cpp", 757);
        break;
    }
}

// 0x008ACAE0 (:844)
static bool cmm_selection_unlocked(int selection)
{
    const multi_game_playlist* playlist = Menu_options[selection].playlist;
    int num_unlocks = 0;
    const multi_xp_unlock* unlocks = multi_game_xp_get_all_unlocks(num_unlocks);
    for (int i = 0; i < num_unlocks; i++)
    {
        if (unlocks[i].unlock_type == MXUT_PLAYLIST && unlocks[i].data.playlist_id == playlist->playlist_id.get_crc() && multi_game_xp_get_my_level() < unlocks[i].level_required)
        {
            return false;
        }
    }
    return true;
}

// 0x008CFD90 (:866)
// Returns 1 when a row was clicked, else -1.
static int cmm_input_process_mouse(bool& events_consumed)
{
    if (Utility_doc_handle != -1 && ui_is_msgbox_active(-2))
    {
        return -1;
    }
    const int hint = multi_player_menu_system_input_mouse_hints(Mp_change_matchmaking_info.doc_handle, 2);
    if (hint >= 0)
    {
        Current_mouse_hint_choice = (mp_change_matchmaking_hint_choices)cmm_hint_config[hint];
        return -1;
    }
    const int num_rows = Mp_change_matchmaking_info.group_item_handles.size();
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Change_matchmaking_selection, num_rows, Mp_change_matchmaking_info.input_id, Mp_change_matchmaking_info.doc_handle, events_consumed);
    if (selection >= 0 && cmm_selection_unlocked(selection))
    {
        if (selection != Change_matchmaking_selection)
        {
            multi_player_menu_system_set_menu_selection(selection, Change_matchmaking_selection, num_rows);
        }
        if (mouse_just_released(0))
        {
            return 1;
        }
    }
    return -1;
}

// 0x008FAF00 (:897)
static void cmm_process_input()
{
    bool events_consumed = false;
    Current_mouse_hint_choice = CMM_HINT_INVALID;
    const int mouse_action = cmm_input_process_mouse(events_consumed);
    const int input_id = Mp_change_matchmaking_info.input_id;
    // Magic numbers: the button indices of ui_get_button_info (1/5 up, 2/6 down).
    const bool up = ui_get_button_info(1, input_id) > 0.0f || ui_get_button_info(5, input_id) > 0.0f;
    const bool down = ui_get_button_info(2, input_id) > 0.0f || ui_get_button_info(6, input_id) > 0.0f;
    const int num_rows = Mp_change_matchmaking_info.group_item_handles.size();
    const int previous = Change_matchmaking_selection;
    int selection = previous;
    if (up)
    {
        selection = previous - 1;
        if (selection < 0)
        {
            selection = num_rows - 1;
        }
    }
    else if (down)
    {
        selection = previous + 1;
        if (selection >= num_rows)
        {
            selection = 0;
        }
    }
    selection = num_rows < 1 ? -1 : selection % num_rows;
    Change_matchmaking_selection = selection;
    if (up || down)
    {
        if (selection == -1)
        {
            return;
        }
        if (selection != previous)
        {
            // Over the locked playlists, in the direction moved, until back at the start.
            while (!cmm_selection_unlocked(Change_matchmaking_selection))
            {
                if (up)
                {
                    Change_matchmaking_selection--;
                    if (Change_matchmaking_selection < 0)
                    {
                        Change_matchmaking_selection = num_rows - 1;
                    }
                }
                else
                {
                    Change_matchmaking_selection++;
                    if (Change_matchmaking_selection >= num_rows)
                    {
                        Change_matchmaking_selection = 0;
                    }
                }
                if (Change_matchmaking_selection == previous)
                {
                    break;
                }
            }
        }
        if (Change_matchmaking_selection == previous)
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
        else
        {
            audiolib_cue_play(Audio_info[up ? 1 : 0], nullptr, nullptr);
        }
        return;
    }
    // Magic number: 15 is the Y button of ui_get_button_info.
    if (ui_get_button_info(15, input_id) > 0.0f || Current_mouse_hint_choice == CMM_HINT_CHAT)
    {
        // Magic number: the Steam id of a chat group, a constant in rfg.exe (CSteamID 0x017000001F6FF3C).
        SteamFriends()->ActivateGameOverlayToUser("chat", CSteamID(0x017000001F6FF3CULL));
    }
    const bool select = ui_get_button_info(10, input_id) > 0.0f;
    const bool back = ui_get_button_info(11, input_id) > 0.0f || Current_mouse_hint_choice == CMM_HINT_ABORT;
    if (select || mouse_action == 1)
    {
        const multi_game_playlist* playlist = Menu_options[Change_matchmaking_selection].playlist;
        if (multi_game_party_in_party() && playlist->max_party_size < (int)multi_game_party_get_num_players_in_party())
        {
            multi_player_menu_system_pull_up_party_too_large_message_box();
        }
        else
        {
            game_query query;
            query.game_query_base::copy(*multi_game_preferences_get_ideal_query());
            query.m_query_type = playlist->m_query_type;
            query.m_game_type = playlist->game_type;
            query.m_playlist_id = playlist->playlist_id.get_crc();
            query.m_game_mode = playlist->mode_type;
            multi_game_preferences_set_ideal_query(&query);
            player_profile_data* profile = player_profile_get("matchmaking", Player_controller_index == -1 ? 0 : Player_controller_index);
            if (profile != nullptr)
            {
                profile->mp_playlist_id = playlist->playlist_id.get_crc();
            }
            cmm_set_state(MMS_WAITING_FOR_PARTY_TO_GO_MATCHMAKING);
        }
        audiolib_cue_play(Audio_info[4], nullptr, nullptr);
    }
    if (back)
    {
        cmm_ui_pop();
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
    }
}

// 0x008FB3E0 (:1048)
static void cmm_process_loaded()
{
    cmm_process_data_items();
    cmm_process_input();
}

// 0x0088A910 (:1214)
static void cmm_ui_push(game_state new_state)
{
    Mp_change_matchmaking_info.exit_action = MMEA_PUSH;
    Mp_change_matchmaking_info.exit_push_target = new_state;
    cmm_set_state(MMS_UNLOADING);
}

// 0x0088A930 (:1221)
static void cmm_ui_pop()
{
    multi_core_matchmake_internal_stop_search_timer();
    Mp_change_matchmaking_info.exit_action = MMEA_POP;
    cmm_set_state(MMS_UNLOADING);
}

// 0x00917210 (:1234)
void ui_document_mp_change_matchmaking_init()
{
    state_info info;
    info.enter = ui_document_mp_change_matchmaking_enter;
    info.exit = ui_document_mp_change_matchmaking_exit;
    info.process = ui_document_mp_change_matchmaking_process;
    info.handshake = ui_document_mp_change_matchmaking_handshake;
    info.render = ui_document_mp_change_matchmaking_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_CHANGE_MATCHMAKING, info);
}

// 0x0090B370 (:1251)
void ui_document_mp_change_matchmaking_enter(game_state prev_state)
{
    cmm_init_state();
    cmm_init_info();
    multi_player_menu_system_shell_load_doc(Mp_change_matchmaking_info.doc_handle, false);
    Mp_change_matchmaking_info.control_id = ui_request_control_id();
    Mp_change_matchmaking_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    Num_menu_options = 0;
    Menu_options.clear();
    for (const multi_game_playlist* playlist = multi_game_playlist_get_first(); playlist != nullptr; playlist = multi_game_playlist_get_next(playlist))
    {
        const menu_option_info option = {nullptr};
        Menu_options.add(option);
        // Original bug: past 16 playlists the add fails (Int3 message) and this still writes behind the array.
        Menu_options[Num_menu_options].playlist = playlist;
        Num_menu_options++;
    }
    Menu_options.sort(option_sort_less_than_or_equal_to);
}

// 0x008E0550 (:1288)
void ui_document_mp_change_matchmaking_exit(game_state new_state)
{
    // Original bug: the document stays loaded on exit.
    cmm_unload_ui_information();
    ui_relinquish_control_id(Mp_change_matchmaking_info.control_id);
    ui_relinquish_input_id(Mp_change_matchmaking_info.input_id);
}

// 0x00914690 (:1302)
void ui_document_mp_change_matchmaking_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    // A party member doesn't choose: back out.
    if (link->isPartyEnabled() && !link->isPartyOwner() && Mp_change_matchmaking_info.state < MMS_UNLOADING && Mp_change_matchmaking_info.pending_state < MMS_UNLOADED)
    {
        cmm_ui_pop();
    }
    g_gameLinkMutex.unlock();
    ui_add_for_processing(Mp_change_matchmaking_info.doc_handle);
    switch (Mp_change_matchmaking_info.state)
    {
    case MMS_LOADING:
        cmm_process_loading();
        break;
    case MMS_LOADED:
        cmm_process_loaded();
        break;
    case MMS_WAITING_FOR_PARTY_TO_GO_MATCHMAKING:
        if (!s_waitingForPartyToGoMatchmakingTimeout.valid() || s_waitingForPartyToGoMatchmakingTimeout.elapsed())
        {
            Lobby_session->session_delete();
            cmm_ui_push(GS_MULTIPLAYER_SEARCH_MATCHMAKING);
        }
        break;
    case MMS_UNLOADED:
        cmm_set_state(MMS_LOADING);
        break;
    default:
        break;
    }
    ui_process(Mp_change_matchmaking_info.control_id, false);
}

// 0x008F2040 (:1364)
void ui_document_mp_change_matchmaking_handshake(rl_renderer*)
{
    Mp_change_matchmaking_info.prev_state = Mp_change_matchmaking_info.state;
    if (Mp_change_matchmaking_info.pending_state != Mp_change_matchmaking_info.state)
    {
        cmm_do_state_change();
    }
}

// 0x008FB3F0 (:1372)
void ui_document_mp_change_matchmaking_render(rl_renderer*)
{
    if (Mp_change_matchmaking_info.doc_handle == -1 || Mp_change_matchmaking_info.control_id == 0)
    {
        return;
    }
    if (Mp_change_matchmaking_info.state == MMS_LOADED && Mp_change_matchmaking_info.prev_state == MMS_LOADED)
    {
        ui_add_for_rendering(Mp_change_matchmaking_info.doc_handle);
    }
    ui_render(Mp_change_matchmaking_info.control_id, false);
}
