// Original: volition/rfg/code/objects/object_item_shared.cpp
//
// The item table (items_3d.xtbl) shared with the tools: the item infos and the stream2 containers of the items, through
// the asset assembler (player_minimal.md P3). With packfiles the containers come from the system's preload file
// (items_containers.asm_pc) and the preload containers are requested at once. Compiled in rfg_objects_unity.cpp.
// The external interface is declared in object_item_shared.h.

#include "volition/rfg/code/objects/object_item_shared.h"

#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_stream2_prims.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/bmpman/peg_assemble.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>
#include <string.h>

// ---- globals and file statics
static const char* Item_table_flags[18] = {"spin", "no_auto_delete", "use_pickup", "collide_as_sphere", "collide_as_capsule", "collide_as_cylinder", "collide_as_box", "collide_as_mover", "inherit_bone_transforms", "ignore_physics", "no_pickup_delete", "is_ammo_box", "is_personal_shield", "is_virtual_shield", "high_res_map_large", "high_res_map_small", "special_bbox", "fast_cleanup"}; // 0x01694618
// The marks of Item_preload_pool after the base items' preload containers (placement inferred).
unsigned int Dlc_item_preload_cpu_mark; // 0x02533800
unsigned int Dlc_item_preload_gpu_mark; // 0x02533804

// Magic numbers: item_info::flags bits of Item_table_flags, and the preload bit item_create_asset adds.
static const int ITEM_FLAG_HIGH_RES_MAP_LARGE = 0x4000;
static const int ITEM_FLAG_HIGH_RES_MAP_SMALL = 0x8000;
static const unsigned int ITEM_FLAG_PRELOADED = 0x80000000;

// The extensions per platform (the original builds the arrays on the stack; every platform uses the PC names).
static const char* const Smesh_ext = ".csmesh_pc";
static const char* const Cmesh_ext = ".ccmesh_pc";
static const char* const Rig_ext = ".rig_pc";
static const char* const Morph_ext = "morph_pc";
static const char* const Mat_ext = "mat_pc";

// A copy of text in pool.
static char* copy_string(mempool* pool, const char* text)
{
    const unsigned int size = (unsigned int)strlen(text) + 1;
    char* copy = (char*)pool->alloc(size, 1);
    memcpy(copy, text, size);
    return copy;
}

// ---- internal functions
// 0x00A19810 (:79, LTCG: item in EAX, pool in EDI)
static void item_get_name(xml_element* item, item_info* infop, mempool* pool)
{
    infop->name = copy_string(pool, xml_table_get_req_string_ref(item, "Name"));
}

// 0x00A66030 (:87, LTCG: infop in EDI, pool in ECX; the preloaded size counters are not passed)
// The item's container: its static mesh, character mesh (with rig and morph), pickup mesh and material effect bitmaps,
// and the high-res texture container ("<mesh>_high") of large and small weapons. Character props and head meshes
// only name their meshes (the human containers hold them). The multiplayer pass builds only the multiplayer items.
static bool item_create_asset(xml_element* item, item_info* infop, mempool* pool, bool multiplayer_pass, vlib_platform plat, unsigned int& total_preloaded_cpu_size, unsigned int& total_preloaded_gpu_size, bool is_dlc)
{
    char filename[64];
    if (!multiplayer_pass)
    {
        infop->smesh_name = nullptr;
        const char* mesh = xml_table_get_opt_filename_ref(item, "Mesh");
        if (mesh != nullptr)
        {
            cf_add_ext(filename, sizeof(filename), mesh, Smesh_ext, false);
            infop->smesh_name = copy_string(pool, filename);
        }
        infop->cmesh_name = nullptr;
        infop->rig_name = nullptr;
        xml_element* character_mesh = xml_table_find_opt(item, "character_mesh");
        if (character_mesh != nullptr)
        {
            cf_add_ext(filename, sizeof(filename), xml_table_get_req_filename_ref(character_mesh, "character_mesh"), Cmesh_ext, false);
            infop->cmesh_name = copy_string(pool, filename);
            cf_add_ext(filename, sizeof(filename), xml_table_get_req_filename_ref(character_mesh, "rig"), Rig_ext, false);
            infop->rig_name = copy_string(pool, filename);
            infop->anim_set_name = copy_string(pool, xml_table_get_req_string_ref(character_mesh, "Anim_set"));
        }
        infop->pickup_smesh_name = nullptr;
        const char* pickup_mesh = xml_table_get_opt_filename_ref(item, "pickup_static_mesh");
        if (pickup_mesh != nullptr)
        {
            cf_add_ext(filename, sizeof(filename), pickup_mesh, Smesh_ext, false);
            infop->pickup_smesh_name = copy_string(pool, filename);
        }
        xml_element* item_flags = xml_table_find_opt(item, "Item_Flags");
        if (item_flags != nullptr)
        {
            infop->flags |= xml_table_get_flags(Item_table_flags, 18, item_flags, false);
        }
    }
    xml_element* category = xml_table_find_opt(item, "streaming_category");
    xml_element* material_effect = xml_table_find_opt(item, "MaterialEffect");
    infop->material_effect_id = 0xFFFFFFFF;
    if (material_effect != nullptr)
    {
        infop->material_effect_id = (unsigned int)material_effects_find_by_name(material_effect->text);
    }
    // Original: the category is required by the data (not checked).
    infop->multiplayer_only = v_stricmp(category->text, "Multiplayer") == 0;
    const bool preload = item_is_preload_tag(category->text) || multiplayer_pass;
    const bool build_peg = item_build_peg(category->text);
    const bool build_container = item_build_container(category->text);
    infop->no_peg_load = !build_peg;
    infop->shared_backpack = v_stricmp(category->text, "MP and WC Backpack") == 0;
    if (!build_container || (infop->multiplayer_only && !multiplayer_pass))
    {
        infop->srid = 0;
        infop->high_res_srid = 0;
        return true;
    }
    if (!infop->multiplayer_only && multiplayer_pass)
    {
        return true;
    }
    unsigned char container_type;
    if (multiplayer_pass)
    {
        container_type = GSC_ITEMS_MP_PRELOAD;
    }
    else if (preload)
    {
        container_type = GSC_ITEMS_PRELOAD;
    }
    else
    {
        container_type = (unsigned char)(infop->shared_backpack ? GSC_SHARED_BACKPACK : GSC_ITEMS);
    }
    assasm_container_start(infop->name, container_type, preload, false);
    // The pegs of preloaded items go into the item preload slw, the others into the container.
    const bool add_peg = build_peg && !preload;
    const bool add_slw = build_peg && preload;
    if (infop->smesh_name != nullptr)
    {
        cf_add_ext(filename, sizeof(filename), infop->smesh_name, Smesh_ext, false);
        assasm_container_add(filename, GSP_STATIC_MESH, "items", add_peg, add_slw, false, nullptr, 0);
    }
    if (infop->cmesh_name != nullptr)
    {
        cf_add_ext(filename, sizeof(filename), infop->cmesh_name, Cmesh_ext, false);
        assasm_container_add(filename, GSP_CHARACTER_MESH, "items", add_peg, add_slw, false, nullptr, 0);
        cf_add_ext(filename, sizeof(filename), infop->cmesh_name, Morph_ext, false);
        if (cf_exist(filename))
        {
            assasm_container_add(filename, GSP_MESH_MORPH, "items", false, false, false, nullptr, 0);
        }
    }
    if (infop->pickup_smesh_name != nullptr)
    {
        cf_add_ext(filename, sizeof(filename), infop->pickup_smesh_name, Smesh_ext, false);
        assasm_container_add(filename, GSP_STATIC_MESH, "items", add_peg, add_slw, false, nullptr, 0);
    }
    if (infop->material_effect_id != 0xFFFFFFFF)
    {
        material_effect_info* effect = material_effects_find_by_id((int)infop->material_effect_id);
        if (effect != nullptr)
        {
            material_effects_add_bitmaps_to_current_container(effect);
        }
    }
    const unsigned int srid = assasm_container_finish(false, is_dlc, true);
    if (srid != 0)
    {
        infop->srid = srid;
    }
    if ((infop->flags & (ITEM_FLAG_HIGH_RES_MAP_LARGE | ITEM_FLAG_HIGH_RES_MAP_SMALL)) == 0)
    {
        infop->high_res_srid = 0;
    }
    else
    {
        char high_res_name[64];
        cf_drop_ext(high_res_name, sizeof(high_res_name), infop->cmesh_name != nullptr ? infop->cmesh_name : infop->smesh_name);
        strcat(high_res_name, "_high");
        assasm_container_start(high_res_name, (unsigned char)((infop->flags & ITEM_FLAG_HIGH_RES_MAP_LARGE) != 0 ? GSC_WEAPON_LARGE : GSC_WEAPON_SMALL), false, false);
        cf_add_ext(filename, sizeof(filename), high_res_name, Peg_platform_ext[plat], false);
        assasm_container_add(filename, GSP_PEG, "highres weapon", false, false, false, nullptr, 0);
        cf_add_ext(filename, sizeof(filename), high_res_name, Mat_ext, false);
        assasm_container_add(filename, GSP_MATERIAL, "highres weapon", false, false, false, nullptr, 0);
        const unsigned int high_res_srid = assasm_container_finish(false, is_dlc, true);
        if (high_res_srid != 0)
        {
            infop->high_res_srid = high_res_srid;
        }
    }
    if (preload)
    {
        infop->flags |= ITEM_FLAG_PRELOADED;
    }
    return true;
}

// 0x00A19850 (:334, LTCG: infop in EAX)
static void item_info_clear_common(item_info* infop)
{
    infop->flags = 0;
    infop->anim_set_name = nullptr;
}

// ---- functions
// 0x00A3CB60 (:347): categories preloaded at boot (everything but weapons, props, environmental items, backpacks and
// multiplayer items).
bool item_is_preload_tag(const char* tag)
{
    return v_stricmp(tag, "player weapon") != 0 && v_stricmp(tag, "character prop") != 0 && v_stricmp(tag, "character prop mesh only") != 0 &&
           v_stricmp(tag, "environmental") != 0 && v_stricmp(tag, "MP and WC Backpack") != 0 && !item_is_multiplayer_tag(tag);
}

// 0x00A19860 (:354)
bool item_is_shared_backpack_tag(const char* tag)
{
    return v_stricmp(tag, "MP and WC Backpack") == 0;
}

// 0x00A19880 (:359)
bool item_is_multiplayer_tag(const char* tag)
{
    return v_stricmp(tag, "Multiplayer") == 0;
}

// 0x00A198A0 (:364)
bool item_build_peg(const char* streaming_cat)
{
    return v_stricmp(streaming_cat, "head mesh") != 0 && v_stricmp(streaming_cat, "Character prop mesh only") != 0;
}

// 0x00A198E0 (:369)
bool item_build_container(const char* streaming_cat)
{
    return v_stricmp(streaming_cat, "Character prop") != 0 && v_stricmp(streaming_cat, "Character prop mesh only") != 0;
}

// 0x00A66800 (:374)
// items_3d.xtbl (or a DLC's table, appended): Item_infos is allocated on the first call with room for 27 more items,
// each new item parsed (item_parse_table_element) and its container built (item_create_asset), then the system's peg
// is loaded. The multiplayer pass only builds the multiplayer items' containers, in table order.
bool item_parse_table_shared(mempool_base* tmp_pool, mempool* container_pool, mempool* pool, bool multiplayer_pass, vlib_platform plat, bool verify_only, int dlc_id)
{
    mempool_mark_scope mark(tmp_pool);
    bool is_dlc = dlc_id != -1;
    const bool load_from_preload = Use_packfiles || Game_preload;
    mempool* info_pool;
    const char* system_name;
    if (is_dlc)
    {
        info_pool = &Level_mempool;
        system_name = multiplayer_pass ? "dlc_mp_items_containers" : "dlc_items_containers";
    }
    else
    {
        info_pool = &Permanent_mempool;
        system_name = multiplayer_pass ? "mp_items_containers" : "items_containers";
    }
    assasm_start(system_name, tmp_pool, container_pool, load_from_preload, pool, true, is_dlc, true, true);
    xml_element* table;
    int index;
    if (is_dlc)
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "items_3d.xtbl");
        table = xml_table_parse(filename);
        index = Num_item_infos;
        if (!load_from_preload && !dlc_allocate_temp_container_list(0x1B, container_pool))
        {
            is_dlc = false;
        }
    }
    else
    {
        table = xml_table_parse("items_3d.xtbl");
        index = 0;
        Num_item_infos = 0;
        for (xml_element* item = xml_find(table, "Item"); item != nullptr; item = xml_find_next(table, item, "Item"))
        {
            Num_item_infos++;
        }
    }
    if (Item_infos == nullptr)
    {
        // Magic number: 27 extra items for the DLC tables.
        Max_item_infos = Num_item_infos + 27;
        Item_infos = (item_info*)Permanent_mempool.static_mempool_base::alloc(Max_item_infos * sizeof(item_info), 0);
        if (Item_infos == nullptr)
        {
            return false;
        }
        memset(Item_infos, 0, Max_item_infos * sizeof(item_info));
    }
    unsigned int total_preloaded_cpu_size = 0;
    unsigned int total_preloaded_gpu_size = 0;
    for (xml_element* item = xml_find(table, "Item"); item != nullptr; item = xml_find_next(table, item, "Item"))
    {
        item_info* infop;
        if (multiplayer_pass)
        {
            infop = &Item_infos[index++];
        }
        else
        {
            // The name is looked up through a copy in the scratch pool.
            item_info lookup;
            item_get_name(item, &lookup, (mempool*)tmp_pool);
            infop = object_item_get_info(lookup.name);
            if (infop == nullptr)
            {
                infop = &Item_infos[index++];
                if (is_dlc && ++Num_item_infos > Max_item_infos)
                {
                    return false;
                }
                infop->srid = 0;
                item_info_clear_common(infop);
                item_get_name(item, infop, info_pool);
                item_parse_table_element(item, infop, info_pool);
            }
        }
        item_create_asset(item, infop, pool, multiplayer_pass, plat, total_preloaded_cpu_size, total_preloaded_gpu_size, is_dlc);
    }
    xml_table_close(table);
    if (Dlc_item_preload_cpu_mark == 0)
    {
        if ((Item_preload_pool.get_flags() & 0x40) == 0)
        {
            Dlc_item_preload_cpu_mark = Item_preload_pool.static_mempool_base::mark();
        }
        else
        {
            Dlc_item_preload_cpu_mark = Item_preload_pool.mark_cpu();
            Dlc_item_preload_gpu_mark = Item_preload_pool.mark_gpu();
        }
    }
    pasm_data peg_data;
    peg_data.cpu_data = nullptr;
    peg_data.cpu_size = 0;
    peg_data.gpu_data = nullptr;
    peg_data.gpu_size = 0;
    assasm_finish(&peg_data, verify_only, true, false);
    char peg_name[64];
    cf_add_ext(peg_name, sizeof(peg_name), is_dlc ? "dlc_items_containers" : (multiplayer_pass ? "mp_items_containers" : "items_containers"), ".cpeg_pc", false);
    if (peg_data.cpu_data == nullptr)
    {
        bm_load_peg(peg_name, pool, true);
    }
    else
    {
        bm_load_peg(peg_name, peg_data.cpu_data, peg_data.cpu_size, peg_data.gpu_data, peg_data.gpu_size, true);
    }
    return true;
}
