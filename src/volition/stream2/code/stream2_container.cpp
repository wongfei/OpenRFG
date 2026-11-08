// Original: volition/stream2/code/stream2_container.cpp
//
// Partial: Error_pool / signal_last_error (an empty function in this build) and get_loaded_size (no address) are not
// implemented.
//
// Allocation (alloc):
// - SCM_PRIM: every enabled primitive without data gets its own allocation from its allocator (allocator_idx, or the
//   container type's). For alloc_split types whose GPU file exists (except chunks), the GPU part goes to the start of
//   the GPU staging block s_d3dMemoryBlock (each primitive of the call after the previous one, 16-byte aligned);
//   otherwise the GPU part follows the CPU part in the same allocation.
// - SCM_CONTAINER: one CPU block for all primitives (calc_container_requirements), GPU parts of alloc_split types in
//   the staging block. A "Vehicle" container also gets the .xwb_pc wavebanks from the audio slot allocator (index
//   0x14).
// Loading: stream() issues the reads (one chain read of <name>.str2_pc for flag 0x80, otherwise one read per CPU and
// GPU file); stream_wait() hands the container to the loading thread for the load_threaded primitives, then runs the
// other load callbacks and the container type's load callback on the owning thread.
//
// Notes (decompiled behaviour, kept as is):
// - In SCM_PRIM alloc a primitive with a GPU size whose type is not alloc_split gets a staging-block pointer that is
//   then replaced by the address after its CPU part, which its allocation does not cover.
// - The 6-argument calc_container_requirements does not exclude chunks from the GPU block and treats a primitive
//   named exactly "vehicle" as a normal one; entry flags (entry::flags) are left uninitialised by add_prims (0 here).
// - stream_wait skips the container type's load callback when a primitive's callback fails, but still marks the
//   container loaded.

#include "volition/stream2/code/stream2_container.h"

#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include "keen/base/memory.hpp"

#include <stdio.h>
#include <string.h>

volatile bool g_stream2ContainerLoaded; // 0x01AC3B4A
volatile stream2_container* g_pStream2ContainerToLoad; // 0x01AC3B60
keen::Event g_stream2ContainerLoadEvent; // 0x01AC3B64
keen::Thread g_stream2ContainerLoadThread; // 0x01AD5120

const int stream2_container::STREAM2_LAST_ERROR_MAX = 50; // 0x012B80D0

// Rounds value up to a multiple of align.
static int align_up(int value, int align)
{
    const int rest = value % align;
    return rest == 0 ? value : value + align - rest;
}

// 0x004AF680 (:68)
int stream2_container_load(const keen::Thread& thread)
{
    // The thread argument is the init function (stream2LoadInitFunction).
    ((void (*)())thread.getArgument())();
    if (thread.isQuitRequested())
    {
        return 0;
    }
    for (;;)
    {
        g_stream2ContainerLoadEvent.sleepUntilSignaled();
        stream2_container* const sc = (stream2_container*)g_pStream2ContainerToLoad;
        if (thread.isQuitRequested())
        {
            return 0;
        }
        if (sc != nullptr)
        {
            for (int i = 0; i < sc->num_entries; i++)
            {
                if (!sc->entries[i].enable)
                {
                    continue;
                }
                stream2_prim* const prim = sc->entries[i].prim;
                if ((prim->flags & 0x1) != 0 || !prim->pdef->flags.load_threaded)
                {
                    continue;
                }
                const bool split = (prim->flags & 0x4) != 0;
                if (!prim->pdef->load_cb(prim->name, (unsigned int)sc, prim->cpu_data, prim->cpu_size, split ? prim->gpu_data : nullptr, split ? prim->gpu_size : 0))
                {
                    sc->flags |= 0x40;
                    break;
                }
                prim->flags |= 0x1;
            }
            g_stream2ContainerLoaded = true;
        }
        if (thread.isQuitRequested())
        {
            return 0;
        }
    }
}

// 0x004AF760 (:93)
void stream2_container_create_loading_thread(void (*pInitFunction)())
{
    g_stream2ContainerLoadEvent.create("stream2Load", 0);
    g_stream2ContainerLoadThread.create("stream2Load", stream2_container_load, 0, 0x10000, 0);
    g_stream2ContainerLoadThread.start((void*)pInitFunction);
}

// 0x004AA420 (:100)
void stream2_container_destroy_loading_thread()
{
    g_stream2ContainerLoadThread.requestQuit();
    g_stream2ContainerLoadEvent.signal();
    g_stream2ContainerLoadThread.join(0xFFFFFFFF);
    g_stream2ContainerLoadThread.destroy();
    g_stream2ContainerLoadEvent.destroy();
}

// 0x004AA460 (:115)
stream2_container_def::stream2_container_def()
{
    load_cb = nullptr;
    unload_cb = nullptr;
    name = nullptr;
    allocator = nullptr;
    container_type = 0;
    mode = SCM_NONE;
}

// 0x004AA480 (:125)
bool stream2_container_def::valid()
{
    return allocator != nullptr;
}

// 0x004AA490 (:140)
void stream2_container::init(const char* _name, mempool_base* pool, stream2_container_def* _cdef, bool passive, bool force_no_stream_generation)
{
    cur_priority = 0x80;
    cur_category = 0;
    name = (char*)pool->alloc((unsigned int)strlen(_name) + 1, 0);
    strcpy(name, _name);
    flags = 0;
    if (passive)
    {
        flags = 0x300;
    }
    if (force_no_stream_generation)
    {
        flags |= 0x200;
    }
    cpu_allocated = nullptr;
    gpu_allocated = nullptr;
    last_error = nullptr;
    pf_base_offset = 0;
    pf_chain_read_size = nullptr;
    pf_chain_read_dest = nullptr;
    cdef = _cdef;
    ref_count = 0;
    pf_chain_size = 0;
}

// 0x004AD1D0 (:172)
void stream2_container::clear()
{
    if (ref_count == 0 && (flags & 0x7B) == 0)
    {
        num_entries = 0;
    }
}

// 0x004AA520 (:185)
bool stream2_container::set_passive(bool passive)
{
    if (passive)
    {
        flags |= 0x100;
    }
    else
    {
        flags &= ~0x100;
    }
    return true;
}

// 0x004AA550 (:220)
void stream2_container::abort_restart()
{
    flags |= 0x20;
}

// 0x004AF7A0 (:225)
void stream2_container::abort()
{
    if ((flags & 0x20) != 0)
    {
        flags &= ~0x20;
    }
    else if ((flags & 0x10) == 0)
    {
        flags |= 0x10;
        req_abort();
    }
}

// 0x004B0430 (:242)
bool stream2_container::abort_wait()
{
    if (g_pStream2ContainerToLoad == this)
    {
        if (!g_stream2ContainerLoaded)
        {
            return true;
        }
        g_pStream2ContainerToLoad = nullptr;
    }
    if (req_poll() == SOS_BUSY)
    {
        return true;
    }
    req_release();
    if ((flags & 0x20) == 0)
    {
        flags &= ~0x18;
        dec_ref_count();
        unload();
        return false;
    }
    // Requested again while aborting: start over without allocating.
    flags &= ~0x38;
    if (!stream(false))
    {
        flags = (short)((flags & ~0x8) | 0x40);
        return false;
    }
    return true;
}

// 0x004AF7C0 (:309)
bool stream2_container::stream(bool do_alloc)
{
    if (do_alloc && !alloc())
    {
        flags = (short)((flags & ~0x8) | 0x40);
        return false;
    }

    if ((flags & 0x80) == 0)
    {
        // One read per file.
        pf_chain_size = 0;
        for (int i = 0; i < num_entries; i++)
        {
            if (!entries[i].enable)
            {
                continue;
            }
            stream2_prim* const prim = entries[i].prim;
            if ((prim->flags & 0x1) != 0)
            {
                continue;
            }
            stream2_os_file cpu_file;
            if ((prim->flags & 0x4) == 0)
            {
                strcpy(cpu_file.filename, prim->name);
            }
            else
            {
                cf_add_ext(cpu_file.filename, sizeof(cpu_file.filename), prim->name, prim->pdef->cpu_ext[prim->split_ext_index], false);
            }
            cpu_file.offset = 0;
            cpu_file.size = (unsigned int)prim->cpu_size;
            cpu_file.dest = prim->cpu_data;
            cpu_file.priority = (unsigned short)cur_priority;
            cpu_file.category = (unsigned short)cur_category;
            pf_chain_read_dest[pf_chain_size] = (void*)stream2_os_read(cpu_file);
            pf_chain_size++;

            if ((prim->flags & 0x4) != 0 && prim->gpu_size > 0)
            {
                stream2_os_file gpu_file;
                cf_add_ext(gpu_file.filename, sizeof(gpu_file.filename), prim->name, prim->pdef->gpu_ext[prim->split_ext_index], false);
                gpu_file.size = (unsigned int)prim->gpu_size;
                gpu_file.offset = 0;
                gpu_file.priority = (unsigned short)cur_priority;
                if (!prim->pdef->flags.alloc_split)
                {
                    gpu_file.dest = prim->cpu_data + align_up(prim->cpu_size, prim->pdef->gpu_align);
                }
                else
                {
                    gpu_file.dest = prim->gpu_data;
                }
                pf_chain_read_dest[pf_chain_size] = (void*)stream2_os_read(gpu_file);
                pf_chain_size++;
            }
        }
    }
    else
    {
        // One chain read of <name>.str2_pc; slot 0 of pf_chain_read_dest holds the request handle.
        stream2_os_file file;
        sprintf(file.filename, "%s%s", name, ".str2_pc");
        file.offset = pf_base_offset;
        file.priority = (unsigned short)cur_priority;
        file.category = (unsigned short)cur_category;
        file.chain_read = true;
        file.size = (unsigned int)pf_total_compressed_read_size;
        file.chain_size = pf_chain_read_size;
        file.chain_dest = (unsigned char**)(pf_chain_read_dest + 1);
        file.chain_num_reads = 0;
        for (int i = 0; i < num_entries; i++)
        {
            if (!entries[i].enable)
            {
                continue;
            }
            stream2_prim* const prim = entries[i].prim;
            const bool loaded = (prim->flags & 0x1) != 0;
            file.chain_dest[file.chain_num_reads++] = loaded ? nullptr : prim->cpu_data;
            if ((prim->flags & 0x4) != 0 && prim->gpu_size > 0)
            {
                file.chain_dest[file.chain_num_reads++] = loaded ? nullptr : prim->gpu_data;
            }
        }
        pf_chain_read_dest[0] = (void*)stream2_os_read(file);
    }
    flags |= 0x8;
    return true;
}

// 0x004AFB20 (:431)
bool stream2_container::stream_wait()
{
    const stream2_os_req_status status = req_poll();
    if (status == SOS_BUSY)
    {
        return true;
    }
    if (status == SOS_DONE_ERROR)
    {
        req_abort();
    }
    if (g_pStream2ContainerToLoad == nullptr)
    {
        // Let the loading thread run the load_threaded callbacks first.
        g_stream2ContainerLoaded = false;
        g_pStream2ContainerToLoad = this;
        g_stream2ContainerLoadEvent.signal();
        return true;
    }
    if (g_pStream2ContainerToLoad == this)
    {
        if (g_stream2ContainerLoaded)
        {
            bool failed = false;
            for (int i = 0; i < num_entries; i++)
            {
                if (!entries[i].enable)
                {
                    continue;
                }
                stream2_prim* const prim = entries[i].prim;
                if ((prim->flags & 0x1) != 0 || prim->pdef->flags.load_threaded)
                {
                    continue;
                }
                bool loaded;
                if ((prim->flags & 0x4) == 0)
                {
                    loaded = prim->pdef->load_cb(prim->name, (unsigned int)this, prim->cpu_data, prim->cpu_size, nullptr, 0);
                }
                else
                {
                    loaded = prim->pdef->load_cb(prim->name, (unsigned int)this, prim->cpu_data, prim->cpu_size, prim->gpu_data, prim->gpu_size);
                }
                if (!loaded)
                {
                    failed = true;
                    break;
                }
                prim->flags |= 0x1;
            }
            if (!failed && cdef->load_cb != nullptr)
            {
                cdef->load_cb((unsigned int)this);
            }
            flags |= 0x1;
            req_release();
            if (status == SOS_DONE_ERROR)
            {
                flags |= 0x40;
            }
            flags &= ~0x8;
            g_pStream2ContainerToLoad = nullptr;
            return false;
        }
    }
    else if (g_stream2ContainerLoaded)
    {
        g_pStream2ContainerToLoad = nullptr;
    }
    return true;
}

// 0x004AD1F0 (:513)
stream2_os_req_status stream2_container::req_poll()
{
    if ((flags & 0x80) != 0)
    {
        return stream2_os_poll((unsigned int)pf_chain_read_dest[0]);
    }
    stream2_os_req_status result = SOS_DONE_SUCCESS;
    for (int i = 0; i < pf_chain_size; i++)
    {
        const stream2_os_req_status status = stream2_os_poll((unsigned int)pf_chain_read_dest[i]);
        if (status == SOS_INVALID)
        {
            VLIB_CRITICAL_FAILURE();
        }
        else if (status == SOS_BUSY)
        {
            if (result != SOS_DONE_ERROR)
            {
                result = SOS_BUSY;
            }
        }
        else if (status == SOS_DONE_ERROR)
        {
            result = SOS_DONE_ERROR;
        }
    }
    return result;
}

// 0x004AD2F0 (:556)
void stream2_container::req_abort()
{
    if ((flags & 0x80) == 0)
    {
        for (int i = 0; i < pf_chain_size; i++)
        {
            stream2_os_cancel((unsigned int)pf_chain_read_dest[i]);
        }
    }
    else
    {
        stream2_os_cancel((unsigned int)pf_chain_read_dest[0]);
    }
}

// 0x004AD340 (:580)
void stream2_container::req_release()
{
    if ((flags & 0x80) != 0)
    {
        stream2_os_release_handle((unsigned int)pf_chain_read_dest[0]);
        pf_chain_read_dest[0] = (void*)0xFFFFFFFF;
        return;
    }
    for (int i = 0; i < pf_chain_size; i++)
    {
        stream2_os_release_handle((unsigned int)pf_chain_read_dest[i]);
        pf_chain_read_dest[i] = (void*)0xFFFFFFFF;
    }
}

// 0x004B04B0 (:626)
bool stream2_container::add_prims(str_data_hash_table_cached<prim_add_entry,1024,unsigned short>* prims, mempool_base* pool, bool skip_validation)
{
    num_entries = (short)prims->get_count();
    entries = (entry*)pool->alloc((unsigned int)num_entries * sizeof(entry), 4);
    Stream2_container_allocated += num_entries * (int)sizeof(entry);
    pf_chain_size = 0;

    // Entries keep the order the primitives were added in (add_index).
    short chain_size = 0;
    const char* key;
    bool more = prims->get_first_key(key);
    while (more)
    {
        const prim_add_entry* const added = prims->get_ptr(key);
        if (added == nullptr)
        {
            break;
        }
        stream2_prim* const prim = added->prim;
        const unsigned char add_flags = prim->flags;
        prim->inc_container_ref_count();
        const bool enable = (add_flags & 0x8) == 0;
        prim->flags = add_flags & 0xC7;
        entry& e = entries[added->add_index];
        e.enable = enable ? 1 : 0;
        e.user0 = (add_flags >> 4) & 1;
        e.user1 = (add_flags >> 5) & 1;
        e.flags = 0;
        e.prim = prim;
        if (enable)
        {
            chain_size += ((prim->flags & 0x4) != 0 && prim->gpu_size > 0) ? 2 : 1;
        }
        more = prims->get_next_key(key, key);
    }
    pf_chain_size = chain_size;
    if ((flags & 0x100) == 0)
    {
        pf_chain_read_size = (unsigned int*)pool->alloc((unsigned int)chain_size * 4, 4);
        pf_chain_read_dest = (void**)pool->alloc((unsigned int)pf_chain_size * 4 + 4, 4);
        Stream2_container_allocated += pf_chain_size * 8;
    }
    return true;
}

// 0x004AD3A0 (:703)
bool stream2_container::enable_prim(const char* prim_name, bool enable)
{
    if (ref_count != 0 || (flags & 0x7B) != 0)
    {
        return false;
    }
    for (int i = 0; i < num_entries; i++)
    {
        if (v_stricmp(prim_name, entries[i].prim->name) == 0)
        {
            entries[i].enable = enable ? 1 : 0;
            return true;
        }
    }
    return false;
}

// 0x004AA570 (:736)
bool stream2_container::contains_prim(stream2_prim* prim) const
{
    for (int i = 0; i < num_entries; i++)
    {
        if (entries[i].prim == prim)
        {
            return true;
        }
    }
    return false;
}

// 0x004AD410 (:750)
bool stream2_container::contains_prim(const char* namev) const
{
    return get_prim(namev) != nullptr;
}

// 0x004AA5A0 (:755)
stream2_prim* stream2_container::get_prim(const char* namev)
{
    for (int i = 0; i < num_entries; i++)
    {
        if (v_stricmp(namev, entries[i].prim->name) == 0)
        {
            return entries[i].prim;
        }
    }
    return nullptr;
}

// 0x004AA5F0 (:769)
const stream2_prim* stream2_container::get_prim(const char* namev) const
{
    for (int i = 0; i < num_entries; i++)
    {
        if (v_stricmp(namev, entries[i].prim->name) == 0)
        {
            return entries[i].prim;
        }
    }
    return nullptr;
}

// 0x004AA640 (:784)
int stream2_container::num_prims() const
{
    return num_entries;
}

// 0x004AD430 (:790)
void stream2_container::inc_ref_count()
{
    ref_count++;
    for (int i = 0; i < num_entries; i++)
    {
        if (entries[i].enable)
        {
            entries[i].prim->inc_ref_count();
        }
    }
}

// 0x004AD460 (:807)
void stream2_container::dec_ref_count()
{
    ref_count--;
    for (int i = 0; i < num_entries; i++)
    {
        if (entries[i].enable)
        {
            entries[i].prim->dec_ref_count();
        }
    }
}

// 0x004AA650 (:826)
int stream2_container::get_ref_count() const
{
    return ref_count;
}

// 0x004AA660 (:832)
bool stream2_container::is_idle() const
{
    return ref_count == 0 && (flags & 0x7B) == 0;
}

// 0x004AA680 (:837)
static bool shouldPrimUseGPU(const stream2_prim* pPrim)
{
    bool use_gpu = pPrim->pdef->flags.alloc_split && (pPrim->flags & 0x4) != 0;
    if (pPrim->pdef->prim_type == 2)
    {
        use_gpu = false;
    }
    return use_gpu;
}

// 0x004AD490 (:850)
void stream2_container::calc_container_requirements(int& cpu_bytes_needed, int& cpu_align_needed, int& gpu_bytes_needed, int& gpu_align_needed) const
{
    cpu_align_needed = 1;
    cpu_bytes_needed = 0;
    gpu_align_needed = 1;
    gpu_bytes_needed = 0;
    for (int i = 0; i < num_entries; i++)
    {
        if (!entries[i].enable)
        {
            continue;
        }
        const stream2_prim* const prim = entries[i].prim;
        const stream2_prim_def* const pdef = prim->pdef;
        if (cpu_align_needed < pdef->cpu_align)
        {
            cpu_align_needed = pdef->cpu_align;
        }
        cpu_bytes_needed = align_up(cpu_bytes_needed, pdef->cpu_align) + prim->cpu_size;
        const bool use_gpu = pdef->flags.alloc_split && (prim->flags & 0x4) != 0;
        if (pdef->prim_type == 2 || !use_gpu)
        {
            if (prim->gpu_size > 0)
            {
                if (cpu_align_needed < pdef->gpu_align)
                {
                    cpu_align_needed = pdef->gpu_align;
                }
                cpu_bytes_needed = align_up(cpu_bytes_needed, pdef->gpu_align) + prim->gpu_size;
            }
        }
        else
        {
            if (gpu_align_needed < pdef->gpu_align)
            {
                gpu_align_needed = pdef->gpu_align;
            }
            gpu_bytes_needed = align_up(gpu_bytes_needed, pdef->gpu_align) + prim->gpu_size;
        }
    }
}

// 0x004AD5F0 (:893)
void stream2_container::calc_container_requirements(int& cpu_bytes_needed, int& cpu_align_needed, int& gpu_bytes_needed, int& gpu_align_needed, int& audio_bytes_needed, int& audio_align_needed) const
{
    cpu_align_needed = 1;
    cpu_bytes_needed = 0;
    gpu_align_needed = 1;
    gpu_bytes_needed = 0;
    audio_bytes_needed = 0;
    audio_align_needed = 0x10;
    for (int i = 0; i < num_entries; i++)
    {
        if (!entries[i].enable)
        {
            continue;
        }
        const stream2_prim* const prim = entries[i].prim;
        const stream2_prim_def* const pdef = prim->pdef;
        if (cpu_align_needed < pdef->cpu_align)
        {
            cpu_align_needed = pdef->cpu_align;
        }
        cpu_bytes_needed = align_up(cpu_bytes_needed, pdef->cpu_align) + prim->cpu_size;
        if (strcmp(prim->name, "vehicle") != 0 && strstr(prim->name, ".xwb_pc") != nullptr)
        {
            // Wavebanks go to the audio block.
            audio_bytes_needed = align_up(audio_bytes_needed, audio_align_needed) + prim->cpu_size;
            continue;
        }
        if (!pdef->flags.alloc_split || (prim->flags & 0x4) == 0)
        {
            if (prim->gpu_size > 0)
            {
                if (cpu_align_needed < pdef->gpu_align)
                {
                    cpu_align_needed = pdef->gpu_align;
                }
                cpu_bytes_needed = align_up(cpu_bytes_needed, pdef->gpu_align) + prim->gpu_size;
            }
        }
        else
        {
            if (gpu_align_needed < pdef->gpu_align)
            {
                gpu_align_needed = pdef->gpu_align;
            }
            gpu_bytes_needed = align_up(gpu_bytes_needed, pdef->gpu_align) + prim->gpu_size;
        }
    }
}

// 0x004AD7D0 (:949)
bool stream2_container::alloc()
{
    unsigned char* gpu_block = s_d3dMemoryBlock.pStart;
    if (cdef->mode != SCM_CONTAINER)
    {
        if (cdef->mode != SCM_PRIM)
        {
            VLIB_CRITICAL_FAILURE();
            return false;
        }
        for (int i = 0; i < num_entries; i++)
        {
            if (!entries[i].enable)
            {
                continue;
            }
            stream2_prim* const prim = entries[i].prim;
            if (prim->cpu_data != nullptr)
            {
                continue;
            }
            const stream2_prim_def* const pdef = prim->pdef;
            const bool use_gpu = shouldPrimUseGPU(prim);
            int cpu_bytes = prim->cpu_size;
            int gpu_bytes = prim->gpu_size;
            if (pdef->flags.alloc_split && !use_gpu)
            {
                cpu_bytes += gpu_bytes;
                gpu_bytes = 0;
            }
            stream2_allocator* const allocator = prim->allocator_idx != 0 ? _Stream_allocators[prim->allocator_idx] : cdef->allocator;
            allocator->alloc(cpu_bytes, pdef->cpu_align, prim->cpu_data, 0, pdef->gpu_align, prim->gpu_data, prim->name);
            int gpu_needed = 0;
            if (gpu_bytes != 0)
            {
                prim->gpu_data = gpu_block;
                gpu_block += (gpu_bytes + 0xF) & ~0xF;
                gpu_needed = gpu_bytes;
            }
            if (prim->cpu_data == nullptr)
            {
                return false;
            }
            if (gpu_needed > 0 && prim->gpu_data == nullptr)
            {
                allocator->free(prim->cpu_data);
                prim->cpu_data = nullptr;
                return false;
            }
            if (!use_gpu && prim->gpu_size > 0)
            {
                unsigned char* const after_cpu = prim->cpu_data + prim->cpu_size;
                const unsigned int rest = (unsigned int)after_cpu % (unsigned int)pdef->gpu_align;
                prim->gpu_data = after_cpu + (rest == 0 ? 0 : pdef->gpu_align - rest);
            }
        }
        return true;
    }

    int cpu_bytes = 0;
    int cpu_align = 1;
    int gpu_bytes = 0;
    int gpu_align = 1;
    unsigned char* audio_data = nullptr;
    const bool vehicle = strcmp(cdef->name, "Vehicle") == 0;
    if (vehicle)
    {
        int audio_bytes = 0;
        int audio_align = 1;
        calc_container_requirements(cpu_bytes, cpu_align, gpu_bytes, gpu_align, audio_bytes, audio_align);
        if (audio_bytes > 0)
        {
            unsigned char* unused = nullptr;
            _Stream_allocators[0x14]->alloc(audio_bytes, audio_align, audio_data, 0, 0, unused, entries[num_entries - 1].prim->name);
            if (audio_data == nullptr)
            {
                keen::debug::nativePrintArgs("[Stream2] <alloc> Could not alloc vehicle audio mem! \n");
            }
        }
    }
    else
    {
        calc_container_requirements(cpu_bytes, cpu_align, gpu_bytes, gpu_align);
    }

    stream2_allocator* const allocator = cdef->allocator;
    allocator->alloc(cpu_bytes, cpu_align, cpu_allocated, 0, gpu_align, gpu_allocated, name);
    int gpu_needed = 0;
    if (gpu_bytes != 0)
    {
        gpu_allocated = gpu_block;
        gpu_needed = gpu_bytes;
    }
    if (cpu_allocated == nullptr)
    {
        return false;
    }
    if (gpu_needed >= 1 && gpu_allocated == nullptr)
    {
        allocator->free(cpu_allocated);
        keen::debug::nativePrintArgs("allocated cpu memory, but couldn't allocate gpu memory");
        return false;
    }

    int cpu_offset = 0;
    int gpu_offset = 0;
    int audio_offset = 0;
    for (int i = 0; i < num_entries; i++)
    {
        if (!entries[i].enable)
        {
            continue;
        }
        stream2_prim* const prim = entries[i].prim;
        if (vehicle && strstr(prim->name, ".xwb_pc") != nullptr)
        {
            prim->cpu_data = audio_data + audio_offset;
            audio_offset += prim->cpu_size;
            continue;
        }
        if (prim->cpu_data != nullptr || prim->gpu_data != nullptr || prim->get_ref_count() != ref_count)
        {
            return false;
        }
        const stream2_prim_def* const pdef = prim->pdef;
        cpu_offset = align_up(cpu_offset, pdef->cpu_align);
        prim->cpu_data = cpu_allocated + cpu_offset;
        cpu_offset += prim->cpu_size;
        if (shouldPrimUseGPU(prim))
        {
            gpu_offset = align_up(gpu_offset, pdef->gpu_align);
            prim->gpu_data = gpu_allocated + gpu_offset;
            gpu_offset += prim->gpu_size;
        }
        else if (prim->gpu_size < 1)
        {
            prim->gpu_data = nullptr;
        }
        else
        {
            cpu_offset = align_up(cpu_offset, pdef->gpu_align);
            prim->gpu_data = cpu_allocated + cpu_offset;
            cpu_offset += prim->gpu_size;
        }
    }
    return true;
}

// 0x004AA6A0 (:1205)
bool stream2_container::is_allocated() const
{
    return cpu_allocated != nullptr;
}

// 0x004A9B90 (.h:141)
stream2_container_mode stream2_container::get_mode() const
{
    return cdef->mode;
}

// 0x004ADC70 (:1211)
bool stream2_container::load(bool fromLoadThread)
{
    for (int i = 0; i < num_entries; i++)
    {
        if (!entries[i].enable)
        {
            continue;
        }
        stream2_prim* const prim = entries[i].prim;
        if ((prim->flags & 0x1) != 0)
        {
            continue;
        }
        const bool threaded = prim->pdef->flags.load_threaded != 0;
        if (threaded != fromLoadThread)
        {
            continue;
        }
        const bool split = (prim->flags & 0x4) != 0;
        if (!prim->pdef->load_cb(prim->name, (unsigned int)this, prim->cpu_data, prim->cpu_size, split ? prim->gpu_data : nullptr, split ? prim->gpu_size : 0))
        {
            return false;
        }
        prim->flags |= 0x1;
    }
    if (!fromLoadThread && cdef->load_cb != nullptr)
    {
        cdef->load_cb((unsigned int)this);
    }
    return true;
}

// 0x004ADD20 (:1277)
void stream2_container::unload()
{
    if ((flags & 0x1) != 0 && cdef->unload_cb != nullptr)
    {
        cdef->unload_cb((unsigned int)this);
    }
    for (int i = 0; i < num_entries; i++)
    {
        if (!entries[i].enable)
        {
            continue;
        }
        stream2_prim* const prim = entries[i].prim;
        if (cdef->mode == SCM_PRIM)
        {
            // Shared primitives stay until no container references them.
            if (prim->get_ref_count() >= 1)
            {
                continue;
            }
            if ((prim->flags & 0x1) != 0)
            {
                prim->pdef->unload_cb(prim->name, (unsigned int)this);
                prim->flags &= ~0x1;
            }
            if (prim->cpu_data == nullptr)
            {
                continue;
            }
            prim->gpu_data = nullptr;
            stream2_allocator* const allocator = prim->allocator_idx != 0 ? _Stream_allocators[prim->allocator_idx] : cdef->allocator;
            allocator->free(prim->cpu_data, nullptr);
            prim->gpu_data = nullptr;
            prim->cpu_data = nullptr;
        }
        else if (cdef->mode == SCM_CONTAINER)
        {
            if ((prim->flags & 0x1) != 0)
            {
                prim->pdef->unload_cb(prim->name, (unsigned int)this);
                prim->flags &= ~0x1;
            }
            if (prim->cpu_data != nullptr && strcmp(cdef->name, "Vehicle") == 0 && strstr(prim->name, ".xwb_pc") != nullptr && _Stream_allocators[0x14] != nullptr)
            {
                _Stream_allocators[0x14]->set_thread_ownership();
                _Stream_allocators[0x14]->free(prim->cpu_data, nullptr);
            }
            prim->gpu_data = nullptr;
            prim->cpu_data = nullptr;
        }
    }
    if (cdef->mode == SCM_CONTAINER && cpu_allocated != nullptr)
    {
        cdef->allocator->free(cpu_allocated, gpu_allocated);
        cpu_allocated = nullptr;
        gpu_allocated = nullptr;
    }
    flags &= ~0x41;
}

// 0x004ADEB0 (:1373)
void stream2_container::get_structure_size(unsigned int& cpu_size, bool count_prim_dups) const
{
    // The container, its entries and chain arrays, and its primitives (shared ones once unless count_prim_dups).
    cpu_size = (unsigned int)align_up((int)(cpu_size + sizeof(stream2_container)), 4);
    cpu_size = (unsigned int)align_up((int)(cpu_size + num_entries * sizeof(entry)), 4);
    if ((flags & 0x100) == 0)
    {
        cpu_size = (unsigned int)align_up((int)(cpu_size + pf_chain_size * 4), 4);
        cpu_size = (unsigned int)align_up((int)(cpu_size + 4 + pf_chain_size * 4), 4);
    }
    if (cdef->mode == SCM_CONTAINER)
    {
        cpu_size = (unsigned int)align_up((int)(cpu_size + num_entries * sizeof(stream2_prim)), 4);
        return;
    }
    for (int i = 0; i < num_entries; i++)
    {
        if (entries[i].enable && (entries[i].prim->get_container_ref_count() < 2 || count_prim_dups))
        {
            cpu_size = (unsigned int)align_up((int)(cpu_size + sizeof(stream2_prim)), 4);
        }
    }
}

// 0x004AA6B0 (:1413)
void stream2_container::get_stream_data_filename(vlib_platform plat, char* filename) const
{
    sprintf(filename, "%s%s", name, ".str2_pc");
}

// 0x004AA560 (:603)
void stream2_container::signal_last_error(const char*, ...)
{
}

// 0x004B1180 (:1749)
bool stream2_container::load_container(cfile* asm_file, mempool_base* pool, bool report_errors, bool is_temp, bool patch_data)
{
    // Container record: name, type, flags, primitive count, chain base offset, chain size count, compressed size,
    // then the chain read sizes and the primitive records.
    char container_name[64];
    cf_read_string(container_name, sizeof(container_name), asm_file);
    const unsigned char type = cf_read_uint8(asm_file);
    const unsigned short container_flags = (unsigned short)cf_read_int16(asm_file);
    const short num_prims = cf_read_int16(asm_file);
    const unsigned int base_offset = cf_read_uint32(asm_file);
    const int num_chain_sizes = cf_read_int32(asm_file);
    const int compressed_size = cf_read_int32(asm_file);
    const unsigned int chain_sizes_pos = cf_tell(asm_file);
    if (num_chain_sizes > 0)
    {
        cf_seek(asm_file, num_chain_sizes * 4, CF_SEEK_CUR);
    }
    stream2_container_start(container_name, pool, type, ((container_flags >> 8) & 1) != 0, false);
    for (int i = 0; i < num_prims; i++)
    {
        if (!stream2_prim::load_prim(asm_file, pool, report_errors))
        {
            return false;
        }
    }
    const unsigned int srid = stream2_container_finish(true, patch_data);
    if (is_temp && srid != 0)
    {
        dlc_store_temp_srid(srid);
    }
    if (num_chain_sizes > 0)
    {
        const unsigned int end_pos = cf_tell(asm_file);
        cf_seek(asm_file, (int)chain_sizes_pos, CF_SEEK_SET);
        if (srid != 0)
        {
            stream2_container* const sc = (stream2_container*)srid;
            if ((sc->flags & 0x100) == 0)
            {
                for (int i = 0; i < num_chain_sizes; i++)
                {
                    sc->pf_chain_read_size[i] = cf_read_uint32(asm_file);
                }
            }
            sc->pf_base_offset = base_offset;
            sc->pf_total_compressed_read_size = compressed_size;
            if ((container_flags & 0x80) != 0)
            {
                sc->flags |= 0x80;
            }
        }
        cf_seek(asm_file, (int)end_pos, CF_SEEK_SET);
    }
    return srid != 0;
}
