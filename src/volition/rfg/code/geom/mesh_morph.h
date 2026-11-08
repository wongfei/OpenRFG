// Original: volition/rfg/code/geom/mesh_morph.h
// Implementation: volition/rfg/code/geom/mesh_morph.cpp
//
// Mesh morphs (character head and face morph targets) loaded as stream2 files: the load callbacks registered by
// game_stream2_register_types and the lookups (the morph entries are in character_instance.cpp).
#pragma once


class rl_morph_container;

// A loaded morph: its morph container and its index in character_instance.cpp's Morph_array (placement of the
// declaration inferred).
// size 0x10
struct mesh_morph_entry
{
    mesh_morph_entry* next; // 0x00
    mesh_morph_entry* prev; // 0x04
    rl_morph_container* data; // 0x08
    int index; // 0x0C
};

bool mesh_morph_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x00808610 (:71)
void mesh_morph_unload(const char* filename, unsigned int srid); // 0x0080B3F0 (:126)
void mesh_morph_system_init(); // 0x007F46C0 (:156)
void mesh_morph_level_init_pre(); // 0x007F4790 (:176)
void mesh_morph_level_shutdown(); // 0x007F47A0 (:182)
int mesh_morph_find_by_name(const char* name, unsigned int srid); // 0x0080B4E0 (:196)
rl_morph_container* mesh_morph_get_data(int handle); // 0x007F47B0 (:217)
const char* mesh_morph_get_debug_name(int handle); // 0x007F47D0 (:228)
