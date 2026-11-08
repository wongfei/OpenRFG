// Original: volition/render_lib/code/rl_depth_of_field.cpp
//
// rl_depth_of_field: the depth of field composite (the sharp and blurred scene blended by the depth's distance from
// the camera's focus range).
//
// The shader slots and the 3x3 depth sampling offsets are ported since V3, the composite since the post effect
// options pass (config.ini depth_of_field_enabled).

#include "volition/render_lib/code/rl_depth_of_field.h"

#include "volition/render_lib/code/rl_camera.h"
#include "volition/render_lib/code/rl_compositor.h"
#include "volition/render_lib/code/rl_primitive_state.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/render_lib/code/rl_shader_library.h"
#include "volition/vlib/code/math/crc.h"

#include <math.h>

float calculate_horizontal_fov_perspective(float vertical_fov, bool use_pixel_aspect_ratio); // 0x004DFB20 (rl_camera.cpp)

// The dev console focus override (off in retail): near start/end and far start/end distances.
bool RL_dof_overide_enable = false; // 0x01B7BC6A
float RL_dof_focus_start_A = 1.0f; // 0x014FA124
float RL_dof_focus_start_B = 3.0f; // 0x014FA128
float RL_dof_focus_end_A = 50.0f; // 0x014FA12C
float RL_dof_focus_end_B = 100.0f; // 0x014FA130

// 0x004E4110 (:19)
rl_depth_of_field::rl_depth_of_field()
{
}

// 0x004BDC80 (:23)
rl_depth_of_field::~rl_depth_of_field()
{
}

// 0x00523C90 (:27)
// The rl_depth_of_field shader slots and the 3x3 depth samples around each texel (sample (x, y) at x + 3 * y).
void rl_depth_of_field::initialize(int width, int height)
{
    rl_shader_library* library = rl_shader_library::get_instance();
    m_dof_effect = library->lookup(crc_stri("rl_depth_of_field", 0));
    m_near_clip_params.set_ref(library->get_shader_const_index(m_dof_effect, crc_stri("Near_clip_params", 0)));
    m_focal_params.set_ref(library->get_shader_const_index(m_dof_effect, crc_stri("Focal_params", 0)));
    m_depth_sampling_offsets_const.set_ref(library->get_shader_const_index(m_dof_effect, crc_stri("Depth_sampling_offsets", 0)));
    m_override_blur_percent.set_ref(library->get_shader_const_index(m_dof_effect, crc_stri("Override_blur_percent", 0)));
    for (int x = 0; x < 3; x++)
    {
        for (int y = 0; y < 3; y++)
        {
            m_depth_sampling_offsets.m_values[x + 3 * y].x = ((float)x - 1.0f) * (1.0f / (float)width);
            m_depth_sampling_offsets.m_values[x + 3 * y].y = ((float)y - 1.0f) * (1.0f / (float)height);
        }
    }
}

// 0x00539B00 (:40)
// The shader constants: Near_clip_params (near, far, near * tan(hfov / 2), near * tan(vfov / 2)), Focal_params (the
// camera's focus range, or the dev console override), the depth sampling offsets and Override_blur_percent (the
// camera's frame override blur, 0, the viewport's z near and 1 / z far). The blurred scene goes to sampler 0, the sharp
// one to 1 and the depth to 2, drawn as a full screen pass into dof_target; the macro state is kept.
void rl_depth_of_field::composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, int backbuffer_texture, const rl_dev_texture& depth_target, int blurred_texture, const rl_dev_render_target& dof_target, int dof_width, int dof_height)
{
    const rl_primitive_state saved_state = renderer.get_macro_state();
    rl_primitive_state dof_state;
    // Magic numbers: bit 16 cleared, bits 15 and 17 set (the color write mode bits).
    dof_state.set_bits(saved_state.get_bits() & 0xFFFEFFFF | 0x28000);
    renderer.set_macro_state(dof_state, false);
    float viewport_z_near;
    float viewport_z_far;
    renderer.get_viewport_z_range(viewport_z_near, viewport_z_far);
    // The camera accessors are inlined.
    const float near_clip = camera.get_near_clip();
    const float far_clip = camera.get_far_clip();
    const float fov = camera.get_fov();
    float horizontal_fov;
    if (camera.get_type() == rl_camera::RLCT_PERSPECTIVE)
    {
        horizontal_fov = calculate_horizontal_fov_perspective(fov, false);
    }
    else
    {
        horizontal_fov = (float)((double)rl_render_lib::get_instance()->get_screen_info()->get_aspect_ratio() * (double)fov);
    }
    // Magic numbers: 0.017453 (degrees to radians, as a float) and 0.5 (the half angle).
    const float tan_half_h = (float)tan((double)(float)((double)horizontal_fov * (double)0.017453f * 0.5));
    const float tan_half_v = (float)tan((double)(float)((double)fov * (double)0.017453f * 0.5));
    const float near_clip_params[4] = { near_clip, far_clip, tan_half_h * near_clip, tan_half_v * near_clip };
    renderer.set_pixel_shader_constant(m_near_clip_params.get_ref(), near_clip_params, 4);
    float focal_params[4];
    if (RL_dof_overide_enable)
    {
        focal_params[0] = RL_dof_focus_start_A;
        focal_params[1] = RL_dof_focus_start_B;
        focal_params[2] = RL_dof_focus_end_A;
        focal_params[3] = RL_dof_focus_end_B;
    }
    else
    {
        focal_params[0] = camera.get_focus_start_A();
        focal_params[1] = camera.get_focus_start_B();
        focal_params[2] = camera.get_focus_end_A();
        focal_params[3] = camera.get_focus_end_B();
    }
    renderer.set_pixel_shader_constant(m_focal_params.get_ref(), focal_params, 4);
    renderer.set_pixel_shader_constant(m_depth_sampling_offsets_const.get_ref(), &m_depth_sampling_offsets.m_values_p->x, m_depth_sampling_offsets.m_num_samples * 4);
    // Notes (decompiled behaviour, kept as is): w is 1 / z far of the viewport's depth range, not 1 / (far - near).
    const float override_blur[4] = { camera.get_frame_override_blur(), 0.0f, viewport_z_near, (float)(1.0 / (double)viewport_z_far) };
    renderer.set_pixel_shader_constant(m_override_blur_percent.get_ref(), override_blur, 4);
    renderer.set_shader_type(shader_type_generic);
    renderer.set_shader_configuration(0);
    renderer.set_active_shader(m_dof_effect, rl_shader_technique_user, 0);
    // rl_renderer::set_texture, both overloads inlined.
    renderer.set_texture(0, blurred_texture);
    renderer.set_texture(1, backbuffer_texture);
    renderer.set_texture(2, depth_target);
    rl_rect saved_vp;
    renderer.get_viewport(saved_vp);
    float z_near;
    float z_far;
    renderer.get_viewport_z_range(z_near, z_far);
    const rl_dev_depth_stencil_target no_ds;
    renderer.begin_pass(dof_target, no_ds);
    const rl_primitive_state pass_saved_state = renderer.get_macro_state();
    rl_primitive_state pass_state;
    // Magic numbers: keep bits 5 and 15-31, clamp on (0x20).
    pass_state.set_bits(pass_saved_state.get_bits() & 0xFFFF8020 | 0x20);
    renderer.set_viewport(rl_rect(0, 0, dof_width, dof_height));
    renderer.set_viewport_z_range(0.0f, 1.0f);
    renderer.set_target_dimensions(dof_width, dof_height);
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
    renderer.set_macro_state(saved_state, false);
}
