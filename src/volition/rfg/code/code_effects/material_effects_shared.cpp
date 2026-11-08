// Original: volition/rfg/code/code_effects/material_effects_shared.cpp
//
// The material effect table (VFX.md FX4 item 15): matfx.xtbl, up to 32 effects of up to 4 layers (a library material
// and its map index, or the base surface), each with its own stream2 container of the layer materials ("matfx_<name>",
// built at start-up by the asset assembler); lookups by name and id.

#include "volition/rfg/code/code_effects/material_effects_shared.h"

#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/hash_table.h"

#include <stdio.h>

// ---- globals
// Effect_infos and Num_effect_infos are file statics in rfg.pdb but used by material_effects.cpp too (one unity build
// in rfg.exe); external here.
material_effect_info Effect_infos[32]; // 0x01DEB6F0
int Num_effect_infos; // 0x01DEE4C4
// Shared with material_preload_shared.cpp (one table in rfg.exe).
const char* Mat_platform_ext[8] = {"mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc", "mat_pc"}; // 0x01648FCC

// ---- file-static globals
static const char* Layer_flag_names[4] = {"Stencil_write", "Stencil_test", "Stencil_clear", "Additive_blending"}; // 0x01648FF0
static str_data_hash_table_cached<material_effect_info*,32,unsigned char> Effect_info_hash; // 0x01E22300

// The container type of the material effect containers (rfg.exe passes 0x22).
static const unsigned char MATFX_CONTAINER_TYPE = 0x22;
// The stream2 primitive type of a material.
static const unsigned char MATERIAL_PRIM_TYPE = 8;

// 0x006E46F0 (:46)
// LTCG: layer_info in ESI.
static void parse_layer(material_effect_layer_info* layer_info, xml_element* layer_elem)
{
    xml_element* type_elem = xml_table_find_req(layer_elem, "Type");
    xml_element* material_elem = xml_table_find_opt(type_elem, "Material");
    if (material_elem != nullptr)
    {
        char filename[256];
        xml_table_get_req_filename(filename, sizeof(filename), material_elem, "Name");
        cf_drop_path(layer_info->mat_name, sizeof(layer_info->mat_name), filename);
        xml_table_get_req_int(layer_info->map_index, material_elem, "Index");
    }
    if (xml_table_find_opt(type_elem, "Base") != nullptr)
    {
        // The base surface: the instance's own material.
        layer_info->mat_name[0] = '\0';
        layer_info->map_index = -1;
    }
    layer_info->flags = 0;
    xml_element* flags_elem = xml_table_find_opt(layer_elem, "Flags");
    if (flags_elem != nullptr)
    {
        layer_info->flags = (unsigned int)xml_table_get_flags(Layer_flag_names, 4, flags_elem, false);
    }
}

// 0x006F20C0 (:78)
void parse_material_effects_table()
{
    xml_element* root = xml_table_parse("matfx.xtbl");
    for (xml_element* effect_elem = xml_find(root, "Effect"); effect_elem != nullptr && Num_effect_infos < 32; effect_elem = xml_find_next(root, effect_elem, "Effect"))
    {
        material_effect_info* info = &Effect_infos[Num_effect_infos];
        info->srid = 0;
        xml_table_get_req_string(info->name, sizeof(info->name), effect_elem, "Name");
        xml_element* layers_elem = xml_table_find_req(effect_elem, "Layers");
        info->num_layers = xml_table_count(layers_elem, "Layer");
        if (info->num_layers < 0)
        {
            info->num_layers = 0;
        }
        else if (info->num_layers > 4)
        {
            info->num_layers = 4;
        }
        int i = 0;
        for (xml_element* layer_elem = xml_table_find_opt(layers_elem, "Layer"); i < info->num_layers && layer_elem != nullptr; layer_elem = xml_table_find_next(layers_elem, layer_elem, "Layer"))
        {
            parse_layer(&info->layers[i], layer_elem);
            i++;
        }
        Effect_info_hash.insert_key_data(info->name, info);
        Num_effect_infos++;
    }
    xml_table_close(root);
}

// 0x006E47D0 (:119)
// One container per effect with the materials of its layers.
void material_effects_generate_stream_data(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat)
{
    assasm_start("material_effects", tmp_pool, pool, Use_packfiles, nullptr, false, false, true, false);
    for (int i = 0; i < Num_effect_infos; i++)
    {
        material_effect_info* info = &Effect_infos[i];
        char container_name[128];
        sprintf(container_name, "matfx_%s", info->name);
        assasm_container_start(container_name, MATFX_CONTAINER_TYPE, true, false);
        for (int j = 0; j < info->num_layers; j++)
        {
            if (info->layers[j].mat_name[0] != '\0')
            {
                char filename[64];
                cf_add_ext(filename, sizeof(filename), info->layers[j].mat_name, Mat_platform_ext[plat], false);
                assasm_container_add(filename, MATERIAL_PRIM_TYPE, "material effect", false, false, false, nullptr, 0);
            }
        }
        info->srid = assasm_container_finish(false, false, false);
    }
    assasm_finish(nullptr, false, true, false);
}

// 0x006E99C0 (:159)
void material_effects_shared_system_shutdown()
{
    for (int i = 0; i < Num_effect_infos; i++)
    {
        if (Effect_infos[i].srid != 0)
        {
            stream2_release(Effect_infos[i].srid);
            stream2_container_delete(Effect_infos[i].srid);
        }
    }
    Effect_info_hash.clear();
    Num_effect_infos = 0;
}

// 0x006EDAF0 (:176)
int material_effects_find_by_name(const char* name)
{
    material_effect_info* info = nullptr;
    if (Effect_info_hash.get(name, info) && info != nullptr)
    {
        return (int)(info - Effect_infos);
    }
    return -1;
}

// 0x006EDB50 (:188)
int material_effects_get_reconstructor_shards_id()
{
    static int reconstructor_shards_id = material_effects_find_by_name("reconstructor_shards"); // guard $S8 0x01E25750
    return reconstructor_shards_id;
}

// 0x006EDB80 (:195)
int material_effects_get_nano_rifle_matfx_id(bool full_fade)
{
    if (full_fade)
    {
        return material_effects_get_reconstructor_shards_id();
    }
    static int nano_partial_fade_id = material_effects_find_by_name("nanorifle_damage"); // guard $S9 0x01E25758
    return nano_partial_fade_id;
}

// 0x006E9A60 (:206)
material_effect_parameters material_effects_get_parameters(unsigned int flags, int lifetime_ms)
{
    material_effect_parameters parameters;
    parameters.flags = flags;
    parameters.lifetime_ms = lifetime_ms;
    return parameters;
}

// 0x006E4970 (:217)
material_effect_info* material_effects_find_by_id(int effect_id)
{
    if (effect_id >= 0 && effect_id < Num_effect_infos)
    {
        return &Effect_infos[effect_id];
    }
    return nullptr;
}

// 0x006E4990 (:225)
// Adds the layer materials' shader files (.slw3) to the container being assembled.
void material_effects_add_bitmaps_to_current_container(material_effect_info* infop)
{
    for (int i = 0; i < infop->num_layers; i++)
    {
        if (infop->layers[i].mat_name[0] != '\0')
        {
            char filename[64];
            cf_add_ext(filename, sizeof(filename), infop->layers[i].mat_name, ".slw3", false);
            assasm_add_slw3(filename, "material effect", false);
        }
    }
}
