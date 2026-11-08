// Original: volition/stream2/code/stream2_prim.cpp
//
// stream2_prim::load runs the type's load callback only on the matching thread: load_threaded types on the stream2
// loading thread (fromLoadThread), the others on the owning thread. A failed callback leaves the primitive unloaded.
// load_prim reads one primitive record of a container assembly file (.asm_pc) and adds it to the container being
// built with its precalculated sizes.

#include "volition/stream2/code/stream2_prim.h"

#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"

// 0x004AB460 (:20)
stream2_prim_def::stream2_prim_def()
{
    flags.debug_skippable = 0;
    flags.alloc_split = 0;
    load_cb = nullptr;
    unload_cb = nullptr;
    cpu_align = -1;
    gpu_align = -1;
    name = nullptr;
    total_allocated = 0;
}

// 0x004AB480 (:33)
bool stream2_prim_def::valid()
{
    return load_cb != nullptr;
}

// 0x004AB490 (:45)
void stream2_prim::init()
{
    name = nullptr;
    pdef = nullptr;
    cpu_data = nullptr;
    gpu_data = nullptr;
    cpu_size = 0;
    gpu_size = 0;
    split_ext_index = 0;
    flags = 0;
    ref_count = 0;
    container_ref_count = 0;
    allocator_idx = 0;
}

// 0x004AB4B0 (:64)
void stream2_prim::inc_ref_count()
{
    ref_count++;
}

// 0x004AB4C0 (:69)
void stream2_prim::dec_ref_count()
{
    ref_count--;
}

// 0x004AB4D0 (:74)
int stream2_prim::get_ref_count()
{
    return ref_count;
}

// 0x004AB4E0 (:79)
void stream2_prim::inc_container_ref_count()
{
    container_ref_count++;
}

// 0x004AB4F0 (:84)
void stream2_prim::dec_container_ref_count()
{
    container_ref_count--;
}

// 0x004AB500 (:89)
int stream2_prim::get_container_ref_count()
{
    return container_ref_count;
}

// 0x004AB510 (:94)
bool stream2_prim::load(unsigned int srid, bool fromLoadThread)
{
    const bool threaded = pdef->flags.load_threaded != 0;
    if (threaded == fromLoadThread)
    {
        bool loaded;
        if ((flags & 0x4) == 0)
        {
            loaded = pdef->load_cb(name, srid, cpu_data, cpu_size, nullptr, 0);
        }
        else
        {
            loaded = pdef->load_cb(name, srid, cpu_data, cpu_size, gpu_data, gpu_size);
        }
        if (!loaded)
        {
            return false;
        }
        flags |= 0x1;
    }
    return true;
}

// 0x004AB590 (:121)
void stream2_prim::unload(unsigned int srid)
{
    pdef->unload_cb(name, srid);
    flags &= ~0x1;
}

// 0x004AB5B0 (:135)
bool stream2_prim::loaded()
{
    return (flags & 0x1) != 0;
}

// 0x004B07D0 (:232)
bool stream2_prim::load_prim(cfile* asm_file, mempool_base* pool, bool report_errors)
{
    stream2_prim_record record;
    cf_read_string(record.name, sizeof(record.name), asm_file);
    record.type = cf_read_uint8(asm_file);
    record.allocator = cf_read_uint8(asm_file);
    record.flags = cf_read_uint8(asm_file);
    record.split_ext_index = cf_read_uint8(asm_file);
    record.cpu_size = cf_read_int32(asm_file);
    record.gpu_size = cf_read_int32(asm_file);
    return stream2_container_add_prim(record.name, "", record.type, record.allocator, report_errors, record.flags, &record) != nullptr;
}
