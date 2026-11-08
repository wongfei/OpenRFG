// Original: volition/render_lib/code/rl_render_lib.cpp
//
// rl_render_lib: the keen graphics device layer of render_lib. Texture creation from pegs and dynamic/render targets,
// the precreated blend, depth/stencil, rasterizer and sampler states of the primitive macro states, the .fxo_kg shader
// loader and the screen and level-of-detail settings.
//
// VFX.md FX7: rl_project_get_bbox_2d_rect_ex, its robust and clipped forms (the decal renderers' scissor rectangles).

#include "volition/render_lib/code/rl_render_lib.h"

#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/render_lib/code/rl_utility.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/vector4.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_morph_weights.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_renderer_metrics.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/rl_scene_renderer.h"
#include "volition/render_lib/code/rl_shader_defines.h"
#include "volition/render_lib/code/rl_shader_fixup.h"
#include "volition/render_lib/code/rl_vertex_formats.hpp"
#include "volition/vlib/code/bmpman/bmpman_defs.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/string_util.h"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/filesystem.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/pixelformat.hpp"
#include "keen/graphics/rendertarget.hpp"
#include "keen/graphics/texturedata.hpp"

#include <float.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

RL_DYNAMIC_CLASS_INFO(rl_render_lib)

keen::MemoryAllocator* g_pVramAllocator = nullptr; // 0x01B6AAF4
keen::MemoryAllocator* g_pFramebufferAllocator = nullptr; // 0x01B7BC0C

const int rl_render_lib::kNumEngineFillModes = 2; // 0x012B8084
const int rl_render_lib::kNumEngineCullModes = 3; // 0x012B8088
const int rl_render_lib::kNumEngineScissorModes = 2; // 0x012B808C
const int rl_render_lib::kNumEngineMsaaModes = 2; // 0x012B8090
const unsigned int rl_render_lib::kDepthBiasLevels = 5; // 0x012B8094
const unsigned int rl_render_lib::kNumUAddressModes = 2; // 0x012B8098
const unsigned int rl_render_lib::kNumVAddressModes = 2; // 0x012B809C
const unsigned int rl_render_lib::kNumWAddressModes = 2; // 0x012B80A0

// The clamped variants of the shaders' samplers, indexed by get_clamped_sampler_handle. Only the handles listed in
// rl_Clamped_sampler_states_to_create are created.
rl_dev_sampler_state rl_Clamped_sampler_states[5184]; // 0x01C27400
// Magic number: the one clamped sampler the shipped shaders need (linear, linear, nearest mip; U wrap, V and W clamp).
unsigned short rl_Clamped_sampler_states_to_create[1] = { 0x0550 }; // 0x014FA450

// Texture filter modes of the shader fixups (rl_shader_tex_fixup::min/mag/mip_filter) as keen modes. Index 5 is the
// shadow-compare filter, rewritten by processTexFixups.
static const keen::SamplerFilterMode Texture_filter_mode_mapping_keen[7] = // 0x014FA3DC
{
    keen::SamplerFilterMode_Linear,
    keen::SamplerFilterMode_Nearest,
    keen::SamplerFilterMode_Nearest,
    keen::SamplerFilterMode_Linear,
    keen::SamplerFilterMode_Anisotropic,
    keen::SamplerFilterMode_Invalid,
    keen::SamplerFilterMode_Linear,
};

// Texture address modes of the shader fixups as keen modes.
static const keen::SamplerAddressMode Texture_address_mode_mapping_keen[7] = // 0x014FA3F8
{
    keen::SamplerAddressMode_Wrap,
    keen::SamplerAddressMode_Wrap,
    keen::SamplerAddressMode_Mirror,
    keen::SamplerAddressMode_Clamp,
    keen::SamplerAddressMode_Border,
    keen::SamplerAddressMode_Wrap,
    keen::SamplerAddressMode_Wrap,
};

// size 0xC
struct Depth_bias_settings
{
    float depth_bias; // 0x00
    float depth_bias_slope_scale; // 0x04
    float depth_bias_clamp; // 0x08
};

// The depth bias of each rl_prim_zbias_mode.
static const Depth_bias_settings Depth_bias_mapping[5] = // 0x014FA414
{
    { 0.0f, 0.0f, FLT_MAX }, // RLPZB_NONE
    { 0.0f, -2.0f, FLT_MAX }, // RLPZB_DECAL
    { 1.0f, 4.0f, FLT_MAX }, // RLPZB_SHADOW
    { 0.0f, -2.0f, FLT_MAX }, // RLPZB_DISTORTION
    { 0.0f, 0.0f, FLT_MAX }, // RLPZB_TERRAIN_LOW_LOD
};

static bool g_tintLegacyTextures = false; // 0x01B7BCEF

// 0x004C5D20 (:36)
bool rl_render_lib::can_create()
{
    return true;
}

// 0x0054D3B0 (:36)
rl_render_lib* rl_render_lib::create()
{
    rl_render_lib* render_lib_p = new rl_render_lib();
    render_lib_p->set_leaf_class_id(0xFFFFFFFF);
    ms_class_dynamic_info.m_objects_allocated++;
    if (ms_class_dynamic_info.m_objects_peak < ms_class_dynamic_info.m_objects_allocated)
    {
        ms_class_dynamic_info.m_objects_peak = ms_class_dynamic_info.m_objects_allocated;
    }
    return render_lib_p;
}

// 0x004C5D30 (:36)
void rl_render_lib::release()
{
    ms_class_dynamic_info.m_objects_allocated--;
    if (this != nullptr)
    {
        delete this;
    }
}

// 0x00516C30 (:40)
void unused_dcf_refresh_rasterizer_states()
{
    rl_render_lib* render_lib_p = rl_render_lib::get_instance();
    render_lib_p->destroy_engine_rasterizer_states();
    render_lib_p->initialize_engine_rasterizer_states();
}

// 0x004F3250 (:157)
void build_screen_info(keen::GraphicsSystem* pGraphicsSystem, rl_screen_info& info)
{
    info.set_window_mode(keen::graphics::getCurrentWindowMode(pGraphicsSystem, nullptr));
    info.set_screen_height((int)keen::graphics::getRenderTargetHeight(keen::graphics::getBackBufferRenderTarget(pGraphicsSystem)));
    info.set_screen_width((int)keen::graphics::getRenderTargetWidth(keen::graphics::getBackBufferRenderTarget(pGraphicsSystem)));
}

// 0x004C5D50 (:172)
keen::PixelFormat keen_format_from_dev_format(rl_dev_format format)
{
    switch (format)
    {
    case RL_FORMAT_BC1_UNORM_SRGB:
        return keen::PixelFormat_Bc1_Gamma;
    case RL_FORMAT_BC1_UNORM:
        return keen::PixelFormat_Bc1;
    case RL_FORMAT_BC2_UNORM_SRGB:
        return keen::PixelFormat_Bc2_Gamma;
    case RL_FORMAT_BC2_UNORM:
        return keen::PixelFormat_Bc2;
    case RL_FORMAT_BC3_UNORM_SRGB:
        return keen::PixelFormat_Bc3_Gamma;
    case RL_FORMAT_BC3_UNORM:
        return keen::PixelFormat_Bc3;
    case RL_FORMAT_R8G8B8A8_UNORM_SRGB:
    case RL_FORMAT_B8G8R8A8_UNORM_SRGB:
    case RL_FORMAT_LDR:
    case RL_FORMAT_BLEND_LDR:
        return keen::PixelFormat_Native_RGBA32_Gamma;
    case RL_FORMAT_R8G8B8A8_UNORM:
    case RL_FORMAT_B8G8R8A8_UNORM:
        return keen::PixelFormat_Native_RGBA32;
    case RL_FORMAT_B5G6R5_UNORM:
    case RL_FORMAT_B5G6R5_UNORM_SRGB:
        return keen::PixelFormat_R5G6B5;
    case RL_FORMAT_A8_UNORM:
        return keen::PixelFormat_A8;
    case RL_FORMAT_R8_UNORM:
        return keen::PixelFormat_L8;
    case RL_FORMAT_R32G32B32A32_FLOAT:
        return keen::PixelFormat_A32B32G32R32F;
    case RL_FORMAT_R16G16B16A16_FLOAT:
    case RL_FORMAT_R16G16B16A16_SNORM:
        return keen::PixelFormat_A16B16G16R16F;
    case RL_FORMAT_R32_FLOAT:
    case RL_FORMAT_DEPTH_BLEND_FLOAT:
        return keen::PixelFormat_R32F;
    case RL_FORMAT_HDR:
        return keen::PixelFormat_R11G11B10F;
    case RL_FORMAT_SHADOW_MAP:
        return keen::PixelFormat_Depth16;
    case RL_FORMAT_DEPTH_TEXTURE:
        return keen::PixelFormat_Depth32F;
    case RL_FORMAT_R8G8_UNORM:
        return keen::PixelFormat_R8G8;
    default:
        return keen::PixelFormat_Count;
    }
}

// 0x004C5E50 (:211)
unsigned char keen_access_from_dev_access_and_usage(rl_dev_access access, rl_dev_usage usage)
{
    if (access == RL_ACCESS_WRITE_ONLY)
    {
        return keen::TextureAccessMode_Write;
    }
    if (access == RL_ACCESS_READ_WRITE)
    {
        return keen::TextureAccessMode_ReadWrite;
    }
    return keen::TextureAccessMode_None;
}

// 0x004C5E70 (:226)
// Drops the mip levels smaller than the format's block.
// Notes (decompiled behaviour, kept as is): BC2 is missing from the 4x4 block formats, and A16B16G16R16F is treated as
// having 2x2 blocks.
void fixup_level_count(keen::TextureDescription& desc)
{
    unsigned int min_size = 1;
    switch (desc.format)
    {
    case keen::PixelFormat_Bc1:
    case keen::PixelFormat_Bc1_Gamma:
    case keen::PixelFormat_Bc3:
    case keen::PixelFormat_Bc3_Gamma:
        min_size = 4;
        break;
    case keen::PixelFormat_A16B16G16R16F:
        min_size = 2;
        break;
    }
    unsigned int width = desc.width;
    unsigned int height = desc.height;
    unsigned char level_count = 0;
    while (level_count < desc.levelCount && width >= min_size && height >= min_size)
    {
        level_count++;
        width >>= 1;
        height >>= 1;
    }
    desc.levelCount = level_count;
}

// 0x004C5F10 (:262)
// LTCG: peg_fmt arrives in EAX and srgb in CL.
static rl_dev_format dev_format_from_peg_format(const int peg_fmt, const bool srgb)
{
    // Magic numbers: the peg bitmap formats (bm_fmt).
    switch (peg_fmt)
    {
    case 400: // DXT1
        return srgb ? RL_FORMAT_BC1_UNORM_SRGB : RL_FORMAT_BC1_UNORM;
    case 401: // DXT3
        return srgb ? RL_FORMAT_BC2_UNORM_SRGB : RL_FORMAT_BC2_UNORM;
    case 402: // DXT5
        return srgb ? RL_FORMAT_BC3_UNORM_SRGB : RL_FORMAT_BC3_UNORM;
    case 403: // R5G6B5
        return srgb ? RL_FORMAT_B5G6R5_UNORM_SRGB : RL_FORMAT_B5G6R5_UNORM;
    case 407: // A8R8G8B8
        return srgb ? RL_FORMAT_B8G8R8A8_UNORM_SRGB : RL_FORMAT_B8G8R8A8_UNORM;
    case 408: // V8U8
        return RL_FORMAT_R8G8_SNORM;
    default:
        VLIB_CRITICAL_FAILURE();
        return RL_FORMAT_UNKNOWN;
    }
}

// 0x004C5FC0 (:311)
DXGI_FORMAT d3d_format_from_dev_format(rl_dev_format dev_format)
{
    switch (dev_format)
    {
    case RL_FORMAT_UNKNOWN:
        return DXGI_FORMAT_UNKNOWN;
    case RL_FORMAT_BC1_UNORM_SRGB:
        return DXGI_FORMAT_BC1_UNORM_SRGB;
    case RL_FORMAT_BC1_UNORM:
        return DXGI_FORMAT_BC1_UNORM;
    case RL_FORMAT_BC2_UNORM_SRGB:
        return DXGI_FORMAT_BC2_UNORM_SRGB;
    case RL_FORMAT_BC2_UNORM:
        return DXGI_FORMAT_BC2_UNORM;
    case RL_FORMAT_BC3_UNORM_SRGB:
        return DXGI_FORMAT_BC3_UNORM_SRGB;
    case RL_FORMAT_BC3_UNORM:
        return DXGI_FORMAT_BC3_UNORM;
    case RL_FORMAT_R8G8B8A8_UNORM_SRGB:
    case RL_FORMAT_LDR:
    case RL_FORMAT_BLEND_LDR:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case RL_FORMAT_R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case RL_FORMAT_R8G8_SNORM:
        return DXGI_FORMAT_R8G8_SNORM;
    case RL_FORMAT_B8G8R8A8_UNORM:
    case RL_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case RL_FORMAT_B5G6R5_UNORM:
    case RL_FORMAT_B5G6R5_UNORM_SRGB:
        return DXGI_FORMAT_B5G6R5_UNORM;
    case RL_FORMAT_A8_UNORM:
        return DXGI_FORMAT_A8_UNORM;
    case RL_FORMAT_R8_UNORM:
        return DXGI_FORMAT_R8_UNORM;
    case RL_FORMAT_R32G32B32A32_FLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case RL_FORMAT_R16G16B16A16_FLOAT:
    case RL_FORMAT_HDR:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case RL_FORMAT_R32G32_FLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case RL_FORMAT_R16G16_FLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case RL_FORMAT_R32_FLOAT:
    case RL_FORMAT_DEPTH_BLEND_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case RL_FORMAT_R16_FLOAT:
        return DXGI_FORMAT_R16_FLOAT;
    case RL_FORMAT_SHADOW_MAP:
    case RL_FORMAT_DEPTH_TEXTURE:
        return DXGI_FORMAT_R32_TYPELESS;
    case RL_FORMAT_R16G16B16A16_SNORM:
        return DXGI_FORMAT_R16G16B16A16_SNORM;
    default:
        VLIB_CRITICAL_FAILURE();
        return DXGI_FORMAT_UNKNOWN;
    }
}

// 0x004C60E0 (:350)
// pitch is the row pitch in bytes (rows of 4x4 blocks for the compressed formats), pitch_rows the row count and
// min_pitch the size of one texel or block.
void get_pitch(DXGI_FORMAT d3d_fmt, int w, int h, int& pitch, int& pitch_rows, int& min_pitch)
{
    pitch_rows = h;
    switch (d3d_fmt)
    {
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        min_pitch = 16;
        pitch = w * 16;
        return;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
        min_pitch = 8;
        pitch = w * 8;
        return;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        min_pitch = 4;
        pitch = w * 4;
        return;
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
        min_pitch = 2;
        pitch = w * 2;
        return;
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_A8_UNORM:
        min_pitch = 1;
        pitch = w;
        return;
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        // 8 bytes per 4x4 block: w / 4 * 8.
        pitch = w * 2;
        pitch_rows = h >> 2;
        min_pitch = 8;
        return;
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
        // 16 bytes per 4x4 block: w / 4 * 16.
        pitch = w * 4;
        pitch_rows = h >> 2;
        min_pitch = 16;
        return;
    default:
        VLIB_CRITICAL_FAILURE();
        return;
    }
}

// 0x004F32D0 (:424)
// Expands every mip of every side of a 565 texture to 8888 (alpha 255). rgba selects the byte order of the output:
// R, G, B, A when set, B, G, R, A otherwise. The caller frees the result with delete[].
unsigned char* convert_565_to_8888(const int width, const int height, const int mip_levels, const int sides, const void* const data_p, int pitch, bool rgba)
{
    const int side_height = height / sides;
    int side_size = 0;
    int level_width = width;
    int level_height = side_height;
    for (int level = 0; level < mip_levels; level++)
    {
        side_size += level_height * level_width * 4;
        level_width = level_width >> 1 < 2 ? 1 : level_width >> 1;
        level_height = level_height >> 1 < 2 ? 1 : level_height >> 1;
    }
    unsigned char* result_p = new unsigned char[side_size * sides];
    const unsigned char* src_p = (const unsigned char*)data_p;
    unsigned char* dest_p = result_p;
    const int red_offset = rgba ? 0 : 2;
    const int blue_offset = rgba ? 2 : 0;
    for (int side = 0; side < sides; side++)
    {
        int level_pitch = pitch;
        level_width = width;
        level_height = side_height;
        for (int level = 0; level < mip_levels; level++)
        {
            for (int y = 0; y < level_height; y++)
            {
                const unsigned short* row_p = (const unsigned short*)(src_p + y * level_pitch);
                unsigned char* out_p = dest_p + y * level_width * 4;
                for (int x = 0; x < level_width; x++)
                {
                    const unsigned short texel = row_p[x];
                    out_p[red_offset] = (unsigned char)((texel >> 8) & 0xF8);
                    out_p[1] = (unsigned char)((texel >> 5) << 2);
                    out_p[blue_offset] = (unsigned char)(texel << 3);
                    out_p[3] = 0xFF;
                    out_p += 4;
                }
            }
            src_p += level_pitch * level_height;
            dest_p += level_width * 4 * level_height;
            level_width = level_width >> 1 < 2 ? 1 : level_width >> 1;
            level_height = level_height >> 1 < 2 ? 1 : level_height >> 1;
            // The pitch of the smaller mips is the 4-byte aligned row size.
            level_pitch = (level_width * 2 + 3) & ~3;
        }
    }
    return result_p;
}

// 0x004F3490 (:548)
// Creates the keen texture for desc in format. Without data the texture is allocated empty (render or dynamic
// textures); B8G8R8A8 is uploaded as R8G8B8A8 and 565 data is converted to 8888 first.
keen::TextureData* create_keen_texture_data(keen::GraphicsSystem* graphics_system_p, keen::TextureDescription& desc, const rl_dev_format format, const void* data_p, bool cpu_data, const char* debug_name_p)
{
    keen::TextureData* texture_p = new keen::TextureData();
    if (data_p == nullptr)
    {
        desc.format = (uint8)keen_format_from_dev_format(format);
        keen::graphics::allocateTextureData(texture_p, graphics_system_p, g_pVramAllocator, desc, nullptr, 0, debug_name_p);
        return texture_p;
    }
    rl_dev_format upload_format = format;
    const DXGI_FORMAT d3d_fmt = d3d_format_from_dev_format(format);
    int pitch = 0;
    int pitch_rows;
    int min_pitch = 0;
    get_pitch(d3d_fmt, desc.width, desc.height, pitch, pitch_rows, min_pitch);
    unsigned char* converted_p = nullptr;
    if (format == RL_FORMAT_B8G8R8A8_UNORM_SRGB || format == RL_FORMAT_B8G8R8A8_UNORM)
    {
        upload_format = format == RL_FORMAT_B8G8R8A8_UNORM_SRGB ? RL_FORMAT_R8G8B8A8_UNORM_SRGB : RL_FORMAT_R8G8B8A8_UNORM;
    }
    else if (format == RL_FORMAT_B5G6R5_UNORM_SRGB || format == RL_FORMAT_B5G6R5_UNORM)
    {
        // Magic number: a cube map converts its 6 sides.
        const int sides = desc.type == keen::TextureType_Cube ? 6 : 1;
        converted_p = convert_565_to_8888(desc.width, desc.height * sides, desc.levelCount, sides, data_p, pitch, true);
        data_p = converted_p;
        upload_format = format == RL_FORMAT_B5G6R5_UNORM_SRGB ? RL_FORMAT_R8G8B8A8_UNORM_SRGB : RL_FORMAT_R8G8B8A8_UNORM;
    }
    // Magic number: flag 0x8000 (set by load_texture for legacy pegs) tints the top level magenta and drops the mips.
    if ((desc.flags & 0x8000) != 0)
    {
        desc.flags &= 0x7FFF;
        const keen::PixelFormatInfo& info = keen::graphics::getPixelFormatInfo(keen_format_from_dev_format(upload_format));
        if (info.bitsPerPixel == 0)
        {
            const unsigned int block_bits = info.bitsPer4x4Block;
            if (block_bits == 0)
            {
                goto create;
            }
            const int block_count = (desc.width >> 2) * (desc.height >> 2);
            unsigned short* block_p = (unsigned short*)data_p;
            for (int i = 0; i < block_count; i++)
            {
                // Magic numbers: both endpoint colors of a BC1 block (or the color block of BC2/BC3) set to 0xF81F.
                if (info.bitsPer4x4Block == 64)
                {
                    block_p[0] = 0xF81F;
                    block_p[1] = 0xF81F;
                }
                else if (info.bitsPer4x4Block == 128)
                {
                    block_p[4] = 0xF81F;
                    block_p[5] = 0xF81F;
                }
                block_p = (unsigned short*)((unsigned char*)block_p + (block_bits >> 3));
            }
        }
        else
        {
            const unsigned int total_bits = (unsigned int)desc.width * desc.height * info.bitsPerPixel;
            if (info.bitsPerPixel == 16)
            {
                unsigned short* texel_p = (unsigned short*)data_p;
                for (unsigned int i = 0; i < (total_bits >> 5) * 2 + ((total_bits >> 5) & 1); i++)
                {
                    texel_p[i] = 0xF81F;
                }
            }
            else
            {
                // Notes (decompiled behaviour, kept as is): only a quarter of a 32-bit texture is tinted (bits / 64
                // texels instead of bits / 32).
                unsigned int* texel_p = (unsigned int*)data_p;
                for (unsigned int i = 0; i < (total_bits >> 6); i++)
                {
                    texel_p[i] = 0xFFFF00FF;
                }
            }
        }
        desc.levelCount = 1;
    }
create:
    desc.format = (uint8)keen_format_from_dev_format(upload_format);
    fixup_level_count(desc);
    keen::graphics::initializeTextureData(texture_p, graphics_system_p, desc, data_p, debug_name_p);
    delete[] converted_p;
    return texture_p;
}

// 0x004C6260 (:697)
void destroy_keen_texture_data(keen::GraphicsSystem* graphics_system_p, const rl_dev_texture& texture, keen::MemoryAllocator* pAllocator)
{
    keen::graphics::freeTextureData(texture.pData, graphics_system_p, pAllocator);
    delete texture.pData;
}

// 0x00543D90 (:703)
rl_render_lib::rl_render_lib()
{
    memset(m_prim_blend_states, 0, sizeof(m_prim_blend_states));
    m_prim_sampler_states[0] = rl_dev_sampler_state();
    m_prim_sampler_states[1] = rl_dev_sampler_state();
    memset(m_prim_depth_stencil_states, 0, sizeof(m_prim_depth_stencil_states));
    memset(m_engine_rasterizer_states, 0, sizeof(m_engine_rasterizer_states));
    memset(m_material_sampler_states, 0, sizeof(m_material_sampler_states));
    m_num_gpus = 1;
    m_current_frame_count = 0;
    // Magic numbers: only "no antialiasing" (one sample) is offered on this platform.
    m_msaa_level[0].m_samples = 1;
    m_msaa_level[0].m_quality = 0;
    m_msaa_level[0].m_enabled = 1;
    for (int i = 1; i < 7; i++)
    {
        m_msaa_level[i].m_samples = 0;
        m_msaa_level[i].m_quality = 0;
        m_msaa_level[i].m_enabled = 0;
    }
}

// 0x00516CF0 (:720)
rl_render_lib::~rl_render_lib()
{
    m_occlusionQueries.destroy(Init_params.m_pKeenGraphicsSystem, Init_params.m_pDefaultAllocator);
    destroy_primitive_macro_states();
    destroy_engine_rasterizer_states();
    for (int i = 0; i < 5184; i++)
    {
        if (rl_Clamped_sampler_states[i].m_internal_state_p != nullptr)
        {
            keen::graphics::destroySamplerState(Init_params.m_pKeenGraphicsSystem, rl_Clamped_sampler_states[i].m_internal_state_p);
        }
    }
    if (m_metrics_p != nullptr)
    {
        m_metrics_p->release();
    }
}

// 0x004C6290 (:741)
const unsigned int rl_render_lib::get_num_msaa_modes()
{
    unsigned int count = 0;
    for (int i = 0; i < 7; i++)
    {
        if (m_msaa_level[i].m_enabled != 0)
        {
            count++;
        }
    }
    return count;
}

// 0x004C62E0 (:752)
void rl_render_lib::get_msaa_modes(rl_antialias_lod* modes_p)
{
    static const rl_antialias_lod Lods[7] = { RL_AA_LOD_OFF, RL_AA_LOD_2X, RL_AA_LOD_4X, RL_AA_LOD_8X, RL_AA_LOD_16X, RL_AA_LOD_8XQ, RL_AA_LOD_16XQ };
    unsigned int count = 0;
    for (int i = 0; i < 7; i++)
    {
        if (m_msaa_level[i].m_enabled != 0)
        {
            modes_p[count++] = Lods[i];
        }
    }
}

// 0x00516DD0 (:766)
void rl_render_lib::initialize(const rl_init_params& params)
{
    m_screen_info = params.m_screen_info;
    m_last_width = m_screen_info.get_screen_width();
    m_last_height = m_screen_info.get_screen_height();
    g_pFramebufferAllocator = params.m_pFramebufferAllocator;
    g_pVramAllocator = params.m_pVramAllocator;
    m_screen_info.set_vsync_enabled(params.m_vsync_enabled != 0);
    keen::graphics::setSwapChainPresentationInterval(Init_params.m_pKeenGraphicsSystem, nullptr, params.m_vsync_enabled != 0 ? 1 : 0);
    m_new_level_of_detail = params.m_lod;
    validate_msaa_mode();
    m_level_of_detail = m_new_level_of_detail;
    // Magic number: 50 vertex formats.
    rl_create_vertex_formats(params.m_pKeenGraphicsSystem, m_vertex_formats_p, 50);
    initialize_engine_rasterizer_states();
    initialize_primitive_macro_states();
    memset(rl_Clamped_sampler_states, 0, sizeof(rl_Clamped_sampler_states));
    m_metrics_p = rl_renderer_metrics::create();
    // Magic number: 512 KB of shader headers, fixups and techniques.
    void* shader_memory_p = malloc(0x80000);
    if (shader_memory_p == nullptr)
    {
        // OpenRFG deviation: the message box is replaced by a log line.
        // MessageBoxA(nullptr, "Failed to initialize renderer.", "Error", 0);
        keen::debug::nativePrintArgs("[error] Failed to initialize renderer.\n");
        ExitProcess(0);
    }
    // Magic numbers: alignment 1, mempool flag 8.
    m_shader_base_mempool.init(shader_memory_p, 0x80000, "shader base mempool", 1, 8);
    m_lodChanged = false;
    m_sizesChanged = false;
    // Magic number: 2560 occlusion queries.
    m_occlusionQueries.create(params.m_pKeenGraphicsSystem, Init_params.m_pDefaultAllocator, 0xA00, params.m_occlusionQueryGPUMemory);
    m_inputBindingMutex.create("inputBindingMutex");
}

// 0x004C6360 (:821)
void rl_render_lib::next_frame()
{
    rl_morph_system_flipBuffers();
    m_current_frame_count++;
}

// 0x004F3730 (:827)
// Creates the keen texture of the bitmap's current peg entry from the peg data (cache[0] holds the data pointer until
// then, data.m_offset marks it consumed). Texture reduction skips the largest mips, except for alpha masks, fonts and
// UI.
bool rl_render_lib::load_texture(bitmap_entry& be)
{
    peg_entry* peg_p = be.current_peg_entry;
    const unsigned char* data_p = nullptr;
    if (peg_p->cache[0] != 0)
    {
        if (peg_p->data.get_offset() == 0)
        {
            data_p = (const unsigned char*)peg_p->cache[0];
            peg_p->cache[0] = 0;
            peg_p->data.set_offset(1);
        }
        if (peg_p->cache[0] != 0)
        {
            VLIB_CRITICAL_FAILURE();
            return true;
        }
    }
    const unsigned char mip_levels = peg_p->mip_levels;
    const unsigned short width = peg_p->width;
    const unsigned short height = peg_p->height;
    static const char* const Alpha_names[17] = { "alpha00", "alpha01", "alpha02", "alpha03", "alpha04", "alpha05", "alpha06", "alpha07", "alpha08", "alpha09", "alpha10", "alpha11", "alpha12", "alpha13", "alpha14", "alpha15", "alpha16" };
    for (int i = 0; i < 17; i++)
    {
        if (strstr(be.filename_ptr, Alpha_names[i]) != nullptr)
        {
            // Magic number: peg flag 0x200 marks a linear (alpha mask) texture.
            peg_p->flags |= 0x200;
            break;
        }
    }
    const rl_dev_format format = dev_format_from_peg_format(peg_p->bm_fmt, (peg_p->flags & 0x200) == 0);
    keen::TextureDescription desc;
    // Magic number: peg flag 8 marks a cube map, stored as 6 sides one above the other.
    if (((peg_p->flags >> 3) & 1) == 0)
    {
        desc.type = keen::TextureType_2d;
        desc.height = height;
    }
    else
    {
        desc.type = keen::TextureType_Cube;
        desc.height = (unsigned short)((int)height / 6);
    }
    desc.depth = 1;
    desc.cpuAccessMode = keen::TextureAccessMode_None;
    desc.width = width;
    desc.levelCount = mip_levels;
    if ((be.current_peg_entry->source_height.combined == 0 || be.current_peg_entry->source_width.combined == 0) && g_tintLegacyTextures)
    {
        desc.flags = 0x8001;
        keen::debug::nativePrintArgs("'%s' most likely legacy.\n", be.filename_ptr);
    }
    if (m_new_level_of_detail.m_new_data.m_texture_reduction != 0 && (peg_p->flags & 0x200) == 0 && keen::findString(be.filename_ptr, "font") == nullptr && keen::findString(be.filename_ptr, "ui_") == nullptr)
    {
        const keen::PixelFormat keen_format = keen_format_from_dev_format(format);
        const unsigned int bits_per_pixel = keen::graphics::getBitsPerPixel(keen_format);
        const unsigned int bits_per_block = bits_per_pixel == 0 ? keen::graphics::getPixelFormatInfo(keen_format).bitsPer4x4Block : 0;
        for (unsigned int i = m_new_level_of_detail.m_new_data.m_texture_reduction; i != 0; i--)
        {
            if (desc.levelCount > 1)
            {
                const unsigned int level_bits = bits_per_pixel == 0 ? (unsigned int)(desc.height >> 2) * (desc.width >> 2) * bits_per_block : (unsigned int)desc.height * desc.width * bits_per_pixel;
                desc.width >>= 1;
                desc.height >>= 1;
                desc.levelCount--;
                data_p += level_bits >> 3;
            }
        }
    }
    keen::TextureData* texture_p = create_keen_texture_data(Init_params.m_pKeenGraphicsSystem, desc, format, data_p, false, be.filename_ptr);
    peg_p->cache[0] = (unsigned int)texture_p;
    return true;
}

// 0x004C6390 (:959)
bool rl_render_lib::unload_texture(bitmap_entry& be)
{
    peg_entry* peg_p = be.current_peg_entry;
    keen::graphics::freeTextureData((keen::TextureData*)peg_p->cache[0], Init_params.m_pKeenGraphicsSystem, nullptr);
    delete (keen::TextureData*)peg_p->cache[0];
    return true;
}

// 0x004C63C0 (:969)
void rl_render_lib::set_vsync_enabled(bool enabled)
{
    m_screen_info.set_vsync_enabled(enabled);
    keen::graphics::setSwapChainPresentationInterval(Init_params.m_pKeenGraphicsSystem, nullptr, enabled ? 1 : 0);
}

// 0x004C63F0 (:979)
bool rl_render_lib::get_vsync_enabled() const
{
    return m_screen_info.get_vsync_enabled();
}

// 0x00516FB0 (:984)
// Converts the file's texture fixups to runtime fixups with a sampler state each, and creates the clamped variants.
void rl_render_lib::processTexFixups(rl_shader_header_runtime* shader_runtime, const rl_shader_tex_fixup* pFixups, unsigned int numTex)
{
    for (unsigned int i = 0; i < numTex; i++)
    {
        const rl_shader_tex_fixup& src = pFixups[i];
        rl_shader_tex_fixup_runtime* fixup = &shader_runtime->tex_fixups[i];
        fixup->crc_value = src.crc_value;
        fixup->constLocData.const_loc = src.constLocData.const_loc_disk;
        fixup->constLocData.border_color = src.colorData.disc.border_color_disk;
        fixup->flags = src.flags;
        fixup->address_u = src.address_u;
        fixup->address_v = src.address_v;
        fixup->address_w = src.address_w;
        fixup->min_filter = src.min_filter;
        fixup->mag_filter = src.mag_filter;
        fixup->mip_filter = src.mip_filter;
        keen::SamplerParameters parameters;
        parameters.addressU = Texture_address_mode_mapping_keen[fixup->address_u];
        parameters.addressV = Texture_address_mode_mapping_keen[fixup->address_v];
        parameters.addressW = Texture_address_mode_mapping_keen[fixup->address_w];
        // Magic number: filter 5 is the shadow map compare filter (linear with a less-equal comparison).
        if (fixup->min_filter == 5 || fixup->mag_filter == 5)
        {
            fixup->min_filter = 3;
            fixup->mag_filter = 3;
            parameters.comparisonFunction = keen::ComparisonFunction_LessEqual;
        }
        parameters.minFilter = Texture_filter_mode_mapping_keen[fixup->min_filter];
        parameters.magFilter = Texture_filter_mode_mapping_keen[fixup->mag_filter];
        parameters.mipFilter = Texture_filter_mode_mapping_keen[fixup->mip_filter];
        // Magic numbers: anisotropy 2, 4 and 8 for the low, medium and high filtering levels.
        switch (m_level_of_detail.m_new_data.m_anisotropic_filtering)
        {
        case RL_AF_LOD_LOW:
            parameters.maxAnisotropyLevel = 2;
            break;
        case RL_AF_LOD_MED:
            parameters.maxAnisotropyLevel = 4;
            break;
        case RL_AF_LOD_HIGH:
            parameters.maxAnisotropyLevel = 8;
            break;
        default:
            parameters.maxAnisotropyLevel = 1;
            break;
        }
        parameters.minLod = 0.0f;
        parameters.mipLodBias = 0.0f;
        fixup->colorData.internal_sampler_state_p = keen::graphics::createSamplerState(Init_params.m_pKeenGraphicsSystem, parameters);
        // Magic numbers: every combination of clamped U (1), V (2) and W (4).
        create_and_cache_clamped_sampler_state(*fixup, 1);
        create_and_cache_clamped_sampler_state(*fixup, 2);
        create_and_cache_clamped_sampler_state(*fixup, 4);
        create_and_cache_clamped_sampler_state(*fixup, 3);
        create_and_cache_clamped_sampler_state(*fixup, 5);
        create_and_cache_clamped_sampler_state(*fixup, 6);
        create_and_cache_clamped_sampler_state(*fixup, 7);
    }
}

// 0x00543FA0 (:1084)
// Loads a compiled shader (.fxo_kg, render_minimal.md 2.1). The header and tables are copied to the shader mempool;
// every vertex and pixel shader blob (raw DXBC, 16-byte aligned after the tables) is mounted as a read-only memory file
// at /shaders/cache/<name> and created through keen. A pipeline and an input binding cache (one slot per vertex format)
// are created per technique.
rl_shader_header_runtime* rl_render_lib::load_shader(const char* const shader_fname)
{
    cfile* p_cfile = cf_open(shader_fname, "rb", PLATFORM_PC, false);
    if (p_cfile == nullptr)
    {
        return nullptr;
    }
    const unsigned int file_size = cf_size(p_cfile);
    unsigned char* file_p = (unsigned char*)malloc(file_size);
    cf_read(file_p, file_size, p_cfile, false);
    cf_close(p_cfile);

    const rl_shader_header* header_p = (const rl_shader_header*)file_p;
    const char* error_p;
    // Notes (decompiled behaviour, kept as is): the space check does not match the sizes allocated below (it counts the
    // texture fixups twice at 8 bytes and the techniques at 12), and the file buffer leaks on every error.
    const unsigned int check_size = (header_p->num_bool_fixups + header_p->num_tex_fixups * 2 + header_p->num_const_fixups) * 8 + sizeof(rl_shader_header) + (header_p->num_pixel_shaders + header_p->num_vertex_shaders * 2) * 4 + header_p->num_techniques * 12;
    if (!m_shader_base_mempool.can_alloc(check_size, 0))
    {
        error_p = "Shader base mempool size needs to be increased! Could not load (%s).";
    }
    // Magic numbers: the .fxo_kg signature and version.
    else if (header_p->signature != 0x4B42A1EE)
    {
        error_p = "%s is not a shader file!";
    }
    else if (header_p->version != 7)
    {
        error_p = "%s is an out-of-date shader version, it needs to be recrunched.";
    }
    else
    {
        // The tables follow the header: texture fixups, constant fixups, bool fixups, vertex shaders, pixel shaders,
        // techniques.
        unsigned int offset = sizeof(rl_shader_header);
        const rl_shader_tex_fixup* tex_fixups_p = (const rl_shader_tex_fixup*)(file_p + offset);
        offset += header_p->num_tex_fixups * sizeof(rl_shader_tex_fixup);
        const rl_shader_const_fixup* const_fixups_p = (const rl_shader_const_fixup*)(file_p + offset);
        offset += header_p->num_const_fixups * sizeof(rl_shader_const_fixup);
        const rl_shader_const_fixup* bool_fixups_p = (const rl_shader_const_fixup*)(file_p + offset);
        offset += header_p->num_bool_fixups * sizeof(rl_shader_const_fixup);
        // Each vertex shader entry starts with its blob size, each pixel shader entry is its blob size.
        const unsigned int* vertex_shaders_p = (const unsigned int*)(file_p + offset);
        offset += header_p->num_vertex_shaders * sizeof(rl_vertex_shader);
        const unsigned int* pixel_shaders_p = (const unsigned int*)(file_p + offset);
        offset += header_p->num_pixel_shaders * sizeof(rl_pixel_shader);
        const rl_shader_technique* techniques_p = (const rl_shader_technique*)(file_p + offset);
        offset += header_p->num_techniques * sizeof(rl_shader_technique);

        rl_shader_header_runtime* shader_runtime = (rl_shader_header_runtime*)m_shader_base_mempool.alloc(sizeof(rl_shader_header_runtime), 0);
        if (shader_runtime == nullptr)
        {
            return nullptr;
        }
        shader_runtime->vertex_shaders = (rl_dev_vertex_shader*)m_shader_base_mempool.alloc(header_p->num_vertex_shaders * sizeof(rl_dev_vertex_shader), 0);
        shader_runtime->pixel_shaders = (rl_dev_pixel_shader*)m_shader_base_mempool.alloc(header_p->num_pixel_shaders * sizeof(rl_dev_pixel_shader), 0);
        shader_runtime->tex_fixups = (rl_shader_tex_fixup_runtime*)m_shader_base_mempool.alloc(header_p->num_tex_fixups * sizeof(rl_shader_tex_fixup_runtime), 0);
        shader_runtime->const_fixups = (rl_shader_const_fixup*)m_shader_base_mempool.alloc(header_p->num_const_fixups * sizeof(rl_shader_const_fixup), 0);
        shader_runtime->bool_fixups = (rl_shader_const_fixup*)m_shader_base_mempool.alloc(header_p->num_bool_fixups * sizeof(rl_shader_const_fixup), 0);
        shader_runtime->shader_techniques = (rl_shader_technique_runtime*)m_shader_base_mempool.alloc(header_p->num_techniques * sizeof(rl_shader_technique_runtime), 0);
        memcpy(shader_runtime->const_fixups, const_fixups_p, header_p->num_const_fixups * sizeof(rl_shader_const_fixup));
        memcpy(shader_runtime->bool_fixups, bool_fixups_p, header_p->num_bool_fixups * sizeof(rl_shader_const_fixup));
        shader_runtime->signature = header_p->signature;
        shader_runtime->version = header_p->version;
        shader_runtime->shader_flags = header_p->shader_flags;
        shader_runtime->num_tex_fixups = header_p->num_tex_fixups;
        shader_runtime->num_bool_fixups = header_p->num_bool_fixups;
        shader_runtime->num_const_fixups = header_p->num_const_fixups;
        shader_runtime->num_pixel_shaders = header_p->num_pixel_shaders;
        shader_runtime->num_vertex_shaders = header_p->num_vertex_shaders;
        shader_runtime->base_technique_index = header_p->base_technique_index;
        shader_runtime->lighting_technique_index = header_p->lighting_technique_index;
        shader_runtime->depth_only_technique_index = header_p->depth_only_technique_index;
        shader_runtime->user_technique_start_index = header_p->user_technique_start_index;
        shader_runtime->num_techniques = header_p->num_techniques;

        for (int i = 0; i < header_p->num_vertex_shaders; i++)
        {
            const unsigned int blob_size = vertex_shaders_p[i * 2];
            if (blob_size != 0)
            {
                offset = (offset + 15) & ~15u;
                keen::FilePath path("/shaders/cache/", true);
                path.setFileNameWithExtension(shader_fname);
                // Deviation (bug_blood_decals.md, rl_shader_fixup.h): the decal vertex shaders that use the UNORM vertex
                // normal without decoding it, and the sky dome shader with the apex NaN, are created from a patched copy
                // of the blob.
                unsigned int patched_size;
                unsigned int num_reads;
                const char* fix;
                unsigned char* patched_p = rl_shader_fixup_vertex_shader(shader_fname, file_p + offset, blob_size, &patched_size, &num_reads, &fix);
                const keen::IoResult<uint32> mount = patched_p != nullptr
                                                         ? keen::file::mountReadOnlyMemoryFile(Init_params.m_pFileSystem, path.getCompletePath(), patched_p, patched_size)
                                                         : keen::file::mountReadOnlyMemoryFile(Init_params.m_pFileSystem, path.getCompletePath(), file_p + offset, blob_size);
                shader_runtime->vertex_shaders[i].m_internal_state_p = keen::graphics::createVertexShaderFromFile(Init_params.m_pKeenGraphicsSystem, path.getCompletePath(), nullptr);
                keen::file::unmount(Init_params.m_pFileSystem, mount.result);
                if (patched_p != nullptr)
                {
                    free(patched_p);
                    char message[256];
                    if (shader_runtime->vertex_shaders[i].m_internal_state_p == nullptr)
                    {
                        // The patched blob was refused: the original one, as in rfg.exe.
                        _snprintf_safe(message, sizeof(message), "rl_shader_fixup: %s vertex shader %d: the patched blob failed, using the original\n", shader_fname, i);
                        const keen::IoResult<uint32> original = keen::file::mountReadOnlyMemoryFile(Init_params.m_pFileSystem, path.getCompletePath(), file_p + offset, blob_size);
                        shader_runtime->vertex_shaders[i].m_internal_state_p = keen::graphics::createVertexShaderFromFile(Init_params.m_pKeenGraphicsSystem, path.getCompletePath(), nullptr);
                        keen::file::unmount(Init_params.m_pFileSystem, original.result);
                    }
                    else
                    {
                        _snprintf_safe(message, sizeof(message), "rl_shader_fixup: %s vertex shader %d: %s (%u reads)\n", shader_fname, i, fix, num_reads);
                    }
                    keen::debug::nativePrintArgs(message);
                }
                offset += blob_size;
                shader_runtime->vertex_shaders[i].m_platform_state_p = nullptr;
            }
        }
        for (int i = 0; i < header_p->num_pixel_shaders; i++)
        {
            const unsigned int blob_size = pixel_shaders_p[i];
            if (blob_size != 0)
            {
                offset = (offset + 15) & ~15u;
                keen::FilePath path("/shaders/cache/", true);
                path.setFileNameWithExtension(shader_fname);
                const keen::IoResult<uint32> mount = keen::file::mountReadOnlyMemoryFile(Init_params.m_pFileSystem, path.getCompletePath(), file_p + offset, blob_size);
                offset += blob_size;
                shader_runtime->pixel_shaders[i].m_internal_state_p = keen::graphics::createFragmentShaderFromFile(Init_params.m_pKeenGraphicsSystem, path.getCompletePath(), nullptr);
                keen::file::unmount(Init_params.m_pFileSystem, mount.result);
            }
        }
        for (int i = 0; i < header_p->num_techniques; i++)
        {
            const rl_shader_technique& technique = techniques_p[i];
            rl_shader_technique_runtime& runtime = shader_runtime->shader_techniques[i];
            runtime.flags = technique.flags;
            runtime.technique_name_crc = technique.technique_name_crc;
            rl_dev_vertex_shader& vertex_shader = shader_runtime->vertex_shaders[technique.vs_index];
            const keen::ShaderPipelineParameters parameters(vertex_shader.m_internal_state_p, shader_runtime->pixel_shaders[technique.ps_index].m_internal_state_p);
            keen::graphics::createShaderPipeline(&runtime.pipeline, Init_params.m_pKeenGraphicsSystem, parameters);
            if (vertex_shader.m_platform_state_p == nullptr)
            {
                // Magic number: one input binding slot per vertex format (50).
                vertex_shader.m_platform_state_p = (const keen::VertexInputBinding**)m_shader_base_mempool.alloc(200, 0);
                memset(vertex_shader.m_platform_state_p, 0, 200);
            }
            runtime.bindings_p = vertex_shader.m_platform_state_p;
        }
        processTexFixups(shader_runtime, tex_fixups_p, header_p->num_tex_fixups);
        free(file_p);
        return shader_runtime;
    }
    char message[256];
    _snprintf_safe(message, sizeof(message), error_p, shader_fname);
    keen::debug::nativePrintArgs(message);
    return nullptr;
}

// 0x004C6400 (:1327)
void rl_render_lib::unload_shader(rl_shader_header_runtime* shader_p)
{
}

// 0x005171B0 (:1348)
// A shader built in code from three pipelines (base, lighting and depth-only techniques, flags 2, 4 and 8).
rl_shader_header_runtime* rl_render_lib::createKgShader(const keen::ShaderPipeline** pPipelines, const rl_shader_tex_fixup* pTexFixups, unsigned int numTex, const rl_shader_const_fixup* pConstFixUps, unsigned int numConsts)
{
    rl_shader_header_runtime* shader_runtime = (rl_shader_header_runtime*)m_shader_base_mempool.alloc(sizeof(rl_shader_header_runtime), 0);
    if (shader_runtime == nullptr)
    {
        return nullptr;
    }
    shader_runtime->vertex_shaders = nullptr;
    shader_runtime->pixel_shaders = nullptr;
    shader_runtime->tex_fixups = (rl_shader_tex_fixup_runtime*)m_shader_base_mempool.alloc(numTex * sizeof(rl_shader_tex_fixup_runtime), 0);
    shader_runtime->const_fixups = (rl_shader_const_fixup*)m_shader_base_mempool.alloc(numConsts * sizeof(rl_shader_const_fixup), 0);
    shader_runtime->bool_fixups = nullptr;
    shader_runtime->shader_techniques = (rl_shader_technique_runtime*)m_shader_base_mempool.alloc(3 * sizeof(rl_shader_technique_runtime), 0);
    memcpy(shader_runtime->const_fixups, pConstFixUps, numConsts * sizeof(rl_shader_const_fixup));
    shader_runtime->num_bool_fixups = 0;
    shader_runtime->num_vertex_shaders = 0;
    shader_runtime->num_pixel_shaders = 0;
    shader_runtime->signature = 0x4B42A1EE;
    shader_runtime->version = 7;
    shader_runtime->shader_flags = 0;
    shader_runtime->num_tex_fixups = (short)numTex;
    shader_runtime->num_const_fixups = (short)numConsts;
    shader_runtime->base_technique_index = 0;
    shader_runtime->lighting_technique_index = 1;
    shader_runtime->depth_only_technique_index = 2;
    shader_runtime->user_technique_start_index = -1;
    shader_runtime->num_techniques = 3;
    unsigned int flags = 1;
    for (int i = 0; i < 3; i++)
    {
        const keen::VertexInputBinding** bindings_p = (const keen::VertexInputBinding**)m_shader_base_mempool.alloc(200, 0);
        memset(bindings_p, 0, 200);
        rl_shader_technique_runtime& runtime = shader_runtime->shader_techniques[i];
        runtime.pipeline = *pPipelines[i];
        runtime.bindings_p = bindings_p;
        flags <<= 1;
        runtime.flags = flags;
    }
    processTexFixups(shader_runtime, pTexFixups, numTex);
    return shader_runtime;
}

// 0x004C6410 (:1395)
void rl_render_lib::deleteKgShader(rl_shader_header_runtime* shader_p)
{
}

// 0x004F3B00 (:1401)
// The blend states per color write mask, the two primitive sampler states (wrap and clamp) and the depth/stencil
// states per depth mode.
void rl_render_lib::initialize_primitive_macro_states()
{
    memset(m_prim_blend_states, 0, sizeof(m_prim_blend_states));
    keen::BlendParametersEx blend;
    blend.blendOpColor = keen::BlendOperation_None;
    blend.sourceFactorColor = keen::BlendFactor_Zero;
    blend.destFactorColor = keen::BlendFactor_Zero;
    blend.blendOpAlpha = keen::BlendOperation_None;
    blend.sourceFactorAlpha = keen::BlendFactor_Zero;
    blend.destFactorAlpha = keen::BlendFactor_Zero;
    for (int i = 1; i < 8; i++)
    {
        blend.colorWriteMask[i] = 0x0F;
    }
    // The color write mask of render target 0 per mode (red 1, green 2, blue 4, alpha 8).
    blend.colorWriteMask[0] = 0x0;
    initialize_alpha_states(blend, RLPCW_NONE);
    blend.colorWriteMask[0] = 0x1;
    initialize_alpha_states(blend, RLPCW_R);
    blend.colorWriteMask[0] = 0x2;
    initialize_alpha_states(blend, RLPCW_G);
    blend.colorWriteMask[0] = 0x5;
    initialize_alpha_states(blend, RLPCW_RB);
    blend.colorWriteMask[0] = 0xA;
    initialize_alpha_states(blend, RLPCW_GA);
    blend.colorWriteMask[0] = 0x7;
    initialize_alpha_states(blend, RLPCW_RGB);
    blend.colorWriteMask[0] = 0xF;
    initialize_alpha_states(blend, RLPCW_RGBA);

    m_prim_sampler_states[RLPCM_WRAP] = rl_dev_sampler_state();
    m_prim_sampler_states[RLPCM_CLAMP] = rl_dev_sampler_state();
    keen::SamplerParameters sampler;
    memset(&sampler, 0, sizeof(sampler));
    sampler.mipLodBias = 0.0f;
    sampler.minLod = 0.0f;
    // Magic number: no LOD clamp.
    sampler.maxLod = 1000.0f;
    sampler.maxAnisotropyLevel = 1;
    sampler.minFilter = keen::SamplerFilterMode_Linear;
    sampler.magFilter = keen::SamplerFilterMode_Linear;
    sampler.mipFilter = keen::SamplerFilterMode_Linear;
    sampler.addressU = keen::SamplerAddressMode_Wrap;
    sampler.addressV = keen::SamplerAddressMode_Wrap;
    sampler.addressW = keen::SamplerAddressMode_Wrap;
    sampler.comparisonFunction = keen::ComparisonFunction_Never;
    sampler.alphaKillEnabled = false;
    m_prim_sampler_states[RLPCM_WRAP].m_internal_state_p = keen::graphics::createSamplerState(Init_params.m_pKeenGraphicsSystem, sampler);
    sampler.addressU = keen::SamplerAddressMode_Clamp;
    sampler.addressV = keen::SamplerAddressMode_Clamp;
    sampler.addressW = keen::SamplerAddressMode_Clamp;
    m_prim_sampler_states[RLPCM_CLAMP].m_internal_state_p = keen::graphics::createSamplerState(Init_params.m_pKeenGraphicsSystem, sampler);

    memset(m_prim_depth_stencil_states, 0, sizeof(m_prim_depth_stencil_states));
    keen::DepthStencilParameters depth_stencil;
    depth_stencil.frontStencil.testEnabled = false;
    depth_stencil.frontStencil.testFunc = keen::ComparisonFunction_Always;
    depth_stencil.frontStencil.testMask = 0xFF;
    depth_stencil.frontStencil.writeMask = 0;
    depth_stencil.frontStencil.opFail = keen::StencilOperation_Keep;
    depth_stencil.frontStencil.opDepthFail = keen::StencilOperation_Keep;
    depth_stencil.frontStencil.opDepthPass = keen::StencilOperation_Keep;
    depth_stencil.backStencil = depth_stencil.frontStencil;
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_Always;
    depth_stencil.depthTestEnabled = false;
    depth_stencil.depthWriteEnabled = false;
    initialize_stencil_states(depth_stencil, RLPZM_NONE);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_Always;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = true;
    initialize_stencil_states(depth_stencil, RLPZM_WRITE);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_LessEqual;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = false;
    initialize_stencil_states(depth_stencil, RLPZM_READ);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_Equal;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = false;
    initialize_stencil_states(depth_stencil, RLPZM_READ_EQUAL);
    depth_stencil.depthComparisonFunction = keen::ComparisonFunction_LessEqual;
    depth_stencil.depthTestEnabled = true;
    depth_stencil.depthWriteEnabled = true;
    initialize_stencil_states(depth_stencil, RLPZM_FULL);
}

// 0x004C6420 (:1514)
void rl_render_lib::destroy_primitive_macro_states()
{
    for (int mode = 0; mode < RLPAM_NUM_ALPHA_MODES; mode++)
    {
        for (int cwm = 0; cwm < RLPCW_NUM_COLOR_WRITE_MODES; cwm++)
        {
            if (m_prim_blend_states[mode][cwm].m_internal_state_p != nullptr)
            {
                keen::graphics::destroyBlendState(Init_params.m_pKeenGraphicsSystem, m_prim_blend_states[mode][cwm].m_internal_state_p);
            }
        }
    }
    for (int i = 0; i < RLPCM_NUM_CLAMP_MODES; i++)
    {
        if (m_prim_sampler_states[i].m_internal_state_p != nullptr)
        {
            keen::graphics::destroySamplerState(Init_params.m_pKeenGraphicsSystem, m_prim_sampler_states[i].m_internal_state_p);
        }
    }
    for (int stencil = 0; stencil < RLPSM_NUM_STENCIL_MODES; stencil++)
    {
        for (int zbuf = 0; zbuf < RLPZM_NUM_ZBUF_MODES; zbuf++)
        {
            if (m_prim_depth_stencil_states[stencil][zbuf].m_internal_state_p != nullptr)
            {
                keen::graphics::destroyDepthStencilState(Init_params.m_pKeenGraphicsSystem, m_prim_depth_stencil_states[stencil][zbuf].m_internal_state_p);
            }
        }
    }
}

// Sets one blend equation for color and alpha.
static void set_blend(keen::BlendParametersEx& desc, keen::BlendOperation op, keen::BlendFactor source_color, keen::BlendFactor source_alpha, keen::BlendFactor dest_color, keen::BlendFactor dest_alpha)
{
    desc.blendOpColor = op;
    desc.blendOpAlpha = op;
    desc.sourceFactorColor = source_color;
    desc.sourceFactorAlpha = source_alpha;
    desc.destFactorColor = dest_color;
    desc.destFactorAlpha = dest_alpha;
}

// 0x004C64C0 (:1556)
// The blend state of every alpha mode for one color write mode. RLPAM_ALPHA_ADDITIVE_CONST, RLPAM_FILTER_CONST and
// RLPAM_ALPHA_SUBTRACTIVE_CONST are not created.
void rl_render_lib::initialize_alpha_states(keen::BlendParametersEx& desc, rl_prim_color_write_mode cwm)
{
    keen::GraphicsSystem* graphics_system_p = Init_params.m_pKeenGraphicsSystem;
    desc.blendOpColor = keen::BlendOperation_None;
    desc.blendOpAlpha = keen::BlendOperation_None;
    m_prim_blend_states[RLPAM_NONE][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    m_prim_blend_states[RLPAM_ALPHA_TEST][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_One, keen::BlendFactor_One, keen::BlendFactor_One, keen::BlendFactor_One);
    m_prim_blend_states[RLPAM_ADDITIVE][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_SrcAlpha, keen::BlendFactor_SrcAlpha, keen::BlendFactor_One, keen::BlendFactor_One);
    m_prim_blend_states[RLPAM_ALPHA_ADDITIVE][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_DestAlpha, keen::BlendFactor_DestAlpha, keen::BlendFactor_One, keen::BlendFactor_One);
    m_prim_blend_states[RLPAM_ALPHA_ADDITIVE_SCREEN][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Subtract, keen::BlendFactor_SrcAlpha, keen::BlendFactor_SrcAlpha, keen::BlendFactor_One, keen::BlendFactor_One);
    m_prim_blend_states[RLPAM_ALPHA_SUBTRACTIVE][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Subtract, keen::BlendFactor_One, keen::BlendFactor_One, keen::BlendFactor_One, keen::BlendFactor_One);
    m_prim_blend_states[RLPAM_SUBTRACTIVE][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_DestColor, keen::BlendFactor_DestAlpha, keen::BlendFactor_Zero, keen::BlendFactor_Zero);
    m_prim_blend_states[RLPAM_ALPHA_MULTIPLY][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_DestAlpha, keen::BlendFactor_DestAlpha, keen::BlendFactor_InvDestAlpha, keen::BlendFactor_InvDestAlpha);
    m_prim_blend_states[RLPAM_FILTER_SCREEN][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_InvDestAlpha, keen::BlendFactor_InvDestAlpha, keen::BlendFactor_DestAlpha, keen::BlendFactor_DestAlpha);
    m_prim_blend_states[RLPAM_FILTER_SCREEN_INVERSE][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_SrcAlpha, keen::BlendFactor_SrcAlpha, keen::BlendFactor_InvSrcAlpha, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_FILTER][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_One, keen::BlendFactor_One, keen::BlendFactor_InvSrcAlpha, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_FILTER_PREMULT_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_One, keen::BlendFactor_Zero, keen::BlendFactor_Zero, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_MODULATE_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_DestColor, keen::BlendFactor_Zero, keen::BlendFactor_Zero, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_MODULATE_COLOR_MODULATE_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_SrcAlpha, keen::BlendFactor_Zero, keen::BlendFactor_InvSrcAlpha, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_FILTER_MODULATE_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_One, keen::BlendFactor_Zero, keen::BlendFactor_InvSrcAlpha, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_FILTER_MODULATE_PREMULT_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_One, keen::BlendFactor_DestAlpha, keen::BlendFactor_SrcAlpha, keen::BlendFactor_Zero);
    m_prim_blend_states[RLPAM_FILTER_MODULATE_INVERSE_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Add, keen::BlendFactor_SrcAlpha, keen::BlendFactor_Zero, keen::BlendFactor_One, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_ALPHA_ADDITIVE_MODULATE_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
    set_blend(desc, keen::BlendOperation_Subtract, keen::BlendFactor_SrcAlpha, keen::BlendFactor_Zero, keen::BlendFactor_One, keen::BlendFactor_InvSrcAlpha);
    m_prim_blend_states[RLPAM_ALPHA_SUBTRACTIVE_MODULATE_ALPHA][cwm].m_internal_state_p = keen::graphics::createBlendState(graphics_system_p, desc);
}

// Sets the front stencil and copies it to the back stencil.
static void set_stencil(keen::DepthStencilParameters& desc, bool enabled, keen::ComparisonFunction func, uint32 test_mask, uint32 write_mask, keen::StencilOperation pass_op)
{
    desc.frontStencil.testEnabled = enabled;
    desc.frontStencil.testFunc = func;
    desc.frontStencil.testMask = test_mask;
    desc.frontStencil.writeMask = write_mask;
    desc.frontStencil.opFail = keen::StencilOperation_Keep;
    desc.frontStencil.opDepthFail = keen::StencilOperation_Keep;
    desc.frontStencil.opDepthPass = pass_op;
    desc.backStencil = desc.frontStencil;
}

// 0x004C6820 (:1700)
// The depth/stencil state of every stencil mode for one depth mode. Stencil bit 4 masks layered materials, bit 5 marks
// their base.
void rl_render_lib::initialize_stencil_states(keen::DepthStencilParameters& desc, rl_prim_zbuf_mode zmode)
{
    keen::GraphicsSystem* graphics_system_p = Init_params.m_pKeenGraphicsSystem;
    set_stencil(desc, false, keen::ComparisonFunction_Always, 0, 0, keen::StencilOperation_Keep);
    m_prim_depth_stencil_states[RLPSM_NONE][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Always, 0xFF, 0xFF, keen::StencilOperation_Replace);
    m_prim_depth_stencil_states[RLPSM_WRITE][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    m_prim_depth_stencil_states[RLPSM_WRITE_NOCOLOR][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Always, 0xFF, 0xFF, keen::StencilOperation_Replace);
    m_prim_depth_stencil_states[RLPSM_CLEAR][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    m_prim_depth_stencil_states[RLPSM_CLEAR_NOCOLOR][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Equal, 0xFF, 0, keen::StencilOperation_Keep);
    m_prim_depth_stencil_states[RLPSM_TEST][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Always, 0xFF, 0x10, keen::StencilOperation_Replace);
    m_prim_depth_stencil_states[RLPSM_LAYERED_MATERIAL_MASKING][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Equal, 0x10, 0x20, keen::StencilOperation_Replace);
    m_prim_depth_stencil_states[RLPSM_LAYERED_MATERIAL_BASE][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Equal, 0x30, 0x30, keen::StencilOperation_Zero);
    m_prim_depth_stencil_states[RLPSM_LAYERED_MATERIAL_TEST_BASE_FINAL][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Equal, 0x30, 0, keen::StencilOperation_Keep);
    m_prim_depth_stencil_states[RLPSM_LAYERED_MATERIAL_TEST_BASE][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Equal, 0x10, 0x30, keen::StencilOperation_Zero);
    m_prim_depth_stencil_states[RLPSM_LAYERED_MATERIAL_TEST_MASKING_FINAL][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Equal, 0x10, 0, keen::StencilOperation_Keep);
    m_prim_depth_stencil_states[RLPSM_LAYERED_MATERIAL_TEST_MASKING][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
    set_stencil(desc, true, keen::ComparisonFunction_Always, 0xFF, 0x30, keen::StencilOperation_Zero);
    m_prim_depth_stencil_states[RLPSM_LAYERED_MATERIAL_IMPLICIT_CLEAR][zmode].m_internal_state_p = keen::graphics::createDepthStencilState(graphics_system_p, desc);
}

// 0x004F3D70 (:1847)
// One rasterizer state per depth bias level, fill mode (solid, wireframe), cull mode (back, front, none) and scissor
// test (off, on). The winding order is clockwise.
void rl_render_lib::initialize_engine_rasterizer_states()
{
    keen::RasterizerParameters parameters;
    for (int bias = 0; bias < 5; bias++)
    {
        parameters.depthBias = Depth_bias_mapping[bias].depth_bias;
        parameters.depthBiasSlopeScale = Depth_bias_mapping[bias].depth_bias_slope_scale;
        parameters.depthBiasClamp = Depth_bias_mapping[bias].depth_bias_clamp;
        parameters.windingOrder = keen::WindingOrder_Cw;
        for (int fill = 0; fill < 2; fill++)
        {
            parameters.fillMode = fill == 0 ? keen::FillMode_Solid : keen::FillMode_Wireframe;
            for (int cull = 0; cull < 3; cull++)
            {
                parameters.cullMode = cull == 0 ? keen::CullMode_Back : (cull == 1 ? keen::CullMode_Front : keen::CullMode_None);
                for (int scissor = 0; scissor < 2; scissor++)
                {
                    parameters.flags = scissor != 0 ? keen::RasterizerFlagMask_EnableScissorTest : 0;
                    m_engine_rasterizer_states[bias][fill][cull][scissor].m_internal_state_p = keen::graphics::createRasterizerState(Init_params.m_pKeenGraphicsSystem, parameters);
                }
            }
        }
    }
}

// 0x004C6C40 (:1963)
void rl_render_lib::destroy_engine_rasterizer_states()
{
    for (int bias = 0; bias < 5; bias++)
    {
        for (int fill = 0; fill < 2; fill++)
        {
            for (int cull = 0; cull < 3; cull++)
            {
                for (int scissor = 0; scissor < 2; scissor++)
                {
                    keen::graphics::destroyRasterizerState(Init_params.m_pKeenGraphicsSystem, m_engine_rasterizer_states[bias][fill][cull][scissor].m_internal_state_p);
                    m_engine_rasterizer_states[bias][fill][cull][scissor].m_internal_state_p = nullptr;
                }
            }
        }
    }
}

// 0x004F3EC0 (:1987)
void rl_render_lib::recreate_engine_rasterizer_states()
{
    destroy_engine_rasterizer_states();
    initialize_engine_rasterizer_states();
}

// 0x004C6CC0 (:1993)
// RLPCW_UNDEFINED means no color writes, or RGBA when undefined_override is set.
rl_dev_blend_state rl_render_lib::get_blend_state_from_prim_alpha_mode(rl_prim_alpha_mode mode, rl_prim_color_write_mode color_write, bool undefined_override)
{
    if (color_write == RLPCW_UNDEFINED)
    {
        color_write = undefined_override ? RLPCW_RGBA : RLPCW_NONE;
    }
    return m_prim_blend_states[mode][color_write];
}

// 0x004C6CF0 (:1999)
rl_dev_depth_stencil_state rl_render_lib::get_depth_stencil_state_from_prim_stencil_zbuf_mode(rl_prim_stencil_mode stencil_mode, rl_prim_zbuf_mode zbuf_mode)
{
    return m_prim_depth_stencil_states[stencil_mode][zbuf_mode];
}

// 0x004C6D10 (:2004)
rl_dev_sampler_state rl_render_lib::get_sampler_state_from_prim_clamp_mode(rl_prim_clamp_mode mode)
{
    return m_prim_sampler_states[mode];
}

// 0x004F3ED0 (:2009)
const rl_dev_texture rl_render_lib::create_dynamic_texture(unsigned int width, unsigned int height, rl_cached_dyn_texture_format format)
{
    rl_dev_format dev_format = RL_FORMAT_B8G8R8A8_UNORM;
    if (format == RLCDT_FORMAT_L8)
    {
        dev_format = RL_FORMAT_R8_UNORM;
    }
    else if (format == RLCDT_FORMAT_A8)
    {
        dev_format = RL_FORMAT_A8_UNORM;
    }
    keen::TextureDescription desc;
    desc.width = (uint16)width;
    desc.height = (uint16)height;
    desc.depth = 1;
    desc.levelCount = 1;
    desc.cpuAccessMode = keen::TextureAccessMode_Write;
    return rl_dev_texture(create_keen_texture_data(Init_params.m_pKeenGraphicsSystem, desc, dev_format, nullptr, false, "DynamicTexture2d"));
}

// 0x004C6D30 (:2037)
void rl_render_lib::destroy_dynamic_texture(rl_dev_texture tex)
{
    keen::graphics::freeTextureData(tex.pData, Init_params.m_pKeenGraphicsSystem, g_pVramAllocator);
    delete tex.pData;
}

// 0x004C6D60 (:2042)
void rl_render_lib::get_dynamic_texture_dimensions(const rl_dev_texture tex, int& width, int& height, int& mip_levels)
{
    width = tex.pData->description.width;
    height = tex.pData->description.height;
    mip_levels = tex.pData->description.levelCount;
}

// 0x004C6D90 (:2049)
// The msaa level is ignored. Shadow map and depth formats are depth/stencil bound, the others render targets.
const rl_dev_texture rl_render_lib::create_render_texture(const int width, const int height, const int mip_levels, const rl_dev_format format, unsigned int msaa_level, const char* pDebugName, unsigned int initFlags)
{
    keen::TextureDescription desc;
    desc.depth = 0;
    desc.width = (uint16)width;
    desc.height = (uint16)height;
    desc.levelCount = (uint8)mip_levels;
    desc.format = (uint8)keen_format_from_dev_format(format);
    desc.cpuAccessMode = keen::TextureAccessMode_None;
    desc.gpuAccessMode = keen::TextureAccessMode_Read;
    desc.type = keen::TextureType_2d;
    desc.flags = (unsigned short)((format == RL_FORMAT_SHADOW_MAP || format == RL_FORMAT_DEPTH_TEXTURE) ? keen::TextureFlagMask_Bind_DepthStencil : keen::TextureFlagMask_Bind_RenderTarget);
    desc.flags |= keen::TextureFlagMask_Bind_ShaderInput;
    keen::TextureData* texture_p = new keen::TextureData();
    keen::graphics::allocateTextureData(texture_p, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator, desc, nullptr, 0, pDebugName);
    return rl_dev_texture(texture_p);
}

// 0x004C6E70 (:2079)
void rl_render_lib::destroy_render_texture(const rl_dev_texture& texture)
{
    keen::graphics::freeTextureData(texture.pData, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator);
    delete texture.pData;
}

// 0x004C6EA0 (:2084)
const rl_dev_render_target rl_render_lib::create_render_target(const rl_dev_texture& texture)
{
    keen::RenderTargetBuffer buffer;
    buffer.pDataBuffer = texture.pData;
    buffer.format = (keen::PixelFormat)texture.pData->description.format;
    const keen::RenderTargetBuffer* color_buffers_p = nullptr;
    const keen::RenderTargetBuffer* depth_buffer_p = nullptr;
    if (buffer.format == keen::PixelFormat_Depth16 || buffer.format == keen::PixelFormat_Depth32F)
    {
        depth_buffer_p = &buffer;
    }
    else
    {
        color_buffers_p = &buffer;
    }
    return rl_dev_render_target(keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, texture.pData->description.width, texture.pData->description.height, color_buffers_p, color_buffers_p != nullptr ? 1 : 0, depth_buffer_p, keen::TilingMode_None));
}

// 0x004C6F20 (:2105)
void rl_render_lib::destroy_render_target(const rl_dev_render_target& rt)
{
    keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, rt.pData);
}

// 0x004C6F40 (:2110)
// A D24S8 depth buffer with its own texture. The msaa level and flags are ignored.
// Notes (decompiled behaviour, kept as is): the depth texture is not destroyed with the target (it leaks).
const rl_dev_depth_stencil_target rl_render_lib::create_depth_stencil_target(const unsigned int width, const unsigned int height, const unsigned int msaa_level, const char* pDebugName, unsigned int flags)
{
    keen::TextureDescription desc;
    desc.depth = 0;
    desc.width = (uint16)width;
    desc.height = (uint16)height;
    desc.levelCount = 1;
    desc.cpuAccessMode = keen::TextureAccessMode_None;
    desc.gpuAccessMode = keen::TextureAccessMode_Read;
    desc.flags = keen::TextureFlagMask_Bind_ShaderInput | keen::TextureFlagMask_Bind_DepthStencil;
    desc.type = keen::TextureType_2d;
    desc.format = keen::PixelFormat_Depth24Stencil8;
    keen::RenderTargetBuffer buffer;
    buffer.pDataBuffer = new keen::TextureData();
    keen::graphics::allocateTextureData(buffer.pDataBuffer, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator, desc, nullptr, 0, pDebugName);
    buffer.format = (keen::PixelFormat)desc.format;
    return rl_dev_depth_stencil_target(keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, desc.width, desc.height, nullptr, 0, &buffer, keen::TilingMode_None));
}

// 0x004C7010 (:2150)
const rl_dev_depth_stencil_target rl_render_lib::create_depth_stencil_target(const rl_dev_texture& texture)
{
    keen::RenderTargetBuffer buffer;
    buffer.pDataBuffer = texture.pData;
    buffer.format = (keen::PixelFormat)texture.pData->description.format;
    return rl_dev_depth_stencil_target(keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, texture.pData->description.width, texture.pData->description.height, nullptr, 0, &buffer, keen::TilingMode_None));
}

// 0x004C7060 (:2165)
const rl_screen_info* rl_render_lib::get_screen_info() const
{
    return &m_screen_info;
}

// 0x004F3F60 (:2170)
void rl_render_lib::update_screen_info(int refresh_num, int refresh_denom)
{
    m_screen_info.set_refresh_rate(refresh_num, refresh_denom);
    build_screen_info(Init_params.m_pKeenGraphicsSystem, m_screen_info);
}

// 0x00517300 (:2176)
void rl_render_lib::set_screen_info(const rl_screen_info& screen_info, const rl_level_of_detail* level_of_detail_p)
{
    if (level_of_detail_p != nullptr)
    {
        set_level_of_detail(*level_of_detail_p);
    }
    m_screen_info = screen_info;
}

// 0x004F4000 (:2185)
// Applied by check_device_status.
void rl_render_lib::set_level_of_detail(const rl_level_of_detail& lod)
{
    m_new_level_of_detail = lod;
    m_lodChanged = true;
    validate_msaa_mode();
}

// 0x004C7070 (:2196)
const rl_level_of_detail& rl_render_lib::get_level_of_detail() const
{
    return m_new_level_of_detail;
}

// 0x004C7080 (:2201)
void rl_render_lib::destroy_depth_stencil_target(const rl_dev_depth_stencil_target& ds)
{
    if (ds.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, ds.pData);
    }
}

// 0x0054D3F0 (:2210)
// Refreshes the screen info from the back buffer and applies a pending level-of-detail change. m_sizesChanged tells
// rl_renderer::updateResourceSizes to recreate the render targets.
void rl_render_lib::check_device_status()
{
    int refresh_num;
    int refresh_denom;
    m_screen_info.get_refresh_rate(refresh_num, refresh_denom);
    update_screen_info(refresh_num, refresh_denom);
    m_sizesChanged = false;
    if (m_lodChanged)
    {
        m_level_of_detail = m_new_level_of_detail;
        Active_scene_p->m_scene_renderer_p->m_new_level_of_detail = m_level_of_detail;
        rl_singleton_phoenix<rl_particle_renderer>::get_instance()->m_level_of_detail = m_level_of_detail.m_particle_lod;
    }
    else if (m_screen_info.get_screen_width() == m_last_width && m_screen_info.get_screen_height() == m_last_height && m_screen_info.get_window_mode() == m_lastWindowMode)
    {
        return;
    }
    m_lodChanged = false;
    m_sizesChanged = true;
    m_last_width = m_screen_info.get_screen_width();
    m_last_height = m_screen_info.get_screen_height();
    m_lastWindowMode = m_screen_info.get_window_mode();
}

// 0x004C70A0 (:2259)
unsigned int getFilterIndex(keen::SamplerFilterMode filter)
{
    if (filter == keen::SamplerFilterMode_Linear)
    {
        return 1;
    }
    if (filter == keen::SamplerFilterMode_Anisotropic)
    {
        return 2;
    }
    return 0;
}

// 0x004C70C0 (:2276)
unsigned int getAddressIndex(keen::SamplerAddressMode address)
{
    if (address == keen::SamplerAddressMode_Wrap)
    {
        return 1;
    }
    if (address == keen::SamplerAddressMode_Mirror)
    {
        return 2;
    }
    return 0;
}

// 0x004C70E0 (:2294)
// The clamped sampler index of a texture fixup: 2 bits each for the U, V and W address (bit 0, 1 and 2 of flags
// force clamp), the min, mag and mip filters, and 3 bits of border color. params_p receives the sampler parameters.
unsigned short rl_render_lib::get_clamped_sampler_handle(const rl_shader_tex_fixup_runtime& fixup, const unsigned int flags, keen::SamplerParameters* params_p)
{
    const keen::SamplerFilterMode min_filter = Texture_filter_mode_mapping_keen[fixup.min_filter];
    const keen::SamplerFilterMode mag_filter = Texture_filter_mode_mapping_keen[fixup.mag_filter];
    const keen::SamplerFilterMode mip_filter = Texture_filter_mode_mapping_keen[fixup.mip_filter];
    const keen::SamplerAddressMode address_u = (flags & 1) != 0 ? keen::SamplerAddressMode_Clamp : Texture_address_mode_mapping_keen[fixup.address_u];
    const keen::SamplerAddressMode address_v = (flags & 2) != 0 ? keen::SamplerAddressMode_Clamp : Texture_address_mode_mapping_keen[fixup.address_v];
    const keen::SamplerAddressMode address_w = (flags & 4) != 0 ? keen::SamplerAddressMode_Clamp : Texture_address_mode_mapping_keen[fixup.address_w];
    if (params_p != nullptr)
    {
        params_p->addressV = address_v;
        params_p->addressU = address_u;
        params_p->addressW = address_w;
        params_p->minFilter = min_filter;
        params_p->magFilter = mag_filter;
        params_p->mipFilter = mip_filter;
        params_p->minLod = 0.0f;
        params_p->mipLodBias = 0.0f;
    }
    unsigned int handle = fixup.constLocData.border_color;
    handle = handle << 2 | getFilterIndex(mip_filter);
    handle = handle << 2 | getFilterIndex(mag_filter);
    handle = handle << 2 | getFilterIndex(min_filter);
    handle = handle << 2 | getAddressIndex(address_w);
    handle = handle << 2 | getAddressIndex(address_v);
    handle = handle << 2 | getAddressIndex(address_u);
    return (unsigned short)handle;
}

// 0x004C7270 (:2372)
bool is_clamped_sampler_state_needed(unsigned short idx)
{
    for (unsigned int i = 0; i < sizeof(rl_Clamped_sampler_states_to_create) / sizeof(rl_Clamped_sampler_states_to_create[0]); i++)
    {
        if (rl_Clamped_sampler_states_to_create[i] == idx)
        {
            return true;
        }
    }
    return false;
}

// 0x004F4030 (:2382)
const keen::SamplerState* rl_render_lib::create_and_cache_clamped_sampler_state(const rl_shader_tex_fixup_runtime& fixup, const unsigned int flags)
{
    keen::SamplerParameters parameters;
    const unsigned short handle = get_clamped_sampler_handle(fixup, flags, &parameters);
    if (rl_Clamped_sampler_states[handle].m_internal_state_p == nullptr && is_clamped_sampler_state_needed(handle))
    {
        rl_Clamped_sampler_states[handle].m_internal_state_p = keen::graphics::createSamplerState(Init_params.m_pKeenGraphicsSystem, parameters);
    }
    return rl_Clamped_sampler_states[handle].m_internal_state_p;
}

// 0x004C7290 (:2393)
unsigned short rl_render_lib::get_clamped_sampler_handle(const rl_shader_tex_fixup_runtime& fixup, const unsigned int flags)
{
    return get_clamped_sampler_handle(fixup, flags, nullptr);
}

// 0x004F4100 (:2400)
// The usage is ignored. The data is the CPU data of the whole mip chain.
rl_dev_texture rl_render_lib::create_texture_2d(const int width, const int height, const int mip_levels, const rl_dev_format format, const rl_dev_usage usage, const rl_dev_access access, const void* const data_p, unsigned int flags)
{
    keen::TextureDescription desc;
    desc.width = (uint16)width;
    desc.height = (uint16)height;
    desc.depth = 1;
    desc.levelCount = (uint8)mip_levels;
    desc.cpuAccessMode = keen_access_from_dev_access_and_usage(access, usage);
    desc.flags = (uint16)flags | keen::TextureFlagMask_Bind_ShaderInput;
    desc.type = keen::TextureType_2d;
    return rl_dev_texture(create_keen_texture_data(Init_params.m_pKeenGraphicsSystem, desc, format, data_p, true, "Texture2d"));
}

// 0x004C72B0 (:2415)
void rl_render_lib::destroy_texture_2d(const rl_dev_texture& texture)
{
    keen::graphics::freeTextureData(texture.pData, Init_params.m_pKeenGraphicsSystem, nullptr);
    delete texture.pData;
}

// 0x004C72E0 (:2420)
const keen::VertexFormat* rl_render_lib::get_keen_vertex_format(const rl_dev_vertex_format format)
{
    return m_vertex_formats_p[format];
}

// 0x004C72F0 (:2426)
// Snaps the requested antialiasing level to a supported one: the last supported level above it, or the highest.
void rl_render_lib::validate_msaa_mode()
{
    const unsigned int num_modes = get_num_msaa_modes();
    if (num_modes == 0)
    {
        m_new_level_of_detail.m_aa_lod = RL_AA_LOD_OFF;
        return;
    }
    rl_antialias_lod* modes_p = (rl_antialias_lod*)_alloca(num_modes * sizeof(rl_antialias_lod));
    get_msaa_modes(modes_p);
    rl_antialias_lod best = modes_p[num_modes - 1];
    for (unsigned int i = 0; i < num_modes; i++)
    {
        if (modes_p[i] == m_new_level_of_detail.m_aa_lod)
        {
            return;
        }
        if (modes_p[i] > m_new_level_of_detail.m_aa_lod)
        {
            best = modes_p[i];
        }
    }
    m_new_level_of_detail.m_aa_lod = best;
}

// 0x004C73D0 (:2456)
rl_stamped_handle rl_render_lib::occlusion_query_issue_begin(rl_renderer& renderer)
{
    rl_stamped_handle handle;
    handle.set(m_occlusionQueries.allocateAndBeginOcclusionQuery(renderer.getCommandWriter()->getBuffer()));
    return handle;
}

// 0x004F41A0 (:2476)
void rl_render_lib::occlusion_query_issue_end(rl_stamped_handle handle, rl_renderer& renderer)
{
    m_occlusionQueries.endOcclusionQuery(renderer.getCommandWriter()->getBuffer(), handle.get());
}

// 0x004F41E0 (:2487)
// The pixel count, or -1 when the result is not ready (wait is ignored).
int rl_render_lib::occlusion_query_get_pixels_drawn(rl_stamped_handle handle, rl_renderer& renderer, bool wait)
{
    uint64 result;
    if (m_occlusionQueries.pollOcclusionQueryResult(renderer.getCommandWriter()->getBuffer(), handle.get(), &result))
    {
        return (int)result;
    }
    return -1;
}

// 0x004F4240 (:2507)
void rl_render_lib::occlusion_query_release_handle(rl_stamped_handle handle)
{
    m_occlusionQueries.freeOcclusionQuery(handle.get());
}

// 0x004B7930 (.h:105)
bool rl_render_lib::is_d3d10() const
{
    return true;
}

// 0x004B7940 (.h:107)
bool rl_render_lib::supports_depth_texture_shadow_maps() const
{
    return true;
}

// 0x007B99B0 (.h:134)
void rl_render_lib::set_frametime_total_ms(unsigned int frametime_total_ms)
{
    m_frametime_total_ms = frametime_total_ms;
}

// 0x004B7950 (.h:135)
unsigned int rl_render_lib::get_frametime_total_ms() const
{
    return m_frametime_total_ms;
}

// 0x004B7960 (.h:140)
bool rl_render_lib::hasTargetSizeChanged() const
{
    return m_sizesChanged;
}

// 0x004B7970 (.h:161)
void rl_render_lib::lockInputBindingMutex()
{
    m_inputBindingMutex.lock(0);
}

// 0x004B7980 (.h:162)
void rl_render_lib::unlockInputBindingMutex()
{
    m_inputBindingMutex.unlock();
}

// 0x004B7B40 (rl_renderer.h:538)
unsigned int rl_render_lib::get_current_frame_count() const
{
    return m_current_frame_count;
}

// 0x004B7B50 (rl_renderer.h:543)
unsigned int rl_render_lib::get_num_gpus() const
{
    return m_num_gpus;
}

// 0x004B7B60 (rl_renderer.h:549)
rl_renderer_metrics* rl_render_lib::get_metrics() const
{
    return m_metrics_p;
}

// ---- the bounding box projections (VFX.md FX7: the decal renderers' scissor rectangles)

// The three corners sharing an edge with each corner of bbox_get_verts.
static const int Bbox_neighbors[8][3] = // 0x014FA0A0
{
    { 1, 3, 4 }, { 2, 0, 5 }, { 3, 1, 6 }, { 0, 2, 7 }, { 5, 7, 0 }, { 6, 4, 1 }, { 7, 5, 2 }, { 4, 6, 3 },
};

// Not in the binary: a render-offset relative point through the renderer's local-to-clip transform (inlined in
// rl_project_get_bbox_2d_rect_robust, in its float order).
static vector4 rl_project_clip_point(const vector& p, const matrix44& m)
{
    return vector4(m.element(1, 0) * p.y + p.x * m.element(0, 0) + m.element(2, 0) * p.z + m.element(3, 0),
                   m.element(1, 1) * p.y + m.element(0, 1) * p.x + m.element(2, 1) * p.z + m.element(3, 1),
                   m.element(1, 2) * p.y + m.element(0, 2) * p.x + m.element(2, 2) * p.z + m.element(3, 2),
                   m.element(1, 3) * p.y + m.element(0, 3) * p.x + m.element(2, 3) * p.z + m.element(3, 3));
}

// Not in the binary: the screen point of a clip point (gr_project_vertex's divide, inlined).
static void rl_project_screen_point(float& sx, float& sy, vector4 clip, const matrix44& clip_screen)
{
    clip.rotate(clip_screen);
    const float inverse_w = 1.0f / clip.w;
    sx = inverse_w * clip.x;
    sy = inverse_w * clip.y;
}

// 0x004F4270 (:2518)
// gr_rotate_vertex on the renderer's local-to-clip transform (the sums in double precision). LTCG: dest in EDX, src
// in ECX.
static unsigned int rl_project_rotate_vertex(vertex& dest, const vector& src, const rl_renderer& renderer)
{
    const matrix44& m = renderer.get_local_clip_transform();
    dest.rx = (float)((double)m.element(1, 0) * src.y + (double)m.element(0, 0) * src.x + (double)m.element(2, 0) * src.z + m.element(3, 0));
    dest.ry = (float)((double)m.element(0, 1) * src.x + (double)m.element(1, 1) * src.y + (double)m.element(2, 1) * src.z + m.element(3, 1));
    dest.rz = (float)((double)m.element(0, 2) * src.x + (double)m.element(1, 2) * src.y + (double)m.element(2, 2) * src.z + m.element(3, 2));
    const float w = (float)((double)m.element(0, 3) * src.x + (double)m.element(1, 3) * src.y + (double)m.element(2, 3) * src.z + m.element(3, 3));
    dest.w = w;
    unsigned int codes = 0;
    dest.flags = 0;
    // The clip codes of gr_rotate_vertex: 8 right, 4 left, 0x20 top, 0x10 bottom, 0x80 behind.
    if (dest.rx > w)
    {
        codes = 8;
    }
    if (-w > dest.rx)
    {
        codes += 4;
    }
    if (dest.ry > w)
    {
        codes += 0x20;
    }
    if (-w > dest.ry)
    {
        codes += 0x10;
    }
    if (!(0.0f < dest.rz))
    {
        codes += 0x80;
    }
    dest.codes = (unsigned char)codes;
    return codes;
}

// 0x004F4480 (:2530)
// The screen rectangle of a render-offset relative box that crosses the near plane: the box corners in front of a
// plane just past the camera's near clip, and where the edges from the corners behind it cross it, projected; the
// rectangle (floored origin, ceiled size) is clamped to screen_rect's size. False when a point still projects behind
// the eye; an empty rectangle when nothing is in front. LTCG: x in ECX, y in EDX.
//
// Approximation: the plane and projection sums use one association for every corner (rfg.exe varies it per corner and
// for the edge points); the result is a scissor rectangle only.
static bool rl_project_get_bbox_2d_rect_robust(const vector& bmin, const vector& bmax, float& x, float& y, float& w, float& h, const rl_rect& screen_rect, const matrix44& clip_screen, const rl_renderer& renderer)
{
    const float screen_w = (float)(screen_rect.m_right - screen_rect.m_left);
    const float screen_h = (float)(screen_rect.m_bottom - screen_rect.m_top);
    float min_x = screen_w;
    float min_y = screen_h;
    float max_x = 0.0f;
    float max_y = 0.0f;
    bool any = false;
    // rl_renderer::get_full_state is not const (the original reads m_state_p directly).
    const rl_camera* camera = const_cast<rl_renderer&>(renderer).get_full_state().get_active_camera();
    const vector& offset = renderer.get_render_offset();
    const vector& fvec = camera->get_rotation().fvec;
    // Magic number: the plane is 0.001 past the near clip plane.
    const float plane_dist = camera->get_near_clip() + 0.001f;
    const vector& eye = camera->get_position();
    const vector plane_point((eye.x - offset.x) + plane_dist * fvec.x, (eye.y - offset.y) + fvec.y * plane_dist, (eye.z - offset.z) + fvec.z * plane_dist);
    plane near_plane;
    near_plane.set(fvec, plane_point);
    vector verts[8];
    bbox_get_verts(verts, bmin, bmax);
    unsigned int behind = 0;
    for (int i = 0; i < 8; i++)
    {
        if (verts[i].y * near_plane.normal.y + verts[i].x * near_plane.normal.x + verts[i].z * near_plane.normal.z + near_plane.offset < 0.0f)
        {
            behind |= 1u << i;
        }
    }
    const matrix44& local_clip = renderer.get_local_clip_transform();
    for (int i = 0; i < 8; i++)
    {
        vector points[3];
        int num_points = 0;
        if ((behind & (1u << i)) == 0)
        {
            points[num_points++] = verts[i];
        }
        else
        {
            for (int k = 0; k < 3; k++)
            {
                const int n = Bbox_neighbors[i][k];
                if ((behind & (1u << n)) != 0)
                {
                    continue;
                }
                line edge;
                edge.point = verts[i];
                edge.dir.set(verts[n].x - verts[i].x, verts[n].y - verts[i].y, verts[n].z - verts[i].z);
                vector crossing;
                ix_line_plane(crossing, edge, near_plane);
                points[num_points++] = crossing;
            }
        }
        for (int p = 0; p < num_points; p++)
        {
            const vector4 clip = rl_project_clip_point(points[p], local_clip);
            if (clip.z <= 0.0f)
            {
                return false;
            }
            float sx;
            float sy;
            rl_project_screen_point(sx, sy, clip, clip_screen);
            if (min_x - sx >= 0.0f)
            {
                min_x = sx;
            }
            if (min_y - sy >= 0.0f)
            {
                min_y = sy;
            }
            if (max_x - sx < 0.0f)
            {
                max_x = sx;
            }
            if (max_y - sy < 0.0f)
            {
                max_y = sy;
            }
            any = true;
        }
    }
    if (!any)
    {
        x = 0.0f;
        y = 0.0f;
        w = 0.0f;
        h = 0.0f;
        return true;
    }
    // Clamped to [0, limit] as rfg.exe compares (v - 0 < 0, then 0 <= v - limit).
    float value = (float)(int)floor((double)min_x);
    value = value - 0.0f < 0.0f ? 0.0f : value;
    x = 0.0f <= value - screen_w ? screen_w : value;
    value = (float)(int)floor((double)min_y);
    value = value - 0.0f < 0.0f ? 0.0f : value;
    y = 0.0f <= value - screen_h ? screen_h : value;
    value = (float)(int)(float)ceil((double)(max_x - x));
    value = value - 0.0f < 0.0f ? 0.0f : value;
    w = 0.0f <= value - (screen_w - x) ? screen_w - x : value;
    value = (float)(int)(float)ceil((double)(max_y - y));
    value = value - 0.0f < 0.0f ? 0.0f : value;
    h = 0.0f <= value - (screen_h - y) ? screen_h - y : value;
    return true;
}

// 0x004F53A0 (:2625)
// The screen rectangle of a world box: the silhouette of the box seen from the camera (render-offset relative),
// projected; the whole screen_rect (and false) when there is no silhouette; the robust form when a silhouette point
// projects behind the eye. The maximum is kept as a truncated integer while the points are folded in (decompiled
// behaviour, kept as is).
bool rl_project_get_bbox_2d_rect_ex(const vector& bmin, const vector& bmax, float& x, float& y, float& w, float& h, const rl_rect& screen_rect, const matrix44& clip_screen, const rl_renderer& renderer)
{
    const vector& offset = renderer.get_render_offset();
    const vector rel_min(bmin.x - offset.x, bmin.y - offset.y, bmin.z - offset.z);
    const vector rel_max(bmax.x - offset.x, bmax.y - offset.y, bmax.z - offset.z);
    // rl_renderer::get_full_state is not const (the original reads m_state_p directly).
    const vector& eye = const_cast<rl_renderer&>(renderer).get_full_state().get_active_camera()->get_position();
    const vector rel_eye(eye.x - offset.x, eye.y - offset.y, eye.z - offset.z);
    // Magic number: a box silhouette has at most 6 corners.
    vector silhouette[6];
    const int num_points = bbox_find_silhouette(silhouette, rel_eye, rel_min, rel_max);
    if (num_points < 1)
    {
        x = (float)screen_rect.m_left;
        y = (float)screen_rect.m_top;
        w = (float)(screen_rect.m_right - screen_rect.m_left);
        h = (float)(screen_rect.m_bottom - screen_rect.m_top);
        return false;
    }
    vertex verts[6];
    int max_x = 0;
    int max_y = 0;
    for (int i = 0; i < num_points; i++)
    {
        vertex& v = verts[i];
        rl_project_rotate_vertex(v, silhouette[i], renderer);
        unsigned char flags = v.flags;
        if ((flags & 3) == 0)
        {
            if (0.0f < v.rz)
            {
                vector4 clip(v.rx, v.ry, v.rz, v.w);
                clip.rotate(clip_screen);
                const float inverse_w = (float)(1.0 / (double)clip.w);
                v.sx = inverse_w * clip.x;
                v.sy = inverse_w * clip.y;
                v.sw = inverse_w * clip.z;
                flags |= 1;
            }
            else
            {
                flags |= 2;
            }
            v.flags = flags;
        }
        if ((flags & 2) != 0)
        {
            return rl_project_get_bbox_2d_rect_robust(rel_min, rel_max, x, y, w, h, screen_rect, clip_screen, renderer);
        }
        if (i == 0)
        {
            x = v.sx;
            y = v.sy;
            max_x = (int)x;
            max_y = (int)v.sy;
            continue;
        }
        if (x - v.sx >= 0.0f)
        {
            x = v.sx;
        }
        if (y - v.sy >= 0.0f)
        {
            y = v.sy;
        }
        max_x = (int)((float)max_x - v.sx < 0.0f ? v.sx : (float)max_x);
        max_y = (int)((float)max_y - v.sy < 0.0f ? v.sy : (float)max_y);
    }
    w = (float)((double)(float)max_x - (double)x);
    h = (float)((double)(float)max_y - (double)y);
    return true;
}

// 0x004F5990 (:2680)
// The box's screen rectangle (rl_project_get_bbox_2d_rect_ex) as integers, clamped to 0 and inside the renderer's
// scissor region (an inverted edge collapses onto the other); the scissor region itself (and false) when the box has no
// silhouette.
bool rl_project_get_bbox_2d_rect_clipped_ex(rl_rect& scissor_rect_r, const vector& bmin, const vector& bmax, const rl_rect& screen_rect, const matrix44& clip_screen, const rl_renderer& renderer)
{
    float x;
    float y;
    float w;
    float h;
    const bool projected = rl_project_get_bbox_2d_rect_ex(bmin, bmax, x, y, w, h, screen_rect, clip_screen, renderer);
    rl_rect scissor;
    renderer.get_scissor_region(scissor);
    if (!projected)
    {
        scissor_rect_r = scissor;
        return false;
    }
    int left = (int)x < 0 ? 0 : (int)x;
    if (left <= scissor.m_left)
    {
        left = scissor.m_left;
    }
    int right = (int)(x + w) < 0 ? 0 : (int)(x + w);
    if (scissor.m_right <= right)
    {
        right = scissor.m_right;
    }
    if (right < left)
    {
        left = right;
    }
    int top = (int)y < 0 ? 0 : (int)y;
    if (top <= scissor.m_top)
    {
        top = scissor.m_top;
    }
    int bottom = (int)(y + h) < 0 ? 0 : (int)(y + h);
    if (scissor.m_bottom <= bottom)
    {
        bottom = scissor.m_bottom;
    }
    if (bottom < top)
    {
        top = bottom;
    }
    scissor_rect_r = rl_rect(left, top, right, bottom);
    return true;
}
