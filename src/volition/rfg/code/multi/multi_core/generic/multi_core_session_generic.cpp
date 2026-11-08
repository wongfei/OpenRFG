// Original: volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.cpp
//
// The generic session over a GameLink lobby (LAN or Steam): the session id is the GameLink lobby id; creating a
// session creates a lobby (on the internet with the build tag, modified table hash and match filters), joining joins
// it, the lobby owner is the host, and a host migration follows the GameLink lobby owner. Leaving says goodbye to the
// host ten times (1 ms apart) before leaving the lobby.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/scheduler.h"
#include "library/modules/runtime/base/include/keen/base/mutex.hpp"
#include "library/modules/runtime/base/include/keen/base/systemtimer.hpp"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"
#include "library/modules/runtime/gamesession/include/unit4/gamesession/localgamesession.hpp"
#include "library/modules/runtime/gamesession/include/unit4/gamesession/remotegamesession.hpp"

// ---- file-private types (rfg.pdb)
// size 0x588
struct preferences_members
{
    preferences_members(); // 0x009601D0

    game_query ideal_match; // 0x00
    game_query current_match; // 0x1C
    multi_game_preferences_history my_history; // 0x38
    multi_game_preferences_history total_history[16]; // 0x88
};

// ---- globals
bool g_joinedGameLobbyViaInvitation; // 0x02FEB701

// ---- file-static globals
// Magic number: the goodbyes sent to the host when leaving (1 ms apart)
static int s_numGoodByePackets = 10; // 0x0167D7F8
// unsigned int s_modTableHash (0x02FEB56C) is used here too; placed in multi_core_matchmaking_generic.cpp.
static unsigned int s_timeHostMigrationStarted; // 0x02FEB57C
static unsigned int s_timeBecameHost; // 0x02FEB580
static unsigned int s_timeSessionEntered; // 0x02FEB584
static bool Server; // 0x02FEB590
static preferences_members* Preferences; // 0x02FEB70C

// The lobby filter words (Lobby_filter_count, Lobby_tag, ...; the host sets the match state at start and end of a
// match) are defined in multi_core_matchmaking_generic.cpp, earlier in the unity TU.

// ---- multi_session_info

// 0x009B4A90 (:76)
multi_session_info::multi_session_info()
{
    m_lobbyId.id.asUint64 = 0;
}

// 0x009A7FB0 (:82)
multi_session_info::multi_session_info(const multi_session_info& src)
    : multi_session_info_base(src)
{
    multi_session_info_base::copy(src);
    m_lobbyId = src.m_lobbyId;
}

// 0x009A8000 (:88): the source is taken to be a multi_session_info (its lobby id is read)
multi_session_info::multi_session_info(const multi_session_info_base& src)
    : multi_session_info_base(src)
{
    multi_session_info_base::copy(src);
    m_lobbyId = static_cast<const multi_session_info&>(src).m_lobbyId;
}

// 0x009A8030 (:93)
void multi_session_info::init()
{
    multi_session_info_base::init();
    m_lobbyId.id.asUint64 = 0;
}

// 0x0098E380 (:99): the source is taken to be a multi_session_info (its lobby id is read)
void multi_session_info::copy(const multi_session_info_base& src)
{
    multi_session_info_base::copy(src);
    m_lobbyId = static_cast<const multi_session_info&>(src).m_lobbyId;
}

// 0x009425A0 (:107): 0 when the lobbies match, -1 otherwise
int multi_session_info::compare_by_id(const multi_session_info_base& comparand) const
{
    return (int)m_lobbyId.is_equal(static_cast<const multi_session_info&>(comparand).m_lobbyId) - 1;
}

// 0x00970F20 (:117)
void multi_session_info::append_migration_info_to_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    host_player_uid.write_to_packet(packet_data, packet_size);
}

// 0x00970F30 (:123)
void multi_session_info::read_migration_info_from_packet(unsigned char* packet_data, int& packet_offset)
{
    host_player_uid.read_from_packet(packet_data, packet_offset);
}

// 0x009425C0 (:129): two flag bytes, the invite type, the slot count
void multi_session_info::append_modification_info_to_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    packet_data[packet_size] = (unsigned char)((party_lobby ? 2 : 0) | (allow_join_in_progress ? 1 : 0));
    packet_data[packet_size + 1] = (unsigned char)((dedicated ? 0x80 : 0) | (systemlink_or_lan ? 0x40 : 0) | (advertise_for_matchmaking ? 0x20 : 0) |
                                                   (reports_stats ? 0x10 : 0) | (uses_abitration ? 8 : 0) | (uses_peer_to_peer_networking ? 4 : 0) |
                                                   (uses_presence ? 2 : 0) | (private_match ? 1 : 0));
    packet_size = (unsigned short)(packet_size + 2);
    packet_data[packet_size++] = (unsigned char)invite_type;
    packet_data[packet_size++] = (unsigned char)max_slots;
}

// 0x00942690 (:152)
void multi_session_info::read_modification_info_from_packet(unsigned char* packet_data, int& packet_offset)
{
    const unsigned char flags0 = packet_data[packet_offset];
    const unsigned char flags1 = packet_data[packet_offset + 1];
    packet_offset += 2;
    private_match = (flags1 & 1) != 0;
    uses_presence = (flags1 >> 1 & 1) != 0;
    uses_peer_to_peer_networking = (flags1 >> 2 & 1) != 0;
    uses_abitration = (flags1 >> 3 & 1) != 0;
    reports_stats = (flags1 >> 4 & 1) != 0;
    advertise_for_matchmaking = (flags1 >> 5 & 1) != 0;
    dedicated = (flags1 >> 7) != 0;
    systemlink_or_lan = (flags1 >> 6 & 1) != 0;
    allow_join_in_progress = (flags0 & 1) != 0;
    party_lobby = (flags0 >> 1 & 1) != 0;
    invite_type = (session_invite_type)packet_data[packet_offset++];
    max_slots = packet_data[packet_offset++];
}

// 0x00970F40 (:175): the lobby id, the host uid, two flag bytes, the invite type, the session type, the slot count
void multi_session_info::append_join_info_to_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    append_id_to_packet(packet_data, packet_size);
    host_player_uid.write_to_packet(packet_data, packet_size);
    packet_data[packet_size] = (unsigned char)((party_lobby ? 4 : 0) | (invited ? 2 : 0) | (allow_join_in_progress ? 1 : 0));
    packet_data[packet_size + 1] = (unsigned char)((dedicated ? 0x80 : 0) | (systemlink_or_lan ? 0x40 : 0) | (advertise_for_matchmaking ? 0x20 : 0) |
                                                   (reports_stats ? 0x10 : 0) | (uses_abitration ? 8 : 0) | (uses_peer_to_peer_networking ? 4 : 0) |
                                                   (uses_presence ? 2 : 0) | (private_match ? 1 : 0));
    packet_size = (unsigned short)(packet_size + 2);
    packet_data[packet_size++] = (unsigned char)invite_type;
    packet_data[packet_size++] = (unsigned char)type;
    packet_data[packet_size++] = (unsigned char)max_slots;
}

// 0x009710B0 (:207)
void multi_session_info::read_join_info_from_packet(const unsigned char* packet_data, int& packet_offset)
{
    read_id_from_packet(const_cast<unsigned char*>(packet_data), packet_offset);
    host_player_uid.read_from_packet(packet_data, packet_offset);
    const unsigned char flags0 = packet_data[packet_offset];
    const unsigned char flags1 = packet_data[packet_offset + 1];
    packet_offset += 2;
    private_match = (flags1 & 1) != 0;
    uses_presence = (flags1 >> 1 & 1) != 0;
    uses_peer_to_peer_networking = (flags1 >> 2 & 1) != 0;
    uses_abitration = (flags1 >> 3 & 1) != 0;
    reports_stats = (flags1 >> 4 & 1) != 0;
    advertise_for_matchmaking = (flags1 >> 5 & 1) != 0;
    dedicated = (flags1 >> 7) != 0;
    allow_join_in_progress = (flags0 & 1) != 0;
    invited = (flags0 >> 1 & 1) != 0;
    party_lobby = (flags0 >> 2 & 1) != 0;
    systemlink_or_lan = (flags1 >> 6 & 1) != 0;
    invite_type = (session_invite_type)packet_data[packet_offset++];
    type = (session_types)packet_data[packet_offset++];
    max_slots = packet_data[packet_offset++];
}

// 0x00942760 (:239): the 8 bytes of the lobby id (in memory order)
void multi_session_info::append_id_to_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    const unsigned char* id = (const unsigned char*)&m_lobbyId.id;
    for (unsigned int i = 0; i < sizeof(m_lobbyId.id); i++)
    {
        packet_data[packet_size++] = id[i];
    }
}

// 0x009427E0 (:244)
void multi_session_info::read_id_from_packet(unsigned char* packet_data, int& packet_offset)
{
    unsigned char* id = (unsigned char*)&m_lobbyId.id;
    for (unsigned int i = 0; i < sizeof(m_lobbyId.id); i++)
    {
        id[i] = packet_data[packet_offset++];
    }
}

// 0x00942850 (:249): no arbitration in the generic session
void multi_session_info::append_arbitration_info_to_packet(unsigned char*, unsigned short&) const
{
}

// 0x00942860 (:255)
void multi_session_info::read_arbitration_info_from_packet(unsigned char*, int&)
{
}

// ---- multi_session

// 0x009B4AE0 (:266)
multi_session::multi_session()
    : multi_session_base(&m_current_info, &m_desired_info)
{
}

// 0x00971270 (:270)
multi_session::~multi_session()
{
}

// 0x0098E3A0 (:274): create the GameLink lobby. The lobby type follows the invite type (a custom game party lobby
// follows the lobby's invite type); a public internet game lobby carries the matchmaking filters, the others the
// build tag and table hash; a LAN lobby has no filters.
session_op_status multi_session::platform_session_create()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    kaiko::GameLinkLobbyType lobby_type = kaiko::GameLinkLobbyType_Public;
    switch (m_desired_info.invite_type)
    {
    case SIT_NONE:
    case SIT_PRIVATE:
        lobby_type = kaiko::GameLinkLobbyType_Private;
        break;
    case SIT_FRIENDS:
        lobby_type = kaiko::GameLinkLobbyType_Friends;
        break;
    default:
        break;
    }
    const bool party_lobby = m_desired_info.party_lobby;
    if (party_lobby)
    {
        const session_invite_type lobby_invite_type = Lobby_session->get_party_lobby_invite_type();
        if (Party_lobby_type == MPPLT_CUSTOM_GAME && lobby_invite_type == SIT_ALL)
        {
            lobby_type = kaiko::GameLinkLobbyType_Public;
        }
        else if (Party_lobby_type == MPPLT_CUSTOM_GAME && lobby_invite_type == SIT_FRIENDS)
        {
            lobby_type = kaiko::GameLinkLobbyType_Friends;
        }
        else
        {
            lobby_type = kaiko::GameLinkLobbyType_Private;
        }
    }
    const unsigned int max_members = (unsigned int)m_desired_info.max_slots;
    bool created = false;
    if (m_desired_info.systemlink_or_lan)
    {
        created = link->createLobby(lobby_type, max_members, nullptr, 0);
    }
    else if (lobby_type == kaiko::GameLinkLobbyType_Public && !party_lobby)
    {
        const game_query* search_query = &Preferences->current_match;
        const multi_game_playlist* playlist = multi_game_playlist_get_by_id(Preferences->current_match.m_playlist_id);
        if (playlist != nullptr)
        {
            unsigned int filters[Lobby_filter_count];
            multi_core_matchmake_internal_init_filters(filters, search_query, false, playlist->max_party_size, MatchInProgressFilter_NotInProgress);
            created = link->createLobby(kaiko::GameLinkLobbyType_Public, max_members, filters, Lobby_filter_count);
        }
    }
    else
    {
        unsigned int filters[Lobby_filter_count];
        filters[0] = multi_core_matchmake_internal_is_my_user_banned() ? Banned_lobby_tag : Lobby_tag;
        filters[1] = s_modTableHash;
        filters[2] = Custom_game_lobby_marker;
        filters[3] = 0;
        filters[4] = 0;
        filters[5] = 0;
        filters[6] = 0;
        filters[Lobby_filter_match_state] = Match_state_not_in_progress;
        created = link->createLobby(lobby_type, max_members, filters, Lobby_filter_count);
    }
    g_gameLinkMutex.unlock();
    // The create completes in platform_process_operation once the lobby is entered.
    return created ? SOS_IN_PROGRESS : SOS_FAILED;
}

// 0x00942870 (:380): join the desired lobby (a party member who is not the party owner follows the owner instead)
session_op_status multi_session::platform_session_join()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (!link->isPartyEnabled() || link->isPartyOwner())
    {
        const kaiko::GameLinkLobby& lobby = m_desired_info.m_lobbyId;
        // A failed join is fine when we are in the lobby already.
        if (!link->joinLobby(lobby) && !link->getLobbyId().is_equal(lobby))
        {
            g_gameLinkMutex.unlock();
            return SOS_FAILED;
        }
    }
    g_gameLinkMutex.unlock();
    return SOS_IN_PROGRESS;
}

// 0x00942910 (:409)
session_op_status multi_session::platform_session_start_arbitration()
{
    return SOS_SUCCESS;
}

// 0x00971290 (:415): the host marks the lobby's match as in progress
session_op_status multi_session::platform_session_start_gameplay()
{
    if (m_host)
    {
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
        const unsigned int index = Lobby_filter_match_state;
        const unsigned int value = Match_state_in_progress;
        link->setLobbyFilters(&index, &value, 1);
        g_gameLinkMutex.unlock();
    }
    return SOS_SUCCESS;
}

// 0x009FD560 (:442): leaving for the server browser deletes the session; otherwise the host marks the lobby's match
// as over and locks a game lobby (a party lobby is unlocked, its type kept)
session_op_status multi_session::platform_session_end_gameplay()
{
    if (gameseq_is_state_in_stack(GS_MULTIPLAYER_LIVE_FIND_SERVERS))
    {
        if (m_state != MSS_DELETING && m_state != MSS_NONE)
        {
            set_state(MSS_DELETING);
        }
        return SOS_SUCCESS;
    }
    if (!m_host)
    {
        return SOS_SUCCESS;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    const unsigned int index = Lobby_filter_match_state;
    const unsigned int value = Match_state_not_in_progress;
    link->setLobbyFilters(&index, &value, 1);
    if (!m_current_info.party_lobby)
    {
        if (!platform_session_lock())
        {
            keen::debug::nativePrintArgs("[multi_session_generic] platform_session_end_gameplay: platform_session_lock failed!\n");
            g_gameLinkMutex.unlock();
            return SOS_FAILED;
        }
    }
    else if (!platform_session_unlock(false))
    {
        keen::debug::nativePrintArgs("[multi_session_generic] platform_session_end_gameplay: platform_session_unlock failed!\n");
        g_gameLinkMutex.unlock();
        return SOS_FAILED;
    }
    g_gameLinkMutex.unlock();
    return SOS_SUCCESS;
}

// 0x009B4BF0 (:487): goodbyes to the host (sent at once), then leave the lobby; platform_process_operation completes it
session_op_status multi_session::platform_session_delete()
{
    g_joinedGameLobbyViaInvitation = false;
    g_gameLinkMutex.lock(0);
    if (m_host_connection != nullptr && m_host_connection != m_me_connection)
    {
        for (int i = 0; i < s_numGoodByePackets; i++)
        {
            multi_send_goodbye(m_host_connection);
            os_sleep(1);
        }
    }
    Remote_game_session.getGameLinkSession()->leaveLobby();
    m_current_info.init();
    g_gameLinkMutex.unlock();
    return SOS_IN_PROGRESS;
}

// 0x00942920 (:515)
session_op_status multi_session::platform_session_modify()
{
    return SOS_FAILED;
}

// 0x00942930 (:557): GameLink moves the lobby ownership itself
session_op_status multi_session::platform_session_migrate()
{
    return SOS_SUCCESS;
}

// 0x009712F0 (:564): the host locks the lobby and makes it private
bool multi_session::platform_session_lock()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (m_host && link->setLobbyLocked(true) && link->setLobbyType(kaiko::GameLinkLobbyType_Private))
    {
        m_locked = true;
        g_gameLinkMutex.unlock();
        return true;
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x00971360 (:584): the host unlocks the lobby (made public on request or for a party lobby)
bool multi_session::platform_session_unlock(bool bSetlobbyTypePublic)
{
    g_gameLinkMutex.lock(0);
    m_locked = false;
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (!m_host || !link->setLobbyLocked(false))
    {
        g_gameLinkMutex.unlock();
        return false;
    }
    bool result = true;
    if (bSetlobbyTypePublic || m_current_info.party_lobby)
    {
        result = link->setLobbyType(kaiko::GameLinkLobbyType_Public);
    }
    g_gameLinkMutex.unlock();
    return result;
}

// 0x009E3230 (:608): a GameLink error fails the operation (a delete succeeds). Creating completes once in a lobby
// with an owner (the lobby owner is the host); deleting completes at once. Migrating (only the automatic one) waits up
// to 15 s for a lobby owner, then follows it: the owner's connection becomes the host, connections gone from the lobby
// are removed.
session_op_status multi_session::platform_process_operation()
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    int platform_error_code = 0;
    const kaiko::GameLinkError error = link->getError(&platform_error_code);
    // Magic number: the errors past 8 are failures (the lower ones are interactions).
    if ((int)error > 8)
    {
        // rfg.exe passes no arguments for the format.
        keen::debug::nativePrintArgs("[multi_session_generic] platform_process_operation: GameLink Error: %u (platformErrorCode: 0x%x)\n");
        link->clearError();
        g_gameLinkMutex.unlock();
        return m_state == MSS_DELETING ? SOS_SUCCESS : SOS_FAILED;
    }
    g_gameLinkMutex.unlock();
    if (m_state == MSS_CREATING)
    {
        g_gameLinkMutex.lock(0);
        link = Remote_game_session.getGameLinkSession();
        if (link->getCurrentState() == kaiko::GameLinkState_InLobby && link->getLobbyId().id.asUint64 != 0 && link->getLobbyOwner() != nullptr)
        {
            m_host = link->isLobbyOwner();
            m_desired_info.m_lobbyId = link->getLobbyId();
            s_timeSessionEntered = keen::SystemTimer::getCurrentMilliseconds();
            s_timeBecameHost = m_host ? s_timeSessionEntered : 0;
            g_gameLinkMutex.unlock();
            return SOS_SUCCESS;
        }
        g_gameLinkMutex.unlock();
        return SOS_IN_PROGRESS;
    }
    if (m_state == MSS_DELETING)
    {
        return SOS_SUCCESS;
    }
    if (m_state != MSS_MIGRATING)
    {
        return SOS_FAILED;
    }
    g_gameLinkMutex.lock(0);
    link = Remote_game_session.getGameLinkSession();
    const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
    if (s_timeHostMigrationStarted == 0)
    {
        s_timeHostMigrationStarted = now;
    }
    const kaiko::GameLinkState link_state = link->getCurrentState();
    // Magic number: 15 s for GameLink to pick the new lobby owner
    if (now - s_timeHostMigrationStarted > 15000 || m_migration_state != MSM_MIGRATING_AUTO ||
        (link_state != kaiko::GameLinkState_InLobby && (link_state != kaiko::GameLinkState_Online || !link->isPartyEnabled())) ||
        link->getError(&platform_error_code) != kaiko::GameLinkError_None)
    {
        s_timeHostMigrationStarted = 0;
        g_gameLinkMutex.unlock();
        return SOS_FAILED;
    }
    const kaiko::GameLinkRemoteStation* owner = link->getLobbyOwner();
    if (owner == nullptr)
    {
        g_gameLinkMutex.unlock();
        return SOS_IN_PROGRESS;
    }
    if (m_migration_reason == SMR_HOST_REQUESTED)
    {
        // Wait until the requested host owns the lobby (a target gone from the lobby turns the migration automatic).
        const kaiko::GameLinkRemoteStation* target = link->getRemoteStationById(m_migrate_target_host->addr.connection_id);
        if (target == nullptr)
        {
            m_migration_reason = SMR_NONE;
        }
        else if (!owner->id.is_equal(target->id))
        {
            g_gameLinkMutex.unlock();
            return SOS_IN_PROGRESS;
        }
    }
    // Magic number: up to 32 lobby members read
    const kaiko::GameLinkRemoteStation* stations[32];
    const unsigned int num_stations = link->getLobbyMembers(stations, 32);
    session_op_status status = SOS_IN_PROGRESS;
    multi_connection* cp = m_connections;
    while (cp != nullptr)
    {
        unsigned int s = 0;
        while (s < num_stations && !cp->player_uid.is_equal(stations[s]->id))
        {
            s++;
        }
        if (s >= num_stations)
        {
            // Original behaviour kept: the removed connection's next is cleared, which ends the walk.
            remove_connection(cp);
            break;
        }
        if (cp->player_uid.is_equal(owner->id))
        {
            m_migrate_target_host = cp;
            m_desired_info_base->host_player_uid.set(owner->id, true);
            const bool now_host = cp == m_me_connection;
            m_host = now_host;
            m_host_connection = cp;
            Server = now_host;
            m_migration_next_host = m_me_connection;
            if (now_host)
            {
                update_lock_state(true);
            }
            status = SOS_SUCCESS;
        }
        cp = cp->next;
        if (cp == m_connections)
        {
            break;
        }
    }
    s_timeHostMigrationStarted = 0;
    g_gameLinkMutex.unlock();
    return status;
}

// 0x00942940 (:818)
session_op_status multi_session::platform_get_operation_status()
{
    return SOS_FAILED;
}

// 0x00942950 (:825)
void multi_session::platform_cancel_current_operation()
{
}

// 0x00942960 (:832): on LAN our station in the lobby; online the main local user's platform id
session_op_status multi_session::platform_get_my_connection_address(connection_address& address, bool systemlink_or_lan)
{
    g_gameLinkMutex.lock(0);
    unit4::LocalGameSession* local_session = rfg_get_local_game_session();
    if (systemlink_or_lan)
    {
        const kaiko::GameLinkRemoteStation* stations[16];
        const unsigned int num_stations = Remote_game_session.getGameLinkSession()->getLobbyMembers(stations, 16);
        for (unsigned int i = 0; i < num_stations; i++)
        {
            if (stations[i]->myStation)
            {
                address.connection_id = stations[i]->id;
                g_gameLinkMutex.unlock();
                return SOS_SUCCESS;
            }
        }
    }
    else if (local_session->getRemoteUserId(address.connection_id, Remote_game_session.getMainUser()))
    {
        g_gameLinkMutex.unlock();
        return SOS_SUCCESS;
    }
    g_gameLinkMutex.unlock();
    return SOS_FAILED;
}

// 0x00971400 (:863): the lobby owner's station
session_op_status multi_session::platform_get_host_connection_address(connection_address& address)
{
    g_gameLinkMutex.lock(0);
    const kaiko::GameLinkRemoteStation* owner = Remote_game_session.getGameLinkSession()->getLobbyOwner();
    if (owner == nullptr)
    {
        g_gameLinkMutex.unlock();
        return SOS_FAILED;
    }
    address.set(owner->id);
    g_gameLinkMutex.unlock();
    return SOS_SUCCESS;
}

// 0x00942A20 (:880)
session_op_status multi_session::platform_add_connection(multi_connection* cp)
{
    return SOS_SUCCESS;
}

// 0x009B4D00 (:887): removing the host (not ourselves) says goodbye to it ten times
session_op_status multi_session::platform_remove_connection(multi_connection* cp)
{
    if (cp == m_host_connection && cp != m_me_connection)
    {
        for (int i = 0; i < s_numGoodByePackets; i++)
        {
            multi_send_goodbye(cp);
            os_sleep(1);
        }
    }
    return SOS_SUCCESS;
}

// 0x00942A30 (:906)
int multi_session::platform_compare_by_id(const multi_session_info_base* info)
{
    return m_current_info.compare_by_id(*info);
}

// 0x00942A40 (:915)
void multi_session::platform_session_update()
{
}
