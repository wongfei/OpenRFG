// Original: volition/render_lib/code/rl_material_data.cpp (render half)
//
// rl_material_data_fixup (decompiled), the file static of rl_material_data.cpp that binds loaded material data to the
// renderer. It lives in its own file because it needs bmpman and the shader library: rl_material_data.cpp (the
// loader) is in the GPU-free rfg library that the headless tests link with a stand-in of this function, and this file
// is in rfg_render.
//
// The fixup:
// - textures: the texture stage and flags move from the disk layout (two 16-bit fields) into the runtime bit fields;
//   names become bitmap handles (bm_load; flag bit 3 or an empty name gives -1, and a trailing 'n' of the name is
//   changed to 'a' in place);
// - the shader name hash becomes a shader library handle; a texture the shader does not sample is removed, the
//   others get the shader's texture stage, and flagged ones (flags bits 0-2) a clamped sampler;
// - the constants move from their packed order to the shader's constant locations (in floats), Specular_Power and
//   Normal_Map_Height are clamped, and num_constants becomes the shader's user constant space in float4s.
//
// Notes (decompiled behaviour, kept as is):
// - The constants are copied to constant_block + const_loc floats with array_size * num_rows floats each, but the
//   Specular_Power and Normal_Map_Height clamps index whole registers from 20 (const_loc - 0x14) and only for
//   const_loc 20..51, the register layout of the console shaders. With the PC float locations they clamp an
//   unrelated float, or none.
// - A constant whose name is not in the shader is copied to location -1 with size -4 bytes (a huge memcpy). The
//   crunched materials of the game never have one.

#include "volition/render_lib/code/rl_material_data.h"

#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/math/crc.h"

#include "keen/base/trace.hpp"

#include <string.h>

// Bits of rl_texture_desc::Data::DataStruct::texture_flags.
static const unsigned int RL_TEXTURE_FLAG_SAMPLER_MASK = 0x7; // clamped sampler flags
static const unsigned int RL_TEXTURE_FLAG_NO_BITMAP = 0x8; // no bitmap to look up

// The user constant space starts at register 20 in the console layout the clamps assume (see the file comment).
static const unsigned int RL_USER_CONST_FIRST_REGISTER = 0x14;
static const unsigned int RL_USER_CONST_END_REGISTER = 0x34;

static rl_material_const Const_fixup_buffer[256]; // 0x01B69B00 (size inferred: max_constants is 8 bits)

// Clamps the material constant of a named shader constant (first match only) to [min_value, max_value].
static void clamp_user_constant(rl_material_data* data, rl_shader* shader, unsigned int crc, float min_value, float max_value)
{
    const int loc = shader->get_const_index((int)crc); // inlined
    if (loc < (int)RL_USER_CONST_FIRST_REGISTER || loc >= (int)RL_USER_CONST_END_REGISTER)
    {
        return;
    }
    float& value = data->constant_block.to_pointer()[loc - RL_USER_CONST_FIRST_REGISTER].elem[0];
    float clamped = value;
    if (clamped - min_value < 0.0f)
    {
        clamped = min_value;
    }
    if (0.0f <= clamped - max_value)
    {
        clamped = max_value;
    }
    value = clamped;
}

// 0x005273C0 (:32)
bool rl_material_data_fixup(rl_material_data* data, const char* texture_name_buffer)
{
    // Textures: disk layout to runtime layout, names to bitmap handles.
    for (int i = 0; i < (int)data->num_textures; i++)
    {
        rl_texture_desc& desc = data->textures.to_pointer()[i];
        const unsigned int stage = desc.data.disc.texture_stage_disk & 0xF;
        const unsigned int flags = desc.data.disc.texture_flags_disk & 0xF;
        desc.data.rt.texture_flags = flags;
        desc.data.rt.texture_stage = stage;
        desc.data.rt.sampler_state_idx = 0;
        if ((desc.data.rt.texture_flags & RL_TEXTURE_FLAG_NO_BITMAP) != 0)
        {
            desc.texture_handle = -1;
            continue;
        }
        // The name buffer is written to: the texture name is in the loaded (writable) mesh data.
        char* filename = (char*)texture_name_buffer + desc.texture_handle;
        const int len = (int)strlen(filename);
        if (len > 0 && filename[len - 1] == 'n')
        {
            filename[len - 1] = 'a';
        }
        desc.texture_handle = filename[0] == 0 ? -1 : bm_load(filename, false);
    }

    // The shader.
    rl_shader_library* library = rl_shader_library::get_instance();
    const unsigned int handle = library->lookup(data->shader_handle); // inlined
    if (handle == 0xFFFFFFFF)
    {
        data->shader_handle = 0xFFFFFFFF;
        keen::debug::nativePrintArgs("Unable to find shader!");
        return false;
    }
    data->shader_handle = handle;
    rl_shader* shader = library->get_reference_shader(handle); // inlined
    if (shader == nullptr)
    {
        keen::debug::nativePrintArgs("Unable to get reference shader");
        return false;
    }

    // Textures the shader samples get its stage (and a clamped sampler when flagged); the others are removed.
    for (int i = 0; i < (int)data->num_textures; i++)
    {
        rl_texture_desc* textures = data->textures.to_pointer();
        const rl_shader_tex_fixup_runtime* fixup = shader->get_tex_fixup((int)textures[i].name_checksum); // inlined
        if (fixup != nullptr && (short)fixup->constLocData.const_loc != -1)
        {
            textures[i].data.rt.texture_stage = fixup->constLocData.const_loc & 0xF;
            if ((textures[i].data.rt.texture_flags & RL_TEXTURE_FLAG_SAMPLER_MASK) != 0)
            {
                const unsigned short sampler = rl_render_lib::get_instance()->get_clamped_sampler_handle(*shader->get_tex_fixup((int)textures[i].name_checksum),textures[i].data.rt.texture_flags, nullptr);
                // Only 16 bits of the 24-bit field are written.
                textures[i].data.rt.sampler_state_idx = sampler;
            }
            continue;
        }
        if (i < (int)data->num_textures - 1)
        {
            memmove(&textures[i], &textures[i + 1], ((int)data->num_textures - i - 1) * sizeof(rl_texture_desc)); // memcpy in the original (overlapping, front to back)
            i--;
        }
        data->num_textures--;
    }

    // Constants: from their packed order to the shader's locations.
    rl_material_const* constant_block = data->constant_block.to_pointer();
    memcpy(Const_fixup_buffer, constant_block, (unsigned int)data->max_constants << 4);
    memset(constant_block, 0, (unsigned int)data->max_constants << 4);
    for (int i = 0; i < (int)data->num_constants; i++)
    {
        const unsigned int crc = data->constant_name_checksum.to_pointer()[i];
        if (crc == 0xFFFFFFFF)
        {
            continue;
        }
        const unsigned int loc = (unsigned int)shader->get_const_index((int)crc); // inlined
        const int size = shader->get_const_num_rows((int)crc); // inlined
        memcpy((float*)constant_block + loc, &Const_fixup_buffer[i], size * 4);
    }

    static unsigned int specular_power_crc = crc_stri("Specular_Power", 0); // 0x01CD286C
    clamp_user_constant(data, shader, specular_power_crc, 0.1f, 128.0f);
    static unsigned int normal_map_height_crc = crc_stri("Normal_Map_Height", 0);
    clamp_user_constant(data, shader, normal_map_height_crc, 0.0f, 1.0f);

    const unsigned char num_constants = (unsigned char)(shader->get_max_num_consts() >> 2); // inlined
    data->num_constants = num_constants;
    if (data->max_constants < num_constants)
    {
        keen::debug::nativePrintArgs("Crunched data has significantly fewer constants than shader.  Cannot correctly load material.");
        return false;
    }
    return true;
}
