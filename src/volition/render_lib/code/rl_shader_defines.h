// Original: volition/render_lib/code/rl_shader_defines.h
//
// Shader enums and the serialized shader header and fixup records (rl_shader_type, rl_shader_technique_type,
// rl_shader_const_type, rl_shader_header_runtime, rl_shader_const_fixup, rl_shader_tex_fixup). Placement inferred.
#pragma once

#include "keen/graphics/graphicssystem.hpp"
#include "volition/vlib/code/util/endian_type.h"

struct rl_dev_pixel_shader;
struct rl_dev_vertex_shader;
struct rl_shader_tex_fixup_runtime;

namespace keen
{
    struct SamplerState;
    struct VertexInputBinding;
}

enum rl_shader_const_type
{
    rl_shader_const_float = 0,
    rl_shader_const_bool = 1,
    rl_shader_const_sampler = 2,
};

enum rl_shader_technique_type
{
    rl_shader_technique_none = -1,
    rl_shader_technique_base = 0,
    rl_shader_technique_lighting = 1,
    rl_shader_technique_depth_only = 2,
    rl_shader_technique_user = 3,
};

enum rl_shader_type
{
    shader_type_invalid = -1,
    shader_type_generic = 0,
    shader_type_standard = 1,
    shader_type_character = 2,
    shader_type_terrain = 3,
    num_rl_shader_types = 4,
};

// size 0x18
struct gr_shader_const
{
    int var_ref; // 0x00
    short const_loc; // 0x04
    unsigned char data[16]; // 0x06
};

// size 0xC
struct gr_shader_texture_ref
{
    int var_ref; // 0x00
    int tex_ref; // 0x04
    short tex_stage; // 0x08
};

// size 0x2
struct gr_shader_engine_const
{
    short engine_const; // 0x00
};

// size 0x14
struct rl_shader_const_keyframe
{
    float time; // 0x00
    unsigned char val[16]; // 0x04
};

// size 0xC
struct rl_shader_technique
{
    short vs_index; // 0x00
    short ps_index; // 0x02
    unsigned int technique_name_crc; // 0x04
    unsigned int flags; // 0x08
};

// size 0x8
struct rl_vertex_shader
{
    int vs_ref; // 0x00
    unsigned int sig_hash; // 0x04
};

// size 0x4
struct rl_pixel_shader
{
    int ps_ref; // 0x00
};

// size 0x8
struct rl_shader_const_fixup
{
    int crc_value; // 0x00
    unsigned char const_loc; // 0x04
    unsigned char flags; // 0x05
    unsigned char num_rows; // 0x06
    unsigned char array_size; // 0x07
};

// size 0x10
struct rl_shader_tex_fixup
{
    // size 0x1
    union ConstLoc
    {
        // size 0x1
        struct ConstLocStruct
        {
            unsigned char const_loc : 5; // 0x00:0
            unsigned char border_color : 3; // 0x00:5
        };

        union
        {
            unsigned char const_loc_disk; // 0x00
            ConstLocStruct const_loc_d; // 0x00
        };
    };

    // size 0x4
    union ColorData
    {
        // size 0x4
        struct ColorDataStruct
        {
            unsigned char border_color_disk; // 0x00
            unsigned char pad[3]; // 0x01
        };

        union
        {
            ColorDataStruct disc; // 0x00
            unsigned int unused_thing_that_is_only_still_here_so_it_is_a_union; // 0x00
        };
    };

    static const unsigned int kConstLocSize; // 0x012B8070
    static const unsigned int kBorderColorSize; // 0x012B8074
    int crc_value; // 0x00
    ConstLoc constLocData; // 0x04
    unsigned char flags; // 0x05
    unsigned char address_u; // 0x06
    unsigned char address_v; // 0x07
    unsigned char address_w; // 0x08
    unsigned char min_filter; // 0x09
    unsigned char mag_filter; // 0x0A
    unsigned char mip_filter; // 0x0B
    ColorData colorData; // 0x0C
};

// size 0x34
struct rl_shader_header
{
    int signature; // 0x00
    int version; // 0x04
    unsigned int shader_flags; // 0x08
    short num_tex_fixups; // 0x0C
    short num_const_fixups; // 0x0E
    short num_bool_fixups; // 0x10
    signed char num_vertex_shaders; // 0x12
    signed char num_pixel_shaders; // 0x13
    et_ptr_offset<rl_vertex_shader,0> vertex_shaders; // 0x14
    et_ptr_offset<rl_pixel_shader,0> pixel_shaders; // 0x18
    et_ptr_offset<rl_shader_tex_fixup,0> tex_fixups; // 0x1C
    et_ptr_offset<rl_shader_const_fixup,0> const_fixups; // 0x20
    et_ptr_offset<rl_shader_const_fixup,0> bool_fixups; // 0x24
    signed char base_technique_index; // 0x28
    signed char lighting_technique_index; // 0x29
    signed char depth_only_technique_index; // 0x2A
    signed char user_technique_start_index; // 0x2B
    et_ptr_offset<rl_shader_technique,0> shader_techniques; // 0x2C
    int num_techniques; // 0x30
};

// size 0x14
struct rl_shader_technique_runtime
{
    keen::ShaderPipeline pipeline; // 0x00
    const keen::VertexInputBinding** bindings_p; // 0x08
    unsigned int technique_name_crc; // 0x0C
    unsigned int flags; // 0x10
};

// size 0x34
struct rl_shader_header_runtime
{
    int signature; // 0x00
    int version; // 0x04
    unsigned int shader_flags; // 0x08
    short num_tex_fixups; // 0x0C
    short num_const_fixups; // 0x0E
    short num_bool_fixups; // 0x10
    signed char num_vertex_shaders; // 0x12
    signed char num_pixel_shaders; // 0x13
    rl_dev_vertex_shader* vertex_shaders; // 0x14
    rl_dev_pixel_shader* pixel_shaders; // 0x18
    rl_shader_tex_fixup_runtime* tex_fixups; // 0x1C
    rl_shader_const_fixup* const_fixups; // 0x20
    rl_shader_const_fixup* bool_fixups; // 0x24
    signed char base_technique_index; // 0x28
    signed char lighting_technique_index; // 0x29
    signed char depth_only_technique_index; // 0x2A
    signed char user_technique_start_index; // 0x2B
    rl_shader_technique_runtime* shader_techniques; // 0x2C
    int num_techniques; // 0x30
};

// size 0x10
struct rl_shader_tex_fixup_runtime
{
    // size 0x4
    union ColorData
    {
        const keen::SamplerState* internal_sampler_state_p; // 0x00
    };

    static const unsigned int kConstLocSize; // 0x012B8078
    static const unsigned int kBorderColorSize; // 0x012B807C
    int crc_value; // 0x00
    struct
    {
        unsigned char const_loc : 5; // 0x00:0
        unsigned char border_color : 3; // 0x00:5
    } constLocData; // 0x04
    unsigned char flags; // 0x05
    unsigned char address_u; // 0x06
    unsigned char address_v; // 0x07
    unsigned char address_w; // 0x08
    unsigned char min_filter; // 0x09
    unsigned char mag_filter; // 0x0A
    unsigned char mip_filter; // 0x0B
    ColorData colorData; // 0x0C
};
