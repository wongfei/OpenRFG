// Original: volition/render_lib/code/rl_material_defines.h
//
// Material data records: rl_material_const, rl_material_data, rl_material_layer and rl_material_type. Placement of all
// but rl_material_const inferred.
#pragma once

#include "volition/render_lib/code/rl_texture_defines.h"
#include "volition/vlib/code/util/endian_type.h"

enum rl_material_type
{
    RLMT_LOADED_MATERIAL = 0,
    RLMT_LAYERED_MATERIAL = 1,
    NUM_RL_MATERIAL_TYPES = 2,
};

// size 0x10
struct rl_material_const
{
    void set(float x, float y, float z, float w); // 0x004B6CC0 (.h:63)

    float elem[4]; // 0x00
};

// size 0x1C
struct rl_material_data
{
    rl_material_data(); // 0x004E9F70

    unsigned int shader_handle; // 0x00
    unsigned int name_checksum; // 0x04
    unsigned int mat_flags; // 0x08
    unsigned short num_textures; // 0x0C
    unsigned char num_constants; // 0x0E
    unsigned char max_constants; // 0x0F
    et_ptr_offset<rl_texture_desc,1> textures; // 0x10
    et_ptr_offset<unsigned int,0> constant_name_checksum; // 0x14
    et_ptr_offset<rl_material_const,0> constant_block; // 0x18
};

// 0x004E9F70
// The offsets start at -1 (et_ptr_offset's constructor); the rest is uninitialized.
inline rl_material_data::rl_material_data()
{
}

// 0x004B6CC0 (.h:63)
inline void rl_material_const::set(float x, float y, float z, float w)
{
    elem[0] = x;
    elem[1] = y;
    elem[2] = z;
    elem[3] = w;
}

// size 0x8
class rl_material_layer
{
public:
    rl_material_layer();

    unsigned int m_mat_handle; // 0x00
    unsigned int m_flags; // 0x04
};
