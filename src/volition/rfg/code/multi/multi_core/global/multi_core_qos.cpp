// Original: volition/rfg/code/multi/multi_core/global/multi_core_qos.cpp
//
// Quality of service: the ping samples of each connection (the mean ping is the middle of the last ten, every 3 s in
// gameplay), the upstream bandwidth each peer reports (PT_BANDWIDTH_INFO) and, on the host of a game, the adaptive
// upstream: 243 buckets of 4 KB from 0xE000 bits per second, each with a confidence score; packet loss over the
// medium threshold drops the bucket and lowers the scores, loss under the low threshold raises them and climbs.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_join.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"

#include <stdlib.h>
#include <string.h>

// ---- file-static globals
static bool Reset_bandwidth_after_decriment = true; // 0x0167D82F
// unsigned int Max_rate_sec (0x0167DD64) is used here too; placed in multi_core_io.cpp.
// unsigned int Base_bandwidth_kbits (0x0167DD68) is used here too; placed in multi_core_io.cpp.
// unsigned int Base_bandwidth_bytes (0x0167DD6C) is used here too; placed in multi_core_io.cpp.
// unsigned int Max_data_per_packet (0x0167DD70) is used here too; placed in multi_connection.cpp.
static unsigned int My_upstream = 0xE000; // 0x0167DE0C
static float Bandwidth_threshold_low_percent = 0.03f; // 0x0167DE10
static float Bandwidth_threshold_med_percent = 0.05f; // 0x0167DE14
static unsigned int Qos_server_recalculate_upstream_ms = 0x1F4; // 0x0167DE18
static unsigned int Qos_server_recalculate_upstream_after_dec_ms = 0x7D0; // 0x0167DE1C
static unsigned int Qos_server_recalculate_upstream_against_bad_bw_ms = 0x1388; // 0x0167DE20
static unsigned int Qos_server_recalculate_upstream_against_good_bw_ms = 0x9C4; // 0x0167DE24
// int Bandwidth_filling_time_ms (0x0167DE28) is used here too; placed in multi_core_bandwidth.cpp.
// player_bandwidth_info Player_bandwidth[6] (0x0167DE2C) is used here too; placed in multi_core_game_bandwidth.cpp.
// int Num_player_bandwidths (0x0167DE5C) is used here too; placed in multi_core_game_bandwidth.cpp.
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_core_platform.cpp.
// timestamp Bandwidth_ready (0x016916C4) is used here too; placed in multi.cpp.
static timestamp Qos_timestamp; // 0x01691CBC, initial data ffffffff
static timestamp Qos_server_calculate_upstream; // 0x01691CC0, initial data ffffffff
// timestamp Bandwidth_filling_timestamp (0x01691CC4) is used here too; placed in multi.cpp.
static timestamp_realtime Wait_for_upstream_increment_ts; // 0x01691CC8, initial data ffffffff
// short Bandwidth_buckets[243] (0x02FE8108) is used here too; placed in multi.cpp.
// bool Send_qos_info (0x02FE9076) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// bool Bandwidth_enabled (0x02FEB5D1) is used here too; placed in multi.cpp.
static void (*Qos_bw_bandwidth_cb)(int); // 0x02FEB610
static bool Override_stored_qos; // 0x02FEB615
static bool Upstream_set; // 0x02FEB616
static bool Dont_use_qos; // 0x02FEB617
static unsigned int Override_stored_qos_value; // 0x02FEB618
static unsigned int Bandwidth_current_bucket; // 0x02FEB61C

// ---- internal functions
static bool is_valid_session_for_update(multi_session* sp); // 0x00972BF0 (:294)
static unsigned int get_bandwidth_from_bucket(unsigned int bucket); // 0x0094AC50 (:309)
static unsigned int get_bandwidth_bucket(unsigned int bandwidth); // 0x00972C40 (:314)
static void load_qos(); // 0x0094AC60 (:325)
static void send_upstream_bandwidth(multi_session* session); // 0x009B6720 (:337)
static int net_ping_compare(const void* arg1, const void* arg2); // 0x0094AC70 (:354)
static unsigned int update_current_bandwidth_buckets(multi_connection* cp, float packet_loss_percent, unsigned int transmitted_upstream, unsigned int calculated_upstream); // 0x009900F0 (:405)
static void update_bandwidth_from_client(multi_connection* cp); // 0x00972C70 (:619)
static void update_bandwidth(); // 0x009AA100 (:629)
static void qos_init_connections(); // 0x009906A0 (:692)

// File statics of earlier files of the unity this file calls.
static float get_packet_loss_pct(const multi_connection* cp); // multi_connection.cpp
static void calculate_new_bandwidth_values(); // multi_core_io.cpp

// Magic numbers: the lowest bucket is 0xE000 bits per second, each bucket 0x1000 more; 243 buckets (0 to 0xF2);
// confidence scores range from -30 to 10.
static const unsigned int Min_bucket_bandwidth = 0xE000;
static const unsigned int Last_bucket = 0xF2;

// 0x00990030 (:201): console command, every connection back to the lowest upstream
void unused_dcf_mp_reset_upstreams()
{
    if (Console_command == 0)
    {
        return;
    }
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        cp->calculated_upstream_bits_sec = Min_bucket_bandwidth;
    }
    Connection_pool.thread_access.leave();
}

// 0x009900A0 (:211): console command "<bits per second>": a fixed upstream
void unused_dcf_mp_override_stored_qos()
{
    if (Console_command == 0 || !console_get_arg(8, false))
    {
        return;
    }
    Base_bandwidth_kbits = (unsigned int)Console_arg_int >> 10;
    Override_stored_qos_value = Console_arg_int;
    My_upstream = Console_arg_int;
    Base_bandwidth_bytes = (unsigned int)Console_arg_int >> 3;
    Override_stored_qos = true;
    calculate_new_bandwidth_values();
}

// 0x00972B70 (:222): console command, bandwidth filling on or off
void unused_dcf_mp_toggle_fill()
{
    if (Console_command == 0)
    {
        return;
    }
    if (Bandwidth_filling_timestamp.valid() && (!Bandwidth_filling_timestamp.valid() || !Bandwidth_filling_timestamp.elapsed()))
    {
        Bandwidth_filling_timestamp.invalidate();
        return;
    }
    Bandwidth_filling_timestamp.set(Bandwidth_filling_time_ms);
}

// 0x00972BC0 (:235): console command
void unused_dcf_mp_qos_info()
{
    if (Console_command != 0)
    {
        console_printf("bucket: %d, bandwidth: %d", Bandwidth_current_bucket, get_bandwidth_from_bucket(Bandwidth_current_bucket));
    }
}

// 0x0094AC10 (:279)
void register_qos_bw_broadcast_cb(void (*cb)(int))
{
    Qos_bw_bandwidth_cb = cb;
}

// 0x0094AC20 (:284): the last_upstream_adjustment codes
const char* get_bandwidth_adjust_text(int adjust_code)
{
    if (adjust_code == 0)
    {
        return "SAME";
    }
    if (adjust_code == 1)
    {
        return "INC";
    }
    if (adjust_code == 2)
    {
        return "DEC";
    }
    return "INVALID TYPE";
}

// 0x00972BF0 (:294): a READY or IN_GAME session we are in
static bool is_valid_session_for_update(multi_session* sp)
{
    if (sp->get_state() != MSS_READY && sp->get_state() != MSS_IN_GAME)
    {
        return false;
    }
    return sp->get_me_connection() != nullptr;
}

// 0x0094AC50 (:309)
static unsigned int get_bandwidth_from_bucket(unsigned int bucket)
{
    return (bucket + 14) * 0x1000;
}

// 0x00972C40 (:314)
static unsigned int get_bandwidth_bucket(unsigned int bandwidth)
{
    if (bandwidth <= Min_bucket_bandwidth)
    {
        bandwidth = Min_bucket_bandwidth;
    }
    unsigned int bucket = (bandwidth - Min_bucket_bandwidth) >> 12;
    if (bucket > Last_bucket - 1)
    {
        bucket = Last_bucket;
    }
    return bucket;
}

// 0x0094AC60 (:325)
static void load_qos()
{
}

// 0x009B6720 (:337): a client reports its upstream to the session host (PT_BANDWIDTH_INFO, 32 bits big endian)
static void send_upstream_bandwidth(multi_session* session)
{
    unsigned char packet[1200];
    packet[0] = PT_BANDWIDTH_INFO;
    const int header = Multi_packets[PT_BANDWIDTH_INFO].group_id != 0 ? 5 : 3;
    packet[header] = (unsigned char)(My_upstream >> 24);
    packet[header + 1] = (unsigned char)(My_upstream >> 16);
    packet[header + 2] = (unsigned char)(My_upstream >> 8);
    packet[header + 3] = (unsigned char)My_upstream;
    const unsigned short payload = (unsigned short)(header + 1);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_reliable(session->get_host_connection(), packet, header + 4, nullptr, nullptr, nullptr);
}

// 0x0094AC70 (:354)
static int net_ping_compare(const void* arg1, const void* arg2)
{
    const int a = *(const int*)arg1;
    const int b = *(const int*)arg2;
    if (a == b)
    {
        return 0;
    }
    return a > b ? 1 : -1;
}

// 0x0094ACA0 (:366)
unsigned int multi_core_qos_get_min_game_bandwidth()
{
    return Player_bandwidth[0].bandwidth;
}

// 0x0094ACB0 (:371)
unsigned int multi_core_qos_get_max_game_bandwidth()
{
    return Player_bandwidth[Num_player_bandwidths - 1].bandwidth;
}

// 0x0094ACC0 (:378): the first entry for at least this many players, else the largest
unsigned int multi_get_required_bandwidth_for_players(unsigned int num_players)
{
    for (int i = 0; i < Num_player_bandwidths; i++)
    {
        if (num_players <= Player_bandwidth[i].num_players)
        {
            return Player_bandwidth[i].bandwidth;
        }
    }
    return Player_bandwidth[Num_player_bandwidths - 1].bandwidth;
}

// 0x0094ACF0 (:392): the most players this bandwidth serves (at least the first entry's)
unsigned int multi_get_num_players_from_bandwidth(unsigned int bandwidth_bits_per_sec)
{
    for (int i = Num_player_bandwidths - 1; i >= 0; i--)
    {
        if (bandwidth_bits_per_sec >= Player_bandwidth[i].bandwidth)
        {
            return Player_bandwidth[i].num_players;
        }
    }
    return Player_bandwidth[0].num_players;
}

// 0x009900F0 (:405): the host's upstream from its packet loss. The thresholds scale down from 1 to 0.25 as the bucket
// rises from the one the player count needs to the top. Over the medium threshold: the bucket falls to the measured
// rate (at most 2 below the current one, not below the needed one when above it) and the scores of the current and
// next bucket and of every bucket dropped through fall by the loss (the dropped ones less the further they are). Under
// the low threshold (once the wait elapsed, and while the measured rate is within 17% of the computed one): the next
// bucket's score recovers, or the bucket climbs up to 3 over buckets of good scores, and the scores at and below rise.
// Then the wait is set by the next bucket's score, filling starts below the needed bucket, and the bucket never falls
// below 70% of the needed bandwidth. rfg.exe computes the scales in x87 doubles.
static unsigned int update_current_bandwidth_buckets(multi_connection* cp, float packet_loss_percent, unsigned int transmitted_upstream,
    unsigned int calculated_upstream)
{
    unsigned int players = cp->owner->get_slot_use_data().used;
    if (players <= Simulate_n_players_online)
    {
        players = Simulate_n_players_online;
    }
    const unsigned int required = multi_get_required_bandwidth_for_players(players);
    const unsigned int target_bucket = get_bandwidth_bucket(required);
    unsigned int bucket = Bandwidth_current_bucket;
    float scale = (float)((double)(int)(bucket - target_bucket) / (double)(0xF3u - target_bucket) * -0.75 + 1.0);
    cp->last_upstream_adjustment = 0;
    if ((float)((double)scale - 0.25) < 0.0f)
    {
        scale = 0.25f;
    }
    if ((float)((double)scale - 1.0) >= 0.0f)
    {
        scale = 1.0f;
    }
    if ((double)packet_loss_percent > (double)Bandwidth_threshold_med_percent * (double)scale)
    {
        unsigned int new_bucket = get_bandwidth_bucket(multi_core_connection_get_bandwidth_bits_sec(cp, true, 2500));
        if (new_bucket >= bucket)
        {
            new_bucket = (int)(bucket - 2) < 0 ? 0 : bucket - 2;
        }
        if (new_bucket < target_bucket)
        {
            if (bucket > target_bucket)
            {
                new_bucket = target_bucket;
            }
            else if (new_bucket < bucket - 2)
            {
                new_bucket = bucket - 2;
            }
        }
        if (Reset_bandwidth_after_decriment)
        {
            Connection_pool.thread_access.enter();
            for (multi_connection* other = Connection_pool.get_first(0); other != nullptr; other = Connection_pool.get_next(other))
            {
                if (other->connection_status == CS_DISCONNECTED)
                {
                    multi_core_connection_reset_packet_loss(other);
                }
            }
            Connection_pool.thread_access.leave();
            bucket = Bandwidth_current_bucket;
        }
        // Magic numbers: 20% loss costs 10 points.
        const short penalty = (short)(unsigned short)(int)((double)packet_loss_percent / (double)0.2f * 10.0 + 0.0);
        if (bucket < Last_bucket)
        {
            short score = (short)(Bandwidth_buckets[bucket + 1] - penalty - 2);
            if (score <= -30)
            {
                score = -30;
            }
            Bandwidth_buckets[bucket + 1] = score;
        }
        const unsigned int old_bucket = bucket;
        if (bucket == new_bucket)
        {
            short score = (short)(Bandwidth_buckets[bucket] - penalty);
            if (score <= -30)
            {
                score = -30;
            }
            Bandwidth_buckets[bucket] = score;
        }
        else if (bucket > new_bucket)
        {
            const float span = (float)(double)(bucket - new_bucket);
            for (unsigned int distance = bucket - new_bucket; bucket > new_bucket; bucket--, distance--)
            {
                const float share = (float)((double)distance / (double)span);
                const short drop = (short)(int)((double)share * 4.0 + 0.0);
                short score = (short)(Bandwidth_buckets[bucket] - drop);
                if (score <= -30)
                {
                    score = -30;
                }
                Bandwidth_buckets[bucket] = score;
            }
            Bandwidth_current_bucket = bucket;
        }
        if (old_bucket != 0)
        {
            cp->last_upstream_adjustment = 2;
        }
    }
    else if ((double)Bandwidth_threshold_low_percent * (double)scale > (double)packet_loss_percent)
    {
        if (!Wait_for_upstream_increment_ts.valid() || Wait_for_upstream_increment_ts.elapsed())
        {
            cp->last_upstream_adjustment = 0;
            if (transmitted_upstream > calculated_upstream ||
                0.17f > (double)(calculated_upstream - transmitted_upstream) / (double)calculated_upstream) // Magic number: 17%
            {
                bucket = Bandwidth_current_bucket;
                if (bucket + 1 != Last_bucket + 1)
                {
                    const short next_score = Bandwidth_buckets[bucket + 1];
                    if (next_score <= -9)
                    {
                        short score = (short)(next_score + 3);
                        if (score >= 10)
                        {
                            score = 10;
                        }
                        Bandwidth_buckets[bucket + 1] = score;
                    }
                    else
                    {
                        if (next_score <= -2 || bucket + 2 == Last_bucket + 1 || Bandwidth_buckets[bucket + 2] <= -2)
                        {
                            bucket = bucket + 1;
                        }
                        else if (bucket + 3 == Last_bucket + 1 || Bandwidth_buckets[bucket + 3] <= -2)
                        {
                            bucket = bucket + 2;
                        }
                        else
                        {
                            bucket = bucket + 3;
                        }
                        Bandwidth_current_bucket = bucket;
                        cp->last_upstream_adjustment = 1;
                    }
                }
                for (int b = cp->last_upstream_adjustment != 0 ? (int)bucket - 1 : (int)bucket; b >= 0; b--)
                {
                    short score = (short)(Bandwidth_buckets[b] + 3);
                    if (score >= 10)
                    {
                        score = 10;
                    }
                    Bandwidth_buckets[b] = score;
                }
            }
        }
    }
    bucket = Bandwidth_current_bucket;
    if (bucket + 1 != Last_bucket + 1)
    {
        if (!Wait_for_upstream_increment_ts.valid() || Wait_for_upstream_increment_ts.elapsed())
        {
            Wait_for_upstream_increment_ts.set(Bandwidth_buckets[Bandwidth_current_bucket + 1] <= -9 ? Qos_server_recalculate_upstream_against_bad_bw_ms :
                Qos_server_recalculate_upstream_against_good_bw_ms);
        }
        bucket = Bandwidth_current_bucket;
    }
    if (bucket < target_bucket && bucket + 1 != Last_bucket + 1 && Bandwidth_buckets[bucket + 1] > -9)
    {
        Bandwidth_filling_timestamp.set(Bandwidth_filling_time_ms);
        bucket = Bandwidth_current_bucket;
    }
    // Magic number: never below 70% of what the player count needs (x87, truncated through 64 bits).
    const unsigned int floor_bucket = get_bandwidth_bucket((unsigned int)(int64_t)((double)required * (double)0.7f));
    if (floor_bucket > bucket)
    {
        bucket = floor_bucket;
        Bandwidth_current_bucket = floor_bucket;
    }
    return (bucket + 14) << 12;
}

// 0x00972C70 (:619)
static void update_bandwidth_from_client(multi_connection* cp)
{
    if (cp->owner->get_me_connection() != cp)
    {
        cp->calculated_upstream_bits_sec = Min_bucket_bandwidth;
    }
}

// 0x009AA100 (:629): the host of a game with others and filling enabled adapts its upstream (after a decrease it
// waits longer)
static void update_bandwidth()
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp != nullptr && is_valid_session_for_update(sp))
    {
        if (!sp->am_i_host() || sp->get_slot_use_data().used < 2 || !multi_core_bandwidth_is_enabled())
        {
            Qos_server_calculate_upstream.set(Qos_server_recalculate_upstream_ms);
            return;
        }
        multi_connection* me = sp->get_me_connection();
        const float loss = compute_server_packet_loss(me);
        const unsigned int sent = multi_core_connection_get_bandwidth_bits_sec(me, false, 2500);
        const unsigned int upstream = update_current_bandwidth_buckets(me, loss, sent, me->calculated_upstream_bits_sec);
        me->calculated_upstream_bits_sec = upstream;
        multi_io_set_bandwidth_in_bits_sec(upstream);
        if (me->last_upstream_adjustment == 2)
        {
            Qos_server_calculate_upstream.set(Qos_server_recalculate_upstream_after_dec_ms);
            return;
        }
    }
    Qos_server_calculate_upstream.set(Qos_server_recalculate_upstream_ms);
}

// 0x0094AD20 (:679)
void multi_core_qos_system_init()
{
    Send_qos_info = false;
    Bandwidth_filling_timestamp.invalidate();
    memset(Bandwidth_buckets, 0, sizeof(Bandwidth_buckets));
}

// 0x0094AD50 (:688)
void multi_core_qos_system_shutdown()
{
}

// 0x009906A0 (:692): a new connection starts at our stored upstream (our own, with a fresh bandwidth slot at one
// packet's worth per send) or at the lowest bucket (a peer)
static void qos_init_connections()
{
    Connection_pool.thread_access.enter();
    const unsigned int per_packet = Max_data_per_packet;
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp->qos_inited != 0)
        {
            continue;
        }
        cp->qos_inited = 1;
        if (cp == cp->owner->get_me_connection())
        {
            cp->calculated_upstream_bits_sec = multi_core_qos_up_bandwidth(nullptr);
            cp->available_bandwidth_tracker.reset(Max_rate_sec * per_packet);
        }
        else
        {
            cp->calculated_upstream_bits_sec = Min_bucket_bandwidth;
        }
    }
    Connection_pool.thread_access.leave();
}

// 0x009B67F0 (:712): the clients report their upstream once, new connections are set up, in gameplay the mean pings
// are updated every 3 s, and the host's upstream is recalculated when due
void multi_core_qos_process()
{
    if (Send_qos_info)
    {
        Send_qos_info = false;
        Session_pool.thread_access.enter();
        for (multi_session* sp = Session_pool.get_first(0); sp != nullptr; sp = Session_pool.get_next(sp))
        {
            if (sp->get_state() == MSS_NONE || !is_valid_session_for_update(sp))
            {
                continue;
            }
            multi_connection* me = sp->get_me_connection();
            if (!sp->am_i_host() && me != nullptr && me->qos_inited == 1)
            {
                me->qos_inited = 2;
                send_upstream_bandwidth(sp);
            }
        }
        Session_pool.thread_access.leave();
    }
    qos_init_connections();
    if ((!Qos_timestamp.valid() || Qos_timestamp.elapsed()) && gameseq_get_state() == GS_GAMEPLAY)
    {
        Qos_timestamp.set(3000);
        Connection_pool.thread_access.enter();
        for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
        {
            if (cp->num_ping_samples > 9)
            {
                int pings[10];
                memcpy(pings, cp->pings, sizeof(pings));
                qsort(pings, 10, 4, net_ping_compare);
                // Original behaviour kept: the 6th and 7th of the sorted ten, not the middle two.
                cp->mean_ping = (pings[6] + pings[5]) / 2;
            }
        }
        Connection_pool.thread_access.leave();
    }
    if (!Qos_server_calculate_upstream.valid() || Qos_server_calculate_upstream.elapsed())
    {
        update_bandwidth();
    }
}

// 0x009907C0 (:768)
int multi_core_qos_find_latency_by_game_data(unsigned int game_data)
{
    Connection_pool.thread_access.enter();
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp->game_data == game_data)
        {
            const unsigned int ping = cp->mean_ping;
            Connection_pool.thread_access.leave();
            return (int)ping;
        }
    }
    Connection_pool.thread_access.leave();
    return 0;
}

// 0x0094AD60 (:782)
unsigned int multi_core_qos_latency_ms(multi_connection* cp)
{
    return cp->mean_ping;
}

// 0x00972C90 (:788): PT_QOS on the server: the client's game upstream (32 bits big endian); the sender is not checked
// for null (as in rfg.exe)
void multi_join_process_qos_packet(unsigned char* packet_data, const sender& from)
{
    if (Multiplayer && !Server)
    {
        return;
    }
    multi_connection* cp = from.connection;
    cp->game_upstream_bits_sec = (unsigned int)packet_data[0] << 24 | (unsigned int)packet_data[1] << 16 | (unsigned int)packet_data[2] << 8 | packet_data[3];
    update_bandwidth_from_client(cp);
}

// 0x00972D20 (:817): a peer's reported upstream, else (ourselves, the host) our stored one (at least the lowest bucket)
unsigned int multi_core_qos_up_bandwidth(multi_connection* cp)
{
    if (cp != nullptr && cp != cp->owner->get_host_connection())
    {
        return cp->upstream_bits_sec;
    }
    return My_upstream <= Min_bucket_bandwidth ? Min_bucket_bandwidth : My_upstream;
}

// 0x009E4080 (:831): PT_BANDWIDTH_INFO: a peer's upstream; the others learn it with the connection update
void multi_core_qos_bandwidth_info(unsigned char* packet_data, const sender& from)
{
    multi_connection* cp = from.connection;
    if (cp == nullptr)
    {
        return;
    }
    cp->upstream_bits_sec = (unsigned int)packet_data[0] << 24 | (unsigned int)packet_data[1] << 16 | (unsigned int)packet_data[2] << 8 | packet_data[3];
    if (cp->game_data != 0xFFFFFFFFu)
    {
        multi_join_send_updated_connection_packet_to_all(cp);
    }
}

// 0x0094AD70 (:850)
float multi_core_qos_packet_loss(multi_connection* cp)
{
    return get_packet_loss_pct(cp);
}

// 0x0094AD90 (:860): the last ten samples
void multi_core_qos_report_ping_time(multi_connection* cp, unsigned int ping_ms)
{
    cp->num_ping_samples++;
    cp->pings[cp->ping_pos] = (int)ping_ms;
    cp->ping_pos++;
    if (cp->ping_pos > 9)
    {
        cp->ping_pos = 0;
    }
}

// 0x0094ADD0 (:878)
unsigned int multi_core_qos_game_upstream_bandwidth(multi_connection* cp)
{
    return cp->calculated_upstream_bits_sec;
}

// 0x0094ADE0 (:885)
void multi_core_qos_render()
{
}

// 0x0094ADF0 (:1076): no stored value on PC
unsigned int multi_core_qos_get_stored_upstream_bandwidth()
{
    return Min_bucket_bandwidth;
}

// 0x00990830 (:1108): a tested upstream: our connections take it, the clients send it, the bucket and the base
// bandwidth follow
void multi_core_qos_set_stored_upstream_bandwidth(unsigned int upstream_bandwidth)
{
    My_upstream = upstream_bandwidth;
    Upstream_set = true;
    Send_qos_info = true;
    Connection_pool.thread_access.enter();
    const unsigned int upstream = My_upstream;
    for (multi_connection* cp = Connection_pool.get_first(0); cp != nullptr; cp = Connection_pool.get_next(cp))
    {
        if (cp == cp->owner->get_me_connection())
        {
            cp->calculated_upstream_bits_sec = upstream;
        }
    }
    Connection_pool.thread_access.leave();
    Bandwidth_current_bucket = get_bandwidth_bucket(upstream_bandwidth);
    Base_bandwidth_kbits = ((Bandwidth_current_bucket + 14) & 0xFFFFF) << 2;
    Base_bandwidth_bytes = ((Bandwidth_current_bucket + 14) & 0xFFFFF) << 9;
    calculate_new_bandwidth_values();
}

// 0x0094AE00 (:1151): filling runs while its timestamp has not elapsed (once bandwidth is enabled and ready)
bool multi_core_qos_bandwidth_filling_is_on()
{
    if (!Bandwidth_enabled || !Bandwidth_ready.elapsed())
    {
        return false;
    }
    return Bandwidth_filling_timestamp.valid() && !Bandwidth_filling_timestamp.elapsed();
}

// 0x0094AE40 (:1161)
void multi_core_qos_bandwidth_filling_enable()
{
    Bandwidth_filling_timestamp.set(Bandwidth_filling_time_ms);
}

// 0x0094AE60 (:1168)
void multi_core_qos_query_live()
{
}

// 0x0094AE70 (:1175)
void multi_core_qos_live_process()
{
}

// 0x0094AE80 (:1210)
void multi_core_qos_done_using()
{
    Dont_use_qos = true;
}

// 0x009AA230 (:1216): at the start of a game our host connection assumes the bandwidth the player count needs (when
// our stored upstream is lower) and the bucket climbs to it over buckets with bad scores. The mode name lookup is the
// rest of a stripped log line.
void multi_core_qos_game_started()
{
    if (!Multiplayer)
    {
        return;
    }
    multi_connection* host = multi_session_get_by_type(ST_GAME)->get_host_connection();
    if (host == nullptr)
    {
        return;
    }
    const unsigned int required = multi_get_required_bandwidth_for_players(host->owner->get_slot_use_data().used);
    if (required > My_upstream)
    {
        host->calculated_upstream_bits_sec = required;
        const unsigned int bucket = get_bandwidth_bucket(required);
        if (bucket > Bandwidth_current_bucket)
        {
            if (bucket == 0)
            {
                Bandwidth_current_bucket = 0;
            }
            else
            {
                while (Bandwidth_current_bucket < bucket - 1 && Bandwidth_buckets[Bandwidth_current_bucket + 1] < -9)
                {
                    Bandwidth_current_bucket++;
                }
            }
        }
    }
    if (Multiplayer && (int)Multi_gameplay_mode > -1 && (int)Multi_gameplay_mode < 8)
    {
        localization_get_display_string_req(Multi_gameplay_mode_names[Multi_gameplay_mode].display_name, nullptr);
        return;
    }
    localization_get_display_string_req(nullptr, nullptr);
}
