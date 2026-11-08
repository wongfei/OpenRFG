// Original: volition/render_lib/code/rl_depth_of_field.h
// Implementation: volition/render_lib/code/rl_depth_of_field.cpp
//
// rl_depth_of_field, the depth of field effect.
#pragma once

#include "volition/render_lib/code/rl_sampling_offsets.h"
#include "volition/render_lib/code/rl_shader.h"

class rl_camera;
class rl_compositor;
struct rl_dev_render_target;
struct rl_dev_texture;
class rl_renderer;

// size 0xBC
class rl_depth_of_field
{
public:
    rl_depth_of_field(); // 0x004E4110 (:19)
    ~rl_depth_of_field(); // 0x004BDC80 (:23)
    void initialize(int width, int height); // 0x00523C90 (:27)
    void composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, int backbuffer_texture, const rl_dev_texture& depth_target, int blurred_texture, const rl_dev_render_target& dof_target, int dof_width, int dof_height); // 0x00539B00 (:40)

private:
    unsigned int m_dof_effect; // 0x00
    rl_shader_const_ref m_near_clip_params; // 0x04
    rl_shader_const_ref m_focal_params; // 0x08
    rl_shader_const_ref m_dof_equation_near; // 0x0C
    rl_shader_const_ref m_dof_lerp_scale; // 0x10
    rl_shader_const_ref m_dof_lerp_bias; // 0x14
    rl_shader_const_ref m_override_blur_percent; // 0x18
    rl_shader_const_ref m_dof_equation_far; // 0x1C
    rl_shader_const_ref m_depth_sampling_offsets_const; // 0x20
    rl_downscale_sampling_offsets<9> m_depth_sampling_offsets; // 0x24
};
