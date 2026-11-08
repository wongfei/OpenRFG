// Original: volition/rfg/code/geom/static_mesh.cpp
//
// Static mesh streaming: the GSP_STATIC_MESH callbacks load and unload the meshes (static_mesh_shared.cpp) and keep
// them by (file name hash, srid) for static_mesh_find. A mesh of a container that is not a whole-container stream
// (mode SCM_CONTAINER) is keyed with srid 0.

#include "volition/rfg/code/geom/static_mesh.h"

#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// ---- file-static globals
static bool Static_mesh_init; // 0x025EF8E4
static data_hash_table<uint64_t,static_mesh*,&hash_uint64,256,unsigned short> Static_mesh_filename_hash(false); // 0x02615BD0

// ---- internal functions
// 0x007F4850 (:38, LTCG: srid in EAX, filename in ECX)
static uint64_t create_static_mesh_key(const char* filename, unsigned int srid)
{
    if (srid != 0 && ((stream2_container*)srid)->cdef->mode != SCM_CONTAINER)
    {
        srid = 0;
    }
    return (uint64_t)str_hash_string(filename) << 32 | srid;
}

// ---- functions
// 0x0080B620 (:61)
bool static_mesh_stream_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    static_mesh* smesh = static_mesh_load(filename, cpu_data, cpu_data_size, gpu_data, gpu_data_size);
    if (smesh == nullptr)
    {
        // The original formats into a thread-local buffer.
        char message[256];
        sprintf(message, "Failed to load rl data for %s", filename);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    Static_mesh_filename_hash.insert_key_data(create_static_mesh_key(filename, srid), smesh);
    return true;
}

// 0x00809340 (:82)
void static_mesh_stream_unload(const char* filename, unsigned int srid)
{
    const uint64_t key = create_static_mesh_key(filename, srid);
    static_mesh** smesh_p = Static_mesh_filename_hash.get_ptr(key);
    if (smesh_p != nullptr && *smesh_p != nullptr)
    {
        static_mesh_unload(*smesh_p);
        Static_mesh_filename_hash.remove(key);
    }
}

// 0x007F4880 (:98)
void static_mesh_system_init()
{
    if (!Static_mesh_init)
    {
        Static_mesh_init = true;
    }
}

// 0x007F48A0 (:107)
void static_mesh_system_shutdown()
{
    if (Static_mesh_init)
    {
        Static_mesh_init = false;
    }
}

// 0x008093E0 (:116): by file name (with or without the .csmesh_pc extension).
static_mesh* static_mesh_find(const char* filename, unsigned int srid)
{
    char name[64];
    cf_add_ext(name, sizeof(name), filename, ".csmesh_pc", false);
    static_mesh** smesh_p = Static_mesh_filename_hash.get_ptr(create_static_mesh_key(name, srid));
    return smesh_p != nullptr ? *smesh_p : nullptr;
}

// 0x007F48C0 (:127)
void static_mesh_level_shutdown()
{
}
