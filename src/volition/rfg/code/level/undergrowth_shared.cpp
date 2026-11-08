// Original: volition/rfg/code/level/undergrowth_shared.cpp
//
// The foliage containers' definitions (shared with the tools, lod.md L2): a "foliage_containers" asset assembly with one
// passive container (GSC_FOLIAGE) per foliage mesh file found by the file system, each holding that mesh
// (GSP_FOLIAGE_MESH, category "undergrowth"). In the game the territories' cell containers carry the foliage meshes
// that the undergrowth draws; this assembly is not read from a preload file.

#include "volition/rfg/code/level/undergrowth_shared.h"

#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/game_stream2_prims.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <stdio.h>

// Magic number: FOLIAGE_MAX_FILES.
static const unsigned int FOLIAGE_MAX_FILES = 0x200;

// 0x0091BF40 (:6)
// Notes (decompiled behaviour, kept as is): the "too many files" message is formatted but not printed, and the files
// beyond the maximum are still listed (the list holds one more).
void undergrowth_load_shared(mempool_base* tmp_pool, mempool* pool, vlib_platform plat)
{
    static const char* const mesh_extensions[] = { ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc" };
    assasm_start("foliage_containers", tmp_pool, (mempool_base*)pool, false, nullptr, false, false, true, false);
    mempool_mark_scope mark(tmp_pool);
    const char** files = (const char**)tmp_pool->alloc((FOLIAGE_MAX_FILES + 1) * sizeof(const char*), 0);
    const char* ext = mesh_extensions[plat];
    const int num_files = (int)cf_find_ext(files, FOLIAGE_MAX_FILES + 1, ext);
    if (num_files > (int)FOLIAGE_MAX_FILES)
    {
        char message[512];
        sprintf(message, "Too many undergrowth files found! Need to increase FOLIAGE_MAX_FILES above %d\n", FOLIAGE_MAX_FILES);
    }
    for (int i = 0; i < num_files; i++)
    {
        assasm_container_start(files[i], GSC_FOLIAGE, false, true);
        char name[64];
        cf_add_ext(name, sizeof(name), files[i], ext, false);
        assasm_container_add(name, GSP_FOLIAGE_MESH, "undergrowth", false, true, false, nullptr, 0);
        assasm_container_finish(false, false, false);
    }
    assasm_finish(nullptr, false, true, false);
}
