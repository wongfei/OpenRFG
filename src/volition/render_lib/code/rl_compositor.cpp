// Original: volition/render_lib/code/rl_compositor.cpp
//
// rl_compositor: fullscreen passes over render targets (a quad drawn with the composite vertex format): plain blends,
// 4:1 downsampling (box 4x4, filtered, or masked), the stencil mask, and simple / separated gaussian blurs.

#include "volition/render_lib/code/rl_compositor.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/render_lib/code/rl_state_manager.h"
#include "volition/vlib/code/math/crc.h"
#include "keen/graphics/graphicscommandwriter.hpp"

// <rl_compositor::release> 0x004BCA80 (:8)
// <rl_compositor::create> 0x0050AD00 (:8)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_compositor)

// Not in the binary: the fullscreen pass every compositor function inlines. The target with the given depth stencil,
// a full-target viewport with the full depth range and target dimensions, the pass state, a clear (color 0), the
// composite quad, then the renderer's state, viewport and depth range back.
static void draw_fullscreen_pass(rl_renderer& renderer, const rl_dev_render_target& target, const rl_dev_depth_stencil_target& ds_surface, unsigned int width, unsigned int height, const rl_primitive_state& pass_state, unsigned int clear_flags)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    renderer.begin_pass(target, ds_surface);
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

// The pass state of the downsample and blur passes: the current state with clamp on and the alpha, depth, stencil
// and cull modes cleared (color write and up kept).
static rl_primitive_state plain_pass_state(rl_renderer& renderer)
{
    rl_primitive_state state;
    // Magic numbers: keep bits 5 and 15-31, set clamp (0x20).
    state.set_bits(renderer.get_macro_state().get_bits() & 0xFFFF8020 | 0x20);
    return state;
}

// 0x004E36D0 (:29)
rl_compositor::rl_compositor()
{
}

// 0x004BCAA0 (:34)
rl_compositor::~rl_compositor()
{
}

// 0x00522510 (:39)
// The shader handles, techniques and constant / sampler slots of the downsample, stencil mask and blur effects, and
// the fullscreen 4x4 offsets.
void rl_compositor::intialize(int width, int height)
{
    rl_shader_library* library = rl_shader_library::get_instance();
    m_downsample_effect = library->lookup(crc_stri("rl_downsample", 0));
    m_downscale_techniques[0].set_ref(library->get_shader_user_technique_index(m_downsample_effect, crc_stri("box 4x4", 0)));
    m_downscale_techniques[1].set_ref(library->get_shader_user_technique_index(m_downsample_effect, crc_stri("box 4x4 filtered", 0)));
    m_downscale_techniques[2].set_ref(library->get_shader_user_technique_index(m_downsample_effect, crc_stri("box 4x4 masked", 0)));
    m_downscale_sampling_offsets_const.set_ref(library->get_shader_const_index(m_downsample_effect, crc_stri("Sampling_offsets", 0)));
    m_downscale_base_sampler.set_ref(library->get_shader_tmu_index(m_downsample_effect, crc_stri("base_sampler", 0)));
    m_downscale_mask_sampler.set_ref(library->get_shader_tmu_index(m_downsample_effect, crc_stri("mask_sampler", 0)));
    m_downscale_sampling_fullscreen.init(width, height);
    m_mask_stencil_effect = library->lookup(crc_stri("rl_mask_stencil", 0));
    m_blur_effect = library->lookup(crc_stri("rl_gaussian_blur", 0));
    m_gaussian_blur_tech.set_ref(library->get_shader_user_technique_index(m_blur_effect, crc_stri("Separated Gaussian", 0)));
    m_simple_blur_tech.set_ref(library->get_shader_user_technique_index(m_blur_effect, crc_stri("Simple", 0)));
    m_gaussian_sampling_offsets_const.set_ref(library->get_shader_const_index(m_blur_effect, crc_stri("Sampling_offsets", 0)));
    m_gaussian_separated_axis_const.set_ref(library->get_shader_const_index(m_blur_effect, crc_stri("Separated_axis", 0)));
}

// 0x004BCAB0 (:64)
void rl_compositor::shutdown()
{
}

// 0x00537AC0 (:68)
// The current shader over the target with the blend mode's alpha mode (opaque modes also clear color, flag 8); depth
// write without stencil unless it is the stencil mask pass or there is no depth stencil.
void rl_compositor::render(rl_renderer& renderer, const rl_dev_render_target& target, const unsigned int width, const unsigned int height, rl_compositor_blend_mode mode, const rl_dev_depth_stencil_target& ds_surface, unsigned int clear_flags) const
{
    rl_primitive_state state = renderer.get_macro_state();
    if (mode == CBLEND_ADDITIVE)
    {
        state.set_alpha_mode(RLPAM_ALPHA_ADDITIVE);
    }
    else if (mode == CBLEND_ADDITIVE_NO_ALPHA)
    {
        state.set_alpha_mode(RLPAM_ADDITIVE);
    }
    else if (mode == CBLEND_BLEND_PREMULT)
    {
        // Magic number: alpha mode 18.
        state.set_alpha_mode((rl_prim_alpha_mode)18);
    }
    else if (mode == CBLEND_PREMULT_ALPHA)
    {
        // Magic number: alpha mode 13.
        state.set_alpha_mode((rl_prim_alpha_mode)13);
    }
    else if (mode == CBLEND_PREMULT_ALPHA_INVERSE)
    {
        VLIB_CRITICAL_FAILURE();
    }
    else if (mode == CBLEND_ALPHA_SUBTRACTIVE)
    {
        // Magic number: alpha mode 10.
        state.set_alpha_mode((rl_prim_alpha_mode)10);
    }
    else
    {
        state.set_alpha_mode(RLPAM_NONE);
        clear_flags |= 8;
    }
    unsigned int bits = state.get_bits();
    if (mode == CBLEND_STENCIL_MASK || ds_surface.pData == nullptr)
    {
        // Magic numbers: no depth test (bits 6-8) and no culling (bits 13-14).
        bits &= 0xFFFF9E3F;
    }
    else
    {
        // Magic numbers: depth mode 3 (0xC0), no culling.
        bits = bits & 0xFFFF9EFF | 0xC0;
    }
    // The stencil mode (bits 9-12): 2 for the stencil mask pass, else 0; clamp on (0x20).
    bits = (((mode == CBLEND_STENCIL_MASK ? 2u : 0u) << 9 ^ bits) & 0x1E00 ^ bits) | 0x20;
    rl_primitive_state pass_state;
    pass_state.set_bits(bits);
    draw_fullscreen_pass(renderer, target, ds_surface, width, height, pass_state, clear_flags);
}

// 0x00537D00 (:138)
// The source scaled 4:1 into the destination with one of the box techniques (technique 2 also samples maskTex);
// white tint.
void rl_compositor::downsample(rl_renderer& renderer, const int source_tex_handle, const unsigned int source_width, const unsigned int source_height, const rl_dev_render_target& dest_rt, const unsigned int dest_width, const unsigned int dest_height, int technique, const rl_dev_texture& maskTex) const
{
    if (technique >= 3)
    {
        return;
    }
    renderer.set_pixel_shader_constant(m_downscale_sampling_offsets_const.get_ref(), &m_downscale_sampling_fullscreen.m_values_p->x, m_downscale_sampling_fullscreen.m_num_samples * 4);
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_downsample_effect, rl_shader_technique_user, m_downscale_techniques[technique].get_ref());
    renderer.set_texture(m_downscale_base_sampler.get_ref(), source_tex_handle);
    if (technique == 2)
    {
        renderer.m_pCommandWriter->writeSetFragmentShaderTextureCommand(m_downscale_mask_sampler.get_ref(), maskTex.pData);
    }
    const rl_dev_depth_stencil_target no_ds;
    // Magic number: clear flags 8.
    draw_fullscreen_pass(renderer, dest_rt, no_ds, dest_width, dest_height, plain_pass_state(renderer), 8);
}

// 0x00537FD0 (:162)
// The stencil mask shader into the destination, with the stencil test (mode 2) against src_ds.
void rl_compositor::mask_stencil(rl_renderer& renderer, const rl_dev_render_target& dest_rt, const rl_dev_depth_stencil_target src_ds, const unsigned int dest_width, const unsigned int dest_height) const
{
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_mask_stencil_effect, rl_shader_technique_user, 0);
    rl_primitive_state pass_state;
    // Magic numbers: keep bits 5, 10 and 15-31; stencil mode 2 (0x400) and clamp (0x20).
    pass_state.set_bits(renderer.get_macro_state().get_bits() & 0xFFFF8420 | 0x420);
    // Magic number: clear flags 9.
    draw_fullscreen_pass(renderer, dest_rt, src_ds, dest_width, dest_height, pass_state, 9);
}

// 0x00538180 (:173)
// One axis of the separated gaussian blur.
void rl_compositor::separated_gaussian_blur(rl_renderer& renderer, int src, const rl_dev_render_target& dst, const unsigned int width, const unsigned int height, const vector4& blur_axis) const
{
    renderer.set_pixel_shader_constant(m_gaussian_separated_axis_const.get_ref(), &blur_axis.x, 4);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_blur_effect, rl_shader_technique_user, 0);
    renderer.m_pCommandWriter->writeSetFragmentShaderTextureCommand(0, get_dev_texture(src).pData);
    const rl_dev_depth_stencil_target no_ds;
    // Magic number: clear flags 8.
    draw_fullscreen_pass(renderer, dst, no_ds, width, height, plain_pass_state(renderer), 8);
}

// 0x005383C0 (:184)
// Horizontal into intermediate, then vertical into destination, with a white tint (restored after).
void rl_compositor::gaussian_blur(rl_renderer& renderer, const int source, rl_render_texture& intermediate, const rl_dev_render_target& destination, const unsigned int width, const unsigned int height, const rl_sampling_offsets& kernel) const
{
    const rl_color_float saved_tint = renderer.get_tint_color();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_pixel_shader_constant(m_gaussian_sampling_offsets_const.get_ref(), &kernel.m_values_p->x, kernel.m_num_samples * 4);
    separated_gaussian_blur(renderer, source, intermediate.get_dev_render_target(), width, height, vector4(1.0f, 0.0f, 0.0f, 0.0f));
    separated_gaussian_blur(renderer, intermediate.get_texture_handle(), destination, width, height, vector4(0.0f, 1.0f, 0.0f, 0.0f));
    renderer.set_tint_color(saved_tint);
}

// 0x00538610 (:200)
// The simple blur technique with the kernel's offsets, white tint (restored after).
void rl_compositor::simple_blur(rl_renderer& renderer, const int source, const rl_dev_render_target& destination, const unsigned int width, const unsigned int height, const rl_sampling_offsets& kernel) const
{
    const rl_color_float saved_tint = renderer.get_tint_color();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_pixel_shader_constant(m_gaussian_sampling_offsets_const.get_ref(), &kernel.m_values_p->x, kernel.m_num_samples * 4);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_blur_effect, rl_shader_technique_user, m_simple_blur_tech.get_ref());
    renderer.m_pCommandWriter->writeSetFragmentShaderTextureCommand(0, get_dev_texture(source).pData);
    const rl_dev_depth_stencil_target no_ds;
    // Magic number: clear flags 8.
    draw_fullscreen_pass(renderer, destination, no_ds, width, height, plain_pass_state(renderer), 8);
    renderer.set_tint_color(saved_tint);
}

// 0x004E3750 (:216)
// rl_downscale_sampling_offsets<16>::init, inlined.
void rl_compositor::resize(int w, int h)
{
    m_downscale_sampling_fullscreen.init(w, h);
}
