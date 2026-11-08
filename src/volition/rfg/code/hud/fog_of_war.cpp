// Original: volition/rfg/code/hud/fog_of_war.cpp
//
// The fog of war: a bit per 16 m map cell, 1 is under the fog, its save section, and its rendering on the full screen map
// (a mask texture of the bits and two materials, used in turn, with a moving fog texture).

#include "volition/rfg/code/hud/fog_of_war.h"

#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_cached_dyn_texture.h"
#include "volition/render_lib/code/rl_draw.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_primitive_defines.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_singleton.h"
#include "volition/render_lib/code/rl_texture_defines.h"
#include "volition/rfg/code/hud/fullscreen_map.h"
#include "volition/rfg/code/interface/ui_memory_manager.h"
#include "volition/rfg/code/level/rfg_zonex.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/bitarray.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <string.h>

// Magic numbers: 512 x 512 cells of 16 m (1/16 = 0.0625) over the 16 x 16 zones of a territory.
static const int Fow_grid_size = 0x200;
static const int Fow_num_bits = Fow_grid_size * Fow_grid_size;
static const float Fow_cell_size = 16.0f;
static const float Fow_inverse_cell_size = 0.0625f;

// ---- file-static globals
static bit_array Fow_bit_array;
static unsigned char Fow_allocation_space[Fow_num_bits / 8];
static vector Fow_offset; // the map's corner: the first zone's minimum

bool Fog_of_war_render = true; // 0x01660D77

// ---- file-static globals of the rendering
static unsigned char Fow_shader_base_color_const = 0xFF; // 0x01660C09
static unsigned char Fow_shader_layer1_offset_scale_const = 0xFF; // 0x01660C0A
static unsigned char Fow_shader_layer2_offset_scale_const = 0xFF; // 0x01660C0B
static unsigned char Fow_shader_layer1_tint_const = 0xFF; // 0x01660D74
static unsigned char Fow_shader_layer2_tint_const = 0xFF; // 0x01660D75
static unsigned char Fow_shader_opacity_const = 0xFF; // 0x01660D76
static float Fow_opacity = 0.8f; // 0x01660D78
static float Fow_layer1_scale = 4.0f; // 0x01660D7C
static float Fow_layer2_scale = 5.0f; // 0x01660D80
static const float Fow_color_alpha = 0.785f; // 0x0166126C + 0xC, Fow_color's alpha; only Fow_alpha reads it
static const vector Fow_layer1_move_speed(0.005f, -0.007f, 0.0f); // 0x0166127C
static const vector Fow_layer2_move_speed(0.01f, 0.008f, 0.0f); // 0x01661288
static const vector Fow_layer1_tint(0.5f, 0.3f, 0.2f); // 0x01661294
static const vector Fow_layer2_tint(0.22f, 0.12f, 0.05f); // 0x016612A0
static const float ninety_radians = 1.57077f; // 0x012FC4E8
static rl_material* Fog_of_war_mask_materials[2]; // 0x02B953E0
static unsigned int Fog_of_war_mask_material_handle[2]; // 0x02B9568C
static rl_material_data* Fog_of_war_mask_material_data[2]; // 0x02B95858
static rl_cached_dyn_texture* Fog_of_war_mask_texture; // 0x02B9DC00
static bool Current_fow_material; // 0x02B9DC04
static int Curr_time; // 0x02B9DBF8
static int Start_time; // 0x02B9DBFC
static float Fow_alpha; // 0x02B9DBF4
static int Background_bitmap_id; // 0x02B9DBE4
static int Crack_bitmap_id; // 0x02B9DBE8
static const bm_image* Crack_image_info; // 0x02B9DBEC
static void* Bbox_memory; // 0x02B9DBF0
static int My_mark; // 0x02B9DBD4
static int My_end_mark; // 0x02B9DBD8
static int Fow_total_rects_drawn; // 0x02B9DBE0
static bool Fow_opacity_modified; // the console's override flag
static vector2 Fow_layer1_offset; // 0x02B9DD70
static vector2 Fow_layer2_offset; // 0x02B9DD78

// ---- internal functions

// 0x0083F6E0 (:166)
// The bit of a position, -1 outside the map.
static int fow_get_bit_index(const vector& pos)
{
    const unsigned int x = (unsigned int)((pos.x - Fow_offset.x) * Fow_inverse_cell_size);
    if (x >= (unsigned int)Fow_grid_size)
    {
        return -1;
    }
    const unsigned int z = (unsigned int)((Fow_offset.z - pos.z) * Fow_inverse_cell_size);
    if (z >= (unsigned int)Fow_grid_size)
    {
        return -1;
    }
    return (int)(z * Fow_grid_size + x);
}

// 0x0083F650 (:135)
void unused_dcf_fow_opacity()
{
    if (Console_command != 0)
    {
        if (console_get_arg(0x10, 0)) // Magic number: the float argument type, as the asm passes.
        {
            Fow_opacity = Console_arg_float;
            Fow_opacity_modified = true;
        }
        else
        {
            Fow_opacity_modified = false;
        }
    }
}

// 0x00850A10 (:513)
// The mask: 255 where the fog is, 0 where it is gone.
static bool fog_of_war_update_mask_texture(rl_renderer& renderer)
{
    if (Fog_of_war_mask_texture == nullptr)
    {
        return false;
    }
    const unsigned int mark = Common_mempool.mark();
    unsigned char* data = (unsigned char*)Common_mempool.alloc(Fow_num_bits, 0);
    if (data == nullptr)
    {
        return false;
    }
    for (int i = 0; i < Fow_num_bits; i++)
    {
        data[i] = Fow_bit_array.get(i) == 1 ? 0xFF : 0;
    }
    Fog_of_war_mask_texture->update(renderer, data, -1);
    Common_mempool.restore_to_mark(mark);
    return true;
}

// 0x00866B00 (:541)
static void fog_of_war_update_material(rl_material_data* mat_data, const vector4& layer1_scale_offset, const vector& layer1_tint, const vector4& layer2_scale_offset, const vector& layer2_tint, float fog_opacity)
{
    float (*constants)[4] = (float (*)[4])mat_data->constant_block.to_pointer();
    // Magic number: the base color's alpha.
    constants[Fow_shader_base_color_const][0] = 0.0f;
    constants[Fow_shader_base_color_const][1] = 0.0f;
    constants[Fow_shader_base_color_const][2] = 0.0f;
    constants[Fow_shader_base_color_const][3] = 0.7f;
    memcpy(constants[Fow_shader_layer1_offset_scale_const], &layer1_scale_offset, 16);
    memcpy(constants[Fow_shader_layer2_offset_scale_const], &layer2_scale_offset, 16);
    constants[Fow_shader_layer1_tint_const][0] = layer1_tint.x;
    constants[Fow_shader_layer1_tint_const][1] = layer1_tint.y;
    constants[Fow_shader_layer1_tint_const][2] = layer1_tint.z;
    constants[Fow_shader_layer1_tint_const][3] = 0.0f;
    constants[Fow_shader_layer2_tint_const][0] = layer2_tint.x;
    constants[Fow_shader_layer2_tint_const][1] = layer2_tint.y;
    constants[Fow_shader_layer2_tint_const][2] = layer2_tint.z;
    constants[Fow_shader_layer2_tint_const][3] = 0.0f;
    constants[Fow_shader_opacity_const][0] = fog_opacity;
    constants[Fow_shader_opacity_const][1] = 0.0f;
    constants[Fow_shader_opacity_const][2] = 0.0f;
    constants[Fow_shader_opacity_const][3] = 0.0f;
}

// 0x0087A1D0 (:557)
static bool fog_of_war_create_mask_material(rl_material** material_p, rl_material_data** material_data_p, unsigned int* mat_handle)
{
    static bool which_material = false; // 0x02BE5950
    if (*mat_handle != 0xFFFFFFFF || *material_p != nullptr || *material_data_p != nullptr || Fog_of_war_mask_texture == nullptr)
    {
        return false;
    }
    rl_shader_library* library = rl_shader_library::get_instance();
    const unsigned int handle = library->lookup("ui_fog_of_war");
    if (handle == 0xFFFFFFFF)
    {
        return false;
    }
    const int num_consts = library->get_shader_num_consts(handle);
    // The constant's slot is in the index's upper bits, as the asm shifts it.
    Fow_shader_base_color_const = (unsigned char)((library->get_shader_const_index(handle, crc_stri("Base_color", 0)) >> 2) & 0x3F);
    Fow_shader_layer1_offset_scale_const = (unsigned char)((library->get_shader_const_index(handle, crc_stri("Fog_layer1_offset_scale", 0)) >> 2) & 0x3F);
    Fow_shader_layer2_offset_scale_const = (unsigned char)((library->get_shader_const_index(handle, crc_stri("Fog_layer2_offset_scale", 0)) >> 2) & 0x3F);
    Fow_shader_layer1_tint_const = (unsigned char)((library->get_shader_const_index(handle, crc_stri("Fog_layer1_tint", 0)) >> 2) & 0x3F);
    Fow_shader_layer2_tint_const = (unsigned char)((library->get_shader_const_index(handle, crc_stri("Fog_layer2_tint", 0)) >> 2) & 0x3F);
    Fow_shader_opacity_const = (unsigned char)((library->get_shader_const_index(handle, crc_stri("Fog_opacity", 0)) >> 2) & 0x3F);
    char name[32];
    _snprintf_safe(name, 0x20, which_material ? "fow_material2" : "fow_material1");
    which_material = !which_material;
    // Magic number: the data's 0x1C bytes, then two 0xC texture descriptors, then the constants.
    // Original bug kept: the block is not cleared, so name_checksum, constant_name_checksum, the descriptors' upper
    // bits and mat_flags (only ORed below) keep whatever the heap left there.
    rl_material_data* mat_data = (rl_material_data*)mem_manager_request_memory(num_consts * 0x10 + 0x34, name, 0);
    rl_texture_desc* textures = (rl_texture_desc*)(mat_data + 1);
    mat_data->textures.from_pointer(textures);
    mat_data->num_textures = 2;
    textures[0].texture_handle = Fog_of_war_mask_texture->get_texture_handle();
    textures[0].data.rt.texture_flags = 0;
    textures[0].name_checksum = crc_stri("Fog_of_war_mask", 0);
    int tmu = library->get_shader_tmu_index(handle, textures[0].name_checksum);
    if (tmu == -1)
    {
        return false;
    }
    textures[0].data.rt.texture_stage = tmu;
    textures[1].texture_handle = bm_load("ui_fsmap_fog.tga", false);
    textures[1].data.rt.texture_flags = 0;
    textures[1].name_checksum = crc_stri("Fog_texture", 0);
    tmu = library->get_shader_tmu_index(handle, textures[1].name_checksum);
    if (tmu == -1)
    {
        return false;
    }
    textures[1].data.rt.texture_stage = tmu;
    mat_data->num_constants = (unsigned char)num_consts;
    mat_data->max_constants = (unsigned char)num_consts;
    mat_data->constant_block.from_pointer((rl_material_const*)(textures + 2));
    memset(mat_data->constant_block.to_pointer(), 0, (unsigned int)mat_data->num_constants << 4);
    const vector4 layer1(Fow_layer1_scale, Fow_layer1_scale, 0.0f, 0.0f);
    const vector4 layer2(Fow_layer2_scale, Fow_layer2_scale, Fow_layer2_offset.x, Fow_layer2_offset.y);
    fog_of_war_update_material(mat_data, layer1, Fow_layer1_tint, layer2, Fow_layer2_tint, Fow_opacity);
    mat_data->mat_flags |= 0x110; // Magic number: the flags the asm sets.
    mat_data->shader_handle = handle;
    *material_data_p = mat_data;
    *material_p = rl_material::create();
    (*material_p)->set_material_data(mat_data);
    *mat_handle = rl_material_library_register(*material_p);
    return *mat_handle != 0xFFFFFFFF;
}

// 0x0083F7C0 (:669)
static bool fog_of_war_allocate_mask_texture()
{
    if (Fog_of_war_mask_texture != nullptr)
    {
        return false;
    }
    Fog_of_war_mask_texture = rl_cached_dyn_texture::create();
    Fog_of_war_mask_texture->init(Fow_grid_size, Fow_grid_size, RLCDT_FORMAT_A8, RLCDT_MEM_DYNAMIC);
    return true;
}

// 0x00866C30 (:691)
static void fog_of_war_render_fog_layer(const bbox& render_area, const bbox& view_area)
{
    if (Fog_of_war_mask_texture == nullptr)
    {
        return;
    }
    Fow_layer1_offset.x = (float)((double)(float)((double)Frametime_real * (double)Fow_layer1_move_speed.x) + (double)Fow_layer1_offset.x);
    Fow_layer1_offset.y = (float)((double)(float)((double)Frametime_real * (double)Fow_layer1_move_speed.y) + (double)Fow_layer1_offset.y);
    Fow_layer2_offset.x = (float)((double)(float)((double)Fow_layer2_move_speed.x * (double)Frametime_real) + (double)Fow_layer2_offset.x);
    Fow_layer2_offset.y = (float)((double)(float)((double)Fow_layer2_move_speed.y * (double)Frametime_real) + (double)Fow_layer2_offset.y);
    // Magic numbers: the fog thins from the 0.1 zoom to the 0.8 zoom, with the float constants widened as in the asm.
    const float zoom = (float)(((double)Fs_info.zoom_level - (double)0.1f) / (double)0.7f);
    float angle = (float)((double)zoom * 10.0 * (double)ninety_radians);
    if ((float)((double)angle - (double)ninety_radians) >= 0.0f)
    {
        angle = ninety_radians;
    }
    const float sine = (float)sin((double)angle);
    Fow_opacity = (float)((double)sine * (double)0.05f + (double)0.8f);
    const int current = Current_fow_material ? 1 : 0;
    const unsigned int material_handle = Fog_of_war_mask_material_handle[current];
    const vector4 layer1(Fow_layer1_scale, Fow_layer1_scale, Fow_layer1_offset.x, Fow_layer1_offset.y);
    const vector4 layer2(Fow_layer2_scale, Fow_layer2_scale, Fow_layer2_offset.x, Fow_layer2_offset.y);
    fog_of_war_update_material(Fog_of_war_mask_material_data[current], layer1, Fow_layer1_tint, layer2, Fow_layer2_tint, Fow_opacity);
    Current_fow_material = !Current_fow_material;
    rl_primitive_state state;
    gr_convert_to_rl_state(state, GM_FILTER);
    rl_draw* draw = rl_draw::get_instance();
    draw->set_material(material_handle);
    draw->tristrip_2d_begin(state);
    rl_primitive_2d_vert* verts = draw->tristrip_2d_allocate_tris(2);
    if (verts == nullptr)
    {
        return;
    }
    // Magic numbers: 1/16 per cell over the mask's 511 cell spans.
    const float u0 = (float)((double)(float)((double)(float)(view_area.min.x - Fow_offset.x) * 0.0625) / 511.0);
    const float u1 = (float)((double)(float)((double)(float)(view_area.max.x - Fow_offset.x) * 0.0625) / 511.0);
    const float v0 = (float)((double)(float)((double)(float)(Fow_offset.z - view_area.max.z) * 0.0625) / 511.0);
    const float v1 = (float)((double)(float)((double)(float)(Fow_offset.z - view_area.min.z) * 0.0625) / 511.0);
    verts[0].uvs.set(u0, v0);
    verts[1].uvs.set(u1, v0);
    verts[2].uvs.set(u0, v1);
    verts[3].uvs.set(u1, v1);
    for (int i = 0; i < 4; i++)
    {
        verts[i].color.red = 0xFF;
        verts[i].color.green = 0xFF;
        verts[i].color.blue = 0xFF;
        verts[i].color.alpha = 0xFF;
    }
    verts[0].pt.set(render_area.min.x, render_area.min.y);
    verts[1].pt.set(render_area.max.x, render_area.min.y);
    verts[2].pt.set(render_area.min.x, render_area.max.y);
    verts[3].pt.set(render_area.max.x, render_area.max.y);
    draw->tristrip_2d_end();
    Fow_total_rects_drawn++;
    draw->set_material(0xFFFFFFFF);
}

// ---- external interface

// 0x00850AB0 (:778)
void fog_of_war_init()
{
    Fow_bit_array.init(Fow_allocation_space, Fow_num_bits);
    Fow_bit_array.clear(true);
    vector zone_min;
    vector zone_max;
    rfg_zonex_get_zone_bbox(0, 0, zone_min, zone_max);
    Fow_offset.x = zone_min.x;
    Fow_offset.y = 0.0f;
    Fow_offset.z = zone_min.z;
    Fog_of_war_mask_materials[0] = nullptr;
    Fog_of_war_mask_materials[1] = nullptr;
    Fog_of_war_mask_material_data[0] = nullptr;
    Fog_of_war_mask_material_data[1] = nullptr;
    Fog_of_war_mask_material_handle[0] = 0xFFFFFFFF;
    Fog_of_war_mask_material_handle[1] = 0xFFFFFFFF;
}

// 0x0083F7F0 (:804)
void fog_of_war_clear()
{
    Fow_bit_array.clear(true);
}

// 0x0083F800 (:813): clears the cells within radius of center, testing two points per 16 m step of a square twice the
// radius on each side.
void fog_of_war_update(const vector& center, float radius)
{
    if (!center.is_valid())
    {
        return;
    }
    const float radius_sqr = radius * radius;
    const float reach = radius * 2.0f;
    for (float x = center.x - reach; x <= center.x + reach; x += Fow_cell_size)
    {
        for (float z = center.z - reach; z <= center.z + reach; z += Fow_cell_size)
        {
            // Magic number: the second point is half a cell along.
            const vector points[2] = {vector(x, 0.0f, z), vector(x + 8.0f, 0.0f, z + 8.0f)};
            for (const vector& point : points)
            {
                const float dx = center.x - point.x;
                const float dz = center.z - point.z;
                if (dx * dx + dz * dz <= radius_sqr)
                {
                    const int bit = fow_get_bit_index(point);
                    if (bit != -1)
                    {
                        Fow_bit_array.set(bit, 0);
                    }
                }
            }
        }
    }
}

// 0x0083FAD0 (:874)
bool fog_of_war_is_index_posn_obscured(int x, int z)
{
    return Fow_bit_array.get(z * Fow_grid_size + x) == 1;
}

// 0x0083FAF0 (:887): outside the map is obscured
bool fog_of_war_is_posn_obscured(const vector& posn)
{
    const int bit = fow_get_bit_index(posn);
    return bit == -1 || Fow_bit_array.get(bit) == 1;
}

// 0x00867010 (:903)
void fog_of_war_render(const bbox& viewing_area, const bbox& render_area)
{
    if (!Fog_of_war_render)
    {
        return;
    }
    Curr_time = timer_get(TIMER_MICROSECONDS);
    if (Start_time == 0)
    {
        Start_time = Curr_time;
    }
    const float elapsed = (float)(Curr_time - Start_time) * 1e-06f;
    Fow_alpha = Fow_color_alpha;
    // Magic number: half a second to fade in. rfg.exe computes this and does not use it.
    if (elapsed < 0.5f)
    {
        Fow_alpha = (1.0f - (0.5f - elapsed) * 2.0f) * Fow_color_alpha;
    }
    Background_bitmap_id = -1;
    const int image_id = vint_bms_load("ui_fsmap_cracks");
    Crack_image_info = vint_bms_get_image_data(image_id);
    Crack_bitmap_id = vint_bms_get_bitmap(image_id);
    fog_of_war_render_fog_layer(render_area, viewing_area);
}

// 0x0087A5B0 (:986)
void fog_of_war_rendering_alloc(rl_renderer& renderer)
{
    Curr_time = 0;
    Start_time = 0;
    Fow_alpha = 0.0f;
    Bbox_memory = nullptr;
    My_mark = (int)Interface_mempool.mark();
    My_end_mark = My_mark;
    fog_of_war_allocate_mask_texture();
    fog_of_war_update_mask_texture(renderer);
    Fow_layer2_offset.x = 1.0f;
    Fow_layer2_offset.y = 1.0f;
    fog_of_war_create_mask_material(&Fog_of_war_mask_materials[0], &Fog_of_war_mask_material_data[0], &Fog_of_war_mask_material_handle[0]);
    Fow_layer2_offset.x = 1.0f;
    Fow_layer2_offset.y = 1.0f;
    fog_of_war_create_mask_material(&Fog_of_war_mask_materials[1], &Fog_of_war_mask_material_data[1], &Fog_of_war_mask_material_handle[1]);
    Current_fow_material = false;
}

// 0x0083FB80 (:1014)
void fog_of_war_rendering_dealloc()
{
    for (int i = 0; i < 2; i++)
    {
        if (Fog_of_war_mask_materials[i] != nullptr)
        {
            rl_material_library_unregister(Fog_of_war_mask_materials[i]);
            Fog_of_war_mask_materials[i]->release();
            Fog_of_war_mask_materials[i] = nullptr;
            Fog_of_war_mask_material_handle[i] = 0xFFFFFFFF;
            Fog_of_war_mask_material_data[i] = nullptr;
        }
    }
    if (Fog_of_war_mask_texture != nullptr)
    {
        Fog_of_war_mask_texture->release();
        Fog_of_war_mask_texture = nullptr;
    }
    mem_manager_free_requested_memory("fow_memory");
    mem_manager_free_requested_memory("fow_material1");
    mem_manager_free_requested_memory("fow_material2");
}

// 0x0083FC30 (:1048): a header (the sizes, which the load skips), the bits as bytes, an end mark
void fog_of_war_save(cfile* file)
{
    // Magic numbers: the header's fields as rfg.exe writes them.
    cf_write_uint16(0x10, file);
    cf_write_uint16(0x400, file);
    cf_write_uint32(Fow_num_bits, file);
    cf_write_uint32(Fow_num_bits, file);
    cf_write_uint32(Fow_num_bits / 8, file);
    for (int first_bit = 0; first_bit < Fow_num_bits; first_bit += 8)
    {
        unsigned char value = 0;
        for (int bit = 0; bit < 8; bit++)
        {
            value |= (unsigned char)(Fow_bit_array.get(first_bit + bit) << bit);
        }
        cf_write_uint8(value, file);
    }
    cf_write_int32(-1, file);
}

// 0x0083FCC0 (:1088)
void fog_of_war_load(cfile* file)
{
    cf_read_uint16(file);
    cf_read_uint16(file);
    cf_read_uint32(file);
    cf_read_uint32(file);
    cf_read_uint32(file);
    Fow_bit_array.clear(true);
    for (int first_bit = 0; first_bit < Fow_num_bits; first_bit += 8)
    {
        const unsigned char value = cf_read_uint8(file);
        for (int bit = 0; bit < 8; bit++)
        {
            Fow_bit_array.set(first_bit + bit, (value >> bit) & 1);
        }
    }
    cf_read_int32(file);
}
