// Original: volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.cpp
//
// The multiplayer menu system (UI.md UI5): the registration of the multiplayer menu states, the mp_shell document's
// data items (the per-menu infos and their process callbacks), the game info item every shell menu shows, the LAN
// server search (every 2 s the results of the last search, then a new one), the lobby update, the shared message
// boxes (player options, leave confirm, migration, lost connection, no opponents, the DLC and launch problems), the
// menus' hint lines, the state stack restore after a match (the menus pushed back under the results), the level
// image streaming and the menus' key, mouse and arrow input.
// The message boxes inline ui_add_msgbox in rfg.exe; they call it here (the same body).

#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"

#include "keen/base/crc32.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/sw_api/sw_api.h"
#include "kaiko/gamelink/gamelink.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/objects/vint_object_base.h"
#include "volition/interface_lib/code/vint_data_item.h"
#include "volition/interface_lib/code/vint_document.h"
#include "volition/interface_lib/code/vint_string_processing.h"
#include "volition/interface_lib/code/vint_variant.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_change_matchmaking.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_character_select.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_lobby.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_options.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_live_find_servers.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_matchmaking.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_options.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_scoreboard.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink_find_servers.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_tutorials.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_xp_tally.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_menu.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
// mp_message_box_type is in multi_player_menu_system.h since UI5 (the menu documents use it).

// size 0x4
struct mp_shell_data_item_active_info
{
    int handle; // 0x00
};
static_assert(sizeof(mp_shell_data_item_active_info) == 0x4, "mp_shell_data_item_active_info");

// size 0x8
struct mp_shell_data_item_setup_info
{
    const char* name; // 0x00
    bool can_hide; // 0x04
};
static_assert(sizeof(mp_shell_data_item_setup_info) == 0x8, "mp_shell_data_item_setup_info");

// size 0x10
struct party_up_offer
{

    multi_player_uid uid; // 0x00
    timestamp offered_time; // 0x0C
};
static_assert(sizeof(party_up_offer) == 0x10, "party_up_offer");

enum playcard_options
{
    PCO_KICK_FROM_PARTY = 0,
    PCO_SHOW_GAMERCARD = 1,
    PCO_SUBMIT_PLAYER_REVIEW = 2,
    PCO_ADD_FRIEND = 3,
    PCO_MUTE_PLAYER = 4,
    PCO_UNMUTE_PLAYER = 5,
    NUM_PLAYCARD_OPTIONS = 6,
};

// ---- file-static globals
// bool s_localization_is_loading (0x01663023) is used here too; placed in localization_shared.h.
// Shared with the menu documents (multi_player_menu_system.h, UI5).
const char* Mp_shell_doc_name = {"rfg_ui_mp_shell"}; // 0x0166320C
static const char* Kick_from_party_string = {"MENU_KICK_FROM_PARTY"}; // 0x01663220
static const char* Gamer_card_string = {"MENU_VIEW_PROFILE"}; // 0x01663224
static const char* Confirm_exit_title_string = {"MENU_MBOX_CONFIRM_EXIT"}; // 0x0166322C
static const char* Confirm_exit_desc_string = {"MENU_CONFIRM_EXIT"}; // 0x01663230
static const char* Reconnecting_string = {"MENU_MBOX_RECONN"}; // 0x01663234
static const char* Opponent_left_string = {"MENU_MBOX_OPPONENT_LEFT"}; // 0x01663238
static const char* Party_too_big_string = {"MENU_MBOX_PARTY_TO_BIG"}; // 0x0166323C
static const char* Cannot_join_session_string = {"MENU_MBOX_CANNOT_JOIN_SESSION"}; // 0x01663244
static const char* Kick_warning_string = {"MENU_KICK_PARTY_WARNING"}; // 0x01663248
static const char* Kick_warning_title_string = {"MENU_KICK_FROM_PARTY"}; // 0x0166324C
static const char* Mute_player_string = {"MENU_MUTE_PLAYER"}; // 0x01663254
static const char* Unmute_player_string = {"MENU_UNMUTE_PLAYER"}; // 0x01663258
int Mp_shell_doc_handle = -1; // 0x0166325C
// The shell's data items (rfg.exe's .data): the item names, and whether the item exists for every menu (can_hide: set
// to false when the menu's infos do not list it).
static mp_shell_data_item_setup_info Mp_shell_data_item_setups[16] = {
    {"MpShellHintInfo", false},        // MPSDIT_HINT_INFO
    {"MpShellTitles", false},          // MPSDIT_TITLES
    {"MpShellInfoText", true},         // MPSDIT_INFO_TEXT
    {"MpShellHighlightInfo", false},   // MPSDIT_SELECTION_INFO
    {"MpShellTypeEnum", false},        // MPSDIT_MENU_TYPE
    {"MpShellMatchmingHint", true},    // MPSDIT_MATCHING_HINT (sic)
    {"MpShellGameInfo", true},         // MPSDIT_GAME_INFO
    {"MpShellPartyInfo", true},        // MPSDIT_PARTY_INFO
    {"MpShellPlaylistInfo", true},     // MPSDIT_PLAYLIST_INFO
    {"MpShellPressStart", true},       // MPSDIT_PRESS_START
    {"MpShellVetoInfo", true},         // MPSDIT_VETO_INFO
    {"MpShellUpdateTimer", true},      // MPSDIT_TIMER
    {"MpShellUpdateTips", true},       // MPSDIT_TIPS
    {"MpShellUpdateCharacter", true},  // MPSDIT_CHARACTER_SELECT
    {"MpShellSetVideo", true},         // MPSDIT_VIDEO
    {"MpShellWCCharacter", true},      // MPSDIT_WC_CHARACTER
}; // 0x01663260
// int Mp_msgbox_handle (0x016632E0) is used here too; placed in ui_document_hud_multiplayer.cpp.
// int Loading_screen_handle (0x01664B44) is used here too; placed in loading.h.
// int Current_input_lock_id (0x01664B4C) is used here too; placed in loading.cpp.
// int Current_input_lock_index (0x01664B50) is used here too; placed in loading.cpp.
// bool Input_enabled_uimen (0x01664FD0) is used here too; placed in loading.cpp.
// const char* cBlank_string (0x01664FF0) is used here too; placed in loading.cpp.
// int Utility_doc_handle (0x01664FF8) is used here too; placed in loading.cpp.
// int Msg_box_data_item_handle (0x01664FFC) is used here too; placed in loading.cpp.
// bool Player_input_was_enabled (0x0166500C) is used here too; placed in loading.cpp.
// bool Player_camera_input_was_enabled (0x0166500D) is used here too; placed in loading.cpp.
timestamp_realtime Server_search_ping_interval; // 0x01666274, initial data ffffffff
static timestamp_realtime Lobby_update_timestamp; // 0x01666278, initial data ffffffff
static farray<int,16> State_stack_doc_handles; // 0x01666280
// farray<input_request_data,16> Input_types (0x016673C8) is used here too; placed in loading.cpp.
multi_player_uid Playercard_player_uid; // 0x02BE9E14
// ui_button_info Button_info (0x02BF1E30) is used here too; placed in ui_message_box.cpp.
// int Audio_info[12] (0x02BF21D0) is used here too; placed in ui_menu.cpp.
static playcard_options Active_playcard_options[6]; // 0x02BF4328
static mp_shell_data_item_active_info Mp_shell_data_items[16]; // 0x02C00DC0
int mpps_hint_config[2]; // 0x02C00E00
// The LAN search results (GameLink::getSearchResults, 16 of 0x50 bytes) and their server infos.
static kaiko::GameLinkSearchResult s_lan_search_results[16]; // 0x02C02E20
bool s_lan_search_running; // 0x02C0357B
unsigned int s_lan_num_search_results; // 0x02C03598
mp_shell_data_item_info* Mp_shell_data_item_infos; // 0x02C0359C
int Num_mp_shell_data_item_infos; // 0x02C035A0
bool Search_search_pong_enabled; // 0x02C035A4
static bool Load_level_image_next_frame; // 0x02C035A5
mp_message_box_type Mp_msgbox_type; // 0x02C035A8
static int Num_active_playcard_options; // 0x02C035AC
vint_data_item* Hint_data_item; // 0x02C035B0
int Num_hints_added; // 0x02C035B4
static const char* Current_level_image_name; // 0x02C035B8
static const char* Old_level_image_name; // 0x02C035BC
multi_server_info s_lan_server_infos[16]; // 0x02C4FAE0
// int Loading_screen_ref_cnt (0x02C080A0) is used here too; placed in loading.h.
// bool Paused_game (0x02C081AE) is used here too; placed in loading.cpp.
// bool Locked_controls (0x02C081B4) is used here too; placed in loading.cpp.
static object_pool<multi_player_uid,16,1> Party_up_offers_current; // 0x02C082E0
static object_pool<party_up_offer,16,1> Party_up_offers_all_time; // 0x02C43B60
// msgbox_info_set Message_box_info_set (0x02CA0340) is used here too; placed in ui_message_box.cpp.

// The header constants of the row and hint objects (one copy per file of rfg.exe's interface unity; this file's).
const char* kHintText1 = {"hint_text_1"}; // 0x016685E0
const char* kHintText2 = {"hint_text_2"}; // 0x016685E4
const char* kHintText3 = {"hint_text_3"}; // 0x016685E8
const char* kHintText4 = {"hint_text_4"}; // 0x016685EC
static const char* kGroup_name = {"mp_shell_row_main"}; // 0x016685F0
static const char* kLeftArrow = {"row_highlight_left"}; // 0x016685F4
static const char* kRightArrow = {"row_highlight_right"}; // 0x016685F8

// A static of the out-of-scope matchmaking menu (ui_document_mp_matchmaking.cpp in rfg.exe's unity), here for leave_cb.

// The state callbacks of the out-of-scope menus whose headers do not declare them (file statics of rfg.exe's unity:
// ui_document_mp_matchmaking.cpp, ui_document_mp_party_lobby.cpp, ui_document_mp_party_options.cpp).
void ui_document_mp_matchmaking_enter(game_state prev_state); // 0x0090CAC0 (:1399)
void ui_document_mp_matchmaking_exit(game_state new_state); // 0x008D3270 (:1437)
void ui_document_mp_matchmaking_process(); // 0x00914CF0 (:1455)
void ui_document_mp_matchmaking_handshake(rl_renderer*); // 0x008F29B0 (:1495)
void ui_document_mp_matchmaking_render(rl_renderer*); // 0x008FBDB0 (:1503)
void ui_document_mp_party_lobby_enter(game_state prev_state); // 0x0090D1A0 (:1542)
void ui_document_mp_party_lobby_exit(game_state new_state); // 0x008E4130 (:1590)
void ui_document_mp_party_lobby_process(); // 0x00914E40 (:1690)
void ui_document_mp_party_lobby_handshake(rl_renderer*); // 0x008F2CF0 (:1953)
void ui_document_mp_party_lobby_render(rl_renderer*); // 0x008FBE00 (:1967)
void ui_document_mp_party_options_enter(game_state prev_state); // 0x0090D2D0 (:728)
void ui_document_mp_party_options_exit(game_state new_state); // 0x008E4340 (:759)
void ui_document_mp_party_options_process(); // 0x009152D0 (:773)
void ui_document_mp_party_options_handshake(rl_renderer*); // 0x008F2E40 (:823)
void ui_document_mp_party_options_render(rl_renderer*); // 0x008FC0E0 (:831)


// ---- compiler-generated copies emitted in this file (rfg.exe: the LAN search's server infos)
// 0x0088A210
multi_session_info_base& multi_session_info_base::operator=(const multi_session_info_base& __that) = default;
// 0x008AC600
multi_session_info& multi_session_info::operator=(const multi_session_info& __that) = default;

// ---- internal functions

// True while a message box is open (the box pool's capacity minus its free count, inlined in rfg.exe).
static bool any_msgbox_open()
{
    const object_pool_server<msgbox_info>& server = Message_box_info_set.msgbox_info_pool;
    return server.get_capacity() != (unsigned int)server.get_free();
}

// 0x0088A110 (:269)
static void cleanup_msgbox_handles()
{
    Mp_msgbox_handle = -1;
    Mp_msgbox_type = MP_MBT_NONE;
}

// 0x0088A130 (:275)
// Only a party lobby's players, and not once the game is counting down, vetoed or running.
static bool allow_kick()
{
    if (Lobby_session != nullptr && Lobby_session->get_info() != nullptr && Lobby_session->get_info()->party_lobby)
    {
        const multi_game_lobby_state_machine state = multi_game_lobby_get_state();
        if (state != MGLSM_GAME_COUNTDOWN && state != MGLSM_VETOED && state != MGLSM_IN_GAME)
        {
            return true;
        }
    }
    return false;
}

// 0x0090A4C0 (:280)
static bool playcard_cb(int mbox_handle, int selection_index, msgbox_choice choice)
{
    if (Mp_msgbox_handle != mbox_handle)
    {
        return false;
    }
    Mp_msgbox_handle = -1;
    Mp_msgbox_type = MP_MBT_NONE;
    if (choice != MBC_FORCED_EXIT && choice != MBC_ABORT && selection_index >= 0)
    {
        switch (Active_playcard_options[selection_index])
        {
        case PCO_KICK_FROM_PARTY:
            if (allow_kick())
            {
                multi_player_menu_system_pull_up_kick_from_party_confirm_message_box(Playercard_player_uid);
            }
            break;
        case PCO_SHOW_GAMERCARD:
            multi_core_platform_show_gamercard(Playercard_player_uid);
            break;
        case PCO_MUTE_PLAYER:
            multi_voice_platform_mute_player(Playercard_player_uid);
            multi_voice_set_original_player_mute_state(Playercard_player_uid, true);
            break;
        case PCO_UNMUTE_PLAYER:
            multi_voice_platform_unmute_player(Playercard_player_uid);
            multi_voice_set_original_player_mute_state(Playercard_player_uid, false);
            break;
        default:
            break;
        }
    }
    return true;
}

// 0x008AC4E0 (:331)
// Accepted: leave the matchmaking search, the game lobby or the party (each its own way).
static bool leave_cb(int message_box_handle, int selection_index, msgbox_choice choice)
{
    if (Mp_msgbox_handle != message_box_handle)
    {
        return false;
    }
    if (choice == MBC_OK)
    {
        switch (gameseq_get_state())
        {
        case GS_MULTIPLAYER_SEARCH_MATCHMAKING:
            if (multi_game_matchmaking_can_cancel())
            {
                if (multi_game_party_in_party() && !multi_game_party_am_i_leader(false))
                {
                    multi_game_party_leave_party(true);
                }
                multi_game_matchmaking_cancel();
            }
            party_lobby_reset_party_state();
            break;
        case GS_MULTIPLAYER_GAME_LOBBY:
            if (multi_game_party_in_party() && !multi_game_party_am_i_leader(false))
            {
                multi_game_party_leave_party(true);
            }
            multi_game_lobby_leave(true);
            party_lobby_reset_party_state();
            break;
        case GS_MULTIPLAYER_PARTY_LOBBY:
        case GS_MULTIPLAYER_XP_TALLY:
        case GS_MULTIPLAYER_RESULTS:
            multi_game_party_leave_party(true);
            multi_game_lobby_leave(true);
            break;
        default:
            break;
        }
    }
    Mp_msgbox_type = MP_MBT_NONE;
    Mp_msgbox_handle = -1;
    return true;
}

// 0x0088A180 (:373)
static bool kick_from_party_cb(int message_box_handle, int selection_index, msgbox_choice choice)
{
    if (Mp_msgbox_handle != message_box_handle)
    {
        return false;
    }
    if (choice == MBC_OK && Playercard_player_uid.is_valid() && allow_kick())
    {
        multi_game_party_kick(Playercard_player_uid);
        Playercard_player_uid.invalidate();
    }
    Mp_msgbox_handle = -1;
    Mp_msgbox_type = MP_MBT_NONE;
    return true;
}

// 0x0088A1E0 (:389)
static bool generic_cb(int message_box_handle, int selection_index, msgbox_choice choice)
{
    if (Mp_msgbox_handle != message_box_handle)
    {
        return false;
    }
    Mp_msgbox_handle = -1;
    Mp_msgbox_type = MP_MBT_NONE;
    return true;
}

// 0x008BFA40 (:401)
// While a search is on (Server_search_ping_interval valid): every 2 s the results of the last search become the
// server infos (multi_fs_server_info lists them), then a new search starts.
static void mpms_server_search_process()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    if (Server_search_ping_interval.valid() && Server_search_ping_interval.elapsed())
    {
        // Magic number: 2000 ms between searches.
        Server_search_ping_interval.set(2000);
        s_lan_num_search_results = link->getSearchResults(s_lan_search_results, 16, &s_lan_search_running);
        for (unsigned int i = 0; i < s_lan_num_search_results; i++)
        {
            const kaiko::GameLinkSearchResult& result = s_lan_search_results[i];
            multi_server_info& server = s_lan_server_infos[i];
            // Original: the whole info is cleared, the session info's vtable pointer too (its operator= keeps it null).
            memset(&server, 0, sizeof(server));
            keen::convertUTF8ToUTF16((unsigned short*)server.name, 0x200, result.lobbyName);
            // Magic number: a listed server is fresh for 3 s.
            server.stamp.set(3000);
            multi_session_info info;
            info.max_slots = result.maxNumPlayers;
            info.m_lobbyId.id = result.lobby.id;
            info.systemlink_or_lan = true;
            info.type = ST_GAME;
            // The host's uid: the lobby id's first four bytes (its address).
            unit4::RemoteUserId host;
            memset(host.providerData, 0, sizeof(host.providerData));
            memcpy(host.providerData, &info.m_lobbyId.id.asUint64, 4);
            host.guestIndex = 0;
            host.isValid = true;
            info.host_player_uid.set(host, true);
            server.session_info = info;
        }
        if (!link->startSearchLobby(16, nullptr, 0))
        {
            keen::debug::nativePrintArgs("[syslink] startSearchLobby failed\n");
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x0088A290 (:458)
// The host's lobby update timer (500 ms) is kept but drives nothing.
static void mpms_lobby_update_process()
{
    multi_game_lobby_process();
    if (Lobby_session != nullptr && Lobby_session->get_state() != MSS_NONE && Lobby_session->am_i_host())
    {
        if (Lobby_update_timestamp.valid() && !Lobby_update_timestamp.elapsed())
        {
            return;
        }
        // Magic number: 500 ms.
        Lobby_update_timestamp.set(500);
    }
}

// 0x008F1B00 (:472)
// LTCG: type in ESI. A box of a higher or equal priority (type) replaces the open one.
static bool override_current_msgbox_with(mp_message_box_type type)
{
    if ((int)Mp_msgbox_type > (int)type)
    {
        return false;
    }
    if (Mp_msgbox_type != MP_MBT_NONE)
    {
        if (Mp_msgbox_handle != -1)
        {
            ui_hide_msgbox(Mp_msgbox_handle, false);
            Playercard_player_uid.invalidate();
        }
        Mp_msgbox_handle = -1;
    }
    Mp_msgbox_type = type;
    return true;
}

// 0x008CF700 (:490)
static void mpms_msgbox_update_process()
{
    if (Mp_msgbox_handle != -1 && !ui_is_msgbox_active(Mp_msgbox_handle))
    {
        Mp_msgbox_handle = -1;
        Mp_msgbox_type = MP_MBT_NONE;
    }
}

// 0x008CF730 (:499)
// LTCG: item_type in EAX. The data item of the type, added to the document.
static vint_data_item* setup_shell_data_item(mp_shell_data_item_active_info* item, mp_shell_data_item_types item_type, int doc_handle)
{
    vint_data_item* vdi = vint_data_item::create_new(Mp_shell_data_item_setups[item_type].name);
    if (vdi == nullptr)
    {
        return nullptr;
    }
    ui_add_data_item(doc_handle, vdi->get_handle());
    item->handle = vdi->get_handle();
    return vdi;
}

// ---- external interface

// 0x009193B0 (:798)
void multi_player_menu_system_init()
{
    Server_search_ping_interval.invalidate();
    Search_search_pong_enabled = false;
    multi_fs_reset_servers();
    Lobby_update_timestamp.invalidate();
    struct registration
    {
        game_state state;
        void (*enter)(game_state);
        void (*exit)(game_state);
        void (*process)();
        void (*handshake)(rl_renderer*);
        void (*render)(rl_renderer*);
    };
    const registration states[] = {
        {GS_MULTIPLAYER_CHANGE_MATCHMAKING, ui_document_mp_change_matchmaking_enter, ui_document_mp_change_matchmaking_exit, ui_document_mp_change_matchmaking_process, ui_document_mp_change_matchmaking_handshake, ui_document_mp_change_matchmaking_render},
        {GS_MULTIPLAYER_GAME_LOBBY, ui_document_mp_game_lobby_enter, ui_document_mp_game_lobby_exit, ui_document_mp_game_game_lobby_process, ui_document_mp_game_lobby_handshake, ui_document_mp_game_lobby_render},
        {GS_MULTIPLAYER_CHANGE_GAME_OPTIONS, ui_document_mp_game_options_enter, ui_document_mp_game_options_exit, ui_document_mp_game_options_process, ui_document_mp_game_options_handshake, ui_document_mp_game_options_render},
        {GS_MULTIPLAYER_LIVE, ui_document_mp_live_enter, ui_document_mp_live_exit, ui_document_mp_live_process, ui_document_mp_live_handshake, ui_document_mp_live_render},
        {GS_MULTIPLAYER_SEARCH_MATCHMAKING, ui_document_mp_matchmaking_enter, ui_document_mp_matchmaking_exit, ui_document_mp_matchmaking_process, ui_document_mp_matchmaking_handshake, ui_document_mp_matchmaking_render},
        {GS_MULTIPLAYER_PARTY_LOBBY, ui_document_mp_party_lobby_enter, ui_document_mp_party_lobby_exit, ui_document_mp_party_lobby_process, ui_document_mp_party_lobby_handshake, ui_document_mp_party_lobby_render},
        {GS_MULTIPLAYER_PARTY_OPTIONS, ui_document_mp_party_options_enter, ui_document_mp_party_options_exit, ui_document_mp_party_options_process, ui_document_mp_party_options_handshake, ui_document_mp_party_options_render},
        {GS_MULTIPLAYER_PLAYER_SELECT, ui_document_mp_player_select_enter, ui_document_mp_player_select_exit, ui_document_mp_player_select_process, ui_document_mp_player_select_handshake, ui_document_mp_player_select_render},
        {GS_MULTIPLAYER_RESULTS, ui_document_mp_results_enter, ui_document_mp_results_exit, ui_document_mp_results_process, ui_document_mp_results_handshake, ui_document_mp_results_render},
        {GS_MULTIPLAYER_SCOREBOARD, ui_document_mp_scoreboard_enter, ui_document_mp_scoreboard_exit, ui_document_mp_scoreboard_process, ui_document_mp_scoreboard_handshake, ui_document_mp_scoreboard_render},
        {GS_MULTIPLAYER_STATS, ui_document_mp_stats_enter, ui_document_mp_stats_exit, ui_document_mp_stats_process, ui_document_mp_stats_handshake, ui_document_mp_stats_render},
        {GS_MULTIPLAYER_SYSLINK, ui_document_mp_syslink_enter, ui_document_mp_syslink_exit, ui_document_mp_syslink_process, ui_document_mp_syslink_handshake, ui_document_mp_syslink_render},
        {GS_MULTIPLAYER_SYSLINK_FIND_SERVERS, ui_document_mp_syslink_fs_enter, ui_document_mp_syslink_fs_exit, ui_document_mp_syslink_fs_process, ui_document_mp_syslink_fs_handshake, ui_document_mp_syslink_fs_render},
        {GS_MULTIPLAYER_TUTORIALS, ui_document_mp_tutorials_enter, ui_document_mp_tutorials_exit, ui_document_mp_tutorials_process, ui_document_mp_tutorials_handshake, ui_document_mp_tutorials_render},
        {GS_MULTIPLAYER_XP_TALLY, ui_document_mp_xp_tally_enter, ui_document_mp_xp_tally_exit, ui_document_mp_xp_tally_process, ui_document_mp_xp_tally_handshake, ui_document_mp_xp_tally_render},
        {GS_MULTIPLAYER_LIVE_FIND_SERVERS, ui_document_mp_live_fs_enter, ui_document_mp_live_fs_exit, ui_document_mp_live_fs_process, ui_document_mp_live_fs_handshake, ui_document_mp_live_fs_render},
    };
    for (const registration& reg : states)
    {
        state_info info;
        info.enter = reg.enter;
        info.exit = reg.exit;
        info.process = reg.process;
        info.handshake = reg.handshake;
        info.render = reg.render;
        info.transparent = false;
        info.pause_beneath = false;
        gameseq_register_state(reg.state, info);
        if (reg.state == GS_MULTIPLAYER_LIVE)
        {
            // rfg.exe clears it between the live and matchmaking registrations.
            Pushed_to_lobby = false;
        }
    }
}

// 0x008DEC40 (:827)
void multi_player_menu_multi_init(mempool_base& pool)
{
    ui_document_mp_tutorials_multi_init(pool);
}

// 0x008AC630 (:836)
void multi_player_menu_multi_shutdown()
{
    ui_document_mp_tutorials_multi_shutdown();
    Search_search_pong_enabled = false;
}

// 0x008CF770 (:846)
// The system link host answers searches (pong) except while the results are up.
void multi_player_menu_system_process()
{
    mpms_server_search_process();
    mpms_lobby_update_process();
    // mpms_msgbox_update_process, inlined.
    if (Mp_msgbox_handle != -1 && !ui_is_msgbox_active(Mp_msgbox_handle))
    {
        Mp_msgbox_handle = -1;
        Mp_msgbox_type = MP_MBT_NONE;
    }
    if (Lobby_session->get_info()->systemlink_or_lan && Lobby_session->am_i_host() && !gameseq_state_is_active(GS_MULTIPLAYER_RESULTS))
    {
        Search_search_pong_enabled = true;
        return;
    }
    Search_search_pong_enabled = false;
}

// 0x0090A590 (:901)
void multi_player_menu_system_shell_load_doc(int& doc_handle, bool show_loading_bar)
{
    // Magic number: a 0.5 s fade in.
    doc_handle = ui_load_document(Mp_shell_doc_name, DFIT_NONE, 0.5f, DLT_NORMAL, show_loading_bar, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (doc_handle != -1)
    {
        Mp_shell_doc_handle = doc_handle;
    }
}

// 0x008F1B50 (:913)
void multi_player_menu_system_shell_unload_doc(int& doc_handle)
{
    if (doc_handle != -1)
    {
        // Magic number: a 0.5 s fade out.
        ui_unload_document(doc_handle, DFOT_NONE, 0.5f);
        doc_handle = -1;
        Mp_shell_doc_handle = -1;
    }
}

// 0x008CF7F0 (:923)
// Every item that may be hidden exists for each menu (hidden unless the menu lists it); the others only when listed.
void multi_player_menu_system_shell_load_data_items(mp_shell_data_item_info* infos, int num_infos)
{
    if (infos == nullptr || Mp_shell_doc_handle == -1)
    {
        return;
    }
    for (int i = 0; i < NUM_MP_SHELL_DATA_ITEM_TYPES; i++)
    {
        Mp_shell_data_items[i].handle = -1;
        if (Mp_shell_data_item_setups[i].can_hide)
        {
            setup_shell_data_item(&Mp_shell_data_items[i], (mp_shell_data_item_types)i, Mp_shell_doc_handle);
        }
    }
    for (int i = 0; i < num_infos; i++)
    {
        const mp_shell_data_item_types type = infos[i].item_type;
        if ((int)type >= 0 && (int)type < NUM_MP_SHELL_DATA_ITEM_TYPES && !Mp_shell_data_item_setups[type].can_hide)
        {
            setup_shell_data_item(&Mp_shell_data_items[type], type, Mp_shell_doc_handle);
        }
    }
    Num_mp_shell_data_item_infos = num_infos;
    Mp_shell_data_item_infos = infos;
}

// 0x008CF8A0 (:962)
void multi_player_menu_system_shell_unload_data_items()
{
    if (Mp_shell_doc_handle == -1 || Mp_shell_data_item_infos == nullptr)
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(Mp_shell_doc_handle);
    if (info != nullptr)
    {
        info->release_data_groups();
        info->release_data_items();
        info->release_data_events();
    }
    Mp_shell_data_item_infos = nullptr;
    Num_mp_shell_data_item_infos = 0;
}

// 0x0088A2F0 (:974)
// A listed item runs its callback (none: left as it is); an unlisted one is hidden (element 0 false).
void multi_player_menu_system_shell_process_data_items()
{
    if (Mp_shell_doc_handle == -1 || Mp_shell_data_item_infos == nullptr)
    {
        return;
    }
    for (int type = 0; type < NUM_MP_SHELL_DATA_ITEM_TYPES; type++)
    {
        if (Mp_shell_data_items[type].handle == -1)
        {
            continue;
        }
        mp_shell_data_item_info* found = nullptr;
        for (int i = 0; i < Num_mp_shell_data_item_infos; i++)
        {
            if (Mp_shell_data_item_infos[i].item_type == type)
            {
                found = &Mp_shell_data_item_infos[i];
                break;
            }
        }
        vint_data_item* vdi = vint_data_item::find((unsigned int)Mp_shell_data_items[type].handle);
        if (vdi == nullptr)
        {
            continue;
        }
        if (found == nullptr)
        {
            vdi->set_element(0, vint_variant(false));
        }
        else
        {
            if (found->process_callback == nullptr)
            {
                continue;
            }
            found->process_callback(vdi);
        }
        ui_data_item_set_updated(vdi);
    }
}

// Appends the localized text of a tag (or the tag itself) to a wide buffer.
static void game_info_append_tag(wchar_t* text, unsigned int text_size, const char* tag, wchar_t* scratch)
{
    if (localization_does_tag_exist(tag))
    {
        wcsncat(text, localization_get_display_string_req(tag, nullptr), text_size);
    }
    else
    {
        // Magic number: 0x40, the scratch buffer's size.
        copy_to_wide(scratch, tag, 0x40);
        wcsncat(text, scratch, text_size);
    }
}

// 0x008DEC60 (:1018)
// The server options as the shell's game info: settings, mode, map, score, time limit, starting equipment (backpack,
// weapons) and up to four other non-default options.
void multi_player_menu_system_shell_process_game_info_item(vint_data_item* vdi)
{
    char score[32];
    char time_limit[32];
    if (Mp_server_options.generic_options.point_limit == 0)
    {
        sprintf(score, "%s", "MENU_NONE");
    }
    else
    {
        sprintf(score, "%d", (unsigned int)Mp_server_options.generic_options.point_limit);
    }
    sprintf(time_limit, "%d:00", (unsigned int)Mp_server_options.generic_options.round_time_limit_minutes);
    vdi->set_element(1, vint_variant("MENU_SETTINGS"));
    vdi->set_element(2, vint_variant("MENU_MODE"));
    const char* mode_name;
    const multi_gameplay_mode mode = Mp_server_options.get_mode();
    if ((mode == MGPM_BAGMAN || Mp_server_options.get_mode() == MGPM_TEAM_BAGMAN) && !Bagman_dlc_unlocked)
    {
        mode_name = "MENU_NOT_APPLICABLE";
    }
    else
    {
        mode_name = multi_gameplay_mode_get_display_name(Mp_server_options.get_mode());
    }
    vdi->set_element(3, vint_variant(mode_name));
    vdi->set_element(4, vint_variant("MENU_MAP"));
    vdi->set_element(5, vint_variant(Mp_server_options.get_level() == nullptr ? "MENU_NOT_APPLICABLE" : Mp_server_options.get_level()->display_name));
    vdi->set_element(6, vint_variant("MENU_SCORE"));
    vdi->set_element(7, vint_variant((const char*)score));
    vdi->set_element(8, vint_variant("MENU_TIME_LIMIT"));
    vdi->set_element(9, vint_variant((const char*)time_limit));
    vdi->set_element(10, vint_variant("MENU_STARTING_EQUIPMENT"));

    // The starting equipment, ", " between the parts.
    wchar_t separator[4];
    wchar_t equipment[128];
    wchar_t scratch[64];
    copy_to_wide(separator, ", ", 4);
    copy_to_wide(equipment, "", 128);
    if (Mp_server_options.generic_options.starting_backpack_type == -1)
    {
        if (Mp_server_options.generic_options.starting_backpack == MGSBO_RANDOM)
        {
            if (localization_does_tag_exist("MENU_RANDOM_BACKPACK"))
            {
                wcsncpy(equipment, localization_get_display_string_req("MENU_RANDOM_BACKPACK", nullptr), 128);
            }
            else
            {
                copy_to_wide(equipment, "MENU_RANDOM_BACKPACK", 128);
            }
        }
    }
    else
    {
        const char* backpack = multi_game_backpack_get_display_name(Mp_server_options.generic_options.starting_backpack_type);
        if (localization_does_tag_exist(backpack))
        {
            wcsncpy(equipment, localization_get_display_string_req(backpack, nullptr), 128);
        }
        else
        {
            copy_to_wide(equipment, backpack, 128);
        }
    }
    const inv_item_info* weapon = multi_game_player_get_default_weapon(multi_game_data_get_my_handle(ST_GAME));
    if (weapon == nullptr)
    {
        if (equipment[0] != L'\0')
        {
            wcsncat(equipment, separator, 128);
        }
        wcsncat(equipment, localization_get_display_string_req("MENU_HAMMER_ONLY", nullptr), 128);
    }
    else
    {
        if (weapon->display_name != nullptr)
        {
            if (equipment[0] != L'\0')
            {
                wcsncat(equipment, separator, 128);
            }
            game_info_append_tag(equipment, 128, weapon->display_name, scratch);
        }
        const weapon_info* third = weapons_get_weapon_info(Mp_server_options.generic_options.third_weapon);
        if (third != nullptr)
        {
            if (equipment[0] != L'\0')
            {
                wcsncat(equipment, separator, 128);
            }
            game_info_append_tag(equipment, 128, third->weapon_inv_item_info->display_name, scratch);
        }
        const weapon_info* fourth = weapons_get_weapon_info(Mp_server_options.generic_options.fourth_weapon);
        if (fourth != nullptr)
        {
            if (equipment[0] != L'\0')
            {
                wcsncat(equipment, separator, 128);
            }
            game_info_append_tag(equipment, 128, fourth->weapon_inv_item_info->display_name, scratch);
        }
    }
    if (equipment[0] == L'\0')
    {
        wcsncpy(equipment, localization_get_display_string_req("MENU_UNKNOWN", nullptr), 128);
        equipment[127] = L'\0';
    }
    char encoded[512];
    vint_encode_wide_text(encoded, sizeof(encoded), equipment);
    vdi->set_element(11, vint_variant((const char*)encoded));
    vdi->set_element(12, vint_variant("MENU_OTHER_SETTINGS"));

    // The other options that are not at their defaults ("name:\r value\n"), at most four, else "default".
    wchar_t other[512];
    memset(other, 0, sizeof(other));
    char option_text[128];
    int count = 0;
    bool none = true;
    copy_to_wide(separator, ":\r ", 4);
    // Magic number: 0x3B, the option count (MGO_*).
    for (int option = MGO_GAME_MODE; option < 0x3B; option++)
    {
        const multi_game_options mgo = (multi_game_options)option;
        if (mgo == MGO_STARTING_BACKPACK || mgo == MGO_STARTING_WEAPON || mgo == MGO_THIRD_WEAPON || mgo == MGO_FOURTH_WEAPON || mgo == MGO_LEVEL
            || mgo == MGO_GAME_MODE || mgo == MGO_TIME_LIMIT || mgo == MGO_SCORE_LIMIT || multi_game_settings_is_option_default(&Mp_server_options, mgo))
        {
            continue;
        }
        game_info_append_tag(other, 512, multi_game_settings_get_option_name(mgo), scratch);
        wcsncat(other, separator, 512);
        multi_game_settings_get_option_text(option_text, sizeof(option_text), &Mp_server_options, mgo);
        game_info_append_tag(other, 512, option_text, scratch);
        copy_to_wide(scratch, "\n", 0x40);
        wcsncat(other, scratch, 512);
        count++;
        none = false;
        // Magic number: four options at most.
        if (count > 3)
        {
            break;
        }
    }
    if (none)
    {
        wcsncpy(other, localization_get_display_string_req("MENU_DEFAULT_OPTION", nullptr), 512);
    }
    char encoded_other[1536];
    vint_encode_wide_text(encoded_other, sizeof(encoded_other), other);
    vdi->set_element(13, vint_variant((const char*)encoded_other));
}

// 0x008AC650 (:1209)
void multi_player_menu_system_start_searching_for_syslink_servers()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = get_game_link_session();
    Server_search_ping_interval.set(0);
    memset(s_lan_search_results, 0, sizeof(s_lan_search_results));
    s_lan_num_search_results = 0;
    s_lan_search_running = true;
    if (!link->startSearchLobby(16, nullptr, 0))
    {
        keen::debug::nativePrintArgs("[syslink] startSearchLobby failed\n");
    }
    g_gameLinkMutex.unlock();
}

// 0x0088A3C0 (:1230)
void multi_player_menu_system_stop_searching_for_syslink_servers()
{
    Server_search_ping_interval.invalidate();
}

// 0x0088A3D0 (:1236)
void multi_player_menu_system_enable_syslink_pong()
{
    Search_search_pong_enabled = true;
}

// 0x0088A3E0 (:1241)
void multi_player_menu_system_disable_syslink_pong()
{
    Search_search_pong_enabled = false;
}

// 0x008CF8F0 (:1246)
// Original: a closed box clears the handle but not Mp_msgbox_type.
bool multi_player_menu_system_has_open_message_box()
{
    int handle = Mp_msgbox_handle;
    if (!ui_is_msgbox_active(Mp_msgbox_handle))
    {
        handle = -1;
        Mp_msgbox_handle = -1;
    }
    return handle != -1;
}

// 0x0090A5D0 (:1256)
// The player's options (kick from party, profile, mute, unmute) as caption items of an accept/cancel box titled with
// the name; with none, a warning box.
void multi_player_menu_system_pull_up_player_option_message_box(const multi_player_uid& player_uid, const wchar_t* player_name, bool allow_mute)
{
    if (!override_current_msgbox_with(MP_MBT_PLAYER_OPTIONS))
    {
        return;
    }
    // Whether the player is in one of the sessions (party, transition, game).
    bool connected = false;
    for (int type = ST_PARTY; type < 3; type++)
    {
        multi_session* session = multi_session_get_by_type((session_types)type);
        if (session != nullptr && session->get_state() != MSS_NONE && session->get_player_connection(player_uid) != nullptr)
        {
            connected = true;
            break;
        }
    }
    Num_active_playcard_options = 0;
    for (int option = 0; option < NUM_PLAYCARD_OPTIONS; option++)
    {
        switch (option)
        {
        case PCO_KICK_FROM_PARTY:
            if (!player_uid.is_equal(My_player_uid) && multi_game_party_in_party() && multi_game_party_am_i_leader(false) && allow_kick())
            {
                Active_playcard_options[Num_active_playcard_options++] = PCO_KICK_FROM_PARTY;
            }
            break;
        case PCO_SHOW_GAMERCARD:
            if (player_uid.is_official() && player_uid.is_online() && multi_core_platform_allow_gamer_card_request() && !Multi_syslink && SW_HasInvites())
            {
                Active_playcard_options[Num_active_playcard_options++] = PCO_SHOW_GAMERCARD;
            }
            break;
        case PCO_MUTE_PLAYER:
            if (!player_uid.is_equal(My_player_uid) && !multi_voice_platform_player_is_muted(player_uid) && connected && allow_mute && !Multi_syslink)
            {
                Active_playcard_options[Num_active_playcard_options++] = PCO_MUTE_PLAYER;
            }
            break;
        case PCO_UNMUTE_PLAYER:
            if (!player_uid.is_equal(My_player_uid) && multi_voice_platform_player_is_muted(player_uid) && connected && allow_mute && !Multi_syslink)
            {
                Active_playcard_options[Num_active_playcard_options++] = PCO_UNMUTE_PLAYER;
            }
            break;
        default:
            break;
        }
    }
    if (Num_active_playcard_options < 1)
    {
        Mp_msgbox_handle = ui_add_msgbox(MBT_OK, "MENU_WARNING", "MENU_NO_OPTIONS", playcard_cb, false, false, nullptr, nullptr, false);
        return;
    }
    Playercard_player_uid.set(player_uid);
    if (player_name == nullptr)
    {
        player_name = localization_get_display_string_req("MENU_UNKNOWN", nullptr);
    }
    wchar_t title[128];
    char encoded[384];
    _snprintf_safe(title, 128, L"%ls", player_name);
    vint_encode_wide_text(encoded, sizeof(encoded), title);
    Mp_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL, encoded, "", playcard_cb, false, false, nullptr, nullptr, false);
    for (int i = 0; i < Num_active_playcard_options; i++)
    {
        const char* caption;
        switch (Active_playcard_options[i])
        {
        case PCO_KICK_FROM_PARTY:
            caption = Kick_from_party_string;
            break;
        case PCO_SHOW_GAMERCARD:
            caption = Gamer_card_string;
            break;
        case PCO_MUTE_PLAYER:
            caption = Mute_player_string;
            break;
        case PCO_UNMUTE_PLAYER:
            caption = Unmute_player_string;
            break;
        default:
            continue;
        }
        ui_msgbox_add_caption_item(Mp_msgbox_handle, caption, false, false);
    }
}

// 0x008DF500 (:1384)
void multi_player_menu_system_close_leave_confirm_box()
{
    if (Mp_msgbox_handle != -1 && Mp_msgbox_type == MP_MBT_CONFIRM_LEAVE)
    {
        ui_hide_msgbox(Mp_msgbox_handle, false);
        Mp_msgbox_handle = -1;
        Mp_msgbox_type = MP_MBT_NONE;
    }
}

// 0x008F84D0 (:1393)
void multi_player_menu_system_pull_up_leave_confirm_message_box()
{
    if (!override_current_msgbox_with(MP_MBT_CONFIRM_LEAVE))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, Confirm_exit_title_string, Confirm_exit_desc_string, leave_cb, false, false, nullptr, nullptr, false);
}

// 0x008F8860 (:1400)
// A loading box (no buttons) while the host migrates.
void multi_player_menu_system_pull_up_migrate_message_box()
{
    if (!override_current_msgbox_with(MP_MBT_MIGRATION))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_LOADING, Reconnecting_string, "", nullptr, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008F8C00 (:1408)
void multi_player_menu_system_pull_up_lost_connection_box()
{
    if (!override_current_msgbox_with(MP_MBT_LOST_CONNECTION))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_OK, "MENU_MBOX_CONNECTION_LOST", "MENU_LOST_LOBBY_CONNECTION", generic_cb, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008F8F80 (:1416)
// A save type box (no buttons, blocks other boxes).
void multi_player_menu_system_pull_up_no_opponents_message_box()
{
    if (!override_current_msgbox_with(MP_MBT_NO_OPPONENTS))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_SAVE, "", Opponent_left_string, generic_cb, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008F9310 (:1424)
void multi_player_menu_system_pull_up_party_too_large_message_box()
{
    if (!override_current_msgbox_with(MP_MBT_PARTY_IS_TO_LARGE))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_OK, "", Party_too_big_string, generic_cb, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008F96A0 (:1432)
void multi_player_menu_system_pull_up_dlc_problem_message_box()
{
    if (!override_current_msgbox_with(MP_MBT_UNABLE_TO_LAUNCH))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_OK, "", "MENU_MP_DLC_NO_PLAYLIST", generic_cb, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008F9A20 (:1440)
void multi_player_menu_system_pull_up_cannot_join_this_session()
{
    if (!override_current_msgbox_with(MP_MBT_CANNOT_JOIN))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_OK, "", Cannot_join_session_string, nullptr, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008F9DB0 (:1448)
void multi_player_menu_system_pull_up_kick_from_party_confirm_message_box(const multi_player_uid& uid)
{
    if (!override_current_msgbox_with(MP_MBT_CONFIRM_PLAYER_KICK))
    {
        return;
    }
    wchar_t name[256];
    if (!multi_game_party_get_player_name(uid, name, 0x100))
    {
        name[0] = L' ';
        name[1] = L'\0';
    }
    wchar_t text[128];
    char encoded[512];
    _snprintf_safe(text, 0x80, localization_get_display_string_req(Kick_warning_string, nullptr), name);
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    Mp_msgbox_handle = ui_add_msgbox(MBT_ACCEPT_CANCEL_SELECT, Kick_warning_title_string, encoded, kick_from_party_cb, false, false, nullptr, nullptr, false);
}

// 0x008FA1F0 (:1471)
// What the clients lack (DLC level, backpack, mode, playlist), with the missing item's name.
void multi_player_menu_system_pull_up_unable_to_launch_message_box(client_start_test_results result)
{
    if (!override_current_msgbox_with(MP_MBT_UNABLE_TO_LAUNCH))
    {
        return;
    }
    wchar_t text[256];
    switch (result)
    {
    case CST_NO_LEVEL:
        _snprintf_safe(text, 0x100, localization_get_display_string_req("MENU_MP_DLC_NO_LEVEL", nullptr),
                       localization_get_display_string_req(Mp_server_options.get_level()->display_name, nullptr));
        break;
    case CST_NO_STARTING_BACKPACK:
        _snprintf_safe(text, 0x100, localization_get_display_string_req("MENU_MP_DLC_NO_BACKPACK", nullptr),
                       localization_get_display_string_req(multi_game_backpack_get_display_name(Mp_server_options.generic_options.starting_backpack_type), nullptr));
        break;
    case CST_NO_DEFAULT_BACKPACK:
        _snprintf_safe(text, 0x100, localization_get_display_string_req("MENU_MP_DLC_NO_BACKPACK", nullptr),
                       localization_get_display_string_req(multi_game_backpack_get_display_name(Mp_server_options.generic_options.backpack_spawn_type), nullptr));
        break;
    case CST_NO_MODE:
        _snprintf_safe(text, 0x100, localization_get_display_string_req("MENU_MP_DLC_NO_MODE", nullptr),
                       localization_get_display_string_req(multi_gameplay_mode_get_display_name(Mp_server_options.get_mode()), nullptr));
        break;
    case CST_NO_PLAYLIST:
        _snprintf_safe(text, 0x100, "%ls", localization_get_display_string_req("MENU_MP_DLC_NO_PLAYLIST", nullptr));
        // Original bug kept: no break, the playlist text is cleared by the default case.
        text[0] = L'\0';
        break;
    default:
        text[0] = L'\0';
        break;
    }
    char encoded[768];
    vint_encode_wide_text(encoded, sizeof(encoded), text);
    Mp_msgbox_handle = ui_add_msgbox(MBT_OK, "MENU_UNABLE_TO_START_GAME", encoded, generic_cb, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008FA6E0 (:1502)
void multi_player_menu_system_pull_up_generic_message_box(const char* title, const char* message)
{
    if (!override_current_msgbox_with(MP_MBT_GENERIC))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(MBT_OK, title, message, generic_cb, false, false, nullptr, nullptr, false);
    Playercard_player_uid.invalidate();
}

// 0x008FAA60 (:1510)
void multi_player_menu_system_pull_up_generic_message_box(msgbox_type mb_type, const char* title, const char* message, bool (*callback_fn)(int, int, msgbox_choice), const char* button1_override, const char* button2_override)
{
    if (!override_current_msgbox_with(MP_MBT_GENERIC))
    {
        return;
    }
    Mp_msgbox_handle = ui_add_msgbox(mb_type, title, message, callback_fn, false, false, button1_override, button2_override, false);
    Playercard_player_uid.invalidate();
}

// 0x008DF540 (:1518)
void multi_player_menu_system_close_message_box()
{
    if (Mp_msgbox_handle != -1)
    {
        ui_hide_msgbox(Mp_msgbox_handle, false);
        Playercard_player_uid.invalidate();
    }
    Mp_msgbox_type = MP_MBT_NONE;
    Mp_msgbox_handle = -1;
}

// 0x0088A3F0 (:1528)
void multi_player_menu_system_setup_hint_item(vint_data_item* vdi)
{
}

// 0x0088A400 (:1552)
void multi_player_menu_system_begin_hints(vint_data_item* vdi)
{
    if (vdi != nullptr)
    {
        Hint_data_item = vdi;
        Num_hints_added = 0;
    }
}

// 0x0088A420 (:1560)
// A hint "[image:<button>] <text>"; five at most.
void multi_player_menu_system_add_hint(const wchar_t* hint_text, control_button_action button)
{
    // Magic number: five hint elements.
    if (Hint_data_item == nullptr || Num_hints_added >= 5)
    {
        return;
    }
    if (hint_text == nullptr)
    {
        hint_text = L"";
    }
    char text[128];
    char image[128];
    char hint[128];
    keen::convertUTF16ToUTF8(text, sizeof(text), (const unsigned short*)hint_text);
    controls_get_button_image_name(image, sizeof(image), button, Player_controller_index);
    keen::formatString(hint, sizeof(hint), "[image:%s] %s", image, text);
    Hint_data_item->set_element(Num_hints_added, vint_variant((const char*)hint));
    Num_hints_added++;
}

// 0x0088A510 (:1580)
// The unused hints cleared; elements 5 and 6 are the select and gamer card buttons' images.
void multi_player_menu_system_end_hints()
{
    for (int i = Num_hints_added; i < 5; i++)
    {
        Hint_data_item->set_element(i, vint_variant((const char*)nullptr));
    }
    char image[32];
    char hint[128];
    controls_get_button_image_name(image, sizeof(image), CBA_MENU_SELECT, Player_controller_index);
    keen::formatString(hint, sizeof(hint), "[image:%s]", image);
    Hint_data_item->set_element(5, vint_variant((const char*)hint));
    controls_get_button_image_name(image, sizeof(image), CBA_MP_VIEW_GAMERCARD, Player_controller_index);
    keen::formatString(hint, sizeof(hint), "[image:%s]", image);
    Hint_data_item->set_element(6, vint_variant((const char*)hint));
    Hint_data_item = nullptr;
    Num_hints_added = 0;
}

// 0x0090AF90 (:1607)
// LTCG: state in ESI. The state's document loaded at once and kept for the restore, then the state pushed (the
// experience tally without its transition).
static void add_state_to_state_stack(game_state state)
{
    const char* name;
    bool transition = true;
    switch (state)
    {
    case GS_MULTIPLAYER_LIVE:
    case GS_MULTIPLAYER_CHANGE_MATCHMAKING:
    case GS_MULTIPLAYER_SEARCH_MATCHMAKING:
    case GS_MULTIPLAYER_GAME_LOBBY:
    case GS_MULTIPLAYER_PARTY_LOBBY:
    case GS_MULTIPLAYER_PARTY_OPTIONS:
    case GS_MULTIPLAYER_SYSLINK:
    case GS_MULTIPLAYER_SYSLINK_FIND_SERVERS:
    case GS_MULTIPLAYER_PLAYER_SELECT:
    case GS_MULTIPLAYER_TUTORIALS:
    case GS_MULTIPLAYER_LIVE_FIND_SERVERS:
        name = "rfg_ui_mp_shell";
        break;
    case GS_MULTIPLAYER_CHANGE_GAME_OPTIONS:
        name = "rfg_ui_mp_options";
        break;
    case GS_MULTIPLAYER_XP_TALLY:
        name = "rfg_ui_mp_xp_tally";
        transition = false;
        break;
    case GS_MULTIPLAYER_RESULTS:
    case GS_MULTIPLAYER_STATS:
        name = "rfg_ui_mp_stats";
        break;
    case GS_MULTIPLAYER_SCOREBOARD:
        name = "rfg_ui_mp_leaderboard";
        break;
    default:
        return;
    }
    int handle = ui_load_document(name, DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
    if (handle != -1)
    {
        State_stack_doc_handles.add_unique(handle);
        gameseq_push_state(state, true, transition);
    }
}

// 0x0090B060 (:1661)
// Back at the main menu after a match: the menus the player came through, pushed again with their documents, the
// results and the experience tally on top (the results only while connected or playing, the tally unless spectating).
void multi_player_menu_system_state_stack_load()
{
    if (State_stack_doc_handles.size() != 0 || !multi_game_lobby_pushed_to_game() || gameseq_get_state() != GS_MAINMENU)
    {
        return;
    }
    if (!Multi_syslink)
    {
        add_state_to_state_stack(GS_MULTIPLAYER_LIVE);
        if (Lobby_session->get_state() != MSS_NONE || !multi_game_spectating())
        {
            int handle = ui_load_document("rfg_ui_mp_shell", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            if (handle != -1)
            {
                State_stack_doc_handles.add_unique(handle);
                gameseq_push_state(GS_MULTIPLAYER_PARTY_LOBBY, true, true);
            }
            add_state_to_state_stack(GS_MULTIPLAYER_CHANGE_MATCHMAKING);
            handle = ui_load_document("rfg_ui_mp_shell", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            if (handle != -1)
            {
                State_stack_doc_handles.add_unique(handle);
                gameseq_push_state(GS_MULTIPLAYER_SEARCH_MATCHMAKING, true, true);
            }
            handle = ui_load_document("rfg_ui_mp_shell", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            if (handle != -1)
            {
                State_stack_doc_handles.add_unique(handle);
                gameseq_push_state(GS_MULTIPLAYER_GAME_LOBBY, true, true);
            }
        }
    }
    else
    {
        int handle = ui_load_document("rfg_ui_mp_shell", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
        if (handle != -1)
        {
            State_stack_doc_handles.add_unique(handle);
            gameseq_push_state(GS_MULTIPLAYER_SYSLINK, true, true);
        }
        // A client came through the server browser.
        if (Lobby_session->get_state() != MSS_NONE && !Lobby_session->am_i_host())
        {
            handle = ui_load_document("rfg_ui_mp_shell", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            if (handle != -1)
            {
                State_stack_doc_handles.add_unique(handle);
                gameseq_push_state(GS_MULTIPLAYER_SYSLINK_FIND_SERVERS, true, true);
            }
        }
        handle = ui_load_document("rfg_ui_mp_shell", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
        if (handle != -1)
        {
            State_stack_doc_handles.add_unique(handle);
            gameseq_push_state(GS_MULTIPLAYER_GAME_LOBBY, true, true);
        }
    }
    if (Lobby_session->get_state() != MSS_NONE || !multi_game_spectating())
    {
        Xp_tally_connected_when_entered = Lobby_session->get_state() != MSS_NONE;
        int handle = ui_load_document("rfg_ui_mp_stats", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
        if (handle != -1)
        {
            State_stack_doc_handles.add_unique(handle);
            gameseq_push_state(GS_MULTIPLAYER_RESULTS, true, true);
        }
        if (!multi_game_spectating())
        {
            handle = ui_load_document("rfg_ui_mp_xp_tally", DFIT_NONE, 0.0f, DLT_SYNCHRONOUS_HARDLOAD, false, STREAM_PRIORITY_AUDIO_CACHE_LOAD, false);
            if (handle != -1)
            {
                State_stack_doc_handles.add_unique(handle);
                gameseq_push_state(GS_MULTIPLAYER_XP_TALLY, true, false);
            }
        }
    }
}

// 0x008F1B90 (:1704)
// The restore's documents go (ui_unload_document without a fade, inlined).
void multi_player_menu_system_state_stack_unload()
{
    for (int i = 0; i < State_stack_doc_handles.size(); i++)
    {
        ui_unload_document(State_stack_doc_handles[i], DFOT_NONE, 0.0f);
    }
    State_stack_doc_handles.clear();
}

// 0x0088A650 (:1712)
bool multi_player_menu_system_state_stack_will_be_pushed()
{
    return !game_booted_to_main_menu() && multi_game_lobby_pushed_to_game();
}

// 0x008AC6C0 (:1724)
// The level image follows the server options' level: a changed level streams its image the next frame and keeps the
// old one until then.
void multi_player_menu_system_level_image_handshake()
{
    const multi_level_info* level = Mp_server_options.get_level();
    if (level == nullptr)
    {
        return;
    }
    if (Load_level_image_next_frame)
    {
        if (Old_level_image_name != nullptr)
        {
            ui_release_image(Old_level_image_name);
        }
        Old_level_image_name = Current_level_image_name;
        Current_level_image_name = level->level_image_name;
        if (Current_level_image_name != nullptr)
        {
            const unsigned int srid = stream2_container_find(Current_level_image_name);
            if (srid != 0)
            {
                stream2_request(srid);
            }
        }
        Load_level_image_next_frame = false;
    }
    if (Current_level_image_name == nullptr)
    {
        Current_level_image_name = level->level_image_name;
        if (Current_level_image_name != nullptr)
        {
            const unsigned int srid = stream2_container_find(Current_level_image_name);
            if (srid != 0)
            {
                stream2_request(srid);
            }
        }
    }
    else if (strcmp(Current_level_image_name, level->level_image_name) != 0)
    {
        Load_level_image_next_frame = true;
    }
}

// 0x008AC7A0 (:1762)
void multi_player_menu_system_level_image_reset()
{
    if (Current_level_image_name != nullptr)
    {
        const unsigned int srid = stream2_container_find(Current_level_image_name);
        if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
        Current_level_image_name = nullptr;
    }
    if (Old_level_image_name != nullptr)
    {
        const unsigned int srid = stream2_container_find(Old_level_image_name);
        if (srid != 0 && stream2_get_status(srid) != ST_UNLOADED)
        {
            stream2_release(srid);
        }
        Old_level_image_name = nullptr;
    }
}

// 0x008AC820 (:1775)
// The current image once loaded, else the old one while no change is pending.
const char* multi_player_menu_system_level_image_get()
{
    if (Current_level_image_name != nullptr)
    {
        const unsigned int srid = stream2_container_find(Current_level_image_name);
        if (srid != 0 && stream2_get_status(srid) == ST_LOADED)
        {
            return Current_level_image_name;
        }
    }
    if (!Load_level_image_next_frame && Old_level_image_name != nullptr)
    {
        const unsigned int srid = stream2_container_find(Old_level_image_name);
        if (srid != 0 && stream2_get_status(srid) == ST_LOADED)
        {
            return Old_level_image_name;
        }
    }
    return nullptr;
}

// 0x0088A670 (:1786)
const char* multi_player_menu_system_get_connection_status_string(multi_session_join_status status)
{
    switch (status)
    {
    case MSJS_JOIN_ACCEPT:
        return "MENU_MSJS_JOIN_ACCEPT";
    case MSJS_JOIN_DENY_INTERNAL_ERROR:
    case MSJS_JOIN_DENY_BUSY:
    case MSJS_JOIN_DENY_DUPLICATE:
        return "MENU_MSJS_JOIN_DENY_INTERNAL_ERROR";
    case MSJS_JOIN_DENY_VERSION:
        return "MENU_MSJS_JOIN_DENY_VERSION";
    case MSJS_JOIN_DENY_SESSION_DNE:
        return "MENU_MSJS_JOIN_DENY_SESSION_DNE";
    case MSJS_JOIN_DENY_SESSION_FULL:
        return "MENU_MSJS_JOIN_DENY_SESSION_FULL";
    case MSJS_JOIN_DENY_SESSION_LOCKED:
        return "MENU_MSJS_JOIN_DENY_SESSION_LOCKED";
    case MSJS_JOIN_DENY_PRIVATE:
        return "MENU_MSJS_JOIN_DENY_PRIVATE";
    case MSJS_JOIN_DENY_TIMEOUT:
        return "MENU_MSJS_JOIN_DENY_TIMEOUT";
    case MSJS_JOIN_DENY_DLC_MISSING:
        return "MENU_MSJS_JOIN_DENY_DLC_MISSING";
    case MSJS_INVALID:
        return "MENU_MSJS_JOIN_IN_PROGRESS";
    default:
        return "MENU_MSJS_JOIN_DENY_INTERNAL_ERROR";
    }
}

// 0x008CF920 (:1810)
// The hint under the mouse (the first one wins), unless a message box is up.
int multi_player_menu_system_input_mouse_hints(int docHandle, int num_hints)
{
    int hint = -1;
    if (Utility_doc_handle == -1 || !any_msgbox_open())
    {
        if (num_hints > 3 && ui_mouse_over_hint(kHintText4, docHandle, true))
        {
            hint = 3;
        }
        if (num_hints > 2 && ui_mouse_over_hint(kHintText3, docHandle, true))
        {
            hint = 2;
        }
        if (num_hints > 1 && ui_mouse_over_hint(kHintText2, docHandle, true))
        {
            hint = 1;
        }
        if (num_hints > 0 && ui_mouse_over_hint(kHintText1, docHandle, true))
        {
            hint = 0;
        }
    }
    return hint;
}

// 0x008AC880 (:1848)
// Wraps around; the up, down or "same" sound.
void multi_player_menu_system_set_menu_selection(int new_selection, int& selection_var, int num_in_menu)
{
    const int old_selection = selection_var;
    selection_var = new_selection;
    if (new_selection < 0)
    {
        selection_var = num_in_menu - 1;
    }
    if (new_selection > num_in_menu - 1)
    {
        selection_var = 0;
    }
    if (old_selection < new_selection)
    {
        audiolib_cue_play(Audio_info[1], nullptr, nullptr);
    }
    else if (new_selection < old_selection)
    {
        audiolib_cue_play(Audio_info[0], nullptr, nullptr);
    }
    else
    {
        audiolib_cue_play(Audio_info[11], nullptr, nullptr);
    }
}

// The input id that may read the buttons (ui_input_id_has_focus, inlined): -1 while nothing is locked, else the lock
// or the top request.
static bool menu_input_has_focus(int input_id)
{
    const int lock = Current_input_lock_id != -1 ? Current_input_lock_id : 0;
    if (input_id == -1 && lock == 0)
    {
        return true;
    }
    int top = Current_input_lock_id;
    if (Current_input_lock_id == -1)
    {
        top = Input_types.size() - 1 < 0 ? 1 : Input_types[Input_types.size() - 1].id;
    }
    return input_id == top;
}

// A button of Button_info pressed (and not waiting for its release).
static bool menu_button_pressed(int input_id, int button)
{
    return menu_input_has_focus(input_id) && !Button_info.binfo[button].m_button_wait_for_up && Button_info.binfo[button].m_button_input > 0.0f;
}

// 0x008CF9D0 (:1875)
// Up (button 1 or 5) and down (2 or 6) move the selection; a menu of one item stays at 0 and consumes nothing.
void multi_player_menu_system_handle_menu_key_input(int& selection_var, int num_in_menu, int input_id, bool& events_consumed)
{
    const bool up = menu_button_pressed(input_id, 1) || menu_button_pressed(input_id, 5);
    const bool down = menu_button_pressed(input_id, 2) || menu_button_pressed(input_id, 6);
    events_consumed = true;
    if (num_in_menu < 2)
    {
        events_consumed = false;
        selection_var = 0;
        return;
    }
    if (up)
    {
        multi_player_menu_system_set_menu_selection(selection_var - 1, selection_var, num_in_menu);
    }
    else if (down)
    {
        multi_player_menu_system_set_menu_selection(selection_var + 1, selection_var, num_in_menu);
    }
    else
    {
        events_consumed = false;
    }
}

// 0x008CFB70 (:1900)
// The row under the mouse ("mp_shell_row_main", "mp_shell_row_main2", ...) when it moved or clicked, unless a message box
// is up.
int multi_player_menu_system_handle_menu_mouse_input(int& selection_var, int num_in_menu, int input_id, int doc_handle, bool events_consumed)
{
    if (!mouse_just_moved() && !mouse_just_released(0))
    {
        return -1;
    }
    if (Utility_doc_handle != -1 && any_msgbox_open())
    {
        return -1;
    }
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return -1;
    }
    char name[128];
    for (int i = 0; i < num_in_menu; i++)
    {
        if (i == 0)
        {
            keen::formatString(name, sizeof(name), "%s", kGroup_name);
        }
        else
        {
            keen::formatString(name, sizeof(name), "%s%d", kGroup_name, i + 1);
        }
        if (ui_is_mouse_over_vint_object(vint_object_base::find_object(name, info->m_doc)))
        {
            return i;
        }
    }
    return -1;
}

// 0x008CFCA0 (:1932)
void multi_player_menu_system_handle_arrow_clicks(mp_mouse_arrow_click_info& click_info, int doc_handle)
{
    click_info = MP_ARROW_CLICK_NONE;
    if (!mouse_just_released(0))
    {
        return;
    }
    vint_doc_info* info = ui_get_doc_info(doc_handle);
    if (info == nullptr || info->m_doc == nullptr)
    {
        return;
    }
    if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kRightArrow, info->m_doc)))
    {
        click_info = MP_ARROW_CLICK_RIGHT;
        return;
    }
    if (ui_is_mouse_over_vint_object(vint_object_base::find_object(kLeftArrow, info->m_doc)))
    {
        click_info = MP_ARROW_CLICK_LEFT;
    }
}

// 0x0088A700 (:1960)
void multi_player_menu_system_clear_hint_config(int* config_array, int size, int& current_count)
{
    for (int i = 0; i < size; i++)
    {
        config_array[i] = -1;
    }
    current_count = 0;
}

// 0x0088A730 (:1968)
void multi_player_menu_system_add_hint_config(int& num_hints, int* hint_config, int new_value)
{
    hint_config[num_hints] = new_value;
    num_hints++;
}
