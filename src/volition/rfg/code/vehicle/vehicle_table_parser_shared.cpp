// Original: volition/rfg/code/vehicle/vehicle_table_parser_shared.cpp
//
// One streamed container per vehicle mesh (GSC_VEHICLE): the .ccar_pc and the animations of its interaction's states
// and actions that anim_files.xtbl does not preload. Shared with the tools (the platform tables hold one extension).

#include "volition/rfg/code/vehicle/vehicle_table_parser_shared.h"

#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_comp.h"

#include <new>
#include <stdio.h>
#include <string.h>

// Container and primitive types of assasm_container_start/_add.
static const unsigned char GSC_VEHICLE_CONTAINER = 0x1D;
static const unsigned char GSP_VEHICLE_PRIM = 0x0A;
static const unsigned char GSP_ANIM_PRIM = 0x11;

// size 0x8
struct vehicle_parse_info
{
    char* interaction; // 0x00
    char* wavebank; // 0x04
};

// A copy of a table string in the temporary pool.
static char* preparse_copy_string(mempool_base* tmp_pool, const char* text)
{
    const unsigned int size = (unsigned int)strlen(text) + 1;
    char* copy = (char*)tmp_pool->alloc(size, 1);
    strcpy(copy, text);
    return copy;
}

// 0x00B93CD0 (:38) (LTCG: vehicle_hash in EDX, filter_out_hash in ECX)
static void preparse_vehicles(str_data_hash_table<vehicle_parse_info*,100,64,unsigned char>* vehicle_hash, mempool_base* tmp_pool, int dlc_id, str_data_hash_table<vehicle_parse_info*,100,64,unsigned char>* filter_out_hash)
{
    xml_element* element;
    if (dlc_id == -1) {
        element = xml_table_parse("vehicles.xtbl");
    }
    else {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "vehicles.xtbl");
        element = xml_table_parse(filename);
    }
    if (element == NULL) {
        return;
    }
    for (xml_element* root = xml_find(element, "Vehicle"); root != NULL; root = xml_find_next(element, root, "Vehicle")) {
        const char* mesh_name = xml_table_get_req_filename_ref(root, "Mesh_Name");
        if (vehicle_hash->exists(mesh_name) || (filter_out_hash != NULL && filter_out_hash->exists(mesh_name))) {
            continue;
        }
        vehicle_parse_info* info = (vehicle_parse_info*)tmp_pool->alloc(sizeof(vehicle_parse_info), 0);
        info->interaction = NULL;
        info->wavebank = NULL;
        const char* text = xml_table_get_opt_string_ref(root, "Vehicle_Interaction");
        if (text != NULL) {
            info->interaction = preparse_copy_string(tmp_pool, text);
        }
        xml_element* foley = xml_find(root, "Foley");
        if (foley != NULL) {
            text = xml_table_get_opt_string_ref(foley, "Wavebank");
            if (text != NULL) {
                info->wavebank = preparse_copy_string(tmp_pool, text);
            }
        }
        vehicle_hash->insert_key_data(mesh_name, info);
    }
    xml_table_close(element);
}

// 0x00B93EC0 (:94)
static void preparse_anims(str_data_hash_table<bool,4096,64,unsigned short>* anims_hash, mempool_base* tmp_pool)
{
    xml_element* element = xml_table_parse("anim_files.xtbl");
    if (element == NULL) {
        return;
    }
    xml_element* anim_files = xml_table_find_req(xml_table_find_req(element, "Files"), "Anim_files");
    for (xml_element* file = xml_find(anim_files, "Anim_file"); file != NULL; file = xml_find_next(anim_files, file, "Anim_file")) {
        for (xml_element* anim = xml_find(file, "Animation"); anim != NULL; anim = xml_find_next(file, anim, "Animation")) {
            // Original quirk kept: the name is copied before the check, duplicates leave their copy in the pool.
            const char* name = preparse_copy_string(tmp_pool, xml_table_get_req_string_ref(anim, "Filename"));
            if (anims_hash->exists(name)) {
                continue;
            }
            const char* preload = xml_table_get_opt_string_ref(anim, "Preload");
            const bool is_preloaded = preload != NULL && v_stricmp(preload, "true") == 0;
            anims_hash->insert_key_data(name, is_preloaded);
        }
    }
    xml_table_close(element);
}

// 0x00B94020 (:116)
static void preparse_interactions(str_data_hash_table<char*,50,64,unsigned char>* interactions_hash, mempool_base* tmp_pool, int dlc_id)
{
    xml_element* element;
    if (dlc_id == -1) {
        element = xml_table_parse("vehicle_interactions.xtbl");
    }
    else {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "vehicle_interactions.xtbl");
        element = xml_table_parse(filename);
    }
    if (element == NULL) {
        return;
    }
    for (xml_element* root = xml_find(element, "Vehicle"); root != NULL; root = xml_find_next(element, root, "Vehicle")) {
        const char* name = xml_table_get_req_string_ref(root, "Name");
        const char* anim_set = xml_table_get_req_string_ref(root, "Anim_Set");
        if (!interactions_hash->exists(name)) {
            char* copy = preparse_copy_string(tmp_pool, anim_set);
            interactions_hash->insert_key_data(name, copy);
        }
    }
    xml_table_close(element);
}

// 0x00B94160 (:141) (LTCG: xml_root in ECX, prim_hash in ESI)
static void parse_anim_tags(xml_element* xml_root, str_data_hash_table<bool,4096,64,unsigned short>* anims_hash, vlib_platform plat, str_data_hash_table<bool,50,64,unsigned char>* prim_hash)
{
    static const char* const anim_extensions[8] = {".anim_pc", ".anim_pc", ".anim_pc", ".anim_pc", ".anim_pc", ".anim_pc", ".anim_pc", ".anim_pc"};
    for (xml_element* anim = xml_find(xml_root, "Animation"); anim != NULL; anim = xml_find_next(xml_root, anim, "Animation")) {
        const char* name = xml_table_get_req_string_ref(anim, "Filename");
        bool is_preloaded = false;
        if (anims_hash->get(name, is_preloaded) && is_preloaded) {
            continue; // in the preload list, not in the vehicle's container
        }
        char filename[64];
        cf_add_ext(filename, 0x40, name, anim_extensions[plat], false);
        if (cf_exist(filename) && !prim_hash->exists(filename)) {
            const bool value = true;
            prim_hash->insert_key_data(filename, value);
            assasm_container_add(filename, GSP_ANIM_PRIM, "vehicles", false, false, false, NULL, '\0');
        }
    }
}

// 0x00BA30C0 (:171)
void build_vehicle_containers_shared(mempool_base* tmp_pool, mempool* pool, vlib_platform plat, bool verify_only, bool release_build, int dlc_id)
{
    static const char* const mesh_extensions[8] = {".ccar_pc", ".ccar_pc", ".ccar_pc", ".ccar_pc", ".ccar_pc", ".ccar_pc", ".ccar_pc", ".ccar_pc"};
    typedef str_data_hash_table<bool,4096,64,unsigned short> anims_table;
    typedef str_data_hash_table<char*,50,64,unsigned char> interactions_table;
    typedef str_data_hash_table<vehicle_parse_info*,100,64,unsigned char> vehicles_table;
    typedef str_data_hash_table<bool,50,64,unsigned char> prims_table;

    bool dlc = dlc_id != -1;
    mempool_mark_scope mark(tmp_pool);
    assasm_start(dlc ? "dlc_vehicle_containers" : "vehicle_containers", tmp_pool, pool, Use_packfiles, NULL, false, dlc, true, false);

    anims_table* anims_hash = new (tmp_pool->alloc(sizeof(anims_table), 0)) anims_table();
    interactions_table* interactions_hash = new (tmp_pool->alloc(sizeof(interactions_table), 0)) interactions_table();
    vehicles_table* vehicle_hash = new (tmp_pool->alloc(sizeof(vehicles_table), 0)) vehicles_table();
    vehicles_table* filter_out_hash = new (tmp_pool->alloc(sizeof(vehicles_table), 0)) vehicles_table();
    prims_table* prim_hash = new (tmp_pool->alloc(sizeof(prims_table), 0)) prims_table();

    preparse_anims(anims_hash, tmp_pool);
    preparse_interactions(interactions_hash, tmp_pool, -1);
    if (dlc_id == -1) {
        preparse_vehicles(vehicle_hash, tmp_pool, -1, NULL);
    }
    else {
        // a DLC table builds containers only for the meshes the base table does not have
        preparse_interactions(interactions_hash, tmp_pool, dlc_id);
        preparse_vehicles(filter_out_hash, tmp_pool, -1, NULL);
        preparse_vehicles(vehicle_hash, tmp_pool, dlc_id, filter_out_hash);
    }

    if (dlc && !Use_packfiles && !dlc_allocate_temp_container_list(10, pool)) {
        dlc = false;
    }

    const char* key = NULL;
    vehicle_parse_info* info = NULL; // a failed lookup keeps the previous key's (it cannot fail: the key is the table's)
    bool more = vehicle_hash->get_first_key(key);
    while (more) {
        vehicle_hash->get(key, info);
        const char* extension = mesh_extensions[plat];
        char mesh_name[64];
        cf_add_ext(mesh_name, 0x40, key, extension, false);
        if (stream2_container_find(mesh_name) == 0) {
            assasm_container_start(mesh_name, GSC_VEHICLE_CONTAINER, false, false);
            prim_hash->clear();
            if (!prim_hash->exists(mesh_name)) {
                const bool value = true;
                prim_hash->insert_key_data(mesh_name, value);
                assasm_container_add(mesh_name, GSP_VEHICLE_PRIM, "vehicles", true, false, false, NULL, '\0');
            }
            if (info->interaction != NULL && interactions_hash->exists(info->interaction)) {
                char* anim_set = NULL;
                interactions_hash->get(info->interaction, anim_set);
                char filename[64];
                cf_add_ext(filename, 0x40, anim_set, "xtbl", false);
                xml_element* element = xml_table_parse(filename);
                if (element == NULL) {
                    // Original quirk kept: a missing animation table ends the whole build without assasm_finish.
                    return;
                }
                for (xml_element* set = xml_find(element, "Skeleton_Set"); set != NULL; set = xml_find_next(element, set, "Skeleton_Set")) {
                    xml_element* groups = xml_table_find_req(set, "Groups");
                    for (xml_element* group = xml_find(groups, "Group"); group != NULL; group = xml_find_next(groups, group, "Group")) {
                        xml_element* states = xml_table_find_req(group, "States");
                        for (xml_element* state = xml_find(states, "State"); state != NULL; state = xml_find_next(states, state, "State")) {
                            parse_anim_tags(state, anims_hash, plat, prim_hash);
                        }
                        xml_element* actions = xml_table_find_req(group, "Actions");
                        for (xml_element* action = xml_find(actions, "Action"); action != NULL; action = xml_find_next(actions, action, "Action")) {
                            parse_anim_tags(action, anims_hash, plat, prim_hash);
                        }
                    }
                }
                xml_table_close(element);
            }
            assasm_container_finish(false, dlc, false);
        }
        more = vehicle_hash->get_next_key(key, key);
    }

    anims_hash->~anims_table();
    filter_out_hash->~vehicles_table();
    vehicle_hash->~vehicles_table();
    prim_hash->~prims_table();
    assasm_finish(NULL, verify_only, true, false);
}
