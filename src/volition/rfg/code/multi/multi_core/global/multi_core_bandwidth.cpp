// Original: volition/rfg/code/multi/multi_core/global/multi_core_bandwidth.cpp
//
// Bandwidth statistics (empty in the retail build: the accounting, rendering and log are compiled out) and the
// enable switch of bandwidth filling, which starts 10 s after it is enabled.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_core_bandwidth.h"
#include "volition/vlib/code/time/timestamp.h"

// ---- file-static globals
static int Bandwidth_filling_time_ms = 30000; // 0x0167DE28
// timestamp Bandwidth_ready (0x016916C4) is used here too; placed in multi.cpp.
// timestamp Bandwidth_filling_timestamp (0x01691CC4) is used here too; placed in multi.cpp.
// bool Bandwidth_enabled (0x02FEB5D1) is used here too; placed in multi.cpp.

// 0x00943E80 (:816)
void multi_core_bandwidth_do_frame()
{
}

// 0x00943E90 (:844)
void multi_core_bandwidth_increase_used(multi_connection*, unsigned int, unsigned int, bool, int)
{
}

// 0x00943EA0 (:868)
void multi_core_bandwidth_add_packet(multi_connection*, unsigned int, bool, unsigned int, unsigned int)
{
}

// 0x00943EB0 (:885)
void multi_core_bandwidth_render()
{
}

// 0x00943EC0 (:1025)
unsigned int multi_core_bandwidth_get_packet_bandwidth_not_sent()
{
    return 0;
}

// 0x00943ED0 (:1039)
void multi_core_bandwidth_add_packet_bandwidth_not_sent(multi_connection*, unsigned int)
{
}

// 0x00943EE0 (:1081)
void multi_core_bandwidth_init()
{
    Bandwidth_enabled = false;
    Bandwidth_ready.invalidate();
}

// 0x00943F00 (:1110)
bool multi_core_bandwidth_is_enabled()
{
    return Bandwidth_enabled && Bandwidth_ready.elapsed();
}

// 0x00971DE0 (:1120): magic number: ready 10 s after enabling
void multi_core_bandwidth_enable(bool enable)
{
    Bandwidth_enabled = enable;
    if (enable)
    {
        Bandwidth_ready.set(10000);
        Bandwidth_filling_timestamp.set(Bandwidth_filling_time_ms);
    }
}

// 0x00943F20 (:1136)
bool multi_core_bandwidth_write_log()
{
    return false;
}
