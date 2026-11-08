// Original: volition/stream2/code/stream2.cpp
//
// Containers live in Stream_containers (circular, next/prev) and Stream_container_hash (by name); primitives in
// Stream_prims (circular) and, for SCM_PRIM containers, Stream_prim_hash (by name and allocator, so containers share
// them). Requests run only on the owning thread (Stream_thread_id): stream2_request adds a reference and queues the
// container for loading unless it is loaded or queued; stream2_release drops it and queues the container for
// unloading at zero (aborting an in-flight read instead when it held the only reference).
// Queues (circular, q_next/q_prev): Stream_q (by cur_priority, highest first), Stream_q_passive (passive containers
// wait until other containers have loaded all their primitives) and Stream_q_unload. queue_process finishes the head
// container (stream_wait / abort_wait) and then starts the next one.
//
// Notes (decompiled behaviour, kept as is):
// - stream2_container_finish without a container leaves the build stack as it is.
// - stream2_read_container_data abandons a container whose primitive cannot be added (its build is not finished and
//   the rest of its records are not skipped) and continues only with patch_data.
// - q_sanity_check walks the container list (next) instead of the queues (q_next).
// - stream2_system_init ignores the task system; stream2_container_system_init clears the primitive types but not
//   the container types.

#include "volition/stream2/code/stream2.h"

#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/stream2/code/stream2_os.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <stdio.h>
#include <string.h>

// Primitive lookup by (file name, allocator): the key combines the name hash and the allocator index (make_key), so
// the same file streamed into two allocators is two primitives. Defined in this file (its members are recorded at
// stream2.cpp:120-146).
// size 4 + data_hash_table (0x1B590 for <stream2_prim *,7000,unsigned short>)
template<typename T, int N, typename I>
class stream2_prim_hash
{
public:
    bool get(const char* fname, int allocator, T& data); // <stream2_prim *,7000,unsigned short> 0x004AFD20 (:120)
    bool exists(const char*, int);
    bool insert(const char* fname, int allocator, T data); // <stream2_prim *,7000,unsigned short> 0x004AE4C0 (:130)
    void remove(const char* fname, int allocator); // <stream2_prim *,7000,unsigned short> 0x004AE500 (:135)
    void clear();
private:
    uint64_t make_key(const char* fname, int allocator); // <stream2_prim *,7000,unsigned short> 0x004AB7C0 (:146)
public:
    stream2_prim_hash(); // <stream2_prim *,7000,unsigned short> 0x004B0270
    ~stream2_prim_hash(); // <stream2_prim *,7000,unsigned short> 0x004AED50

private:
    data_hash_table<uint64_t, T, &uint64_hash, N, I> hash; // 0x00
};

// A container being built between stream2_container_start and stream2_container_finish: the primitives added so far
// (by name, with their add order) and the pool mark to roll back on stream2_container_abort.
// size 0x4014
struct build_item
{
    void clear(); // 0x004ABF50 (:87)
    build_item(); // 0x004B01B0
    ~build_item(); // 0x004B01E0

    stream2_container* container; // 0x00
    str_data_hash_table_cached<prim_add_entry, 1024, unsigned short> prims; // 0x04
    mempool_base* pool; // 0x400C
    int pool_mark; // 0x4010
};

stream2_container* Stream_containers; // 0x01AC3B50
stream2_prim* Stream_prims; // 0x01AC3B54
int Stream2_container_allocated; // 0x01AC3B5C

// ---- file-static globals
static stream2_prim_def _Stream_prim_defs[255]; // 0x01AD20D0, by prim_type (stream2_register_primitive_type)
static stream2_container_def _Stream_container_defs[255]; // 0x01B07E20, by container_type
static stream2_prim_hash<stream2_prim*, 7000, unsigned short> Stream_prim_hash; // 0x01AD5178 (:153)
static str_data_hash_table_cached<stream2_container*, 8000, unsigned short> Stream_container_hash; // 0x01AF0708 (:104)
static build_item Build_stack[2]; // 0x01AC3B68 (:95), nested container builds
static int Build_stack_pos = -1; // 0x014F9D18
static stream2_container* Stream_q; // 0x01AC3B40, load queue (circular, by cur_priority)
static stream2_container* Stream_q_passive; // 0x01AC3B44
static stream2_container* Stream_q_unload; // 0x01AC3B3C
static bool Stream_q_paused; // 0x01AC3B48, stream2_q_pause
static bool Stream2_system_init; // 0x01AC3B49
static unsigned int Stream_thread_id; // 0x01AC3B4C, owning thread (stream2_set_thread_ownership)
static int Stream2_container_count; // 0x01AC3B58

// ---- stream2_prim_hash

// <stream2_prim *,7000,unsigned short> 0x004B0270
template<typename T, int N, typename I>
stream2_prim_hash<T, N, I>::stream2_prim_hash()
    : hash(false)
{
}

// <stream2_prim *,7000,unsigned short> 0x004AED50
template<typename T, int N, typename I>
stream2_prim_hash<T, N, I>::~stream2_prim_hash()
{
}

// <stream2_prim *,7000,unsigned short> 0x004AB7C0 (:146)
template<typename T, int N, typename I>
uint64_t stream2_prim_hash<T, N, I>::make_key(const char* fname, int allocator)
{
    // The name CRC in the high half, the (sign-extended) allocator index in the low half.
    return ((uint64_t)crc_stri(fname, 0) << 32) | (uint64_t)(int64_t)allocator;
}

// <stream2_prim *,7000,unsigned short> 0x004AFD20 (:120)
template<typename T, int N, typename I>
bool stream2_prim_hash<T, N, I>::get(const char* fname, int allocator, T& data)
{
    return hash.get(make_key(fname, allocator), data);
}

// <stream2_prim *,7000,unsigned short> 0x004AE4C0 (:130)
template<typename T, int N, typename I>
bool stream2_prim_hash<T, N, I>::insert(const char* fname, int allocator, T data)
{
    return hash.insert_key_data(make_key(fname, allocator), data);
}

// <stream2_prim *,7000,unsigned short> 0x004AE500 (:135)
template<typename T, int N, typename I>
void stream2_prim_hash<T, N, I>::remove(const char* fname, int allocator)
{
    hash.remove(make_key(fname, allocator));
}

// ---- build_item

// 0x004B01B0
build_item::build_item()
{
    clear();
}

// 0x004B01E0
build_item::~build_item()
{
}

// 0x004ABF50 (:87)
void build_item::clear()
{
    container = nullptr;
    prims.clear();
    pool = nullptr;
    pool_mark = 0;
}

// ---- queues

// 0x004A9BB0 (:179)
static void enqueue_for_unload(stream2_container* sc)
{
    sc->flags |= 0x4;
    if (Stream_q_unload == nullptr)
    {
        sc->q_next = sc;
        sc->q_prev = sc;
        Stream_q_unload = sc;
        return;
    }
    sc->q_prev = Stream_q_unload->q_prev;
    sc->q_next = Stream_q_unload;
    Stream_q_unload->q_prev->q_next = sc;
    Stream_q_unload->q_prev = sc;
}

// 0x004A9BE0 (:189)
static void dequeue_for_unload(stream2_container* sc)
{
    sc->flags &= ~0x4;
    if (Stream_q_unload == sc && (Stream_q_unload = sc->q_next) == sc)
    {
        Stream_q_unload = nullptr;
    }
    else
    {
        sc->q_prev->q_next = sc->q_next;
        sc->q_next->q_prev = sc->q_prev;
    }
    sc->q_prev = nullptr;
    sc->q_next = nullptr;
}

// 0x004AED60 (:199)
static void queue_process_unload()
{
    stream2_container* sc = Stream_q_unload;
    while (sc != nullptr)
    {
        stream2_container* const next = sc->q_next != Stream_q_unload ? sc->q_next : nullptr;
        sc->unload();
        dequeue_for_unload(sc);
        sc = next;
    }
}

// 0x004A9C30 (:221)
static void enqueue(stream2_container* sc)
{
    if ((sc->flags & 0x100) != 0)
    {
        // Passive containers wait in their own queue, in request order.
        if ((sc->flags & 0x2) != 0)
        {
            return;
        }
        if (Stream_q_passive != nullptr)
        {
            Stream_q_passive->q_prev->q_next = sc;
            sc->q_prev = Stream_q_passive->q_prev;
            Stream_q_passive->q_prev = sc;
            sc->flags |= 0x2;
            sc->q_next = Stream_q_passive;
            Stream_q_passive = sc;
            return;
        }
        sc->flags |= 0x2;
        sc->q_next = sc;
        sc->q_prev = sc;
        Stream_q_passive = sc;
        return;
    }

    if (Stream_q == nullptr)
    {
        sc->q_next = sc;
        sc->q_prev = sc;
        Stream_q = sc;
        sc->flags |= 0x2;
        return;
    }
    if ((sc->flags & 0x2) != 0)
    {
        // Re-queue (priority change): take it out first.
        if (Stream_q == sc && (Stream_q = sc->q_next) == sc)
        {
            Stream_q = nullptr;
        }
        else
        {
            sc->q_prev->q_next = sc->q_next;
            sc->q_next->q_prev = sc->q_prev;
        }
        sc->flags &= ~0x2;
        sc->q_prev = nullptr;
        sc->q_next = nullptr;
    }
    stream2_container* const head = Stream_q;
    if (head == nullptr)
    {
        sc->flags |= 0x2;
        sc->q_next = sc;
        sc->q_prev = sc;
        Stream_q = sc;
        return;
    }
    // From the tail, the last container with at least this priority (never past the head).
    stream2_container* const tail = head->q_prev;
    stream2_container* after = tail;
    if (tail != head)
    {
        do
        {
            if (sc->cur_priority <= after->cur_priority)
            {
                break;
            }
            after = after == head ? nullptr : after->q_prev;
        } while (after != head);
    }
    if (after == nullptr)
    {
        tail->q_next = sc;
        sc->q_prev = head->q_prev;
        head->q_prev = sc;
        sc->flags |= 0x2;
        sc->q_next = head;
        Stream_q = sc;
        return;
    }
    sc->q_next = after->q_next;
    sc->q_prev = after;
    after->q_next->q_prev = sc;
    after->q_next = sc;
    sc->flags |= 0x2;
}

// 0x004A9D60 (:270)
static void dequeue(stream2_container* sc)
{
    stream2_container*& queue = (sc->flags & 0x100) == 0 ? Stream_q : Stream_q_passive;
    if (queue == sc && (queue = sc->q_next) == sc)
    {
        sc->flags &= ~0x2;
        queue = nullptr;
        sc->q_next = nullptr;
        sc->q_prev = nullptr;
        return;
    }
    sc->q_prev->q_next = sc->q_next;
    sc->q_next->q_prev = sc->q_prev;
    sc->flags &= ~0x2;
    sc->q_next = nullptr;
    sc->q_prev = nullptr;
}

// 0x004B0920 (:291)
static void queue_process()
{
    static bool re_entrant; // 0x01B09A04
    if (re_entrant)
    {
        return;
    }
    re_entrant = true;
    stream2_container* sc = Stream_q;
    if (sc == nullptr)
    {
        re_entrant = false;
        return;
    }
    if ((sc->flags & 0x10) != 0 || (sc->flags & 0x8) != 0)
    {
        // The head is reading (or aborting): wait for it.
        const bool busy = (sc->flags & 0x10) != 0 ? sc->abort_wait() : sc->stream_wait();
        if (busy)
        {
            re_entrant = false;
            return;
        }
        dequeue(sc);
    }
    if (!Stream_q_paused)
    {
        // Start the next container; drop the ones that cannot be allocated (stream(true), inlined in the original).
        while (Stream_q != nullptr)
        {
            sc = Stream_q;
            if (sc->stream(true))
            {
                break;
            }
            dequeue(sc);
        }
    }
    re_entrant = false;
}

// 0x004AEDE0 (:375)
static void queue_process_passive()
{
    stream2_container* next = Stream_q_passive;
    for (;;)
    {
        stream2_container* const sc = next;
        if (sc == nullptr)
        {
            return;
        }
        next = sc->q_next != Stream_q_passive ? sc->q_next : nullptr;

        // Wait until other containers have read every primitive.
        bool all_read = true;
        for (int i = 0; i < sc->num_entries; i++)
        {
            if (sc->entries[i].enable && (sc->entries[i].prim->flags & 0x1) == 0)
            {
                all_read = false;
                break;
            }
        }
        if (!all_read)
        {
            continue;
        }

        for (int i = 0; i < sc->num_entries; i++)
        {
            if (!sc->entries[i].enable)
            {
                continue;
            }
            stream2_prim* const prim = sc->entries[i].prim;
            if ((prim->flags & 0x1) == 0 && prim->pdef->flags.load_threaded)
            {
                const bool split = (prim->flags & 0x4) != 0;
                if (!prim->pdef->load_cb(prim->name, (unsigned int)sc, prim->cpu_data, prim->cpu_size, split ? prim->gpu_data : nullptr, split ? prim->gpu_size : 0))
                {
                    break;
                }
                prim->flags |= 0x1;
            }
        }
        bool failed = false;
        for (int i = 0; i < sc->num_entries; i++)
        {
            if (!sc->entries[i].enable)
            {
                continue;
            }
            stream2_prim* const prim = sc->entries[i].prim;
            if ((prim->flags & 0x1) == 0 && !prim->pdef->flags.load_threaded)
            {
                const bool split = (prim->flags & 0x4) != 0;
                if (!prim->pdef->load_cb(prim->name, (unsigned int)sc, prim->cpu_data, prim->cpu_size, split ? prim->gpu_data : nullptr, split ? prim->gpu_size : 0))
                {
                    failed = true;
                    break;
                }
                prim->flags |= 0x1;
            }
        }
        if (!failed && sc->cdef->load_cb != nullptr)
        {
            sc->cdef->load_cb((unsigned int)sc);
        }
        sc->flags |= 0x1;
        dequeue(sc);
    }
}

// 0x004ABFC0 (:413)
// LTCG: pool in EDI, the other arguments on the stack; `category` is not passed (unused).
static stream2_prim* stream2_create_prim(const char* filename, const char* category, unsigned char type, unsigned char allocator, mempool_base* pool, stream2_prim_record* precalc)
{
    stream2_prim* const prim = (stream2_prim*)pool->alloc(sizeof(stream2_prim), 4);
    if (prim == nullptr)
    {
        return nullptr;
    }
    Stream2_container_allocated += sizeof(stream2_prim);
    prim->init();
    prim->name = (char*)pool->alloc((unsigned int)strlen(filename) + 1, 0);
    if (prim->name == nullptr)
    {
        return nullptr;
    }
    strcpy(prim->name, filename);
    prim->pdef = &_Stream_prim_defs[type];
    prim->allocator_idx = allocator;
    if (precalc != nullptr)
    {
        prim->flags = precalc->flags & 0xC7;
        prim->split_ext_index = precalc->split_ext_index;
        prim->cpu_size = precalc->cpu_size;
        prim->gpu_size = precalc->gpu_size;
        return prim;
    }

    // Split into a CPU and a GPU file when a GPU file of one of the type's extensions exists.
    char gpu_name[256];
    char cpu_name[256];
    prim->flags &= ~0x4;
    for (unsigned char ext = 0; ext < 2; ext++)
    {
        if (prim->pdef->gpu_ext[ext] == nullptr)
        {
            continue;
        }
        cf_add_ext(gpu_name, sizeof(gpu_name), filename, prim->pdef->gpu_ext[ext], false);
        if (cf_exist(gpu_name))
        {
            prim->flags |= 0x4;
            cf_add_ext(cpu_name, sizeof(cpu_name), filename, prim->pdef->cpu_ext[ext], false);
            prim->split_ext_index = ext;
            break;
        }
    }
    unsigned int size = 0;
    if ((prim->flags & 0x4) == 0)
    {
        if (!cf_get_file_size(&size, prim->name, false))
        {
            return nullptr;
        }
        prim->cpu_size = (int)size;
        prim->gpu_size = 0;
        return prim;
    }
    if (!cf_get_file_size(&size, cpu_name, false))
    {
        return nullptr;
    }
    prim->cpu_size = (int)size;
    if (!cf_get_file_size(&size, gpu_name, false))
    {
        return nullptr;
    }
    prim->gpu_size = (int)size;
    return prim;
}

// ---- system

// 0x004AC1B0 (:506)
void stream2_container_system_init()
{
    Stream_prims = nullptr;
    Stream_containers = nullptr;
    Stream_q = nullptr;
    Stream_q_passive = nullptr;
    memset(_Stream_prim_defs, 0, sizeof(_Stream_prim_defs));
    for (int i = 0; i < 255; i++)
    {
        _Stream_prim_defs[i].prim_type = 0;
    }
    Stream_thread_id = get_current_thread_id();
}

// 0x004B0D10 (:524)
void stream2_container_system_shutdown()
{
    // Releases every remaining reference (aborting in-flight reads) and queues the containers for unloading.
    stream2_container* sc = Stream_containers;
    if (sc == nullptr)
    {
        return;
    }
    do
    {
        if (sc->get_ref_count() != 0 && Stream_thread_id == get_current_thread_id())
        {
            if (sc->get_ref_count() == 1 && (sc->flags & 0x8) != 0)
            {
                sc->abort();
            }
            else
            {
                sc->dec_ref_count();
                if (sc->get_ref_count() < 1)
                {
                    if ((sc->flags & 0x2) != 0)
                    {
                        dequeue(sc);
                    }
                    enqueue_for_unload(sc);
                }
            }
        }
        sc = sc->next;
    } while (sc != Stream_containers);
}

// 0x004A9DE0 (:540)
void stream2_set_priority(unsigned int srid, unsigned char priority)
{
    stream2_container* const sc = (stream2_container*)srid;
    if (sc != nullptr && sc->cur_priority <= (short)priority)
    {
        sc->cur_priority = priority;
        if ((sc->flags & 0x2) != 0 && (sc->flags & 0x8) == 0)
        {
            enqueue(sc);
        }
    }
}

// 0x004A9E10 (:563)
void stream2_set_category(unsigned int srid, unsigned char category)
{
    if (srid != 0)
    {
        ((stream2_container*)srid)->cur_category = category;
    }
}

// 0x004A9E30 (:595)
bool stream2_container_get_passive(unsigned int srid)
{
    if (srid == 0)
    {
        return false;
    }
    return (((stream2_container*)srid)->flags & 0x100) != 0;
}

// 0x004AC200 (:606)
bool stream2_container_set_passive(unsigned int srid, bool passive)
{
    if (srid == 0)
    {
        return false;
    }
    return ((stream2_container*)srid)->set_passive(passive);
}

// 0x004AC230 (:1149)
void stream2_system_init(keen::TaskSystem* pTaskSystem)
{
    if (!Stream2_system_init)
    {
        Stream2_system_init = true;
        stream2_container_system_init();
    }
}

// 0x004B1320 (:1170)
void stream2_system_shutdown()
{
    if (!Stream2_system_init)
    {
        return;
    }
    Stream2_system_init = false;
    stream2_container_system_shutdown();
    if (Stream_thread_id == get_current_thread_id())
    {
        queue_process_unload();
        queue_process();
        queue_process_passive();
    }
    stream2_os_system_shutdown();
}

// 0x004B0E00 (:1201)
bool stream2_do_frame(bool do_passive)
{
    if (Stream_thread_id != get_current_thread_id())
    {
        return true;
    }
    queue_process_unload();
    queue_process();
    if (do_passive)
    {
        queue_process_passive();
    }
    return Stream_q != nullptr || (do_passive && Stream_q_passive != nullptr);
}

// 0x004A9E50 (:1235)
void stream2_set_thread_ownership()
{
    Stream_thread_id = get_current_thread_id();
}

// 0x004A9E60 (:1241)
void stream2_q_pause()
{
    Stream_q_paused = true;
}

// 0x004A9E70 (:1247)
void stream2_q_unpause()
{
    Stream_q_paused = false;
}

// 0x004A9E80 (:1254)
bool stream2_q_busy()
{
    if (Stream_thread_id != get_current_thread_id())
    {
        return true;
    }
    if (Stream_q_paused)
    {
        return Stream_q != nullptr && (Stream_q->flags & 0x8) != 0;
    }
    return Stream_q != nullptr;
}

// 0x004AEF40 (:1273)
bool stream2_request(unsigned int srid)
{
    if (Stream_thread_id != get_current_thread_id())
    {
        return false;
    }
    stream2_container* const sc = (stream2_container*)srid;
    if (sc == nullptr || (sc->flags & 0x40) != 0)
    {
        return false;
    }
    if ((sc->flags & 0x10) != 0)
    {
        // Aborting: load again once the abort is done.
        sc->flags |= 0x20;
        return true;
    }
    sc->inc_ref_count();
    if ((sc->flags & 0x4) != 0)
    {
        dequeue_for_unload(sc);
    }
    if ((sc->flags & 0x1) == 0 && (sc->flags & 0x2) == 0)
    {
        enqueue(sc);
    }
    return true;
}

// 0x004B0E50 (:1323)
bool stream2_preload(unsigned int srid)
{
    if (Stream_thread_id != get_current_thread_id())
    {
        return false;
    }
    if (srid == 0)
    {
        return false;
    }
    // Drain the load queue, then load this container synchronously.
    do
    {
        while (Stream_thread_id != get_current_thread_id())
        {
        }
        queue_process_unload();
        queue_process();
    } while (Stream_q != nullptr);
    if (!stream2_request(srid))
    {
        return false;
    }
    const bool disc_paused = stream2_os_disc_is_paused();
    stream2_os_unpause_disc_processing();
    for (stream2_status_val status = stream2_get_status(srid); status != ST_LOADED; status = stream2_get_status(srid))
    {
        if (status == ST_ERROR)
        {
            stream2_release(srid);
            return false;
        }
        if (Stream_thread_id == get_current_thread_id())
        {
            queue_process_unload();
            queue_process();
            queue_process_passive();
        }
        thread_yield();
    }
    if (disc_paused)
    {
        stream2_os_pause_disc_processing();
    }
    return true;
}

// 0x004B02A0 (:1413)
void stream2_release(unsigned int srid)
{
    if (Stream_thread_id != get_current_thread_id() || srid == 0)
    {
        return;
    }
    stream2_container* const sc = (stream2_container*)srid;
    if (sc->get_ref_count() == 1 && (sc->flags & 0x8) != 0)
    {
        // The last reference of a container being read: abort the read.
        sc->abort();
        return;
    }
    sc->dec_ref_count();
    if (sc->get_ref_count() < 1)
    {
        if ((sc->flags & 0x2) != 0)
        {
            dequeue(sc);
        }
        enqueue_for_unload(sc);
    }
}

// 0x004AC250 (:1458)
stream2_status_val stream2_get_status(unsigned int srid)
{
    const stream2_container* const sc = (const stream2_container*)srid;
    if (sc == nullptr)
    {
        return ST_UNLOADED;
    }
    if ((sc->flags & 0x40) != 0)
    {
        return ST_ERROR;
    }
    if ((sc->flags & 0x10) != 0)
    {
        return (sc->flags & 0x20) != 0 ? ST_LOADING : ST_ABORTING;
    }
    if ((sc->flags & 0x4) != 0 || sc->get_ref_count() < 1)
    {
        return ST_UNLOADED;
    }
    if ((sc->flags & 0x1) != 0)
    {
        return ST_LOADED;
    }
    return (stream2_status_val)((sc->flags & 0x2) >> 1);
}

// 0x004AC2A0 (:1513)
int stream2_get_ref_count(unsigned int srid)
{
    if (srid == 0)
    {
        return 0;
    }
    return ((stream2_container*)srid)->get_ref_count();
}

// 0x004AC2B0 (:1526)
bool stream2_register_primitive_type(stream2_primitive_type_reg& prim, mempool* pool)
{
    if (prim.load_cb == nullptr || prim.unload_cb == nullptr)
    {
        return false;
    }
    stream2_prim_def& def = _Stream_prim_defs[prim.prim_type];
    def.load_cb = prim.load_cb;
    def.unload_cb = prim.unload_cb;
    def.cpu_align = prim.cpu_memalign < 1 ? 1 : prim.cpu_memalign;
    def.gpu_align = prim.gpu_memalign < 1 ? 1 : prim.gpu_memalign;
    def.flags.debug_skippable = prim.flags.debug_skippable;
    def.flags.alloc_split = prim.flags.alloc_split;
    def.flags.load_threaded = prim.flags.load_threaded;
    def.name = (char*)pool->alloc((unsigned int)strlen(prim.name) + 1, 1);
    def.prim_type = prim.prim_type;
    strcpy(def.name, prim.name);
    def.cpu_ext[0] = prim.cpu_ext[0];
    def.cpu_ext[1] = prim.cpu_ext[1];
    def.gpu_ext[0] = prim.gpu_ext[0];
    def.gpu_ext[1] = prim.gpu_ext[1];
    return true;
}

// 0x004A9EC0 (:1561)
bool stream2_register_container_type(stream2_container_type_reg& container, mempool* pool)
{
    if (container.allocator_type == 0)
    {
        return false;
    }
    stream2_container_def& def = _Stream_container_defs[container.container_type];
    def.load_cb = container.load_cb;
    def.unload_cb = container.unload_cb;
    def.allocator_idx = container.allocator_type;
    def.allocator = _Stream_allocators[container.allocator_type];
    def.name = (char*)pool->alloc((unsigned int)strlen(container.name) + 1, 1);
    def.container_type = container.container_type;
    strcpy(def.name, container.name);
    def.mode = container.mode;
    return true;
}

// 0x004AC380 (:1589)
void q_sanity_check(stream2_prim* prim)
{
    // A primitive must not be in a queued container.
    stream2_container* const queues[2] = { Stream_q, Stream_q_passive };
    for (int q = 0; q < 2; q++)
    {
        stream2_container* sc = queues[q];
        stream2_container* const first = queues[q];
        if (sc == nullptr)
        {
            continue;
        }
        do
        {
            for (int i = 0; i < sc->num_entries; i++)
            {
                if (sc->entries[i].prim == prim)
                {
                    VLIB_CRITICAL_FAILURE();
                    break;
                }
            }
            sc = sc->next;
        } while (sc != first);
    }
}

// 0x004AC440 (:1611)
bool stream2_container_start(const char* name, mempool_base* pool, unsigned char tpe, bool passive, bool force_no_stream_generation)
{
    if (Build_stack_pos > 1)
    {
        return false;
    }
    build_item& build = Build_stack[++Build_stack_pos];
    build.container = nullptr;
    build.prims.clear();
    build.pool_mark = 0;
    build.pool = pool;
    if ((pool->get_flags() & 0x2) == 0)
    {
        build.pool_mark = (int)pool->mark();
    }
    stream2_container* const sc = (stream2_container*)pool->alloc(sizeof(stream2_container), 4);
    Stream2_container_allocated += sizeof(stream2_container);
    build.container = sc;
    if (sc == nullptr)
    {
        return false;
    }
    sc->init(name, pool, &_Stream_container_defs[tpe], passive, force_no_stream_generation);
    return true;
}

// 0x004B0310 (:1655)
stream2_prim* stream2_container_add_prim(const char* filename, const char* category, unsigned char tpe, unsigned char allocator, bool report_errors_immediately, unsigned char add_flags, stream2_prim_record* precalc)
{
    if ((unsigned int)Build_stack_pos > 1)
    {
        return nullptr;
    }
    build_item& build = Build_stack[Build_stack_pos];
    prim_add_entry added;
    if (build.prims.get(filename, added))
    {
        return added.prim;
    }
    if (build.container == nullptr)
    {
        return nullptr;
    }
    const stream2_container_def* const cdef = build.container->cdef;
    stream2_prim* prim;
    if (cdef->mode == SCM_CONTAINER)
    {
        // Container-mode primitives are private to their container.
        prim = stream2_create_prim(filename, category, tpe, allocator, build.pool, precalc);
    }
    else
    {
        if (allocator == 0)
        {
            allocator = (unsigned char)cdef->allocator_idx;
        }
        prim = nullptr;
        Stream_prim_hash.get(filename, allocator, prim);
        if (prim == nullptr)
        {
            prim = stream2_create_prim(filename, category, tpe, allocator, build.pool, precalc);
            if (prim == nullptr)
            {
                return nullptr;
            }
            Stream_prim_hash.insert(prim->name, allocator, prim);
            prim->flags |= 0x2;
        }
    }
    if (prim == nullptr)
    {
        return nullptr;
    }
    prim->flags |= add_flags;
    return stream2_container_add_prim(prim) ? prim : nullptr;
}

// 0x004AEFA0 (:1752)
bool stream2_container_add_prim(stream2_prim* prim)
{
    if ((unsigned int)Build_stack_pos > 1 || prim == nullptr)
    {
        return false;
    }
    build_item& build = Build_stack[Build_stack_pos];
    if (!build.prims.exists(prim->name))
    {
        prim_add_entry added;
        added.prim = prim;
        added.add_index = build.prims.get_count();
        build.prims.insert_key_data(prim->name, added);
    }
    return true;
}

// 0x004AC4F0 (:1774)
void stream2_container_abort()
{
    if ((unsigned int)Build_stack_pos >= 2)
    {
        return;
    }
    build_item& build = Build_stack[Build_stack_pos];
    if ((build.pool->get_flags() & 0x2) == 0)
    {
        build.pool->restore_to_mark((unsigned int)build.pool_mark);
    }
    build.container = nullptr;
    build.prims.clear();
    Build_stack_pos--;
    build.pool = nullptr;
    build.pool_mark = 0;
}

// 0x004B0F10 (:1795)
unsigned int stream2_container_finish(bool skip_allocator_size_verification, bool patch)
{
    if ((unsigned int)Build_stack_pos > 1)
    {
        return 0;
    }
    build_item& build = Build_stack[Build_stack_pos];

    // New primitives join the global list.
    for (prim_add_entry* added = build.prims.get_first(); added != nullptr; added = build.prims.get_next(added))
    {
        stream2_prim* const prim = added->prim;
        if (prim->get_container_ref_count() != 0)
        {
            continue;
        }
        if (Stream_prims == nullptr)
        {
            prim->next = prim;
            prim->prev = prim;
            Stream_prims = prim;
        }
        else
        {
            prim->prev = Stream_prims->prev;
            prim->next = Stream_prims;
            Stream_prims->prev->next = prim;
            Stream_prims->prev = prim;
        }
    }

    if (build.container == nullptr)
    {
        return 0;
    }
    build.container->add_prims(&build.prims, build.pool, skip_allocator_size_verification);
    if (Stream_container_hash.exists(build.container->name))
    {
        build.clear();
        Build_stack_pos--;
        return 0;
    }
    Stream_container_hash.insert_key_data(build.container->name, build.container);
    stream2_container* const sc = build.container;
    if (Stream_containers == nullptr)
    {
        sc->next = sc;
        sc->prev = sc;
        Stream_containers = sc;
    }
    else
    {
        sc->prev = Stream_containers->prev;
        sc->next = Stream_containers;
        Stream_containers->prev->next = sc;
        Stream_containers->prev = sc;
    }
    build.clear();
    Build_stack_pos--;
    Stream2_container_count++;
    return (unsigned int)sc;
}

// 0x004B1360 (:1855)
bool stream2_container_delete(const char* name)
{
    stream2_container* sc = nullptr;
    if (Stream_container_hash.get(name, sc) && sc != nullptr)
    {
        return stream2_container_delete((unsigned int)sc);
    }
    return false;
}

// 0x004B1050 (:1864)
bool stream2_container_delete(unsigned int srid)
{
    if (srid == 0)
    {
        return false;
    }
    // Finish everything queued first.
    do
    {
        while (Stream_thread_id != get_current_thread_id())
        {
        }
        queue_process_unload();
        queue_process();
    } while (Stream_q != nullptr);

    stream2_container* const sc = (stream2_container*)srid;
    if (!sc->is_idle())
    {
        return false;
    }
    for (int i = 0; i < sc->num_entries; i++)
    {
        stream2_prim* const prim = sc->entries[i].prim;
        prim->dec_container_ref_count();
        if (prim->get_container_ref_count() != 0)
        {
            continue;
        }
        if (Stream_prims == prim && (Stream_prims = prim->next) == prim)
        {
            prim->next = nullptr;
            prim->prev = nullptr;
            Stream_prims = nullptr;
        }
        else
        {
            if (prim->prev != nullptr)
            {
                prim->prev->next = prim->next;
            }
            if (prim->next != nullptr)
            {
                prim->next->prev = prim->prev;
            }
            prim->next = nullptr;
            prim->prev = nullptr;
        }
        if ((prim->flags & 0x2) != 0)
        {
            Stream_prim_hash.remove(prim->name, prim->allocator_idx);
        }
    }
    sc->clear();
    Stream_container_hash.remove(sc->name);
    if (Stream_containers == sc && (Stream_containers = sc->next) == sc)
    {
        Stream2_container_count--;
        sc->next = nullptr;
        sc->prev = nullptr;
        Stream_containers = nullptr;
        return true;
    }
    if (sc->prev != nullptr)
    {
        sc->prev->next = sc->next;
    }
    if (sc->next != nullptr)
    {
        sc->next->prev = sc->prev;
    }
    Stream2_container_count--;
    sc->next = nullptr;
    sc->prev = nullptr;
    return true;
}

// 0x004AF030 (:1907)
unsigned int stream2_container_find(const char* name)
{
    stream2_container* sc = nullptr;
    if (Stream_container_hash.get(name, sc))
    {
        return (unsigned int)sc;
    }
    return 0;
}

// 0x004AF080 (:1916)
stream2_container* stream_container_find_container(const char* name)
{
    stream2_container* sc = nullptr;
    if (Stream_container_hash.get(name, sc) && sc != nullptr)
    {
        return sc;
    }
    return nullptr;
}

// 0x004A9F50 (:1926)
bool stream2_container_data_is_unique(unsigned int srid)
{
    if (srid == 0)
    {
        return false;
    }
    return ((stream2_container*)srid)->cdef->mode == SCM_CONTAINER;
}

// 0x004AF0C0 (:1938)
bool stream2_container_enable_prim(unsigned int srid, const char* prim_name, bool enable)
{
    if (srid == 0)
    {
        return false;
    }
    return ((stream2_container*)srid)->enable_prim(prim_name, enable);
}

// 0x004A9F70 (:1956)
const char* stream2_get_container_name(unsigned int srid)
{
    if (srid == 0)
    {
        return nullptr;
    }
    return ((stream2_container*)srid)->name;
}

// 0x004A9F80 (:1968)
stream2_container* stream2_get_container(unsigned int srid)
{
    return (stream2_container*)srid;
}

// 0x004A9F90 (:1977)
unsigned int stream2_get_srid(stream2_container* sc)
{
    return (unsigned int)sc;
}

// 0x004A9FA0 (:1987)
void stream2_reset_allocated_stats()
{
    Stream2_container_allocated = 0;
}

// 0x004B1660 (:2105)
bool stream2_read_container_data(const char* filename, mempool_base* tmp_pool, mempool_base* pool, bool report_errors, bool is_temp, bool patch_data)
{
    // Reads the whole file into tmp_pool and parses it from memory.
    cfile* f = cf_open(filename, "rb", PLATFORM_PC, false);
    if (f == nullptr)
    {
        return false;
    }
    const unsigned int size = cf_size(f);
    unsigned int cpu_mark;
    unsigned int gpu_mark = 0;
    if ((tmp_pool->get_flags() & 0x40) == 0)
    {
        cpu_mark = tmp_pool->mark();
    }
    else
    {
        cpu_mark = ((split_mempool*)tmp_pool)->mark_cpu();
        gpu_mark = ((split_mempool*)tmp_pool)->mark_gpu();
    }
    unsigned char* const buf = (unsigned char*)tmp_pool->alloc(size, 0);
    if (cf_read(buf, size, f, false) != size)
    {
        cf_close(f);
        return false;
    }
    cf_close(f);
    f = cf_open(buf, size, "rb", PLATFORM_PC);
    const bool result = stream2_read_container_data(f, pool, report_errors, is_temp, patch_data);
    cf_close(f);
    if ((tmp_pool->get_flags() & 0x40) != 0)
    {
        ((split_mempool*)tmp_pool)->restore_to_mark_cpu(cpu_mark);
        ((split_mempool*)tmp_pool)->restore_to_mark_gpu(gpu_mark);
        return result;
    }
    tmp_pool->restore_to_mark(cpu_mark);
    return result;
}

// 0x004B13B0 (:2142)
bool stream2_read_container_data(cfile* f, mempool_base* pool, bool report_errors, bool is_temp, bool patch_data)
{
    // Container assembly data: stream2_container_header, then one container record after the other (see
    // stream2_container::load_container, which reads one record the same way).
    stream2_container_header header;
    cf_read(&header, sizeof(header), f, false);
    if (is_temp && !dlc_allocate_temp_container_list(header.num_containers, pool))
    {
        is_temp = false;
    }
    for (int c = 0; c < (int)header.num_containers; c++)
    {
        char container_name[64];
        cf_read_string(container_name, sizeof(container_name), f);
        const unsigned char type = cf_read_uint8(f);
        const unsigned short container_flags = (unsigned short)cf_read_int16(f);
        const short num_prims = cf_read_int16(f);
        const unsigned int base_offset = cf_read_uint32(f);
        const int num_chain_sizes = cf_read_int32(f);
        const int compressed_size = cf_read_int32(f);
        const unsigned int chain_sizes_pos = cf_tell(f);
        if (num_chain_sizes > 0)
        {
            cf_seek(f, num_chain_sizes * 4, CF_SEEK_CUR);
        }
        stream2_container_start(container_name, pool, type, ((container_flags >> 8) & 1) != 0, false);

        bool failed = false;
        for (int p = 0; p < num_prims; p++)
        {
            stream2_prim_record record;
            cf_read_string(record.name, sizeof(record.name), f);
            record.type = cf_read_uint8(f);
            record.allocator = cf_read_uint8(f);
            record.flags = cf_read_uint8(f);
            record.split_ext_index = cf_read_uint8(f);
            record.cpu_size = cf_read_int32(f);
            record.gpu_size = cf_read_int32(f);
            if (stream2_container_add_prim(record.name, "", record.type, record.allocator, report_errors, record.flags, &record) == nullptr)
            {
                failed = true;
                break;
            }
        }
        if (!failed)
        {
            const unsigned int srid = stream2_container_finish(true, patch_data);
            if (is_temp && srid != 0)
            {
                dlc_store_temp_srid(srid);
            }
            if (num_chain_sizes > 0)
            {
                const unsigned int end_pos = cf_tell(f);
                cf_seek(f, (int)chain_sizes_pos, CF_SEEK_SET);
                if (srid != 0)
                {
                    stream2_container* const sc = (stream2_container*)srid;
                    if ((sc->flags & 0x100) == 0)
                    {
                        for (int i = 0; i < num_chain_sizes; i++)
                        {
                            sc->pf_chain_read_size[i] = cf_read_uint32(f);
                        }
                    }
                    sc->pf_base_offset = base_offset;
                    sc->pf_total_compressed_read_size = compressed_size;
                    if ((container_flags & 0x80) != 0)
                    {
                        sc->flags |= 0x80;
                    }
                }
                cf_seek(f, (int)end_pos, CF_SEEK_SET);
            }
            if (srid != 0)
            {
                continue;
            }
        }
        if (!patch_data)
        {
            return false;
        }
    }
    return true;
}

// 0x004A9FB0 (:2185)
stream2_container* stream2_get_first_container()
{
    return Stream_containers;
}

// 0x004A9FC0 (:2190)
stream2_container* stream2_get_next_container(stream2_container* current)
{
    return current->next != Stream_containers ? current->next : nullptr;
}

// ---- stream2.h inline code

// 0x007B9920 (.h:59)
stream2_primitive_type_reg::stream2_primitive_type_reg()
{
    prim_type = 0xFF;
    name[0] = '\0';
    load_cb = nullptr;
    unload_cb = nullptr;
    cpu_memalign = 0;
    gpu_memalign = 0;
    cpu_ext[0] = nullptr;
    gpu_ext[0] = nullptr;
    cpu_ext[1] = nullptr;
    gpu_ext[1] = nullptr;
    flags.debug_skippable = 0;
    flags.alloc_split = 0;
}

// 0x007B9950 (.h:103)
stream2_container_type_reg::stream2_container_type_reg()
{
    container_type = 0xFF;
    name[0] = '\0';
    load_cb = nullptr;
    unload_cb = nullptr;
    mode = SCM_NONE;
    allocator_type = 0;
}
