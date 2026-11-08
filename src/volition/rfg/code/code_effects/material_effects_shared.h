// Original: volition/rfg/code/code_effects/material_effects_shared.h
// Implementation: volition/rfg/code/code_effects/material_effects_shared.cpp
//
// The material effect table (material_effects.xtbl, up to 32 effects of layered material settings), shared with the
// tools: parsing, stream data generation and lookups by name and id.
#pragma once

#include "volition/vlib/code/vlib_core/vlib_core.h"

class material_effect_parameters;
class mempool_base;

// size 0x48
struct material_effect_layer_info
{
    char mat_name[64]; // 0x00
    int map_index; // 0x40
    unsigned int flags; // 0x44
};

// size 0x168
struct material_effect_info
{
    char name[64]; // 0x00
    int num_layers; // 0x40
    material_effect_layer_info layers[4]; // 0x44
    unsigned int srid; // 0x164
};

// File statics in rfg.pdb, shared by material_effects.cpp and material_preload_shared.cpp (external here).
extern material_effect_info Effect_infos[32]; // 0x01DEB6F0
extern int Num_effect_infos; // 0x01DEE4C4
extern const char* Mat_platform_ext[8]; // 0x01648FCC

void parse_material_effects_table(); // 0x006F20C0 (:78)
void material_effects_generate_stream_data(mempool_base* tmp_pool, mempool_base* pool, vlib_platform plat); // 0x006E47D0 (:119)
void material_effects_shared_system_shutdown(); // 0x006E99C0 (:159)
int material_effects_find_by_name(const char* name); // 0x006EDAF0 (:176)
int material_effects_get_reconstructor_shards_id(); // 0x006EDB50 (:188)
int material_effects_get_nano_rifle_matfx_id(bool full_fade); // 0x006EDB80 (:195)
material_effect_parameters material_effects_get_parameters(unsigned int flags, int lifetime_ms); // 0x006E9A60 (:206)
material_effect_info* material_effects_find_by_id(int effect_id); // 0x006E4970 (:217)
void material_effects_add_bitmaps_to_current_container(material_effect_info* infop); // 0x006E4990 (:225)
