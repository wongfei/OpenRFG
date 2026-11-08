// Original: volition/rfg/code/objects/humans/human_info_shared.cpp
//
// The containers of the human types, shared with the tools (player_minimal.md P3): one container per Character of
// character.xtbl (the player's in "Player", the others in "Human") with its character mesh and morphs, its heads (LOD
// mesh, peg, morph, material) and the meshes of its prop items. With packfiles the containers come from the system's
// preload file (character_containers.asm_pc). Compiled in rfg_objects_unity.cpp.
// The external interface is declared in human_info_shared.h.

#include "volition/rfg/code/objects/humans/human_info_shared.h"

#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_stream2_prims.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>
#include <string.h>

// ---- TU-private types (rfg.pdb)
// The meshes of a character prop item of items_3d.xtbl.
// size 0xC
struct item_container_info
{
    char* smesh_name; // 0x00
    char* cmesh_name; // 0x04
    bool add_peg; // 0x08
};

// The extensions per platform (the original builds the arrays on the stack; every platform uses the PC names).
static const char* const Human_smesh_ext = ".csmesh_pc";
static const char* const Human_cmesh_ext = ".ccmesh_pc";
static const char* const Human_morph_ext = "morph_pc";
static const char* const Human_mat_ext = "mat_pc";

// A copy of text in pool.
static char* copy_human_string(mempool_base* pool, const char* text)
{
    const unsigned int size = (unsigned int)strlen(text) + 1;
    char* copy = (char*)pool->alloc(size, 1);
    memcpy(copy, text, size);
    return copy;
}

// ---- internal functions
// 0x00A56000 (:41, LTCG: items_hash in ECX, tmp_pool in ESI)
// The character prop items of items_3d.xtbl (or a DLC's table) by name, with their mesh file names, in tmp_pool.
static void preparse_items(str_data_hash_table_cached<item_container_info*,500,unsigned short>& items_hash, mempool_base* tmp_pool, int dlc_id)
{
    xml_element* table;
    if (dlc_id == -1)
    {
        table = xml_table_parse("items_3d.xtbl");
    }
    else
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "items_3d.xtbl");
        table = xml_table_parse(filename);
    }
    if (table == nullptr)
    {
        return;
    }
    for (xml_element* item = xml_find(table, "Item"); item != nullptr; item = xml_find_next(table, item, "Item"))
    {
        const char* category = xml_table_get_req_string_ref(item, "streaming_category");
        if (v_stricmp(category, "Character prop") != 0 && v_stricmp(category, "Character prop mesh only") != 0)
        {
            continue;
        }
        char name[64];
        xml_table_get_req_string(name, 64, item, "Name");
        char* key = copy_human_string(tmp_pool, name);
        item_container_info* existing = nullptr;
        if (items_hash.get(key, existing))
        {
            continue;
        }
        item_container_info* info = (item_container_info*)tmp_pool->alloc(sizeof(item_container_info), 4);
        info->smesh_name = nullptr;
        char mesh[64];
        char filename[72];
        if (xml_table_get_opt_filename(mesh, sizeof(mesh), item, "Mesh"))
        {
            cf_add_ext(filename, 64, mesh, Human_smesh_ext, false);
            info->smesh_name = copy_human_string(tmp_pool, filename);
        }
        info->cmesh_name = nullptr;
        if (xml_table_get_opt_filename(mesh, sizeof(mesh), xml_find(item, "character_mesh"), "character_mesh"))
        {
            cf_add_ext(filename, 64, mesh, Human_cmesh_ext, false);
            info->cmesh_name = copy_human_string(tmp_pool, filename);
        }
        // Mesh-only props get their pegs elsewhere.
        info->add_peg = v_stricmp(category, "Character prop") == 0;
        items_hash.insert_key_data(key, info);
    }
    xml_table_close(table);
}

// Adds the meshes of a prop item (static mesh, character mesh and its morph) to the current container.
static void add_prop_item(str_data_hash_table_cached<item_container_info*,500,unsigned short>& items_hash, const char* name, const char* morph_ext)
{
    item_container_info* info = nullptr;
    if (!items_hash.get(name, info) || info == nullptr)
    {
        return;
    }
    char filename[64];
    if (info->smesh_name != nullptr)
    {
        cf_add_ext(filename, sizeof(filename), info->smesh_name, Human_smesh_ext, false);
        assasm_container_add(filename, GSP_STATIC_MESH, "humans", info->add_peg, false, false, nullptr, 0);
    }
    if (info->cmesh_name != nullptr)
    {
        cf_add_ext(filename, sizeof(filename), info->cmesh_name, Human_cmesh_ext, false);
        assasm_container_add(filename, GSP_CHARACTER_MESH, "humans", info->add_peg, false, false, nullptr, 0);
        cf_add_ext(filename, sizeof(filename), info->cmesh_name, morph_ext, false);
        if (cf_exist(filename))
        {
            assasm_container_add(filename, GSP_MESH_MORPH, "humans", false, false, false, nullptr, 0);
        }
    }
}

// 0x00A562A0 (:102, LTCG: xml_char_root, items_hash and is_temp on the stack, plat in ECX; tmp_pool is not passed)
// The container of one Character, unless it (or its shared container) exists already.
static void human_parse_shared(xml_element* xml_char_root, mempool_base* tmp_pool, str_data_hash_table_cached<item_container_info*,500,unsigned short>& items_hash, vlib_platform plat, bool is_temp)
{
    xml_element* name = xml_find(xml_char_root, "Name");
    const char* model = xml_table_get_req_filename_ref(xml_char_root, "Model");
    const char* shared_container = xml_table_get_opt_string_ref(xml_char_root, "shared_container");
    // Mason's container is the player's.
    unsigned char container_type = GSC_HUMAN;
    if (v_stricmp(model, "mason.cmeshx") == 0 || v_stricmp(name->text, "rfg_PC_DLC") == 0)
    {
        container_type = GSC_PLAYER;
    }
    if (stream2_container_find(name->text) != 0)
    {
        return;
    }
    if (shared_container != nullptr && shared_container[0] != '\0' && stream2_container_find(shared_container) != 0)
    {
        return;
    }
    assasm_container_start(name->text, container_type, false, false);
    char filename[64];
    cf_add_ext(filename, sizeof(filename), model, Human_morph_ext, false);
    if (cf_exist(filename))
    {
        assasm_container_add(filename, GSP_MESH_MORPH, "humans", false, false, false, nullptr, 0);
    }
    cf_add_ext(filename, sizeof(filename), model, Human_cmesh_ext, false);
    assasm_container_add(filename, GSP_CHARACTER_MESH, "humans", true, false, false, nullptr, 0);
    xml_element* heads = xml_table_find_opt(xml_char_root, "Heads");
    if (heads != nullptr)
    {
        for (xml_element* head = xml_find(heads, "Head"); head != nullptr; head = xml_find_next(heads, head, "Head"))
        {
            const char* head_name = xml_table_get_req_string_ref(head, "Name");
            const char* lod_head = xml_table_get_req_string_ref(head, "Lod_Head");
            if (v_stricmp(lod_head, "None") != 0)
            {
                item_container_info* lod_info = nullptr;
                if (items_hash.get(lod_head, lod_info) && lod_info != nullptr)
                {
                    cf_add_ext(filename, sizeof(filename), lod_head, Human_cmesh_ext, false);
                    assasm_container_add(filename, GSP_CHARACTER_MESH, "humans", false, false, false, nullptr, 0);
                }
            }
            cf_add_ext(filename, sizeof(filename), head_name, Peg_platform_ext[plat], false);
            assasm_container_add(filename, GSP_PEG, "humans", false, false, false, nullptr, 0);
            cf_add_ext(filename, sizeof(filename), head_name, Human_morph_ext, false);
            if (cf_exist(filename))
            {
                assasm_container_add(filename, GSP_MESH_MORPH, "humans", false, false, false, nullptr, 0);
            }
            cf_add_ext(filename, sizeof(filename), head_name, Human_mat_ext, false);
            if (cf_exist(filename))
            {
                assasm_container_add(filename, GSP_MATERIAL, "humans", false, false, false, nullptr, 0);
            }
        }
    }
    xml_element* props = xml_table_find_opt(xml_char_root, "Props");
    if (props != nullptr)
    {
        for (xml_element* prop = xml_find(props, "Prop"); prop != nullptr; prop = xml_find_next(props, prop, "Prop"))
        {
            xml_element* items = xml_table_find_req(prop, "Items");
            for (xml_element* item = xml_find(items, "Item"); item != nullptr; item = xml_find_next(items, item, "Item"))
            {
                item_container_info* info = nullptr;
                if (!items_hash.get(xml_table_get_req_string_ref(item, "Name"), info) || info == nullptr)
                {
                    continue;
                }
                add_prop_item(items_hash, xml_table_get_req_string_ref(item, "Name"), Human_morph_ext);
                xml_element* sub_items = xml_table_find_opt(item, "Sub_Items");
                if (sub_items != nullptr)
                {
                    for (xml_element* sub_item = xml_find(sub_items, "Sub_Item"); sub_item != nullptr; sub_item = xml_find_next(sub_items, sub_item, "Sub_Item"))
                    {
                        add_prop_item(items_hash, xml_table_get_req_string_ref(sub_item, "Name"), Human_morph_ext);
                    }
                }
            }
        }
    }
    assasm_container_finish(false, is_temp, false);
}

// ---- functions
// 0x00A9D1E0 (:353)
void build_human_containers_shared(mempool_base* tmp_pool, mempool* pool, vlib_platform plat, bool verify_only, int dlc_id)
{
    mempool_mark_scope mark(tmp_pool);
    bool is_dlc = dlc_id != -1;
    const bool load_from_preload = Use_packfiles;
    assasm_start(is_dlc ? "dlc_character_containers" : "character_containers", tmp_pool, pool, load_from_preload, nullptr, false, is_dlc, true, false);
    str_data_hash_table_cached<item_container_info*,500,unsigned short> items_hash;
    preparse_items(items_hash, tmp_pool, dlc_id);
    xml_element* table;
    if (is_dlc)
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "character.xtbl");
        table = xml_table_parse(filename);
    }
    else
    {
        table = xml_table_parse("character.xtbl");
    }
    if (table == nullptr)
    {
        return;
    }
    if (is_dlc && !load_from_preload && !dlc_allocate_temp_container_list(0x19, pool))
    {
        is_dlc = false;
    }
    for (xml_element* character = xml_find(table, "Character"); character != nullptr; character = xml_find_next(table, character, "Character"))
    {
        human_parse_shared(character, tmp_pool, items_hash, plat, is_dlc);
    }
    xml_table_close(table);
    assasm_finish(nullptr, verify_only, true, false);
}
