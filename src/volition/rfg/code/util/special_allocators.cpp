// Original: volition/rfg/code/util/special_allocators.cpp
//
// Partial: the pregame split lock (stub_impl.md S3) and the gameplay GPU lock (the effects pool, used by full screen
// movies and to verify a save). The gameplay CPU locks (the animation pool cached to anim_temp.dat) are not ported yet.

#include "volition/rfg/code/util/special_allocators.h"

#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/os/thread.h"

// Four pools, so locks nest (the effects preload packfile inside the effects table's scratch, the table precache
// inside the boot).
static object_pool<rfg_split_mempool, 4, 1> Pregame_split_pools(nullptr, nullptr, 0); // 0x0375A900

// 0x00B61E00 (:45)
void salloc_get_pregame_split_lock_max_sizes(int& cpu_size, int& gpu_size)
{
    cpu_size = 0x2000000; // magic number: 32 MB
    gpu_size = 0;
}

// 0x00B62600 (:62)
// A pool over cpu_size bytes of Level_memmgr; null when all four pools are locked. The GPU half gets no memory
// (gpu_size is only recorded).
rfg_split_mempool* salloc_pregame_split_lock(int cpu_size, int gpu_size)
{
    rfg_split_mempool* pool = Pregame_split_pools.alloc(0);
    if (pool == nullptr)
    {
        return nullptr;
    }
    void* mem = Level_memmgr.alloc((unsigned int)cpu_size, "salloc_pregame_split_lock", 4);
    // 16-byte CPU alignment, GPU alignment 0x80
    pool->init_rfg_split_mempool("Pregame_split_pool", mem, (unsigned int)cpu_size, 0x10, nullptr, (unsigned int)gpu_size, 0x80, 0);
    return pool;
}

// 0x00B62900 (:87)
void salloc_pregame_split_unlock(rfg_split_mempool* pool)
{
    if (pool == nullptr)
    {
        return;
    }
    Level_memmgr.free(pool->get_base());
    pool->clear_cpu();
    Pregame_split_pools.free(pool);
}

// 0x00B61E30 (:108)
// The effects preload pool, waiting here until streaming has released its peg container. Null when already locked.
mempool_base* salloc_gameplay_gpu_lock()
{
    if (!__effects_pool_lock_async())
    {
        return nullptr;
    }
    rfg_split_mempool* pool;
    do
    {
        stream2_do_frame(true);
        thread_yield();
        pool = __effects_pool_lock_async_poll();
    } while (pool == nullptr);
    return pool;
}

// 0x00B61E60 (:134)
bool salloc_gameplay_gpu_lock_async()
{
    return __effects_pool_lock_async();
}

// 0x00B61E70 (:139): the pool once streaming has released its peg container, null until then
mempool_base* salloc_gameplay_gpu_lock_async_poll()
{
    return __effects_pool_lock_async_poll();
}

// 0x00B61E80 (:154)
void salloc_gameplay_gpu_unlock_async()
{
    __effects_pool_unlock_async();
}

// 0x00B61E90 (:159)
bool salloc_gameplay_gpu_unlock_async_poll()
{
    return __effects_pool_unlock_async_poll();
}

// 0x00B61EC0 (:174)
void salloc_gameplay_cpu_unlock_async()
{
    __anim_pool_unlock_async();
}
