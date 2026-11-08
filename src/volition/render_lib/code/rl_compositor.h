// Original: volition/render_lib/code/rl_compositor.h
// Implementation: volition/render_lib/code/rl_compositor.cpp
//
// rl_compositor, the base of full-screen post-processing compositors (blend modes).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_sampling_offsets.h"
#include "volition/render_lib/code/rl_shader.h"

class rl_class_dynamic_info;
struct rl_dev_depth_stencil_target;
struct rl_dev_render_target;
struct rl_dev_texture;
class rl_render_texture;
class rl_renderer;
class vector4;

enum rl_compositor_blend_mode
{
    CBLEND_NONE = 0,
    CBLEND_ADDITIVE = 1,
    CBLEND_ADDITIVE_NO_ALPHA = 2,
    CBLEND_BLEND_PREMULT = 3,
    CBLEND_PREMULT_ALPHA = 4,
    CBLEND_PREMULT_ALPHA_INVERSE = 5,
    CBLEND_ALPHA_SUBTRACTIVE = 6,
    CBLEND_STENCIL_MASK = 7,
    NUM_COMPOSITOR_BLEND_MODES = 8,
};

// size 0x18
struct rl_compositor_vertex
{
    float m_pos[4]; // 0x00
    float m_uv[2]; // 0x10
};

// The fullscreen quad as a triangle strip (clip space, top-left uv origin). A header constant: every compositor pass
// and the scene renderer's blit draw it (one copy at 0x012BEB80 after the linker folds them).
static const rl_compositor_vertex vertex_data[4] = {
    { { -1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
    { { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
    { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
};

// size 0x144
class rl_compositor : public rl_base_object
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
protected:
    rl_compositor(); // 0x004E36D0 (:29)
    virtual ~rl_compositor(); // vtbl+0x00 0x004BCAA0 (:34)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_compositor* create(); // 0x0050AD00 (:8)
    static bool can_create(); // 0x004BCA70 (:8)
    virtual void release(); // vtbl+0x04 0x004BCA80 (:8)
    void intialize(int width, int height); // 0x00522510 (:39)
    void resize(int w, int h); // 0x004E3750 (:216)
    void shutdown(); // 0x004BCAB0 (:64)
    void render(rl_renderer& renderer, const rl_dev_render_target& target, const unsigned int width, const unsigned int height, rl_compositor_blend_mode mode, const rl_dev_depth_stencil_target& ds_surface, unsigned int clear_flags) const; // 0x00537AC0 (:68)
    void downsample(rl_renderer& renderer, const int source_tex_handle, const unsigned int source_width, const unsigned int source_height, const rl_dev_render_target& dest_rt, const unsigned int dest_width, const unsigned int dest_height, int technique, const rl_dev_texture& maskTex) const; // 0x00537D00 (:138)
    void mask_stencil(rl_renderer& renderer, const rl_dev_render_target& dest_rt, const rl_dev_depth_stencil_target src_ds, const unsigned int dest_width, const unsigned int dest_height) const; // 0x00537FD0 (:162)
    void simple_blur(rl_renderer& renderer, const int source, const rl_dev_render_target& destination, const unsigned int width, const unsigned int height, const rl_sampling_offsets& kernel) const; // 0x00538610 (:200)
    void gaussian_blur(rl_renderer& renderer, const int source, rl_render_texture& intermediate, const rl_dev_render_target& destination, const unsigned int width, const unsigned int height, const rl_sampling_offsets& kernel) const; // 0x005383C0 (:184)
private:
    void separated_gaussian_blur(rl_renderer& renderer, int src, const rl_dev_render_target& dst, const unsigned int width, const unsigned int height, const vector4& blur_axis) const; // 0x00538180 (:173)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514900
private:
    unsigned int m_downsample_effect; // 0x08
    rl_downscale_sampling_offsets<16> m_downscale_sampling_fullscreen; // 0x0C
    rl_shader_const_ref m_downscale_sampling_offsets_const; // 0x114
    rl_shader_const_ref m_downscale_base_sampler; // 0x118
    rl_shader_const_ref m_downscale_mask_sampler; // 0x11C
    rl_shader_technique_ref m_downscale_techniques[3]; // 0x120
    unsigned int m_mask_stencil_effect; // 0x12C
    unsigned int m_blur_effect; // 0x130
    rl_shader_technique_ref m_gaussian_blur_tech; // 0x134
    rl_shader_technique_ref m_simple_blur_tech; // 0x138
    rl_shader_const_ref m_gaussian_sampling_offsets_const; // 0x13C
    rl_shader_const_ref m_gaussian_separated_axis_const; // 0x140
};
