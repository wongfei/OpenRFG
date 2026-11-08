// Original: volition/render_lib/code/rl_motion_blur.cpp
//
// rl_motion_blur: the camera based and radial motion blur composites.
//
// The shader slots are ported since V3, the composite since VFX.md FX5. Dead in rfg.exe: nothing calls composite
// (no call, jump or pointer to 0x0053FF80 anywhere in the image; rl_scene_renderer::render_scene_part2 has no motion
// blur pass), so the camera's motion blur and the radial blur that fb_accum_blur.cpp feeds it are never drawn. It is
// ported whole and left uncalled, as in the original.

#include "volition/render_lib/code/rl_motion_blur.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_compositor.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/math/crc.h"

// 0x004C3AD0 (:11)
rl_motion_blur::rl_motion_blur()
{
}

// 0x004C3AF0 (:15)
rl_motion_blur::~rl_motion_blur()
{
}

// 0x005294F0 (:20)
// The rl_motion_blur shader slots; the previous render offset starts at zero.
void rl_motion_blur::initialize()
{
    rl_shader_library* library = rl_shader_library::get_instance();
    m_motion_blur_effect = library->lookup(crc_stri("rl_motion_blur", 0));
    m_camera_based_tech.set_ref(library->get_shader_user_technique_index(m_motion_blur_effect, crc_stri("Camera based", 0)));
    m_radial_tech.set_ref(library->get_shader_user_technique_index(m_motion_blur_effect, crc_stri("Radial", 0)));
    m_curr_to_prev_const.set_ref(library->get_shader_const_index(m_motion_blur_effect, crc_stri("Curr_to_prev", 0)));
    m_blur_scale.set_ref(library->get_shader_const_index(m_motion_blur_effect, crc_stri("Blur_scale", 0)));
    m_blur_clamp.set_ref(library->get_shader_const_index(m_motion_blur_effect, crc_stri("Blur_clamp", 0)));
    m_radial_blur_radius.set_ref(library->get_shader_const_index(m_motion_blur_effect, crc_stri("Radial_blur_radius", 0)));
    m_radial_blur_position.set_ref(library->get_shader_const_index(m_motion_blur_effect, crc_stri("Radial_blur_position", 0)));
    m_render_offset_prev = ZERO_VECTOR;
}

// 0x0053FF80 (:38)
// Never called in rfg.exe (file header). The current view projection, taken relative to the previous frame's render
// offset so that both frames are in one space, is inverted and carried into the previous frame's clip space
// (Curr_to_prev, transposed for the shader); then this frame's view projection and render offset are kept for the
// next. The camera's blur amount, the clamp, the inverse radial radius and the radial position in texture space go to
// the shader, with the radial or the camera based technique by the camera's blur type, the back buffer in sampler 0
// and the depth in sampler 1, drawn as a full screen pass into the motion blur target.
//
// Approximation: the matrix products use matrix44::operator* (rfg.exe's inlined products keep some sums in double);
// GPU input only.
void rl_motion_blur::composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int backbuffer_texture, const rl_dev_texture& depth_texture, const rl_dev_render_target& motion_blur_target, const int width, const int height)
{
    matrix44 inverse_view_proj = camera.get_game_view_transform(m_render_offset_prev) * camera.get_projection_transform();
    inverse_view_proj.inverse();
    const matrix44 curr_to_prev = (inverse_view_proj * m_view_proj_prev).copy_transpose();
    m_view_proj_prev = camera.get_game_view_transform(renderer.get_render_offset()) * camera.get_projection_transform();
    m_render_offset_prev = renderer.get_render_offset();
    // Magic number: the blur clamp (0.07, at 0x012BEBE8).
    const float blur_clamp = 0.07f;
    const float blur_scale = camera.get_motion_blur_amount();
    const rl_camera::motion_blur_type type = camera.get_motion_blur_type();
    const float inverse_radius = 1.0f / camera.get_radial_blur_radius();
    const vector2& radial_position = camera.get_radial_blur_position();
    const float position[2] = { radial_position.x / (float)width, radial_position.y / (float)height };
    // Magic number: 16 floats (the 4x4 matrix).
    renderer.set_pixel_shader_constant(m_curr_to_prev_const.get_ref(), &curr_to_prev.element(0, 0), 16);
    renderer.set_pixel_shader_constant(m_blur_scale.get_ref(), &blur_scale, 1);
    renderer.set_pixel_shader_constant(m_blur_clamp.get_ref(), &blur_clamp, 1);
    renderer.set_pixel_shader_constant(m_radial_blur_radius.get_ref(), &inverse_radius, 1);
    renderer.set_pixel_shader_constant(m_radial_blur_position.get_ref(), position, 2);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    const int technique = type == rl_camera::MOTION_BLUR_TYPE_RADIAL ? m_radial_tech.get_ref() : m_camera_based_tech.get_ref();
    renderer.set_active_shader(m_motion_blur_effect, rl_shader_technique_user, technique);
    // rl_renderer::set_texture, both overloads inlined.
    renderer.set_texture(0, backbuffer_texture);
    renderer.set_texture(1, depth_texture);
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    const rl_dev_depth_stencil_target no_ds;
    renderer.begin_pass(motion_blur_target, no_ds);
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    // Magic numbers: keep bits 5 and 15-31, clamp on (0x20).
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
    renderer.set_viewport(saved_vp);
    renderer.set_viewport_z_range(z_near, z_far);
}
