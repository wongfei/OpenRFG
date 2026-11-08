// Original: volition/rfg/code/hud/in_world_gps_shared.cpp
//
// The stream container of the GPS chevron mesh.

#include "volition/rfg/code/hud/in_world_gps_shared.h"

#include "volition/rfg/code/geom/foliage_mesh.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

// ---- file-static globals
static unsigned int In_world_gps_chevron_srid; // 0x02B9DA2C
static bool In_world_gps_shared_init; // 0x02B9DD37

// 0x0084C170 (:72)
void in_world_gps_generate_stream_data(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat)
{
    // Magic number: one extension per platform; in this build every entry is the PC one.
    static const char* const extensions[8] = {".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc", ".cfmesh_pc"};
    char filename[64];
    assasm_start("In_world_gps_preload", tmp_pool, pool, Use_packfiles, nullptr, false, false, true, false);
    assasm_container_start("in_world_gps_chevron", '&', true, false); // Magic number: the container type the asm passes.
    cf_add_ext(filename, 0x40, "in_world_gps_chevron", extensions[plat], false);
    assasm_container_add(filename, 7, "foliage mesh", true, false, false, nullptr, 0); // Magic number: the primitive type.
    In_world_gps_chevron_srid = assasm_container_finish(false, false, false);
    assasm_finish(nullptr, false, true, false);
}

// 0x0084C250 (:100)
void in_world_gps_shared_system_init()
{
    // Original bug, kept: this tests the flag it sets, so the flag stays false and the body never runs.
    if (In_world_gps_shared_init)
    {
        In_world_gps_shared_init = true;
        in_world_gps_generate_stream_data(&Common_mempool, &Permanent_mempool, PLATFORM_PC);
    }
}

// 0x0084C280 (:113)
void in_world_gps_shared_system_shutdown()
{
    if (In_world_gps_shared_init)
    {
        In_world_gps_shared_init = false;
        stream2_release(In_world_gps_chevron_srid);
        stream2_container_delete(In_world_gps_chevron_srid);
    }
}

// 0x0084C2B0 (:126)
foliage_mesh* in_world_gps_get_chevron_fmesh()
{
    foliage_mesh* mesh = foliage_mesh_find("in_world_gps_chevron");
    if (mesh == nullptr)
    {
        stream2_preload(In_world_gps_chevron_srid);
        mesh = foliage_mesh_find("in_world_gps_chevron");
    }
    return mesh;
}
