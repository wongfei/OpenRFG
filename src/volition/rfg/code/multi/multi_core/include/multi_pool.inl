// Original: volition/rfg/code/multi/multi_core/include/multi_pool.inl
//
// The multiplayer object pools: a dynamic_object_pool of N objects (one category) whose storage is allocated from a
// mempool when the multiplayer memory is set up (multi_core_memory_alloc, multi_game_memory_alloc), not inline like
// object_pool<T, N, K>. The instances are file statics of the multi unity TU (Snapshot_packet_headers, Server_pool,
// the delta compression and object state pools...). Written from multi_pool<snapshot_packet_header,500>; the other 24
// instances in rfg.exe have the same code.
#pragma once

#include "volition/vlib/code/memory/object_pool.h"

class mempool_base;

// size 0x40 (<snapshot_packet_header,500>)
template<typename T, int N>
class multi_pool : public dynamic_object_pool<T, 1>
{
public:
    void init(mempool_base& pool); // <snapshot_packet_header,500> 0x009B3740 (:32)
    multi_pool(); // <snapshot_packet_header,500> 0x009A94F0
    ~multi_pool(); // <snapshot_packet_header,500> 0x0098F8D0
};

// <snapshot_packet_header,500> 0x009B3740 (:32): the server init with N objects, then the in-use list cleared
template<typename T, int N>
void multi_pool<T, N>::init(mempool_base& pool)
{
    dynamic_object_pool<T, 1>::init(pool, N);
}

// <snapshot_packet_header,500> 0x009A94F0: no callbacks, the client bound to this server (dynamic_object_pool's)
template<typename T, int N>
multi_pool<T, N>::multi_pool()
{
}

// <snapshot_packet_header,500> 0x0098F8D0: empty; the storage belongs to the mempool
template<typename T, int N>
multi_pool<T, N>::~multi_pool()
{
}
