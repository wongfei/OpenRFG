// Original: volition/rfg/code/code_effects/material_preload_shared.cpp
//
// The preloaded effect materials (VFX.md FX4 item 15): the x-ray and the stealth distortion materials, each in its own
// stream2 container built at start-up by the asset assembler.

#include "volition/rfg/code/code_effects/material_preload_shared.h"

#include "volition/rfg/code/code_effects/distortion_effect.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/code_effects/xray_effect.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

// ---- file-static globals
static bool Material_preload_shared_init; // 0x01DEE4C1

// The container type of the preloaded material containers (rfg.exe passes 0x23).
static const unsigned char MATERIAL_PRELOAD_CONTAINER_TYPE = 0x23;
// The stream2 primitive type of a material.
static const unsigned char MATERIAL_PRIM_TYPE = 8;

// 0x006E4A10 (:48)
void material_preload_generate_stream_data(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat)
{
    char filename[64];
    assasm_start("material_preload", tmp_pool, pool, Use_packfiles, nullptr, false, false, true, false);
    assasm_container_start("xray_effect", MATERIAL_PRELOAD_CONTAINER_TYPE, true, false);
    cf_add_ext(filename, sizeof(filename), "xray_effect", Mat_platform_ext[plat], false);
    assasm_container_add(filename, MATERIAL_PRIM_TYPE, "material effect", true, false, false, nullptr, 0);
    Xray_effect_mat_srid = assasm_container_finish(false, false, false);
    assasm_container_start("distortion_effect", MATERIAL_PRELOAD_CONTAINER_TYPE, true, false);
    cf_add_ext(filename, sizeof(filename), "vfx_stealth_dist_matfx", Mat_platform_ext[plat], false);
    assasm_container_add(filename, MATERIAL_PRIM_TYPE, "material effect", true, false, false, nullptr, 0);
    Distortion_effect_mat_srid = assasm_container_finish(false, false, false);
    assasm_finish(nullptr, false, true, false);
}

// 0x006E4B20 (:85)
void material_preload_shared_system_init()
{
    if (!Material_preload_shared_init)
    {
        Material_preload_shared_init = true;
        material_preload_generate_stream_data(&Common_mempool, &Permanent_mempool, PLATFORM_PC);
    }
}

// 0x006E4B50 (:98)
void material_preload_shared_system_shutdown()
{
    if (Material_preload_shared_init)
    {
        Material_preload_shared_init = false;
        stream2_release(Xray_effect_mat_srid);
        stream2_container_delete(Xray_effect_mat_srid);
        stream2_release(Distortion_effect_mat_srid);
        stream2_container_delete(Distortion_effect_mat_srid);
    }
}
