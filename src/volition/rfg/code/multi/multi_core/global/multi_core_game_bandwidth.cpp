// Original: volition/rfg/code/multi/multi_core/global/multi_core_game_bandwidth.cpp
//
// Upstream bandwidth probing. Each player's tested upstream and its test limit are tracked per uid (30 slots, slot 0
// is ours; values are kept in 4 KB steps). While a party or game session is READY (not in an active game), the host
// manages the tests: it tests itself, then picks a peer (PT_BW_TEST_INFO) to test; the tested station floods its
// peers with PT_BW_TESTER packets at rates found by a binary search over multiples of Game_bw_test_size and watches
// their packet loss; the results go to the host (PT_FLOOD_UPDATE) and from the host to everyone
// (PT_FLOOD_FULL_RESULTS).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py). Globals with a
// dynamic initializer are placed by the initializer's source line (rfg.pdb).

#include "volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/vlib/code/util/varray.h"

#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x14
struct binary_search_info
{
    int start_index; // 0x00
    int end_index; // 0x04
    int cur_index; // 0x08
    int pass_index; // 0x0C
    int fail_index; // 0x10
};

// size 0x8
struct player_bandwidth_info
{
    unsigned int num_players; // 0x00
    unsigned int bandwidth; // 0x04
};

enum test_bw_result
{
    TEST_RESULT_UNKNOWN = 0,
    TEST_RESULT_SUCCESS = 1,
    TEST_RESULT_FAIL = 2,
    TEST_RESULT_COULD_NOT_TEST = 3,
    TEST_RESULT_FORCE_STOP = 4,
};

enum test_bw_states
{
    TEST_BW_START = 0,
    TEST_BW_TRANSMIT_DATA = 1,
    TEST_BW_FINISHED_DATA = 2,
    TEST_BW_DONE = 3,
};

enum test_management_state
{
    TM_NONE = 0,
    TM_WAIT_MIN_PLAYER = 1,
    TM_TEST_SELF = 2,
    TM_SELECT_TARGET = 3,
    TM_TEST_TARGET = 4,
};

// ---- file-static globals
static bool Bandwidth_probing_enabled = true; // 0x0167D82D
static session_types Game_bandwidth_test_session_type = ST_INVALID; // 0x0167D83C, initial data ffffffff
// int Game_bw_test_size (0x0167D840) is used here too; placed in multi.cpp.
static int BW_min_players_to_test = 3; // 0x0167D844
// The bandwidth for a player count (bits per second); the last entry is the most a game asks for.
static player_bandwidth_info Player_bandwidth[6] = {
    {4, 0x10000}, {6, 0x1B800}, {8, 0x34800}, {10, 0x48800}, {12, 0x5A000}, {16, 0x70800}}; // 0x0167DE2C
static int Num_player_bandwidths = 6; // 0x0167DE5C
static timestamp_realtime Game_bandwidth_send_stamp; // 0x01691790, initial data ffffffff
static timestamp_realtime Game_bandwidth_test_duration; // 0x01691794, initial data ffffffff
static timestamp_realtime Game_last_test_packet_rcvd_time; // 0x01691798, initial data ffffffff
static timestamp_realtime Game_bandwidth_test_start_time; // 0x0169179C, initial data ffffffff
// The tracked bandwidth of each player uid (slot 0 is ours): a file static, defined here by its initializer's line and
// used by the earlier multi.cpp (here with external linkage, see multi.cpp).
farray<upstream_bandwidth_info,30> Tracked_upstream_bandwidth_info_list; // 0x016917A0 (:172)
static int Min_game_bw_bps; // 0x02FE6D80
static int Max_game_test_bw_bps; // 0x02FE8104
static binary_search_info Game_bandwidth_binary_search_info; // 0x02FE8DA0
static test_bw_states Game_bandwidth_state; // 0x02FE8DC8
static test_bw_result Bandwidth_test_result; // 0x02FE8E58
static int Max_game_bw_bps; // 0x02FEB384
static test_management_state Tm_state; // 0x02FEB568
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// ---- internal functions
static unsigned int compress_bandwidth(unsigned int in_bandwidth); // 0x00972240 (:226)
static unsigned int decompress_bandwidth(unsigned int compressed_bw); // 0x00947530 (:234)
static unsigned int compress_time(unsigned int time_ms); // 0x00972250 (:239)
static unsigned int decompress_time(unsigned int compressed_time); // 0x00947540 (:247)
static void send_flood_bw_result(multi_session* sp, int flood_send, int flood_limit, int age_ms); // 0x009D8610 (:299)
static void tracked_bandwidth_slot_reset(int slot_index); // 0x0098F890 (:317)
static int tracked_bandwidth_info_get_slot_index(const multi_player_uid& player_uid); // 0x00972270 (:336)
static int tracked_bandwidth_info_add_slot(const multi_player_uid& player_uid); // 0x009A9000 (:352)
static multi_session* get_active_flood_session(); // 0x009A91A0 (:469)
static void send_bandwidth_test_stop(); // 0x009D87A0 (:491)
static bool is_valid_bwtest_connection(const multi_connection* cp); // 0x00947560 (:506)
static int get_bw_test_connection_send_count(const multi_session* sp); // 0x00972320 (:518)
static int get_search_param_index_from_bandwidth(int bandwidth); // 0x00947590 (:536)
static int round_up_connection_bw_to_send_multiple(int in_bw); // 0x009475A0 (:547)
static bool should_test_connection(const multi_player_uid& uid, test_target_info& target_info, int send_connection_count); // 0x00972380 (:554)
static multi_connection* choose_bw_test_target(const multi_session* sp, int connection_send_count); // 0x00972420 (:590)
static void bandwidth_process_management(const multi_session* sp); // 0x009D8860 (:670)
static void reset_test(); // 0x009475C0 (:759)
static bool adjust_binary_search_params(bool test_successful); // 0x009725A0 (:772)
static void decrease_binary_search_params_ceiling(); // 0x009475F0 (:797)
static void init_binary_search_params(int max_bw_multiple); // 0x00947660 (:813)
static int compute_max_send_bandwidth_bps(int num_send_targets); // 0x009476A0 (:824)
static void update_platform_computed_bandwidth(multi_session* sp); // 0x009476B0 (:832)
static bool send_maxed_out(int send_max_multiple, multi_session* sp); // 0x009D8A30 (:866)
static void test_bandwidth_process(multi_session* sp); // 0x009D8AE0 (:892)

// File statics of the earlier multi_connection.cpp this file calls.
static float get_packet_loss_pct(const multi_connection* cp);

// 0x00972240 (:226): 4 KB steps in 9 bits
static unsigned int compress_bandwidth(unsigned int in_bandwidth)
{
    unsigned int compressed = in_bandwidth >> 12;
    if (compressed > 0x1FE)
    {
        compressed = 0x1FF;
    }
    return compressed;
}

// 0x00947530 (:234)
static unsigned int decompress_bandwidth(unsigned int compressed_bw)
{
    return compressed_bw << 12;
}

// 0x00972250 (:239): minutes in 9 bits
static unsigned int compress_time(unsigned int time_ms)
{
    unsigned int compressed = time_ms / 60000;
    if (compressed > 0x1FE)
    {
        compressed = 0x1FF;
    }
    return compressed;
}

// 0x00947540 (:247)
static unsigned int decompress_time(unsigned int compressed_time)
{
    return compressed_time * 60000;
}

// 0x009D83F0 (:252): the host sends every tracked player of the session (PT_FLOOD_FULL_RESULTS): a count, then 32 bits
// each (session index 5, upstream 9, test limit 9, age in minutes 9)
void multi_game_send_full_bw_results(multi_session* sp)
{
    if (!sp->am_i_host())
    {
        return;
    }
    unsigned char bits[64];
    bit_packer packer(bits, 64);
    int upstream_bw;
    int test_limit;
    int age_ms;
    int count = 0;
    multi_connection* cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            break;
        }
        const multi_player_uid& uid = cp == sp->get_me_connection() ? My_player_uid : cp->player_uid;
        if (tracked_bandwidth_info_get_info(uid, upstream_bw, test_limit, age_ms) && cp->index_in_session != 0xFF)
        {
            count++;
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
    unsigned char packet[1200];
    packet[0] = PT_FLOOD_FULL_RESULTS;
    unsigned int size = Multi_packets[PT_FLOOD_FULL_RESULTS].group_id != 0 ? 5 : 3;
    packet[size] = (unsigned char)count;
    size++;
    cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            break;
        }
        const multi_player_uid& uid = cp == sp->get_me_connection() ? My_player_uid : cp->player_uid;
        if (tracked_bandwidth_info_get_info(uid, upstream_bw, test_limit, age_ms) && cp->index_in_session != 0xFF)
        {
            packer.set_next_bits(cp->index_in_session, 5);
            packer.set_next_bits(compress_bandwidth(upstream_bw), 9);
            packer.set_next_bits(compress_bandwidth(test_limit), 9);
            packer.set_next_bits(compress_time(age_ms), 9);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
    for (int i = 0; i < count * 4; i++)
    {
        packet[size & 0xFFFF] = bits[i];
        size++;
    }
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_to_all_reliable(sp, packet, size & 0xFFFF);
}

// 0x009D8610 (:299): a client tells its host its test result (PT_FLOOD_UPDATE: our uid, then the upstream, the limit
// and the age, each 32 bits big endian)
static void send_flood_bw_result(multi_session* sp, int flood_send, int flood_limit, int age_ms)
{
    unsigned char packet[1200];
    packet[0] = PT_FLOOD_UPDATE;
    unsigned short size = Multi_packets[PT_FLOOD_UPDATE].group_id != 0 ? 5 : 3;
    My_player_uid.write_to_packet(packet, size);
    const unsigned int values[3] = {(unsigned int)flood_send, (unsigned int)flood_limit, (unsigned int)age_ms};
    for (int v = 0; v < 3; v++)
    {
        packet[size] = (unsigned char)(values[v] >> 24);
        packet[size + 1] = (unsigned char)(values[v] >> 16);
        packet[size + 2] = (unsigned char)(values[v] >> 8);
        packet[size + 3] = (unsigned char)values[v];
        size += 4;
    }
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    if (!sp->am_i_host() && sp->get_host_connection() != nullptr)
    {
        multi_io_send_reliable(sp->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
    }
}

// 0x0098F890 (:317): an empty slot at the test size
static void tracked_bandwidth_slot_reset(int slot_index)
{
    if ((unsigned int)slot_index >= 30)
    {
        return;
    }
    upstream_bandwidth_info& info = Tracked_upstream_bandwidth_info_list[slot_index];
    info.player_uid.invalidate();
    info.upstream_bw = Game_bw_test_size;
    info.upstream_bw_test_limit = Game_bw_test_size;
    info.update_time.invalidate();
}

// 0x00972270 (:336): our own uid is slot 0
static int tracked_bandwidth_info_get_slot_index(const multi_player_uid& player_uid)
{
    if (My_player_uid.get_internal_id().is_equal(player_uid.get_internal_id()))
    {
        return 0;
    }
    for (int i = 1; i < Tracked_upstream_bandwidth_info_list.size(); i++)
    {
        if (Tracked_upstream_bandwidth_info_list[i].player_uid.get_internal_id().is_equal(player_uid.get_internal_id()))
        {
            return i;
        }
    }
    return -1;
}

// 0x009A9000 (:352): a new slot, or when all 30 are used the one updated longest ago (slot 29 when none is older
// than now)
static int tracked_bandwidth_info_add_slot(const multi_player_uid&)
{
    const int count = Tracked_upstream_bandwidth_info_list.size();
    if (count < Tracked_upstream_bandwidth_info_list.capacity())
    {
        upstream_bandwidth_info info;
        Tracked_upstream_bandwidth_info_list.add(info);
        return count;
    }
    int oldest = -1;
    int oldest_age = 0;
    for (int i = 1; i < 30; i++)
    {
        const int age = Tracked_upstream_bandwidth_info_list[i].update_time.time_since();
        if (oldest_age < age)
        {
            oldest_age = age;
            oldest = i;
        }
    }
    if (oldest == -1)
    {
        oldest = 29;
    }
    tracked_bandwidth_slot_reset(oldest);
    return oldest;
}

// 0x009B58B0 (:395): only our own slot, at the test size
void tracked_bandwidth_info_system_init()
{
    Tracked_upstream_bandwidth_info_list.remove_all();
    upstream_bandwidth_info info;
    Tracked_upstream_bandwidth_info_list.add(info);
    tracked_bandwidth_info_set_info(My_player_uid, Game_bw_test_size, Game_bw_test_size, 0);
}

// 0x009A90C0 (:407): a new slot takes the values; an existing one only a failing test (upstream below its limit) or a
// higher upstream. The age back-dates the update time.
void tracked_bandwidth_info_set_info(const multi_player_uid& player_uid, int upstream_bw, int upstream_bw_test_limit, int age_ms)
{
    bool created = false;
    int slot = tracked_bandwidth_info_get_slot_index(player_uid);
    if (slot == -1)
    {
        slot = tracked_bandwidth_info_add_slot(player_uid);
        if (slot == -1)
        {
            return;
        }
        Tracked_upstream_bandwidth_info_list[slot].player_uid = player_uid;
        created = true;
    }
    int old_upstream = 0;
    const int found = tracked_bandwidth_info_get_slot_index(player_uid);
    if (found != -1)
    {
        old_upstream = Tracked_upstream_bandwidth_info_list[found].upstream_bw;
        Tracked_upstream_bandwidth_info_list[found].update_time.time_since();
    }
    if (created || upstream_bw < upstream_bw_test_limit || upstream_bw > old_upstream)
    {
        upstream_bandwidth_info& info = Tracked_upstream_bandwidth_info_list[slot];
        info.upstream_bw = (int)(compress_bandwidth((unsigned int)upstream_bw) << 12);
        info.upstream_bw_test_limit = (int)(compress_bandwidth((unsigned int)upstream_bw_test_limit) << 12);
        info.update_time.set(-age_ms);
    }
}

// 0x009722D0 (:454)
bool tracked_bandwidth_info_get_info(const multi_player_uid& player_uid, int& upstream_bw, int& upstream_bw_test_limit, int& age_ms)
{
    const int slot = tracked_bandwidth_info_get_slot_index(player_uid);
    if (slot == -1)
    {
        return false;
    }
    upstream_bw = Tracked_upstream_bandwidth_info_list[slot].upstream_bw;
    upstream_bw_test_limit = Tracked_upstream_bandwidth_info_list[slot].upstream_bw_test_limit;
    age_ms = Tracked_upstream_bandwidth_info_list[slot].update_time.time_since();
    return true;
}

// 0x009A91A0 (:469): the tested session while it is READY
static multi_session* get_active_flood_session()
{
    session_types type;
    switch (Game_bandwidth_test_session_type)
    {
    case ST_PARTY:
        type = ST_PARTY;
        break;
    case ST_GAME:
        type = ST_GAME;
        break;
    default:
        return nullptr;
    }
    multi_session* sp = multi_session_get_by_type(type);
    if (sp != nullptr && sp->get_state() != MSS_READY)
    {
        return nullptr;
    }
    return sp;
}

// 0x009D87A0 (:491): the host stops the tests (PT_BW_TEST_INFO 0)
static void send_bandwidth_test_stop()
{
    session_types type;
    switch (Game_bandwidth_test_session_type)
    {
    case ST_PARTY:
        type = ST_PARTY;
        break;
    case ST_GAME:
        type = ST_GAME;
        break;
    default:
        return;
    }
    multi_session* sp = multi_session_get_by_type(type);
    if (sp == nullptr || sp->get_state() != MSS_READY || !sp->am_i_host())
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_BW_TEST_INFO;
    const int header = Multi_packets[PT_BW_TEST_INFO].group_id != 0 ? 5 : 3;
    packet[header] = 0;
    const unsigned short payload = (unsigned short)(header - 2);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_to_all_reliable(sp, packet, header + 1);
}

// 0x00947560 (:506): connected for more than 3 s
static bool is_valid_bwtest_connection(const multi_connection* cp)
{
    if (cp->connection_status != CS_CONNECTED && cp->connection_status != CS_CONNECTED_GAMEPLAY)
    {
        return false;
    }
    return timer_get(TIMER_MILLISECONDS) - cp->join_time > 3000;
}

// 0x00972320 (:518): the peers a test floods
static int get_bw_test_connection_send_count(const multi_session* sp)
{
    multi_connection* me = sp->get_me_connection();
    int count = 0;
    multi_connection* cp = sp->get_connection_list();
    while (cp != nullptr)
    {
        if (cp != me && is_valid_bwtest_connection(cp))
        {
            count++;
        }
        cp = cp->next;
        if (cp == sp->get_connection_list())
        {
            break;
        }
    }
    return count;
}

// 0x00947590 (:536)
static int get_search_param_index_from_bandwidth(int bandwidth)
{
    return (Game_bw_test_size - 1 + bandwidth) / Game_bw_test_size;
}

// 0x009475A0 (:547)
static int round_up_connection_bw_to_send_multiple(int in_bw)
{
    return (Game_bw_test_size - 1 + in_bw) / Game_bw_test_size * Game_bw_test_size;
}

// 0x00972380 (:554): untested (or older than 30 minutes): test. A failed test under 5 minutes old: not again. Otherwise
// test when the upstream is below what flooding these peers needs and not yet the test maximum.
static bool should_test_connection(const multi_player_uid& uid, test_target_info& target_info, int send_connection_count)
{
    const int slot = tracked_bandwidth_info_get_slot_index(uid);
    if (slot != -1)
    {
        const upstream_bandwidth_info& info = Tracked_upstream_bandwidth_info_list[slot];
        const int upstream = info.upstream_bw;
        const int limit = info.upstream_bw_test_limit;
        const unsigned int age = (unsigned int)info.update_time.time_since();
        if (age <= 1800000) // Magic number: 30 minutes
        {
            if (upstream < limit && age - 1 <= 299998) // Magic number: 5 minutes
            {
                target_info.upstream_bw = upstream;
                target_info.bw_fail_by_limit = true;
                return false;
            }
            const bool test = Game_bw_test_size * send_connection_count > upstream && upstream != Max_game_test_bw_bps;
            target_info.upstream_bw = upstream;
            target_info.bw_untested = false;
            target_info.bw_fail_by_limit = upstream < limit;
            return test;
        }
    }
    target_info.upstream_bw = 0;
    target_info.bw_fail_by_limit = false;
    target_info.bw_untested = true;
    return true;
}

// 0x00972420 (:590): among the peers to test, an untested one at random with a chance of half the untested share,
// else the tested one with the highest upstream
static multi_connection* choose_bw_test_target(const multi_session* sp, int connection_send_count)
{
    multi_connection* me = sp->get_me_connection();
    test_target_info targets[16];
    test_target_info* untested[16];
    test_target_info* tested[16];
    int num_targets = 0;
    unsigned int num_untested = 0;
    int num_tested = 0;
    test_target_info* target = targets;
    multi_connection* cp = sp->get_connection_list();
    while (cp != nullptr)
    {
        if (cp != me && (cp->connection_status == CS_CONNECTED || cp->connection_status == CS_CONNECTED_GAMEPLAY) && cp->cannot_serve == 0 &&
            cp->game_data != 0xFFFFFFFFu)
        {
            const multi_game_data* data = &Mp_game_data->data[cp->game_data & 0xFFFF][cp->game_data >> 16];
            if (data != nullptr)
            {
                target->cp = cp;
                if (should_test_connection(data->player_uid, *target, connection_send_count))
                {
                    num_targets++;
                    target->cp = cp;
                    if (target->bw_untested)
                    {
                        untested[num_untested++] = target;
                    }
                    else
                    {
                        tested[num_tested++] = target;
                    }
                    target++;
                }
            }
        }
        cp = cp->next;
        if (cp == sp->get_connection_list())
        {
            break;
        }
    }
    if (num_targets == 0)
    {
        return nullptr;
    }
    if (num_tested != 0)
    {
        const float roll = fl_rand();
        const float untested_chance = (float)((double)(int)num_untested * 0.5 / (double)num_targets);
        if (untested_chance <= roll)
        {
            multi_connection* best = nullptr;
            int best_upstream = -1;
            for (int i = 0; i < num_tested; i++)
            {
                if (!tested[i]->bw_untested && tested[i]->upstream_bw > best_upstream)
                {
                    best = tested[i]->cp;
                    best_upstream = tested[i]->upstream_bw;
                }
            }
            return best;
        }
    }
    return untested[vrand() % num_untested]->cp;
}

// 0x009D8860 (:670): the host's test schedule: with too few peers it waits; it tests itself first (when due), then a
// chosen peer, which has 2 s to start sending before the host stops it and picks another
static void bandwidth_process_management(const multi_session* sp)
{
    if (!sp->am_i_host())
    {
        return;
    }
    const int send_count = get_bw_test_connection_send_count(sp);
    if (send_count + 1 < BW_min_players_to_test)
    {
        if (Tm_state != TM_WAIT_MIN_PLAYER)
        {
            send_bandwidth_test_stop();
        }
        Tm_state = TM_WAIT_MIN_PLAYER;
        return;
    }
    if (Game_bandwidth_state == TEST_BW_DONE && Tm_state == TM_TEST_SELF)
    {
        Tm_state = TM_SELECT_TARGET;
    }
    else if (Tm_state > TM_TEST_TARGET)
    {
        return;
    }
    switch (Tm_state)
    {
    case TM_NONE:
    {
        test_target_info info;
        if (should_test_connection(My_player_uid, info, send_count))
        {
            Game_bandwidth_state = TEST_BW_START;
            Tm_state = TM_TEST_SELF;
            return;
        }
        Tm_state = TM_SELECT_TARGET;
        return;
    }
    case TM_WAIT_MIN_PLAYER:
        Tm_state = TM_NONE;
        return;
    case TM_TEST_SELF:
        return;
    case TM_SELECT_TARGET:
    {
        multi_connection* target = choose_bw_test_target(sp, send_count);
        if (target == nullptr)
        {
            Tm_state = TM_NONE;
            return;
        }
        unsigned char packet[1200];
        packet[0] = PT_BW_TEST_INFO;
        unsigned short size = Multi_packets[PT_BW_TEST_INFO].group_id != 0 ? 5 : 3;
        packet[size] = 1;
        size++;
        const unsigned short payload = (unsigned short)(size - 3);
        packet[1] = (unsigned char)(payload >> 8);
        packet[2] = (unsigned char)payload;
        multi_io_send_reliable(target, packet, size, nullptr, nullptr, nullptr);
        Tm_state = TM_TEST_TARGET;
        Game_last_test_packet_rcvd_time.set(0);
        return;
    }
    case TM_TEST_TARGET:
        if (Game_last_test_packet_rcvd_time.time_since() > 2000)
        {
            send_bandwidth_test_stop();
            Tm_state = TM_SELECT_TARGET;
        }
        return;
    }
}

// 0x009475C0 (:759)
static void reset_test()
{
    Bandwidth_test_result = TEST_RESULT_UNKNOWN;
    Game_bandwidth_test_duration.set(0);
    Game_bandwidth_state = TEST_BW_TRANSMIT_DATA;
    Game_bandwidth_send_stamp.invalidate();
}

// 0x009725A0 (:772): the next multiple to try; false when the search is over
static bool adjust_binary_search_params(bool test_successful)
{
    binary_search_info& search = Game_bandwidth_binary_search_info;
    if (search.start_index == search.end_index)
    {
        return false;
    }
    if (test_successful)
    {
        if (search.pass_index <= search.cur_index)
        {
            search.pass_index = search.cur_index;
        }
        search.start_index = search.cur_index + 1;
    }
    else
    {
        if (search.cur_index <= search.fail_index)
        {
            search.fail_index = search.cur_index;
        }
        search.end_index = search.cur_index - 1;
    }
    if (search.start_index <= search.end_index)
    {
        search.cur_index = (search.end_index + search.start_index) / 2;
        return true;
    }
    return false;
}

// 0x009475F0 (:797)
static void decrease_binary_search_params_ceiling()
{
    binary_search_info& search = Game_bandwidth_binary_search_info;
    search.end_index--;
    if (search.start_index <= search.end_index)
    {
        search.cur_index = (search.end_index + search.start_index) / 2;
        reset_test();
        return;
    }
    Game_bandwidth_state = TEST_BW_FINISHED_DATA;
    Bandwidth_test_result = TEST_RESULT_FORCE_STOP;
}

// 0x00947660 (:813): magic numbers: the search starts at 2 multiples; 1 passes and 1000 fails until tested
static void init_binary_search_params(int max_bw_multiple)
{
    binary_search_info& search = Game_bandwidth_binary_search_info;
    search.end_index = max_bw_multiple;
    search.start_index = 2;
    search.cur_index = (max_bw_multiple + 2) / 2;
    search.pass_index = 1;
    search.fail_index = 1000;
}

// 0x009476A0 (:824): magic number: 0x32000 bits per second per target
static int compute_max_send_bandwidth_bps(int num_send_targets)
{
    return num_send_targets * 0x32000;
}

// 0x009476B0 (:832)
static void update_platform_computed_bandwidth(multi_session*)
{
}

// 0x009D8A30 (:866): our tracked upstream already covers the maximum (tested within 30 minutes), or a failed test is
// under 5 minutes old: no test; a client reports the tracked values to its host. Without a slot the three values are
// the multiple itself (original behaviour).
static bool send_maxed_out(int send_max_multiple, multi_session* sp)
{
    int upstream = send_max_multiple;
    int limit = send_max_multiple;
    int age = send_max_multiple;
    const int slot = tracked_bandwidth_info_get_slot_index(My_player_uid);
    if (slot != -1)
    {
        upstream = Tracked_upstream_bandwidth_info_list[slot].upstream_bw;
        limit = Tracked_upstream_bandwidth_info_list[slot].upstream_bw_test_limit;
        age = Tracked_upstream_bandwidth_info_list[slot].update_time.time_since();
    }
    const bool maxed = upstream >= Game_bw_test_size * send_max_multiple && (unsigned int)(age - 1) <= 0x1B773E;
    const bool failing = upstream < limit && (unsigned int)(age - 1) <= 299998;
    if (!maxed && !failing)
    {
        return false;
    }
    if (!sp->am_i_host())
    {
        send_flood_bw_result(sp, upstream, limit, age);
    }
    return true;
}

// 0x009D8AE0 (:892): our own test while the host has us testing ourselves. With the game in the background or too
// few peers it ends with what is known. Otherwise: START sets up the binary search; TRANSMIT_DATA floods every peer
// every 20 ms with PT_BW_TESTER packets sized for the current multiple, failing on over 5.1% mean loss and passing
// after 4 s; FINISHED_DATA stores and reports the result and moves the search on.
static void test_bandwidth_process(multi_session* sp)
{
    if (sp == nullptr || !Game_bandwidth_test_start_time.valid() || !Game_bandwidth_test_start_time.elapsed())
    {
        return;
    }
    bandwidth_process_management(sp);
    if (Tm_state == TM_WAIT_MIN_PLAYER || Tm_state != TM_TEST_SELF)
    {
        return;
    }
    const int send_count = get_bw_test_connection_send_count(sp);
    int max_bw = compute_max_send_bandwidth_bps(send_count);
    if (max_bw >= Game_bw_test_size * send_count)
    {
        max_bw = Game_bw_test_size * send_count;
    }
    int send_max_multiple = max_bw / Game_bw_test_size;
    if (send_max_multiple >= Max_game_test_bw_bps / Game_bw_test_size)
    {
        send_max_multiple = Max_game_test_bw_bps / Game_bw_test_size;
    }
    if (!rfg_is_gameframework_fully_in_foreground() || send_count + 1 < BW_min_players_to_test)
    {
        if (Game_bandwidth_state == TEST_BW_DONE)
        {
            return;
        }
        if (send_maxed_out(send_max_multiple, sp))
        {
            Game_bandwidth_state = TEST_BW_DONE;
            return;
        }
        const int passed = Game_bandwidth_binary_search_info.pass_index * Game_bw_test_size;
        send_flood_bw_result(sp, passed, passed, 0);
        Game_last_test_packet_rcvd_time.set(0);
        Game_bandwidth_state = TEST_BW_DONE;
        return;
    }
    switch (Game_bandwidth_state)
    {
    case TEST_BW_START:
        reset_test();
        if (send_maxed_out(send_max_multiple, sp))
        {
            Game_bandwidth_state = TEST_BW_DONE;
            return;
        }
        if (send_max_multiple <= 1)
        {
            Bandwidth_test_result = TEST_RESULT_COULD_NOT_TEST;
            Game_bandwidth_state = TEST_BW_DONE;
            return;
        }
        init_binary_search_params(send_max_multiple);
        return;
    case TEST_BW_TRANSMIT_DATA:
    {
        multi_connection* first = sp->get_connection_list();
        multi_connection* me = sp->get_me_connection();
        float loss = 0.0f;
        int count = 0;
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (multi_connection_is_connected(cp) && cp != me)
            {
                loss = (float)((double)get_packet_loss_pct(cp) + (double)loss);
                count++;
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
        if (count != 0)
        {
            loss = (float)((double)loss / (double)(unsigned int)count); // x87, rounded once
        }
        if ((double)loss > (double)0.051f) // Magic number: 5.1% mean loss fails the rate
        {
            Bandwidth_test_result = TEST_RESULT_FAIL;
            for (multi_connection* cp = first; cp != nullptr;)
            {
                if (multi_connection_is_connected(cp) && cp != me)
                {
                    multi_core_connection_reset_packet_loss(cp);
                }
                cp = cp->next;
                if (cp == sp->get_connection_list())
                {
                    break;
                }
            }
            Game_bandwidth_state = TEST_BW_FINISHED_DATA;
            return;
        }
        if (!Game_bandwidth_send_stamp.valid() || Game_bandwidth_send_stamp.elapsed())
        {
            // The bytes per peer per 20 ms send for the current multiple (50 sends per second).
            const unsigned int bytes = (unsigned int)(Game_bandwidth_binary_search_info.cur_index * Game_bw_test_size) / 50 / (unsigned int)send_count >> 3;
            if (bytes * 400 > (unsigned int)Game_bw_test_size)
            {
                decrease_binary_search_params_ceiling();
                return;
            }
            unsigned int payload = bytes - 7;
            if (payload != 0 && payload >= 0x1F9)
            {
                payload = 0x1F9;
            }
            // The tester packet's payload is uninitialized stack in rfg.exe (only its size matters); zeros here.
            unsigned char packet[1200];
            memset(packet, 0, sizeof(packet));
            packet[0] = PT_BW_TESTER;
            const unsigned int size = (Multi_packets[PT_BW_TESTER].group_id != 0 ? 5u : 3u) + payload;
            const unsigned short packet_payload = (unsigned short)(size - 3);
            packet[1] = (unsigned char)(packet_payload >> 8);
            packet[2] = (unsigned char)packet_payload;
            for (multi_connection* cp = sp->get_connection_list(); cp != nullptr;)
            {
                if (cp != me && is_valid_bwtest_connection(cp))
                {
                    multi_core_connection_enable_packet_loss_detection(cp);
                    cp->last_flood_test_time.set(0);
                    multi_io_send(cp, packet, size & 0xFFFF);
                    multi_connection_flush_current_packet(cp);
                }
                cp = cp->next;
                if (cp == sp->get_connection_list())
                {
                    break;
                }
            }
            Game_bandwidth_send_stamp.set(20);
        }
        if (Game_bandwidth_test_duration.time_since() > 4000)
        {
            Bandwidth_test_result = TEST_RESULT_SUCCESS;
            Game_bandwidth_state = TEST_BW_FINISHED_DATA;
        }
        return;
    }
    case TEST_BW_FINISHED_DATA:
    {
        const int tested = Game_bandwidth_binary_search_info.cur_index * Game_bw_test_size;
        if (Bandwidth_test_result == TEST_RESULT_FORCE_STOP)
        {
            const int passed = Game_bandwidth_binary_search_info.pass_index * Game_bw_test_size;
            send_flood_bw_result(sp, passed, passed, 0);
            tracked_bandwidth_info_set_info(My_player_uid, passed, passed, 0);
            Game_bandwidth_state = TEST_BW_DONE;
            return;
        }
        if (Bandwidth_test_result == TEST_RESULT_SUCCESS)
        {
            multi_core_qos_set_stored_upstream_bandwidth(tested);
            send_flood_bw_result(sp, tested, tested, 0);
            tracked_bandwidth_info_set_info(My_player_uid, tested, tested, 0);
        }
        else if (Bandwidth_test_result == TEST_RESULT_FAIL)
        {
            const int passed = Game_bandwidth_binary_search_info.pass_index * Game_bw_test_size;
            multi_core_qos_set_stored_upstream_bandwidth(passed);
            send_flood_bw_result(sp, passed, tested, 0);
            tracked_bandwidth_info_set_info(My_player_uid, passed, tested, 0);
        }
        if (Game_bandwidth_binary_search_info.cur_index == Game_bandwidth_binary_search_info.end_index)
        {
            Game_bandwidth_state = TEST_BW_DONE;
            return;
        }
        const bool success = Bandwidth_test_result == TEST_RESULT_SUCCESS;
        reset_test();
        if (!adjust_binary_search_params(success))
        {
            Game_bandwidth_state = TEST_BW_DONE;
        }
        return;
    }
    default:
        return;
    }
}

// 0x00972610 (:1131): the tests start 3 s after init; the test maximum is the largest player bandwidth rounded up to
// a multiple of the test size
void multi_game_bw_system_init()
{
    Min_game_bw_bps = (int)Player_bandwidth[0].bandwidth;
    Max_game_bw_bps = (int)Player_bandwidth[Num_player_bandwidths - 1].bandwidth;
    Tm_state = TM_NONE;
    Game_bandwidth_state = TEST_BW_DONE;
    Max_game_test_bw_bps = round_up_connection_bw_to_send_multiple(Max_game_bw_bps);
    Game_last_test_packet_rcvd_time.set(0);
    Game_bandwidth_test_session_type = ST_INVALID;
    Game_bandwidth_test_start_time.set(3000);
}

// 0x009D8F80 (:1146)
void multi_game_bw_stop()
{
    if (Game_bandwidth_test_session_type != ST_INVALID)
    {
        send_bandwidth_test_stop();
        multi_game_bw_system_init();
    }
}

// 0x009D8FA0 (:1156): tests start over on this session type (party, transition or game) in 3 s
void multi_game_bw_reset(int int_session_type)
{
    if ((unsigned int)int_session_type >= 3)
    {
        return;
    }
    if (Game_bandwidth_test_session_type != ST_INVALID)
    {
        send_bandwidth_test_stop();
        multi_game_bw_system_init();
    }
    Tm_state = TM_NONE;
    Game_bandwidth_state = TEST_BW_DONE;
    Game_last_test_packet_rcvd_time.set(0);
    Game_bandwidth_test_session_type = (session_types)int_session_type;
    Game_bandwidth_test_start_time.set(3000);
}

// 0x009D9000 (:1173): the tests run on the probed session while it is READY and no game session is active
void multi_game_bw_process()
{
    if (!Bandwidth_probing_enabled)
    {
        return;
    }
    session_types type;
    switch (Game_bandwidth_test_session_type)
    {
    case ST_PARTY:
        type = ST_PARTY;
        break;
    case ST_GAME:
        type = ST_GAME;
        break;
    default:
        return;
    }
    multi_session* sp = multi_session_get_by_type(type);
    if (sp == nullptr || sp->get_state() != MSS_READY)
    {
        return;
    }
    multi_session* game_session = multi_session_get_by_type(ST_GAME);
    if (game_session == nullptr || !multi_session_is_active_game_session(game_session))
    {
        test_bandwidth_process(sp);
    }
}

// 0x009476C0 (:1198): PT_BW_TESTER: a peer is flooding us
void multi_game_process_bw_tester(unsigned char*, const sender& from)
{
    Game_last_test_packet_rcvd_time.set(0);
    if (from.connection != nullptr)
    {
        from.connection->last_flood_test_time.set(0);
    }
}

// 0x009476F0 (:1214): PT_BW_TEST_INFO from the host: 0 stops our test, 1 starts it
void multi_game_process_bw_test_info(unsigned char* packet_data, const sender&)
{
    if (packet_data[0] == 0)
    {
        Tm_state = TM_NONE;
        Game_bandwidth_state = TEST_BW_START;
    }
    else if (packet_data[0] == 1)
    {
        Tm_state = TM_TEST_SELF;
        Game_bandwidth_state = TEST_BW_START;
    }
}

// 0x009A91E0 (:1233): PT_FLOOD_UPDATE: a peer's own result (our own uid is ignored)
void multi_game_process_flood_update(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr)
    {
        return;
    }
    int offset = 0;
    multi_player_uid uid;
    uid.read_from_packet(packet_data, offset);
    unsigned int values[3];
    for (int v = 0; v < 3; v++)
    {
        const unsigned char* p = packet_data + offset + v * 4;
        values[v] = (unsigned int)p[0] << 24 | (unsigned int)p[1] << 16 | (unsigned int)p[2] << 8 | p[3];
    }
    if (!uid.get_internal_id().is_equal(My_player_uid.get_internal_id()))
    {
        tracked_bandwidth_info_set_info(uid, (int)values[0], (int)values[1], (int)values[2]);
    }
}

// 0x009A9340 (:1259): PT_FLOOD_FULL_RESULTS from the host: every player's values by session index
void multi_game_process_full_flood_results(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr || from.connection->owner == nullptr)
    {
        return;
    }
    multi_session* sp = from.connection->owner;
    unsigned int count = packet_data[0];
    unsigned char bits[64];
    if (count != 0)
    {
        memcpy(bits, packet_data + 1, count * 4);
    }
    bit_packer packer(bits, 64);
    for (; count != 0; count--)
    {
        const unsigned int index = packer.get_next_bits(5);
        const unsigned int upstream = packer.get_next_bits(9);
        const unsigned int limit = packer.get_next_bits(9);
        const unsigned int age = packer.get_next_bits(9);
        multi_connection* first = sp->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (cp->index_in_session == (unsigned char)index)
            {
                tracked_bandwidth_info_set_info(cp->player_uid, (int)decompress_bandwidth(upstream), (int)decompress_bandwidth(limit),
                    (int)decompress_time(age));
                break;
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
    }
}
