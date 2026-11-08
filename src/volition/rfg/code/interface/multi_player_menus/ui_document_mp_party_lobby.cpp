// Original: volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.cpp
//
// The party lobby (GS_MULTIPLAYER_PARTY_LOBBY, stub_impl.md S7) on rfg_ui_mp_shell: the leader creates a private lobby and
// picks custom game or matchmaking, the members wait for it; a party state machine runs next to the menu's load states.
//
// Original bugs kept (commented): party_lobby_get_prev_type goes forward; the document stays loaded on exit.

#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/sw_api/sw_api.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_data_group.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/multi_server_browser.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

class rl_renderer;

// ---- file-private types (rfg.pdb)
enum mp_menu_states_plobby
{
    MMS_PL_UNLOADING = 0,
    MMS_PL_UNLOADED = 1,
    MMS_PL_LOADING = 2,
    MMS_PL_LOADED = 3,
    MMS_PL_EXITING = 4,
    NUM_MP_PL_MENU_STATES = 5,
};

enum mp_menu_exit_actions_plobby
{
    MMEA_PL_POP = 0,
    MMEA_PL_PUSH = 1,
};

enum party_lobby_hint_choices
{
    PL_HINT_INVALID = -1,
    PL_HINT_ABORT = 0,
    PL_HINT_OPTIONS = 1,
    PL_HINT_FRIENDS_LIST = 2,
    PL_HINT_SETTINGS = 3,
};

// size 0xD4
struct mp_party_shell_info
{
    mp_menu_states_plobby state; // 0x00
    mp_menu_states_plobby pending_state; // 0x04
    mp_menu_states_plobby prev_state; // 0x08
    mp_menu_exit_actions_plobby exit_action; // 0x0C
    game_state exit_push_target; // 0x10
    int doc_handle; // 0x14
    int data_group_handle; // 0x18
    farray<int,16> player_menu_rows; // 0x1C
    farray<unsigned int,16> sorted_player_list; // 0x6C
    unsigned int control_id; // 0xBC
    int input_id; // 0xC0
    bool show_streamed_image; // 0xC4
    bool exit_msg_box_created; // 0xC5
    timestamp_realtime exit_time; // 0xC8
    mp_party_states party_state; // 0xCC
    mp_party_states party_pending_state; // 0xD0
};
static_assert(sizeof(mp_party_shell_info) == 0xD4, "mp_party_shell_info");

// ---- file-static globals
static void party_lobby_process_data_item_hint_text(vint_data_item* vdi);
static void party_lobby_process_data_item_titles(vint_data_item* vdi);
static void party_lobby_process_data_item_info_text(vint_data_item* vdi);
static void party_lobby_process_data_item_selection_info(vint_data_item* vdi);
static void party_lobby_process_data_item_menu_type(vint_data_item* vdi);
static void party_lobby_process_data_item_game_info(vint_data_item* vdi);
static void party_lobby_process_data_item_party_info(vint_data_item* vdi);
static void party_lobby_process_data_item_playlist_info(vint_data_item* vdi);
static void party_lobby_process_data_item_press_start(vint_data_item* vdi);
static const char* Party_lobby_type_names[3] = {"MENU_CUSTOM_GAME", "MENU_MATCHMAKING", "MENU_UNKNOWN"}; // 0x01663604
mp_party_lobby_types Party_lobby_type = MPPLT_MATCHMAKING; // 0x01663610
static mp_shell_data_item_info Party_lobby_data_items[9] = { // 0x01663620
    {MPSDIT_HINT_INFO, party_lobby_process_data_item_hint_text},
    {MPSDIT_TITLES, party_lobby_process_data_item_titles},
    {MPSDIT_INFO_TEXT, party_lobby_process_data_item_info_text},
    {MPSDIT_SELECTION_INFO, party_lobby_process_data_item_selection_info},
    {MPSDIT_MENU_TYPE, party_lobby_process_data_item_menu_type},
    {MPSDIT_GAME_INFO, party_lobby_process_data_item_game_info},
    {MPSDIT_PARTY_INFO, party_lobby_process_data_item_party_info},
    {MPSDIT_PLAYLIST_INFO, party_lobby_process_data_item_playlist_info},
    {MPSDIT_PRESS_START, party_lobby_process_data_item_press_start},
};
// Only ui_document_mp_party_options.cpp reads this.
const char* Party_lobby_invite_type_names[5] = {"MENU_SIT_NONE", "MENU_SIT_ALL", "MENU_SIT_FRIENDS", "MENU_SIT_PRIVATE", "MENU_FRIENDS_LIST"}; // 0x01663668
static const char* Friends_list_string = {"MENU_FRIENDS_LIST"}; // 0x01663678
static const char* Settings_string = {"MENU_SETTINGS"}; // 0x01663680
static const char* Players_string_plobby = {"MENU_PLAYERS"}; // 0x01663684
static const char* Hint_string = {"MENU_LEAVE_PARTY"}; // 0x0166369C
static const char* Confirm_exit_title_string_plobby = {"MENU_MBOX_CONFIRM_EXIT"}; // 0x016636A8
static const char* Confirm_exit_desc_string_plobby = {"MENU_CONFIRM_EXIT"}; // 0x016636AC
static const char* Load_image_string_plobby = {"MENU_LOADING_IMAGE"}; // 0x016636B0
static const char* Party_options_string = {"MENU_PARTY_OPTIONS"}; // 0x016636B4
static party_lobby_hint_choices Current_mouse_hint_choice_plobby = PL_HINT_INVALID; // 0x016636BC
static int Mouse_arrow_clicked_plobby = -1; // 0x016636C0
static mp_party_shell_info Party_lobby_menu_info; // 0x016667F0
static timestamp_realtime Party_lobby_join_timeout; // 0x016668C4
static int Party_lobby_hint_config[4]; // 0x02BF2180
static bool Selection_is_local_player; // 0x02C0360E
static bool Party_lobby_joined_lobby_session; // 0x02C0360F
static int Party_lobby_menu_selection; // 0x02C0361C
static bool Party_lobby_loaded; // 0x02C03620
static bool Auto_retry_game_launch; // 0x02C03621
static bool Show_connecting_screen; // 0x02C03622
static bool Start_game_clicked_plobby; // 0x02C03623
static int Party_lobby_num_hints; // 0x02C03624

// ---- internal functions
static void party_lobby_unload_ui_information();
static void party_lobby_load_ui_information();
static void party_lobby_request_type_update();
static void party_lobby_ui_push(game_state new_state);
static void party_lobby_ui_pop(bool immediate);

// Port helper: the matchmaking menu's copy of the party, rfg.exe inlines the loop into the two party states.
static void party_lobby_copy_party_player_data()
{
    ui_document_mp_matchmaking_clear_party_player_data();
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false); data != multi_game_data_end(ST_GAME); data = multi_game_data_next(data, false, false))
    {
        ui_document_mp_matchmaking_add_party_player_data(data);
    }
}

// Port helper: the writers of a packet without payload, inlined in rfg.exe.
static unsigned int party_lobby_start_empty_packet(unsigned char* packet, unsigned char type)
{
    packet[0] = type;
    const unsigned int size = Multi_packets[type].group_id != 0 ? 5 : 3; // in-order packets carry a group header
    // The size field doesn't count the three bytes of type and size.
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    return size;
}

// 0x008ADD00 (:278)
static bool party_lobby_leave_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (choice == MBC_OK)
    {
        Party_lobby_menu_info.exit_action = MMEA_PL_POP;
        Party_lobby_menu_info.pending_state = MMS_PL_EXITING;
    }
    return true;
}

// 0x008ADD20 (:286)
static void party_lobby_do_party_state_change()
{
    do
    {
        Party_lobby_menu_info.party_state = Party_lobby_menu_info.party_pending_state;
        switch (Party_lobby_menu_info.party_pending_state)
        {
        case MPS_HOST_CREATING_LOBBY:
            if (Lobby_session->get_state() == MSS_NONE || !Lobby_session->get_info()->private_match)
            {
                Show_connecting_screen = true;
                multi_game_lobby_create(false, true, false, true);
            }
            break;
        case MPS_HOST_LOBBY_READY:
        {
            if (multi_game_party_is_locked_down())
            {
                multi_game_party_unlock();
            }
            unsigned char packet[1200];
            const unsigned int size = party_lobby_start_empty_packet(packet, PT_RETURN_TO_LOBBY);
            multi_io_send_to_all_reliable(Lobby_session, packet, size);
            Show_connecting_screen = false;
            break;
        }
        case MPS_HOST_WAITING_ON_PARTY:
            party_lobby_copy_party_player_data();
            break;
        case MPS_HOST_STARTING_GAME:
            if (Party_lobby_type == MPPLT_CUSTOM_GAME)
            {
                multi_game_lobby_signal_game_start();
            }
            else if (Party_lobby_type == MPPLT_MATCHMAKING)
            {
                if (Lobby_session->get_state() == MSS_READY || Lobby_session->get_state() == MSS_REGISTERED)
                {
                    Lobby_session->session_delete();
                }
                party_lobby_ui_push(GS_MULTIPLAYER_CHANGE_MATCHMAKING);
            }
            else
            {
                return;
            }
            break;
        case MPS_CLIENT_WAITING_FOR_LOBBY:
            // Magic number: 20.5 s for the leader's lobby.
            Party_lobby_join_timeout.set(0x5014);
            Show_connecting_screen = true;
            break;
        case MPS_CLIENT_WAITING_FOR_SERVER:
            party_lobby_request_type_update();
            Show_connecting_screen = false;
            Party_lobby_join_timeout.invalidate();
            break;
        case MPS_CLIENT_MOVING_TO_MATCHMAKING:
            party_lobby_copy_party_player_data();
            multi_game_lobby_leave(true);
            break;
        case MPS_ERROR:
            Show_connecting_screen = false;
            return;
        default:
            return;
        }
    }
    while (Party_lobby_menu_info.party_state != Party_lobby_menu_info.party_pending_state);
}

// 0x008F29D0 (:384)
static void party_lobby_do_state_change()
{
    do
    {
        Party_lobby_menu_info.state = Party_lobby_menu_info.pending_state;
        switch (Party_lobby_menu_info.pending_state)
        {
        case MMS_PL_UNLOADING:
            if (Party_lobby_menu_info.exit_msg_box_created || Loading_screen_ref_cnt > 0)
            {
                ui_remove_loading_screen();
            }
            Show_connecting_screen = false;
            party_lobby_unload_ui_information();
            if (Party_lobby_menu_info.exit_action == MMEA_PL_POP)
            {
                gameseq_pop_state();
                Multi_partied = false;
            }
            else if (Party_lobby_menu_info.exit_action == MMEA_PL_PUSH)
            {
                gameseq_push_state(Party_lobby_menu_info.exit_push_target, true, true);
            }
            Party_lobby_menu_info.pending_state = MMS_PL_UNLOADED;
            break;
        case MMS_PL_UNLOADED:
            Party_lobby_loaded = false;
            return;
        case MMS_PL_LOADING:
            Party_lobby_menu_info.exit_msg_box_created = false;
            return;
        case MMS_PL_LOADED:
            party_lobby_load_ui_information();
            Party_lobby_loaded = true;
            Multi_partied = true;
            break;
        case MMS_PL_EXITING:
            Party_lobby_menu_info.exit_time.set(0);
            Party_lobby_menu_info.exit_msg_box_created = false;
            multi_game_party_leave_party(true);
            multi_game_lobby_leave(true);
            break;
        default:
            return;
        }
    }
    while (Party_lobby_menu_info.state != Party_lobby_menu_info.pending_state);
}

// 0x0088C760 (:455)
static void party_lobby_set_state(mp_menu_states_plobby new_state)
{
    Party_lobby_menu_info.pending_state = new_state;
}

// 0x0088C770 (:472)
static void party_lobby_set_party_state(mp_party_states new_state)
{
    Party_lobby_menu_info.party_pending_state = new_state;
}

// 0x0088C780 (:481) inlined
static void party_lobby_init_state()
{
    Party_lobby_menu_info.state = MMS_PL_UNLOADED;
    Party_lobby_menu_info.pending_state = MMS_PL_UNLOADED;
    Party_lobby_menu_info.prev_state = MMS_PL_UNLOADED;
    Party_lobby_menu_info.party_state = MPS_NONE;
    Party_lobby_menu_info.party_pending_state = MPS_NONE;
}

// 0x008ADFB0 (:492) inlined
static void party_lobby_init_info()
{
    Party_lobby_menu_info.player_menu_rows.clear();
    Party_lobby_menu_info.sorted_player_list.clear();
    Party_lobby_menu_info.data_group_handle = -1;
}

// 0x0088C7B0 (:502)
// First me, then the party, then my team (team modes), then by name.
static bool player_sort_less_than_or_equal_to_plobby(const unsigned int& lhs_handle, const unsigned int& rhs_handle)
{
    if (lhs_handle == rhs_handle)
    {
        return true;
    }
    const multi_game_data* me = multi_game_data_get_my_data(ST_GAME, false, false);
    const multi_game_data* lhs = multi_game_data_get(lhs_handle, false, false);
    const multi_game_data* rhs = multi_game_data_get(rhs_handle, false, false);
    if (lhs == nullptr)
    {
        return false;
    }
    if (rhs == nullptr || lhs == me)
    {
        return true;
    }
    if (rhs == me)
    {
        return false;
    }
    if (Lobby_session->get_player_connection(lhs->player_uid) == nullptr || Lobby_session->get_player_connection(rhs->player_uid) == nullptr)
    {
        return false;
    }
    if (multi_game_party_in_party())
    {
        const bool lhs_party = multi_game_party_has_player(lhs->player_uid);
        const bool rhs_party = multi_game_party_has_player(rhs->player_uid);
        if (lhs_party)
        {
            if (!rhs_party)
            {
                return true;
            }
        }
        else if (rhs_party)
        {
            return false;
        }
    }
    if (Mp_server_options.get_mode() != MGPM_NONE && multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
    {
        const human_teams my_team = me->team;
        if (lhs->team == my_team)
        {
            if (rhs->team != my_team)
            {
                return true;
            }
        }
        else if (rhs->team == my_team)
        {
            return false;
        }
    }
    return _v_stricmp(lhs->display_name, rhs->display_name) < 1;
}

// 0x008C0DC0 (:577)
static void party_lobby_add_player_row(vint_data_group* vdg)
{
    vint_data_item* item = vdg->add_data_item(nullptr);
    if (item != nullptr)
    {
        vint_variant_type types[8] = {
            VINT_PROP_TYPE_INT, // index
            VINT_PROP_TYPE_INT, // badge
            VINT_PROP_TYPE_STRING, // name
            VINT_PROP_TYPE_INT, // team
            VINT_PROP_TYPE_STRING, // level
            VINT_PROP_TYPE_INT, // voice state
            VINT_PROP_TYPE_BOOL, // party leader
            VINT_PROP_TYPE_BOOL, // in the party
        };
        item->set_template(types, 8);
        Party_lobby_menu_info.player_menu_rows.add((int)item->get_handle());
    }
}

// 0x008ADFD0 (:603)
static void party_lobby_remove_player_row(vint_data_group* vdg)
{
    const int last = Party_lobby_menu_info.player_menu_rows.size() - 1;
    vdg->remove_data_item(Party_lobby_menu_info.player_menu_rows[last]);
    Party_lobby_menu_info.player_menu_rows.resize(last);
}

// 0x008D3300 (:617) inlined
static void party_lobby_load_data_groups()
{
    vint_data_group* group = vint_data_group::create_new("MpShellPlayerInfo");
    if (group != nullptr)
    {
        Party_lobby_menu_info.data_group_handle = group->get_handle();
        ui_add_data_group(Party_lobby_menu_info.doc_handle, Party_lobby_menu_info.data_group_handle);
    }
}

// 0x008D3340 (:653)
static void party_lobby_load_ui_information()
{
    multi_player_menu_system_shell_load_data_items(Party_lobby_data_items, 9);
    party_lobby_load_data_groups();
}

// 0x008E33D0 (:662)
static void party_lobby_unload_ui_information()
{
    multi_player_menu_system_shell_unload_data_items();
    party_lobby_init_info();
    multi_player_menu_system_close_message_box();
}

// 0x008E3460 (:671)
static void party_lobby_process_data_item_hint_text(vint_data_item* vdi)
{
    Party_lobby_hint_config[1] = PL_HINT_INVALID;
    Party_lobby_hint_config[2] = PL_HINT_INVALID;
    Party_lobby_hint_config[3] = PL_HINT_INVALID;
    multi_player_menu_system_begin_hints(vdi);
    Party_lobby_hint_config[0] = PL_HINT_ABORT;
    Party_lobby_num_hints = 1;
    multi_player_menu_system_add_hint(localization_get_display_string_opt(Hint_string), CBA_MENU_ABORT);
    if (!multi_game_lobby_game_starting())
    {
        if (Lobby_session->am_i_host() && multi_game_party_in_party() && multi_game_party_am_i_leader(false) && SW_HasInvites() != 0)
        {
            multi_player_menu_system_add_hint_config(Party_lobby_num_hints, Party_lobby_hint_config, PL_HINT_OPTIONS);
            multi_player_menu_system_add_hint(localization_get_display_string_req(Party_options_string, nullptr), CBA_MENU_X);
        }
        if (multi_game_party_in_party() && SW_HasInvites() != 0)
        {
            multi_player_menu_system_add_hint_config(Party_lobby_num_hints, Party_lobby_hint_config, PL_HINT_FRIENDS_LIST);
            multi_player_menu_system_add_hint(localization_get_display_string_req(Friends_list_string, nullptr), CBA_MENU_Y);
        }
        if (Lobby_session->am_i_host() && Party_lobby_type == MPPLT_CUSTOM_GAME)
        {
            multi_player_menu_system_add_hint_config(Party_lobby_num_hints, Party_lobby_hint_config, PL_HINT_SETTINGS);
            multi_player_menu_system_add_hint(localization_get_display_string_req(Settings_string, nullptr), CBA_MENU_R2);
        }
    }
    multi_player_menu_system_end_hints();
}

// 0x0088C8E0 (:719)
static void party_lobby_process_data_item_titles(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant("MENU_TIP_PARTY_LOBBY"));
    vdi->set_element(1, vint_variant(Party_lobby_type_names[Party_lobby_type]));
}

// 0x0088C950 (:725)
static bool party_lobby_trying_to_launch()
{
    return Party_lobby_menu_info.party_state > MPS_HOST_LOBBY_READY && Party_lobby_menu_info.party_state < MPS_CLIENT_WAITING_FOR_LOBBY;
}

// 0x008E35F0 (:739)
static void party_lobby_process_data_item_info_text(vint_data_item* vdi)
{
    const bool in_custom_match = Live_server_browser.isInCustomMatch();
    multi_player_uid leader;
    if (!multi_game_party_get_leader(&leader) && !in_custom_match)
    {
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant("MENU_MSJS_JOIN_IN_PROGRESS"));
        vdi->set_element(2, vint_variant(""));
    }
    else if (Party_lobby_type == MPPLT_MATCHMAKING && !in_custom_match)
    {
        wchar_t leader_name[256];
        wchar_t text[512];
        char encoded[1536];
        if (!multi_game_party_get_player_name(leader, leader_name, 256))
        {
            leader_name[0] = L' ';
            leader_name[1] = L'\0';
        }
        const unsigned int num_players = multi_game_party_get_num_players_in_party();
        const wchar_t* size_label = localization_get_display_string_req("MENU_PARTY_SIZE", nullptr);
        const wchar_t* leader_label = localization_get_display_string_req("MENU_PARTY_LEADER", nullptr);
#pragma warning(suppress : 4996) // deprecated form, behaviour kept
        swprintf(text, L"%ls:\r %ls\n%ls:\r %d", leader_label, leader_name, size_label, num_players);
        vint_encode_wide_text(encoded, 1536, text);
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant(encoded));
        vdi->set_element(2, vint_variant(""));
    }
    else
    {
        // The custom game's level image, or a word while it loads.
        wchar_t text[128] = {};
        char image_name[64] = {};
        char encoded[384];
        if (Party_lobby_menu_info.show_streamed_image)
        {
            const char* image = multi_player_menu_system_level_image_get();
            if (image == nullptr)
            {
                _snprintf_safe(text, 128, L"%ls", localization_get_display_string_req(Load_image_string_plobby, nullptr));
            }
            else
            {
                strcpy(image_name, image);
            }
        }
        vint_encode_wide_text(encoded, 384, text);
        vdi->set_element(0, vint_variant(true));
        vdi->set_element(1, vint_variant(encoded));
        vdi->set_element(2, vint_variant(image_name));
    }
    if (Lobby_session->get_state() == MSS_READY && !party_lobby_trying_to_launch())
    {
        vdi->set_element(3, vint_variant(false));
        vdi->set_element(4, vint_variant(""));
    }
    else
    {
        vdi->set_element(3, vint_variant(true));
        vdi->set_element(4, vint_variant("MENU_WORKING"));
    }
}

// 0x008E3960 (:803)
static void party_lobby_process_data_item_game_info(vint_data_item* vdi)
{
    if (!console_is_chat_window_enabled())
    {
        Ui_text_chat_enabled = true;
        console_enable_chat_window(VCP_PARTY);
    }
    const bool show = Lobby_session->is_connected() && Party_lobby_type == MPPLT_CUSTOM_GAME && multi_game_lobby_has_game_settings();
    vdi->set_element(0, vint_variant(show));
    if (show)
    {
        multi_player_menu_system_shell_process_game_info_item(vdi);
    }
}

// 0x0088C970 (:822)
static void party_lobby_process_data_item_press_start(vint_data_item* vdi)
{
    if (Lobby_session->get_state() != MSS_READY || party_lobby_trying_to_launch())
    {
        vdi->set_element(0, vint_variant(false));
        return;
    }
    vdi->set_element(0, vint_variant(true));
    if (multi_game_lobby_waiting_for_user_launch())
    {
        char image_name[32];
        char image_text[128];
        controls_get_button_image_name(image_name, 32, CBA_MENU_START, Player_controller_index);
        keen::formatString(image_text, 128, "[image:%s]", image_name);
        vdi->set_element(1, vint_variant(image_text));
        vdi->set_element(2, vint_variant("MENU_MP_WORD_READY"));
        vdi->set_element(3, vint_variant("MENU_START_GAME"));
    }
    else
    {
        vdi->set_element(1, vint_variant((const char*)nullptr));
        vdi->set_element(2, vint_variant(""));
        vdi->set_element(3, vint_variant(Lobby_session->am_i_host() ? "MENU_WORKING" : "MENU_WAIT_FOR_LEADER"));
    }
}

// 0x0088CB80 (:853)
static void party_lobby_process_data_item_selection_info(vint_data_item* vdi)
{
    char image_name[32];
    char image_text[128];
    controls_get_button_image_name(image_name, 32, CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(image_text, 128, "[image:%s]", image_name);
    vdi->set_element(0, vint_variant(Party_lobby_menu_selection));
    vdi->set_element(1, vint_variant(image_text));
    vdi->set_element(2, vint_variant(Selection_is_local_player && multi_game_lobby_allow_team_changing()));
}

// 0x0088CC80 (:866)
static void party_lobby_process_data_item_menu_type(vint_data_item* vdi)
{
    // Magic number: the shell's menu layout with the player list (rfg_ui_mp_shell).
    vdi->set_element(0, vint_variant(1));
}

// 0x008E39F0 (:871)
static void party_lobby_process_data_item_party_info(vint_data_item* vdi)
{
    int num_team_0 = 0;
    int num_team_1 = 0;
    int num_spectators = 0;
    int num_players = 0;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false); data != multi_game_data_end(ST_GAME); data = multi_game_data_next(data, false, false))
    {
        // Magic numbers: human_teams 0 and 1 are the two sides, 3 spectates.
        if (data->team == 0)
        {
            num_team_0++;
        }
        else if (data->team == 1)
        {
            num_team_1++;
        }
        else if (data->team == 3)
        {
            num_spectators++;
        }
        num_players++;
    }
    wchar_t text[128];
    char encoded[384];
    if (Party_lobby_type != MPPLT_CUSTOM_GAME)
    {
        _snprintf_safe(text, 128, L"%ls (%d)", localization_get_display_string_opt(Players_string_plobby), num_players);
    }
    else if (multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
    {
        const wchar_t* team_1_name = human_team_get_display_name((human_teams)1);
        const wchar_t* team_0_name = human_team_get_display_name((human_teams)0);
        _snprintf_safe(text, 128, L"%ls (%d) %ls (%d) [format][scale:1.0][image_tint:ui_fend_icon_spec][/format] (%d)", team_0_name, num_team_0, team_1_name, num_team_1, num_spectators);
    }
    else
    {
        _snprintf_safe(text, 128, L"%ls (%d) [format][scale:1.0][image_tint:ui_fend_icon_spec][/format] (%d)", localization_get_display_string_opt(Players_string_plobby), num_players, num_spectators);
    }
    vint_encode_wide_text(encoded, 384, text);
    vdi->set_element(0, vint_variant(num_players > 0));
    vdi->set_element(1, vint_variant(encoded));
}

// 0x0088CCB0 (:913)
static void party_lobby_process_data_item_playlist_info(vint_data_item* vdi)
{
    vdi->set_element(0, vint_variant(false));
}

// 0x008E3C10 (:956)
static void party_lobby_update_player_data(int index, const multi_game_data* data)
{
    vint_data_item* item = vint_data_item::find((unsigned int)Party_lobby_menu_info.player_menu_rows[index]);
    if (item == nullptr)
    {
        return;
    }
    wchar_t name[64];
    char encoded[192];
    if (data != nullptr)
    {
        _snprintf_safe(name, 64, L"%ls", data->display_name);
    }
    else
    {
        wcsncpy(name, localization_get_display_string_opt("MENU_UNKNOWN"), 64);
        name[63] = L'\0';
    }
    vint_encode_wide_text(encoded, 192, name);
    // Magic numbers: row 2 and voice state 1 are the shell's defaults for a player without data.
    int team = 2;
    int talk_state = 1;
    int badge = -1;
    char level[8] = "--";
    if (data != nullptr)
    {
        str_i2s(level, 8, data->player_level);
        badge = (int)data->player_badge;
        talk_state = (int)multi_voice_get_icon_talk_state(data->player_uid);
        if (Party_lobby_type == MPPLT_CUSTOM_GAME)
        {
            team = (int)data->team;
        }
    }
    item->set_element(0, vint_variant(index + 1));
    item->set_element(1, vint_variant(badge));
    item->set_element(2, vint_variant(encoded));
    item->set_element(3, vint_variant(team));
    item->set_element(4, vint_variant(level));
    item->set_element(5, vint_variant(talk_state));
    bool is_leader = false;
    bool in_party = false;
    if (data != nullptr && multi_game_party_in_party())
    {
        // The lobby's host stands for the leader.
        multi_player_uid leader;
        if (multi_game_party_get_leader(&leader) && Lobby_session->get_host_connection() != nullptr && Lobby_session->get_host_connection()->game_data == data->handle)
        {
            is_leader = true;
        }
        in_party = multi_game_party_has_player(data->player_uid);
    }
    item->set_element(6, vint_variant(is_leader));
    item->set_element(7, vint_variant(in_party));
    item->set_element(8, vint_variant(false));
}

// 0x008E3F30 (:1013)
static void party_lobby_process_data_group_menu_options()
{
    vint_data_group* group = vint_data_group::find(Party_lobby_menu_info.data_group_handle);
    if (Lobby_session == nullptr || Lobby_session->get_state() == MSS_NONE || Lobby_session->get_state() == MSS_DELETING)
    {
        while (group->get_num_data_items() != 0)
        {
            party_lobby_remove_player_row(group);
        }
        return;
    }
    if (group == nullptr)
    {
        return;
    }
    Party_lobby_menu_info.sorted_player_list.clear();
    unsigned int num_players = 0;
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false); data != multi_game_data_end(ST_GAME); data = multi_game_data_next(data, false, false))
    {
        Party_lobby_menu_info.sorted_player_list.add(data->handle);
        num_players++;
    }
    Party_lobby_menu_info.sorted_player_list.sort(player_sort_less_than_or_equal_to_plobby);
    for (unsigned int i = 0; i < num_players; i++)
    {
        if ((unsigned int)group->get_num_data_items() <= i)
        {
            party_lobby_add_player_row(group);
        }
        party_lobby_update_player_data((int)i, multi_game_data_get(Party_lobby_menu_info.sorted_player_list[(int)i], false, false));
    }
    while (num_players < (unsigned int)group->get_num_data_items())
    {
        party_lobby_remove_player_row(group);
    }
}

// 0x008F2AC0 (:1058)
static void party_lobby_process_loading()
{
    switch (ui_get_doc_asset_state(Party_lobby_menu_info.doc_handle))
    {
    case ST_UNLOADED:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_party_lobby.cpp", 1058);
        multi_player_menu_system_shell_unload_doc(Party_lobby_menu_info.doc_handle);
        party_lobby_set_state(MMS_PL_UNLOADING);
        break;
    case ST_ERROR:
        multi_player_menu_system_shell_unload_doc(Party_lobby_menu_info.doc_handle);
        party_lobby_set_state(MMS_PL_UNLOADING);
        break;
    case ST_LOADING:
        break;
    case ST_LOADED:
        party_lobby_set_state(MMS_PL_LOADED);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "ui_document_mp_party_lobby.cpp", 1058);
        break;
    }
}

// 0x008D3390 (:1130)
static void party_lobby_input_process_mouse(bool& events_consumed)
{
    Start_game_clicked_plobby = false;
    Mouse_arrow_clicked_plobby = -1;
    mp_game_input_process_chatmouse();
    if (multi_core_platform_ui_is_active())
    {
        return;
    }
    // Only with the input focus: the lock, else the newest request.
    int focus = Current_input_lock_id;
    if (focus == -1)
    {
        focus = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    if (Party_lobby_menu_info.input_id != focus)
    {
        return;
    }
    const int hint = multi_player_menu_system_input_mouse_hints(Party_lobby_menu_info.doc_handle, 4);
    if (hint >= 0)
    {
        Current_mouse_hint_choice_plobby = (party_lobby_hint_choices)Party_lobby_hint_config[hint];
        return;
    }
    const char* start_names[1] = {"game_start_click"};
    Start_game_clicked_plobby = ui_mouse_process_hints(start_names, 1, Party_lobby_menu_info.doc_handle, true) != -1;
    const char* arrow_names[2] = {"row_highlight_left", "row_highlight_right"};
    Mouse_arrow_clicked_plobby = ui_mouse_process_hints(arrow_names, 2, Party_lobby_menu_info.doc_handle, true);
    if (Mouse_arrow_clicked_plobby != -1)
    {
        return;
    }
    const int num_players = Party_lobby_menu_info.sorted_player_list.size();
    const int selection = multi_player_menu_system_handle_menu_mouse_input(Party_lobby_menu_selection, num_players, Party_lobby_menu_info.input_id, Party_lobby_menu_info.doc_handle, events_consumed);
    if (selection >= 0)
    {
        if (selection != Party_lobby_menu_selection)
        {
            multi_player_menu_system_set_menu_selection(selection, Party_lobby_menu_selection, num_players);
        }
        if (mouse_just_released(0))
        {
            events_consumed = true;
        }
    }
}

// 0x0090CC20 (:1173)
static void party_lobby_process_input()
{
    bool events_consumed = false;
    Current_mouse_hint_choice_plobby = PL_HINT_INVALID;
    party_lobby_input_process_mouse(events_consumed);
    if (events_consumed)
    {
        return;
    }
    multi_player_menu_system_handle_menu_key_input(Party_lobby_menu_selection, Party_lobby_menu_info.sorted_player_list.size(), Party_lobby_menu_info.input_id, events_consumed);
    if (events_consumed)
    {
        return;
    }
    const int input_id = Party_lobby_menu_info.input_id;
    // Magic numbers: the button indices of ui_get_button_info.
    const bool player_options = ui_get_button_info(0x20, input_id) > 0.0f;
    const bool left = ui_get_button_info(3, input_id) > 0.0f || ui_get_button_info(7, input_id) > 0.0f || Mouse_arrow_clicked_plobby == 0;
    const bool right = ui_get_button_info(4, input_id) > 0.0f || ui_get_button_info(8, input_id) > 0.0f || Mouse_arrow_clicked_plobby == 1;
    const bool back = ui_get_button_info(0xB, input_id) > 0.0f || Current_mouse_hint_choice_plobby == PL_HINT_ABORT;
    const bool options = ui_get_button_info(0xE, input_id) > 0.0f || Current_mouse_hint_choice_plobby == PL_HINT_OPTIONS;
    const bool friends = ui_get_button_info(0xF, input_id) > 0.0f || Current_mouse_hint_choice_plobby == PL_HINT_FRIENDS_LIST;
    const bool settings = ui_get_button_info(0x13, input_id) > 0.0f || Current_mouse_hint_choice_plobby == PL_HINT_SETTINGS;
    const bool start = ui_get_button_info(9, input_id) > 0.0f || Start_game_clicked_plobby;
    if (player_options)
    {
        if (Party_lobby_menu_selection >= 0 && Party_lobby_menu_selection < Party_lobby_menu_info.sorted_player_list.size())
        {
            const multi_game_data* player = multi_game_data_get(Party_lobby_menu_info.sorted_player_list[Party_lobby_menu_selection], false, false);
            if (player != nullptr && (multi_game_lobby_has_player_list() || Party_lobby_menu_selection == 0))
            {
                multi_player_menu_system_pull_up_player_option_message_box(player->player_uid, player->display_name, true);
            }
            audiolib_cue_play(Audio_info[4], nullptr, nullptr);
        }
        else
        {
            audiolib_cue_play(Audio_info[11], nullptr, nullptr);
        }
        return;
    }
    if (back)
    {
        multi_player_menu_system_pull_up_generic_message_box(MBT_ACCEPT_CANCEL_SELECT, Confirm_exit_title_string_plobby, Confirm_exit_desc_string_plobby, party_lobby_leave_cb, nullptr, nullptr);
        audiolib_cue_play(Audio_info[5], nullptr, nullptr);
        return;
    }
    if (Selection_is_local_player && (left || right) && multi_game_lobby_allow_team_changing())
    {
        if (left)
        {
            multi_game_player_decrement_my_team(Lobby_session);
            audiolib_cue_play(Audio_info[7], nullptr, nullptr);
        }
        else
        {
            multi_game_player_increment_my_team(Lobby_session);
            audiolib_cue_play(Audio_info[8], nullptr, nullptr);
        }
        return;
    }
    if (friends)
    {
        if (!multi_core_platform_ui_is_active() && SW_HasInvites() != 0)
        {
            ui_wait_for_button_ups();
            multi_core_platform_show_friends_ui();
            audiolib_cue_play(Audio_info[6], nullptr, nullptr);
        }
        return;
    }
    if (options && multi_game_party_in_party() && multi_game_party_am_i_leader(false) && SW_HasInvites() != 0)
    {
        party_lobby_ui_push(GS_MULTIPLAYER_PARTY_OPTIONS);
        audiolib_cue_play(Audio_info[6], nullptr, nullptr);
        return;
    }
    if (!multi_game_lobby_waiting_for_user_launch())
    {
        return;
    }
    if (Party_lobby_type == MPPLT_CUSTOM_GAME)
    {
        if (settings)
        {
            party_lobby_ui_push(GS_MULTIPLAYER_CHANGE_GAME_OPTIONS);
            audiolib_cue_play(Audio_info[6], nullptr, nullptr);
        }
        else if (Lobby_session->get_state() == MSS_READY && start)
        {
            party_lobby_set_party_state(MPS_HOST_WAITING_ON_PARTY);
            audiolib_cue_play(Ga_cue_ids.multiplayer_select_start, nullptr, nullptr);
        }
    }
    else if (Party_lobby_type == MPPLT_MATCHMAKING && multi_game_party_in_party() && Lobby_session->get_state() == MSS_READY && start)
    {
        if (Party_lobby_menu_info.party_state != MPS_HOST_WAITING_ON_PARTY)
        {
            party_lobby_set_party_state(MPS_HOST_WAITING_ON_PARTY);
        }
        audiolib_cue_play(Ga_cue_ids.multiplayer_select_start, nullptr, nullptr);
    }
}

// 0x0090D090 (:1286)
static void party_lobby_process_loaded()
{
    if (Show_connecting_screen)
    {
        if (Loading_screen_ref_cnt < 1)
        {
            ui_show_loading_screen(false, "MENU_MSJS_JOIN_IN_PROGRESS");
        }
    }
    else if (Loading_screen_ref_cnt > 0)
    {
        ui_remove_loading_screen();
    }
    Selection_is_local_player = Party_lobby_menu_selection >= 0 && multi_game_data_get_my_handle(ST_GAME) == Party_lobby_menu_info.sorted_player_list[Party_lobby_menu_selection];
    party_lobby_process_input();
}

// 0x0090D100 (:1450)
static void party_lobby_process_exit()
{
    if (multi_game_party_in_party())
    {
        multi_game_party_leave_party(multi_game_party_get_num_players_in_party() > 1);
    }
    if (Lobby_session == nullptr || Lobby_session->get_state() == MSS_NONE)
    {
        party_lobby_set_state(MMS_PL_UNLOADING);
        return;
    }
    if (Lobby_session->get_state() == MSS_READY)
    {
        multi_game_lobby_leave(true);
        return;
    }
    // Magic number: 250 ms of deleting before the disconnect screen shows.
    if (Lobby_session->get_state() == MSS_DELETING && !Party_lobby_menu_info.exit_msg_box_created && Party_lobby_menu_info.exit_time.time_since() > 250)
    {
        ui_show_loading_screen(false, "MENU_DISCONNECT");
        Party_lobby_menu_info.exit_msg_box_created = true;
    }
}

// 0x0088CD00 (:1494)
static void party_lobby_ui_push(game_state new_state)
{
    Party_lobby_menu_info.exit_action = MMEA_PL_PUSH;
    Party_lobby_menu_info.exit_push_target = new_state;
    party_lobby_set_state(MMS_PL_UNLOADING);
}

// 0x0088CD20 (:1505)
static void party_lobby_ui_pop(bool immediate)
{
    Party_lobby_menu_info.exit_action = MMEA_PL_POP;
    party_lobby_set_state(immediate ? MMS_PL_UNLOADING : MMS_PL_EXITING);
}

// 0x0088CD40 (:1527)
static void party_lobby_request_type_update()
{
    unsigned char packet[1200];
    const unsigned int size = party_lobby_start_empty_packet(packet, PT_PRIVATE_LOBBY_TYPE_REQ);
    multi_io_send_reliable(Lobby_session->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
}

// 0x0090D1A0 (:1542)
void ui_document_mp_party_lobby_enter(game_state prev_state)
{
    Show_connecting_screen = true;
    Party_lobby_menu_info.exit_action = MMEA_PL_POP;
    Party_lobby_menu_info.exit_push_target = GS_MULTIPLAYER_LIVE;
    Party_lobby_menu_info.show_streamed_image = true;
    Party_lobby_menu_selection = 0;
    Party_lobby_joined_lobby_session = false;
    party_lobby_init_info();
    party_lobby_init_state();
    multi_player_menu_system_shell_load_doc(Party_lobby_menu_info.doc_handle, false);
    Party_lobby_menu_info.control_id = ui_request_control_id();
    Party_lobby_menu_info.input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    multi_game_party_clear_left_party_by_choice();
    Party_lobby_loaded = false;
}

// 0x008E4130 (:1590)
void ui_document_mp_party_lobby_exit(game_state new_state)
{
    // Original bug: the document stays loaded on exit.
    console_disable_chat_window();
    Ui_text_chat_enabled = false;
    party_lobby_unload_ui_information();
    ui_relinquish_control_id(Party_lobby_menu_info.control_id);
    ui_relinquish_input_id(Party_lobby_menu_info.input_id);
    if (gameseq_get_pending_state() == GS_LOAD)
    {
        multi_game_lobby_push_to_game();
    }
    multi_player_menu_system_level_image_reset();
}

// 0x008F2B70 (:1622)
// A member whose leader moved the party into a game lobby follows it (not for a custom game's lobby).
static bool handleJoinSession()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    bool joined = false;
    unsigned int filters[8] = {};
    if (link->getLobbyFilters(filters, 8) != 0)
    {
        bool any_filter = false;
        for (int i = 0; i < 8; i++)
        {
            any_filter = any_filter || filters[i] != 0;
        }
        // Magic number: the third filter's marker of a custom game (Custom_game_filter_marker of multi_server_browser.cpp).
        const bool custom_game = filters[2] == 0x1234FFFF;
        if (any_filter && link->isPartyEnabled() && !link->isPartyOwner() && link->getCurrentState() == kaiko::GameLinkState_InLobby && link->getLobbyId().is_valid() && !custom_game)
        {
            g_joinWithoutReservation = true;
            Show_connecting_screen = false;
            if (Loading_screen_ref_cnt > 0)
            {
                ui_remove_loading_screen();
            }
            Party_lobby_join_timeout.invalidate();
            party_lobby_ui_push(GS_MULTIPLAYER_GAME_LOBBY);
            party_lobby_set_party_state(MPS_NONE);
            joined = true;
        }
    }
    g_gameLinkMutex.unlock();
    return joined;
}

// 0x0088CDC0 (:1672)
static bool handleBecamePartyOwner()
{
    if (!multi_game_party_in_party() || !multi_game_party_am_i_leader(false))
    {
        return false;
    }
    if (Lobby_session->get_state() != MSS_NONE && Lobby_session->get_state() != MSS_READY)
    {
        return false;
    }
    party_lobby_set_party_state(MPS_HOST_CREATING_LOBBY);
    return true;
}

// 0x00914E40 (:1690)
void ui_document_mp_party_lobby_process()
{
    if (Bink_info_main.handle != 0)
    {
        mm_process_bink_video();
    }
    multi_player_menu_system_process();
    if (multi_game_party_left_party_by_choice())
    {
        party_lobby_ui_pop(true);
        return;
    }
    ui_add_for_processing(Party_lobby_menu_info.doc_handle);
    if (Party_lobby_loaded && Party_lobby_menu_info.state != MMS_PL_EXITING)
    {
        multi_player_menu_system_shell_process_data_items();
        party_lobby_process_data_group_menu_options();
        party_lobby_process_loaded();
    }
    switch (Party_lobby_menu_info.state)
    {
    case MMS_PL_UNLOADING:
        multi_game_party_disable_party();
        break;
    case MMS_PL_UNLOADED:
        party_lobby_set_state(MMS_PL_LOADING);
        break;
    case MMS_PL_LOADING:
        multi_game_party_enable_party();
        party_lobby_process_loading();
        break;
    case MMS_PL_EXITING:
        party_lobby_process_exit();
        break;
    default:
        break;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    const mp_party_states party_state = Party_lobby_menu_info.party_state;
    // Magic numbers: the game link's errors past 8 end the party, for a client waiting on the server only those past 0x18.
    if (link != nullptr && party_state != MPS_ERROR)
    {
        const int error = (int)link->getError(nullptr);
        if ((error > 8 && party_state != MPS_CLIENT_WAITING_FOR_SERVER) || error > 0x18)
        {
            link->setPartyEnabled(false);
            game_boot_to_main_menu(BC_REMOTE_GAME_SESSION);
            party_lobby_set_party_state(MPS_ERROR);
        }
    }
    bool run_party_states = true;
    // The host's states need the party's ownership.
    if (party_state != MPS_HOST_CREATING_LOBBY && party_state != MPS_HOST_LOBBY_READY && party_state != MPS_HOST_STARTING_GAME && party_state != MPS_HOST_WAITING_ON_CLIENTS_TO_JOIN && party_state != MPS_HOST_WAITING_ON_PARTY)
    {
        run_party_states = Party_lobby_menu_info.party_pending_state != MPS_ERROR;
    }
    else if (link->isPartyOwner())
    {
        run_party_states = Party_lobby_menu_info.party_pending_state != MPS_ERROR;
    }
    else
    {
        party_lobby_set_party_state(MPS_NONE);
    }
    if (run_party_states)
    {
        switch (party_state)
        {
        case MPS_NONE:
            if (multi_game_party_in_party())
            {
                if (!multi_game_party_am_i_leader(false))
                {
                    party_lobby_set_party_state(MPS_CLIENT_WAITING_FOR_LOBBY);
                }
                else if (Lobby_session->get_state() == MSS_NONE)
                {
                    party_lobby_set_party_state(MPS_HOST_CREATING_LOBBY);
                }
                else if (Lobby_session->get_state() == MSS_READY)
                {
                    if (Lobby_session->am_i_host())
                    {
                        party_lobby_set_party_state(MPS_HOST_CREATING_LOBBY);
                    }
                    else
                    {
                        Lobby_session->session_delete();
                    }
                }
            }
            else if (Live_server_browser.isInCustomMatch() || g_joinedGameLobbyViaInvitation)
            {
                Show_connecting_screen = false;
                Party_lobby_type = MPPLT_CUSTOM_GAME;
                party_lobby_set_party_state(MPS_CLIENT_WAITING_FOR_LOBBY);
                handleJoinSession();
            }
            else
            {
                link->setPartyEnabled(true);
                handleJoinSession();
            }
            break;
        case MPS_HOST_CREATING_LOBBY:
            if (Lobby_session->get_state() == MSS_READY)
            {
                party_lobby_set_party_state(MPS_HOST_LOBBY_READY);
            }
            else if (Lobby_session->get_state() == MSS_NONE)
            {
                party_lobby_set_party_state(MPS_NONE);
            }
            break;
        case MPS_HOST_LOBBY_READY:
            if (Lobby_session->get_state() == MSS_NONE)
            {
                party_lobby_set_party_state(MPS_NONE);
            }
            else if (Lobby_session->get_state() == MSS_READY)
            {
                if (multi_game_party_is_locked_down())
                {
                    multi_game_party_unlock();
                }
                else if (Auto_retry_game_launch)
                {
                    Auto_retry_game_launch = false;
                    party_lobby_set_party_state(MPS_HOST_WAITING_ON_PARTY);
                }
            }
            break;
        case MPS_HOST_WAITING_ON_PARTY:
            if (!multi_game_party_is_locked_down())
            {
                multi_game_party_lockdown();
            }
            else if (Party_lobby_type == MPPLT_CUSTOM_GAME)
            {
                party_lobby_set_party_state(MPS_HOST_WAITING_ON_CLIENTS_TO_JOIN);
            }
            else if (Party_lobby_type == MPPLT_MATCHMAKING)
            {
                party_lobby_ui_push(GS_MULTIPLAYER_CHANGE_MATCHMAKING);
            }
            else
            {
                multi_game_party_unlock();
                party_lobby_set_party_state(MPS_HOST_LOBBY_READY);
            }
            break;
        case MPS_HOST_WAITING_ON_CLIENTS_TO_JOIN:
            if (multi_game_party_all_players_joined_lobby())
            {
                party_lobby_set_party_state(MPS_HOST_STARTING_GAME);
            }
            break;
        case MPS_HOST_STARTING_GAME:
            if (Lobby_session->get_state() == MSS_NONE)
            {
                Auto_retry_game_launch = true;
                party_lobby_set_party_state(MPS_NONE);
            }
            break;
        case MPS_CLIENT_WAITING_FOR_LOBBY:
            if (!handleJoinSession() && !handleBecamePartyOwner())
            {
                if (multi_game_lobby_pending_join() || Lobby_session->am_i_host() || Lobby_session->get_state() != MSS_READY)
                {
                    if (Party_lobby_join_timeout.elapsed())
                    {
                        game_boot_to_main_menu(Lobby_session->get_last_join_status() == MSJS_JOIN_DENY_SESSION_LOCKED ? BC_GAME_ALREADY_STARTED : BC_LOBBY_MIGRATION_FAIL);
                        party_lobby_set_party_state(MPS_ERROR);
                    }
                }
                else
                {
                    party_lobby_set_party_state(MPS_CLIENT_WAITING_FOR_SERVER);
                }
            }
            break;
        case MPS_CLIENT_WAITING_FOR_SERVER:
            if (!handleJoinSession())
            {
                handleBecamePartyOwner();
            }
            break;
        case MPS_CLIENT_MOVING_TO_MATCHMAKING:
            if (!handleJoinSession() && Lobby_session->get_state() == MSS_NONE && link->getCurrentState() == kaiko::GameLinkState_Online)
            {
                party_lobby_ui_push(GS_MULTIPLAYER_SEARCH_MATCHMAKING);
                party_lobby_set_party_state(MPS_NONE);
            }
            break;
        default:
            break;
        }
    }
    ui_process(Party_lobby_menu_info.control_id, false);
    g_gameLinkMutex.unlock();
}

// 0x008F2CF0 (:1953)
void ui_document_mp_party_lobby_handshake(rl_renderer*)
{
    Party_lobby_menu_info.prev_state = Party_lobby_menu_info.state;
    if (Party_lobby_menu_info.pending_state != Party_lobby_menu_info.state)
    {
        party_lobby_do_state_change();
    }
    if (Party_lobby_menu_info.party_pending_state != Party_lobby_menu_info.party_state)
    {
        party_lobby_do_party_state_change();
    }
    multi_player_menu_system_level_image_handshake();
}

// 0x008FBE00 (:1967)
void ui_document_mp_party_lobby_render(rl_renderer*)
{
    if (Party_lobby_menu_info.doc_handle == -1 || Party_lobby_menu_info.control_id == 0)
    {
        return;
    }
    if (Party_lobby_menu_info.state > MMS_PL_LOADING && Party_lobby_menu_info.prev_state > MMS_PL_LOADING)
    {
        ui_add_for_rendering(Party_lobby_menu_info.doc_handle);
    }
    ui_render(Party_lobby_menu_info.control_id, false);
}

// 0x00915280 (:1994)
void ui_document_mp_party_lobby_init()
{
    state_info info;
    info.enter = ui_document_mp_party_lobby_enter;
    info.exit = ui_document_mp_party_lobby_exit;
    info.process = ui_document_mp_party_lobby_process;
    info.handshake = ui_document_mp_party_lobby_handshake;
    info.render = ui_document_mp_party_lobby_render;
    info.transparent = false;
    info.pause_beneath = false;
    gameseq_register_state(GS_MULTIPLAYER_PARTY_LOBBY, info);
}

// Port helper: a big-endian int from a packet, inlined in rfg.exe.
static unsigned int party_lobby_read_packet_uint(const unsigned char* packet_data)
{
    return (unsigned int)packet_data[0] << 24 | (unsigned int)packet_data[1] << 16 | (unsigned int)packet_data[2] << 8 | packet_data[3];
}

// 0x008AE020 (:2010)
void ui_document_mp_party_lobby_go_matchmake(unsigned char* packet_data, const sender& from)
{
    const multi_game_playlist* playlist = multi_game_playlist_get_by_id(party_lobby_read_packet_uint(packet_data));
    if (playlist == nullptr)
    {
        multi_game_party_leave_party(false);
        return;
    }
    game_query query;
    query.game_query_base::copy(*multi_game_preferences_get_ideal_query());
    query.m_game_type = playlist->game_type;
    query.m_query_type = playlist->m_query_type;
    query.m_playlist_id = playlist->playlist_id.get_crc();
    query.m_game_mode = playlist->mode_type;
    multi_game_preferences_set_ideal_query(&query);
    multi_game_preferences_set_current_query(&query);
    multi_core_platform_set_user_playlist(query.m_playlist_id);
    if (Party_lobby_menu_info.party_state != MPS_CLIENT_MOVING_TO_MATCHMAKING)
    {
        party_lobby_set_party_state(MPS_CLIENT_MOVING_TO_MATCHMAKING);
    }
}

// 0x008AE100 (:2050)
void ui_document_mp_party_lobby_type_request(unsigned char* packet_data, const sender& from)
{
    party_lobby_send_type_update();
}

// 0x0088CE10 (:2055)
void ui_document_mp_party_lobby_type(unsigned char* packet_data, const sender& from)
{
    Party_lobby_type = (mp_party_lobby_types)party_lobby_read_packet_uint(packet_data);
}

// 0x008AE110 (:2061)
void ui_document_mp_party_lobby_return_to_lobby(unsigned char* packet_data, const sender& from)
{
    const game_state state = gameseq_get_state();
    if (state == GS_MULTIPLAYER_PARTY_LOBBY)
    {
        return;
    }
    if (state == GS_MULTIPLAYER_XP_TALLY || state == GS_MULTIPLAYER_RESULTS)
    {
        ui_document_mp_results_ui_pop(true);
    }
    else
    {
        Pending_pop = true;
    }
}

// 0x0088CE40 (:2072)
mp_party_lobby_types party_lobby_get_next_type(mp_party_lobby_types current_type)
{
    return (mp_party_lobby_types)((current_type + 1) % NUM_MP_SELECTABLE_PARTY_LOBBY_TYPES);
}

// 0x0088CE60 (:2077)
mp_party_lobby_types party_lobby_get_prev_type(mp_party_lobby_types current_type)
{
    // Original bug: forward as well, which makes no difference with the two selectable types.
    return (mp_party_lobby_types)((current_type + 1) % NUM_MP_SELECTABLE_PARTY_LOBBY_TYPES);
}

// 0x0088CE80 (:2082)
void party_lobby_send_type_update()
{
    unsigned char packet[1200];
    unsigned int size = party_lobby_start_empty_packet(packet, PT_PRIVATE_LOBBY_TYPE);
    const unsigned int type = (unsigned int)Party_lobby_type;
    packet[size] = (unsigned char)(type >> 24);
    packet[size + 1] = (unsigned char)(type >> 16);
    packet[size + 2] = (unsigned char)(type >> 8);
    packet[size + 3] = (unsigned char)type;
    size += 4;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_to_all_reliable(Lobby_session, packet, size);
}

// 0x0088CF50 (:2091)
void party_lobby_reset_party_state()
{
    Party_lobby_menu_info.party_state = MPS_NONE;
    Party_lobby_menu_info.party_pending_state = MPS_NONE;
}

// 0x0088CF60 (:2097)
mp_party_states party_lobby_get_party_state()
{
    return Party_lobby_menu_info.party_state;
}

// 0x0088CF70 (:2102)
mp_party_states party_lobby_get_pending_party_state()
{
    return Party_lobby_menu_info.party_pending_state;
}
