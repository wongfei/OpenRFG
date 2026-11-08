// Original: volition/rfg/code/objects/humans/npc_sound_info.inl
//
// npc_sound_info::queue<T> (decompiled from the npc_sound_info_building_damage instance at 0x0072B010): copies the info
// into the async queue mempool and pushes it on the queued stack (slist_lock_free_stack::push inlined); send_queued
// propagates the queued infos later. Nothing is queued when the mempool is full.

#include "volition/vlib/code/memory/async_mempool.h"

#include <new>

// npc_sound_info.inl:5
template<typename T>
void npc_sound_info::queue(const T& sound_info)
{
    void* mem = queue_mempool.alloc(sizeof(T), 0);
    if (mem == nullptr)
    {
        return;
    }
    T* info = new (mem) T(sound_info);
    info->lfn.value = info;
    info->lfn.base_node.next = nullptr;
    queued_infos.push(&info->lfn.base_node); // inlined
}
