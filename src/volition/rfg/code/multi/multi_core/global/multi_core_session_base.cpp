// Original: volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp
//
// The session base class: the state machine of a session (create, join, register, start, end, delete, migrate,
// modify), each step a sub state machine over the platform session (multi_session, generic: a GameLink lobby); the
// connection list (a circular list, the newest connection first), its slots and reservations; the session packets
// (data request/response, goodbye, heartbeat, migration, modification, arbitration, start/end).
// rfg.exe inlines most sub state setters into their callers; the port calls them (the same steps). The connection
// walks left in after a migration state change only fed a removed debug spew and are dropped.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_join.h"
#include "volition/rfg/code/multi/multi_core/include/multi_leave.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_matchmaking_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_playlists.h"
#include "volition/rfg/code/multi/multi_game/multi_game_timer.h"
#include "volition/rfg/code/multi_server_browser.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_lobby.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_party_options.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/timer.h"
#include "library/modules/runtime/base/include/keen/base/mutex.hpp"
#include "library/modules/runtime/base/include/keen/base/stringbuilder.hpp"
#include "library/modules/runtime/base/include/keen/base/systemtimer.hpp"
#include "library/modules/runtime/base/include/keen/base/trace.hpp"
#include "library/modules/runtime/gamesession/include/unit4/gamesession/remotegamesession.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

#include <string.h>

// ---- file-static globals
// multi_timer Timer_time (0x01693A64) is used here too; placed in multi_core_platform.cpp.
// multi_game_playlist* Playlists (0x02FE6C70) is used here too; placed in multi_core_matchmaking_generic.cpp.
// bool jip_destruction_sync_possible (0x02FE6C77) is used here too; placed in multi.cpp.
static multi_game_lobby_state_machine Lobby_state; // 0x02FE8344
// unsigned int s_modTableHash (0x02FEB56C) is used here too; placed in multi_core_matchmaking_generic.cpp.
// bool s_searchTimerRunning (0x02FEB570) is used here too; placed in multi_core_matchmaking_generic.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// bool Dedicated (0x02FEB591) is used here too; placed in multi.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
// preferences_members* Preferences (0x02FEB70C) is used here too; placed in multi_core_session_generic.cpp.

// A file static of multi_game_data.cpp (later in the unity TU), declared here for multi_send_migrate_packet.
static void append_player_data_full(unsigned char* packet_data, unsigned short& packet_size, multi_game_data* player_data, unsigned int handle); // 0x00951B60 (multi_game_data.cpp:548)

// Magic numbers: a session holds at most 16 connections (the acked uid lists, the party member reads, the indices).
static const int Max_session_slots = 16;

// ---- internal functions

// 0x00972D50 (:155): whether the connection's player is in the acked list. LTCG: acked_uids in EAX.
static bool connection_acked(int num_uids, multi_player_uid* acked_uids, multi_connection* connection)
{
    for (int i = 0; i < num_uids; i++)
    {
        if (connection->player_uid.is_equal(acked_uids[i]))
        {
            return true;
        }
    }
    return false;
}

// 0x00972D90 (:178): whether every connected connection but the exclusion is acked
static bool all_connections_acked(int num_uids, multi_player_uid* acked_uids, multi_connection* connections, const multi_connection* exclusion)
{
    multi_connection* cp = connections;
    while (cp != nullptr)
    {
        if (cp != exclusion && cp->connection_status != CS_DISCONNECTED)
        {
            int i = 0;
            if (num_uids < 1)
            {
                return false;
            }
            while (!cp->player_uid.is_equal(acked_uids[i]))
            {
                i++;
                if (num_uids <= i)
                {
                    return false;
                }
            }
        }
        cp = cp->next;
        if (cp == connections)
        {
            break;
        }
    }
    return true;
}

// 0x0094AE90 (:202): whether a platform user is in the acked list; only the provider data is compared (an inlined
// memcmp, not RemoteUserId::is_equal). LTCG: acked_uids in EAX.
static bool is_remote_user_id_acked(int num_uids, multi_player_uid* acked_uids, const unit4::RemoteUserId& remoteUserId)
{
    for (int i = 0; i < num_uids; i++)
    {
        if (memcmp(acked_uids[i].get_internal_id().providerData, remoteUserId.providerData, sizeof(remoteUserId.providerData)) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x0094AF30 (:215): whether every member of the GameLink party is acked
static bool are_all_party_members_acked(int num_uids, multi_player_uid* acked_uids)
{
    unit4::NamedRemoteUserId members[Max_session_slots];
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    const unsigned int num_members = link->getPartyMembers(members, Max_session_slots);
    for (unsigned int m = 0; m < num_members; m++)
    {
        int i = 0;
        if (num_uids < 1)
        {
            g_gameLinkMutex.unlock();
            return false;
        }
        while (!members[m].id.is_equal(acked_uids[i].get_internal_id()))
        {
            i++;
            if (num_uids <= i)
            {
                g_gameLinkMutex.unlock();
                return false;
            }
        }
    }
    g_gameLinkMutex.unlock();
    return true;
}

// 0x00990950 (:244): walks the sessions and does nothing else (the body was compiled out)
void session_connection_dropped(multi_connection* cp)
{
    Session_pool.thread_access.enter();
    for (multi_session* sp = Session_pool.get_first(0); sp != nullptr; sp = Session_pool.get_next(sp))
    {
    }
    Session_pool.thread_access.leave();
}

// ---- multi_session_info_base

// 0x009AA310 (:265)
multi_session_info_base::multi_session_info_base()
{
    host_player_uid.invalidate();
    type = ST_PARTY;
    max_slots = 0;
    private_match = false;
    uses_presence = false;
    uses_peer_to_peer_networking = false;
    uses_abitration = false;
    advertise_for_matchmaking = false;
    reports_stats = false;
    systemlink_or_lan = false;
    invited = false;
    dedicated = false;
    allow_join_in_progress = false;
    party_lobby = false;
    invite_type = SIT_NONE;
}

// 0x009909F0 (:270): copy() here is the virtual one of this class (the vptr is still this class's)
multi_session_info_base::multi_session_info_base(const multi_session_info_base& src)
{
    multi_session_info_base::copy(src);
}

// 0x00990A10 (:275)
void multi_session_info_base::init()
{
    host_player_uid.invalidate();
    type = ST_PARTY;
    max_slots = 0;
    private_match = false;
    uses_presence = false;
    uses_peer_to_peer_networking = false;
    uses_abitration = false;
    advertise_for_matchmaking = false;
    reports_stats = false;
    systemlink_or_lan = false;
    invited = false;
    dedicated = false;
    allow_join_in_progress = false;
    party_lobby = false;
    invite_type = SIT_NONE;
}

// 0x00972E00 (:299)
void multi_session_info_base::copy(const multi_session_info_base& src)
{
    type = src.type;
    host_player_uid = src.host_player_uid;
    max_slots = src.max_slots;
    private_match = src.private_match;
    uses_presence = src.uses_presence;
    uses_peer_to_peer_networking = src.uses_peer_to_peer_networking;
    uses_abitration = src.uses_abitration;
    advertise_for_matchmaking = src.advertise_for_matchmaking;
    reports_stats = src.reports_stats;
    systemlink_or_lan = src.systemlink_or_lan;
    invited = src.invited;
    dedicated = src.dedicated;
    allow_join_in_progress = src.allow_join_in_progress;
    party_lobby = src.party_lobby;
    invite_type = src.invite_type;
}

// 0x0094B000 (:324)
void multi_session_info_base::copy(const multi_session_info_base* src)
{
    copy(*src);
}

// 0x00972E80 (:329)
bool multi_session_info_base::is_valid() const
{
    return host_player_uid.is_valid();
}

// ---- multi_session_base

// 0x009AA350 (:340)
multi_session_base::multi_session_base(multi_session_info_base* current, multi_session_info_base* desired)
    : m_current_info_base(current), m_desired_info_base(desired)
{
    init(true);
}

// 0x0094B010 (:346)
multi_session_base::~multi_session_base()
{
}

// 0x00990A40 (:351): the join status and the migration reason survive unless clear_persistent_members
void multi_session_base::init(bool clear_persistent_members)
{
    m_last_state = MSS_NONE;
    m_state = MSS_NONE;
    m_create_state = MSC_NOT_CREATING;
    m_join_state = MSJ_NOT_JOINING;
    m_migration_state = MSM_NOT_MIGRATING;
    m_modification_state = MSD_NOT_MODIFYING;
    for (int i = 0; i < Max_session_slots; i++)
    {
        m_slots.reservations[i].reserver.invalidate();
        m_slots.reservations[i].reservee.invalidate();
    }
    m_slots.reserved = 0;
    m_slots.used = 0;
    m_host = false;
    m_connections = nullptr;
    m_host_connection = nullptr;
    m_me_connection = nullptr;
    m_locked = false;
    m_create_source = nullptr;
    for (int i = 0; i < Max_session_slots; i++)
    {
        m_create_players_acked_uids[i].invalidate();
    }
    m_create_players_acked_count = 0;
    m_create_timeout.invalidate();
    m_join_source = nullptr;
    for (int i = 0; i < Max_session_slots; i++)
    {
        m_join_players_acked_uids[i].invalidate();
    }
    m_join_players_acked_count = 0;
    m_join_timeout.invalidate();
    m_join_invited = false;
    m_join_is_merge = false;
    if (clear_persistent_members)
    {
        m_join_last_status = MSJS_INVALID;
        m_migration_reason = SMR_NONE;
    }
    m_joinReservedSlot = false;
    m_migrate_packet_received = false;
    m_migrate_target_host = nullptr;
    m_migrate_timeout.invalidate();
    for (int i = 0; i < Max_session_slots; i++)
    {
        m_migrate_players_acked_uids[i].invalidate();
    }
    m_migrate_players_acked_count = 0;
    m_migration_choose_server_timestamp.invalidate();
    m_migration_next_host = nullptr;
    m_modification_timeout.invalidate();
    for (int i = 0; i < Max_session_slots; i++)
    {
        m_modification_acked_uids[i].invalidate();
    }
    m_modification_acked_count = 0;
    m_starting_received_start = false;
    m_starting_timeout.invalidate();
    for (int i = 0; i < Max_session_slots; i++)
    {
        m_starting_acked_uids[i].invalidate();
    }
    m_starting_acked_count = 0;
    m_cb_migarate_allow_migrate = nullptr;
    m_cb_migrate_about_to_start = nullptr;
    m_cb_migration_just_occurred = nullptr;
    m_cb_migration_failed = nullptr;
    m_cb_player_may_join = nullptr;
    m_cb_player_joined = nullptr;
    m_cb_player_about_to_leave = nullptr;
    m_cb_player_left = nullptr;
    m_cb_send_join_new_session = nullptr;
    m_cb_recv_join_new_session = nullptr;
    const session_invite_type invite_type = ui_document_mp_party_options_get_desired_invite_type();
    m_partyLobbyInviteType = invite_type == SIT_NONE ? SIT_FRIENDS : invite_type;
}

// 0x009FDC80 (:459)
void multi_session_base::session_create(const multi_session_info_base* info, multi_session_base* create_source)
{
    if (m_state == MSS_NONE && info->max_slots <= Max_session_slots)
    {
        m_create_source = create_source;
        m_host = true;
        m_desired_info_base->copy(*info);
        set_state(MSS_CREATING);
    }
}

// 0x009FDCC0 (:478): a join creates the local session too (set_creation_state joins instead of creating)
void multi_session_base::session_join(const multi_session_info_base* info, multi_session_base* join_source, bool reservedSlot)
{
    if (m_state == MSS_NONE && info->max_slots <= Max_session_slots)
    {
        m_joinReservedSlot = reservedSlot;
        m_join_source = join_source;
        m_host = false;
        m_desired_info_base->copy(*info);
        set_state(MSS_CREATING);
    }
}

// 0x009FDD10 (:504): hand the host over: the lobby owner passes the GameLink lobby ownership first; if that fails the
// session is deleted
void multi_session_base::session_migrate(multi_connection* new_host, session_migration_reason reason)
{
    if (new_host == nullptr || new_host == m_host_connection)
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (!link->isLobbyOwner() || link->setLobbyOwner(new_host->addr.connection_id))
    {
        multi_connection_send_cannot_connect_block(static_cast<multi_session*>(this), new_host);
        multi_send_force_migrate_packet(new_host, m_migration_next_host, reason);
        m_migrate_target_host = new_host;
        m_migration_reason = reason;
        set_state(MSS_MIGRATING);
    }
    else if (m_state != MSS_DELETING && m_state != MSS_NONE)
    {
        set_state(MSS_DELETING);
    }
    g_gameLinkMutex.unlock();
}

// 0x009FBBD0 (:535)
void multi_session_base::session_start_arbitration()
{
    if (m_state == MSS_READY)
    {
        const session_op_status status = platform_session_start_arbitration();
        if (status == SOS_SUCCESS)
        {
            set_state(MSS_REGISTERED);
        }
        else if (status == SOS_IN_PROGRESS)
        {
            set_state(MSS_REGISTERING);
        }
    }
}

// 0x009B6A70 (:555): the start is taken by process_state (READY -> STARTING)
void multi_session_base::session_start_gameplay()
{
    m_starting_received_start = true;
    multi_voice_handle_voice_chat_at_game_start_and_end(true);
    s_searchTimerRunning = false;
}

// 0x009FBC00 (:564): the host tells every client (PT_END)
void multi_session_base::session_end_gameplay()
{
    if (m_host)
    {
        multi_connection* cp = m_connections;
        while (cp != nullptr)
        {
            if (cp != m_host_connection)
            {
                multi_send_end(cp);
            }
            cp = cp->next;
            if (cp == m_connections)
            {
                break;
            }
        }
    }
    if (m_state == MSS_IN_GAME)
    {
        set_state(MSS_ENDING);
        multi_voice_handle_voice_chat_at_game_start_and_end(false);
    }
}

// 0x009FBCB0 (:581)
void multi_session_base::session_delete()
{
    if (m_state != MSS_DELETING && m_state != MSS_NONE)
    {
        set_state(MSS_DELETING);
    }
}

// 0x0094B020 (:588)
void multi_session_base::session_modify(const multi_session_info_base* info)
{
    keen::debug::nativePrintArgs("multi_session_base::session_modify() is not supported anymore!\n");
}

// 0x009419E0 (.h:306)
bool multi_session_base::is_locked()
{
    return m_locked;
}

// 0x00941A00 (.h:571)
const slot_use_data& multi_session_base::get_slot_use_data() const
{
    return m_slots;
}

// 0x0094B030 (:602)
void multi_session_base::session_lock()
{
    platform_session_lock();
}

// 0x0094B040 (:607)
void multi_session_base::session_unlock(bool bSetlobbyTypePublic)
{
    platform_session_unlock(bSetlobbyTypePublic);
}

// 0x0094B050 (:613)
bool multi_session_base::operation_in_progress()
{
    switch (m_state)
    {
    case MSS_CREATING:
    case MSS_JOINING:
    case MSS_REGISTERING:
    case MSS_STARTING:
    case MSS_ENDING:
    case MSS_DELETING:
    case MSS_MIGRATING:
    case MSS_MODIFYING:
        return true;
    default:
        return false;
    }
}

// 0x009FBCD0 (:632)
void multi_session_base::cancel_current_operation()
{
    platform_cancel_current_operation();
    switch (m_state)
    {
    case MSS_CREATING:
    case MSS_JOINING:
    case MSS_MIGRATING:
    case MSS_MODIFYING:
        m_desired_info_base->copy(*m_current_info_base);
        break;
    default:
        break;
    }
    set_state(m_last_state);
}

// 0x009FBD30 (:648): one step of the current operation. A session whose GameLink lobby changed under it is deleted.
void multi_session_base::process_state()
{
    bool lobby_changed = false;
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (m_state != MSS_NONE && m_state != MSS_DELETING && link != nullptr)
    {
        const kaiko::GameLinkLobby& current_lobby = static_cast<multi_session_info*>(m_current_info_base)->m_lobbyId;
        const kaiko::GameLinkLobby lobby = link->getLobbyId();
        if (lobby.id.asUint64 != 0 && current_lobby.id.asUint64 != 0 && !lobby.is_equal(current_lobby))
        {
            lobby_changed = true;
        }
    }
    g_gameLinkMutex.unlock();
    if (lobby_changed)
    {
        set_state(MSS_DELETING);
    }
    session_op_status status;
    switch (m_state)
    {
    case MSS_CREATING:
        status = process_creating();
        if (status == SOS_SUCCESS)
        {
            set_state(MSS_CREATED);
        }
        else if (status == SOS_FAILED)
        {
            set_state(MSS_NONE);
        }
        break;
    case MSS_JOINING:
        status = process_joining();
        if (status == SOS_SUCCESS)
        {
            set_state(MSS_JOINED);
        }
        else if (status == SOS_FAILED)
        {
            session_delete();
        }
        break;
    case MSS_READY:
        if (m_migration_state == MSM_MIGRATION_FAILED)
        {
            session_delete();
        }
        else if (m_starting_received_start)
        {
            m_starting_received_start = false;
            set_state(MSS_STARTING);
        }
        else if (m_migrate_packet_received)
        {
            set_state(MSS_MIGRATING);
        }
        break;
    case MSS_REGISTERING:
        status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            set_state(MSS_REGISTERED);
        }
        else if (status == SOS_FAILED)
        {
            session_delete();
        }
        break;
    case MSS_REGISTERED:
    case MSS_ENDED:
        if (m_migration_state == MSM_MIGRATION_FAILED)
        {
            session_delete();
        }
        break;
    case MSS_STARTING:
        status = process_starting();
        if (status == SOS_SUCCESS)
        {
            set_state(MSS_IN_GAME);
        }
        else if (status == SOS_FAILED)
        {
            session_delete();
        }
        break;
    case MSS_IN_GAME:
        if (m_migration_state == MSM_MIGRATION_FAILED)
        {
            session_end_gameplay();
        }
        else if (m_migrate_packet_received)
        {
            set_state(MSS_MIGRATING);
        }
        break;
    case MSS_ENDING:
        status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            set_state(MSS_ENDED);
        }
        else if (status == SOS_FAILED)
        {
            set_state(MSS_NONE);
        }
        break;
    case MSS_DELETING:
        status = platform_process_operation();
        if (status == SOS_SUCCESS || status == SOS_FAILED)
        {
            set_state(MSS_NONE);
        }
        break;
    case MSS_MIGRATING:
        status = process_migrating();
        if (status == SOS_SUCCESS)
        {
            reset_connection_first_transmission_time(m_host_connection);
            set_state(m_last_state);
        }
        else if (status == SOS_FAILED)
        {
            set_state(m_last_state);
            if (m_state == MSS_IN_GAME)
            {
                set_state(MSS_ENDING);
            }
            else
            {
                session_delete();
            }
        }
        break;
    case MSS_MODIFYING:
        status = process_modification();
        if (status == SOS_SUCCESS || status == SOS_FAILED)
        {
            set_state(m_last_state);
        }
        break;
    default:
        break;
    }
}

// 0x0094B090 (:885)
bool multi_session_base::is_party_owner_and_has_other_party_members()
{
    unit4::NamedRemoteUserId members[Max_session_slots];
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link->isPartyEnabled() && link->isPartyOwner() && link->getPartyMembers(members, Max_session_slots) > 1)
    {
        g_gameLinkMutex.unlock();
        return true;
    }
    g_gameLinkMutex.unlock();
    return false;
}

// 0x009F7340 (:904): enter a state and run it while it completes at once
void multi_session_base::set_state(multi_session_state new_state)
{
    for (;;)
    {
        const bool same_state = new_state == m_state;
        if (!same_state)
        {
            m_last_state = m_state;
            m_state = new_state;
        }
        session_op_status status;
        switch (m_state)
        {
        case MSS_NONE:
            g_joinedGameLobbyViaInvitation = false;
            if (m_connections != nullptr)
            {
                if (m_me_connection != nullptr)
                {
                    remove_connection(m_me_connection);
                }
                multi_connection* cp = m_connections;
                while (cp != nullptr)
                {
                    multi_connection* const next = cp->next;
                    multi_connection* const head = m_connections;
                    remove_connection(cp);
                    // Original behaviour kept: the connection was freed back to its pool by remove_connection.
                    multi_connection_disconnect(cp, CDR_SESSION_REMOVED);
                    cp = next != head ? next : nullptr;
                }
            }
            if (m_current_info_base->type == ST_GAME)
            {
                Server = false;
                Dedicated = false;
            }
            if (m_migration_state == MSM_MIGRATION_FAILED && m_cb_migration_failed != nullptr)
            {
                m_cb_migration_failed(this);
            }
            init(false);
            return;
        case MSS_CREATING:
            status = set_creation_state(MSC_WAITING_ON_CREATE);
            if (status == SOS_SUCCESS)
            {
                new_state = MSS_CREATED;
                continue;
            }
            if (status == SOS_FAILED)
            {
                new_state = MSS_NONE;
                continue;
            }
            return;
        case MSS_CREATED:
            new_state = !m_host && m_join_state != MSJ_JOIN_FAILED ? MSS_JOINING : MSS_READY;
            continue;
        case MSS_JOINING:
            status = set_join_state(MSJ_WAITING_FOR_ACCEPT);
            if (status == SOS_SUCCESS)
            {
                new_state = MSS_READY;
                continue;
            }
            if (status == SOS_FAILED)
            {
                new_state = MSS_NONE;
                continue;
            }
            return;
        case MSS_JOINED:
            s_searchTimerRunning = false;
            new_state = MSS_READY;
            continue;
        case MSS_STARTING:
            if (same_state)
            {
                // Re-entered: a client registered for arbitration starts now.
                if (m_starting_state != MSSS_CLIENT_WAITING_FOR_HOST_TO_SIGNAL_START)
                {
                    return;
                }
                status = set_starting_state(MSSS_STARTING);
                if (status == SOS_SUCCESS)
                {
                    // Original behaviour kept: this path goes back to READY, not IN_GAME.
                    new_state = MSS_READY;
                    continue;
                }
            }
            else
            {
                if (!m_current_info_base->uses_abitration)
                {
                    status = set_starting_state(MSSS_STARTING);
                }
                else if (m_host)
                {
                    status = set_starting_state(MSSS_HOST_WAITING_FOR_CLIENTS_TO_REGISTER);
                }
                else
                {
                    status = set_starting_state(MSSS_REGISTERING);
                }
                if (status == SOS_SUCCESS)
                {
                    new_state = MSS_IN_GAME;
                    continue;
                }
            }
            if (status == SOS_FAILED)
            {
                session_delete();
            }
            return;
        case MSS_ENDING:
            status = platform_session_end_gameplay();
            if (status == SOS_SUCCESS)
            {
                new_state = MSS_ENDED;
                continue;
            }
            if (status == SOS_FAILED && m_state != MSS_DELETING && m_state != MSS_NONE)
            {
                new_state = MSS_DELETING;
                continue;
            }
            return;
        case MSS_ENDED:
            new_state = m_migration_state != MSM_MIGRATION_FAILED ? MSS_READY : MSS_DELETING;
            continue;
        case MSS_DELETING:
            status = platform_session_delete();
            if (status == SOS_SUCCESS || status == SOS_FAILED)
            {
                new_state = MSS_NONE;
                continue;
            }
            return;
        case MSS_MIGRATING:
            if (m_migration_reason != SMR_HOST_REQUESTED)
            {
                // A lost or leaving host: GameLink picks the new lobby owner (process_migrating follows it).
                set_migration_state(MSM_MIGRATING_AUTO);
                return;
            }
            if (m_migrate_target_host == nullptr && m_cb_migarate_allow_migrate != nullptr && !m_cb_migarate_allow_migrate(this))
            {
                status = SOS_FAILED;
            }
            else
            {
                if (m_cb_migrate_about_to_start != nullptr && !same_state)
                {
                    m_cb_migrate_about_to_start(this);
                }
                if (m_migration_next_host == m_me_connection || m_migrate_target_host == m_me_connection)
                {
                    status = set_migration_state(MSM_BECOMING_HOST);
                }
                else if (!m_migrate_packet_received)
                {
                    status = set_migration_state(MSM_WAITING_FOR_HOST);
                }
                else
                {
                    status = set_migration_state(MSM_MIGRATING);
                }
            }
            if (status == SOS_SUCCESS)
            {
                new_state = m_last_state;
                continue;
            }
            if (status != SOS_FAILED)
            {
                return;
            }
            set_migration_state(MSM_MIGRATION_FAILED);
            if (m_last_state == MSS_READY)
            {
                session_delete();
                return;
            }
            if (m_last_state != MSS_IN_GAME)
            {
                // rfg.exe: an Int3 macro (the break is compiled out; the format arguments are not passed).
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                return;
            }
            new_state = MSS_ENDING;
            continue;
        case MSS_MODIFYING:
            status = set_modification_state(MSD_MODIFYING);
            if (status == SOS_SUCCESS)
            {
                new_state = m_last_state;
                continue;
            }
            if (status == SOS_FAILED && m_state != MSS_DELETING && m_state != MSS_NONE)
            {
                new_state = MSS_DELETING;
                continue;
            }
            return;
        default:
            return;
        }
    }
}

// 0x009F7980 (:1187)
session_op_status multi_session_base::process_creating()
{
    switch (m_create_state)
    {
    case MSC_WAITING_ON_CREATE:
    {
        const session_op_status status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            return set_creation_state(MSC_CREATE_SUCCESS);
        }
        if (status != SOS_FAILED)
        {
            return SOS_IN_PROGRESS;
        }
        break;
    }
    case MSC_WAITING_FOR_INFO:
        if (!m_create_timeout.elapsed())
        {
            return SOS_IN_PROGRESS;
        }
        // Magic number: the info request is sent 4 times in all
        if (m_create_info_retry_count < 4)
        {
            return set_creation_state(MSC_WAITING_FOR_INFO);
        }
        break;
    case MSC_WAITING_FOR_CLIENTS:
        if (!m_create_timeout.elapsed())
        {
            if (!are_all_party_members_in_game_lobby() && !are_all_party_members_acked(m_create_players_acked_count, m_create_players_acked_uids))
            {
                return SOS_IN_PROGRESS;
            }
            return set_creation_state(MSC_NOT_CREATING);
        }
        break;
    default:
        return SOS_FAILED;
    }
    return set_creation_state(MSC_CREATE_FAILED);
}

// 0x009E4100 (:1256): enter a creation state and run it while it completes at once
session_op_status multi_session_base::set_creation_state(multi_session_creation_state new_state)
{
    for (;;)
    {
        bool party = is_party_owner_and_has_other_party_members();
        const multi_session_creation_state last_state = m_create_state;
        m_create_state = new_state;
        switch (new_state)
        {
        case MSC_NOT_CREATING:
            return SOS_SUCCESS;
        case MSC_WAITING_ON_CREATE:
        {
            // A joining client "creates" its side of the session by joining the platform session.
            const session_op_status status = m_host ? platform_session_create() : platform_session_join();
            if (status == SOS_SUCCESS)
            {
                new_state = MSC_CREATE_SUCCESS;
            }
            else if (status == SOS_FAILED)
            {
                new_state = MSC_CREATE_FAILED;
            }
            else
            {
                return SOS_IN_PROGRESS;
            }
            break;
        }
        case MSC_WAITING_FOR_INFO:
        {
            m_current_info_base->copy(*m_desired_info_base);
            connection_address host_address;
            if (platform_get_host_connection_address(host_address) != SOS_SUCCESS)
            {
                return set_creation_state(MSC_CREATE_FAILED);
            }
            multi_connection* cp = multi_connection_allocate(&host_address, static_cast<multi_session*>(this), nullptr, nullptr, false, false);
            multi_session_send_data_request(cp, m_desired_info_base);
            multi_connection_flush_current_packet(cp);
            multi_connection_delete(cp);
            if (last_state == m_create_state)
            {
                m_create_info_retry_count++;
            }
            else
            {
                m_create_info_retry_count = 0;
            }
            // Magic number: 7 s per info request
            m_create_timeout.set(7000);
            return SOS_IN_PROGRESS;
        }
        case MSC_WAITING_FOR_CLIENTS:
            if (party)
            {
                // The party leader reserves slots for its party members (not in a party lobby).
                if (m_host && is_party_owner_and_has_other_party_members() && !m_current_info_base->party_lobby && !m_desired_info_base->party_lobby)
                {
                    unit4::NamedRemoteUserId members[Max_session_slots];
                    g_gameLinkMutex.lock(0);
                    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
                    const unsigned int num_members = link->getPartyMembers(members, Max_session_slots);
                    g_gameLinkMutex.unlock();
                    if (num_members > 1)
                    {
                        multi_player_uid reservees[Max_session_slots];
                        int num_reservees = 0;
                        for (unsigned int i = 0; i < num_members; i++)
                        {
                            if (!members[i].id.is_equal(My_player_uid.get_internal_id()))
                            {
                                reservees[num_reservees].set(members[i].id, true);
                                num_reservees++;
                            }
                        }
                        if (!reserve_slots(num_reservees, My_player_uid, reservees))
                        {
                            return SOS_FAILED;
                        }
                    }
                }
                m_create_players_acked_count = 1;
                m_create_players_acked_uids[0] = My_player_uid;
                // Magic number: 15 s for the party to follow its leader
                m_create_timeout.set(15000);
                return SOS_IN_PROGRESS;
            }
            new_state = MSC_NOT_CREATING;
            break;
        case MSC_CREATE_SUCCESS:
        {
            m_current_info_base->copy(*m_desired_info_base);
            multi_game_bw_reset(m_current_info_base->type);
            connection_address host_address;
            connection_address my_address;
            if (platform_get_host_connection_address(host_address) != SOS_SUCCESS ||
                platform_get_my_connection_address(my_address, m_current_info_base->systemlink_or_lan) != SOS_SUCCESS)
            {
                return set_creation_state(MSC_CREATE_FAILED);
            }
            const bool am_host = my_address.connection_id.is_equal(host_address.connection_id);
            m_host = am_host;
            m_connections = nullptr;
            g_gameLinkMutex.lock(0);
            kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
            const kaiko::GameLinkRemoteStation* owner = link->getLobbyOwner();
            if (owner != nullptr)
            {
                m_current_info_base->host_player_uid.set(owner->id, true);
            }
            g_gameLinkMutex.unlock();
            multi_player_uid* host_uid = &m_current_info_base->host_player_uid;
            m_host_connection = multi_connection_allocate(&host_address, static_cast<multi_session*>(this), host_uid, host_uid, true, am_host);
            m_me_connection = am_host ? m_host_connection
                                      : multi_connection_allocate(&my_address, static_cast<multi_session*>(this), &My_player_uid, &My_player_uid, false, true);
            if (add_connection(m_host_connection, 0xFF, 0) == MSJS_JOIN_ACCEPT)
            {
                if (m_host_connection == m_me_connection || add_connection(m_me_connection, 0xFF, 0) == MSJS_JOIN_ACCEPT)
                {
                    if (m_current_info_base->type == ST_GAME)
                    {
                        Server = m_host;
                        if (Server)
                        {
                            Dedicated = m_current_info_base->dedicated;
                        }
                    }
                    new_state = is_party_owner_and_has_other_party_members() ? MSC_WAITING_FOR_CLIENTS : MSC_NOT_CREATING;
                    break;
                }
                remove_connection(m_host_connection);
                multi_connection_delete(m_me_connection);
            }
            else
            {
                // Original behaviour kept: as the host both are the same connection, deleted twice.
                multi_connection_delete(m_host_connection);
                multi_connection_delete(m_me_connection);
            }
            m_me_connection = nullptr;
            m_host_connection = nullptr;
            return set_creation_state(MSC_CREATE_FAILED);
        }
        case MSC_CREATE_FAILED:
            if (last_state == MSC_WAITING_ON_CREATE)
            {
                return SOS_FAILED;
            }
            session_delete();
            return SOS_IN_PROGRESS;
        default:
            return SOS_FAILED;
        }
    }
}

// 0x009E45A0 (:1556)
session_op_status multi_session_base::process_joining()
{
    switch (m_join_state)
    {
    case MSJ_WAITING_FOR_ACCEPT:
    {
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
        const kaiko::GameLinkLobby lobby = link->getLobbyId();
        if (static_cast<multi_session_info*>(m_current_info_base)->m_lobbyId.is_equal(lobby))
        {
            g_gameLinkMutex.unlock();
            if (connection_acked(m_join_players_acked_count, m_join_players_acked_uids, m_host_connection))
            {
                return set_join_state(is_party_owner_and_has_other_party_members() ? MSJ_WAITING_FOR_CLIENTS : MSJ_JOIN_COMPLETED);
            }
            // A deny invalidates the timeout (join_received_join_denied keeps its status).
            if (m_join_timeout.valid())
            {
                if (!m_join_timeout.elapsed())
                {
                    return SOS_IN_PROGRESS;
                }
                m_join_last_status = MSJS_JOIN_DENY_TIMEOUT;
            }
            return set_join_state(MSJ_JOIN_FAILED);
        }
        m_join_last_status = MSJS_JOIN_DENY_TIMEOUT;
        const session_op_status status = set_join_state(MSJ_JOIN_FAILED);
        g_gameLinkMutex.unlock();
        return status;
    }
    case MSJ_WAITING_FOR_CLIENTS:
        if (m_join_timeout.elapsed())
        {
            return set_join_state(MSJ_JOIN_FAILED);
        }
        if (!are_all_party_members_in_game_lobby() && !are_all_party_members_acked(m_join_players_acked_count, m_join_players_acked_uids))
        {
            return SOS_IN_PROGRESS;
        }
        return set_join_state(MSJ_JOIN_COMPLETED);
    case MSJ_WAITING_FOR_HOST:
    {
        g_gameLinkMutex.lock(0);
        kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
        unit4::NamedRemoteUserId party_owner;
        session_op_status status;
        if (link->getPartyOwner(&party_owner) && is_remote_user_id_acked(m_join_players_acked_count, m_join_players_acked_uids, party_owner.id))
        {
            status = set_join_state(MSJ_JOIN_COMPLETED);
        }
        else if (!m_join_timeout.elapsed())
        {
            g_gameLinkMutex.unlock();
            return SOS_IN_PROGRESS;
        }
        else
        {
            status = set_join_state(MSJ_JOIN_FAILED);
        }
        g_gameLinkMutex.unlock();
        return status;
    }
    case MSJ_MERGE_WAITING_FOR_CLIENTS:
        if (!m_join_timeout.elapsed() && !are_all_party_members_acked(m_join_players_acked_count, m_join_players_acked_uids))
        {
            // Original behaviour kept: waiting reports a failure here (not IN_PROGRESS).
            return SOS_FAILED;
        }
        return set_join_state(MSJ_NOT_JOINING);
    default:
        return SOS_FAILED;
    }
}

// 0x0094B130 (:1711): whether every GameLink party member is in the lobby (provider data compared). With no party
// members it is true; with party members and no lobby members, false.
bool multi_session_base::are_all_party_members_in_game_lobby()
{
    unit4::NamedRemoteUserId members[Max_session_slots];
    const kaiko::GameLinkRemoteStation* stations[Max_session_slots];
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    const unsigned int num_members = link->getPartyMembers(members, Max_session_slots);
    const unsigned int num_stations = link->getLobbyMembers(stations, Max_session_slots);
    bool result = true;
    for (unsigned int m = 0; m < num_members; m++)
    {
        unsigned int s = 0;
        while (s < num_stations && memcmp(stations[s]->id.providerData, members[m].id.providerData, sizeof(members[m].id.providerData)) != 0)
        {
            s++;
        }
        if (s >= num_stations)
        {
            result = false;
            break;
        }
    }
    g_gameLinkMutex.unlock();
    return result;
}

// 0x0094B270 (:1745)
void multi_session_base::set_session_info(const multi_session_info_base& sessionInfo)
{
    m_desired_info_base->copy(sessionInfo);
    m_current_info_base->copy(sessionInfo);
}

// Port deviation (see multi_core_session_base.h), off by default.
bool g_openrfgSkipSystemLinkTableHashCheck = false;

// 0x009F7AB0 (:1751): the lobby filters carry the build tag and the modified table hash; a lobby that does not match
// is left with a message box (the party is kept)
void multi_session_base::check_modified_table_hash()
{
    // Magic numbers: the lobby filter build tags (word 0): of banned users' lobbies, and of the others.
    static const unsigned int banned_lobby_tag = 0xDEADD00D;
    static const unsigned int lobby_tag = 0xAAAA0008;
    unsigned int filters[8];
    // Port deviation (see g_openrfgSkipSystemLinkTableHashCheck): a system link session is not checked.
    if (g_openrfgSkipSystemLinkTableHashCheck && m_current_info_base->systemlink_or_lan)
    {
        return;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && link->getCurrentState() == kaiko::GameLinkState_InLobby && link->getLobbyFilters(filters, 8) != 0)
    {
        bool any_filter = false;
        for (int i = 0; i < 8; i++)
        {
            any_filter = any_filter || filters[i] != 0;
        }
        if (any_filter)
        {
            const bool tag_ok = multi_core_matchmake_internal_is_my_user_banned() ? filters[0] == banned_lobby_tag : filters[0] == lobby_tag;
            if (!(tag_ok && filters[1] == s_modTableHash) && m_state != MSS_DELETING && m_state != MSS_NONE)
            {
                if (!tag_ok)
                {
                    ui_add_msgbox(MBT_OK, "MENU_VERSION_MISMATCH_TITLE", "MENU_VERSION_MISMATCH_DESC", nullptr, false, true, nullptr, nullptr, false);
                }
                else
                {
                    ui_add_msgbox(MBT_OK, "MENU_MBOX_INCORRECT_MODIFIED_TABLE_HASH", "MENU_INCORRECT_MODIFIED_TABLE_HASH", nullptr, false, true, nullptr, nullptr, false);
                }
                const bool party_enabled = link->isPartyEnabled();
                link->setPartyEnabled(false);
                set_state(MSS_DELETING);
                if (party_enabled)
                {
                    link->setPartyEnabled(true);
                }
            }
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x009B6A90 (:1829): the custom game party lobby owner sets the GameLink lobby type and tells the lobby (PT_INVITE_TYPE)
void multi_session_base::set_party_lobby_invite_type(session_invite_type inviteType)
{
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    m_partyLobbyInviteType = inviteType;
    if (Party_lobby_type == MPPLT_CUSTOM_GAME && link->isLobbyOwner() && m_current_info_base->party_lobby)
    {
        kaiko::GameLinkLobbyType lobby_type = kaiko::GameLinkLobbyType_None;
        bool known_type = true;
        switch (inviteType)
        {
        case SIT_ALL:
            lobby_type = kaiko::GameLinkLobbyType_Public;
            break;
        case SIT_FRIENDS:
            lobby_type = kaiko::GameLinkLobbyType_Friends;
            break;
        case SIT_PRIVATE:
            lobby_type = kaiko::GameLinkLobbyType_Private;
            break;
        default:
            known_type = false;
            break;
        }
        if (known_type && link->setLobbyType(lobby_type))
        {
            unsigned char packet[1200];
            packet[0] = PT_INVITE_TYPE;
            const unsigned int header = Multi_packets[PT_INVITE_TYPE].group_id != 0 ? 5 : 3;
            packet[header] = (unsigned char)inviteType;
            const unsigned int size = header + 1;
            packet[1] = (unsigned char)((size - 3) >> 8);
            packet[2] = (unsigned char)(size - 3);
            multi_session_base* lobby = Lobby_session;
            multi_connection* cp = lobby->m_connections;
            while (cp != nullptr)
            {
                if (cp->connection_status != CS_DISCONNECTED && cp != static_cast<multi_session_base*>(cp->owner)->m_host_connection)
                {
                    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
                }
                cp = cp->next;
                if (cp == lobby->m_connections)
                {
                    break;
                }
            }
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x0094B2A0 (:1891)
session_invite_type multi_session_base::get_party_lobby_invite_type() const
{
    return m_partyLobbyInviteType;
}

// 0x0094B2B0 (:1896)
void multi_session_base::process_invite_type_packet(unsigned char* packet_data, const sender& from)
{
    if (m_current_info_base->party_lobby)
    {
        m_partyLobbyInviteType = (session_invite_type)packet_data[0];
    }
}

// 0x00990C40 (:1908): after a migration the pending reliable packets to the new host count from now
void multi_session_base::reset_connection_first_transmission_time(multi_connection* pConnection)
{
    if (pConnection == nullptr)
    {
        return;
    }
    pConnection->m_reliable_data.thread_access.enter();
    for (reliable_data* rd = pConnection->m_reliable_data.get_first(0); rd != nullptr; rd = pConnection->m_reliable_data.get_next(rd))
    {
        if (rd->m_connection_ack_id != 0xFFFF)
        {
            rd->m_first_transmission_time = Frametime_total_milliseconds_increment_always;
        }
    }
    pConnection->m_reliable_data.thread_access.leave();
}

// 0x00972EB0 (:1922): a playlist of solo players disables the GameLink party (outside a party lobby)
void multi_session_base::disable_party_if_solo_playlist()
{
    // Magic number: lobby filter word 2 with this value keeps the party (meaning not recovered).
    static const unsigned int Keep_party_filter = 0x1234FFFF;
    if ((m_state == MSS_READY || m_state == MSS_STARTING || m_state == MSS_IN_GAME || m_state == MSS_ENDED) && Playlists != nullptr)
    {
        // Original behaviour kept: rfg.exe tests &Preferences->current_match against null, which never fails.
        multi_game_playlist* playlist = Playlists;
        while (playlist->playlist_id.get_crc() != Preferences->current_match.m_playlist_id)
        {
            playlist = playlist->next;
            if (playlist == Playlists || playlist == nullptr)
            {
                return;
            }
        }
        if (playlist->max_party_size == 1)
        {
            unsigned int filters[8];
            g_gameLinkMutex.lock(0);
            kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
            if (link != nullptr && link->getLobbyFilters(filters, 8) != 0)
            {
                bool any_filter = false;
                for (int i = 0; i < 8; i++)
                {
                    any_filter = any_filter || filters[i] != 0;
                }
                if (any_filter && filters[2] != Keep_party_filter && !m_current_info_base->party_lobby && link->isPartyEnabled() &&
                    link->getCurrentState() == kaiko::GameLinkState_InLobby)
                {
                    link->setPartyEnabled(false);
                }
            }
            g_gameLinkMutex.unlock();
        }
    }
}

// 0x00973000 (:1980): the Steam lobby owner lists the lobby members' Steam ids in the lobby data ("players", once a
// second) for Nerio (the remaster's online services)
void multi_session_base::set_extra_lobby_data_for_nerio()
{
    static unsigned int s_lastTimeNerioDataUpdated; // 0x03017B18
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link == nullptr)
    {
        g_gameLinkMutex.unlock();
        return;
    }
    ISteamMatchmaking* matchmaking = SteamMatchmaking();
    if (matchmaking != nullptr && link->isLobbyOwner())
    {
        const kaiko::GameLinkLobby lobby = link->getLobbyId();
        if (lobby.id.asUint64 != 0)
        {
            const unsigned int now = keen::SystemTimer::getCurrentMilliseconds();
            // Magic number: once a second
            if (now - s_lastTimeNerioDataUpdated > 1000)
            {
                s_lastTimeNerioDataUpdated = now;
                char players[1024];
                memset(players, 0, sizeof(players));
                keen::StringBuilder builder(players, sizeof(players));
                const CSteamID steam_lobby(lobby.id.asUint64);
                const int num_members = matchmaking->GetNumLobbyMembers(steam_lobby);
                for (int i = 0; i < num_members; i++)
                {
                    const CSteamID member = matchmaking->GetLobbyMemberByIndex(steam_lobby, i);
                    builder.appendFormattedString("%llu", member.ConvertToUint64());
                    if (i + 1 >= num_members)
                    {
                        break;
                    }
                    builder.appendCharacter(',');
                }
                matchmaking->SetLobbyData(steam_lobby, "players", players);
            }
        }
    }
    g_gameLinkMutex.unlock();
}

// 0x009DA3A0 (:2028): enter a join state and run it while it completes at once
session_op_status multi_session_base::set_join_state(multi_session_join_state new_state)
{
    m_join_state = new_state;
    bool party = is_party_owner_and_has_other_party_members();
    multi_session_join_state state = m_join_state;
    while (state < NUM_MULTI_SESSION_JOIN_STATES)
    {
        switch (state)
        {
        case MSJ_NOT_JOINING:
            m_join_source = nullptr;
            m_join_invited = false;
            m_join_is_merge = false;
            m_join_players_acked_count = 0;
            m_join_last_status = MSJS_JOIN_ACCEPT;
            m_join_timeout.invalidate();
            return SOS_SUCCESS;
        case MSJ_WAITING_FOR_ACCEPT:
            multi_join_send_join_request_packet(m_host_connection, static_cast<multi_session*>(this), m_joinReservedSlot);
            m_join_players_acked_count = 0;
            m_join_timeout.set(Join_request_timeout);
            return SOS_IN_PROGRESS;
        case MSJ_WAITING_FOR_CLIENTS:
            if (party)
            {
                m_join_players_acked_count = 1;
                m_join_players_acked_uids[0] = My_player_uid;
                // Magic number: 10 s for the party to follow
                m_join_timeout.set(10000);
                return SOS_IN_PROGRESS;
            }
            state = MSJ_JOIN_COMPLETED;
            break;
        case MSJ_WAITING_FOR_HOST:
            m_join_players_acked_count = 0;
            // Magic number: 10 s for the party leader
            m_join_timeout.set(10000);
            return SOS_IN_PROGRESS;
        case MSJ_MERGE_WAITING_FOR_CLIENTS:
            if (party)
            {
                m_join_players_acked_count = 1;
                m_join_players_acked_uids[0] = My_player_uid;
                m_join_timeout.set(10000);
                return SOS_IN_PROGRESS;
            }
            state = MSJ_NOT_JOINING;
            break;
        case MSJ_JOIN_FAILED:
            return SOS_FAILED;
        case MSJ_JOIN_COMPLETED:
            Local_player_voice_info.has_headset_dirty = true;
            multi_game_bw_reset(m_current_info_base->type);
            state = MSJ_NOT_JOINING;
            break;
        default:
            return SOS_FAILED;
        }
        m_join_state = state;
        party = is_party_owner_and_has_other_party_members();
        state = m_join_state;
    }
    return SOS_FAILED;
}

// 0x009E4840 (:2171)
session_op_status multi_session_base::process_migrating()
{
    session_op_status status;
    switch (m_migration_state)
    {
    case MSM_MIGRATING_AUTO:
    case MSM_MIGRATING:
        status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            return set_migration_state(MSM_MIGRATION_SUCCESS);
        }
        if (status != SOS_FAILED)
        {
            return SOS_IN_PROGRESS;
        }
        return set_migration_state(MSM_MIGRATION_FAILED);
    case MSM_BECOMING_HOST:
        status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            return set_migration_state(MSM_WAITING_FOR_CLIENTS);
        }
        if (status != SOS_FAILED)
        {
            return SOS_IN_PROGRESS;
        }
        return set_migration_state(MSM_MIGRATION_FAILED);
    case MSM_WAITING_FOR_HOST:
        if (m_migration_next_host == m_me_connection)
        {
            return set_migration_state(MSM_BECOMING_HOST);
        }
        if (m_migrate_packet_received)
        {
            return set_migration_state(MSM_MIGRATING);
        }
        if (!m_migrate_timeout.elapsed())
        {
            return SOS_IN_PROGRESS;
        }
        return set_migration_state(MSM_MIGRATION_FAILED);
    case MSM_WAITING_FOR_CLIENTS:
        if (all_connections_acked(m_migrate_players_acked_count, m_migrate_players_acked_uids, m_connections, m_host_connection))
        {
            return set_migration_state(MSM_MIGRATION_SUCCESS);
        }
        if (!m_migrate_timeout.elapsed())
        {
            return SOS_IN_PROGRESS;
        }
        return set_migration_state(MSM_MIGRATION_DROPPING_LOST_PEOPLE);
    default:
        return SOS_FAILED;
    }
}

// 0x009DA520 (:2263): enter a migration state and run it while it completes at once
session_op_status multi_session_base::set_migration_state(multi_session_migration_state new_state)
{
    for (;;)
    {
        m_migration_state = new_state;
        // Whether no connection but the host is still connected.
        bool no_peers = true;
        multi_connection* cp = m_connections;
        while (cp != nullptr)
        {
            if (cp != m_host_connection && cp->connection_status != CS_DISCONNECTED)
            {
                no_peers = false;
                break;
            }
            cp = cp->next;
            if (cp == m_connections)
            {
                break;
            }
        }
        switch (new_state)
        {
        case MSM_NOT_MIGRATING:
            return SOS_SUCCESS;
        case MSM_MIGRATING_AUTO:
            return SOS_IN_PROGRESS;
        case MSM_BECOMING_HOST:
        {
            m_desired_info_base->copy(*m_current_info_base);
            m_desired_info_base->host_player_uid = m_me_connection->player_uid;
            m_desired_info_base->dedicated = false;
            m_host_connection = m_me_connection;
            m_host = true;
            const session_op_status status = platform_session_migrate();
            if (status == SOS_SUCCESS)
            {
                new_state = MSM_WAITING_FOR_CLIENTS;
                continue;
            }
            if (status == SOS_FAILED)
            {
                new_state = MSM_MIGRATION_FAILED;
                continue;
            }
            return SOS_IN_PROGRESS;
        }
        case MSM_WAITING_FOR_HOST:
            // Magic number: 25 s for the new host's PT_MIGRATE
            m_migrate_timeout.set(25000);
            if (m_host)
            {
                m_migration_choose_server_timestamp.invalidate();
                m_host = false;
                if (m_current_info_base->type == ST_GAME)
                {
                    Server = false;
                    Dedicated = false;
                }
            }
            return SOS_IN_PROGRESS;
        case MSM_MIGRATING:
            if (!m_migrate_packet_received || m_migrate_target_host == nullptr)
            {
                new_state = MSM_MIGRATION_FAILED;
                continue;
            }
            else
            {
                const session_op_status status = platform_session_migrate();
                if (status == SOS_SUCCESS)
                {
                    new_state = MSM_MIGRATION_SUCCESS;
                    continue;
                }
                if (status != SOS_FAILED)
                {
                    return SOS_IN_PROGRESS;
                }
                new_state = MSM_MIGRATION_FAILED;
                continue;
            }
        case MSM_WAITING_FOR_CLIENTS:
            m_current_info_base->copy(*m_desired_info_base);
            if (m_desired_info_base->type == ST_GAME)
            {
                Server = true;
            }
            if (no_peers)
            {
                new_state = MSM_MIGRATION_SUCCESS;
                continue;
            }
            cp = m_connections;
            while (cp != nullptr)
            {
                if (cp != m_host_connection && cp->connection_status != CS_DISCONNECTED)
                {
                    multi_send_migrate_packet(cp, m_desired_info_base, m_migration_reason);
                }
                cp = cp->next;
                if (cp == m_connections)
                {
                    break;
                }
            }
            m_migrate_players_acked_count = 0;
            // Magic number: 9 s for the clients' PT_MIGRATED
            m_migrate_timeout.set(9000);
            return SOS_IN_PROGRESS;
        case MSM_MIGRATION_SUCCESS:
            if (!m_host)
            {
                m_current_info_base->copy(*m_desired_info_base);
                multi_connection* const new_host = m_migrate_target_host;
                m_host_connection = new_host;
                m_migrate_target_host = nullptr;
                m_migration_next_host = nullptr;
                m_migration_choose_server_timestamp.invalidate();
                multi_send_migrated_packet(new_host);
                m_migrate_packet_received = false;
            }
            else
            {
                m_migration_next_host = nullptr;
                // Magic number: the next host is chosen 10 s after a migration
                m_migration_choose_server_timestamp.set(10000);
                cp = m_connections;
                while (cp != nullptr)
                {
                    multi_join_send_new_connection_packet_to_all(cp);
                    cp = cp->next;
                    if (cp == m_connections)
                    {
                        break;
                    }
                }
            }
            if (m_cb_migration_just_occurred != nullptr)
            {
                m_cb_migration_just_occurred(this);
            }
            new_state = MSM_NOT_MIGRATING;
            continue;
        case MSM_MIGRATION_DROPPING_LOST_PEOPLE:
            cp = m_connections;
            while (cp != nullptr)
            {
                if (cp != m_me_connection && !connection_acked(m_migrate_players_acked_count, m_migrate_players_acked_uids, cp))
                {
                    multi_connection_disconnect(cp, CDR_SESSION_MIGRATE_FAILED);
                }
                cp = cp->next;
                if (cp == m_connections)
                {
                    break;
                }
            }
            new_state = MSM_MIGRATION_SUCCESS;
            continue;
        case MSM_MIGRATION_FAILED:
            if (m_host_connection != nullptr && m_host_connection == m_me_connection)
            {
                m_host = true;
            }
            return SOS_FAILED;
        default:
            return SOS_FAILED;
        }
    }
}

// 0x009E4CF0 (:2443)
session_op_status multi_session_base::process_modification()
{
    switch (m_modification_state)
    {
    case MSD_MODIFYING:
    {
        const session_op_status status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            // m_connections is not checked for null (as in rfg.exe).
            if (m_host && m_connections->next != m_connections)
            {
                return set_modification_state(MSD_WAITING_FOR_CLIENTS);
            }
            return set_modification_state(MSD_MODIFY_SUCCESS);
        }
        if (status != SOS_FAILED)
        {
            return SOS_IN_PROGRESS;
        }
        return set_modification_state(MSD_MODIFY_FAILED);
    }
    case MSD_WAITING_FOR_CLIENTS:
        if (all_connections_acked(m_modification_acked_count, m_modification_acked_uids, m_connections, m_host_connection))
        {
            return set_modification_state(MSD_MODIFY_SUCCESS);
        }
        if (!m_modification_timeout.elapsed())
        {
            return SOS_IN_PROGRESS;
        }
        return set_modification_state(MSD_MODIFY_DROPPING_LOST_PEOPLE);
    default:
        return SOS_FAILED;
    }
}

// 0x009DA870 (:2492): enter a modification state and run it while it completes at once
session_op_status multi_session_base::set_modification_state(multi_session_modification_state new_state)
{
    for (;;)
    {
        m_modification_state = new_state;
        switch (new_state)
        {
        case MSD_NOT_MODIFYING:
            return SOS_SUCCESS;
        case MSD_MODIFYING:
        {
            const session_op_status status = platform_session_modify();
            if (status == SOS_SUCCESS)
            {
                // m_connections is not checked for null (as in rfg.exe).
                new_state = !m_host || m_connections->next == m_connections ? MSD_MODIFY_SUCCESS : MSD_WAITING_FOR_CLIENTS;
            }
            else if (status == SOS_FAILED)
            {
                new_state = MSD_MODIFY_FAILED;
            }
            else
            {
                return SOS_IN_PROGRESS;
            }
            break;
        }
        case MSD_WAITING_FOR_CLIENTS:
        {
            multi_connection* cp = m_connections;
            while (cp != nullptr)
            {
                if (cp != m_me_connection)
                {
                    multi_send_modify_session_packet(cp, m_desired_info_base);
                }
                cp = cp->next;
                if (cp == m_connections)
                {
                    break;
                }
            }
            // Magic number: 15 s for the clients' PT_MODIFIED_SESSION
            m_modification_timeout.set(15000);
            m_modification_acked_count = 0;
            return SOS_IN_PROGRESS;
        }
        case MSD_MODIFY_SUCCESS:
            m_current_info_base->copy(*m_desired_info_base);
            if (!m_host)
            {
                multi_send_modified_session_packet(m_host_connection);
            }
            new_state = MSD_NOT_MODIFYING;
            break;
        case MSD_MODIFY_DROPPING_LOST_PEOPLE:
        {
            multi_connection* cp = m_connections;
            while (cp != nullptr)
            {
                if (cp != m_me_connection && !connection_acked(m_modification_acked_count, m_modification_acked_uids, cp))
                {
                    multi_connection_disconnect(cp, CDR_SESSION_MODIFY_FAILED);
                }
                cp = cp->next;
                if (cp == m_connections)
                {
                    break;
                }
            }
            new_state = MSD_MODIFY_SUCCESS;
            break;
        }
        case MSD_MODIFY_FAILED:
            m_desired_info_base->copy(*m_current_info_base);
            return SOS_FAILED;
        default:
            return SOS_FAILED;
        }
    }
}

// 0x009E4EC0 (:2571)
session_op_status multi_session_base::process_starting()
{
    // m_connections is not checked for null (as in rfg.exe).
    multi_connection* const connections = m_connections;
    const bool alone = connections->next == connections;
    session_op_status status;
    switch (m_starting_state)
    {
    case MSSS_HOST_WAITING_FOR_CLIENTS_TO_REGISTER:
        if (all_connections_acked(m_starting_acked_count, m_starting_acked_uids, connections, m_host_connection))
        {
            return set_starting_state(MSSS_REGISTERING);
        }
        if (!m_starting_timeout.elapsed())
        {
            return SOS_IN_PROGRESS;
        }
        return set_starting_state(MSSS_HOST_DROPPING_NON_REGISTERED_PLAYERS);
    case MSSS_REGISTERING:
        status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            return set_starting_state(m_host ? MSSS_STARTING : MSSS_CLIENT_WAITING_FOR_HOST_TO_SIGNAL_START);
        }
        break;
    case MSSS_CLIENT_WAITING_FOR_HOST_TO_SIGNAL_START:
        if (!m_starting_received_start)
        {
            if (!m_starting_timeout.elapsed())
            {
                return SOS_IN_PROGRESS;
            }
            return set_starting_state(MSSS_FAILED);
        }
        m_starting_received_start = false;
        return set_starting_state(MSSS_STARTING);
    case MSSS_STARTING:
        status = platform_process_operation();
        if (status == SOS_SUCCESS)
        {
            return set_starting_state(!m_host || alone ? MSSS_SUCCESS : MSSS_WAITING_FOR_CLIENTS_TO_START);
        }
        break;
    case MSSS_WAITING_FOR_CLIENTS_TO_START:
        if (all_connections_acked(m_starting_acked_count, m_starting_acked_uids, connections, m_host_connection))
        {
            return set_starting_state(MSSS_SUCCESS);
        }
        if (!m_starting_timeout.elapsed())
        {
            return SOS_IN_PROGRESS;
        }
        return set_starting_state(MSSS_HOST_DROPPING_NON_STARTED_PLAYERS);
    default:
        return SOS_FAILED;
    }
    if (status != SOS_FAILED)
    {
        return SOS_IN_PROGRESS;
    }
    return set_starting_state(MSSS_FAILED);
}

// 0x009DA9F0 (:2666): enter a starting state and run it while it completes at once
session_op_status multi_session_base::set_starting_state(multi_session_starting_state new_state)
{
    for (;;)
    {
        multi_connection* cp = m_connections;
        const bool alone = cp == nullptr || cp->next == cp;
        m_starting_state = new_state;
        switch (new_state)
        {
        case MSSS_HOST_WAITING_FOR_CLIENTS_TO_REGISTER:
            if (!m_current_info_base->uses_abitration)
            {
                return SOS_FAILED;
            }
            if (alone)
            {
                new_state = MSSS_REGISTERING;
                continue;
            }
            m_starting_acked_count = 0;
            // Magic number: 15 s for the clients' PT_REGISTERED
            m_starting_timeout.set(15000);
            cp = m_connections;
            while (cp != nullptr)
            {
                if (cp != m_me_connection)
                {
                    multi_send_register_for_arbitration(cp);
                }
                cp = cp->next;
                if (cp == m_connections)
                {
                    break;
                }
            }
            return SOS_IN_PROGRESS;
        case MSSS_HOST_DROPPING_NON_REGISTERED_PLAYERS:
            while (cp != nullptr)
            {
                if (cp != m_me_connection && !connection_acked(m_starting_acked_count, m_starting_acked_uids, cp))
                {
                    multi_connection_disconnect(cp, CDR_SESSION_ARBITRATE_FAILED);
                }
                cp = cp->next;
                if (cp == m_connections)
                {
                    break;
                }
            }
            new_state = MSSS_REGISTERING;
            continue;
        case MSSS_REGISTERING:
        {
            if (!m_current_info_base->uses_abitration)
            {
                return SOS_FAILED;
            }
            const session_op_status status = platform_session_start_arbitration();
            if (status == SOS_SUCCESS)
            {
                new_state = m_host ? MSSS_STARTING : MSSS_CLIENT_WAITING_FOR_HOST_TO_SIGNAL_START;
            }
            else if (status == SOS_FAILED)
            {
                new_state = MSSS_FAILED;
            }
            else
            {
                return SOS_IN_PROGRESS;
            }
            continue;
        }
        case MSSS_CLIENT_WAITING_FOR_HOST_TO_SIGNAL_START:
            multi_send_registered_for_arbitration(m_host_connection);
            // Magic number: 30 s for the host's PT_START
            m_starting_timeout.set(30000);
            return SOS_IN_PROGRESS;
        case MSSS_STARTING:
        {
            if (!alone)
            {
                m_starting_acked_count = 0;
                if (m_host)
                {
                    while (cp != nullptr)
                    {
                        if (cp != m_me_connection)
                        {
                            multi_send_start(cp);
                        }
                        cp = cp->next;
                        if (cp == m_connections)
                        {
                            break;
                        }
                    }
                }
            }
            const session_op_status status = platform_session_start_gameplay();
            if (status == SOS_SUCCESS)
            {
                new_state = !m_host || alone ? MSSS_SUCCESS : MSSS_WAITING_FOR_CLIENTS_TO_START;
            }
            else if (status == SOS_FAILED)
            {
                new_state = MSSS_FAILED;
            }
            else
            {
                return SOS_IN_PROGRESS;
            }
            continue;
        }
        case MSSS_WAITING_FOR_CLIENTS_TO_START:
            if (alone)
            {
                return SOS_FAILED;
            }
            // Magic number: 15 s for the clients' PT_STARTED
            m_starting_timeout.set(15000);
            return SOS_IN_PROGRESS;
        case MSSS_HOST_DROPPING_NON_STARTED_PLAYERS:
            while (cp != nullptr)
            {
                if (cp != m_me_connection && !connection_acked(m_starting_acked_count, m_starting_acked_uids, cp))
                {
                    multi_connection_disconnect(cp, CDR_SESSION_START_FAILED);
                }
                cp = cp->next;
                if (cp == m_connections)
                {
                    break;
                }
            }
            new_state = MSSS_SUCCESS;
            continue;
        case MSSS_SUCCESS:
            if (!m_host)
            {
                multi_send_started(m_host_connection);
            }
            return SOS_SUCCESS;
        default:
            return SOS_FAILED;
        }
    }
}

// 0x0094B2D0 (:2798)
multi_session_state multi_session_base::get_state() const
{
    return m_state;
}

// 0x0094B2E0 (:2803)
session_migration_reason multi_session_base::get_migration_reason() const
{
    return m_migration_reason;
}

// 0x0094B2F0 (:2808)
multi_session_creation_state multi_session_base::get_create_state() const
{
    return m_create_state;
}

// 0x0094B300 (:2813)
multi_session_migration_state multi_session_base::get_migration_state() const
{
    return m_migration_state;
}

// 0x0094B310 (:2818)
multi_session_join_state multi_session_base::get_join_state() const
{
    return m_join_state;
}

// 0x0094B320 (:2823)
multi_session_modification_state multi_session_base::get_modification_state() const
{
    return m_modification_state;
}

// 0x0094B330 (:2828)
void multi_session_base::register_migration_callbacks(bool (*migrate_allow)(multi_session_base*), void (*migration_starting)(multi_session_base*),
                                                      void (*migration_ended)(multi_session_base*), void (*migration_failed)(multi_session_base*))
{
    m_cb_migarate_allow_migrate = migrate_allow;
    m_cb_migrate_about_to_start = migration_starting;
    m_cb_migration_just_occurred = migration_ended;
    m_cb_migration_failed = migration_failed;
}

// 0x0094B360 (:2836)
void multi_session_base::register_join_callbacks(multi_session_join_status (*player_may_join)(multi_connection*, int), void (*player_joined)(multi_connection*),
                                                 void (*player_about_to_leave)(multi_connection*), void (*player_left)(multi_connection*))
{
    m_cb_player_may_join = player_may_join;
    m_cb_player_joined = player_joined;
    m_cb_player_about_to_leave = player_about_to_leave;
    m_cb_player_left = player_left;
}

// 0x0094B390 (:2844)
void multi_session_base::register_event_callbacks(void (*send_join_new_session)(unsigned char*, unsigned short&, const multi_session_info_base*),
                                                  void (*recv_join_new_session)(unsigned char*, int&, const multi_session_info_base*))
{
    m_cb_send_join_new_session = send_join_new_session;
    m_cb_recv_join_new_session = recv_join_new_session;
}

// 0x00973190 (:2850): reserve slots for the reservees not connected yet (all or none)
bool multi_session_base::reserve_slots(int num_slots, const multi_player_uid& reserver, const multi_player_uid* reservees)
{
    // Magic number: 1 to 15 reservees (the reserver holds a slot itself)
    if ((unsigned int)(num_slots - 1) > 14)
    {
        return false;
    }
    bool needs_slot[Max_session_slots];
    int num_new = 0;
    for (int i = 0; i < num_slots; i++)
    {
        needs_slot[i] = false;
        bool connected = false;
        multi_connection* cp = m_connections;
        while (cp != nullptr)
        {
            if (cp->player_uid.is_equal(reservees[i]))
            {
                connected = true;
                break;
            }
            cp = cp->next;
            if (cp == m_connections)
            {
                break;
            }
        }
        if (!connected)
        {
            num_new++;
            needs_slot[i] = true;
        }
    }
    if (m_slots.used + m_slots.reserved + num_new > m_current_info_base->max_slots)
    {
        return false;
    }
    for (int i = 0; i < num_slots; i++)
    {
        if (needs_slot[i])
        {
            m_slots.reservations[m_slots.reserved].reserver = reserver;
            m_slots.reservations[m_slots.reserved].reservee = reservees[i];
            m_slots.reserved++;
        }
    }
    return true;
}

// 0x009732E0 (:2895): the last reservation fills the gap
void multi_session_base::remove_reservation(int reservation_idx)
{
    m_slots.reserved--;
    if (m_slots.reserved != reservation_idx)
    {
        m_slots.reservations[reservation_idx].reserver = m_slots.reservations[m_slots.reserved].reserver;
        m_slots.reservations[reservation_idx].reservee = m_slots.reservations[m_slots.reserved].reservee;
    }
}

// 0x00973350 (:2907): drop the reservations held by or for the player
void multi_session_base::unreserve_slots(const multi_player_uid& uid)
{
    for (int i = m_slots.reserved - 1; i >= 0; i--)
    {
        if (m_slots.reservations[i].reservee.is_equal(uid) || m_slots.reservations[i].reserver.is_equal(uid))
        {
            remove_reservation(i);
        }
    }
}

// 0x0094B3B0 (:2917)
int multi_session_base::get_slots_reserved() const
{
    return m_slots.reserved;
}

// 0x0094B3C0 (:2922)
int multi_session_base::get_slots_used() const
{
    return m_slots.used;
}

// 0x00973400 (:2927): the connected players (with their reservers as group) then the reservees (with theirs).
// max_slots is not checked (as in rfg.exe).
int multi_session_base::get_used_slot_information(int* num_used_slots, multi_player_uid* slot_player_uids, multi_player_uid* slot_group_uids, const int max_slots) const
{
    int count = 0;
    multi_connection* cp = m_connections;
    while (cp != nullptr)
    {
        slot_player_uids[count] = cp->player_uid;
        slot_group_uids[count] = cp->reserver_uid;
        cp = cp->next;
        count++;
        if (cp == m_connections)
        {
            break;
        }
    }
    for (int i = 0; i < m_slots.reserved; i++)
    {
        slot_player_uids[count] = m_slots.reservations[i].reservee;
        slot_group_uids[count] = m_slots.reservations[i].reserver;
        count++;
    }
    *num_used_slots = count;
    return m_slots.used + m_slots.reserved;
}

// 0x009AA3A0 (:2957): admit a connection (the host checks busy states, reservations and free slots, then the game's
// may-join callback), give it the first free index and put it at the head of the list
multi_session_join_status multi_session_base::add_connection(multi_connection* cp, unsigned char manually_assigned_index, int reservations)
{
    int reservation = -1;
    if (m_host && cp != m_host_connection)
    {
        switch (m_state)
        {
        case MSS_CREATING:
        case MSS_JOINING:
        case MSS_REGISTERING:
        case MSS_STARTING:
        case MSS_ENDING:
        case MSS_DELETING:
        case MSS_MIGRATING:
        case MSS_MODIFYING:
            if ((m_state != MSS_CREATING || m_create_state != MSC_WAITING_FOR_CLIENTS) && (m_state != MSS_JOINING || m_join_state != MSJ_WAITING_FOR_CLIENTS))
            {
                return MSJS_JOIN_DENY_BUSY;
            }
            break;
        default:
            break;
        }
        for (int i = m_slots.reserved - 1; i >= 0; i--)
        {
            if (m_slots.reservations[i].reservee.is_equal(cp->player_uid))
            {
                reservation = i;
                break;
            }
        }
        if (reservation != -1)
        {
            if (m_locked)
            {
                return MSJS_JOIN_DENY_SESSION_LOCKED;
            }
        }
        else if (m_current_info_base->max_slots < m_slots.used + 1 + m_slots.reserved)
        {
            return MSJS_JOIN_DENY_SESSION_FULL;
        }
    }
    if (cp != m_host_connection && cp != m_me_connection && m_cb_player_may_join != nullptr)
    {
        const multi_session_join_status status = m_cb_player_may_join(cp, reservations);
        if (status != MSJS_JOIN_ACCEPT)
        {
            return status;
        }
    }
    if (reservation != -1)
    {
        cp->reserver_uid = m_slots.reservations[reservation].reserver;
        remove_reservation(reservation);
    }
    if (manually_assigned_index == 0xFF)
    {
        // rfg.exe marks the indices in a 16-byte array, so an index of 0xFF would land past it on the stack; the
        // port marks into 256 entries.
        unsigned char used[256];
        memset(used, 0, sizeof(used));
        multi_connection* other = m_connections;
        while (other != nullptr)
        {
            used[other->index_in_session] = 1;
            other = other->next;
            if (other == m_connections)
            {
                break;
            }
        }
        cp->index_in_session = 0xFF;
        for (unsigned int index = 0; index < (unsigned int)Max_session_slots; index++)
        {
            if (used[index] == 0)
            {
                cp->index_in_session = (unsigned char)index;
                break;
            }
        }
    }
    else
    {
        cp->index_in_session = manually_assigned_index;
    }
    if (platform_add_connection(cp) != SOS_SUCCESS)
    {
        if (manually_assigned_index == 0xFF)
        {
            return MSJS_JOIN_DENY_INTERNAL_ERROR;
        }
        multi_connection_disconnect(cp, CDR_FAILED_PLATFORM_ADD);
    }
    if (m_connections == nullptr)
    {
        cp->next = cp;
        cp->prev = cp;
    }
    else
    {
        m_connections->prev->next = cp;
        cp->prev = m_connections->prev;
        m_connections->prev = cp;
        cp->next = m_connections;
    }
    m_slots.used++;
    m_connections = cp;
    if (m_host && cp != m_host_connection)
    {
        // Magic number: the next host is chosen 10 s after the connection list changed
        m_migration_choose_server_timestamp.set(10000);
    }
    if (m_host || (cp != m_host_connection && m_host_connection->registered))
    {
        register_connection(cp);
    }
    return MSJS_JOIN_ACCEPT;
}

// 0x00990CB0 (:3093): the game sees the player join; registering the host registers everyone (a client registers
// the connections once it was accepted)
void multi_session_base::register_connection(multi_connection* cp)
{
    if (cp->registered)
    {
        return;
    }
    cp->registered = true;
    if (cp != m_me_connection)
    {
        multi_voice_register_talker(cp);
    }
    if (m_cb_player_joined != nullptr)
    {
        m_cb_player_joined(cp);
    }
    if (cp == m_host_connection)
    {
        multi_connection* other = m_connections;
        while (other != nullptr)
        {
            if (!other->registered)
            {
                register_connection(other);
            }
            other = other->next;
            if (other == m_connections)
            {
                break;
            }
        }
    }
}

// 0x009DAC80 (:3122): unlink and free a connection; the host says goodbye to it and tells the others
void multi_session_base::remove_connection(multi_connection* cp)
{
    if (cp == nullptr)
    {
        keen::debug::nativePrintArgs("[multi_session_base] tried to remove nullptr connection!\n");
        return;
    }
    if (m_cb_player_about_to_leave != nullptr)
    {
        m_cb_player_about_to_leave(cp);
    }
    multi_connection* other = m_connections;
    while (other != nullptr)
    {
        other->cannot_serve &= ~(1 << (cp->index_in_session & 0x1F));
        other = other->next;
        if (other == m_connections)
        {
            break;
        }
    }
    if (m_connections == cp && cp->next == cp)
    {
        cp->next = nullptr;
        cp->prev = nullptr;
        m_connections = nullptr;
    }
    else
    {
        if (m_connections == cp)
        {
            m_connections = cp->next;
        }
        if (cp->prev != nullptr)
        {
            cp->prev->next = cp->next;
        }
        if (cp->next != nullptr)
        {
            cp->next->prev = cp->prev;
        }
        cp->next = nullptr;
        cp->prev = nullptr;
    }
    m_slots.used--;
    multi_connection* const me = m_me_connection;
    if (cp == m_host_connection && cp == me)
    {
        multi_leave_send_connection_left_packet_to_all(cp);
        multi_io_force_send_all_buffered_packets();
        platform_remove_connection(cp);
        m_me_connection = nullptr;
        m_host_connection = nullptr;
    }
    else
    {
        if (m_host && cp != me && cp->connection_status != CS_DISCONNECTED)
        {
            multi_send_goodbye(cp);
        }
        session_connection_dropped(cp);
        if (m_migration_next_host == cp)
        {
            m_migration_next_host = nullptr;
            m_migration_choose_server_timestamp.set(0);
        }
        if (m_host)
        {
            multi_leave_send_connection_left_packet_to_all(cp);
        }
        platform_remove_connection(cp);
        if (cp == m_me_connection)
        {
            m_me_connection = nullptr;
        }
        else if (cp == m_host_connection)
        {
            m_host_connection = nullptr;
        }
        else if (cp == m_migrate_target_host)
        {
            set_migration_state(MSM_MIGRATION_FAILED);
        }
    }
    unreserve_slots(cp->player_uid);
    if (cp->registered)
    {
        if (m_cb_player_left != nullptr)
        {
            m_cb_player_left(cp);
        }
        if (cp != me)
        {
            multi_voice_remove_talker(cp);
        }
        cp->registered = false;
    }
    cp->index_in_session = 0xFF;
    multi_connection_delete(cp);
}

// 0x00990D20 (:3209)
void multi_session_base::unregister_connection(multi_connection* cp, bool unregistering_me_connection)
{
    if (cp->registered)
    {
        if (m_cb_player_left != nullptr)
        {
            m_cb_player_left(cp);
        }
        if (!unregistering_me_connection)
        {
            multi_voice_remove_talker(cp);
        }
        cp->registered = false;
    }
}

// 0x0094B3D0 (:3224)
bool multi_session_base::is_connected() const
{
    switch (m_state)
    {
    case MSS_READY:
    case MSS_REGISTERING:
    case MSS_REGISTERED:
    case MSS_STARTING:
    case MSS_IN_GAME:
    case MSS_ENDING:
    case MSS_ENDED:
    case MSS_MIGRATING:
    case MSS_MODIFYING:
        return true;
    default:
        return false;
    }
}

// 0x0094B410 (:3255)
bool multi_session_base::am_i_host() const
{
    return m_host;
}

// 0x0094B420 (:3260)
bool multi_session_base::am_i_next_host() const
{
    return m_migration_next_host == m_me_connection;
}

// 0x0094B440 (:3265)
const multi_session_info_base* multi_session_base::get_info() const
{
    return m_current_info_base;
}

// 0x0094B450 (:3270)
multi_connection* multi_session_base::get_connection_list() const
{
    return m_connections;
}

// 0x0094B460 (:3275)
multi_connection* multi_session_base::get_host_connection() const
{
    return m_host_connection;
}

// 0x0094B470 (:3280)
multi_connection* multi_session_base::get_me_connection() const
{
    return m_me_connection;
}

// 0x0094B480 (:3285)
multi_connection* multi_session_base::get_next_host_connection() const
{
    return m_migration_next_host;
}

// 0x00973500 (:3291)
multi_connection* multi_session_base::get_player_connection(const multi_player_uid& uid) const
{
    multi_connection* cp = m_connections;
    while (cp != nullptr)
    {
        if (cp->player_uid.is_equal(uid))
        {
            return cp;
        }
        cp = cp->next;
        if (cp == m_connections)
        {
            break;
        }
    }
    return nullptr;
}

// 0x0094B490 (:3302)
multi_connection* multi_session_base::get_player_connection(unsigned int multi_game_data_handle) const
{
    multi_connection* cp = m_connections;
    while (cp != nullptr)
    {
        if (cp->game_data == multi_game_data_handle)
        {
            return cp;
        }
        cp = cp->next;
        if (cp == m_connections)
        {
            break;
        }
    }
    return nullptr;
}

// 0x0094B4C0 (:3313)
multi_connection* multi_session_base::get_connection_by_index(unsigned char index) const
{
    multi_connection* cp = m_connections;
    while (cp != nullptr)
    {
        if (cp->index_in_session == index)
        {
            return cp;
        }
        cp = cp->next;
        if (cp == m_connections)
        {
            break;
        }
    }
    return nullptr;
}

// 0x0094B4F0 (:3324)
multi_session_join_status multi_session_base::get_last_join_status() const
{
    return m_join_last_status;
}

// 0x0094B500 (:3337)
int multi_session_base::compare_by_id(const multi_session_info_base* info)
{
    return platform_compare_by_id(info);
}

// 0x009DAEA0 (:3342): a peer left; a host connection is only marked (process() then migrates)
void multi_session_base::received_connection_left(multi_connection* cp)
{
    if (cp == nullptr)
    {
        keen::debug::nativePrintArgs("[multi_session_base] received_connection_left: cp == nullptr\n");
        return;
    }
    if (cp != m_host_connection)
    {
        remove_connection(cp);
        return;
    }
    multi_connection_disconnect(cp, CDR_SESSION_CONNECTION_LEFT);
}

// 0x009AA680 (:3358)
void multi_session_base::received_new_connection(multi_connection* cp)
{
    add_connection(cp, cp->index_in_session, 0);
}

// 0x009FBFE0 (:3363): the host accepted us; process_joining sees it in the acked list
void multi_session_base::join_received_join_accept(multi_connection* cp)
{
    if (m_join_state == MSJ_WAITING_FOR_ACCEPT)
    {
        m_join_players_acked_uids[m_join_players_acked_count] = cp->player_uid;
        m_join_players_acked_count++;
        register_connection(m_host_connection);
        process_state();
    }
}

// 0x0094B510 (:3376): process_joining fails the join on the invalid timeout
void multi_session_base::join_received_join_denied(multi_connection* cp, multi_session_join_status reason)
{
    if (m_join_state == MSJ_WAITING_FOR_ACCEPT)
    {
        m_join_timeout.invalidate();
        m_join_last_status = reason;
    }
}

// 0x0094B530 (:3387)
void multi_session_base::join_received_join_success(multi_connection* cp)
{
}

// 0x0094B540 (:3405)
void multi_session_base::join_received_join_failed(multi_connection* cp)
{
}

// 0x0094B550 (:3417)
void multi_session_base::join_received_join_complete(multi_connection* cp)
{
}

// 0x0094B560 (:3430)
void multi_session_base::join_received_merged(multi_connection* cp)
{
}

// 0x0094B570 (:3442)
void multi_session_base::join_sending_join_new_target(unsigned char* packet_data, unsigned short& packet_size, const multi_session_info_base* target_session_info)
{
    if (m_cb_send_join_new_session != nullptr)
    {
        m_cb_send_join_new_session(packet_data, packet_size, target_session_info);
    }
}

// 0x0094B5A0 (:3451)
void multi_session_base::join_received_join_new_session(unsigned char* packet_data, int& packet_offset, multi_connection* cp, const multi_session_info_base* session_info)
{
    if (m_join_state == MSJ_NOT_JOINING && m_cb_recv_join_new_session != nullptr)
    {
        m_cb_recv_join_new_session(packet_data, packet_offset, session_info);
    }
}

// 0x009E5280 (:3461)
void multi_session_base::join_received_info_response(const multi_session_info_base* responded_info)
{
    if (m_state == MSS_CREATING && m_create_state == MSC_WAITING_FOR_INFO)
    {
        // Original behaviour kept: a response without info passes nullptr here (copy dereferences it).
        m_desired_info_base->copy(*responded_info);
        set_creation_state(MSC_CREATE_SUCCESS);
    }
}

// 0x0094B5D0 (:3471)
void multi_session_base::join_lost_connection(multi_connection* join_source_cp)
{
}

// 0x0094B5E0 (:3480)
void multi_session_base::join_added_connection(multi_connection* cp)
{
}

// 0x009F7C20 (:3498): we are the requested new host
void multi_session_base::migrate_received_force_migrate(multi_connection* next_host, session_migration_reason reason)
{
    m_migration_next_host = next_host;
    m_migrate_target_host = m_me_connection;
    m_migration_reason = reason;
    set_state(MSS_MIGRATING);
}

// 0x00973540 (:3507): the new host announced itself; process_state starts migrating
void multi_session_base::migrate_received_migrate(multi_connection* cp, const multi_session_info_base* migration_info, session_migration_reason reason)
{
    if (migration_info->host_player_uid.is_equal(cp->player_uid))
    {
        m_desired_info_base->copy(*migration_info);
        m_migrate_packet_received = true;
        m_migrate_target_host = cp;
        m_migration_next_host = nullptr;
        m_migration_reason = reason;
    }
}

// 0x00973590 (:3521)
void multi_session_base::migrate_received_migrated(multi_connection* cp)
{
    if (m_migration_state == MSM_WAITING_FOR_CLIENTS || m_migration_state == MSM_MIGRATING_AUTO)
    {
        m_migrate_players_acked_uids[m_migrate_players_acked_count] = cp->player_uid;
        m_migrate_players_acked_count++;
    }
}

// 0x0094B5F0 (:3533)
void multi_session_base::migrate_received_next_server(multi_connection* cp)
{
    m_migration_next_host = m_me_connection;
}

// 0x0094B600 (:3542)
void multi_session_base::modify_received_modify(const multi_session_info_base* modification_info)
{
    keen::debug::nativePrintArgs("multi_session_base::session_modify() is not supported anymore!\n");
}

// 0x009735E0 (:3549)
void multi_session_base::modify_received_modified(multi_connection* cp)
{
    if (m_modification_state == MSD_WAITING_FOR_CLIENTS)
    {
        m_modification_acked_uids[m_modification_acked_count] = cp->player_uid;
        m_modification_acked_count++;
    }
}

// 0x009B6BE0 (:3559)
void multi_session_base::starting_received_register(unsigned char* packet_data, multi_connection* cp)
{
    int packet_offset = 0;
    m_current_info_base->read_arbitration_info_from_packet(packet_data, packet_offset);
    session_start_gameplay();
}

// 0x00973620 (:3566)
void multi_session_base::starting_received_registered(multi_connection* cp)
{
    if (m_starting_state == MSSS_HOST_WAITING_FOR_CLIENTS_TO_REGISTER)
    {
        m_starting_acked_uids[m_starting_acked_count] = cp->player_uid;
        m_starting_acked_count++;
    }
}

// 0x00973660 (:3576)
void multi_session_base::starting_received_started(multi_connection* cp)
{
    if (m_starting_state == MSSS_STARTING || m_starting_state == MSSS_WAITING_FOR_CLIENTS_TO_START)
    {
        m_starting_acked_uids[m_starting_acked_count] = cp->player_uid;
        m_starting_acked_count++;
    }
}

// 0x0094B610 (:3586). LTCG: state in EAX.
static bool state_allows_us_to_futs_with_connection_list(multi_session_state state)
{
    switch (state)
    {
    case MSS_CREATING:
    case MSS_JOINING:
    case MSS_ENDING:
    case MSS_DELETING:
    case MSS_MIGRATING:
        return false;
    default:
        return true;
    }
}

// 0x009736B0 (:3609): the host of a running match or of a lobby counting down
bool multi_session_base::is_allowed_to_change_lock_state()
{
    return m_host && ((gameseq_state_is_active(GS_GAMEPLAY) && m_state == MSS_IN_GAME) || Lobby_state == MGLSM_GAME_COUNTDOWN);
}

// 0x009736F0 (:3629): a match takes joiners in gameplay (destruction can be synced, no migration, over 2 minutes
// left) while the playlist has room; without the playlist, only a party lobby
bool multi_session_base::does_game_state_allow_joining()
{
    // Magic number: no joins in the last 2 minutes of a match.
    if (Multi_match_info.match_state != MGMS_GAMEPLAY || !jip_destruction_sync_possible || Migration_state != MIGRATION_STATE_NONE ||
        (Timer_time.valid() && Timer_time.time_until() < 120000))
    {
        return false;
    }
    multi_game_playlist* playlist = Playlists;
    while (playlist != nullptr)
    {
        if (playlist->playlist_id.get_crc() == Current_user_playlist)
        {
            return m_slots.used < playlist->max_players;
        }
        playlist = playlist->next;
        if (playlist == Playlists)
        {
            break;
        }
    }
    return Lobby_session->get_info()->party_lobby;
}

// 0x00973770 (:3667)
void multi_session_base::update_lock_state(bool ignoreCurrentLockState)
{
    if (m_host && ((gameseq_state_is_active(GS_GAMEPLAY) && m_state == MSS_IN_GAME) || Lobby_state == MGLSM_GAME_COUNTDOWN))
    {
        if ((m_locked || ignoreCurrentLockState) && does_game_state_allow_joining())
        {
            platform_session_unlock(true);
        }
        if ((!m_locked || ignoreCurrentLockState) && !does_game_state_allow_joining())
        {
            platform_session_lock();
        }
    }
}

// 0x009FC040 (:3702): once a frame: the platform session, the lock, the lobby checks; then (outside the busy states)
// every peer: a lost host (or one gone from the GameLink lobby) migrates or deletes the session, other lost peers are
// removed, peers gone from the lobby for 5 s are dropped, and the rest get a heartbeat every 2 s; then the state step
void multi_session_base::process()
{
    platform_session_update();
    update_lock_state(false);
    check_modified_table_hash();
    disable_party_if_solo_playlist();
    set_extra_lobby_data_for_nerio();
    Live_server_browser.updateLobbyDataAsHost();
    Live_server_browser.updateInCustomMatchStatus();
    multi_connection* cp = m_connections;
    while (cp != nullptr)
    {
        if (cp->connection_status == CS_PENDING)
        {
            cp->connection_status = CS_CONNECTED;
        }
        cp = cp->next;
        if (cp == m_connections)
        {
            break;
        }
    }
    if (state_allows_us_to_futs_with_connection_list(m_state))
    {
        const int now = timer_get(TIMER_MILLISECONDS);
        multi_connection* next = m_connections;
        while (next != nullptr)
        {
            cp = next;
            next = cp->next != m_connections ? cp->next : nullptr;
            if (cp == m_me_connection)
            {
                continue;
            }
            if (!m_host && cp == m_host_connection && !multi_connection_test_connection(cp))
            {
                g_gameLinkMutex.lock(0);
                Remote_game_session.getGameLinkSession()->setPartyEnabled(false);
                session_delete();
                g_gameLinkMutex.unlock();
                process_state();
                return;
            }
            if (cp->connection_status == CS_DISCONNECTED)
            {
                if (cp == m_host_connection)
                {
                    g_gameLinkMutex.lock(0);
                    const kaiko::GameLinkState link_state = Remote_game_session.getGameLinkSession()->getCurrentState();
                    g_gameLinkMutex.unlock();
                    if (link_state != kaiko::GameLinkState_InLobby)
                    {
                        session_delete();
                        break;
                    }
                    if (m_state == MSS_READY || m_state == MSS_IN_GAME)
                    {
                        m_migration_reason = SMR_HOST_LOST;
                        if (cp->disconnect_reason == CDR_SESSION_CONNECTION_LEFT || cp->disconnect_reason == CDR_TEST_ADDRESS_FAILED)
                        {
                            m_migration_reason = SMR_HOST_LEFT;
                        }
                        set_state(MSS_MIGRATING);
                        if (m_state == MSS_NONE)
                        {
                            return;
                        }
                    }
                }
                else
                {
                    remove_connection(cp);
                }
                continue;
            }
            if (!multi_connection_is_connected(cp))
            {
                continue;
            }
            g_gameLinkMutex.lock(0);
            kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
            if (link != nullptr)
            {
                // Magic number: a peer gone from the GameLink lobby for 5 s after joining is dropped
                if (link->getRemoteStationById(cp->addr.connection_id) == nullptr && now - cp->join_time > 5000)
                {
                    multi_connection_disconnect(cp, CDR_SESSION_CONNECTION_LEFT);
                    g_gameLinkMutex.unlock();
                    continue;
                }
                if (!cp->peer_heartbeat_timer.valid() || cp->peer_heartbeat_timer.elapsed())
                {
                    unsigned char packet[1200];
                    packet[0] = PT_HEARTBEAT;
                    const unsigned int size = Multi_packets[PT_HEARTBEAT].group_id != 0 ? 5 : 3;
                    packet[1] = (unsigned char)((size - 3) >> 8);
                    packet[2] = (unsigned char)(size - 3);
                    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
                    // Magic number: a heartbeat every 2 s
                    cp->peer_heartbeat_timer.set(2000);
                }
            }
            g_gameLinkMutex.unlock();
        }
    }
    process_state();
}

// 0x0094B650 (:3986)
void multi_session_base::force_migration_host_update()
{
    m_migration_choose_server_timestamp.set(1000);
}

// ---- session packets

// 0x009AA6A0 (:3995): PT_GOODBYE, sent at once (the send job is held off while the packet is flushed)
void multi_send_goodbye(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_GOODBYE;
    const unsigned int size = Multi_packets[PT_GOODBYE].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send(cp, packet, size);
    if (MP_send_process_trigger != nullptr)
    {
        scheduler_relinquish_cpu(MP_send_process_trigger, ccjt_any);
        MP_send_process_trigger = nullptr;
    }
    multi_connection_flush_current_packet_no_sync(cp);
    Multi_connections_acquired = 0;
}

// 0x0094B670 (:4004)
void multi_process_goodbye(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    if (cp != nullptr)
    {
        multi_connection_disconnect(cp, CDR_SESSION_CONNECTION_LEFT);
    }
}

// 0x009AA730 (:4012): PT_DATA_REQUEST: the session id
void multi_session_send_data_request(multi_connection* cp, const multi_session_info_base* session_info)
{
    unsigned char packet[1200];
    packet[0] = PT_DATA_REQUEST;
    unsigned short size = Multi_packets[PT_DATA_REQUEST].group_id != 0 ? 5 : 3;
    session_info->append_id_to_packet(packet, size);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send(cp, packet, size);
}

// 0x009DAEE0 (:4021): answer with the session's join info (to the sender's address, outside any session)
void multi_session_process_data_request(unsigned char* packet_data, const sender& from)
{
    connection_address address;
    memmove(&address, from.address, sizeof(address));
    multi_session_info request_info;
    int packet_offset = 0;
    request_info.read_id_from_packet(packet_data, packet_offset);
    multi_session* sp = multi_session_find_by_id(request_info);
    const multi_session_info_base* response_info = sp != nullptr ? sp->get_info() : nullptr;
    multi_connection* cp = multi_connection_allocate(&address, nullptr, nullptr, nullptr, false, false);
    multi_session_send_data_response(cp, &request_info, response_info);
    if (MP_send_process_trigger != nullptr)
    {
        scheduler_relinquish_cpu(MP_send_process_trigger, ccjt_any);
        MP_send_process_trigger = nullptr;
    }
    multi_connection_flush_current_packet_no_sync(cp);
    Multi_connections_acquired = 0;
    multi_connection_delete(cp);
}

// 0x009B6C20 (:4043): PT_DATA_RESPONSE: the requested id, a found flag, the join info
void multi_session_send_data_response(multi_connection* cp, const multi_session_info_base* request_session_info, const multi_session_info_base* response_session_info)
{
    if (request_session_info == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_DATA_RESPONSE;
    unsigned short size = Multi_packets[PT_DATA_RESPONSE].group_id != 0 ? 5 : 3;
    request_session_info->append_id_to_packet(packet, size);
    if (response_session_info == nullptr)
    {
        packet[size++] = 0;
    }
    else
    {
        packet[size++] = 1;
        response_session_info->append_join_info_to_packet(packet, size);
    }
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009E52B0 (:4061)
void multi_session_process_data_response(unsigned char* packet_data, const sender& from)
{
    multi_session_info info;
    int packet_offset = 0;
    info.read_id_from_packet(packet_data, packet_offset);
    multi_session* sp = multi_session_find_by_id(info);
    if (sp == nullptr)
    {
        return;
    }
    const bool found = packet_data[packet_offset++] != 0;
    if (found)
    {
        info.read_join_info_from_packet(packet_data, packet_offset);
    }
    sp->join_received_info_response(found ? &info : nullptr);
}

// 0x009B6D00 (:4082): PT_FORCE_MIGRATE (host only): the next host's index (0xFF: none) and the reason
void multi_send_force_migrate_packet(multi_connection* new_host, multi_connection* next_host, session_migration_reason reason)
{
    if (!new_host->owner->am_i_host())
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_FORCE_MIGRATE;
    const unsigned int header = Multi_packets[PT_FORCE_MIGRATE].group_id != 0 ? 5 : 3;
    packet[header] = next_host != nullptr ? next_host->index_in_session : 0xFF;
    packet[header + 1] = (unsigned char)reason;
    const unsigned int size = header + 2;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(new_host, packet, size, nullptr, nullptr, nullptr);
}

// 0x009F7C50 (:4098)
void multi_process_force_migrate_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr)
    {
        return;
    }
    multi_session* sp = from.connection->owner;
    if (sp == nullptr)
    {
        return;
    }
    const session_migration_reason reason = (session_migration_reason)packet_data[1];
    multi_connection* next_host = nullptr;
    if (packet_data[0] != 0xFF)
    {
        next_host = sp->get_connection_by_index(packet_data[0]);
        if (next_host == nullptr)
        {
            return;
        }
    }
    sp->migrate_received_force_migrate(next_host, reason);
}

// 0x009B6DC0 (:4119): PT_MIGRATE from the new host: the session id, our index, nonce and upstream (big endian), our
// uid, our full game data (in multiplayer), the migration info, the reason
void multi_send_migrate_packet(multi_connection* cp, const multi_session_info_base* migrated_info, session_migration_reason reason)
{
    multi_connection* me = cp->owner->get_me_connection();
    unsigned char packet[1200];
    packet[0] = PT_MIGRATE;
    unsigned short size = Multi_packets[PT_MIGRATE].group_id != 0 ? 5 : 3;
    migrated_info->append_id_to_packet(packet, size);
    packet[size] = me->index_in_session;
    const unsigned int nonce = (unsigned int)me->local_nonce;
    packet[size + 1] = (unsigned char)(nonce >> 24);
    packet[size + 2] = (unsigned char)(nonce >> 16);
    packet[size + 3] = (unsigned char)(nonce >> 8);
    packet[size + 4] = (unsigned char)nonce;
    const unsigned int upstream = me->upstream_bits_sec;
    packet[size + 5] = (unsigned char)(upstream >> 24);
    packet[size + 6] = (unsigned char)(upstream >> 16);
    packet[size + 7] = (unsigned char)(upstream >> 8);
    packet[size + 8] = (unsigned char)upstream;
    size = (unsigned short)(size + 9);
    me->player_uid.write_to_packet(packet, size);
    const unsigned int handle = me->game_data;
    if (Multiplayer && handle != 0xFFFFFFFFu)
    {
        multi_game_data* data = &Mp_game_data->data[handle & 0xFFFF][handle >> 16];
        append_player_data_full(packet, size, data, data->handle);
    }
    migrated_info->append_migration_info_to_packet(packet, size);
    packet[size++] = (unsigned char)reason;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009B6FA0 (:4136): the new host's PT_MIGRATE; from an unknown address it is added to the session first. The game
// data is read whether it was sent or not (as in rfg.exe: always multiplayer).
void multi_process_migrate_packet(unsigned char* packet_data, const sender& from)
{
    multi_session_info info;
    int packet_offset = 0;
    info.read_id_from_packet(packet_data, packet_offset);
    multi_connection* cp = from.connection;
    const bool new_connection = cp == nullptr;
    if (new_connection)
    {
        multi_session* sp = multi_session_find_by_id(info);
        if (sp == nullptr)
        {
            return;
        }
        connection_address address;
        address.copy(*from.address);
        cp = multi_connection_allocate(&address, sp, nullptr, nullptr, true, false);
        if (cp == nullptr)
        {
            return;
        }
    }
    cp->index_in_session = packet_data[8];
    cp->local_nonce = (int)(((unsigned int)packet_data[9] << 24) | ((unsigned int)packet_data[10] << 16) | ((unsigned int)packet_data[11] << 8) | packet_data[12]);
    cp->upstream_bits_sec = ((unsigned int)packet_data[13] << 24) | ((unsigned int)packet_data[14] << 16) | ((unsigned int)packet_data[15] << 8) | packet_data[16];
    // Magic number: past the 8-byte session id, the index, the nonce and the upstream
    packet_offset = 17;
    cp->player_uid.read_from_packet(packet_data, packet_offset);
    cp->game_data = multi_game_data_update_player_data(packet_data, packet_offset, false);
    if (new_connection)
    {
        cp->owner->add_connection(cp, cp->index_in_session, 0);
    }
    const multi_session_info* current = static_cast<const multi_session_info*>(cp->owner->get_info());
    info.multi_session_info_base::copy(static_cast<const multi_session_info_base*>(current));
    info.m_lobbyId = current->m_lobbyId;
    info.host_player_uid.read_from_packet(packet_data, packet_offset);
    const session_migration_reason reason = (session_migration_reason)packet_data[packet_offset];
    cp->owner->migrate_received_migrate(cp, &info, reason);
}

// 0x009B7200 (:4182): PT_MIGRATED: a client follows the new host
void multi_send_migrated_packet(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_MIGRATED;
    const unsigned int size = Multi_packets[PT_MIGRATED].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009737F0 (:4190)
void multi_process_migrated_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->migrate_received_migrated(from.connection);
    }
}

// 0x0094B690 (:4197)
void multi_process_next_server_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->migrate_received_next_server(from.connection);
    }
}

// 0x009B7260 (:4203)
void multi_send_modify_session_packet(multi_connection* cp, const multi_session_info_base* modified_info)
{
    unsigned char packet[1200];
    packet[0] = PT_MODIFY_SESSION;
    unsigned short size = Multi_packets[PT_MODIFY_SESSION].group_id != 0 ? 5 : 3;
    modified_info->append_modification_info_to_packet(packet, size);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009AA7D0 (:4212): read and dropped (modification is not supported any more)
void multi_process_modify_session_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        multi_session_info info(*from.connection->owner->get_info());
        int packet_offset = 0;
        info.read_modification_info_from_packet(packet_data, packet_offset);
        from.connection->owner->modify_received_modify(&info);
    }
}

// 0x009B7300 (:4224)
void multi_send_modified_session_packet(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_MODIFIED_SESSION;
    const unsigned int size = Multi_packets[PT_MODIFIED_SESSION].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x00973840 (:4232)
void multi_process_modified_session_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->modify_received_modified(from.connection);
    }
}

// 0x009B7360 (:4238): PT_REGISTER: the session's arbitration info
void multi_send_register_for_arbitration(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_REGISTER;
    unsigned short size = Multi_packets[PT_REGISTER].group_id != 0 ? 5 : 3;
    cp->owner->get_info()->append_arbitration_info_to_packet(packet, size);
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009B7400 (:4247)
void multi_process_register_for_arbitration(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->starting_received_register(packet_data, from.connection);
    }
}

// 0x009B7450 (:4253)
void multi_send_registered_for_arbitration(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_REGISTERED;
    const unsigned int size = Multi_packets[PT_REGISTERED].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x00973890 (:4261)
void multi_process_registered_for_arbitration(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->starting_received_registered(from.connection);
    }
}

// 0x009B74B0 (:4267)
void multi_send_start(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_START;
    const unsigned int size = Multi_packets[PT_START].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009B7510 (:4275)
void multi_process_start(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->session_start_gameplay();
    }
}

// 0x009B7540 (:4281)
void multi_send_started(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_STARTED;
    const unsigned int size = Multi_packets[PT_STARTED].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009738E0 (:4289)
void multi_process_started(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->starting_received_started(from.connection);
    }
}

// 0x009B75A0 (:4295)
void multi_send_end(multi_connection* cp)
{
    unsigned char packet[1200];
    packet[0] = PT_END;
    const unsigned int size = Multi_packets[PT_END].group_id != 0 ? 5 : 3;
    packet[1] = (unsigned char)((size - 3) >> 8);
    packet[2] = (unsigned char)(size - 3);
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009FC3A0 (:4303)
void multi_process_end(unsigned char* packet_data, const sender& from)
{
    if (from.connection != nullptr)
    {
        from.connection->owner->session_end_gameplay();
    }
}

// 0x0094B6B0 (:4310): empty in the shipped build
void multi_nat_debug_render()
{
}
