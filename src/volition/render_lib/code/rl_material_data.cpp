// Original: volition/render_lib/code/rl_material_data.cpp
//
// rl_material_data_load. rl_material_data_fixup (the renderer binding) is in rl_material_data_render.cpp, so that this
// file stays GPU-free for the headless tests (rl_material_data.h).
//
// Layout at offset: u32 total size, the rl_material_data (0x1C), pad to 4, the texture descriptions (0xC each), pad to
// 4, the constant name checksums (4 each), pad to 16, the constant block (16 each, max_constants of them). The block
// must be 16-aligned in memory and the parsed size must equal the stored size, else the load fails.

#include "volition/render_lib/code/rl_material_data.h"

// Rounds offset up to a multiple of align (a power of two).
static int align_offset(int offset, int align)
{
    const int rest = offset % align;
    return rest == 0 ? offset : offset + align - rest;
}

// 0x0053F020 (:189)
rl_material_data* rl_material_data_load(unsigned char* buf, int& offset, const char* texture_name_buffer, bool init_render_data)
{
    const int start = offset;
    const int size = *(int*)(buf + start);
    rl_material_data* data = (rl_material_data*)(buf + start + 4);
    offset = align_offset(start + 4 + (int)sizeof(rl_material_data), 4);
    data->textures.from_pointer((rl_texture_desc*)(buf + offset));
    offset += data->num_textures * 0xC;
    offset = align_offset(offset, 4);
    data->constant_name_checksum.from_pointer((unsigned int*)(buf + offset));
    offset += data->num_constants * 4;
    offset = align_offset(offset, 16);
    if (((unsigned int)(buf + offset) & 0xF) != 0)
    {
        return nullptr;
    }
    data->constant_block.from_pointer((rl_material_const*)(buf + offset));
    offset += data->max_constants * 16;
    if (offset - start != size)
    {
        return nullptr;
    }
    if (init_render_data && !rl_material_data_fixup(data, texture_name_buffer))
    {
        return nullptr;
    }
    return data;
}
