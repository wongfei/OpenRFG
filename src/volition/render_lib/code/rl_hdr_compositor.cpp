// Original: volition/render_lib/code/rl_hdr_compositor.cpp
//
// rl_hdr_compositor: the final composite of the HDR scene target (tone mapping with the adapted luminance, bloom, sun
// shafts and particles) into the destination, and the luminance / bloom targets it uses.
//
// Partial: the composite, the targets, the shader slots, the luminance measurement with the eye adaptation, the
// bright pass and bloom chain (V5) and the sun shafts. Not done: the older bloom chain render_bloom (composite uses
// render_bloom_3).
// The sun shafts run whenever the scene renderer passes a mask texture (the SSAO blur target while SSAO is on); they
// overwrite the final bloom stage that bloom_composite then adds to, so without them the bloom piles up frame after
// frame (the muzzle flash washing out the sky and distant terrain).

#include "volition/render_lib/code/rl_hdr_compositor.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_color_correct_state.h"
#include "volition/render_lib/code/rl_compositor.h"
#include "volition/render_lib/code/rl_dyn_texture.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_hdr_state.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/os/frametime.h"
#include "keen/graphics/graphicscommandwriter.hpp"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/rendertarget.hpp"
#include "keen/graphics/texturedata.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/graphicscommandbuffer_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

#include <math.h>

// <rl_hdr_compositor::release> 0x004C1F00 (:36)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_hdr_compositor)

// The weight multiplier of the 17-sample bloom blur kernels (compute_sampling_offsets).
static float multiplier = 2.0f; // 0x015175C8
extern bool RL_hdr_bloom_super_soft; // 0x01B7BCA6 (rl_hdr_state.cpp)

// The sun shaft tweakables (render_sun_shafts). Rl_sun_shafts_enabled (0x014FA288) is in rl_scene_renderer.cpp.
bool Rl_sun_shafts_sun_mask = true; // 0x014FA289, mask the shafts around the sun instead of downsampling the sky
// Sun shafts sample the 4:1 downsampled source instead of the full resolution scene.
bool g_sunShaftsUseHalfResSource = true; // 0x014FA28A
float Rl_sun_shafts_sun_size = 300.0f; // 0x014FA28C
float Rl_sun_shafts_scale = 1.0f; // 0x014FA290
float Rl_sun_shafts_base_lum = 1.0f; // 0x014FA294
float Rl_sun_shafts_lum_step_scale = 1.0f; // 0x014FA298
float Rl_sun_shafts_radius = 5.0f; // 0x014FA29C
float Rl_sun_shafts_color_curve_base = -4.0f; // 0x014FA2A0
float Rl_sun_shafts_color_curve_exp = 1.6f; // 0x014FA2A4
float Rl_sun_shafts_color_curve_shift = -0.02f; // 0x014FA2A8
float Rl_sun_shafts_blur_multiplier = 1.0f; // 0x014FA2AC
float Rl_sun_shafts_blur_rho = 0.75f; // 0x014FA2B0

const int rl_hdr_compositor::kDownsampleTechnique = 0; // 0x012BEDF4
const int rl_hdr_compositor::kDownsampleMaskTechnique = 2; // 0x012BEDF8
// Magic numbers: the luminance chain 64x64 -> 16x16 -> 4x4 -> 4x4 -> 1x1.
const int rl_hdr_compositor::LUM_STAGE_0_WIDTH = 64; // 0x012BEDFC
const int rl_hdr_compositor::LUM_STAGE_0_HEIGHT = 64; // 0x012BEE00
const int rl_hdr_compositor::LUM_DOWN_SAMPLE_WIDTH = 16; // 0x012BEE04
const int rl_hdr_compositor::LUM_DOWN_SAMPLE_HEIGHT = 16; // 0x012BEE08
const int rl_hdr_compositor::LUM_STAGE_1_WIDTH = 4; // 0x012BEE0C
const int rl_hdr_compositor::LUM_STAGE_1_HEIGHT = 4; // 0x012BEE10
const int rl_hdr_compositor::LUM_ADAPT_WIDTH = 1; // 0x012BEE14
const int rl_hdr_compositor::LUM_ADAPT_HEIGHT = 1; // 0x012BEE18

// Not in the binary: the fullscreen pass of composite (the same steps as rl_compositor's passes, inlined).
static void draw_fullscreen_pass(rl_renderer& renderer, const rl_dev_render_target& target, unsigned int width, unsigned int height, const rl_primitive_state& pass_state, unsigned int clear_flags)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    const rl_dev_depth_stencil_target no_ds;
    renderer.begin_pass(target, no_ds);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    renderer.set_viewport(rl_rect(0, 0, width, height));
    renderer.set_viewport_z_range(0.0f, 1.0f);
    renderer.set_target_dimensions(width, height);
    renderer.set_macro_state(pass_state, false);
    renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 0.0f), clear_flags, 1.0f, 0);
    renderer.set_macro_state(pass_state, false);
    renderer.set_vertex_format(RLVF_COMPOSITOR);
    renderer.draw_primitive(RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 2, vertex_data, sizeof(rl_compositor_vertex));
    renderer.set_macro_state(saved_state, false);
    renderer.end_pass();
    renderer.set_viewport(saved_vp);
    renderer.set_viewport_z_range(z_near, z_far);
}

// Not in the binary: a luminance target, a dynamic texture over a framebuffer texture of pixel format 0x2A and its
// render target (rl_render_texture::init's steps for an external texture, inlined in create_render_textures).
void rl_hdr_compositor::init_luminance_target(rl_render_texture& target, unsigned int width, unsigned int height, const char* pDebugName)
{
    target.m_flags = 0;
    if (target.m_dyn_texture_p == nullptr)
    {
        target.m_dyn_texture_p = rl_dyn_texture::create();
    }
    keen::TextureDescription description;
    description.width = (uint16)width;
    description.height = (uint16)height;
    description.depth = 0;
    // Magic numbers: render target flags 3, 2D, pixel format 0x2A, one level, GPU access.
    description.flags = 3;
    description.type = 1;
    description.format = 0x2A;
    description.lutFormat = 0;
    description.multiSampleType = 0;
    description.levelCount = 1;
    description.cpuAccessMode = 0;
    description.gpuAccessMode = 1;
    description.addressModeU = 0;
    description.addressModeV = 0;
    description.addressModeW = 0;
    keen::TextureData* pTexture = new keen::TextureData();
    keen::graphics::allocateTextureData(pTexture, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator, description, nullptr, 0, pDebugName);
    target.m_dyn_texture_p->m_dev_texture.pData = pTexture;
    keen::RenderTargetBuffer buffer;
    buffer.pDataBuffer = target.m_dyn_texture_p->get_texture_data().pData;
    buffer.format = (keen::PixelFormat)buffer.pDataBuffer->description.format;
    // Magic numbers: pixel formats 0x2D and 0x37 are the depth formats (a depth buffer instead of a color buffer).
    const bool is_depth = buffer.format == 0x2D || buffer.format == 0x37;
    target.m_render_target.pData = keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, buffer.pDataBuffer->description.width, buffer.pDataBuffer->description.height, is_depth ? nullptr : &buffer, is_depth ? 0 : 1, is_depth ? &buffer : nullptr, keen::TilingMode_None);
}

// 0x004E8130 (:603)
// A render pass over the target that clears its color to the packed color (0xAABBGGRR), and nothing else.
static void clearTarget(rl_render_texture* pTexture, unsigned int color, keen::GraphicsCommandWriter* pCommandWriter)
{
    // Magic numbers: depth 1, stencil 0, clear mask 2 (color).
    const keen::ClearSettings clear(color, 1.0f, 0, 2);
    pCommandWriter->writeBeginRenderPassCommand(pTexture->get_dev_render_target().pData, (keen::RenderTargetPartFlagMask)0, &clear);
    pCommandWriter->writeEndRenderPassCommand((keen::RenderTargetPartFlagMask)0);
}

// 0x004E80C0 (:540)
static int get_luminance_frame_index()
{
    rl_render_lib* render_lib = rl_render_lib::get_instance();
    return ((int)render_lib->m_current_frame_count % (int)render_lib->m_num_gpus) * 2;
}

// 0x005253F0 (:44)
// The sampling offset arrays point at their storage; textures and slots unset; the default level of detail.
rl_hdr_compositor::rl_hdr_compositor()
{
    m_adapted_lum_p = nullptr;
    m_last_luminance_calc = 0;
}

// 0x00525750 (:49)
// The render textures' destructors (each also releases its dynamic texture).
rl_hdr_compositor::~rl_hdr_compositor()
{
}

// 0x004C1F20 (:54)
int rl_hdr_compositor::get_downsampled_width(int width)
{
    return width >> 2;
}

// 0x004C1F30 (:59)
int rl_hdr_compositor::get_downsampled_height(int height)
{
    return height >> 2;
}

// 0x0054E820 (:64)
// Two adapted luminance targets per GPU, the target sizes, the render textures, the rl_hdr shader slots, the
// fullscreen 4x4 offsets and the 64x64 radial sun mask.
void rl_hdr_compositor::initialize(const unsigned int width, const unsigned int height, const rl_level_of_detail& lod, keen::GraphicsCommandWriter* pCommandWriter)
{
    m_adapted_lum_p = new rl_render_texture[rl_render_lib::get_instance()->m_num_gpus * 2];
    resize(width, height);
    m_level_of_detail = lod;
    create_render_textures(pCommandWriter);
    rl_shader_library* library = rl_shader_library::get_instance();
    m_hdr_shader_handle = library->lookup(crc_stri("rl_hdr", 0));
    m_luminance_stage_0_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("compute_luminance_stage_0", 0)));
    m_luminance_stage_1_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("compute_luminance_stage_1", 0)));
    m_luminance_adapt_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("compute_luminance_adaptive", 0)));
    m_brightpass_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("brightpass_filter", 0)));
    m_brightpass_tech_alt.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("brightpass_filter_alternate", 0)));
    m_brightpass_tech_alt2.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("brightpass_filter_alternate2", 0)));
    m_bloom_composite_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("bloom_composite", 0)));
    m_bloom_composite_3_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("bloom_composite_3", 0)));
    m_tech_final_composite_tonemapped_bloom.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("composite_tonemapped_bloom", 0)));
    m_tech_final_composite_bloom_after_tonemapping.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("composite_bloom_after_tonemapping", 0)));
    m_tech_final_composite_no_tonemapping.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("composite_no_tonemapping", 0)));
    m_tech_final_composite_tonemapping_no_bloom.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("composite_tonemapping_no_bloom", 0)));
    m_tech_final_composite_debug.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("composite_debug", 0)));
    m_tech_compute_particle_fog_color.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("compute_particle_fog", 0)));
    m_light_shafts_render_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("light_shafts_render", 0)));
    m_light_shafts_mask_tech.set_ref(library->get_shader_user_technique_index(m_hdr_shader_handle, crc_stri("light_shafts_mask", 0)));
    m_sampling_offsets_const.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Sampling_offsets", 0)));
    m_adapt_time_const.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Adapt_time", 0)));
    m_hdr_level_const.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("HDR_level", 0)));
    m_luminance_conversion_const.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Luminance_conversion", 0)));
    m_luminance_max.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Luminance_max", 0)));
    m_luminance_min.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Luminance_min", 0)));
    m_luminance_mask_max.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Luminance_mask_max", 0)));
    m_brightpass_threshold.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Brightpass_threshold", 0)));
    m_brightpass_offset.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Brightpass_offset", 0)));
    m_use_hdr_level_const.set_ref(library->get_shader_bool_index(m_hdr_shader_handle, crc_stri("Use_hdr_level", 0)));
    m_eye_adaption_base.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Eye_adaption_base", 0)));
    m_eye_adaption_amount.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Eye_adaption_amount", 0)));
    m_eye_fade_min_const.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Eye_fade_min", 0)));
    m_eye_fade_max_const.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Eye_fade_max", 0)));
    m_bloom_amount.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Bloom_amount", 0)));
    m_bloom_curve_values_const.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Bloom_curve_values", 0)));
    m_lum_range.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Lum_range", 0)));
    m_lum_offset.set_ref(library->get_shader_const_index(m_hdr_shader_handle, crc_stri("Lum_offset", 0)));
    m_debug_bloom_buffer.set_ref(library->get_shader_bool_index(m_hdr_shader_handle, crc_stri("Debug_bloom_buffer", 0)));
    m_debug_lummap.set_ref(library->get_shader_bool_index(m_hdr_shader_handle, crc_stri("Debug_lummap", 0)));
    m_debug_avg_lum.set_ref(library->get_shader_bool_index(m_hdr_shader_handle, crc_stri("Debug_avg_lum", 0)));
    m_base_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("base_sampler", 0)));
    m_mask_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("mask_sampler", 0)));
    m_sun_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("sun_sampler", 0)));
    m_particle_brightpass_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("Particle_brightpass_sampler", 0)));
    m_particle_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("Particle_sampler", 0)));
    m_lum_adapt_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("lum_adapt_sampler", 0)));
    m_bloom_stage_0_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("bloom_stage_0_sampler", 0)));
    m_bloom_stage_1_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("bloom_stage_1_sampler", 0)));
    m_bloom_stage_2_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("bloom_stage_2_sampler", 0)));
    m_bloom_stage_3_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("bloom_stage_3_sampler", 0)));
    m_bloom_stage_4_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("bloom_stage_4_sampler", 0)));
    m_light_shafts_brightpass_sampler.set_ref(library->get_shader_tmu_index(m_hdr_shader_handle, crc_stri("Light_shafts_brightpass_sampler", 0)));
    m_downscale_sampling_fullscreen.init(width, height);
    // Magic numbers: the 64x64 sun mask, 1 at the center falling linearly to 0 at radius 31.5 (texel centers at +0.5).
    unsigned char sun_mask[64 * 64];
    for (int y = 0; y < 64; y++)
    {
        const float dy = 32.0f - ((float)y + 0.5f);
        for (int x = 0; x < 64; x++)
        {
            const float dx = 32.0f - ((float)x + 0.5f);
            float value = (31.5f - sqrtf(dx * dx + dy * dy)) / 31.5f;
            if (value - 1.0f >= 0.0f)
            {
                value = 1.0f;
            }
            if (0.0f - value >= 0.0f)
            {
                value = 0.0f;
            }
            sun_mask[y * 64 + x] = (unsigned char)(int)(value * 255.0f);
        }
    }
    keen::TextureDescription description;
    description.flags = 1;
    description.gpuAccessMode = 1;
    description.type = 1;
    description.format = 0;
    description.lutFormat = 0;
    description.multiSampleType = 0;
    description.addressModeU = 0;
    description.width = 64;
    description.height = 64;
    description.depth = 1;
    description.addressModeV = 0;
    description.addressModeW = 0;
    description.levelCount = 1;
    description.cpuAccessMode = 0;
    m_sun_mask_texture.pData = create_keen_texture_data(Init_params.m_pKeenGraphicsSystem, description, RL_FORMAT_R8_UNORM, sun_mask, true, "Texture2d");
    m_target_adapted_lum_buffer = 0;
}

// 0x0054B200 (:634)
// The downsampled source (and sun shaft targets), the luminance chain cleared to opaque black, the adapted luminance
// targets cleared to 0xFF4D (r 1.0, g ~0.3), and the bright pass and bloom stages when bloom is enabled.
void rl_hdr_compositor::create_render_textures(keen::GraphicsCommandWriter* pCommandWriter)
{
    m_downsampled_source.init(m_downsampled_width, m_downsampled_height, RL_FORMAT_HDR, 0, "DwnsmpldSource", 0);
    // Magic number: opaque black.
    clearTarget(&m_downsampled_source, 0xFF000000, pCommandWriter);
    if (m_level_of_detail.m_sun_shafts_enabled)
    {
        m_downsampled_sun_shafts.init(m_downsampled_width, m_downsampled_height, RL_FORMAT_HDR, 0, "DwnsmpldSunShafts", 0);
        m_downsampled_sun_shafts_int.init(m_downsampled_width, m_downsampled_height, RL_FORMAT_HDR, 0, "DwnsmpldSunShaftsInt", 0);
    }
    init_luminance_target(m_luminance_stage_0, LUM_STAGE_0_WIDTH, LUM_STAGE_0_HEIGHT, "LumStage0");
    clearTarget(&m_luminance_stage_0, 0xFF000000, pCommandWriter);
    init_luminance_target(m_downsampled_lum_0, LUM_DOWN_SAMPLE_WIDTH, LUM_DOWN_SAMPLE_HEIGHT, "dslum0");
    clearTarget(&m_downsampled_lum_0, 0xFF000000, pCommandWriter);
    init_luminance_target(m_downsampled_lum_1, LUM_STAGE_1_WIDTH, LUM_STAGE_1_HEIGHT, "dslum1");
    clearTarget(&m_downsampled_lum_1, 0xFF000000, pCommandWriter);
    init_luminance_target(m_luminance_stage_1, LUM_STAGE_1_WIDTH, LUM_STAGE_1_HEIGHT, "LumStage1");
    clearTarget(&m_luminance_stage_1, 0xFF000000, pCommandWriter);
    for (int i = 0; i < (int)(rl_render_lib::get_instance()->m_num_gpus * 2); i++)
    {
        init_luminance_target(m_adapted_lum_p[i], LUM_ADAPT_WIDTH, LUM_ADAPT_HEIGHT, "adapted_lum_X");
        // Magic number: the initial adapted luminance (packed 0xAABBGGRR).
        clearTarget(&m_adapted_lum_p[i], 0x4DFF, pCommandWriter);
    }
    if (m_level_of_detail.m_bloom_enabled)
    {
        // rl_render_texture::init, inlined for the bright pass targets.
        for (int i = 0; i < 2; i++)
        {
            m_brightpass[i].init(m_downsampled_width, m_downsampled_height, RL_FORMAT_HDR, 0, "brightpass", 0);
        }
        for (int i = 0; i < 2; i++)
        {
            m_bloom_stage_1[i].init(m_bloom_stage_1_width, m_bloom_stage_1_height, RL_FORMAT_HDR, 0, "bloomStage1", 0);
        }
        for (int i = 0; i < 2; i++)
        {
            m_bloom_stage_2[i].init(m_bloom_stage_2_width, m_bloom_stage_2_height, RL_FORMAT_HDR, 0, "bloomStage2", 0);
        }
        for (int i = 0; i < 2; i++)
        {
            m_bloom_stage_3[i].init(m_bloom_stage_3_width, m_bloom_stage_3_height, RL_FORMAT_HDR, 0, "bloomStage3", 0);
        }
        for (int i = 0; i < 2; i++)
        {
            m_bloom_stage_4[i].init(m_bloom_stage_4_width, m_bloom_stage_4_height, RL_FORMAT_HDR, 0, "bloomStage4", 0);
        }
    }
}

// 0x00525970 (:165)
void rl_hdr_compositor::shutdown()
{
    keen::graphics::freeTextureData(m_sun_mask_texture.pData, Init_params.m_pKeenGraphicsSystem, nullptr);
    delete m_sun_mask_texture.pData;
    delete[] m_adapted_lum_p;
}

// 0x0053D4C0 (:562)
// The luminance measurement of the frame (when not done yet), which also computes the particle fog color into
// fog_color_target; the renderer's tint, fade, saturation, macro state and viewport are kept.
void rl_hdr_compositor::calculate_particle_fog_color(rl_renderer& renderer, rl_compositor& compositor, const rl_color_float& fog_color, const rl_dev_render_target& fog_color_target, const int source_tex, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    const rl_color_float saved_tint = renderer.get_tint_color();
    const rl_color_float saved_fade = renderer.get_fade_color();
    const float saved_saturation = renderer.get_saturation();
    const rl_primitive_state saved_state = renderer.get_macro_state();
    if (m_last_luminance_calc != rl_render_lib::get_instance()->m_current_frame_count)
    {
        const rl_dev_texture no_mask;
        compositor.downsample(renderer, source_tex, saved_vp.m_right, saved_vp.m_bottom, m_downsampled_source.get_dev_render_target(), m_downsampled_width, m_downsampled_height, kDownsampleTechnique, no_mask);
        if (hdr_state.m_enable)
        {
            measure_luminance(renderer, compositor, hdr_state, color_correct_state, fog_color, fog_color_target);
        }
    }
    renderer.set_tint_color(saved_tint);
    renderer.set_fade_color(saved_fade);
    renderer.set_saturation(saved_saturation);
    renderer.set_macro_state(saved_state, false);
    renderer.set_viewport(saved_vp);
}

// 0x0054A9F0 (:177)
// The source downsampled and the luminance measured (once per frame), sun shafts and bloom when on, then the final
// composite technique (no tone mapping without HDR; tone mapping with or without bloom) over dest_rt with the source,
// the adapted luminance of the previous frame, the final bloom stage and the particles; the renderer's tint, fade,
// saturation, macro state and viewport are kept.
//
// Notes (decompiled behaviour, kept as is):
//   - The final bloom stage's texture data is fetched and dropped (a call through its dynamic texture, which is null
//     when bloom is off and sun shafts are not drawn: the level of detail must keep bloom on).
void rl_hdr_compositor::composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int particle_texture_handle, const int particle_brightpass_texture_handle, const rl_dev_texture& sunShaftsMaskTex, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, const int source_tex, const rl_dev_render_target& dest_rt)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    const rl_color_float saved_tint = renderer.get_tint_color();
    const rl_color_float saved_fade = renderer.get_fade_color();
    const float saved_saturation = renderer.get_saturation();
    const rl_primitive_state saved_state = renderer.get_macro_state();
    const unsigned int width = saved_vp.m_right;
    const unsigned int height = saved_vp.m_bottom;
    rl_color_float particle_fog_color(0.0f, 0.0f, 0.0f, 0.0f);
    if (m_last_luminance_calc != rl_render_lib::get_instance()->m_current_frame_count)
    {
        const rl_dev_texture no_mask;
        compositor.downsample(renderer, source_tex, width, height, m_downsampled_source.get_dev_render_target(), m_downsampled_width, m_downsampled_height, kDownsampleTechnique, no_mask);
        if (hdr_state.m_enable)
        {
            const rl_dev_render_target no_fog_target;
            measure_luminance(renderer, compositor, hdr_state, color_correct_state, particle_fog_color, no_fog_target);
        }
    }
    const bool sun_shafts = sunShaftsMaskTex.pData != nullptr;
    int technique = m_tech_final_composite_no_tonemapping.get_ref();
    rl_render_texture* final_bloom_stage;
    if (!hdr_state.m_enable)
    {
        final_bloom_stage = &m_brightpass[0];
    }
    else
    {
        if (m_level_of_detail.m_bloom_enabled || sun_shafts)
        {
            technique = hdr_state.m_tone_mapped_bloom ? m_tech_final_composite_tonemapped_bloom.get_ref() : m_tech_final_composite_bloom_after_tonemapping.get_ref();
        }
        else
        {
            technique = m_tech_final_composite_tonemapping_no_bloom.get_ref();
        }        final_bloom_stage = sun_shafts ? &m_downsampled_sun_shafts_int : &m_brightpass[1];
    }
    if (sun_shafts)
    {
        const int sun_source = g_sunShaftsUseHalfResSource ? m_downsampled_source.get_texture_handle() : source_tex;
        render_sun_shafts(renderer, compositor, camera, sun_source, width, height, sunShaftsMaskTex, hdr_state, *final_bloom_stage);
    }
    if (hdr_state.m_enable && m_level_of_detail.m_bloom_enabled)
    {
        bright_pass_filter(renderer, compositor, particle_brightpass_texture_handle, hdr_state, color_correct_state);
        render_bloom_3(renderer, compositor, hdr_state);
        bloom_composite(renderer, compositor, final_bloom_stage->get_dev_render_target(), m_downsampled_width, m_downsampled_height, sun_shafts);
    }
    upload_tone_mapping_constantants(renderer, hdr_state, color_correct_state);
    renderer.set_texture(m_base_sampler.get_ref(), source_tex);
    renderer.set_texture(m_lum_adapt_sampler.get_ref(), m_adapted_lum_p[get_current_adapted_luminance_source_index()].get_texture_handle());
    renderer.set_texture(m_bloom_stage_0_sampler.get_ref(), final_bloom_stage->get_texture_handle());
    renderer.set_texture(m_particle_sampler.get_ref(), particle_texture_handle);
    renderer.set_viewport(saved_vp);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, technique);
    final_bloom_stage->get_texture_data();
    // Magic numbers: clear the alpha (bits 0-4), depth and stencil (bits 6-12) and cull (bits 13-14) modes, clamp on.
    // The base is the renderer's macro state now (read after begin_pass in the binary), not the one saved on entry.
    rl_primitive_state pass_state;
    // pass_state.set_bits(saved_state.get_bits() & 0xFFFF9FE0 & 0xFFFFE03F | 0x20); // OLD
    pass_state.set_bits(renderer.get_macro_state().get_bits() & 0xFFFF9FE0 & 0xFFFFE03F | 0x20);
    // Magic number: clear flags 8.
    draw_fullscreen_pass(renderer, dest_rt, width, height, pass_state, 8);
    renderer.set_tint_color(saved_tint);
    renderer.set_fade_color(saved_fade);
    renderer.set_saturation(saved_saturation);
    renderer.set_macro_state(saved_state, false);
    renderer.set_viewport(saved_vp);
}

// 0x005259E0 (:254)
// The tone mapping constants of the HDR state and the color correction tint (alpha 1) and saturation (second
// saturation 1).
void rl_hdr_compositor::upload_tone_mapping_constantants(rl_renderer& renderer, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state)
{
    renderer.set_pixel_shader_constant(m_luminance_conversion_const.get_ref(), &hdr_state.m_luminance_conversion.x, 3);
    renderer.set_pixel_shader_constant(m_luminance_max.get_ref(), &hdr_state.m_luminance_max, 1);
    renderer.set_pixel_shader_constant(m_luminance_min.get_ref(), &hdr_state.m_luminance_min, 1);
    renderer.set_pixel_shader_constant(m_luminance_mask_max.get_ref(), &hdr_state.m_luminance_mask_max, 1);
    renderer.set_pixel_shader_constant(m_hdr_level_const.get_ref(), &hdr_state.m_bloom_exposure, 1);
    const float use_hdr_level = (float)(unsigned int)(hdr_state.m_use_hdr_level != false);
    renderer.set_pixel_shader_constant(m_use_hdr_level_const.get_ref(), &use_hdr_level, 1);
    renderer.set_pixel_shader_constant(m_brightpass_threshold.get_ref(), &hdr_state.m_brightpass_threshold, 1);
    renderer.set_pixel_shader_constant(m_brightpass_offset.get_ref(), &hdr_state.m_brightpass_offset, 1);
    renderer.set_pixel_shader_constant(m_eye_adaption_base.get_ref(), &hdr_state.m_eye_adaption_base, 1);
    renderer.set_pixel_shader_constant(m_eye_adaption_amount.get_ref(), &hdr_state.m_eye_adaption_amount, 1);
    renderer.set_pixel_shader_constant(m_eye_fade_min_const.get_ref(), &hdr_state.m_eye_fade_min, 1);
    renderer.set_pixel_shader_constant(m_eye_fade_max_const.get_ref(), &hdr_state.m_eye_fade_max, 1);
    renderer.set_pixel_shader_constant(m_bloom_amount.get_ref(), &hdr_state.m_bloom_amount, 1);
    renderer.set_pixel_shader_constant(m_lum_range.get_ref(), &hdr_state.m_lum_range, 1);
    renderer.set_pixel_shader_constant(m_lum_offset.get_ref(), &hdr_state.m_lum_offset, 1);
    // theta, slope A, slope B, theta * slope A.
    const float bloom_curve[4] = {hdr_state.m_bloom_theta, hdr_state.m_bloom_slope_A, hdr_state.m_bloom_slope_B, hdr_state.m_bloom_slope_A * hdr_state.m_bloom_theta};
    renderer.set_pixel_shader_constant(m_bloom_curve_values_const.get_ref(), bloom_curve, 4);
    const rl_color_float& tint = color_correct_state.get_tint();
    renderer.set_tint_color(rl_color_float(tint.red, tint.green, tint.blue, 1.0f));
    renderer.set_saturation(color_correct_state.get_saturation(), 1.0f);
}

// The luminance stages' sample offsets (inlined in both compute_sampling_offsets): 3x3 texels of the 64x64 stage 0
// target at (x - 1) / 64, (y - 1) / 64 (stored at x + 3 * y), and 4x4 of the 4x4 stage 1 at (x - 1.5) / 4.
static void compute_luminance_sampling_offsets(rl_downscale_sampling_offsets<9>& stage_0, rl_downscale_sampling_offsets<16>& stage_1)
{
    for (int x = 0; x < 3; x++)
    {
        for (int y = 0; y < 3; y++)
        {
            // Magic number: 0.015625, one texel of 64.
            stage_0.m_values[x + 3 * y].x = ((float)x - 1.0f) * 0.015625f;
            stage_0.m_values[x + 3 * y].y = ((float)y - 1.0f) * 0.015625f;
        }
    }
    for (int x = 0; x < 4; x++)
    {
        for (int y = 0; y < 4; y++)
        {
            // Magic number: 0.25, one texel of 4.
            stage_1.m_values[x + 4 * y].x = ((float)x - 1.5f) * 0.25f;
            stage_1.m_values[x + 4 * y].y = ((float)y - 1.5f) * 0.25f;
        }
    }
}

// 0x004E7DB0 (:291): the bloom blur kernels (17 samples, rho 3, the weight multiplier) and the luminance offsets.
void rl_hdr_compositor::compute_sampling_offsets()
{
    m_bloom_blur_stage_0_opt.init(m_bloom_stage_0_opt_width, m_bloom_stage_0_opt_height, false, nullptr, multiplier, 3.0f);
    m_bloom_blur_stage_0.init(m_bloom_stage_0_width, m_bloom_stage_0_height, false, nullptr, multiplier, 3.0f);
    m_bloom_blur_stage_1.init(m_bloom_stage_1_width, m_bloom_stage_1_height, false, nullptr, multiplier, 3.0f);
    m_bloom_blur_stage_2.init(m_bloom_stage_2_width, m_bloom_stage_2_height, false, nullptr, multiplier, 3.0f);
    m_bloom_blur_stage_3.init(m_bloom_stage_3_width, m_bloom_stage_3_height, false, nullptr, multiplier, 3.0f);
    m_bloom_blur_stage_4.init(m_bloom_stage_4_width, m_bloom_stage_4_height, false, nullptr, multiplier, 3.0f);
    compute_luminance_sampling_offsets(m_downscale_sampling_lum_stage_0, m_downscale_sampling_lum_stage_1);
}

// 0x0050FD30 (:305): the bilinear bloom blur kernels (normalized, rho 2 for the first stages and 3 for the smaller
// ones) and the luminance offsets.
void rl_hdr_compositor::compute_sampling_offsets_3()
{
    m_bloom_blur_stage_0_opt.init_bilinear(m_bloom_stage_0_opt_width, m_bloom_stage_0_opt_height, true, nullptr, 1.0f, 2.0f);
    m_bloom_blur_stage_0.init_bilinear(m_bloom_stage_0_width, m_bloom_stage_0_height, true, nullptr, 1.0f, 2.0f);
    m_bloom_blur_stage_1.init_bilinear(m_bloom_stage_1_width, m_bloom_stage_1_height, true, nullptr, 1.0f, 3.0f);
    m_bloom_blur_stage_2.init_bilinear(m_bloom_stage_2_width, m_bloom_stage_2_height, true, nullptr, 1.0f, 3.0f);
    m_bloom_blur_stage_3.init_bilinear(m_bloom_stage_3_width, m_bloom_stage_3_height, true, nullptr, 1.0f, 3.0f);
    m_bloom_blur_stage_4.init_bilinear(m_bloom_stage_4_width, m_bloom_stage_4_height, true, nullptr, 1.0f, 3.0f);
    compute_luminance_sampling_offsets(m_downscale_sampling_lum_stage_0, m_downscale_sampling_lum_stage_1);
}

// 0x0053B9A0 (:322): the scene's luminance, 64x64 from the downsampled source (stage 0, 3x3 samples), 16x16 and 4x4
// (downsampled), 4x4 (stage 1, 4x4 samples), then adapted over time (iris rate * frame time, instantly on the first
// frame) into the next adapted luminance target of this GPU's pair; with a fog color target, the particle fog color
// from the new adapted luminance.
void rl_hdr_compositor::measure_luminance(rl_renderer& renderer, rl_compositor& compositor, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state, const rl_color_float& particle_fog_color, const rl_dev_render_target& fog_color_target)
{
    // Magic number: the first adaptation is instant (an adaptation time of 10000).
    static bool first_time = true; // 0x01518650
    // The fullscreen passes: the alpha, depth, stencil and cull modes cleared, clamp on; clear flags 8.
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    pass_state.set_bits(saved_state.get_bits() & 0xFFFF9FE0 & 0xFFFFE03F | 0x20);
    const rl_dev_texture no_mask;

    upload_tone_mapping_constantants(renderer, hdr_state, color_correct_state);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    upload_sampling_offsets(renderer, m_downscale_sampling_lum_stage_0);
    renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, m_luminance_stage_0_tech.get_ref());
    renderer.set_texture(m_base_sampler.get_ref(), m_downsampled_source.get_texture_handle());
    draw_fullscreen_pass(renderer, m_luminance_stage_0.get_dev_render_target(), LUM_STAGE_0_WIDTH, LUM_STAGE_0_HEIGHT, pass_state, 8);
    compositor.downsample(renderer, m_luminance_stage_0.get_texture_handle(), LUM_STAGE_0_WIDTH, LUM_STAGE_0_HEIGHT, m_downsampled_lum_0.get_dev_render_target(), LUM_DOWN_SAMPLE_WIDTH, LUM_DOWN_SAMPLE_HEIGHT, kDownsampleTechnique, no_mask);
    compositor.downsample(renderer, m_downsampled_lum_0.get_texture_handle(), LUM_DOWN_SAMPLE_WIDTH, LUM_DOWN_SAMPLE_HEIGHT, m_downsampled_lum_1.get_dev_render_target(), LUM_STAGE_1_WIDTH, LUM_STAGE_1_HEIGHT, kDownsampleTechnique, no_mask);

    upload_sampling_offsets(renderer, m_downscale_sampling_lum_stage_1);
    renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, m_luminance_stage_1_tech.get_ref());
    renderer.set_texture(m_base_sampler.get_ref(), m_downsampled_lum_1.get_texture_handle());
    draw_fullscreen_pass(renderer, m_luminance_stage_1.get_dev_render_target(), LUM_STAGE_1_WIDTH, LUM_STAGE_1_HEIGHT, pass_state, 8);

    float adapt_time = hdr_state.m_iris_rate * Frametime;
    if (first_time)
    {
        adapt_time = 10000.0f;
        first_time = false;
    }    const int target_index = get_current_adapted_luminance_target_index();
    const int source_index = get_current_adapted_luminance_source_index();
    renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, m_luminance_adapt_tech.get_ref());
    renderer.set_pixel_shader_constant(m_adapt_time_const.get_ref(), &adapt_time, 1);
    renderer.set_texture(m_base_sampler.get_ref(), m_luminance_stage_1.get_texture_handle());
    renderer.set_texture(m_lum_adapt_sampler.get_ref(), m_adapted_lum_p[source_index].get_texture_handle());
    draw_fullscreen_pass(renderer, m_adapted_lum_p[target_index].get_dev_render_target(), LUM_ADAPT_WIDTH, LUM_ADAPT_HEIGHT, pass_state, 8);
    // The target just written becomes the source of the next frame.
    set_current_adapted_luminance_target_index(source_index);

    if (fog_color_target.pData != nullptr)
    {
        const rl_color_float saved_fog_color = renderer.get_fog_color();
        renderer.set_fog_color(particle_fog_color);
        renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, m_tech_compute_particle_fog_color.get_ref());
        renderer.set_texture(m_lum_adapt_sampler.get_ref(), m_adapted_lum_p[get_current_adapted_luminance_source_index()].get_texture_handle());
        draw_fullscreen_pass(renderer, fog_color_target, 1, 1, pass_state, 8);
        renderer.set_fog_color(saved_fog_color);
    }
    m_last_luminance_calc = rl_render_lib::get_instance()->m_current_frame_count;
}

// 0x004E8080 (:533)
void rl_hdr_compositor::upload_sampling_offsets(rl_renderer& renderer, const rl_sampling_offsets& offsets)
{
    renderer.set_pixel_shader_constant(m_sampling_offsets_const.get_ref(), &offsets.m_values_p->x, offsets.m_num_samples * 4);
}

// 0x0053C720 (:391): the bright parts of the downsampled source (and the particles' bright pass) above the adapted
// luminance into the first bright pass target, at the downsampled size.
void rl_hdr_compositor::bright_pass_filter(rl_renderer& renderer, rl_compositor& compositor, const int particle_brightpass_texture_handle, const rl_hdr_state& hdr_state, const rl_color_correct_state& color_correct_state)
{
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    int technique = hdr_state.m_bloom_alternate ? m_brightpass_tech_alt.get_ref() : m_brightpass_tech.get_ref();
    if (hdr_state.m_bloom_new)
    {
        technique = m_brightpass_tech_alt2.get_ref();
    }
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    renderer.set_viewport(rl_rect(0, 0, m_downsampled_width, m_downsampled_height));
    upload_tone_mapping_constantants(renderer, hdr_state, color_correct_state);
    renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, technique);
    renderer.set_texture(m_base_sampler.get_ref(), m_downsampled_source.get_texture_handle());
    renderer.set_texture(m_particle_brightpass_sampler.get_ref(), particle_brightpass_texture_handle);
    renderer.set_texture(m_lum_adapt_sampler.get_ref(), m_adapted_lum_p[get_current_adapted_luminance_source_index()].get_texture_handle());
    rl_primitive_state pass_state;
    pass_state.set_bits(renderer.get_macro_state().get_bits() & 0xFFFF9FE0 & 0xFFFFE03F | 0x20);
    draw_fullscreen_pass(renderer, m_brightpass[0].get_dev_render_target(), m_downsampled_width, m_downsampled_height, pass_state, 8);
    renderer.set_viewport(saved_vp);
}

// 0x0053CAF0 (:422)
void rl_hdr_compositor::create_bloom_texture(rl_renderer& renderer, rl_compositor& compositor, const int src_texture, int src_width, int src_height, const rl_dev_render_target& dest_texture, int dest_width, int dest_height)
{
    const rl_dev_texture no_mask;
    compositor.downsample(renderer, src_texture, src_width, src_height, dest_texture, dest_width, dest_height, kDownsampleTechnique, no_mask);
}

// 0x0053CB30 (:429): a separable blur of tex[0] through tex[1], twice with RL_hdr_bloom_super_soft.
void rl_hdr_compositor::blur_bloom_texture(rl_renderer& renderer, rl_compositor& compositor, rl_render_texture* tex, int width, int height, const rl_sampling_offsets& kernel)
{
    compositor.gaussian_blur(renderer, tex[0].get_texture_handle(), tex[1], tex[0].get_dev_render_target(), width, height, kernel);
    if (RL_hdr_bloom_super_soft)
    {
        compositor.gaussian_blur(renderer, tex[0].get_texture_handle(), tex[1], tex[0].get_dev_render_target(), width, height, kernel);
    }
}

// 0x0053CBE0 (:439)
void rl_hdr_compositor::render_bloom(rl_renderer& renderer, rl_compositor& compositor, const rl_hdr_state& hdr_state)
{
    keen::debug::nativePrintArgs("rl_hdr_compositor::render_bloom: not ported (V5)\n");
}

// 0x0053CE50 (:476): the bright pass blurred, and with soft bloom four more stages, each downsampled from the previous
// one and blurred.
void rl_hdr_compositor::render_bloom_3(rl_renderer& renderer, rl_compositor& compositor, const rl_hdr_state& hdr_state)
{
    compute_sampling_offsets_3();
    blur_bloom_texture(renderer, compositor, m_brightpass, m_bloom_stage_0_opt_width, m_bloom_stage_0_opt_height, m_bloom_blur_stage_0_opt);
    if (!hdr_state.m_bloom_soft)
    {
        return;
    }
    const rl_dev_texture no_mask;
    compositor.downsample(renderer, m_brightpass[0].get_texture_handle(), m_bloom_stage_0_opt_width, m_bloom_stage_0_opt_height, m_bloom_stage_1[0].get_dev_render_target(), m_bloom_stage_1_width, m_bloom_stage_1_height, kDownsampleTechnique, no_mask);
    blur_bloom_texture(renderer, compositor, m_bloom_stage_1, m_bloom_stage_1_width, m_bloom_stage_1_height, m_bloom_blur_stage_1);
    compositor.downsample(renderer, m_bloom_stage_1[0].get_texture_handle(), m_bloom_stage_1_width, m_bloom_stage_1_height, m_bloom_stage_2[0].get_dev_render_target(), m_bloom_stage_2_width, m_bloom_stage_2_height, kDownsampleTechnique, no_mask);
    blur_bloom_texture(renderer, compositor, m_bloom_stage_2, m_bloom_stage_2_width, m_bloom_stage_2_height, m_bloom_blur_stage_2);
    compositor.downsample(renderer, m_bloom_stage_2[0].get_texture_handle(), m_bloom_stage_2_width, m_bloom_stage_2_height, m_bloom_stage_3[0].get_dev_render_target(), m_bloom_stage_3_width, m_bloom_stage_3_height, kDownsampleTechnique, no_mask);
    blur_bloom_texture(renderer, compositor, m_bloom_stage_3, m_bloom_stage_3_width, m_bloom_stage_3_height, m_bloom_blur_stage_3);
    compositor.downsample(renderer, m_bloom_stage_3[0].get_texture_handle(), m_bloom_stage_3_width, m_bloom_stage_3_height, m_bloom_stage_4[0].get_dev_render_target(), m_bloom_stage_4_width, m_bloom_stage_4_height, kDownsampleTechnique, no_mask);
    blur_bloom_texture(renderer, compositor, m_bloom_stage_4, m_bloom_stage_4_width, m_bloom_stage_4_height, m_bloom_blur_stage_4);
}

// 0x0053D090 (:511): the five bloom stages combined into dest_rt with a white tint, added over it (without alpha) when
// the sun shafts are already there.
void rl_hdr_compositor::bloom_composite(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& dest_rt, const unsigned int width, const unsigned int height, bool sun_shafts_enabled)
{
    const rl_color_float saved_tint = renderer.get_tint_color();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_texture(m_bloom_stage_0_sampler.get_ref(), m_brightpass[0].get_texture_handle());
    renderer.set_texture(m_bloom_stage_1_sampler.get_ref(), m_bloom_stage_1[0].get_texture_handle());
    renderer.set_texture(m_bloom_stage_2_sampler.get_ref(), m_bloom_stage_2[0].get_texture_handle());
    renderer.set_texture(m_bloom_stage_3_sampler.get_ref(), m_bloom_stage_3[0].get_texture_handle());
    renderer.set_texture(m_bloom_stage_4_sampler.get_ref(), m_bloom_stage_4[0].get_texture_handle());
    renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, m_bloom_composite_3_tech.get_ref());
    const rl_dev_depth_stencil_target no_ds;
    compositor.render(renderer, dest_rt, width, height, sun_shafts_enabled ? CBLEND_ADDITIVE_NO_ALPHA : CBLEND_NONE, no_ds, 0);
    renderer.set_tint_color(saved_tint);
}

// 0x004E80E0 (:546)
int rl_hdr_compositor::get_current_adapted_luminance_target_index()
{
    return m_target_adapted_lum_buffer + get_luminance_frame_index();
}

// 0x004E8100 (:552)
int rl_hdr_compositor::get_current_adapted_luminance_source_index()
{
    return (m_target_adapted_lum_buffer ^ 1) + get_luminance_frame_index();
}

// 0x004C1F40 (:557)
void rl_hdr_compositor::set_current_adapted_luminance_target_index(const int idx)
{
    m_target_adapted_lum_buffer = idx & 1;
}

// 0x00510000 (:596)
// The render targets of every texture (the adapted luminance ones of all GPUs too).
void rl_hdr_compositor::release_resources()
{
    m_downsampled_source.destroy();
    m_downsampled_sun_shafts.destroy();
    m_downsampled_sun_shafts_int.destroy();
    m_luminance_stage_0.destroy();
    m_downsampled_lum_0.destroy();
    m_downsampled_lum_1.destroy();
    m_luminance_stage_1.destroy();
    for (int i = 0; i < 2; i++)
    {
        for (int gpu = 0; gpu < (int)rl_render_lib::get_instance()->m_num_gpus; gpu++)
        {
            // rl_render_texture::destroy, inlined.
            m_adapted_lum_p[i + gpu * 2].destroy();
        }
        m_brightpass[i].destroy();
        m_bloom_stage_1[i].destroy();
        m_bloom_stage_2[i].destroy();
        m_bloom_stage_3[i].destroy();
        m_bloom_stage_4[i].destroy();
    }
}

// 0x0054F390 (:618)
// create_render_textures, inlined.
void rl_hdr_compositor::renew_resources(keen::GraphicsCommandWriter* pCommandWriter)
{
    create_render_textures(pCommandWriter);
}

// 0x004E83E0 (:681)
// The downsampled (4:1) and bloom stage sizes (each stage half the previous, rounded) and the fullscreen offsets.
void rl_hdr_compositor::resize(int w, int h)
{
    int width = get_downsampled_width(w);
    int height = get_downsampled_height(h);
    m_downsampled_width = width;
    m_bloom_stage_0_opt_width = width;
    m_bloom_stage_0_width = width;
    m_downsampled_height = height;
    m_bloom_stage_0_opt_height = height;
    m_bloom_stage_0_height = height;
    // Magic numbers: half size, rounded to nearest.
    width = (int)((float)width * 0.5f + 0.5f);
    height = (int)((float)height * 0.5f + 0.5f);
    m_bloom_stage_1_width = width;
    m_bloom_stage_1_height = height;
    width = (int)((float)width * 0.5f + 0.5f);
    height = (int)((float)height * 0.5f + 0.5f);
    m_bloom_stage_2_width = width;
    m_bloom_stage_2_height = height;
    width = (int)((float)width * 0.5f + 0.5f);
    height = (int)((float)height * 0.5f + 0.5f);
    m_bloom_stage_3_width = width;
    m_bloom_stage_3_height = height;
    m_bloom_stage_4_width = (int)((float)width * 0.5f + 0.5f);
    m_bloom_stage_4_height = (int)((float)height * 0.5f + 0.5f);
    m_downscale_sampling_fullscreen.init(w, h);
}

// 0x004C1F50 (:711)
void rl_hdr_compositor::set_level_of_detail(const rl_level_of_detail& lod)
{
    m_level_of_detail = lod;
}

// Unbinds every pixel and vertex shader resource directly on the D3D context (bypassing the command writer), as
// render_sun_shafts does before each of its passes.
static void unbind_shader_resources(rl_renderer& renderer)
{
    // Magic number: 16, every pixel and vertex shader resource slot.
    ID3D11ShaderResourceView* null_views[16] = {};
    ID3D11DeviceContext* context_p = renderer.getCommandWriter()->getBuffer()->pContext;
    context_p->PSSetShaderResources(0, 16, null_views);
    context_p->VSSetShaderResources(0, 16, null_views);
}

// 0x0053D7A0 (:716)
// The sun shafts into final_bloom_stage, at the downsampled size:
//   1. the shaft source into m_downsampled_sun_shafts: with Rl_sun_shafts_sun_mask the light_shafts_mask technique over
//      the source, the sky mask and the sun mask texture around the sun (the fog parameters carry the sun size), else
//      the source downsampled through the sky mask;
//   2. that blurred (bilinear 5x5 kernel) through m_downsampled_sun_shafts_int;
//   3. the light_shafts_render technique (the tint carries scale, base luminance and luminance step, the fog color the
//      radius and the color curve) over the blurred shafts into final_bloom_stage, overwriting it (no blending).
// The bloom is added over final_bloom_stage afterwards (bloom_composite), so step 3 is what keeps it from
// accumulating across frames.
// The fog parameters, fog color and tint are restored; the msaa resolve flags of the targets are not kept (the
// render textures here have no msaa texture).
void rl_hdr_compositor::render_sun_shafts(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int source_tex, const int src_width, const int src_height, const rl_dev_texture& sunShaftsMaskTex, const rl_hdr_state& hdr_state, rl_render_texture& final_bloom_stage)
{
    const int width = m_downsampled_width;
    const int height = m_downsampled_height;
    unbind_shader_resources(renderer);
    if (!Rl_sun_shafts_sun_mask)
    {
        compositor.downsample(renderer, source_tex, src_width, src_height, m_downsampled_sun_shafts.get_dev_render_target(), width, height, kDownsampleMaskTechnique, sunShaftsMaskTex);
    }
    else
    {
        renderer.set_world_transform(IDENTITY_MATRIX43);
        camera.render_refresh(renderer);
        renderer.set_dummy_light_pos(hdr_state.m_sun_shafts_position);
        float fog_height_density;
        float fog_atmosphere_scale;
        float fog_density;
        float fog_density_offset;
        renderer.get_fog_parameters(fog_height_density, fog_atmosphere_scale, fog_density, fog_density_offset);
        renderer.set_fog_parameters(Rl_sun_shafts_sun_size, Rl_sun_shafts_sun_size, 0.0f, 0.0f);
        upload_sampling_offsets(renderer, m_downscale_sampling_fullscreen);
        renderer.set_shader_type(shader_type_generic);
        renderer.set_shader_configuration(0);
        renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, m_light_shafts_mask_tech.get_ref());
        renderer.set_texture(m_base_sampler.get_ref(), source_tex);
        renderer.set_texture(m_mask_sampler.get_ref(), sunShaftsMaskTex);
        renderer.set_texture(m_sun_sampler.get_ref(), m_sun_mask_texture);
        // Magic numbers: clear the alpha (bits 0-4), depth and stencil (bits 6-12) and cull (bits 13-14) modes, clamp
        // on; clear flags 8.
        rl_primitive_state pass_state;
        pass_state.set_bits(renderer.get_macro_state().get_bits() & 0xFFFF9FE0 & 0xFFFFE03F | 0x20);
        draw_fullscreen_pass(renderer, m_downsampled_sun_shafts.get_dev_render_target(), width, height, pass_state, 8);
        renderer.set_fog_parameters(fog_height_density, fog_atmosphere_scale, fog_density, fog_density_offset);
    }

    rl_gaussian5x5_blur_kernel kernel;
    kernel.init_bilinear(width, height, true, nullptr, Rl_sun_shafts_blur_multiplier, Rl_sun_shafts_blur_rho);
    compositor.gaussian_blur(renderer, m_downsampled_sun_shafts.get_texture_handle(), m_downsampled_sun_shafts_int, m_downsampled_sun_shafts.get_dev_render_target(), width, height, kernel);

    unbind_shader_resources(renderer);
    renderer.set_world_transform(IDENTITY_MATRIX43);
    camera.render_refresh(renderer);
    renderer.set_dummy_light_pos(hdr_state.m_sun_shafts_position);
    const rl_color_float saved_tint = renderer.get_tint_color();
    const rl_color_float saved_fog_color = renderer.get_fog_color();
    // Magic number: 0.15625, the shaft scale factor.
    renderer.set_tint_color(rl_color_float((float)((double)Rl_sun_shafts_scale * 0.15625), Rl_sun_shafts_base_lum, Rl_sun_shafts_lum_step_scale, 1.0f));
    renderer.set_fog_color(rl_color_float(Rl_sun_shafts_radius, Rl_sun_shafts_color_curve_base, Rl_sun_shafts_color_curve_exp, Rl_sun_shafts_color_curve_shift));
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_hdr_shader_handle, rl_shader_technique_user, m_light_shafts_render_tech.get_ref());
    renderer.set_texture(m_light_shafts_brightpass_sampler.get_ref(), get_dev_texture(m_downsampled_sun_shafts.get_texture_handle()));
    rl_primitive_state pass_state;
    pass_state.set_bits(renderer.get_macro_state().get_bits() & 0xFFFF9FE0 & 0xFFFFE03F | 0x20);
    draw_fullscreen_pass(renderer, final_bloom_stage.get_dev_render_target(), width, height, pass_state, 8);
    renderer.set_fog_color(saved_fog_color);
    renderer.set_tint_color(saved_tint);
}

// 0x004C1F70 (:791)
void rl_hdr_compositor::save_textures(rl_scene_renderer& scene_renderer)
{
}
