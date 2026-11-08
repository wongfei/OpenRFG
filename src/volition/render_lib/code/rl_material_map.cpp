// Original: volition/render_lib/code/rl_material_map.cpp
//
// rl_material_map_data_load. The rl_material_map class (factory, material lookup, cloning) is in
// rl_material_map_render.cpp, so that this file stays GPU-free for the headless tests.

#include "volition/render_lib/code/rl_material_map.h"

// 0x0053F1A0 (:126)
rl_material_map_data* rl_material_map_data_load(unsigned char* buf, int& offset)
{
    // The map header (8 bytes) is followed by num_materials material handles.
    rl_material_map_data* data = (rl_material_map_data*)(buf + offset);
    offset += sizeof(rl_material_map_data);
    data->materials.from_pointer((unsigned int*)(buf + offset));
    offset += data->num_materials * 4;
    return data;
}
