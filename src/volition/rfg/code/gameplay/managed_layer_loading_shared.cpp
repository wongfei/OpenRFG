// Original: volition/rfg/code/gameplay/managed_layer_loading_shared.cpp

#include "volition/rfg/code/gameplay/managed_layer_loading_shared.h"

#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/parse/xml_table_tools.h"

// 0x016514B0, strings 0x012DA4D0 ("layer_file_0") down to 0x012DA3E0 ("layer_file_15")
const char* XML_MANAGED_LAYER_FILE_NAME_TAGS[16] = {
    "layer_file_0", "layer_file_1", "layer_file_2", "layer_file_3",
    "layer_file_4", "layer_file_5", "layer_file_6", "layer_file_7",
    "layer_file_8", "layer_file_9", "layer_file_10", "layer_file_11",
    "layer_file_12", "layer_file_13", "layer_file_14", "layer_file_15",
};

// 0x0075E730 (:40)
bool managed_layer_loading_parse_shared(const char* system_name, const char* xml_filename, const char* xml_tag_to_find, unsigned char container_type, mempool_base* tmp_pool, mempool* pool, bool preload, vlib_platform plat,
                                        bool is_temp, int dlc_id)
{
    // Original: an extension per platform, ".layer_pc" for all eight.
    static const char* const LAYER_EXTENSIONS[8] = { ".layer_pc", ".layer_pc", ".layer_pc", ".layer_pc", ".layer_pc", ".layer_pc", ".layer_pc", ".layer_pc" };
    // Magic number: primitive type 3 is a layer file.
    static const unsigned char PRIM_TYPE_LAYER = 3;
    bool is_dlc = dlc_id != -1;
    assasm_start(system_name, tmp_pool, (mempool_base*)pool, preload, nullptr, false, is_temp, true, false);
    xml_element* root = xml_table_parse(xml_filename);
    if (root == nullptr)
    {
        return false;
    }
    if (is_temp && !preload && !dlc_allocate_temp_container_list(10, (mempool_base*)pool))
    {
        is_temp = false;
    }
    for (xml_element* element = xml_find(root, xml_tag_to_find); element != nullptr; element = xml_find_next(root, element, xml_tag_to_find))
    {
        const char* name = xml_table_get_req_string_ref(element, "Name");
        xml_element* info = xml_find(element, "managed_layer_info");
        if (info == nullptr || !assasm_container_start(name, container_type, false, false))
        {
            continue;
        }
        for (int i = 0; i < 16; i++)
        {
            const char* layer_file = xml_table_get_opt_string_ref(info, XML_MANAGED_LAYER_FILE_NAME_TAGS[i]);
            if (layer_file != nullptr)
            {
                char file_name[64];
                cf_add_ext(file_name, sizeof(file_name), layer_file, LAYER_EXTENSIONS[plat], false);
                assasm_container_add(file_name, PRIM_TYPE_LAYER, "layer file", false, false, false, nullptr, 0);
            }
        }
        unsigned int srid = assasm_container_finish(false, is_temp, false);
        if (is_dlc && !is_temp)
        {
            dlc_managed_layer_store_srid(srid);
        }
    }
    xml_table_close(root);
    assasm_finish(nullptr, false, true, false);
    return true;
}
