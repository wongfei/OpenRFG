// Original: volition/render_lib/code/rl_distortion.h
// Implementation: volition/render_lib/code/rl_distortion.cpp
//
// rl_distortion, screen distortion (heat haze, shockwaves).
#pragma once

#include "volition/render_lib/code/rl_shader.h"

class rl_camera;
class rl_compositor;
struct rl_dev_render_target;
class rl_renderer;

// size 0x14
class rl_distortion
{
public:
    rl_distortion(); // 0x004BEB30 (:15)
    ~rl_distortion(); // 0x004BEB50 (:20)
    void initialize(); // 0x00523F00 (:25)
    void composite(rl_renderer& renderer, rl_compositor& compositor, rl_camera& camera, const int backbuffer_texture, const int distortion_texture, const int blurred_texture, const rl_dev_render_target& distortion_target, const int width, const int height, const bool diffraction_enabled); // 0x0053A4C0 (:38)

private:
    unsigned int m_distortion_effect; // 0x00
    rl_shader_const_ref m_distortion_scale; // 0x04
    rl_shader_const_ref m_blur_scale; // 0x08
    rl_shader_technique_ref m_technique_base; // 0x0C
    rl_shader_technique_ref m_technique_diffraction; // 0x10
};
