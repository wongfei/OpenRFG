// Original: volition/rfg/code/geom/character_mesh.h
// Implementation: volition/rfg/code/geom/character_mesh.cpp
//
// Character mesh streaming: character_mesh_load_cb is the GSP_CHARACTER_MESH load callback; meshes are kept by
// (file name, srid) in a stream2_data_hash (character_mesh_find).
#pragma once

#include "volition/rfg/code/geom/character_mesh_shared.h"
#include "volition/vlib/code/util/varray.h"

class rl_color_float;

extern farray<character_mesh*,32> Character_mesh_modified_characters; // 0x0165F3C0

bool character_mesh_load_cb(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x0080A8A0 (:32)
void character_mesh_unload_cb(const char* filename, unsigned int srid); // 0x0080A960 (:61)
bool character_mesh_modify_rimlight_colors(character_mesh* cmesh, const rl_color_float& rimlight_color); // 0x008023A0 (:82)
void character_mesh_system_init(); // 0x007F2500 (:147)
void character_mesh_system_shutdown(); // 0x007F2510 (:151)
character_mesh* character_mesh_find(const char* filename, unsigned int srid); // 0x0080AA20 (:155)
