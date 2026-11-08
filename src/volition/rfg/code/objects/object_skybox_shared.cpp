// Original: volition/rfg/code/objects/object_skybox_shared.cpp
//
// The sky's container definitions (shared with the tools) and horizon_mountain_parameters.

#include "volition/rfg/code/objects/object_skybox_shared.h"

#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"

// 0x00A1C380
horizon_mountain_parameters::horizon_mountain_parameters()
{
}

// 0x00A1C440 (:6): the two sky containers (preloaded, type GSC_SKYBOX), each holding its
// static mesh.
void object_skybox_load_shared(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat)
{
    static const char* const mesh_extensions[] = { ".csmesh_pc", ".csmesh_pc", ".csmesh_pc", ".csmesh_pc", ".csmesh_pc", ".csmesh_pc", ".csmesh_pc", ".csmesh_pc" };
    assasm_start("skybox_containers", tmp_pool, pool, load_from_preload, nullptr, false, false, true, false);
    const char* ext = mesh_extensions[plat];
    char name[64];
    cf_add_ext(name, sizeof(name), "skybox_clouds", ext, false);
    assasm_container_start(name, 28, true, false); // 28: GSC_SKYBOX
    assasm_container_add(name, 5, "skybox", true, false, false, nullptr, 0); // 5: GSP_STATIC_MESH
    assasm_container_finish(false, false, false);
    cf_add_ext(name, sizeof(name), "rfg_skybox", ext, false);
    assasm_container_start(name, 28, true, false);
    assasm_container_add(name, 5, "skybox", true, false, false, nullptr, 0);
    assasm_container_finish(false, false, false);
    assasm_finish(nullptr, false, true, false);
}
