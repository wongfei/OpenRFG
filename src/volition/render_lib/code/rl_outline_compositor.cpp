// Original: volition/render_lib/code/rl_outline_compositor.cpp
//
// rl_outline_compositor: object outlines. The outlined objects are drawn per layer into a quarter resolution mask
// (with one mask material per layer), an edge detection pass turns the mask into outlines, and the outlines are added
// to the bloom buffer with the layer colors.
//
// Partial port (V3): the shader slots, mask materials and targets and the composite into the bloom buffer are ported;
// the mask layer and edge detection passes print "not ported" (nothing is outlined in the V3 viewer).

#include "volition/render_lib/code/rl_outline_compositor.h"

#include "volition/render_lib/code/rl_compositor.h"
#include "volition/render_lib/code/rl_dyn_texture.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_outline_compositor_defines.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/math/crc.h"
#include "keen/graphics/graphicssystem.hpp"
#include "keen/graphics/rendertarget.hpp"
#include "keen/graphics/texturedata.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/rendertarget_d3d11.hpp"
#include "library/modules/runtime/graphics/sources/d3d11/texturedata_d3d11.hpp"

#include <stdio.h>

// <rl_outline_compositor::release> 0x004C3B10 (:74)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_outline_compositor)

// The color constants of the composite shader, one per outline color.
static const char* Composite_outline_names[2] = {"Outline_color_0", "Outline_color_1"}; // 0x014FA304

// 0x00529660 (:77)
rl_outline_compositor::rl_outline_compositor()
{
    for (int i = 0; i < 4; i++)
    {
        m_mask_material_handle[i] = 0xFFFFFFFF;
        m_mask_material_p[i] = nullptr;
    }
    m_edge_detect_shader = 0xFFFFFFFF;
    m_composite_outline_shader = 0xFFFFFFFF;
}

// 0x005296F0 (:89)
// The mask target's destructor (it also releases its dynamic texture).
rl_outline_compositor::~rl_outline_compositor()
{
}

// 0x0054F5B0 (:96)
// The edge detect and composite shaders (both required), one mask material per layer (rl_outline_mask with the layer
// index in its constant), and the quarter resolution mask target (pixel format 0x28) and depth stencil.
void rl_outline_compositor::initialize(int width, int height)
{
    rl_shader_library* library = rl_shader_library::get_instance();
    m_edge_detect_shader = library->lookup(crc_stri("rl_edge_detect", 0));
    if (m_edge_detect_shader == 0xFFFFFFFF)
    {
        keen::debug::nativePrintArgs("Could not find edge detect shader!!");
        return;
    }
    // Magic number: the edge detect threshold.
    m_edge_detect_threshold.x = 0.005f;
    m_edge_detect_threshold.y = 0.0f;
    m_edge_detect_threshold.z = 0.0f;
    m_edge_detect_threshold.w = 0.0f;
    m_composite_outline_shader = library->lookup(crc_stri("rl_outline_composite", 0));
    if (m_composite_outline_shader == 0xFFFFFFFF)
    {
        keen::debug::nativePrintArgs("Could not find composite outline shader!!");
        return;
    }
    m_composite_outline_texture_ref.set_ref(library->get_shader_tmu_index(m_composite_outline_shader, crc_stri("Outline_sampler", 0)));
    for (int i = 0; i < 2; i++)
    {
        m_composite_outline_color_ref[i].set_ref(library->get_shader_const_index(m_composite_outline_shader, crc_stri(Composite_outline_names[i], 0)));
    }
    for (int i = 0; i < 4; i++)
    {
        rl_material_data& data = m_mask_material_data[i];
        data.shader_handle = rl_shader_library::get_instance()->lookup(crc_stri("rl_outline_mask", 0));
        char name[256];
        _snprintf_s(name, sizeof(name), _TRUNCATE, "Outline_material_%d", i);
        data.name_checksum = crc_stri(name, 0);
        // Magic number: material flags 0x100.
        data.mat_flags = 0x100;
        data.num_textures = 0;
        data.textures.from_pointer(&m_mask_material_textures[i]);
        m_mask_material_textures[i].data.rt.texture_stage = 0;
        m_mask_material_textures[i].data.rt.texture_flags = 0;
        m_mask_material_textures[i].name_checksum = 0;
        m_mask_material_textures[i].texture_handle = -1;
        data.num_constants = 1;
        data.constant_block.from_pointer(&m_mask_material_consts[i]);
        m_mask_material_consts[i].set(0.0f, 0.0f, 0.0f, (float)i);
        m_mask_material_p[i] = rl_material::create();
        m_mask_material_p[i]->set_material_data(&data);
        m_mask_material_handle[i] = rl_material_library_register(m_mask_material_p[i]);
    }
    m_mask_render_target_width = width >> 2;
    m_mask_render_target_height = height >> 2;
    // The mask target: a dynamic texture over a framebuffer texture and its render target (rl_render_texture::init's
    // steps for an external texture, inlined).
    m_mask_render_target.m_flags = 0;
    if (m_mask_render_target.m_dyn_texture_p == nullptr)
    {
        m_mask_render_target.m_dyn_texture_p = rl_dyn_texture::create();
    }
    keen::TextureDescription description;
    description.width = (uint16)(width >> 2);
    description.height = (uint16)m_mask_render_target_height;
    description.depth = 0;
    // Magic numbers: render target flags 3, 2D, pixel format 0x28, one level, GPU access.
    description.flags = 3;
    description.type = 1;
    description.format = 0x28;
    description.lutFormat = 0;
    description.multiSampleType = 0;
    description.levelCount = 1;
    description.cpuAccessMode = 0;
    description.gpuAccessMode = 1;
    description.addressModeU = 0;
    description.addressModeV = 0;
    description.addressModeW = 0;
    keen::TextureData* pTexture = new keen::TextureData();
    keen::graphics::allocateTextureData(pTexture, Init_params.m_pKeenGraphicsSystem, g_pFramebufferAllocator, description, nullptr, 0, "outlineMask");
    m_mask_render_target.m_dyn_texture_p->m_dev_texture.pData = pTexture;
    keen::RenderTargetBuffer buffer;
    buffer.pDataBuffer = m_mask_render_target.m_dyn_texture_p->get_texture_data().pData;
    buffer.format = (keen::PixelFormat)buffer.pDataBuffer->description.format;
    // Magic numbers: pixel formats 0x2D and 0x37 are the depth formats (a depth buffer instead of a color buffer).
    const bool is_depth = buffer.format == 0x2D || buffer.format == 0x37;
    m_mask_render_target.m_render_target.pData = keen::graphics::createRenderTarget(Init_params.m_pKeenGraphicsSystem, buffer.pDataBuffer->description.width, buffer.pDataBuffer->description.height, is_depth ? nullptr : &buffer, is_depth ? 0 : 1, is_depth ? &buffer : nullptr, keen::TilingMode_None);
    m_mask_ds_surface = rl_render_lib::get_instance()->create_depth_stencil_target(m_mask_render_target_width, m_mask_render_target_height, 0, "outlineMaskDS", 0);
}

// 0x00529720 (:164)
void rl_outline_compositor::shutdown()
{
    for (int i = 0; i < 4; i++)
    {
        if (m_mask_material_p[i] != nullptr)
        {
            rl_material_library_unregister(m_mask_material_p[i]);
            m_mask_material_p[i]->release();
            m_mask_material_p[i] = nullptr;
        }
        m_mask_material_handle[i] = 0xFFFFFFFF;
    }
    m_mask_render_target.destroy();
    if (m_mask_ds_surface.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_mask_ds_surface.pData);
    }
    m_mask_ds_surface.pData = nullptr;
}

// 0x004C3B30 (:183)
bool rl_outline_compositor::any_outline_pass_enabled(const rl_outline_state& outline_state)
{
    for (int i = 0; i < 4; i++)
    {
        if (outline_state.layer[i] != RLOL_DISABLED)
        {
            return true;
        }
    }
    return false;
}

// 0x0054FAF0 (:195)
// The mask of the outlined objects and its edges, when any layer is on.
void rl_outline_compositor::render_outline_edge_mask(rl_renderer& renderer, rl_scene_renderer& scene_renderer, rl_compositor& compositor, rl_camera& camera, rl_light& light, const rl_dev_render_target& render_target, int width, int height, rl_outline_state& outline_state, rl_batched_pass& batched_pass, rl_visibility_manager& visibility_manager, const keen::TextureData* pDepthTexture)
{
    if (any_outline_pass_enabled(outline_state))
    {
        render_mask_layer(renderer, scene_renderer, camera, light, outline_state, batched_pass, visibility_manager, pDepthTexture);
        render_edge_detection(renderer, compositor, render_target, width, height);
    }
}

// 0x0054BF00 (:207)
void rl_outline_compositor::render_mask_layer(rl_renderer& renderer, rl_scene_renderer& scene_renderer, rl_camera& camera, rl_light& light, rl_outline_state& outline_state, rl_batched_pass& batched_pass, rl_visibility_manager& visibility_manager, const keen::TextureData* pDepthTexture)
{
    static bool s_reported = false;
    if (!s_reported)
    {
        s_reported = true;
        keen::debug::nativePrintArgs("rl_outline_compositor::render_mask_layer: not ported (V5)\n");
    }
}

// 0x00541BB0 (:264)
void rl_outline_compositor::render_edge_detection(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& outline_target, const int target_width, const int target_height)
{
    static bool s_reported = false;
    if (!s_reported)
    {
        s_reported = true;
        keen::debug::nativePrintArgs("rl_outline_compositor::render_edge_detection: not ported (V5)\n");
    }
}

// 0x00541FC0 (:291)
// When any layer is on: the outline texture added (alpha additive) into the bloom target with the two outline colors,
// color write mode 4.
void rl_outline_compositor::add_outline_to_buffer(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& bloom_target, const int bloom_width, const int bloom_height, rl_outline_state& outline_state, const int outline_texture_handle)
{
    if (!any_outline_pass_enabled(outline_state))
    {
        return;
    }
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state state;
    // Magic numbers: color write mode bits 15-17 set to 4.
    state.set_bits(saved_state.get_bits() & 0xFFFE7FFF | 0x20000);
    renderer.set_macro_state(state, false);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_composite_outline_shader, rl_shader_technique_base, -1);
    for (int i = 0; i < 2; i++)
    {
        renderer.set_pixel_shader_constant(m_composite_outline_color_ref[i].get_ref(), &outline_state.colors[i].red, 4);
    }
    renderer.set_texture(m_composite_outline_texture_ref.get_ref(), outline_texture_handle);
    const rl_dev_depth_stencil_target no_ds;
    compositor.render(renderer, bloom_target, bloom_width, bloom_height, CBLEND_ADDITIVE, no_ds, 0);
    renderer.set_macro_state(saved_state, false);
}

// 0x00513910 (:330)
void rl_outline_compositor::release_resources()
{
    m_mask_render_target.destroy();
    if (m_mask_ds_surface.pData != nullptr)
    {
        keen::graphics::destroyRenderTarget(Init_params.m_pKeenGraphicsSystem, m_mask_ds_surface.pData);
    }
    m_mask_ds_surface.pData = nullptr;
}

// 0x0054FB60 (:337)
// create_render_target, inlined.
void rl_outline_compositor::renew_resources()
{
    create_render_target();
}

// 0x0054C560 (:342)
void rl_outline_compositor::create_render_target()
{
    m_mask_render_target.init(m_mask_render_target_width, m_mask_render_target_height, RL_FORMAT_LDR, 0, "outlineMask", 0);
    m_mask_ds_surface = rl_render_lib::get_instance()->create_depth_stencil_target(m_mask_render_target_width, m_mask_render_target_height, 0, "outlineMaskDS", 0);
}

// 0x004C3B60 (:324)
void rl_outline_compositor::resize(int w, int h)
{
    m_mask_render_target_width = w >> 2;
    m_mask_render_target_height = h >> 2;
}
