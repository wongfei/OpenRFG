// Original: volition/rfg/code/multi/multi_game/multi_game_party.cpp
//
// Parties: players who go from lobby to lobby behind a leader, on GameLink's party (a Steam lobby; the LAN link has
// none). The leader sends the members low level messages with its game lobby and playlist whenever they change; a
// member asks for them when the leader or the party lobby changes, every 2 s while it waits, and follows the leader
// into the lobby.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi_server_browser.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_live.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/os/timer.h"
#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/mutex.hpp"
#include "keen/base/stringencoding.hpp"
#include "unit4/gamesession/remotegamesession.hpp"

#include <string.h>

// ---- globals
bool g_joinWithoutReservation; // 0x02FEB700

// ---- file-static globals
static int Last_received_lobby_update = -1; // 0x0167F288
static multi_session_info Current_party_lobby; // 0x01693278
static const kaiko::GameLinkRemoteStation* Last_player_list[32]; // 0x02FE5B20
static unit4::NamedRemoteUserId s_last_party_owner; // 0x02FE5C18
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
static kaiko::GameLinkLobby s_last_gamelink_lobby; // 0x02FE8DE0
static kaiko::GameLinkLobby Last_sent_lobby_data; // 0x02FE8DE8
static connection_address Last_update_party_host; // 0x02FE906C
static unsigned int Last_num_players; // 0x02FEB344
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
static bool Left_party_by_choice; // 0x02FEB702
static int Last_requested_updated_time; // 0x02FEB708
// bool Multi_kicked_from_party (0x02FEB703) is used here too; placed in multi_game.cpp.
// preferences_members* Preferences (0x02FEB70C) is used here too; placed in multi_core_session_generic.cpp.
// pending_invite_info Pending_invite (0x02FEBBD8) is used here too; placed in multi_core_platform_generic.cpp.

// 0x0095F2A0 (:109)
void multi_game_party_system_init()
{
    Multi_kicked_from_party = false;
    Multi_partied = false;
}

// 0x0095F2B0 (:116): empty in rfg.exe
void multi_game_party_system_shutdown()
{
}

// 0x009CDF60 (:121): LLPT_PARTY_LOBBY_DATA_UPDATE: the update number and the playlist (32 bits big endian each), then
// the game lobby's join info. LTCG: pStation in ECX, pGameLink on the stack.
// Original bug kept: the playlist is not checked for null (rfg.exe inlines the lookup four times, one per byte).
static void multi_game_party_send_lobby_data_update(kaiko::gamelink::GameLink* pGameLink, const kaiko::GameLinkRemoteStation* pStation)
{
    if (pStation->myStation || !multi_game_party_is_remote_station_in_my_party(pStation))
    {
        return;
    }
    multi_session_info info;
    info.copy(*Lobby_session->get_info());
    info.invited = true;
    unsigned char packet[1200];
    packet[0] = (unsigned char)((unsigned int)Last_received_lobby_update >> 24);
    packet[1] = (unsigned char)((unsigned int)Last_received_lobby_update >> 16);
    packet[2] = (unsigned char)((unsigned int)Last_received_lobby_update >> 8);
    packet[3] = (unsigned char)Last_received_lobby_update;
    const unsigned int playlist_id = multi_game_playlist_get_by_id(Preferences->ideal_match.m_playlist_id)->playlist_id.get_crc();
    packet[4] = (unsigned char)(playlist_id >> 24);
    packet[5] = (unsigned char)(playlist_id >> 16);
    packet[6] = (unsigned char)(playlist_id >> 8);
    packet[7] = (unsigned char)playlist_id;
    unsigned short size = 8; // Magic number: the two words above
    info.append_join_info_to_packet(packet, size);
    connection_address address;
    address.set(pStation->id);
    multi_io_send_low_level_message_via_game_link(pGameLink, LLPT_PARTY_LOBBY_DATA_UPDATE, packet, size, address);
}

// 0x00981FE0 (:154): to the party's owner. LTCG: pGameLink in ESI.
static void multi_game_party_request_lobby_data_update(kaiko::gamelink::GameLink* pGameLink)
{
    g_gameLinkMutex.lock(0);
    unit4::NamedRemoteUserId owner;
    if (pGameLink->getPartyOwner(&owner))
    {
        connection_address address;
        address.set(owner.id);
        const unsigned char packet[4] = {0, 0, 0, 0};
        multi_io_send_low_level_message_via_game_link(pGameLink, LLPT_PARTY_LOBBY_REQUEST_UPDATE, packet, sizeof(packet), address);
    }
    g_gameLinkMutex.unlock();
}

// 0x009CE1A0 (:175): the leader, each frame: to everybody when the lobby or its members changed.
// Original bug kept: a changed lobby sends to the last known member list without refreshing it.
static void multi_game_party_check_send_lobby_data_update(kaiko::gamelink::GameLink* pGameLink, const kaiko::GameLinkLobby& currentLobby)
{
    if (!Last_sent_lobby_data.is_equal(currentLobby))
    {
        Last_received_lobby_update++;
    }
    else
    {
        const kaiko::GameLinkRemoteStation* members[32];
        const unsigned int count = pGameLink->getLobbyMembers(members, 32);
        if (count == Last_num_players)
        {
            unsigned int i = 0;
            while (i < count && members[i]->id.is_equal(Last_player_list[i]->id))
            {
                i++;
            }
            if (i == count)
            {
                return;
            }
        }
        memcpy(Last_player_list, members, count * sizeof(members[0]));
        Last_num_players = count;
    }
    for (unsigned int i = 0; i < Last_num_players; i++)
    {
        const kaiko::GameLinkRemoteStation* station = Last_player_list[i];
        const kaiko::GameLinkRemoteStation* owner = pGameLink->getLobbyOwner();
        if (owner == nullptr || !station->id.is_equal(owner->id))
        {
            multi_game_party_send_lobby_data_update(pGameLink, station);
        }
    }
    Last_sent_lobby_data = currentLobby;
}

// 0x009B1840 (:228): a member takes the leader's update: an older number from the same host is ignored (numbers of a
// different sign count as newer), an unknown playlist leaves the party
void multi_game_party_lobby_data_update_received(const unsigned char* packet_data, unsigned short data_size, const connection_address& sender)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled() && !link->isPartyOwner())
    {
        const int update = (int)(((unsigned int)packet_data[0] << 24) | ((unsigned int)packet_data[1] << 16) | ((unsigned int)packet_data[2] << 8) | packet_data[3]);
        multi_player_uid leader;
        if (multi_game_party_get_leader(&leader) && sender.connection_id.is_equal(leader.get_internal_id()))
        {
            const bool same_sign = (update < 0) == (Last_received_lobby_update < 0);
            if (update >= Last_received_lobby_update || !same_sign || !Last_update_party_host.connection_id.is_equal(sender.connection_id))
            {
                Last_received_lobby_update = update;
                Last_update_party_host.set(leader.get_internal_id());
                const unsigned int playlist_id = ((unsigned int)packet_data[4] << 24) | ((unsigned int)packet_data[5] << 16) | ((unsigned int)packet_data[6] << 8) | packet_data[7];
                const multi_game_playlist* playlist = multi_game_playlist_get_by_id(playlist_id);
                if (playlist == nullptr)
                {
                    multi_game_party_leave_party(false);
                }
                else
                {
                    game_query query;
                    query.game_query_base::copy(Preferences->ideal_match);
                    query.m_game_type = playlist->game_type;
                    query.m_query_type = playlist->m_query_type;
                    query.m_game_mode = playlist->mode_type;
                    query.m_playlist_id = playlist->playlist_id.get_crc();
                    Preferences->ideal_match.copy(query);
                    multi_game_preferences_set_current_query(&query);
                    int offset = 8; // Magic number: the two words above
                    Current_party_lobby.read_join_info_from_packet(packet_data, offset);
                }
            }
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x009CE2A0 (:296)
void multi_game_party_lobby_data_update_request_received(const unsigned char* packet_data, unsigned short data_size, const connection_address& sender)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled() && link->isPartyOwner())
    {
        const kaiko::GameLinkRemoteStation* station = link->getRemoteStationById(sender.connection_id);
        if (station != nullptr)
        {
            multi_game_party_send_lobby_data_update(link, station);
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x00A04CD0 (:318): a pending invite first, then the leader's sends or the member following the leader's lobby.
// Original bug kept: Lobby_session is read before its null check further down.
void multi_game_party_system_process()
{
    if (Migration_state == MIGRATION_STATE_NONE && Lobby_session->get_state() != MSS_MIGRATING && gameseq_get_state() != GS_LOAD && Pending_invite.pending)
    {
        ui_hide_msgbox(-2, true);
        if (Multi_syslink)
        {
            game_set_multi_shutdown(GS_MAINMENU);
        }
        else
        {
            pending_invite_info invite = Pending_invite;
            if (ui_is_msgbox_active(-2))
            {
                ui_hide_msgbox(-2, true);
            }
            if (multi_game_party_join_party(&invite, false, true))
            {
                multi_core_platform_clear_pending_invite();
            }
        }
    }
    if (Multi_syslink)
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link == nullptr || !link->isPartyEnabled() || Lobby_session == nullptr || Pending_invite.pending)
    {
        g_gameLinkMutex.unlock();
        return;
    }
    unit4::NamedRemoteUserId owner;
    if (link->getPartyOwner(&owner) && !owner.id.is_equal(s_last_party_owner.id))
    {
        s_last_party_owner = owner;
        if (!link->isPartyOwner())
        {
            multi_game_party_reset_last_received_lobby_data_update();
            multi_game_party_request_lobby_data_update(link);
        }
    }
    const kaiko::GameLinkLobby party_lobby = link->getLobbyId();
    if (!party_lobby.is_equal(s_last_gamelink_lobby))
    {
        multi_game_party_reset_last_received_lobby_data_update();
        s_last_gamelink_lobby = party_lobby;
    }
    if (link->getCurrentState() != kaiko::GameLinkState_InLobby)
    {
        g_gameLinkMutex.unlock();
        return;
    }
    const kaiko::GameLinkLobby game_lobby = static_cast<const multi_session_info*>(Lobby_session->get_info())->m_lobbyId;
    if (multi_game_party_am_i_leader(false))
    {
        multi_game_party_check_send_lobby_data_update(link, party_lobby);
    }
    else if (party_lobby.id.asUint64 != 0)
    {
        if (party_lobby.is_equal(game_lobby))
        {
            Last_requested_updated_time = 0;
        }
        else
        {
            const multi_session_state state = Lobby_session->get_state();
            if (party_lobby.is_equal(Current_party_lobby.m_lobbyId) && Current_party_lobby.m_lobbyId.id.asUint64 != 0 && (state == MSS_NONE || state == MSS_READY))
            {
                g_joinedGameLobbyViaInvitation = false;
                const bool reserved_slot = !Current_party_lobby.party_lobby && !g_joinWithoutReservation;
                multi_game_lobby_join(&Current_party_lobby, nullptr, reserved_slot, false);
                Current_party_lobby.init();
                g_joinWithoutReservation = false;
            }
            else
            {
                const int now = timer_get(TIMER_MILLISECONDS);
                if (now - Last_requested_updated_time >= 2000)
                {
                    multi_game_party_request_lobby_data_update(link);
                    Last_requested_updated_time = now;
                }
            }
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x0099FE90 (:449)
void multi_game_party_enable_party()
{
    if (Live_server_browser.isInCustomMatch() || g_joinedGameLobbyViaInvitation)
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && !link->isPartyEnabled())
    {
        multi_game_party_reset_last_received_lobby_data_update();
        link->setPartyEnabled(true);
    }
    g_gameLinkMutex.unlock();
}

// 0x0099FEF0 (:472)
void multi_game_party_disable_party()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr)
    {
        multi_game_party_reset_last_received_lobby_data_update();
        link->setPartyEnabled(false);
    }
    g_gameLinkMutex.unlock();
}

// 0x00A00AB0 (:492): an invite with a friend id is to the friend's party, one without is to a game lobby (which the
// menus must be at; the other menus go there first and answer false, keeping the invite pending).
// Original bug kept: with no playlists the game lobby path reads a null playlist.
bool multi_game_party_join_party(const pending_invite_info* pending_party_invite_info, bool invited_from_game, bool force_destruction)
{
    if (force_destruction)
    {
        if (multi_game_party_in_party())
        {
            multi_game_party_leave_party(true);
        }
        if (Lobby_session->get_state() != MSS_NONE)
        {
            multi_game_lobby_leave(true);
        }
    }
    if (pending_party_invite_info->invitation_data.friendId.IsValid())
    {
        if (!invited_from_game)
        {
            const game_state state = gameseq_get_state();
            if (state == GS_MAINMENU)
            {
                return false;
            }
            if (state == GS_MULTIPLAYER_LIVE)
            {
                ui_document_mp_live_ui_push(GS_MULTIPLAYER_PARTY_LOBBY);
                return false;
            }
            if (state != GS_MULTIPLAYER_PARTY_LOBBY)
            {
                gameseq_set_state(GS_MAINMENU, false);
                return false;
            }
        }
    }
    else
    {
        g_joinedGameLobbyViaInvitation = true;
        const game_state state = gameseq_get_state();
        if (state == GS_MAINMENU)
        {
            return false;
        }
        if (state == GS_MULTIPLAYER_LIVE)
        {
            ui_document_mp_live_ui_push(GS_MULTIPLAYER_GAME_LOBBY);
            return false;
        }
        if (state != GS_MULTIPLAYER_GAME_LOBBY)
        {
            gameseq_set_state(GS_MAINMENU, false);
            return false;
        }
        multi_session_info info;
        memcpy(&info.m_lobbyId, &pending_party_invite_info->invitation_data.lobbyId, sizeof(info.m_lobbyId));
        info.max_slots = 0;
        info.invite_type = SIT_ALL;
        info.party_lobby = false;
        info.private_match = false;
        info.type = ST_GAME;
        // The first quick match playlist, else the first one
        const multi_game_playlist* playlist = Playlists;
        for (const multi_game_playlist* p = Playlists; p != nullptr; p = p->next)
        {
            if (p->m_query_type == QT_QUICKMATCH)
            {
                playlist = p;
                break;
            }
            if (p->next == Playlists)
            {
                break;
            }
        }
        game_query query;
        query.game_query_base::copy(Preferences->ideal_match);
        query.m_game_type = playlist->game_type;
        query.m_query_type = playlist->m_query_type;
        query.m_game_mode = playlist->mode_type;
        query.m_playlist_id = playlist->playlist_id.get_crc();
        Preferences->ideal_match.copy(query);
        Preferences->current_match.copy(query);
        multi_game_lobby_join(&info, nullptr, false, false);
    }
    if (Current_game_type_context != PGTC_PUBLIC)
    {
        Current_game_type_context = PGTC_PUBLIC;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link == nullptr)
    {
        g_gameLinkMutex.unlock();
        return false;
    }
    const bool joining = link->joinLobby(&pending_party_invite_info->invitation_data);
    g_gameLinkMutex.unlock();
    return joining;
}

// 0x00A00D00 (:598)
void multi_game_party_process_kicked(unsigned char* packet_data, const sender& from)
{
    multi_game_party_leave_party(false);
    if (Lobby_session != nullptr && Lobby_session->get_state() != MSS_NONE && Lobby_session->get_info()->party_lobby)
    {
        multi_game_lobby_leave(false);
    }
    Multi_kicked_from_party = true;
    game_boot_to_main_menu(BC_KICKED_FROM_LOBBY);
}

// 0x0099FF70 (:613)
void multi_game_party_leave_party(bool left_by_choice)
{
    Left_party_by_choice = left_by_choice;
    party_lobby_reset_party_state();
    if (!Left_party_by_choice && gameseq_state_is_active(GS_MULTIPLAYER_PARTY_LOBBY))
    {
        ui_add_msgbox(MBT_OK, "", "MENU_YOU_ARE_LEADER", nullptr, false, false, nullptr, nullptr, false);
    }
    multi_game_party_disable_party();
}

// 0x00982070 (:640). Original bug kept: the memset also clears Current_party_lobby's vtable pointer.
void multi_game_party_reset_last_received_lobby_data_update()
{
    memset((void*)&Current_party_lobby, 0, sizeof(Current_party_lobby));
    Last_received_lobby_update = -1;
    memset((void*)&Last_update_party_host, 0, sizeof(Last_update_party_host));
}

// 0x009CE320 (:650)
void multi_game_party_kick(const multi_player_uid& uid)
{
    if (!multi_game_party_am_i_leader(true))
    {
        return;
    }
    multi_connection* cp = Lobby_session->get_player_connection(uid);
    if (cp != nullptr)
    {
        unsigned char packet[1200];
        packet[0] = PT_PARTY_KICKED;
        const unsigned short size = Multi_packets[PT_PARTY_KICKED].group_id != 0 ? 5 : 3;
        const unsigned short payload = (unsigned short)(size - 3);
        packet[1] = (unsigned char)(payload >> 8);
        packet[2] = (unsigned char)payload;
        multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
    }
}

// 0x0095F2C0 (:665): without a link, the answer is party_must_exist (rfg.exe)
bool multi_game_party_am_i_leader(bool party_must_exist)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link == nullptr)
    {
        g_gameLinkMutex.unlock();
        return party_must_exist;
    }
    const bool leader = link->isPartyEnabled() && link->isPartyOwner();
    g_gameLinkMutex.unlock();
    return leader;
}

// 0x0095F330 (:684)
bool multi_game_party_left_party_by_choice()
{
    return Left_party_by_choice;
}

// 0x0095F340 (:689)
void multi_game_party_clear_left_party_by_choice()
{
    Left_party_by_choice = false;
}

// 0x0095F350 (:694)
bool multi_game_party_in_party()
{
    if (Pending_invite.pending)
    {
        return false;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr)
    {
        unit4::NamedRemoteUserId owner;
        if (link->getPartyOwner(&owner))
        {
            const bool enabled = link->isPartyEnabled();
            g_gameLinkMutex.unlock();
            return enabled;
        }
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x0095F3F0 (:727): multi_game_data_get inlined
bool multi_game_party_all_clients_have_playlist(const multi_connection* const cp_list, const multi_game_playlist* const playlist)
{
    const multi_connection* cp = cp_list;
    while (cp != nullptr)
    {
        const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
        if (data != nullptr && ((data->dlc_mask & playlist->required_dlc_flags) != playlist->required_dlc_flags || (playlist->requires_bagman && !data->bagman_dlc_unlocked)))
        {
            return false;
        }
        cp = cp->next;
        if (cp == cp_list)
        {
            break;
        }
    }
    return true;
}

// 0x0095F490 (:749)
unsigned int multi_game_party_get_num_players_in_party()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled())
    {
        const unsigned int count = link->getNumPartyMembers();
        g_gameLinkMutex.unlock();
        return count;
    }
    g_gameLinkMutex.unlock();
    return 0;
}

// 0x0095F4F0 (:767)
bool multi_game_party_has_player(const multi_player_uid& uid)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled())
    {
        unit4::NamedRemoteUserId members[32];
        const unsigned int count = link->getPartyMembers(members, 32);
        for (unsigned int i = 0; i < count; i++)
        {
            if (members[i].id.is_equal(uid.get_internal_id()))
            {
                g_gameLinkMutex.unlock();
                return true;
            }
        }
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x009820B0 (:794)
bool multi_game_party_get_player_name(const multi_player_uid& uid, wchar_t* out_name, unsigned int name_capacity)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled())
    {
        unit4::NamedRemoteUserId members[32];
        const unsigned int count = link->getPartyMembers(members, 32);
        for (unsigned int i = 0; i < count; i++)
        {
            if (members[i].id.is_equal(uid.get_internal_id()))
            {
                char short_name[257]; // Magic number: 0x101, rfg.exe's capacity
                rfg_shorten_display_string(members[i].displayName, short_name, sizeof(short_name), 32, true);
                keen::convertUTF8ToUTF16((uint16*)out_name, name_capacity, short_name);
                g_gameLinkMutex.unlock();
                return true;
            }
        }
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x0095F5A0 (:824)
bool multi_game_party_is_player_leader(const multi_player_uid& uid)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled())
    {
        unit4::NamedRemoteUserId owner;
        if (link->getPartyOwner(&owner) && owner.id.is_equal(uid.get_internal_id()))
        {
            g_gameLinkMutex.unlock();
            return true;
        }
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x0095F640 (:847)
bool multi_game_party_get_leader(multi_player_uid* uid)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled())
    {
        unit4::NamedRemoteUserId owner;
        if (link->getPartyOwner(&owner))
        {
            uid->set(owner.id, true);
            g_gameLinkMutex.unlock();
            return true;
        }
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x009821B0 (:873)
unsigned int multi_game_party_get_players(multi_player_uid* out_uids, unsigned int capacity)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->isPartyEnabled())
    {
        unit4::NamedRemoteUserId members[32];
        unsigned int count = link->getPartyMembers(members, 32);
        if (count > capacity)
        {
            count = capacity;
        }
        for (unsigned int i = 0; i < count; i++)
        {
            out_uids[i].set(members[i].id, true);
        }
        g_gameLinkMutex.unlock();
        return count;
    }
    g_gameLinkMutex.unlock();
    return 0;
}

// 0x0095F6E0 (:904): always in rfg.exe
bool multi_game_party_is_locked_down()
{
    return true;
}

// 0x0095F6F0 (:916): empty in rfg.exe
void multi_game_party_lockdown()
{
}

// 0x0095F700 (:924): empty in rfg.exe
void multi_game_party_unlock()
{
}

// 0x00982290 (:935): every party member has a connection in the lobby session
bool multi_game_party_all_players_joined_lobby()
{
    if (Lobby_session == nullptr)
    {
        return false;
    }
    multi_player_uid players[32];
    const unsigned int count = multi_game_party_get_players(players, 32);
    for (unsigned int i = 0; i < count; i++)
    {
        multi_connection* const first = Lobby_session->get_connection_list();
        multi_connection* cp = first;
        bool found = false;
        while (cp != nullptr)
        {
            if (cp->player_uid.get_internal_id().is_equal(players[i].get_internal_id()))
            {
                found = true;
                break;
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

// 0x0095F710 (:955): compares the provider data only (memcmp inlined), at most 16 members
bool multi_game_party_is_remote_station_in_my_party(const kaiko::GameLinkRemoteStation* pRemoteStation)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link == nullptr)
    {
        g_gameLinkMutex.unlock();
        return false;
    }
    unit4::NamedRemoteUserId members[16];
    const unsigned int count = link->getPartyMembers(members, 16);
    for (unsigned int i = 0; i < count; i++)
    {
        if (memcmp(members[i].id.providerData, pRemoteStation->id.providerData, sizeof(members[i].id.providerData)) == 0)
        {
            g_gameLinkMutex.unlock();
            return true;
        }
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x0095F830 (:979): SIT_ALL..SIT_PRIVATE, wrapping
session_invite_type multi_game_party_next_invite_type(session_invite_type invite_type)
{
    const int next = (int)invite_type + 1;
    return next > 3 ? SIT_ALL : (session_invite_type)next;
}

// 0x0095F840 (:988)
session_invite_type multi_game_party_prev_invite_type(session_invite_type invite_type)
{
    const int prev = (int)invite_type - 1;
    return prev < 1 ? SIT_PRIVATE : (session_invite_type)prev;
}
