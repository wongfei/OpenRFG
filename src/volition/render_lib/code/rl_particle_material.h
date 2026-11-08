// Original: volition/render_lib/code/rl_particle_material.h
//
// rl_particle_material_data, the material of particle renderables. Placement inferred.
#pragma once

#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/vlib/code/util/endian_type.h"

// size 0x2
struct rl_particle_const_ref
{
    short index; // 0x00
};

// size 0x20
struct rl_particle_shader_const
{
    gr_shader_const sc; // 0x00
    et_ptr_offset<rl_shader_const_keyframe,0> keys; // 0x18
    unsigned char num_keys; // 0x1C
    unsigned char var_type; // 0x1D
};

// size 0xC
struct rl_particle_const_info
{
    et_ptr_offset<rl_particle_const_ref,0> const_refs; // 0x00
    unsigned short num_const_refs; // 0x04
    unsigned short num_engine_consts; // 0x06
    et_ptr_offset<gr_shader_engine_const,0> engine_consts; // 0x08
};

// size 0x30
struct rl_particle_material_data
{
    enum e_flags
    {
        MATERIAL_FLAG_UNUSED = 1,
        MATERIAL_FLAG_ADDITIVE = 2,
        MATERIAL_FLAG_SUBTRACTIVE = 4,
        MATERIAL_FLAG_DISTORTION = 8,
        MATERIAL_FLAG_COLOR_PASS = 16,
        MATERIAL_FLAG_HALF_RES = 32,
        MATERIAL_FLAG_QUARTER_RES = 64,
        MATERIAL_FLAG_ALWAYS_ON_TOP = 128,
        MATERIAL_FLAG_SOFT_RENDER = 256,
        MATERIAL_FLAG_COLOR_CORRECT = 512,
        MATERIAL_FLAG_GLOW_PASS = 1024,
    };

    int effect_ref; // 0x00
    short shader_idx; // 0x04
    short num_textures; // 0x06
    et_ptr_offset<gr_shader_texture_ref,1> textures; // 0x08
    unsigned short num_shader_consts; // 0x0C
    et_ptr_offset<rl_particle_shader_const,1> shader_consts; // 0x10
    rl_particle_const_info vs_const_info; // 0x14
    rl_particle_const_info ps_const_info; // 0x20
    int flags; // 0x2C
};
