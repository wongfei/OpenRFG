// Original: volition/render_lib/code/rl_mesh_recompile.cpp
//
// Mesh recompilation: destroyable model instances queue rl_mesh_recompile_parameters on recompile_queue; the mesh
// recompile thread compiles them (rl_destroyable_model_instance::compile_cache_data) and passes them on finalize_queue
// to the render thread, which installs the new data in rl_mesh_recompile_finalize. Replaced data is freed two
// finalizes later (s_completionEntries), when the GPU no longer uses it.
// Pausing (rl_mesh_recompile_pause/unpause, nested by Rl_recompile_mesh_pause_count) waits for the thread to be idle
// (s_handshake_start) and keeps it waiting (s_handshake_end) while instances change their sub instances.

#include "volition/render_lib/code/rl_mesh_recompile.h"

#include "volition/render_lib/code/rl_batch_defines.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/pool_list.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/sync_event.h"
#include "volition/vlib/code/os/thread.h"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/mutex.hpp"
#include "keen/graphics/rendergeometry.hpp"

#include <intrin.h>
#include <string.h>
#include <windows.h>

// size 0x8
struct CompletionEntry
{
    rl_mesh_recompile_data* pData; // 0x00
    int counter; // 0x04
};

static spsc_queue<rl_mesh_recompile_parameters*> recompile_queue; // 0x01CC5C60
static spsc_queue<rl_mesh_recompile_parameters*> finalize_queue; // 0x01C20A48
static keen::Mutex s_completionEntriesMutex; // 0x01C37190
static unsigned char* s_params_mem_p = nullptr; // 0x01B7BCD8
static pool_list<rl_mesh_recompile_parameters*> s_params_pool; // 0x01514E44
static unsigned int mesh_recompile_thread_handle; // 0x01B7BCD4
// Magic number: 2048 pending frees.
static CompletionEntry s_completionEntries[2048]; // 0x01B72BF8
static volatile bool keep_running = true; // 0x014FA2DF
static sync_event s_handshake_start(false, false); // 0x01C226CC
static sync_event s_handshake_end(false, false); // 0x01C226C8
bool Rl_recompile_mesh_enable = true; // 0x014FA134
volatile unsigned int Rl_recompile_mesh_pause_count = 0; // 0x01B7BCDC
static cs_wrapper start_recompile_cs(0); // 0x01C22670
static int numRecompileMeshDatas; // 0x01B7BCE0

// <rl_mesh_recompile_parameters*> 0x004D7BF0 (.h:49)
template<typename T>
spsc_queue<T>::node::node(const T& datum)
    : val(datum)
    , next_p(nullptr)
{
}

// <rl_mesh_recompile_parameters*> 0x004D5D30 (.h:62)
template<typename T>
spsc_queue<T>::spsc_queue()
{
}

// <rl_mesh_recompile_parameters*> 0x00504600 (.h:68)
// A dummy node: the queue is empty while divider_p == last_p.
template<typename T>
void spsc_queue<T>::create()
{
    first_p = new node(T());
    divider_p = first_p;
    last_p = first_p;
}

// <rl_mesh_recompile_parameters*> 0x004D5D40 (.h:74)
template<typename T>
void spsc_queue<T>::destroy()
{
    while (first_p != nullptr)
    {
        node* node_p = first_p;
        first_p = node_p->next_p;
        delete node_p;
    }
    first_p = nullptr;
}

// <rl_mesh_recompile_parameters*> 0x00504640 (.h:84)
// Producer side: appends a node and frees the nodes the consumer has passed.
template<typename T>
void spsc_queue<T>::enqueue(const T& t)
{
    last_p->next_p = new node(t);
    InterlockedExchange((volatile LONG*)&last_p, (LONG)last_p->next_p);
    while (first_p != divider_p)
    {
        node* node_p = first_p;
        first_p = first_p->next_p;
        delete node_p;
    }
}

// <rl_mesh_recompile_parameters*> 0x004D5D60 (.h:100)
// Consumer side.
template<typename T>
bool spsc_queue<T>::dequeue(T& result)
{
    if (divider_p == last_p)
    {
        return false;
    }
    result = divider_p->next_p->val;
    InterlockedExchange((volatile LONG*)&divider_p, (LONG)divider_p->next_p);
    return true;
}

// 0x004C3140 (:237)
rl_mesh_recompile_data::rl_mesh_recompile_data()
{
    numRecompileMeshDatas++;
}

// 0x004E98C0 (:242)
rl_mesh_recompile_data::~rl_mesh_recompile_data()
{
    delete[] render_blocks_p;
    keen::graphics::freeRenderGeometry(&geometry, Init_params.m_pKeenGraphicsSystem, nullptr);
    keen::MemoryAllocator* allocator_p = Init_params.m_pBufferAllocator;
    allocator_p->free(index_data_p);
    allocator_p->free(vertex_data_p);
    numRecompileMeshDatas--;
}

// 0x00528CD0 (:43)
// Defers the delete to the next finalize but one (the GPU may still use the data); deletes at once when the
// completion list is full.
void rl_delete_mesh_recompile_data(rl_mesh_recompile_data* pData)
{
    s_completionEntriesMutex.lock(0);
    for (unsigned int i = 0; i < 2048; i++)
    {
        if (s_completionEntries[i].pData == nullptr)
        {
            s_completionEntries[i].pData = pData;
            s_completionEntries[i].counter = 1;
            s_completionEntriesMutex.unlock();
            return;
        }
    }
    if (pData != nullptr)
    {
        delete pData;
    }
    s_completionEntriesMutex.unlock();
}

// 0x004C30E0 (:62)
// Lets a paused caller run: signals that the thread is idle and waits for the end of the pause.
void rl_mesh_recompile_block()
{
    s_handshake_start.signal();
    s_handshake_end.wait();
}

// 0x004C3100 (:70)
// The first pause waits until the recompile thread is idle.
void rl_mesh_recompile_pause()
{
    const unsigned int old_count = _InterlockedExchangeAdd((volatile long*)&Rl_recompile_mesh_pause_count, 1);
    if (old_count == 0)
    {
        s_handshake_start.wait();
    }
}

// 0x004C3120 (:79)
// The last unpause releases the recompile thread.
void rl_mesh_recompile_unpause()
{
    _InterlockedDecrement((volatile long*)&Rl_recompile_mesh_pause_count);
    if (Rl_recompile_mesh_pause_count == 0)
    {
        s_handshake_end.signal();
    }
}

// 0x00512720 (:88)
// Compiles the queued instances; when idle it signals s_handshake_start and waits for s_handshake_end.
int mesh_recompile_thread(void*)
{
    while (keep_running)
    {
        rl_mesh_recompile_parameters* params_p = nullptr;
        if (!Rl_recompile_mesh_enable || !recompile_queue.dequeue(params_p))
        {
            s_handshake_start.signal();
            s_handshake_end.wait();
            continue;
        }
        if (params_p->m_valid != 0)
        {
            params_p->m_recompile_data_p = params_p->m_instance_p->compile_cache_data(params_p);
        }
        finalize_queue.enqueue(params_p);
    }
    return 0;
}

// 0x00528D80 (:113)
// Magic numbers: 4096 parameter records (0xC bytes each).
void rl_mesh_recompile_initialize()
{
    recompile_queue.create();
    finalize_queue.create();
    s_completionEntriesMutex.create("completionEntries");
    s_params_mem_p = new unsigned char[0xC000];
    s_params_pool.init_dynamic(0x1000);
    for (int offset = 0; offset < 0xC000; offset += sizeof(rl_mesh_recompile_parameters))
    {
        s_params_pool.add((rl_mesh_recompile_parameters*)(s_params_mem_p + offset));
    }
    mesh_recompile_thread_handle = thread_spawn(mesh_recompile_thread, nullptr, 0, THREAD_PRIOR_LOW, "mesh recompile thread", 0);
    memset(s_completionEntries, 0, sizeof(s_completionEntries));
}

// 0x00528EC0 (:142)
// Ages the deferred deletes, then installs the finished recompiles (or discards them when disabled or invalidated)
// and returns their parameter records to the pool.
void rl_mesh_recompile_finalize()
{
    s_completionEntriesMutex.lock(0);
    for (unsigned int i = 0; i < 2048; i++)
    {
        rl_mesh_recompile_data* data_p = s_completionEntries[i].pData;
        if (data_p == nullptr)
        {
            continue;
        }
        if (s_completionEntries[i].counter == 0)
        {
            delete data_p;
            s_completionEntries[i].pData = nullptr;
        }
        else
        {
            s_completionEntries[i].counter--;
        }
    }
    s_completionEntriesMutex.unlock();
    rl_mesh_recompile_parameters* params_p = nullptr;
    while (finalize_queue.dequeue(params_p))
    {
        rl_destroyable_model_instance* instance_p = params_p->m_instance_p;
        rl_mesh_recompile_data* old_data_p;
        if (!Rl_recompile_mesh_enable || params_p->m_valid == 0)
        {
            // rl_destroyable_model_instance::set_compiled_cache_data(nullptr, ...) inlined; the new data is dropped.
            old_data_p = instance_p->set_compiled_cache_data(nullptr, params_p);
            if (params_p->m_recompile_data_p != nullptr)
            {
                delete params_p->m_recompile_data_p;
            }
        }
        else
        {
            old_data_p = instance_p->set_compiled_cache_data(params_p->m_recompile_data_p, params_p);
        }
        if (old_data_p != nullptr)
        {
            rl_delete_mesh_recompile_data(old_data_p);
        }
        start_recompile_cs.enter();
        s_params_pool.add(params_p);
        start_recompile_cs.leave();
    }
}

// 0x004E9840 (:188)
// Notes (decompiled behaviour, kept as is): the completion entries, the mutex and the parameter pool's array are not
// freed.
void rl_mesh_recompile_shutdown()
{
    keep_running = false;
    s_handshake_end.signal();
    thread_wait_until_done(mesh_recompile_thread_handle);
    delete[] s_params_mem_p;
    finalize_queue.destroy();
    recompile_queue.destroy();
}

// 0x005290D0 (:201)
// Queues a recompile of the instance (invalidating its pending one) when recompiling is enabled and a parameter record
// is free; outside init the recompile thread is paused around it.
void rl_start_recompile_mesh_instance(rl_destroyable_model_instance* instance_p, bool initing)
{
    if (!Rl_recompile_mesh_enable || s_params_pool.size() <= 0)
    {
        return;
    }
    if (!initing)
    {
        rl_mesh_recompile_pause();
    }
    start_recompile_cs.enter();
    rl_mesh_recompile_parameters* params_p = s_params_pool.remove_back();
    params_p->m_instance_p = instance_p;
    params_p->m_recompile_data_p = nullptr;
    _InterlockedExchange((volatile long*)&params_p->m_valid, 1);
    instance_p->set_recompile_parameters(params_p);
    recompile_queue.enqueue(params_p);
    start_recompile_cs.leave();
    if (!initing)
    {
        rl_mesh_recompile_unpause();
    }
}
