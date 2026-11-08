// Original: volition/rfg/code/multi/multi_core/global/multi.cpp
//
// The multiplayer core: system init/shutdown, starting and stopping multiplayer (multi_init/multi_shutdown, the memory
// of the multiplayer pools, the packet table), the per-frame network processing (multi_process: the platform and game
// link, the clock, incoming packets, the sessions, the server/client load states, acks and packet trackers, the
// transmit groups and the send job) and the handshake.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py). Globals with a
// dynamic initializer are placed by the initializer's source line (rfg.pdb), which can be a later file of the unity.

#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_core/include/multi_find_servers.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_core/include/multi_voice.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_callbacks.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/mutex.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"
#include "keen/base/mutex.hpp"
#include "keen/base/trace.hpp"
#include "unit4/gamesession/localgamesession.hpp"
#include "unit4/gamesession/remotegamesession.hpp"

#include <winsock2.h>
#include <stdio.h>
#include <string.h>

// ---- file-private types (rfg.pdb)
enum multi_states
{
    MS_OFF = 0,
    MS_ON = 1,
    MS_SHUTTING_DOWN = 2,
    NUM_MULTI_STATES = 3,
};

// size 0x8
struct network_game_handle
{
    unsigned short m_network_handle; // 0x00
    unsigned int m_game_handle; // 0x04
};

// size 0x18
struct upstream_bandwidth_info
{
    upstream_bandwidth_info(); // 0x00947550
    upstream_bandwidth_info& operator=(const upstream_bandwidth_info& __that); // 0x00987B20

    multi_player_uid player_uid; // 0x00
    int upstream_bw; // 0x0C
    int upstream_bw_test_limit; // 0x10
    timestamp update_time; // 0x14
};

// ---- globals (rfg.pdb has no source file for them; placed here, their main writer, or by their initializer's line)
bool Multiplayer; // 0x02FEB588
bool Multi_syslink; // 0x02FEB589, the game is a system link (LAN) game
bool Multi_partied; // 0x02FEB6F3, the local player came in with a party
bool Multi_join_in_progress; // 0x02FEB58A
bool Multi_last_session_start_succeeded; // 0x02FEB58B
packet_def* Multi_packets; // 0x02FEB58C, Multi_core_packets and Multi_game_packets, copied by multi_init
client_state Client_state; // 0x02FEB6A8
packet_send_info Packet_send_info; // 0x01691630
mp_transmit_list Transmit_list; // 0x03017748
bool Multi_left_game_by_choice; // 0x02FEB658
keen::Mutex g_gameLinkMutex; // 0x02FEB77C (:407); created by keen::rfg::Application::initialize ("GameLinkMutexRFG")
mutex Multi_thread_mutex; // 0x02FED1B8 (:416), guards the game link's sockets against the send job
cs_wrapper MP_send_setup_cs(0); // 0x02FEBC40 (:513)
scheduling_trigger* MP_send_scheduling_trigger; // 0x02FEB5B4
event_trigger* MP_send_process_trigger; // 0x02FEB5B8
bool MP_reached_render_trigger_point; // 0x02FEB5B1

// ---- file-static globals
static bool MP_send_process_job = true; // 0x0167D7CA
static unsigned int Broadcast_port = 1005; // 0x0167D81C, the LAN (GameLinkLan) socket and broadcast port; no other writer
static int Game_bw_test_size = 61440; // 0x0167D840
static unsigned short Cur_network_handle = 0x3E8; // 0x0167DD78
static jip_destruction_sync jip_destruction_sync_state = JIP_DESTRUCTION_STATE_NONE; // 0x0167E708, initial data 03000000
static timestamp Bandwidth_ready; // 0x016916C4, initial data ffffffff
static timestamp Bandwidth_filling_timestamp; // 0x01691CC4, initial data ffffffff
// object_pool<zone_file,32,1> Zones (0x02CEC200) is used here too; placed in rfg_zonex.cpp.
static bool jip_destruction_sync_possible; // 0x02FE6C77
static unsigned int Prev_frametime; // 0x02FE6D00
static short Bandwidth_buckets[243]; // 0x02FE8108
static int Settings_version; // 0x02FE88B0, from tweak_table.xtbl
// Placed with its variable: rfg.exe's item is multi_voice.cpp's (later in this unity TU).
static tweak_table_item Tweak_table_item_Settings_version("Settings_version", &Settings_version); // 0x02FEBC94
static bool Send_qos_info; // 0x02FE9076
static unsigned int Clock_update_time; // 0x02FEB340
static unsigned int Clock_current_time; // 0x02FEB538
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static bool Dedicated; // 0x02FEB591
static bool Initd; // 0x02FEB592
static bool Multi_system_init; // 0x02FEB593
static unsigned char* Mempool_buffer; // 0x02FEB594
static unsigned char* MP_alloc_vram_buffer; // 0x02FEB598
static unsigned char* Mp_mp_cpu_buffer; // 0x02FEB59C
static unsigned char* Mp_mp_vram_buffer; // 0x02FEB5A0
static multi_states Multi_state; // 0x02FEB5A4
static void (*Pre_io_send_job_game_callback)(); // 0x02FEB5A8
static void (*Handshake_game_callback)(); // 0x02FEB5AC
static unsigned char Multi_version_channel; // 0x02FEB5B0
static int Console_set_ip; // 0x02FEB5BC
static bool Bandwidth_enabled; // 0x02FEB5D1
static unsigned char (*Get_dcmo_index_from_game_handle_cb)(unsigned int); // 0x02FEB5D4
static void (*Set_dcmo_index_for_game_handle_cb)(unsigned int, unsigned char); // 0x02FEB5D8
static bool Scoreboard_system_initd; // 0x02FEB5DE
static remote_object* Remote_objects_pool; // 0x02FEB5EC
static rop_sort_element* Rop_sort_element; // 0x02FEB5F0
static rop_sort_element** PTR_rop_sort_element; // 0x02FEB5F4
static int Num_reserved_network_handles; // 0x02FEB5F8
static network_game_handle* Network_game_handles; // 0x02FEB5FC
static bool Spectating; // 0x02FEB659
static bool Multi_badge_render_my_badge; // 0x02FEB65B
static unsigned int My_badge; // 0x02FEB66C
static migration_state Migration_state; // 0x02FEB6C4
// pending_invite_info Pending_invite (0x02FEBBD8) is used here too; placed in multi_core_platform_generic.cpp (MP2).
// 0x0169409C: client_should_send_at_30_hz's only read; rfg.pdb has no symbol for it (name from the Ghidra DB). Magic
// number: 500 bits of headroom.
static unsigned int bw_min_avail = 500;

// File statics of later files of the unity that this file uses (their dynamic initializers place them there; rfg.exe
// declares them extern before the static definition, MSVC's C4211 extension). Here they have external linkage.
extern farray<upstream_bandwidth_info, 30> Tracked_upstream_bandwidth_info_list; // 0x016917A0, multi_core_game_bandwidth.cpp
extern multi_pool<multi_server_info, 64> Server_pool; // 0x01691CD0, multi_find_servers.cpp

// ---- internal functions
static void multi_server_process(); // 0x009F70B0 (:598)
static void multi_client_process(); // 0x009FD640 (:610)
static void send_reliable_acks(const mp_transmit_list& transmit_list); // 0x009D7A70 (:624)
static void unify_packets(mempool_base& pool); // 0x00943130 (:638)
static void multi_core_memory_alloc(); // 0x00943170 (:645)
static void multi_core_memory_release(); // 0x00943230 (:674)
static int connection_get_transmission_group(const multi_connection* cp); // 0x00943290 (:696)
static bool is_int_in_list(const int* int_list, int list_size, int target_int); // 0x009432B0 (:708)
static void populate_transmit_list(mp_transmit_list& transmit_list, int* transmit_group_indices, int num_groups); // 0x0098E6A0 (:719)
static bool client_should_send_at_30_hz(); // 0x009A8070 (:750)
static int get_send_update_time(); // 0x009A8100 (:791)
static void setup_transmit_list(mp_transmit_list& transmit_list); // 0x009A8140 (:806)
static void multi_core_send_job_pre(mp_transmit_list& transmit_list); // 0x009D7D40 (:1060)

// File statics of later files of the unity that this file calls (rfg.exe declares them earlier in the TU).
static void packet_trackers_process(const mp_transmit_list& transmit_list); // multi_connection.cpp
static void transmit_packet_id_acks(const mp_transmit_list& transmit_list); // multi_connection.cpp
static void process_presence_state(); // multi_core_platform.cpp
static unsigned char multi_get_dcmo_index_from_game_handle(unsigned int game_handle); // multi_game_callbacks.cpp
static void multi_set_dcmo_index_for_game_handle(unsigned int game_handle, unsigned char index); // multi_game_callbacks.cpp
static void multi_game_join_process_queued_loaded_clients(); // multi_game_io.cpp
static void multi_game_player_mp_flags_snapshot(); // multi_game_object_player.cpp
static void multi_game_player_mp_flags_handshake(); // multi_game_object_player.cpp

// 0x00943060 (:486): console command "mp_channel [c]": prints or sets the version channel (0 to 255)
void unused_dcf_mp_channel()
{
    char text[512];
    if (console_get_arg(8, true))
    {
        if ((unsigned int)Console_arg_int < 0x100)
        {
            Multi_version_channel = (unsigned char)Console_arg_int;
            sprintf(text, "Current channel set to: %d\n", Console_arg_int & 0xFF);
        }
        else
        {
            sprintf(text, "%s", "Usage: mp_channel (c)\nc: An optional number in the range 0-255 (inclusive) specifying what channel to use.\n   If [c] is omitted, this command will return the current channel you're on.");
        }
    }
    else
    {
        sprintf(text, "Current channel is: %d\n", (unsigned int)Multi_version_channel);
    }
    console_output(text, nullptr, false);
}

// 0x0098E620 (:523): console command "mp_version". rfg.exe formats into a thread-local scratch buffer (TLS + 0x38,
// 1024 bytes); the version's 0x20000000 bit (always set) selects "release final, ". Magic number: 0xFF, the major
// version byte multi_get_multiplayer_version puts at the bottom.
void unused_dcf_mp_version()
{
    char text[0x400];
    const unsigned int version = multi_get_multiplayer_version();
    snprintf(text, sizeof(text), "MP version: %i, settings: %d, %s%s%schannel: %d\nSent as: 0x%p", 0xFF, Settings_version, "", "",
        (version & 0x20000000) != 0 ? "release final, " : "", (unsigned int)Multi_version_channel, (void*)(uintptr_t)version);
    console_output(text, nullptr, false);
}

// 0x00943100 (:581)
void multi_core_register_pre_io_send_job_game_callback(void (*cb)())
{
    Pre_io_send_job_game_callback = cb;
}

// 0x00943110 (:586)
void multi_core_register_handshake_game_callback(void (*cb)())
{
    Handshake_game_callback = cb;
}

// 0x00943120 (:592)
void multi_server_do_post_sim()
{
}

// 0x009F70B0 (:598): the server's own level load state; when it is in gameplay, the queued clients get their players
static void multi_server_process()
{
    if (Client_state == CLIENT_STATE_ESTABLISHED)
    {
        Client_state = CLIENT_STATE_LOADING_GAME;
    }
    else if (Client_state == CLIENT_STATE_LOADING_GAME && gameseq_state_is_active(GS_GAMEPLAY))
    {
        Client_state = CLIENT_STATE_IN_GAME;
        multi_game_join_process_queued_loaded_clients();
    }
    multi_core_dc_flush_old_object_states();
}

// 0x009FD640 (:610): the join-game-start packet sets PENDING_GAME_LOAD; once in gameplay, the server is told
static void multi_client_process()
{
    if (Client_state == CLIENT_STATE_PENDING_GAME_LOAD)
    {
        multi_game_lobby_client_start_game();
        Client_state = CLIENT_STATE_LOADING_GAME;
    }
    else if (Client_state == CLIENT_STATE_LOADING_GAME && gameseq_state_is_active(GS_GAMEPLAY))
    {
        Client_state = CLIENT_STATE_IN_GAME;
        multi_game_send_client_loaded_level();
    }
    multi_core_dc_send_acks();
    multi_core_dc_flush_old_object_states();
}

// 0x009D7A70 (:624): per connection, an ack packet (0x22) with as many queued reliable ack ids as fit in the
// unreliable buffer; acks sent twice are dropped from the queue. rfg.exe inlines multi_io_send here.
static void send_reliable_acks(const mp_transmit_list& transmit_list)
{
    for (int i = 0; i < transmit_list.num_transmit_connections; i++)
    {
        multi_connection* cp = transmit_list.connections[i];
        // Magic numbers: 0x3F3 is the unreliable buffer limit (0x3F7) less the 4 header bytes; 2 bytes per ack id.
        unsigned int num_acks = cp->ack_queue_count;
        const unsigned int max_acks = (0x3F3u - cp->unreliable_buffer_size) >> 1;
        if (max_acks < num_acks)
        {
            num_acks = max_acks;
        }
        if (num_acks == 0)
        {
            continue;
        }
        unsigned char packet[1200];
        packet[0] = PT_ACK_REPLY;
        unsigned int size = Multi_packets[PT_ACK_REPLY].group_id != 0 ? 5 : 3; // in-order packets carry a sequence id
        packet[size] = (unsigned char)num_acks;
        size++;
        for (unsigned int a = 0; a < num_acks; a++)
        {
            const unsigned short ack_id = cp->ack_queue[a].ack_id;
            cp->ack_queue[a].ack_sent_count++;
            packet[size & 0xFFFF] = (unsigned char)(ack_id >> 8);
            packet[(size & 0xFFFF) + 1] = (unsigned char)ack_id;
            size += 2;
        }
        const unsigned short payload = (unsigned short)(size - 3);
        packet[1] = (unsigned char)(payload >> 8);
        packet[2] = (unsigned char)payload;
        size &= 0xFFFF;
        multi_io_send(cp, packet, size);
        unsigned char kept = 0;
        for (int a = 0; a < (int)cp->ack_queue_count; a++)
        {
            if (cp->ack_queue[a].ack_sent_count < 2)
            {
                cp->ack_queue[kept++] = cp->ack_queue[a];
            }
        }
        cp->ack_queue_count = kept;
    }
}

// 0x00943130 (:638): the core and game packet tables in one array, indexed by packet id
static void unify_packets(mempool_base& pool)
{
    Multi_packets = (packet_def*)pool.alloc(sizeof(packet_def) * 128, 0);
    memcpy(Multi_packets, Multi_core_packets, sizeof(Multi_core_packets));
    memcpy(Multi_packets + MAX_CORE_PACKET_TYPES, Multi_game_packets, sizeof(Multi_game_packets));
}

// 0x00943170 (:645): the multiplayer pools in the level memory. Magic numbers: 0x2F00200 bytes (47 MB) of allocators,
// 0x80000 multi purpose bytes, 16-byte alignment; the vram half of the allocators is 0x500000 bytes in 8 blocks.
static void multi_core_memory_alloc()
{
    Mempool_buffer = (unsigned char*)Level_memmgr.alloc(0x2F00200, "multi_core_memory_alloc()", 16);
    memset(Mempool_buffer, 0, 0x2F00200);
    Mp_mp_cpu_buffer = (unsigned char*)Level_memmgr.alloc(0x80000, "multi_core_memory_alloc()", 16);
    memset(Mp_mp_cpu_buffer, 0, 0x80000);
    MP_allocators_mempool.init_rfg_split_mempool("mp alloc", Mempool_buffer, 0x2F00200, 16, MP_alloc_vram_buffer, 0x500000, 16, 8);
    MP_allocators_mempool.clear();
    MP_multi_purpose_mempool.init_rfg_split_mempool("mp multipurpose", Mp_mp_cpu_buffer, 0x80000, 16, Mp_mp_vram_buffer, 0, 1, 0);
    MP_multi_purpose_mempool.clear();
}

// 0x00943230 (:674)
static void multi_core_memory_release()
{
    MP_allocators_mempool.set_thread_ownership();
    MP_allocators_mempool.clear();
    Level_memmgr.free(Mempool_buffer);
    Mempool_buffer = nullptr;
    MP_alloc_vram_buffer = nullptr;
    MP_multi_purpose_mempool.clear();
    Level_memmgr.free(Mp_mp_cpu_buffer);
    Mp_mp_cpu_buffer = nullptr;
    Mp_mp_vram_buffer = nullptr;
}

// 0x00943290 (:696): connections are spread over 3 transmit groups by their session index
static int connection_get_transmission_group(const multi_connection* cp)
{
    if (cp->owner != nullptr && cp->index_in_session != 0xFF)
    {
        return cp->index_in_session % 3;
    }
    return 0;
}

// 0x009432B0 (:708)
static bool is_int_in_list(const int* int_list, int list_size, int target_int)
{
    for (int i = 0; i < list_size; i++)
    {
        if (int_list[i] == target_int)
        {
            return true;
        }
    }
    return false;
}

// 0x0098E6A0 (:719): the connected connections (not our own) whose transmit group is due this frame
static void populate_transmit_list(mp_transmit_list& transmit_list, int* transmit_group_indices, int num_groups)
{
    Packet_send_info.group_0_transmit_this_frame = is_int_in_list(transmit_group_indices, num_groups, 0);
    transmit_list.num_transmit_connections = 0;
    transmit_list.num_transmit_groups = num_groups;
    if (num_groups == 0)
    {
        return;
    }
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp->connection_status != CS_CONNECTED && cp->connection_status != CS_CONNECTED_GAMEPLAY)
        {
            continue;
        }
        if (cp->owner != nullptr && cp == cp->owner->get_me_connection())
        {
            continue;
        }
        if (is_int_in_list(transmit_group_indices, num_groups, connection_get_transmission_group(cp)))
        {
            transmit_list.connections[transmit_list.num_transmit_connections++] = cp;
        }
    }
    Connection_pool.thread_access.leave();
}

// 0x009A8070 (:750): a client sends at 30 Hz (instead of 15) while it may use the bandwidth or has reliables waiting
static bool client_should_send_at_30_hz()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp == nullptr || sp->am_i_host())
    {
        return false;
    }
    multi_connection* host = sp->get_host_connection();
    if (host == nullptr)
    {
        return false;
    }
    int players = host->owner->get_slot_use_data().used - 1;
    if (players < 2)
    {
        players = 1;
    }
    const unsigned int bandwidth = multi_get_required_bandwidth_for_players(players);
    const unsigned int num_players = multi_get_num_players_from_bandwidth(bandwidth);
    // Magic number: 2000 bits kept free per second.
    const unsigned int budget = bandwidth / (num_players - 1) - 2000;
    if (bw_min_avail + host->available_bandwidth_tracker.get_bandwidth_consumed(1000) < budget)
    {
        return true;
    }
    return connection_has_unsent_reliable_data(host);
}

// 0x009A8100 (:791): ms between sends: 10 per second on the server, 15 or 30 on a client
static int get_send_update_time()
{
    if (Multiplayer && Server)
    {
        return 100;
    }
    return 1000 / (client_should_send_at_30_hz() ? 30 : 15);
}

// 0x009A8140 (:806): the transmit groups due since the last transmit (one group per third of the send interval)
static void setup_transmit_list(mp_transmit_list& transmit_list)
{
    int rate;
    if (Multiplayer && Server)
    {
        rate = 10;
    }
    else
    {
        rate = client_should_send_at_30_hz() ? 30 : 15;
    }
    const int send_ms = 1000 / rate;
    const int since = Packet_send_info.last_transmit_time.time_since();
    int groups[3];
    int num_groups = 0;
    for (int third = 1; third <= 3; third++)
    {
        if (since > send_ms * third / 3)
        {
            groups[num_groups++] = Packet_send_info.next_transmit_group;
            Packet_send_info.next_transmit_group = (Packet_send_info.next_transmit_group + 1) % 3;
        }
    }
    if (num_groups > 0)
    {
        if (since > send_ms * 4 / 3)
        {
            Packet_send_info.last_transmit_time.set(0);
        }
        else
        {
            Packet_send_info.last_transmit_time.set(send_ms * num_groups / 3 - since);
        }
    }
    populate_transmit_list(transmit_list, groups, num_groups);
}

// 0x009D7CD0 (:859): once per process: the platform layer, our own bandwidth slot, DLC, the level list
void multi_system_init()
{
    if (Multi_system_init)
    {
        return;
    }
    Multi_system_init = true;
    multi_core_platform_system_init();
    Tracked_upstream_bandwidth_info_list.remove_all();
    upstream_bandwidth_info info;
    Tracked_upstream_bandwidth_info_list.add(info);
    tracked_bandwidth_info_set_info(My_player_uid, Game_bw_test_size, Game_bw_test_size, false);
    dlc_system_init();
    multi_game_settings_preparse_levels(Permanent_mempool);
}

// 0x00971710 (:877)
void multi_system_shutdown()
{
    if (!Multi_system_init)
    {
        return;
    }
    Multi_system_init = false;
    g_gameLinkMutex.lock(0);
    Remote_game_session.destroy(rfg_get_cpu_scratch_allocator());
    g_gameLinkMutex.unlock();
}

// 0x009FD690 (:889): runs on a scheduler job (multi_init_job). Allocates the pools, starts the platform session (LAN
// with the parameters below, else the internet link), installs the packet table and the game.
void multi_init(bool game_is_syslink_or_lan)
{
    keen::debug::nativePrintArgs("multi_init for %s\n", game_is_syslink_or_lan ? "syslink" : "live");
    if (Initd || Multi_state == MS_ON)
    {
        keen::debug::nativePrintArgs("Multiplayer is already initd!");
        return;
    }
    Multi_syslink = game_is_syslink_or_lan;
    multi_core_memory_alloc();
    multi_game_memory_alloc();
    multi_core_session_info session_info;
    memset(&session_info, 0, sizeof(session_info));
    if (game_is_syslink_or_lan)
    {
        // Magic numbers: the LAN lobby timings in ms and the game id every LAN peer must share.
        session_info.game_link_params.lan.portMatchmakingBroadcast = (unsigned short)Broadcast_port;
        session_info.game_link_params.lan.sendLobbyInfoRequestDeltaTime = 200;
        session_info.game_link_params.lan.sendLobbyStateDeltaTime = 200;
        session_info.game_link_params.lan.sendJoinRequestDeltaTime = 200;
        session_info.game_link_params.lan.gameUniqueIdentifier = 0x12345678;
        session_info.game_link_params.lan.searchLobbyDuration = 1000;
        session_info.game_link_params.lan.sendKeepAliveDeltaTime = 100;
        session_info.game_link_params.lan.timeoutLobbyMember = 5000;
        session_info.game_link_params.lan.timeoutJoin = 5000;
        session_info.game_link_params.lan.timeoutReceiveFirstLobbyStateUpdate = 5000;
        session_info.game_link_params.lan.hostMigrationEnabled = false;
    }
    session_info.game_link_params.connectionType = (kaiko::GameLinkConnectionType)!game_is_syslink_or_lan;
    Multi_last_session_start_succeeded = multi_core_platform_internal_session_start(session_info);
    Pre_io_send_job_game_callback = multi_game_player_mp_flags_snapshot;
    Handshake_game_callback = multi_game_player_mp_flags_handshake;
    Multiplayer = true;
    Multi_state = MS_ON;
    Packet_send_info.next_transmit_group = 0;
    Packet_send_info.group_0_transmit_this_frame = false;
    Packet_send_info.last_transmit_time.set(0);
    Multi_join_in_progress = false;
    Server_pool.init(MP_allocators_mempool);
    Send_qos_info = false;
    Bandwidth_filling_timestamp.invalidate();
    memset(Bandwidth_buckets, 0, sizeof(Bandwidth_buckets));
    Clock_current_time = 0;
    Clock_update_time = 0;
    Prev_frametime = 0;
    multi_io_init(MP_allocators_mempool);
    Bandwidth_enabled = false;
    Bandwidth_ready.invalidate();
    multi_core_dc_init();
    unify_packets(MP_allocators_mempool);
    multi_core_platform_multi_init(MP_allocators_mempool);
    multi_voice_init(game_is_syslink_or_lan);
    Get_dcmo_index_from_game_handle_cb = multi_get_dcmo_index_from_game_handle;
    Set_dcmo_index_for_game_handle_cb = multi_set_dcmo_index_for_game_handle;
    multi_game_init();
    audiolib_sound_bank_load("mp_wep", nullptr, nullptr, 2, 0, true, &MP_allocators_mempool);
    if (!Scoreboard_system_initd)
    {
        // Magic number: 101 scoreboard entries.
        Scoreboard_system_initd = multi_core_platform_internal_scoreboard_init(&MP_allocators_mempool, 101);
        if (!Scoreboard_system_initd)
        {
            keen::debug::nativePrintArgs("Failed to initialize scoreboard system");
        }
    }
    Initd = true;
}

// 0x00A00D40 (:999): the opposite of multi_init; the handshake then moves MS_SHUTTING_DOWN to MS_OFF
void multi_shutdown()
{
    keen::debug::nativePrintArgs("multi_shutdown\n");
    if (!Initd || Multi_state != MS_ON)
    {
        return;
    }
    // Original bug kept: multi_init loads mp_wep as bank type 2 (streaming wavebank), but this looks it up as type 1
    // (in-memory wavebank), so the lookup fails and the bank stays loaded.
    audiolib_sound_bank_unload(audiolib_sound_bank_get_id("mp_wep", true), false, true);
    dlc_exit_mp();
    multi_game_shutdown();
    Remote_objects_pool = nullptr;
    Rop_sort_element = nullptr;
    PTR_rop_sort_element = nullptr;
    Network_game_handles = nullptr;
    Num_reserved_network_handles = 0;
    Cur_network_handle = 1000; // Magic number: the first network handle (lower ones are reserved)
    multi_session_system_shutdown();
    multi_voice_shutdown();
    multi_core_memory_release();
    multi_game_memory_release();
    Server = false;
    Multiplayer = false;
    Initd = false;
    Multi_join_in_progress = false;
    Multi_state = MS_SHUTTING_DOWN;
    Multi_packets = nullptr;
    if (Multi_last_session_start_succeeded)
    {
        g_gameLinkMutex.lock(0);
        Remote_game_session.endGameLinkSession();
        g_gameLinkMutex.unlock();
    }
    if (Scoreboard_system_initd)
    {
        // rfg.exe inlines multi_core_platform_internal_scoreboard_shutdown here.
        Scoreboard_read_progress = POS_FAILED;
        Last_scoreboard_results = nullptr;
        Current_scoreboard_ranking_results.pEntries = nullptr;
        Current_scoreboard_ranking_results.totalRankingEntries = 0;
        Current_scoreboard_ranking_results.filledEntriesCount = 0;
        Max_num_ranking_results = 0;
        Scoreboard_system_initd = false;
    }
}

// 0x009D7D40 (:1060)
static void multi_core_send_job_pre(mp_transmit_list& transmit_list)
{
    multi_core_object_send_job_pre(transmit_list);
    if (Pre_io_send_job_game_callback != nullptr)
    {
        Pre_io_send_job_game_callback();
    }
}

// 0x00A04FB0 (:1076): every frame (not in GS_MULTI_INIT/_SHUTDOWN). The game link learns whether gameplay runs (it
// keeps voice and keep-alives from blocking input), the platform runs, and while multiplayer is on: the clock, the
// incoming packets, the sessions, the server or client load states (or migration / JIP sync), QoS, voice, party, the
// transmit groups, the game's sends, acks and trackers, and the send (a ccjt_sim job in gameplay).
void multi_process(bool booted_to_main_menu)
{
    g_gameLinkMutex.lock(0);
    if (Remote_game_session.isInsideGameLinkSession())
    {
        bool killed = false;
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            killed = pp->is_killed();
        }
        kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
        link->setGameplayRunning(gameseq_state_is_active(GS_GAMEPLAY) && Multi_match_info.match_state == MGMS_GAMEPLAY &&
            control_player_input_enabled() && !player_is_local_player_controls_locked() && !Spectating && !killed &&
            multi_game_data_count_players(NUM_SESSION_TYPES) >= 2 && link->getNumLobbyMembers() >= 2);
    }
    g_gameLinkMutex.unlock();
    multi_core_platform_system_process();
    if ((gameseq_state_is_active(GS_GAMEPLAY) || gameseq_state_is_active(GS_VIDEO_CUTSCENE_PLAY)) && Pending_invite.pending &&
        !Multiplayer)
    {
        if (!gameseq_state_is_active(GS_INGAME_OPTIONS) && !game_booted_to_main_menu())
        {
            if (gamewide_check_and_clear_invite_flag())
            {
                game_boot_to_main_menu(BC_ACCEPTED_INVITE);
            }
            else
            {
                gamewide_process_invite();
            }
        }
    }
    if (!Multiplayer || game_booted_to_main_menu())
    {
        return;
    }
    if (Remote_game_session.isInCriticalErrorState() || rfg_get_local_game_session()->didLocalPlayerExit(nullptr))
    {
        if (Pending_invite.had_invite_when_entering_multiplayer)
        {
            multi_core_platform_clear_pending_invite();
        }
        game_boot_to_main_menu(BC_REMOTE_GAME_SESSION);
        return;
    }
    // A game link error (past the recoverable ones) while the lobby session is still up: back to the main menu.
    bool link_lost = false;
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    if (link != nullptr && (int)link->getError(nullptr) > 24 && Lobby_session != nullptr) // Magic number: 24, the last soft error
    {
        if (Lobby_session->get_state() == MSS_NONE)
        {
            link_lost = link->getCurrentState() == kaiko::GameLinkState_InLobby;
        }
        else
        {
            link_lost = link->getCurrentState() == kaiko::GameLinkState_Online;
        }
    }
    g_gameLinkMutex.unlock();
    if (link_lost)
    {
        game_boot_to_main_menu(BC_REMOTE_GAME_SESSION);
        return;
    }
    multi_core_clock_process();
    Multi_thread_mutex.lock(true);
    multi_io_process_incoming();
    Multi_thread_mutex.unlock();
    multi_connection_update_latencies();
    multi_session_system_process();
    if (!booted_to_main_menu && multi_session_get_by_type(ST_GAME) != nullptr)
    {
        if (Migration_state != MIGRATION_STATE_NONE)
        {
            multi_game_migration_do_frame();
        }
        else if (Multiplayer && Server)
        {
            multi_server_process();
        }
        else if (jip_destruction_sync_state == JIP_DESTRUCTION_STATE_NONE)
        {
            multi_client_process();
        }
        else
        {
            multi_game_destruction_jip_sync_process();
        }
    }
    multi_core_qos_process();
    multi_voice_do_frame();
    multi_game_party_system_process();
    setup_transmit_list(Transmit_list);
    multi_game_process(Transmit_list, Packet_send_info.group_0_transmit_this_frame);
    send_reliable_acks(Transmit_list);
    packet_trackers_process(Transmit_list);
    transmit_packet_id_acks(Transmit_list);
    multi_core_send_job_pre(Transmit_list);
    if (MP_send_process_job && gameseq_state_is_active(GS_GAMEPLAY) && Multi_match_info.match_state == MGMS_GAMEPLAY)
    {
        // The send runs as a sim job; game_handshake_do_frame activates its trigger in the middle of the frame.
        MP_send_setup_cs.enter();
        MP_send_scheduling_trigger = scheduler_get_scheduling_trigger();
        MP_send_process_trigger = scheduler_get_event_trigger();
        cpu_job job;
        job.job_func = multi_io_send_process_job;
        job.type = ccjt_sim;
        scheduler_add_job(MP_send_scheduling_trigger, &job, MP_send_process_trigger);
        MP_send_scheduling_trigger->activate();
        MP_send_scheduling_trigger = nullptr;
        MP_send_setup_cs.leave();
        return;
    }
    multi_io_send_process(Transmit_list);
}

// 0x009FB410 (:1357): the middle of the frame: buffered remote objects are destroyed, the game takes its flags
void multi_handshake()
{
    if (Multi_state == MS_SHUTTING_DOWN)
    {
        Multi_state = MS_OFF;
    }
    else if (Multi_state == MS_ON)
    {
        multi_core_object_destroy_buffered_remote_objects();
        if (Handshake_game_callback != nullptr)
        {
            Handshake_game_callback();
        }
    }
}

// 0x009D7D60 (:1379): magic numbers: a 150 x 150 badge at full scale
void multi_render()
{
    if (Multi_badge_render_my_badge)
    {
        multi_game_badge_render(My_badge, 150, 150, 1.0f, 1.0f);
    }
}

// 0x00971770 (:1388)
void multi_do_post_sim()
{
}

// 0x009432D0 (:1400): the console override or the first address of the host name (host byte order)
int pc_get_my_ip()
{
    if (Console_set_ip != 0)
    {
        return Console_set_ip;
    }
    char host_name[256];
    gethostname(host_name, sizeof(host_name));
#pragma warning(suppress : 4996) // deprecated Winsock call, behaviour kept
    hostent* host = gethostbyname(host_name);
    return (int)ntohl(*(u_long*)host->h_addr_list[0]);
}

// 0x00971780 (:1415)
bool multi_allow_join_in_progress()
{
    return jip_destruction_sync_possible && Migration_state == MIGRATION_STATE_NONE;
}

// 0x00943330 (:1424)
void multi_set_as_server(bool server)
{
    Server = server;
}

// 0x00943340 (:1432)
bool multi_is_server()
{
    return Server;
}

// 0x00943350 (:1437)
bool multi_is_syslink_game()
{
    return Multi_syslink;
}

// 0x00943360 (:1442)
void multi_set_as_dedicated(bool dedicated)
{
    Dedicated = dedicated;
}

// 0x00943370 (:1448)
bool multi_is_dedicated_server()
{
    return Server && Dedicated;
}

// 0x009717A0 (:1463): 0x20 major, the channel, the settings version, 0xFF
unsigned int multi_get_multiplayer_version()
{
    return (((unsigned int)Multi_version_channel << 8 | (unsigned int)Settings_version) << 8) | 0x200000FF;
}

// 0x00943390 (:1476): magic number: 3 transmit groups
void multi_get_transmit_group_info(int& num_transmit_groups, int& transmit_group_count)
{
    num_transmit_groups = Transmit_list.num_transmit_groups;
    transmit_group_count = 3;
}

// 0x00947550: an invalid update time (the uid and values are not set)
upstream_bandwidth_info::upstream_bandwidth_info()
{
}

// 0x00987B20
upstream_bandwidth_info& upstream_bandwidth_info::operator=(const upstream_bandwidth_info& __that)
{
    player_uid = __that.player_uid;
    upstream_bw = __that.upstream_bw;
    upstream_bw_test_limit = __that.upstream_bw_test_limit;
    update_time = __that.update_time;
    return *this;
}

// 0x009717C0 (:1482)
void multi_memory_alloc()
{
    multi_core_memory_alloc();
    multi_game_memory_alloc();
}

// 0x009717D0 (:1488)
void multi_memory_release()
{
    multi_core_memory_release();
    multi_game_memory_release();
}
