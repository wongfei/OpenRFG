// Original: volition/rfg/code/geom/material.h
// Implementation: volition/rfg/code/geom/rfg_material.cpp
//
// Standalone materials (decals, effects) loaded as stream2 files: the material and its render_lib data, the stream2
// load callbacks registered by game_stream2_register_types and the lookup by file name (rfg_material.cpp, which has no
// header of its own; material_shared.cpp has the material_load/material_unload pair the tools use). Placement of the
// rfg_material.cpp functions inferred.
#pragma once

#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/vlib/code/util/endian_type.h"

// size 0x28
struct material
{
    unsigned int m_signature; // 0x00
    unsigned int m_version; // 0x04
    et_sized_native_pointer<rl_material_map> m_material_map; // 0x08
    et_sized_native_pointer<et_sized_native_pointer<rl_material> > m_materials; // 0x10
    unsigned int m_num_materials; // 0x18
    et_sized_native_pointer<char const > m_texture_names; // 0x20
};

bool material_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size); // 0x00808AD0 (rfg_material.cpp:38)
void material_unload(const char* filename, unsigned int srid); // 0x0080B550 (rfg_material.cpp:64)
void material_system_init(); // 0x007F4800 (rfg_material.cpp:79)
void material_system_shutdown(); // 0x007F4820 (rfg_material.cpp:88)
material* material_find(const char* filename, unsigned int srid); // 0x0080B5C0 (rfg_material.cpp:97)
void material_level_shutdown(); // 0x007F4840 (rfg_material.cpp:109)
