// Original: volition/render_lib/code/rl_distortion.cpp
//
// rl_distortion: the screen space distortion composite (heat haze and shock waves from the distortion target).
//
// The shader slots are ported since V3, the composite since the post effect options pass (config.ini
// distortion_enabled).

#include "volition/render_lib/code/rl_distortion.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_compositor.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/math/crc.h"

// The distortion strength and the blur of the distorted pixels (dev console tweakables).
float RL_distortion_scale = 0.09f; // 0x014FA138
float RL_distortion_blur_scale = 2.0f; // 0x014FA13C

// 0x004BEB30 (:15)
rl_distortion::rl_distortion()
{
}

// 0x004BEB50 (:20)
rl_distortion::~rl_distortion()
{
}

// 0x00523F00 (:25)
void rl_distortion::initialize()
{
    rl_shader_library* library = rl_shader_library::get_instance();
    m_distortion_effect = library->lookup(crc_stri("rl_distortion", 0));
    m_distortion_scale.set_ref(library->get_shader_const_index(m_distortion_effect, crc_stri("Distortion_scale", 0)));
    m_blur_scale.set_ref(library->get_shader_const_index(m_distortion_effect, crc_stri("Blur_scale", 0)));
    m_technique_base.set_ref(library->get_shader_user_technique_index(m_distortion_effect, crc_stri("Base Technique", 0)));
    m_technique_diffraction.set_ref(library->get_shader_user_technique_index(m_distortion_effect, crc_stri("Diffraction Technique", 0)));
}

// 0x0053A4C0 (:38)
// The shader constants: Distortion_scale (the scale, the camera's signal noise frequency and scale) and Blur_scale;
// the diffraction or the base technique. With a white tint the scene goes to sampler 0, the distortion target to 1
// and the blurred scene to 2, drawn as a full screen pass into distortion_target; the tint and the macro state are
// kept.
//
// Notes (decompiled behaviour, kept as is): the w of Distortion_scale is an uninitialized stack slot in rfg.exe (0
// here).
void rl_distortion::composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int backbuffer_texture, const int distortion_texture, const int blurred_texture, const rl_dev_render_target& distortion_target, const int width, const int height, const bool diffraction_enabled)
{
    const rl_color_float saved_tint = renderer.get_tint_color();
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state distortion_state;
    // Magic numbers: bit 16 cleared, bits 15 and 17 set (the color write mode bits).
    distortion_state.set_bits(saved_state.get_bits() & 0xFFFEFFFF | 0x28000);
    renderer.set_macro_state(distortion_state, false);
    // The camera accessors are inlined; the frequency goes through a double (+ 0.0).
    const float distortion_scale[4] = { RL_distortion_scale, (float)((double)camera.get_signal_noise_frequency() + 0.0), camera.get_signal_noise_scale(), 0.0f };
    renderer.set_pixel_shader_constant(m_distortion_scale.get_ref(), distortion_scale, 4);
    renderer.set_pixel_shader_constant(m_blur_scale.get_ref(), &RL_distortion_blur_scale, 1);
    const int technique = diffraction_enabled ? m_technique_diffraction.get_ref() : m_technique_base.get_ref();
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_distortion_effect, rl_shader_technique_user, technique);
    renderer.set_tint_color(rl_color_float(1.0f, 1.0f, 1.0f, 1.0f));
    // rl_renderer::set_texture, inlined.
    renderer.set_texture(0, backbuffer_texture);
    renderer.set_texture(1, distortion_texture);
    renderer.set_texture(2, blurred_texture);
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    const rl_dev_depth_stencil_target no_ds;
    renderer.begin_pass(distortion_target, no_ds);
    const rl_primitive_state pass_saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    // Magic numbers: keep bits 5 and 15-31, clamp on (0x20).
    pass_state.set_bits(pass_saved_state.get_bits() & 0xFFFF8020 | 0x20);
    renderer.set_viewport(rl_rect(0, 0, width, height));
    renderer.set_viewport_z_range(0.0f, 1.0f);
    renderer.set_target_dimensions(width, height);
    renderer.set_macro_state(pass_state, false);
    // Magic number: clear flags 8.
    renderer.clear(rl_color_float(0.0f, 0.0f, 0.0f, 0.0f), 8, 1.0f, 0);
    renderer.set_macro_state(pass_state, false);
    renderer.set_vertex_format(RLVF_COMPOSITOR);
    renderer.draw_primitive(RL_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, 2, vertex_data, sizeof(rl_compositor_vertex));
    renderer.set_macro_state(pass_saved_state, false);
    renderer.end_pass();
    renderer.set_viewport(saved_vp);
    renderer.set_viewport_z_range(z_near, z_far);
    renderer.set_tint_color(saved_tint);
    renderer.set_macro_state(saved_state, false);
}
