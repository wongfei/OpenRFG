// Original: volition/rfg/code/multi/multi_core/global/multi_core_clock.cpp
//
// The network clock: Clock_current_time advances with the real-time timer; the game server sends it (PT_CLOCK, once a
// second per client, in the connection's spare send budget) and a client takes it plus its mean ping to the host.
// multi_timestamp is a timestamp on this clock, wrapping at 1,800,000,000 ms (a value below 0 is invalid).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/vlib/code/os/timer.h"

#include <string.h>

// ---- file-static globals
// unsigned int Prev_frametime (0x02FE6D00) is used here too; placed in multi.cpp.
// unsigned int Clock_update_time (0x02FEB340) is used here too; placed in multi.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.

// Magic numbers: the clock wraps at 1,800,000,000 ms; differences beyond half of that count as the other direction.
static const int Clock_wrap_ms = 1800000000;
static const int Clock_half_wrap_ms = 900000000;

// 0x00943F30 (:88)
void multi_core_clock_system_init()
{
    Clock_current_time = 0;
    Clock_update_time = 0;
    Prev_frametime = 0;
}

// 0x00943F50 (:96)
void multi_core_clock_process()
{
    const unsigned int now = (unsigned int)timer_get(TIMER_MILLISECONDS);
    Clock_current_time += now - Prev_frametime;
    Prev_frametime = now;
}

// 0x00943F80 (:111)
unsigned int multi_core_clock_get_time()
{
    return Clock_current_time;
}

// 0x00943F90 (:117)
void multi_core_clock_set_time(unsigned int current_time)
{
    Clock_current_time = current_time;
}

// 0x00971E10 (:127): PT_CLOCK from the host: the host's time (big endian) plus our mean ping to it. The owner is not
// checked for null (as in rfg.exe).
void multi_core_process_clock_packet(unsigned char* packet_data, const sender& from)
{
    if (from.connection == nullptr)
    {
        return;
    }
    multi_connection* host = from.connection->owner->get_host_connection();
    if (host == nullptr)
    {
        return;
    }
    Clock_update_time = (unsigned int)packet_data[0] << 24 | (unsigned int)packet_data[1] << 16 | (unsigned int)packet_data[2] << 8 | packet_data[3];
    Clock_current_time = host->mean_ping + Clock_update_time;
}

// 0x00971EA0 (:168): the game server puts a PT_CLOCK straight into the client's unreliable buffer, once a second, when
// it fits the spare budget
void multi_core_clock_poll_for_packet(multi_connection* cp, unsigned int max_size)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    multi_session* sp = cp->owner;
    if (!sp->am_i_host() || sp->get_info()->type != ST_GAME || sp->get_state() == MSS_MIGRATING)
    {
        return;
    }
    if (cp->clock_update_timestamp.valid() && !cp->clock_update_timestamp.elapsed())
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_CLOCK;
    const unsigned int header = Multi_packets[PT_CLOCK].group_id != 0 ? 5 : 3;
    const unsigned short payload = (unsigned short)(header + 1);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    packet[header] = (unsigned char)(Clock_current_time >> 24);
    packet[header + 1] = (unsigned char)(Clock_current_time >> 16);
    packet[header + 2] = (unsigned char)(Clock_current_time >> 8);
    packet[header + 3] = (unsigned char)Clock_current_time;
    const unsigned int size = header + 4;
    if (size <= max_size)
    {
        memcpy(cp->unreliable_buffer + cp->unreliable_buffer_size, packet, size);
        cp->unreliable_buffer_size = (unsigned short)(cp->unreliable_buffer_size + size);
        cp->clock_update_timestamp.set(1000);
    }
}

// 0x00943FA0 (:199)
void multi_timestamp::set(int delta_milliseconds)
{
    value = delta_milliseconds + (int)Clock_current_time;
    if (delta_milliseconds < 0)
    {
        if (value < 0)
        {
            value += Clock_wrap_ms;
        }
    }
    else if (value > Clock_wrap_ms)
    {
        value -= Clock_wrap_ms;
    }
}

// 0x00943FE0 (:217)
void multi_timestamp::add_time(int milliseconds)
{
    value += milliseconds;
}

// 0x00943FF0 (:222)
void multi_timestamp::deduct_time(int milliseconds)
{
    value -= milliseconds;
}

// 0x00944000 (:229)
bool multi_timestamp::elapsed() const
{
    if (value < 0)
    {
        return false;
    }
    if ((int)Clock_current_time < value)
    {
        return value - (int)Clock_current_time >= Clock_half_wrap_ms;
    }
    return (int)(Clock_current_time - value) < Clock_half_wrap_ms;
}

// 0x00944030 (:261)
int multi_timestamp::time_until() const
{
    if (value < 0)
    {
        return Clock_wrap_ms;
    }
    if ((int)Clock_current_time < value)
    {
        const int until = value - (int)Clock_current_time;
        if (until > Clock_half_wrap_ms)
        {
            return until - Clock_wrap_ms;
        }
        return until;
    }
    if ((int)(Clock_current_time - value) > Clock_half_wrap_ms)
    {
        return (value - (int)Clock_current_time) + Clock_wrap_ms;
    }
    return -(int)(Clock_current_time - value);
}

// 0x00944070 (:289)
int multi_timestamp::time_since() const
{
    if (value < 0)
    {
        return -Clock_wrap_ms;
    }
    if ((int)Clock_current_time < value)
    {
        const int until = value - (int)Clock_current_time;
        if (until > Clock_half_wrap_ms)
        {
            return -(until - Clock_wrap_ms);
        }
        return -until;
    }
    if ((int)(Clock_current_time - value) > Clock_half_wrap_ms)
    {
        return -((value - (int)Clock_current_time) + Clock_wrap_ms);
    }
    return (int)(Clock_current_time - value);
}

// 0x009440C0 (:297)
bool multi_timestamp::valid() const
{
    return value > -1;
}

// 0x00941CF0 (.h:49)
void multi_timestamp::invalidate()
{
    value = -1;
}

// 0x009440D0 (:306): rfg.exe inlines multi_timestamp::set
void multi_timestamp_percent::set(int milliseconds)
{
    set_milliseconds = milliseconds;
    multi_timestamp::set(milliseconds);
}

// 0x00944110 (:315): the share of the time still to run (1 when invalid, 0 when elapsed)
float multi_timestamp_percent::percent() const
{
    if (!valid())
    {
        return 1.0f;
    }
    if (elapsed())
    {
        return 0.0f;
    }
    return (float)time_until() / (float)set_milliseconds;
}

// 0x00944160 (:328)
int multi_timestamp_percent::time_since_set() const
{
    if (!valid())
    {
        return Clock_wrap_ms;
    }
    if (elapsed())
    {
        return time_since() + set_milliseconds;
    }
    return set_milliseconds - time_until();
}
