// Original: volition/rfg/code/geom/foliage_mesh.cpp
//
// The foliage meshes (lod.md L2): stream2 primitives (GSP_FOLIAGE_MESH, .cfmesh_pc) in the territory's cell
// containers. A loaded mesh is recorded by the hash of its file name (str_hash_string of "name.cfmesh_pc"), which the
// undergrowth looks it up by (foliage_mesh_find adds the extension).

#include "volition/rfg/code/geom/foliage_mesh.h"

#include "volition/rfg/code/geom/foliage_mesh_shared.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// The loaded foliage meshes by the hash of their file name.
static data_hash_table<unsigned int,foliage_mesh*,&hash_uint,64,unsigned char> Foliage_mesh_filename_hash(false); // 0x0165F450
static bool Foliage_system_init; // 0x025EF8DA

// 0x007F2F20 (:38)
static unsigned int create_foliage_mesh_key(const char* filename)
{
    return str_hash_string(filename);
}

// 0x0080B210 (:52)
// foliage_mesh_load inlined. Notes (decompiled behaviour, kept as is): the failure message goes to a per-thread
// buffer; a second mesh with the same name is not recorded (its unload finds the first).
bool foliage_mesh_stream_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    foliage_mesh* fmesh = foliage_mesh_load(filename, cpu_data, cpu_data_size, gpu_data, gpu_data_size);
    if (fmesh == nullptr)
    {
        char message[256];
        sprintf(message, "Failed to load rl data for %s", filename);
        keen::debug::nativePrintArgs(message);
        return false;
    }
    Foliage_mesh_filename_hash.insert_key_data(create_foliage_mesh_key(filename), fmesh);
    return true;
}

// 0x00808150 (:73)
void foliage_mesh_stream_unload(const char* filename, unsigned int srid)
{
    foliage_mesh* fmesh = nullptr;
    Foliage_mesh_filename_hash.get(create_foliage_mesh_key(filename), fmesh);
    if (fmesh != nullptr)
    {
        foliage_mesh_unload(fmesh);
        Foliage_mesh_filename_hash.remove(create_foliage_mesh_key(filename));
    }
}

// 0x007F2F30 (:91)
void foliage_mesh_system_init()
{
    if (!Foliage_system_init)
    {
        Foliage_system_init = true;
    }
}

// 0x007F2F50 (:100)
void foliage_mesh_system_shutdown()
{
    if (Foliage_system_init)
    {
        Foliage_system_init = false;
    }
}

// 0x008081A0 (:109)
// The loaded mesh of that name (with or without the extension), or null.
foliage_mesh* foliage_mesh_find(const char* filename)
{
    char name[64];
    cf_add_ext(name, sizeof(name), filename, ".cfmesh_pc", false);
    foliage_mesh* fmesh = nullptr;
    Foliage_mesh_filename_hash.get(create_foliage_mesh_key(name), fmesh);
    return fmesh;
}

// 0x007F2F70 (:120)
// Empty.
void foliage_mesh_level_shutdown()
{
}
