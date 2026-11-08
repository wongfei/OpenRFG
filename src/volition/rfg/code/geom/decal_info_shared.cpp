// Original: volition/rfg/code/geom/decal_info_shared.cpp
//
// Decal table (decal_info.xtbl, up to 32 decals): each decal is registered by name and gets its own asset container
// holding its material (".mat_pc"), all built into the "decal_containers" peg.

#include "volition/rfg/code/geom/decal_info_shared.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"

#include <math.h>

// 0x0080ABA0 (:27)
static void decal_parse(xml_element* cur_info, vlib_platform plat)
{
    static const char* const material_ext[NUM_VLIB_PLATFORMS] = { "mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc" };
    decal_info* info = &Decal_info[Num_decal_infos];
    xml_table_get_req_string(info->name, 0x40, cur_info, "Name");
    Decal_info_name_hash.insert_key_data(info->name, info);
    info->material_name[0] = '\0';
    info->mat = nullptr;
    info->flags = 0;
    char material[64];
    xml_table_get_req_filename(material, sizeof(material), cur_info, "Material");
    cf_add_ext(info->material_name, 0x40, material, material_ext[plat], false);
    bool preload = false;
    xml_table_get_req_bool(preload, cur_info, "Preload");
    bool double_sided = false;
    xml_table_get_req_bool(double_sided, cur_info, "Double_sided");
    if (double_sided)
    {
        info->flags |= 4;
    }
    assasm_container_start(info->name, (unsigned char)(preload ? 11 : 10), preload, false);
    assasm_container_add(info->material_name, 8, "decals", !preload, preload, false, nullptr, 0);
    assasm_container_finish(false, false, false);
    if (preload)
    {
        info->flags |= 1;
    }
    xml_table_get_req_int(info->life_time, cur_info, "Life_Time");
    xml_table_get_req_int(info->fade_time, cur_info, "Fade_Time");
    xml_table_get_req_float(info->width, cur_info, "Width");
    xml_table_get_req_float(info->length, cur_info, "Length");
    xml_table_get_req_float(info->depth, cur_info, "Depth");
    xml_table_get_req_float(info->depth_fade_start_pct, cur_info, "Depth_Fade_Start");
    xml_table_get_req_float(info->depth_fade_end_pct, cur_info, "Depth_Fade_End");
    float angle = 0.0f;
    xml_table_get_req_float(angle, cur_info, "Slope_Fade_Start");
    info->slope_fade_start = cosf(angle * 0.017453f);
    xml_table_get_req_float(angle, cur_info, "Slope_Fade_End");
    ++Num_decal_infos;
    info->slope_fade_end = cosf(angle * 0.017453f);
}

// 0x0080AE60 (:109)
void decal_info_parse_table_shared(pasm_data* peg_data, mempool_base* tmp_pool, mempool* pool, mempool* peg_pool, vlib_platform plat)
{
    mempool_mark_scope mark_scope(tmp_pool);
    assasm_start("decal_containers", tmp_pool, pool, Use_packfiles, peg_pool, true, false, true, false);
    xml_element* root = xml_table_parse("decal_info.xtbl");
    for (xml_element* cur_info = xml_find(root, "Decal_Info"); cur_info != nullptr; cur_info = xml_find_next(root, cur_info, "Decal_Info"))
    {
        decal_parse(cur_info, plat); // inlined in the original
    }
    xml_table_close(root);
    assasm_finish(peg_data, false, true, false);
}
