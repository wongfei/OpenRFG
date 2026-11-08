// Original: volition/rfg/code/geom/decal_info.cpp
//
// A split TU (deviation, VFX.md FX7, as effects.cpp in FX3): rfg compiles the table part (decal_info_system_init and
// the name lookups); rfg_effects compiles it again with OPENRFG_EFFECTS for the rest (the decal materials found per
// container, the renderer parameter getters decal_info_get*, the release of the preloaded containers, shutdown and the
// reload command). The other programs keep their stand-ins for the second part.

#include "volition/rfg/code/geom/decal_info.h"

#if !defined(OPENRFG_EFFECTS)
#include "volition/rfg/code/geom/decal_info_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/bmpman/peg_assemble.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"

#include <stdio.h>

decal_info Decal_info[32]; // 0x025DEDC0
int Num_decal_infos; // 0x025EF8DC
str_data_hash_table<decal_info*, 32, 64, unsigned char> Decal_info_name_hash; // 0x025F5FD8

// ---- file-static globals (external: decal_info_system_shutdown in the OPENRFG_EFFECTS part reads it)
bool Decal_system_init; // 0x025EF8D9

// 0x0080BE70 (:122)
void decal_info_system_init()
{
    // Parses the table and loads the decal peg: the one just built, or the prebuilt "decal_containers.cpeg_pc".
    if (Decal_system_init)
    {
        return;
    }
    Decal_system_init = true;
    pasm_data peg_data;
    decal_info_parse_table_shared(&peg_data, &Common_mempool, &Decal_preload_mempool, &Decal_preload_mempool, PLATFORM_PC);
    if (peg_data.cpu_data != nullptr)
    {
        bm_load_peg("decal_containers", peg_data.cpu_data, (unsigned int)peg_data.cpu_size, peg_data.gpu_data, (unsigned int)peg_data.gpu_size, true);
        return;
    }
    char filename[64];
    sprintf(filename, "%s\\%s", "/gamebuild", "decal_containers");
    cf_add_ext(filename, sizeof(filename), "decal_containers", ".cpeg_pc", false);
    bm_load_peg(filename, &Decal_preload_mempool, true);
}

// 0x00807FF0 (:200)
int decal_info_find(const char* decal_info_name)
{
    // Index of the decal in Decal_info, -1 if unknown.
    decal_info* info;
    if (decal_info_name != nullptr && Decal_info_name_hash.get(decal_info_name, info) && info != nullptr)
    {
        return (int)(info - Decal_info);
    }
    return -1;
}

// 0x00808050 (:215)
int decal_info_find_by_hash(unsigned int decal_info_name_hash)
{
    // Walks the registered names for one whose crc_stri matches.
    const char* key;
    if (!Decal_info_name_hash.get_first_key(key))
    {
        return -1;
    }
    while (crc_stri(key, 0) != decal_info_name_hash)
    {
        if (!Decal_info_name_hash.get_next_key(key, key))
        {
            return -1;
        }
    }
    return decal_info_find(key);
}

#else // OPENRFG_EFFECTS

#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/os/vconsole.h"

// The decal_info.cpp file static, shared with the table part (one unity TU in rfg.exe).
extern bool Decal_system_init; // 0x025EF8D9

// unused_dcf_decal_info_reload (0x007F2EA0, a console test running "reload_decal_infos") is not ported: nothing
// references it.

// 0x007F2EB0 (:77)
// The preloaded decal containers released (flag 1 cleared).
static void decal_info_release_preloaded()
{
    for (int i = 0; i < Num_decal_infos; i++)
    {
        if ((Decal_info[i].flags & 1) != 0)
        {
            const unsigned int srid = stream2_container_find(Decal_info[i].name);
            if (srid != 0)
            {
                stream2_release(srid);
            }
            Decal_info[i].flags &= ~1;
        }
    }
}

// 0x0080BDC0 (:98)
// GSC_DECALS / GSC_DECALS_PRELOAD load: the container named after a decal holds its material (<material_name>.mat_pc),
// found in Material_filename_hash; flag 2.
void decal_info_load(unsigned int srid)
{
    const char* name = stream2_get_container_name(srid);
    decal_info* info = nullptr;
    if (!Decal_info_name_hash.get(name, info) || info == nullptr)
    {
        return;
    }
    // material_find, inlined (Material_filename_hash is rfg_material.cpp's file static).
    material* mat = material_find(info->material_name, srid);
    info->flags |= 2;
    info->mat = mat;
}

// 0x00807F90 (:109)
// Notes (decompiled behaviour, kept as is): every flag is cleared, the preloaded one too.
void decal_info_unload(unsigned int srid)
{
    const char* name = stream2_get_container_name(srid);
    decal_info* info = nullptr;
    if (!Decal_info_name_hash.get(name, info) || info == nullptr)
    {
        return;
    }
    info->mat = nullptr;
    info->flags = 0;
}

// 0x007F2F00 (:154)
void decal_info_system_shutdown()
{
    if (Decal_system_init)
    {
        Decal_system_init = false;
        decal_info_release_preloaded();
    }
}

// 0x0080AB60 (:165)
void decal_info_get(rl_decal_parameters* out_info, const char* decal_info_name)
{
    decal_info_get(out_info, decal_info_find(decal_info_name));
}

// 0x0080AB80 (:171)
void decal_info_get_by_hash(rl_decal_parameters* out_info, unsigned int decal_info_hash)
{
    decal_info_get(out_info, decal_info_find_by_hash(decal_info_hash));
}

// 0x007FBE00 (:177)
// A planar decal of the info: lifetime, fade, double sided (flag 4), box, the depth and slope fade ranges (start as
// the maximum, end as the minimum) and the first material of the decal material's map (-1 until it is loaded).
void decal_info_get(rl_decal_parameters* out_info, int decal_info_id)
{
    if (decal_info_id == -1 || out_info == nullptr)
    {
        return;
    }
    const decal_info& info = Decal_info[decal_info_id];
    out_info->m_type = RLDT_PLANAR;
    out_info->m_lifetime = info.life_time;
    out_info->m_fade_time = info.fade_time;
    out_info->m_double_sided = ((unsigned int)info.flags >> 2 & 1) != 0;
    out_info->m_planar_params.m_width = info.width;
    out_info->m_planar_params.m_length = info.length;
    out_info->m_planar_params.m_depth = info.depth;
    out_info->m_planar_params.m_max_fade_depth = info.depth_fade_start_pct;
    out_info->m_planar_params.m_min_fade_depth = info.depth_fade_end_pct;
    out_info->m_planar_params.m_max_fade_angle = info.slope_fade_start;
    out_info->m_planar_params.m_min_fade_angle = info.slope_fade_end;
    if (info.mat == nullptr)
    {
        out_info->m_planar_params.m_material_handle = 0xFFFFFFFF;
        return;
    }
    out_info->m_planar_params.m_material_handle = info.mat->m_material_map->get_material(0);
}

#endif // OPENRFG_EFFECTS
