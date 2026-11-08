// Original: volition/rfg/code/multi/multi_core/global/multi_core_platform.cpp
//
// The platform layer as the game sees it: the game type and mode contexts, user properties, pending invites (Steam and
// Discord), the scoreboard state, achievements, and wrappers of the platform implementation
// (multi_core_platform_generic.cpp). rfg.exe inlines most wrappers (multi_core_platform_send_packet is a copy of the
// internal function); here they call it.
// The rich presence (set_presence_state, process_presence_state, multi_core_platform_system_process) came with
// multi.cpp in MP3.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode_shared.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/time/timestamp.h"
#include "keen/base/mutex.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/stringscanner.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/thirdparty/discord-rpc/include/discord_rpc.h"
#include "unit4/gamesession/localgamesession.hpp"
#include "unit4/gamesession/remotegamesession.hpp"

#include <string.h>
#include <time.h>

// ---- globals (no source file in the PDB; placed in their only writer)
platform_game_type_context Current_game_type_context; // 0x02FE8DC0
platform_game_mode_context Current_game_mode_context; // 0x02FE8DB8
unsigned int Current_user_playlist; // 0x02FE73B8
unsigned int Current_user_dlc_mask; // 0x02FE7388
unsigned int Current_user_playlist_version; // 0x02FE73BC
unsigned int Current_session_is_started; // 0x02FEB38C
unsigned int Game_properties[NUM_PLATFORM_GAME_PROPERTIES]; // 0x02FE5AFC
void (*Login_changed_cb)(bool, bool); // 0x02FE5BA8
const char* Platform_game_type_context_names[NUM_PLATFORM_GAME_TYPE_CONTEXTS] = {"MENU_RANKED", "MENU_SOCIAL"}; // 0x0167DDB8
// Original data: the playlist contexts 7 to 15 have no name (null); the names of 3 to 6 are mode names.
const char* Platform_game_mode_context_names[NUM_PLATFORM_GAME_MODE_CONTEXTS] = {"Main Menu", "Campaign", "Multiplayer", "Anarchy",
    "Team Anarchy", "Bagman", "Team Bagman", "Capture the Flag", "Siege", "Damage Control"}; // 0x0167DDC0

// ---- file-static globals
static const char* Rich_presence_strings[15] = {"Rich_Presence_0_Desc", "Rich_Presence_1_Desc", "Rich_Presence_2_Desc", "Rich_Presence_3_Desc", "Rich_Presence_4_Desc", "Rich_Presence_5_Desc", "Rich_Presence_6_Desc", "Rich_Presence_7_Desc", "Rich_Presence_8_Desc", "Rich_Presence_9_Desc", "Rich_Presence_10_Desc", "Rich_Presence_11_Desc", "Rich_Presence_12_Desc", "Rich_Presence_13_Desc", "Rich_Presence_14_Desc"}; // 0x0167DD7C
// size 0xC (multi_gameplay_mode.cpp's type; defined here with its table, see below)
struct multi_gameplay_mode_names_multi_game
{
    const char* display_name; // 0x00
    const char* internal_name; // 0x04
    const char* enum_name; // 0x08
};

// The current mode and the mode names: statics of multi_gameplay_mode.cpp in the stubs (the stem rule), placed here,
// their first user in the unity (multiplayer.md MP0).
static multi_gameplay_mode Multi_gameplay_mode = MGPM_NONE; // 0x0167DEE0, initial data ffffffff
// {display_name (localization tag), internal_name, enum_name}
static multi_gameplay_mode_names_multi_game Multi_gameplay_mode_names[8] = { // 0x0167E4A8
    {"MENU_MP_MODE_ANARCHY", "deathmatch", nullptr},
    {"MENU_MP_MODE_TEAM_ANARCHY", "team_deathmatch", nullptr},
    {"MENU_MP_MODE_CAPTURE_FLAG", "capture_the_flag", nullptr},
    {"MENU_MP_MODE_SIEGE", "siege", nullptr},
    {"MENU_MP_MODE_DMG_CNTRL", "king_of_the_hill", nullptr},
    {"MENU_MP_MODE_BAGMAN", "bagman", nullptr},
    {"MENU_MP_MODE_TM_BAGMAN", "team_bagman", nullptr},
    {"MENU_MP_MODE_DEMOLITION", "demolition", nullptr},
};
static timestamp_realtime Presence_process_tick; // 0x01691CB8, initial data ffffffff
static multi_timer Timer_time; // 0x01693A64, initial data ffffffff
// bool Scoreboard_system_initd (0x02FEB5DE) is used here too; placed in multi.cpp.
static platform_game_presence Current_presence_state; // 0x02FEB60C
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
static int My_xp; // 0x02FEB760
// pending_invite_info Pending_invite (0x02FEBBD8) is used here too; placed in multi_core_platform_generic.cpp.

// ---- internal functions (MP3, see above)
static void set_presence_state(platform_game_presence presence); // 0x0094A4B0 (:239)
static void process_presence_state(); // 0x009A9BB0 (:250)

// 0x00971750: the session parameters' own constructor (zero, the default connection type)
multi_core_session_info::multi_core_session_info()
{
}

// 0x00459940: the ids are cleared by their constructors; pending and had_invite_when_entering_multiplayer are left
// as they are (zero for the file static)
pending_invite_info::pending_invite_info()
{
}

// 0x00970360 (.h:180): had_invite_when_entering_multiplayer is kept
void pending_invite_info::clear()
{
    invitation_data.lobbyId.Clear();
    invitation_data.friendId.Clear();
    pending = false;
}

// 0x00972910 (:557): the main menu presence and contexts; the remote game session is created
void multi_core_platform_system_init()
{
    Current_game_type_context = PGTC_PUBLIC;
    Current_game_mode_context = PGMC_MAIN_MENU;
    Login_changed_cb = nullptr;
    multi_core_platform_internal_system_init();
    Current_user_playlist = 0;
    Current_user_dlc_mask = 0;
    Current_user_playlist_version = 0;
    Current_session_is_started = 0;
    Pending_invite.invitation_data.lobbyId = CSteamID();
    Pending_invite.invitation_data.friendId = CSteamID();
    Pending_invite.pending = false;
    rfg_get_local_game_session()->setPresence(PGMP_MAIN_MENU);
    Presence_process_tick.set(4000); // ms until the presence is processed again
    Current_presence_state = PGMP_MAIN_MENU;
}

// 0x0094A4B0 (:239)
static void set_presence_state(platform_game_presence presence)
{
    rfg_get_local_game_session()->setPresence(presence);
    Presence_process_tick.set(4000);
    Current_presence_state = presence;
}

// 0x009A9BB0 (:250): every 4 s (with a local controller): the presence from the game state, and the Discord rich
// presence: the state text, and in a session the mode, the player counts, the match end time and the lobby id as
// party id and join/spectate secrets (a party lobby only when it is public)
static void process_presence_state()
{
    if (!Presence_process_tick.elapsed() || Player_controller_index == 0xFFFFFFFFu)
    {
        return;
    }
    multi_session* sp = nullptr;
    platform_game_presence presence;
    if (Multiplayer)
    {
        sp = multi_session_get_by_type(ST_GAME);
        if (gameseq_state_is_active(GS_MULTIPLAYER_RESULTS))
        {
            presence = PGMP_MP_END_MATCH;
        }
        else if (sp != nullptr && (gameseq_state_is_active(GS_LOAD) || gameseq_state_is_active(GS_GAMEPLAY)))
        {
            if (Spectating)
            {
                presence = PGMP_MP_SPECTATOR;
            }
            else if (!sp->get_info()->party_lobby)
            {
                presence = PGMP_MP_QUICK_MATCH;
            }
            else
            {
                presence = PGMP_MATCH;
            }
        }
        else if (gameseq_state_is_active(GS_MULTIPLAYER_SEARCH_MATCHMAKING))
        {
            presence = PGMP_MP_MATCHMAKING;
        }
        else if (gameseq_state_is_active(GS_MULTIPLAYER_PARTY_LOBBY))
        {
            presence = PGMP_MP_PARTY_LOBBY;
        }
        else
        {
            presence = PGMP_MP_MENUS;
        }
    }
    else if (wrecking_crew_is_active())
    {
        presence = PGMP_WC;
    }
    else if (gameseq_state_is_active(GS_WRECKING_CREW_MAIN_MENU))
    {
        presence = PGMP_MP_MENUS;
    }
    else if (gameseq_state_is_active(GS_LOAD_CUTSCENE) || gameseq_state_is_active(GS_VIDEO_CUTSCENE_PLAY))
    {
        presence = PGMP_SP_CUTSCENE;
    }
    else if (gameseq_state_is_active(GS_LOAD))
    {
        presence = PGMP_LOADING;
    }
    else if (gameseq_state_is_active(GS_GAMEPLAY))
    {
        if (mission_is_active())
        {
            presence = PGMP_SP_MISSION;
        }
        else if (activity_get_mode() != 0)
        {
            presence = PGMP_SP_ACTIVITY;
        }
        else
        {
            presence = PGMP_SP_DEFAULT;
        }
    }
    else
    {
        presence = PGMP_MAIN_MENU;
    }
    set_presence_state(presence);
    DiscordRichPresence discord;
    memset(&discord, 0, sizeof(discord));
    char state_text[128];
    state_text[0] = '\0';
    const wchar_t* state_string = localization_get_display_string_req(Rich_presence_strings[Current_presence_state], nullptr);
    if (state_string != nullptr)
    {
        keen::convertUTF16ToUTF8(state_text, sizeof(state_text), (const uint16*)state_string);
        discord.state = state_text;
    }
    // Their scopes span the whole function: Discord copies the strings when the presence is updated below.
    char mode_text[256];
    char empty_a[128];
    char empty_b[128];
    char empty_c[128];
    char empty_d[128];
    char xp_text[128];
    char details[1024];
    char party_id[64];
    char join_secret[64];
    char spectate_secret[64];
    if (sp != nullptr && sp->get_info() != nullptr)
    {
        const multi_session_info_base* info = sp->get_info();
        // A party lobby takes joins and spectators only when it is public.
        const bool can_join = !info->party_lobby || sp->get_party_lobby_invite_type() == SIT_ALL;
        const bool can_spectate = !info->party_lobby || sp->get_party_lobby_invite_type() == SIT_ALL;
        int party_size = sp->get_slot_use_data().used;
        int party_max = info->max_slots;
        const multi_game_playlist* playlist = multi_game_playlist_get_by_id(Current_user_playlist);
        if (playlist != nullptr)
        {
            party_size = multi_game_data_count_players(NUM_SESSION_TYPES);
            party_max = playlist->max_players;
        }
        int64_t end_time = 0;
        if (Multi_match_info.match_state == MGMS_GAMEPLAY && Timer_time.valid())
        {
            end_time = _time64(nullptr) + Timer_time.time_until() / 1000;
        }
        const multi_gameplay_mode mode = Multiplayer ? Multi_gameplay_mode : MGPM_NONE;
        mode_text[0] = '\0';
        if (mode != MGPM_NONE)
        {
            const wchar_t* mode_name = localization_get_display_string_req(multi_gameplay_mode_get_display_name(mode), nullptr);
            const wchar_t* mode_title = localization_get_display_string_req("MENU_GAME_MODE", nullptr);
            if (mode_name != nullptr && mode_title != nullptr)
            {
                char name_utf8[128];
                char title_utf8[128];
                keen::convertUTF16ToUTF8(name_utf8, sizeof(name_utf8), (const uint16*)mode_name);
                keen::convertUTF16ToUTF8(title_utf8, sizeof(title_utf8), (const uint16*)mode_title);
                keen::formatString(mode_text, sizeof(mode_text), "%s: %s - ", title_utf8, name_utf8);
            }
        }
        // Original data: four empty parts between the mode and the XP (cut features).
        empty_a[0] = '\0';
        empty_b[0] = '\0';
        empty_c[0] = '\0';
        empty_d[0] = '\0';
        xp_text[0] = '\0';
        const wchar_t* xp_title = localization_get_display_string_req("MENU_XP", nullptr);
        if (xp_title != nullptr)
        {
            char xp_utf8[128];
            keen::convertUTF16ToUTF8(xp_utf8, sizeof(xp_utf8), (const uint16*)xp_title);
            keen::formatString(xp_text, sizeof(xp_text), "%s: %d", xp_utf8, My_xp);
        }
        keen::formatString(details, sizeof(details), "%s%s%s%s%s%s", mode_text, empty_a, empty_b, empty_c, empty_d, xp_text);
        discord.details = details;
        discord.partySize = party_size;
        discord.partyMax = party_max;
        discord.endTimestamp = end_time;
        if (can_join || can_spectate)
        {
            const uint64_t lobby_id = ((const multi_session_info*)info)->m_lobbyId.id.asUint64;
            keen::formatString(party_id, sizeof(party_id), "%llx", lobby_id);
            discord.partyId = party_id;
            keen::formatString(join_secret, sizeof(join_secret), "x%llx", lobby_id);
            keen::formatString(spectate_secret, sizeof(spectate_secret), "0x%llx", lobby_id);
            if (can_join)
            {
                discord.joinSecret = join_secret;
            }
            if (can_spectate)
            {
                discord.spectateSecret = spectate_secret;
            }
        }
    }
    Discord_UpdatePresence(&discord);
    Presence_process_tick.set(4000);
}

// 0x009AA0A0 (:586): rfg.exe inlines this into multi_process
void multi_core_platform_system_process()
{
    process_presence_state();
    multi_core_platform_internal_system_process();
}

// 0x0094A540 (:581)
void multi_core_platform_system_shutdown()
{
    g_gameLinkMutex.lock(0);
    Remote_game_session.destroy(rfg_get_cpu_scratch_allocator());
    g_gameLinkMutex.unlock();
}

// 0x0094A570 (:594)
void multi_core_platform_system_handshake()
{
}

// 0x0094A580 (:599)
void multi_core_platform_system_render()
{
}

// 0x0094A590 (:604)
platform_nat_type multi_core_platform_get_nat_type()
{
    return PNT_OPEN;
}

// 0x00972990 (:609): public multiplayer
void multi_core_platform_multi_init(mempool_base&)
{
    if (Current_game_type_context != PGTC_PUBLIC)
    {
        Current_game_type_context = PGTC_PUBLIC;
    }
    if (Current_game_mode_context != PGMC_MULTIPLAYER)
    {
        Current_game_mode_context = PGMC_MULTIPLAYER;
    }
}

// 0x0094A5A0 (:616)
void multi_core_platform_multi_shutdown()
{
}

// 0x0094A5B0 (:621)
void multi_core_platform_set_pending_invite(const pending_invite_info* src_invite_info)
{
    if (src_invite_info != nullptr)
    {
        Pending_invite.invitation_data.lobbyId = src_invite_info->invitation_data.lobbyId;
        Pending_invite.invitation_data.friendId = src_invite_info->invitation_data.friendId;
        Pending_invite.pending = true;
    }
}

// 0x0094A5F0 (:630)
void multi_core_platform_get_pending_invite(pending_invite_info* invite_info)
{
    invite_info->invitation_data.lobbyId = Pending_invite.invitation_data.lobbyId;
    invite_info->invitation_data.friendId = Pending_invite.invitation_data.friendId;
    invite_info->pending = Pending_invite.pending;
    invite_info->had_invite_when_entering_multiplayer = Pending_invite.had_invite_when_entering_multiplayer;
}

// 0x0094A620 (:639)
bool multi_core_platform_has_pending_invite()
{
    return Pending_invite.pending;
}

// 0x0094A630 (:644)
void multi_core_platform_clear_pending_invite()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::setInvitationData(nullptr);
    Pending_invite.pending = false;
    Pending_invite.had_invite_when_entering_multiplayer = false;
    g_gameLinkMutex.unlock();
}

// 0x0094A660 (:652)
bool multi_core_platform_get_had_last_invite_on_multiplayer_start()
{
    return Pending_invite.had_invite_when_entering_multiplayer;
}

// 0x0094A670 (:657)
void multi_core_platform_set_had_last_invite_on_multiplayer_start(bool value)
{
    Pending_invite.had_invite_when_entering_multiplayer = value;
}

// 0x0094A680 (:663)
void multi_core_platform_register_login_changed_callback(void (*login_changed_cb)(bool, bool))
{
    Login_changed_cb = login_changed_cb;
}

// 0x0094A690 (:668)
void multi_core_platform_login_changed(bool only_changed_connectivity, bool remote_game_session_related)
{
    if (Login_changed_cb != nullptr)
    {
        Login_changed_cb(only_changed_connectivity, remote_game_session_related);
    }
}

// 0x0094A6A0 (:675): the state is read for a stripped check
void multi_core_platform_set_active_controller(unsigned int controller_index)
{
    Player_controller_index = controller_index;
    gameseq_get_state();
}

// 0x0094A6B0 (:686)
unsigned int multi_core_platform_get_active_controller()
{
    return Player_controller_index;
}

// 0x0094A6C0 (:691)
bool multi_core_platform_signin_change_is_pending()
{
    return false;
}

// 0x009729C0 (:696)
bool multi_core_platform_get_player_name(wchar_t* player_name, int max_player_name_size)
{
    return multi_core_platform_internal_get_player_name(player_name, max_player_name_size);
}

// 0x0094A6D0 (:754)
const char* multi_core_platform_get_game_type_context_name(platform_game_type_context context)
{
    if (context < NUM_PLATFORM_GAME_TYPE_CONTEXTS)
    {
        return Platform_game_type_context_names[context];
    }
    return "Invalid platform_game_type_context";
}

// 0x0094A6F0 (:760)
const char* multi_core_platform_get_game_mode_context_name(platform_game_mode_context context)
{
    if (context < NUM_PLATFORM_GAME_MODE_CONTEXTS)
    {
        return Platform_game_mode_context_names[context];
    }
    return "Invalid platform_game_mode_context";
}

// 0x0094A710 (:766)
void multi_core_platform_set_game_type_context(platform_game_type_context context)
{
    if (Current_game_type_context != context)
    {
        Current_game_type_context = context;
    }
}

// 0x0094A730 (:774)
platform_game_type_context multi_core_platform_get_game_type_context()
{
    return Current_game_type_context;
}

// 0x0094A740 (:779)
void multi_core_platform_set_game_mode_context(platform_game_mode_context context)
{
    if (Current_game_mode_context != context)
    {
        Current_game_mode_context = context;
    }
}

// 0x0094A760 (:787)
platform_game_mode_context multi_core_platform_get_game_mode_context()
{
    return Current_game_mode_context;
}

// 0x009729D0 (:792)
bool multi_core_platform_is_signed_into_profile(int controller_index)
{
    return multi_core_platform_internal_is_signed_into_profile(controller_index);
}

// 0x0094A770 (:798): a session whose link is not offline
bool multi_core_platform_is_connected_to_internet()
{
    g_gameLinkMutex.lock(0);
    if (!Remote_game_session.isInsideGameLinkSession())
    {
        g_gameLinkMutex.unlock();
        return false;
    }
    const kaiko::GameLinkState state = Remote_game_session.getGameLinkSession()->getCurrentState();
    g_gameLinkMutex.unlock();
    return state != kaiko::GameLinkState_Offline;
}

// 0x0094A7C0 (:809)
bool multi_core_platform_is_connected_to_network()
{
    return multi_core_platform_internal_is_connected_to_network();
}

// 0x0094A7D0 (:818)
bool multi_core_platform_ui_is_active()
{
    return multi_core_platform_internal_ui_is_active();
}

// 0x0094A7E0 (:823)
void multi_core_platform_show_login_ui(bool)
{
}

// 0x0094A7F0 (:828)
void multi_core_platform_show_gamercard(const multi_player_uid& player_uid)
{
    multi_core_platform_internal_show_gamercard(player_uid);
}

// 0x0094A820 (:833)
bool multi_core_platform_allow_gamer_card_request()
{
    return true;
}

// 0x0094A830 (:838)
void multi_core_platform_show_player_review(const multi_player_uid&)
{
}

// 0x0094A840 (:843)
void multi_core_platform_add_friend(const multi_player_uid&)
{
}

// 0x0094A850 (:850)
void multi_core_platform_show_friends_ui()
{
    multi_core_platform_internal_show_friends_ui();
}

// 0x009AA0B0 (:857)
bool multi_core_platform_session_start(const multi_core_session_info& session_info)
{
    return multi_core_platform_internal_session_start(session_info);
}

// 0x0094A880 (:862)
void multi_core_platform_session_end()
{
    multi_core_platform_internal_session_end();
}

// 0x0094A8A0 (:867)
bool multi_core_platform_session_has_critical_error()
{
    return multi_core_platform_internal_session_has_critical_error();
}

// 0x0094A8D0 (:873)
void multi_core_platform_register_massage_send_data_callback(bool (*)(const unsigned char*, int, unsigned char*, int, int&, unsigned char*, int&))
{
}

// 0x0094A8E0 (:880)
bool multi_core_platform_send_packet(const connection_address& target, unsigned char* send_data, unsigned int data_length)
{
    return multi_core_platform_internal_send_packet(target, send_data, data_length);
}

// 0x0098FFF0 (:885)
unsigned int multi_core_platform_receive_packet(connection_address& origin, unsigned char* data_buffer, unsigned int capacity)
{
    return multi_core_platform_internal_receive_packet(origin, data_buffer, capacity);
}

// 0x0094A8F0 (:892)
unsigned int multi_core_platform_get_game_property(platform_game_property pgp)
{
    return Game_properties[pgp];
}

// 0x0094A900 (:897): only kept here (the PC platform does not publish them)
void multi_core_platform_set_game_property(platform_game_property pgp, unsigned int value)
{
    Game_properties[pgp] = value;
}

// 0x0094A910 (:903)
unsigned int multi_core_platform_get_user_playlist()
{
    return Current_user_playlist;
}

// 0x0094A920 (:908)
void multi_core_platform_set_user_playlist(unsigned int playlist)
{
    Current_user_playlist = playlist;
}

// 0x0094A930 (:914)
platform_op_status multi_core_platform_get_user_playlist_state()
{
    return POS_SUCCESS;
}

// 0x0094A940 (:919)
unsigned int multi_core_platform_get_user_dlc_mask()
{
    return Current_user_dlc_mask;
}

// 0x0094A950 (:924)
void multi_core_platform_set_user_dlc_mask(unsigned int playlist_entry)
{
    Current_user_dlc_mask = playlist_entry;
}

// 0x0094A960 (:930)
platform_op_status multi_core_platform_get_user_dlc_mask_state()
{
    return POS_SUCCESS;
}

// 0x0094A970 (:935)
unsigned int multi_core_platform_get_user_playlist_version()
{
    return Current_user_playlist_version;
}

// 0x0094A980 (:940)
void multi_core_platform_set_user_playlist_version(unsigned int usage)
{
    Current_user_playlist_version = usage;
}

// 0x0094A990 (:946)
platform_op_status multi_core_platform_get_user_playlist_version_state()
{
    return POS_SUCCESS;
}

// 0x0094A9A0 (:951)
unsigned int multi_core_platform_get_session_is_started()
{
    return Current_session_is_started;
}

// 0x0094A9B0 (:956)
void multi_core_platform_set_session_is_started(unsigned int started)
{
    Current_session_is_started = started;
}

// 0x0094A9C0 (:962)
platform_op_status multi_core_platform_get_session_is_started_state()
{
    return POS_SUCCESS;
}

// 0x0094A9D0 (:971)
void multi_core_platform_retrieve_remote_file(unsigned char**, unsigned int*)
{
}

// 0x0094A9E0 (:977)
platform_op_status multi_core_platform_get_retrieve_remote_file_state()
{
    return POS_FAILED;
}

// 0x0094A9F0 (:982)
void multi_core_platform_remote_file_process()
{
}

// 0x009AA0C0 (:987)
void multi_core_platform_get_player_skill(skill_info* skill, const multi_player_uid& player_uid)
{
    multi_core_platform_internal_get_player_skill(skill, player_uid);
}

// 0x0094AA00 (:992)
void multi_core_platform_get_aggregate_skill(skill_info& aggregate_skill, skill_info* skills, unsigned int num_skills)
{
    multi_core_platform_internal_get_aggregate_skill(aggregate_skill, skills, num_skills);
}

// 0x0094AA10 (:997)
bool multi_core_platform_get_large_skill_variance(const multi_player_uid*, unsigned int)
{
    return false;
}

// 0x0094AA20 (:1003)
void multi_core_platform_get_logged_in_users(unsigned int* controller_indices, int& num_users, const int max_users)
{
    multi_core_platform_internal_get_logged_in_users(controller_indices, num_users, max_users);
}

// 0x0094AA40 (:1008)
int multi_core_platform_get_dlc_avail()
{
    return 0;
}

// 0x0094AA50 (:1013)
bool multi_core_platform_scoreboard_ready()
{
    return Scoreboard_system_initd;
}

// 0x00990000 (:1020)
bool multi_core_platform_scoreboard_init(mempool* pool, int max_entries)
{
    if (!Scoreboard_system_initd)
    {
        Scoreboard_system_initd = multi_core_platform_internal_scoreboard_init(pool, max_entries);
    }
    return Scoreboard_system_initd;
}

// 0x00972A20 (:1031)
void multi_core_platform_scoreboard_shutdown()
{
    if (Scoreboard_system_initd)
    {
        multi_core_platform_internal_scoreboard_shutdown();
        Scoreboard_system_initd = false;
    }
}

// 0x0094AA60 (:1040)
const char* multi_core_platform_scoreboard_get_display_name(platform_scoreboard_type scoreboard_type, platform_game_type_context context)
{
    if (context == PGTC_PUBLIC)
    {
        switch (scoreboard_type)
        {
        case PST_XP:
            return "SCOREBOARD_PUBLIC_XP";
        case PST_WINS:
            return "SCOREBOARD_PUBLIC_WINS";
        case PST_KILLS:
            return "SCOREBOARD_PUBLIC_KILLS";
        case PST_DESTRUCTION:
            return "SCOREBOARD_PUBLIC_DESTRUCTION";
        default:
            return "UNKNOWN PUBLIC SCOREBOARD";
        }
    }
    switch (scoreboard_type)
    {
    case PST_XP:
        return "SCOREBOARD_RANKED_XP";
    case PST_WINS:
        return "SCOREBOARD_RANKED_WINS";
    case PST_KILLS:
        return "SCOREBOARD_RANKED_KILLS";
    case PST_DESTRUCTION:
        return "SCOREBOARD_RANKED_DESTRUCTION";
    default:
        return "UNKNOWN RANKED SCOREBOARD";
    }
}

// 0x0094A4E0 (:529)
void scoreboard_request::set(const scoreboard_info* infop, int offset, int count, unsigned char* scratchp)
{
    info = infop;
    request_type = SRT_DEFAULT;
    player_uids = nullptr;
    rank_offset = offset;
    entries_to_retrieve = count;
    scratch = scratchp;
}

// 0x0094A510 (:543)
void scoreboard_request::set_uids(const scoreboard_info* infop, multi_player_uid* uid_list, int count, unsigned char* scratchp)
{
    info = infop;
    player_uids = uid_list;
    request_type = SRT_PLAYERS;
    rank_offset = 0;
    entries_to_retrieve = count;
    scratch = scratchp;
}

// 0x00972A60 (:1079): original behaviour: not checked against Scoreboard_system_initd (unlike the others)
void multi_core_platform_scoreboard_retrieve_data(scoreboard_results* results, const scoreboard_request* request)
{
    multi_core_platform_internal_scoreboard_retrieve_data(results, request);
}

// 0x00972A70 (:1085)
platform_op_status multi_core_platform_scoreboard_retrieve_state()
{
    if (!Scoreboard_system_initd)
    {
        return POS_FAILED;
    }
    return multi_core_platform_internal_scoreboard_retrieve_state();
}

// 0x0094AAE0 (:1091)
void multi_core_platform_scoreboard_cancel_retrieve()
{
    if (Scoreboard_system_initd)
    {
        multi_core_platform_internal_scoreboard_cancel_retrieve();
    }
}

// 0x0094AB00 (:1097)
bool multi_core_platform_scoreboard_begin_upload_score(const multi_player_uid& player_uid, unsigned short table_id, scoreboard_column* scores, unsigned int num_scores)
{
    if (!Scoreboard_system_initd)
    {
        return false;
    }
    return multi_core_platform_internal_scoreboard_begin_upload_score(player_uid, table_id, scores, num_scores);
}

// 0x0094AB20 (:1105)
void multi_core_platform_update_achievement_progress(int achievement_number, unsigned int progress)
{
    multi_core_platform_internal_update_achievement_progress(achievement_number, progress);
}

// 0x0094AB60 (:1110)
void multi_core_platform_award_achievement(int achievement_number)
{
    multi_core_platform_internal_award_achievement(achievement_number);
}

// 0x0094ABA0 (:1125)
void multi_core_platform_discord_ready(const DiscordUser*)
{
}

// 0x0094ABB0 (:1129)
void multi_core_platform_discord_disconnected(int errorCode, const char* message)
{
    keen::debug::nativePrintArgs("[discord] Disconnected from Discord. Error code 0x%x: %s\n", errorCode, message);
}

// 0x0094ABD0 (:1134)
void multi_core_platform_discord_errored(int errorCode, const char* message)
{
    keen::debug::nativePrintArgs("[discord] Discord error. Error code 0x%x: %s\n", errorCode, message);
}

// Discord join and spectate secrets are the Steam lobby id in hexadecimal.
static void discord_set_invitation(const char* secret, bool spectate)
{
    uint64_t lobbyId;
    if (keen::readUint64FromHexadecimalString(&lobbyId, &secret) == keen::StringReadResult_Ok)
    {
        kaiko::GameLinkInvitationData invitation;
        invitation.lobbyId.SetFromUint64(lobbyId);
        invitation.friendId.SetFromUint64(0);
        kaiko::gamelink::setInvitationData(&invitation);
        Spectating = spectate;
    }
}

// 0x00972A90 (:1139)
void multi_core_platform_discord_join_game(const char* joinSecret)
{
    discord_set_invitation(joinSecret, false);
}

// 0x00972B00 (:1153)
void multi_core_platform_discord_spectate_game(const char* spectateSecret)
{
    discord_set_invitation(spectateSecret, true);
}

// 0x0094ABF0 (:1167): every join request is accepted
void multi_core_platform_discord_join_request(const DiscordUser* request)
{
    Discord_Respond(request->userId, DISCORD_REPLY_YES);
}
