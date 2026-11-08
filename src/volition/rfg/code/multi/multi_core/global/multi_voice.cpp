// Original: volition/rfg/code/multi/multi_core/global/multi_voice.cpp
//
// Voice chat bookkeeping: the talkers (one per player uid, with its connections in the sessions), the headset state of
// each, sent as PT_VOICE_INFO (one byte per player: the update type, the value and the connection index) to the host
// and from the host to the clients once a second when something changed. The audio itself is the platform layer's
// (multi_voice_generic.cpp).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/vlib/code/util/varray.h"

#include <string.h>

// ---- file-static globals
// bool Push_to_talk_enabled (0x0167D833) is used here too; placed in multi_voice_generic.cpp.
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
// int Num_registered_talkers (0x02FE6C78), voice_talker Registered_talkers[30] (0x02FEBCA0) are used here too; placed in multi_voice_generic.cpp.
static timestamp Voice_update_timestamp; // 0x01691D50, initial data ffffffff
local_player_voice_info Local_player_voice_info; // 0x01691D54
static farray<unsigned int, 4> Other_local_talkers; // 0x01691D60
static bool Registered_talker_added; // 0x02FE5C63
static bool Registered_talker_dirty; // 0x02FE8D5B
static bool (*Party_lobby_voice_cb)(multi_connection*); // 0x02FEB624
static bool (*Game_voice_cb)(multi_connection*); // 0x02FEB628
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// 0x0094B9E0
voice_talker::voice_talker()
{
}

// 0x0094B9F0
local_player_voice_info::local_player_voice_info()
{
}

// ---- internal functions
// 0x00974100 (:86)
static voice_talker* find_talker_registration(multi_connection* talker_cp)
{
    return find_talker_registration_by_uid(talker_cp->player_uid);
}

// 0x0094BA00 (:92). LTCG: update_type on the stack, update_val in EDX, connection_index in ECX.
static unsigned char compress_voice_info(voice_update_type update_type, unsigned int update_val, unsigned int connection_index)
{
    return (unsigned char)((((unsigned int)update_type * 4) | (update_val & 3)) << 4 | (connection_index & 0xF));
}

// 0x0094BA20 (:101). LTCG: compressed_info in AL, update_type in EDX, the others on the stack.
static void decompress_voice_info(unsigned char compressed_info, voice_update_type& update_type, unsigned char& update_val, unsigned char& connection_index)
{
    update_type = (voice_update_type)(compressed_info >> 6);
    update_val = (compressed_info >> 4) & 3;
    connection_index = compressed_info & 0xF;
}

// 0x00974110 (:108): spoke in the last 2 s, so it has a headset whatever its last update said
static bool was_talking_headset_keep_alive(const multi_player_uid& uid)
{
    voice_talker* talker = find_talker_registration_by_uid(uid);
    return talker != nullptr && talker->last_voice_recvd.valid() && talker->last_voice_recvd.time_since() < 2000;
}

// 0x0094BA40 (:118): every player's headset for a newly added talker. LTCG: packet_size in EAX, packet_data in ECX,
// update_self dropped (always sent), the others on the stack.
static void build_full_voice_update(unsigned char* packet_data, unsigned short& packet_size, const multi_connection* send_cp, multi_session* session, bool update_self, bool self_has_headset)
{
    const unsigned short count_offset = packet_size;
    packet_data[packet_size] = 0;
    packet_size++;
    packet_data[packet_size] = compress_voice_info(VI_HEADSET_UPDATE, self_has_headset, session->get_me_connection()->index_in_session);
    packet_size++;
    int count = 1;
    for (int i = 0; i < Num_registered_talkers; i++)
    {
        const voice_talker& talker = Registered_talkers[i];
        for (int c = 0; c < talker.num_connections; c++)
        {
            const multi_connection* cp = talker.connections[c];
            if (cp != send_cp && cp->owner == session)
            {
                packet_data[packet_size] = compress_voice_info(VI_HEADSET_UPDATE, talker.has_headset, cp->index_in_session);
                packet_size++;
                count++;
            }
        }
    }
    packet_data[count_offset] = (unsigned char)count;
}

// 0x0094BB30 (:154): only what changed: answers the number of updates. LTCG: packet_size in ECX, the others on the stack.
static int build_partial_voice_update(unsigned char* packet_data, unsigned short& packet_size, const multi_connection* send_cp, multi_session* session, bool update_self, bool self_has_headset)
{
    const unsigned short count_offset = packet_size;
    packet_data[packet_size] = 0;
    packet_size++;
    int count = 0;
    if (update_self)
    {
        packet_data[packet_size] = compress_voice_info(VI_HEADSET_UPDATE, self_has_headset, session->get_me_connection()->index_in_session);
        packet_size++;
        count = 1;
    }
    if (Registered_talker_dirty)
    {
        for (int i = 0; i < Num_registered_talkers; i++)
        {
            const voice_talker& talker = Registered_talkers[i];
            if (!talker.has_headset_dirty)
            {
                continue;
            }
            for (int c = 0; c < talker.num_connections; c++)
            {
                const multi_connection* cp = talker.connections[c];
                if (cp != send_cp && cp->owner == session)
                {
                    packet_data[packet_size] = compress_voice_info(VI_HEADSET_UPDATE, talker.has_headset, cp->index_in_session);
                    packet_size++;
                    count++;
                    break;
                }
            }
        }
    }
    packet_data[count_offset] = (unsigned char)count;
    return count;
}

// 0x00974150 (:200). LTCG: session in ECX, update_self on the stack.
static int get_dirty_count(bool update_self, multi_session* session)
{
    int count = update_self ? 1 : 0;
    multi_connection* const first = session->get_connection_list();
    multi_connection* cp = first;
    while (cp != nullptr)
    {
        if (cp != cp->owner->get_me_connection())
        {
            voice_talker* talker = find_talker_registration(cp);
            if (talker != nullptr && talker->has_headset_dirty)
            {
                count++;
            }
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
    return count;
}

// 0x009B87E0 (:220): to every client: everything if its talker is new, else what changed. LTCG: session in ECX, the
// others on the stack.
static void session_host_send_voice_info_updates(multi_session* session, bool update_self, bool self_has_headset)
{
    if (session->get_slots_used() < 2)
    {
        return;
    }
    const int dirty_count = get_dirty_count(update_self, session);
    multi_connection* cp = session->get_connection_list();
    while (cp != nullptr)
    {
        voice_talker* talker = cp != cp->owner->get_me_connection() ? find_talker_registration(cp) : nullptr;
        if (talker != nullptr)
        {
            unsigned char packet[1200];
            packet[0] = PT_VOICE_INFO;
            unsigned short size = Multi_packets[PT_VOICE_INFO].group_id != 0 ? 5 : 3;
            if (talker->newly_added)
            {
                build_full_voice_update(packet, size, cp, session, update_self, self_has_headset);
                const unsigned short payload = (unsigned short)(size - 3);
                packet[1] = (unsigned char)(payload >> 8);
                packet[2] = (unsigned char)payload;
                multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
            }
            else if (dirty_count > 0)
            {
                const int count = build_partial_voice_update(packet, size, cp, session, update_self, self_has_headset);
                const unsigned short payload = (unsigned short)(size - 3);
                packet[1] = (unsigned char)(payload >> 8);
                packet[2] = (unsigned char)payload;
                if (count > 0)
                {
                    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
                }
            }
        }
        cp = cp->next;
        if (cp == session->get_connection_list())
        {
            break;
        }
    }
}

// 0x009B89C0 (:269): once a second: our headset, then the changes to each ready session's host or clients.
// Original bug kept: the session type table has three entries for two types (the party session is visited twice).
static void update_voice_info()
{
    if (Multi_syslink || !Local_player_voice_info.update_check_time.elapsed())
    {
        return;
    }
    Local_player_voice_info.update_check_time.set(1000);
    const bool had_headset = Local_player_voice_info.has_headset;
    if (multi_voice_chat_allowed(Multi_syslink))
    {
        multi_voice_platform_update_my_headset_status();
    }
    const bool has_headset = Local_player_voice_info.has_headset;
    if (had_headset != has_headset)
    {
        Local_player_voice_info.has_headset_dirty = true;
    }
    const bool self_dirty = Local_player_voice_info.has_headset_dirty;
    if (self_dirty || Registered_talker_dirty || Registered_talker_added)
    {
        const session_types types[3] = {ST_PARTY, ST_GAME, ST_PARTY};
        for (int t = 0; t < 3; t++)
        {
            multi_session* session = multi_session_get_by_type(types[t]);
            if (session == nullptr)
            {
                continue;
            }
            const multi_session_state state = session->get_state();
            if (state != MSS_READY && state != MSS_IN_GAME)
            {
                continue;
            }
            if (session->am_i_host())
            {
                session_host_send_voice_info_updates(session, self_dirty, has_headset);
            }
            else if (self_dirty)
            {
                multi_connection* me = session->get_me_connection();
                unsigned char packet[1200];
                packet[0] = PT_VOICE_INFO;
                unsigned short size = Multi_packets[PT_VOICE_INFO].group_id != 0 ? 5 : 3;
                packet[size] = 1;
                size++;
                packet[size] = compress_voice_info(VI_HEADSET_UPDATE, has_headset, me->index_in_session);
                size++;
                const unsigned short payload = (unsigned short)(size - 3);
                packet[1] = (unsigned char)(payload >> 8);
                packet[2] = (unsigned char)payload;
                multi_io_send_reliable(me->owner->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
            }
        }
    }
    Local_player_voice_info.has_headset_dirty = false;
    Registered_talker_dirty = false;
    Registered_talker_added = false;
    for (int i = 0; i < Num_registered_talkers; i++)
    {
        Registered_talkers[i].has_headset_dirty = false;
        Registered_talkers[i].newly_added = false;
    }
}

// 0x00990EA0 (:339): multi_voice_platform_register_local_talker inlined
static void register_local_talker()
{
    if (!multi_voice_platform_engine_valid())
    {
        return;
    }
    Local_player_voice_info.registered_for_voice = false;
    Local_player_voice_info.has_headset = false;
    Local_player_voice_info.has_headset_dirty = false;
    Local_player_voice_info.controller_index = Player_controller_index;
    Local_player_voice_info.update_check_time.set(0);
    Other_local_talkers.remove_all();
    if (Player_controller_index != 0)
    {
        const unsigned int other_controller = 0;
        Other_local_talkers.add(other_controller);
    }
    multi_voice_platform_register_local_talker();
}

// 0x009741E0 (:366)
static void unregister_local_talker()
{
    if (multi_voice_platform_engine_valid() && Local_player_voice_info.registered_for_voice)
    {
        multi_voice_platform_unregister_local_talker();
        Local_player_voice_info.registered_for_voice = false;
        Local_player_voice_info.controller_index = 0xFFFFFFFF;
        Other_local_talkers.remove_all();
    }
}

// 0x0094BC40 (:402)
voice_talker* find_talker_registration_by_uid(const multi_player_uid& uid)
{
    for (int i = 0; i < Num_registered_talkers; i++)
    {
        if (Registered_talkers[i].player_uid.get_internal_id().is_equal(uid.get_internal_id()))
        {
            return &Registered_talkers[i];
        }
    }
    return nullptr;
}

// 0x00990FD0 (:413): nothing when already running (multi_voice_platform_init inlined)
void multi_voice_init(bool game_is_syslink_or_lan)
{
    if (multi_voice_platform_engine_valid())
    {
        return;
    }
    for (int i = 0; i < 30; i++)
    {
        Registered_talkers[i].num_connections = 0;
        memset(&Registered_talkers[i].player_uid, 0, 10); // the id's provider data, guest index and valid flag; not is_real_uid
    }
    Num_registered_talkers = 0;
    Registered_talker_dirty = false;
    Registered_talker_added = false;
    multi_voice_platform_init();
    register_local_talker();
}

// 0x00974240 (:445): multi_voice_platform_shutdown inlined
void multi_voice_shutdown()
{
    if (multi_voice_platform_engine_valid())
    {
        unregister_local_talker();
        Local_player_voice_info.registered_for_voice = false;
        multi_voice_platform_shutdown();
        return;
    }
    Local_player_voice_info.registered_for_voice = false;
}

// 0x0094BC90 (:455)
void multi_voice_set_should_send_callback(bool (*callback)(multi_connection*), int session_type)
{
    if (session_type == ST_GAME)
    {
        Game_voice_cb = callback;
    }
    else if (session_type == ST_PARTY)
    {
        Party_lobby_voice_cb = callback;
    }
}

// 0x009B8BB0 (:467). The last multi_voice_chat_allowed's result is not used in rfg.exe.
void multi_voice_do_frame()
{
    if (!multi_voice_platform_engine_valid() || Player_controller_index == 0xFFFFFFFF || Multi_syslink)
    {
        return;
    }
    if (!Voice_update_timestamp.valid())
    {
        Voice_update_timestamp.set(200);
    }
    multi_voice_platform_do_frame();
    if (Local_player_voice_info.registered_for_voice)
    {
        update_voice_info();
    }
    multi_voice_chat_allowed(Multi_syslink);
}

// 0x009742A0 (:491): at most 30 talkers and 4 connections each
void multi_voice_register_talker(multi_connection* talker_cp)
{
    if (Multi_syslink || !multi_voice_platform_engine_valid())
    {
        return;
    }
    voice_talker* talker = find_talker_registration_by_uid(talker_cp->player_uid);
    if (talker == nullptr)
    {
        if (Num_registered_talkers >= 30)
        {
            return;
        }
        talker = &Registered_talkers[Num_registered_talkers];
        talker->num_connections = 1;
        talker->connections[0] = talker_cp;
        talker->player_uid = talker_cp->player_uid;
        talker->has_headset = false;
        Num_registered_talkers++;
    }
    else
    {
        if (talker->num_connections > 3)
        {
            return;
        }
        talker->connections[talker->num_connections] = talker_cp;
        talker->num_connections++;
    }
    Registered_talker_added = true;
    talker->newly_added = true;
}

// 0x00974390 (:553). The last connection of a talker removes it without checking that it is this one (rfg.exe). The
// engine check's result is not used in rfg.exe.
void multi_voice_remove_talker(multi_connection* talker_cp)
{
    if (Multi_syslink || talker_cp == nullptr)
    {
        return;
    }
    multi_voice_platform_engine_valid();
    voice_talker* talker = find_talker_registration_by_uid(talker_cp->player_uid);
    if (talker == nullptr)
    {
        return;
    }
    const int count = talker->num_connections;
    if (count < 2)
    {
        voice_talker* last = &Registered_talkers[Num_registered_talkers - 1];
        if (talker != last)
        {
            *talker = *last;
        }
        Num_registered_talkers--;
        return;
    }
    for (int i = 0; i < count; i++)
    {
        if (talker->connections[i] == talker_cp)
        {
            talker->num_connections = count - 1;
            talker->connections[i] = talker->connections[count - 1];
            return;
        }
    }
}

// 0x0094BCC0 (:618): empty in rfg.exe
void multi_voice_login_changed()
{
}

// 0x00991070 (:622)
void multi_voice_login_something()
{
    if (!Multi_syslink)
    {
        unregister_local_talker();
        register_local_talker();
    }
}

// 0x0094BCD0 (:632)
bool multi_voice_is_headset_present(const multi_player_uid& uid)
{
    if (Multi_syslink)
    {
        return false;
    }
    if (My_player_uid.get_internal_id().is_equal(uid.get_internal_id()))
    {
        return Local_player_voice_info.has_headset;
    }
    voice_talker* talker = find_talker_registration_by_uid(uid);
    return talker != nullptr && talker->has_headset;
}

// 0x00974480 (:651)
multi_voice_talk_state multi_voice_get_talk_state(const multi_player_uid& uid)
{
    if (multi_voice_platform_player_is_muted(uid))
    {
        return MVTS_MUTED;
    }
    if (!multi_voice_is_headset_present(uid) && !was_talking_headset_keep_alive(uid))
    {
        return MVTS_NO_HEADSET;
    }
    const bool talking = uid.get_internal_id().is_equal(My_player_uid.get_internal_id()) ? multi_voice_platform_local_player_is_talking(uid) : multi_voice_platform_remote_player_is_talking(uid);
    return talking ? MVTS_TALKING : MVTS_NOT_TALKING;
}

// 0x00974540 (:674): no headset shows as muted
multi_voice_talk_state multi_voice_get_icon_talk_state(const multi_player_uid& uid)
{
    const multi_voice_talk_state state = multi_voice_get_talk_state(uid);
    if (state >= MVTS_MUTED)
    {
        if (state < MVTS_NO_HEADSET)
        {
            return state;
        }
        if (state == MVTS_NO_HEADSET)
        {
            return MVTS_MUTED;
        }
    }
    return MVTS_NOT_TALKING;
}

// 0x0094BD10 (:694)
void multi_voice_signal_to_send_self_update()
{
    Local_player_voice_info.has_headset_dirty = true;
}

// 0x00974570 (:699): PT_VOICE_INFO: the count, then one compressed update per player; a host relays what changed
void multi_process_game_voice_info_update(unsigned char* packet_data, const sender& from)
{
    if (Multi_syslink || from.connection == nullptr)
    {
        return;
    }
    multi_session* session = from.connection->owner;
    if (session == nullptr)
    {
        return;
    }
    const int count = packet_data[0];
    for (int i = 0; i < count; i++)
    {
        voice_update_type update_type;
        unsigned char update_val;
        unsigned char connection_index;
        decompress_voice_info(packet_data[i + 1], update_type, update_val, connection_index);
        multi_connection* cp = session->get_connection_by_index(connection_index);
        if (cp == nullptr)
        {
            continue;
        }
        voice_talker* talker = find_talker_registration(cp);
        if (talker == nullptr || update_type != VI_HEADSET_UPDATE)
        {
            continue;
        }
        const bool has_headset = update_val == 1;
        if (session->am_i_host() && talker->has_headset != has_headset)
        {
            talker->has_headset_dirty = true;
            Registered_talker_dirty = true;
        }
        talker->has_headset = has_headset;
    }
}

// 0x0094BD20 (:747). multi_voice_platform_chat_allowed inlined.
bool multi_voice_chat_allowed(bool lan_game)
{
    if (Multi_syslink)
    {
        return false;
    }
    return multi_voice_platform_chat_allowed(lan_game);
}

// 0x0094BD70 (:755)
bool multi_voice_push_to_talk_enabled()
{
    return Push_to_talk_enabled;
}

// 0x0094BD80 (:760)
void multi_voice_push_to_talk_set_enabled(bool enabled)
{
    Push_to_talk_enabled = enabled;
}

// 0x009AABE0 (:766): the player's mute choice in the first session that knows it
void multi_voice_set_original_player_mute_state(const multi_player_uid& player_uid, bool isMuted)
{
    const session_types types[2] = {ST_PARTY, ST_GAME};
    for (int t = 0; t < 2; t++)
    {
        multi_session* session = multi_session_get_by_type(types[t]);
        if (session == nullptr)
        {
            continue;
        }
        const multi_game_data* data = multi_game_data_get_by_player_uid(session->get_info()->type, player_uid);
        if (data != nullptr && data->player_uid.get_internal_id().is_equal(player_uid.get_internal_id()))
        {
            multi_game_data_set_isMuted(data->handle, isMuted);
            return;
        }
    }
}

// 0x009AAC80 (:796): at the start the other team is muted, at the end only the players muted by choice stay so.
// multi_game_data_get_my_data inlined. Original bug kept: a connection's game data is not checked for null.
void multi_voice_handle_voice_chat_at_game_start_and_end(bool isStarting)
{
    const session_types types[2] = {ST_PARTY, ST_GAME};
    for (int t = 0; t < 2; t++)
    {
        multi_session* session = multi_session_get_by_type(types[t]);
        if (session == nullptr)
        {
            continue;
        }
        const multi_game_data* my_data = multi_game_data_get_my_data(session->get_info()->type, false, false);
        if (my_data == nullptr)
        {
            continue;
        }
        multi_connection* const first = session->get_connection_list();
        multi_connection* cp = first;
        while (cp != nullptr)
        {
            if (cp != session->get_me_connection())
            {
                const multi_player_uid uid = cp->player_uid;
                const multi_game_data* data = multi_game_data_get_by_player_uid(session->get_info()->type, uid);
                if ((isStarting && my_data->team != data->team) || data->isMuted)
                {
                    multi_voice_platform_mute_player(uid);
                }
                else
                {
                    multi_voice_platform_unmute_player(uid);
                }
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
    }
}
