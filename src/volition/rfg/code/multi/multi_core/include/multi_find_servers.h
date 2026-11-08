// Original: volition/rfg/code/multi/multi_core/include/multi_find_servers.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_find_servers.cpp
//
// Finding servers (the LAN and internet server lists). Placement of multi_server_info inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/vlib/code/time/timestamp.h"

class mempool_base;

// size 0x238
struct multi_server_info
{
    multi_server_info(); // 0x0088A0A0
    ~multi_server_info(); // 0x0088A0C0

    wchar_t name[256]; // 0x00
    multi_session_info session_info; // 0x200
    timestamp stamp; // 0x230
};

multi_server_info* multi_fs_server_info(unsigned int index); // 0x0094B6D0 (:173)
void multi_fs_init(mempool_base& pool); // 0x009FC3C0 (:194)
void multi_fs_reset_servers(); // 0x009AA870 (:200)
