// Original: volition/render_lib/code/rl_motion_blur.h
// Implementation: volition/render_lib/code/rl_motion_blur.cpp
//
// rl_motion_blur, the motion blur effect.
#pragma once

#include "volition/render_lib/code/rl_shader.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class rl_camera;
class rl_compositor;
struct rl_dev_render_target;
struct rl_dev_texture;
class rl_renderer;

// size 0x6C
class rl_motion_blur
{
public:
    rl_motion_blur(); // 0x004C3AD0 (:11)
    ~rl_motion_blur(); // 0x004C3AF0 (:15)
    void initialize(); // 0x005294F0 (:20)
    void composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int backbuffer_texture, const rl_dev_texture& depth_texture, const rl_dev_render_target& motion_blur_target, const int width, const int height); // 0x0053FF80 (:38)

private:
    unsigned int m_motion_blur_effect; // 0x00
    rl_shader_technique_ref m_camera_based_tech; // 0x04
    rl_shader_technique_ref m_radial_tech; // 0x08
    rl_shader_const_ref m_curr_to_prev_const; // 0x0C
    rl_shader_const_ref m_blur_scale; // 0x10
    rl_shader_const_ref m_blur_clamp; // 0x14
    rl_shader_const_ref m_radial_blur_radius; // 0x18
    rl_shader_const_ref m_radial_blur_position; // 0x1C
    matrix44 m_view_proj_prev; // 0x20
    vector m_render_offset_prev; // 0x60
};
