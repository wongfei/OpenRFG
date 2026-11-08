// Original: volition/rfg/code/objects/salvage.h
// Implementation: volition/rfg/code/objects/salvage.cpp
//
// Salvage: the salvage items destroyed objects drop (salvage table: value per material), and when they spawn.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/vlib/code/util/hash_table.h"

class item;
class matrix;
class vector;

// size 0xC, type of Salvage_material_data (file local in rfg.pdb; object_item.cpp reads it too, inlining salvage_get_value)
struct salvage_material_data_type
{
    int first_index; // 0x00
    int last_index; // 0x04
    int value; // 0x08
};

extern salvage_material_data_type Salvage_material_data[3]; // 0x0301AE4C
extern const char* Salvage_material_names[3]; // 0x0169476C
extern str_data_hash_table<unsigned int, 32, 32, unsigned char> Salvage_material_name_table; // 0x033B1A30, name -> salvage_material_type

void salvage_system_init(); // 0x00A95FE0 (:196)
void salvage_level_init(); // 0x00A1F3F0 (:202)
void salvage_level_shutdown(); // 0x00A1F400 (:206)
void salvage_do_frame(); // 0x00A1F410 (:210)
item* salvage_create_item(salvage_material_type material, const vector& pos, const matrix& orient); // 0x00A41920 (:214)
salvage_material_type salvage_resolve_material_name(const char* material_name); // 0x00A72490 (:251)
int salvage_get_value(salvage_material_type material); // 0x00A1F420 (:267)
bool salvage_should_spawn(); // 0x00A1F460 (:282)
void salvage_render_debug(); // 0x00A724D0 (:318)
