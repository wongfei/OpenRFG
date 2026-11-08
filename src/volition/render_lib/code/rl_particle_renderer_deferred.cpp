// Original: volition/render_lib/code/rl_particle_renderer_deferred.cpp
//
// The deferred particle passes of rl_particle_renderer: by particle LOD the quarter and half resolution particles
// (composited up into the full resolution target), the distortion pass, the full resolution particles and the
// particle bright pass. The HDR composite reads the full resolution target as scene * alpha + color, so the pass
// that clears it (to alpha 1) is required even without particles.
// Coronas are drawn with the half resolution particles at the low and medium LODs, with the full resolution ones
// above (V3 ported the very high path and the bright pass; VFX.md FX2 the rest).

#include "volition/render_lib/code/rl_particle_renderer.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_compositor.h"
#include "volition/render_lib/code/rl_corona_instance.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shader.h"
#include "keen/base/trace.hpp"
#include "keen/graphics/graphicscommandwriter.hpp"

#include <string.h>

// 0x0054C750 (:15)
// By LOD: the quarter resolution particles (low), the half resolution ones (all but very high), the distortion pass,
// the quarter composite into the half target (low), the half target stretched into the full one (all but very high),
// the full resolution particles and the bright pass.
void rl_particle_renderer::render_deferred(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const rl_particle_framebuffer_state& fullscreen, const rl_particle_framebuffer_state& halfscreen, const rl_particle_brightpass_state& brightpass, const rl_particle_distortion_state& distortion, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint)
{
    if (m_level_of_detail == RL_PARTICLE_LOD_LOW)
    {
        downsample_depth(renderer, compositor, halfscreen.m_depth_texture, halfscreen.m_width, halfscreen.m_height, m_quarter_depth_target.get_dev_render_target(), m_quarter_ds_surface, m_quarter_width, m_quarter_height);
        render_quarter_res_particles(renderer, camera, lighting_state, fog_color_texture_handle, particle_tint);
    }
    if (m_level_of_detail != RL_PARTICLE_LOD_VERY_HIGH)
    {
        render_half_res_particles(renderer, camera, halfscreen.m_render_target_p->get_dev_render_target(), halfscreen.m_depth_stencil, halfscreen.m_depth_texture, halfscreen.m_width, halfscreen.m_height, lighting_state, fog_color_texture_handle, particle_tint);
    }
    if (distortion.m_enabled)
    {
        // The depth of the target the distortion target matches in size.
        const bool half = distortion.m_width == halfscreen.m_width;
        const rl_dev_depth_stencil_target distortion_ds = half ? halfscreen.m_depth_stencil : fullscreen.m_depth_stencil;
        const rl_dev_texture distortion_depth = half ? halfscreen.m_depth_texture : fullscreen.m_depth_texture;
        render_distortion(renderer, camera, distortion.m_render_target, distortion_ds, distortion_depth, distortion.m_width, distortion.m_height, distortion.m_material, *distortion.m_batch_p, *distortion.m_distortion_only_entries_p, distortion.m_vis_flags, lighting_state, fog_color_texture_handle);
    }
    if (m_level_of_detail == RL_PARTICLE_LOD_LOW)
    {
        composite_quarter_res_to_half_res(renderer, compositor, halfscreen.m_render_target_p->get_dev_render_target(), halfscreen.m_width, halfscreen.m_height);
    }
    if (m_level_of_detail != RL_PARTICLE_LOD_VERY_HIGH)
    {
        const int half_res_texture = halfscreen.m_render_target_p->get_texture_handle();
        stretch_half_res_to_full_res(renderer, compositor, fullscreen.m_render_target_p->get_dev_render_target(), fullscreen.m_width, fullscreen.m_height, half_res_texture);
    }
    render_full_res_particles(renderer, camera, fullscreen.m_render_target_p->get_dev_render_target(), fullscreen.m_depth_stencil, fullscreen.m_depth_texture, fullscreen.m_width, fullscreen.m_height, lighting_state, fog_color_texture_handle, particle_tint);
    if (brightpass.m_enabled)
    {
        render_particle_brightpass(renderer, compositor, brightpass.m_render_target, brightpass.m_width, brightpass.m_height, fullscreen.m_render_target_p->get_texture_handle());
    }
}

// 0x00542170 (:61): the half resolution depth into the quarter resolution depth target with the downsample shader
// (four samples half a destination texel apart; z min 0, range 1).
void rl_particle_renderer::downsample_depth(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_texture& src_depth, const int src_width, const int src_height, const rl_dev_render_target& dst_depth, const rl_dev_depth_stencil_target& dst_ds_surface, const int dst_width, const int dst_height)
{
    // Magic numbers: half a destination texel.
    const float half_x = 0.5f / (float)dst_width;
    const float half_y = 0.5f / (float)dst_height;
    const float offsets[16] = { 0.0f, 0.0f, 0.0f, 0.0f, half_x, 0.0f, 0.0f, 0.0f, 0.0f, half_y, 0.0f, 0.0f, half_x, half_y, 0.0f, 0.0f };
    renderer.set_pixel_shader_constant(m_downscale_sampling_offsets_const.get_ref(), offsets, 16);
    const float z_min_and_range[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
    renderer.set_pixel_shader_constant(m_downscale_z_min_and_range_const.get_ref(), z_min_and_range, 4);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_downsample_depth_effect, rl_shader_technique_user, 0);
    renderer.set_texture(m_downscale_depth_sampler_const.get_ref(), src_depth);
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    renderer.begin_pass(dst_depth, dst_ds_surface);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    // Magic numbers (0x0054238F): keep bits 15-31, clamp on (0x20); zbuf NONE without a depth surface, else WRITE (0xC0).
    pass_state.set_bits((saved_state.get_bits() & 0xFFFF8000) | 0x20 | (dst_ds_surface.pData != nullptr ? 0xC0 : 0));
    renderer.set_viewport(rl_rect(0, 0, dst_width, dst_height));
    renderer.set_viewport_z_range(0.0f, 1.0f);
    renderer.set_target_dimensions(dst_width, dst_height);
    renderer.set_macro_state(pass_state, false);
    // Magic number: clear flags 8.
    renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 0.0f), 8, 1.0f, 0);
    renderer.set_macro_state(pass_state, false);
    renderer.set_vertex_format(RLVF_COMPOSITOR);
    renderer.draw_primitive(RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 2, vertex_data, sizeof(rl_compositor_vertex));
    renderer.set_macro_state(saved_state, false);
    renderer.end_pass();
    renderer.set_viewport(saved_vp);
    renderer.set_viewport_z_range(z_near, z_far);
}

// 0x0052B2C0 (:107)
// The particle pass state (alpha mode 13, or 17 with mod_alpha; read depth, no culling; the color write, scissor,
// msaa and zbias of the current state kept) and the particle shader constants: the camera's inverse projection
// (transposed) and the depth dimensions.
//
// Notes (decompiled behaviour, kept as is): zDimensions.x and .y are 0 / width and 0 / height (always 0).
void rl_particle_renderer::set_deferred_render_states(rl_renderer& renderer, rl_camera& camera, const int depth_width, const int depth_height, const bool mod_alpha)
{
    rl_primitive_state state;
    // Magic numbers: alpha mode 0xD (+ 4 with mod_alpha), depth mode read (0x40) and clamp (0x20).
    state.set_bits(renderer.get_macro_state().get_bits() & 0xFF838060 | ((unsigned int)mod_alpha * 4 + 0xD) | 0x60);
    renderer.set_macro_state(state, false);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    matrix44 inverse_projection = camera.get_projection_transform();
    inverse_projection.inverse();
    const matrix44& inverse = inverse_projection;
    float transposed[16];
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            transposed[row * 4 + col] = inverse.element(col, row);
        }
    }
    memcpy(&m_fragmentData.invProjTM, transposed, sizeof(transposed));
    const float zx = 0.0f / (float)depth_width;
    const float zy = 0.0f / (float)depth_height;
    m_fragmentData.zDimensions.x = zx;
    m_fragmentData.zDimensions.y = zy;
    memcpy(&m_vertexData.invProjTM, &m_fragmentData.invProjTM, sizeof(transposed));
    m_vertexData.zDimensions.x = zx;
    m_vertexData.zDimensions.y = zy;
    m_fragmentData.zDimensions.z = z_near;
    m_fragmentData.zDimensions.w = 1.0f / (z_far - z_near);
    m_vertexData.zDimensions.z = (float)depth_width;
    m_vertexData.zDimensions.w = (float)depth_height;
}

// The coronas of the frame, each in its three phases.
static void render_corona_entries(rl_particle_renderer& particle_renderer, rl_renderer& renderer, rl_camera& camera, const base_array<rl_corona_instance*>& entries)
{
    for (int i = 0; i < entries.size(); i++)
    {
        particle_renderer.render(renderer, camera, entries[i]);
    }
}

// 0x0052B520 (:143): the quarter resolution particles (low LOD) into the quarter target, cleared to alpha 1, over the
// downsampled depth.
void rl_particle_renderer::render_quarter_res_particles(rl_renderer& renderer, rl_camera& camera, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    renderer.begin_pass(m_quarter_res_rt.get_dev_render_target(), m_quarter_ds_surface);
    renderer.set_viewport(rl_rect(0, 0, m_quarter_width, m_quarter_height));
    // Magic number: clear flags 1, color.
    renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 1.0f), 1, 1.0f, 0);
    set_deferred_render_states(renderer, camera, m_quarter_width, m_quarter_height, true);
    renderer.set_tint_color(particle_tint);
    renderer.set_texture(1, get_dev_texture(m_quarter_depth_target.get_texture_handle()));
    renderer.set_texture(2, fog_color_texture_handle);
    for (int i = 0; i < m_quarter_res_entries.size(); i++)
    {
        render(renderer, camera, m_quarter_res_entries[i], lighting_state, false);
    }
    renderer.end_pass();
    renderer.set_viewport(saved_vp);
}

// 0x0052B880 (:176): the half resolution particles into the half target, cleared to alpha 1: at the low and medium
// LODs the half resolution particles and the coronas, above the low LOD also the quarter resolution particles.
void rl_particle_renderer::render_half_res_particles(rl_renderer& renderer, rl_camera& camera, const rl_dev_render_target& half_res_rt, const rl_dev_depth_stencil_target& half_ds_surface, const rl_dev_texture& half_depth_target, const int half_width, const int half_height, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    renderer.begin_pass(half_res_rt, half_ds_surface);
    renderer.set_viewport(rl_rect(0, 0, half_width, half_height));
    renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 1.0f), 1, 1.0f, 0);
    set_deferred_render_states(renderer, camera, half_width, half_height, true);
    renderer.set_tint_color(particle_tint);
    renderer.set_texture(1, half_depth_target);
    renderer.set_texture(2, fog_color_texture_handle);
    if (m_level_of_detail < RL_PARTICLE_LOD_HIGH)
    {
        for (int i = 0; i < m_half_res_entries.size(); i++)
        {
            render(renderer, camera, m_half_res_entries[i], lighting_state, false);
        }
        render_corona_entries(*this, renderer, camera, m_corona_entries);
    }
    if (m_level_of_detail != RL_PARTICLE_LOD_LOW)
    {
        for (int i = 0; i < m_quarter_res_entries.size(); i++)
        {
            render(renderer, camera, m_quarter_res_entries[i], lighting_state, false);
        }
    }
    renderer.end_pass();
    renderer.set_viewport(saved_vp);
}

// 0x005424E0 (:218): the quarter target composited (premultiplied blend) into the half target with the up-res shader;
// the tint is white meanwhile.
void rl_particle_renderer::composite_quarter_res_to_half_res(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& half_res_rt, const int half_width, const int half_height)
{
    const rl_color_float saved_tint = renderer.get_tint_color();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_up_res_effect, rl_shader_technique_user, 0);
    renderer.set_texture(m_up_res_sampler_const.get_ref(), get_dev_texture(m_quarter_res_rt.get_texture_handle()));
    const rl_dev_depth_stencil_target no_ds;
    compositor.render(renderer, half_res_rt, half_width, half_height, CBLEND_BLEND_PREMULT, no_ds, 0);
    renderer.set_tint_color(saved_tint);
}

// 0x00542720 (:236): the half target stretched over the full target (replacing it) with the up-res shader; the tint
// is white meanwhile.
void rl_particle_renderer::stretch_half_res_to_full_res(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& full_res_rt, const int full_res_width, const int full_res_height, const int half_res_rt)
{
    const rl_color_float saved_tint = renderer.get_tint_color();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_up_res_effect, rl_shader_technique_user, 0);
    renderer.set_texture(m_up_res_sampler_const.get_ref(), get_dev_texture(half_res_rt));
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    const rl_dev_depth_stencil_target no_ds;
    renderer.begin_pass(full_res_rt, no_ds);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    // Magic numbers: keep bits 5 and 15-31, clamp on.
    pass_state.set_bits(saved_state.get_bits() & 0xFFFF8020 | 0x20);
    renderer.set_viewport(rl_rect(0, 0, full_res_width, full_res_height));
    renderer.set_viewport_z_range(0.0f, 1.0f);
    renderer.set_target_dimensions(full_res_width, full_res_height);
    renderer.set_macro_state(pass_state, false);
    renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 0.0f), 8, 1.0f, 0);
    renderer.set_macro_state(pass_state, false);
    renderer.set_vertex_format(RLVF_COMPOSITOR);
    renderer.draw_primitive(RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 2, vertex_data, sizeof(rl_compositor_vertex));
    renderer.set_macro_state(saved_state, false);
    renderer.end_pass();
    renderer.set_viewport(saved_vp);
    renderer.set_viewport_z_range(z_near, z_far);
    renderer.set_tint_color(saved_tint);
}

// 0x0052BC20 (:254)
// Into the full resolution target with the scene depth: the particle states and tint, the depth texture (pixel slot
// 1) and the fog color (slot 2); at the very high LOD the target is cleared to transparent black with alpha 1 first
// (no half resolution pass wrote it) and the half and quarter resolution particles are drawn here too, then the full
// resolution ones.
void rl_particle_renderer::render_full_res_particles(rl_renderer& renderer, rl_camera& camera, const rl_dev_render_target& full_res_rt, const rl_dev_depth_stencil_target& ds_surface, const rl_dev_texture& full_res_depth_target, const int full_width, const int full_height, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle, const rl_color_float& particle_tint)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    renderer.begin_pass(full_res_rt, ds_surface);
    renderer.set_viewport(rl_rect(0, 0, full_width, full_height));
    set_deferred_render_states(renderer, camera, full_width, full_height, true);
    renderer.set_tint_color(particle_tint);
    renderer.set_texture(1, full_res_depth_target);
    renderer.set_texture(2, fog_color_texture_handle);
    if (m_level_of_detail == RL_PARTICLE_LOD_VERY_HIGH)
    {
        // Magic number: clear flags 1, color.
        renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 1.0f), 1, 1.0f, 0);
    }
    if (m_level_of_detail > RL_PARTICLE_LOD_MED)
    {
        for (int i = 0; i < m_half_res_entries.size(); i++)
        {
            render(renderer, camera, m_half_res_entries[i], lighting_state, false);
        }
        render_corona_entries(*this, renderer, camera, m_corona_entries);
    }
    if (m_level_of_detail == RL_PARTICLE_LOD_VERY_HIGH)
    {
        for (int i = 0; i < m_quarter_res_entries.size(); i++)
        {
            render(renderer, camera, m_quarter_res_entries[i], lighting_state, false);
        }
    }
    for (int i = 0; i < m_full_res_entries.size(); i++)
    {
        render(renderer, camera, m_full_res_entries[i], lighting_state, false);
    }
    renderer.end_pass();
    renderer.set_viewport(saved_vp);
}

// 0x00542A90 (:304)
// The particles' bright parts into the bright pass target: the downscale offsets, the bright pass shader over the
// full resolution particles with the tint (0.85, 0.3, 2, 3) as its parameters (the tint is restored).
void rl_particle_renderer::render_particle_brightpass(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& target, const int width, const int height, const int full_res_tex)
{
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    const rl_color_float saved_tint = renderer.get_tint_color();
    // Magic numbers: the bright pass parameters in the tint.
    renderer.set_tint_color(rl_color_float(0.85f, 0.3f, 2.0f, 3.0f));
    renderer.set_pixel_shader_constant(m_brightpass_downscale_sampling_offsets_const.get_ref(), &m_brightpass_downscale_sampling.m_values_p->x, m_brightpass_downscale_sampling.m_num_samples * 4);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_particle_brightpass_effect, rl_shader_technique_user, 0);
    renderer.set_texture(m_brightpass_sampler.get_ref(), full_res_tex);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    const rl_dev_depth_stencil_target no_ds;
    renderer.begin_pass(target, no_ds);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    // Magic numbers: keep bits 5 and 15-31, clamp on.
    pass_state.set_bits(saved_state.get_bits() & 0xFFFF8020 | 0x20);
    renderer.set_viewport(rl_rect(0, 0, width, height));
    renderer.set_viewport_z_range(0.0f, 1.0f);
    renderer.set_target_dimensions(width, height);
    renderer.set_macro_state(pass_state, false);
    // Magic number: clear flags 8.
    renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 0.0f), 8, 1.0f, 0);
    renderer.set_macro_state(pass_state, false);
    renderer.set_vertex_format(RLVF_COMPOSITOR);
    renderer.draw_primitive(RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 2, vertex_data, sizeof(rl_compositor_vertex));
    renderer.set_macro_state(saved_state, false);
    renderer.end_pass();
    renderer.set_viewport_z_range(z_near, z_far);
    renderer.set_viewport(saved_vp);
    renderer.set_tint_color(saved_tint);
}

// 0x0052C000 (:330)
void rl_particle_renderer::render_distortion(rl_renderer& renderer, rl_camera& camera, const rl_dev_render_target& distortion_target, rl_dev_depth_stencil_target distortion_ds_surface, const rl_dev_texture& distortion_depth_texture, const int width, const int height, const unsigned int distortion_mat, rl_batched_pass& scene_batch, base_array<rl_renderable_instance*>& distortion_only_entries, const unsigned int vis_flags, const rl_particle_lighting_state& lighting_state, const int fog_color_texture_handle)
{
    const rl_color_float saved_fog_color = renderer.get_fog_color();
    float fog_height_density;
    float fog_atmosphere_scale;
    float fog_density;
    float fog_density_offset;
    renderer.get_fog_parameters(fog_height_density, fog_atmosphere_scale, fog_density, fog_density_offset);
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    // Magic numbers: the distortion strength in the tint (blue x 10).
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 10.0f, 1.0f));
    renderer.set_active_camera(&camera);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    // Magic numbers: the distortion pass state bits.
    pass_state.set_bits(saved_state.get_bits() & 0xFFFFBE70 | 0x2050);
    renderer.set_macro_state(pass_state, false);
    renderer.set_world_transform(IDENTITY_MATRIX43);
    camera.render_refresh(renderer);
    renderer.begin_pass(distortion_target, distortion_ds_surface);
    renderer.set_viewport(rl_rect(0, 0, width, height));
    // Magic number: the "no distortion" color (0.5, 0.5 offsets, alpha 1).
    renderer.clear(rl_color(0xFF808000u), 1, 1.0f, 0);
    set_deferred_render_states(renderer, camera, width, height, true);
    renderer.set_texture(1, distortion_depth_texture);
    renderer.set_texture(2, fog_color_texture_handle);
    for (int i = 0; i < m_distortion_entries.size(); i++)
    {
        render(renderer, camera, m_distortion_entries[i], lighting_state, true);
    }
    render_mesh_distortion(renderer, camera, scene_batch, distortion_only_entries, distortion_mat, vis_flags);
    renderer.end_pass();
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    renderer.set_fog_color(saved_fog_color);
    renderer.set_fog_parameters(fog_height_density, fog_atmosphere_scale, fog_density, fog_density_offset);
    renderer.set_macro_state(saved_state, false);
    renderer.set_viewport(saved_vp);
    if (camera.m_type == rl_camera::RLCT_PERSPECTIVE)
    {
        camera.setup_perspective(renderer);
    }
    else
    {
        camera.setup_ortho(renderer);
    }
    const vector& render_offset = renderer.get_render_offset();
    const vector& camera_position = camera.get_position();
    renderer.set_eye_position(vector(camera_position.x - render_offset.x, camera_position.y - render_offset.y, camera_position.z - render_offset.z));
}
