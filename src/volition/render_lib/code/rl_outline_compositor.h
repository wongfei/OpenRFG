// Original: volition/render_lib/code/rl_outline_compositor.h
// Implementation: volition/render_lib/code/rl_outline_compositor.cpp
//
// rl_outline_compositor, draws object outlines (highlighting).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_device.h"
#include "volition/render_lib/code/rl_material_defines.h"
#include "volition/render_lib/code/rl_render_texture.h"
#include "volition/render_lib/code/rl_shader.h"
#include "volition/render_lib/code/rl_texture_defines.h"
#include "volition/vlib/code/math/vector4.h"

class rl_batched_pass;
class rl_camera;
class rl_class_dynamic_info;
class rl_compositor;
class rl_light;
class rl_material;
struct rl_outline_state;
class rl_renderer;
class rl_scene_renderer;
class rl_visibility_manager;

namespace keen
{
    struct TextureData;
}

// size 0x150
class rl_outline_compositor : public rl_base_object
{
    // Access inferred: the scene renderer reads and writes these members directly.
    friend class rl_scene_renderer;
protected:
    rl_outline_compositor(); // 0x00529660 (:77)
    virtual ~rl_outline_compositor(); // vtbl+0x00 0x005296F0 (:89)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_outline_compositor* create(); // 0x00541B20 (:74)
    static bool can_create(); // 0x004C3B00 (:74)
    virtual void release(); // vtbl+0x04 0x004C3B10 (:74)
    void initialize(int width, int height); // 0x0054F5B0 (:96)
    void shutdown(); // 0x00529720 (:164)
    void render_outline_edge_mask(rl_renderer& renderer, rl_scene_renderer& scene_renderer, rl_compositor& compositor, rl_camera& camera, rl_light& light, const rl_dev_render_target& render_target, int width, int height, rl_outline_state& outline_state, rl_batched_pass& batched_pass, rl_visibility_manager& visibility_manager, const keen::TextureData* pDepthTexture); // 0x0054FAF0 (:195)
    void add_outline_to_buffer(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& bloom_target, const int bloom_width, const int bloom_height, rl_outline_state& outline_state, const int outline_texture_handle); // 0x00541FC0 (:291)
    bool any_outline_pass_enabled(const rl_outline_state& outline_state); // 0x004C3B30 (:183)
    void release_resources(); // 0x00513910 (:330)
    void renew_resources(); // 0x0054FB60 (:337)
    void create_render_target(); // 0x0054C560 (:342)
    void resize(int w, int h); // 0x004C3B60 (:324)
private:
    void render_mask_layer(rl_renderer& renderer, rl_scene_renderer& scene_renderer, rl_camera& camera, rl_light& light, rl_outline_state& outline_state, rl_batched_pass& batched_pass, rl_visibility_manager& visibility_manager, const keen::TextureData* pDepthTexture); // 0x0054BF00 (:207)
    void render_edge_detection(rl_renderer& renderer, rl_compositor& compositor, const rl_dev_render_target& outline_target, const int target_width, const int target_height); // 0x00541BB0 (:264)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514F64
private:
    unsigned int m_mask_material_handle[4]; // 0x08
    rl_material* m_mask_material_p[4]; // 0x18
    unsigned int m_edge_detect_shader; // 0x28
    vector4 m_edge_detect_threshold; // 0x30
    unsigned int m_composite_outline_shader; // 0x40
    rl_shader_const_ref m_composite_outline_texture_ref; // 0x44
    rl_shader_const_ref m_composite_outline_color_ref[2]; // 0x48
    rl_material_data m_mask_material_data[4]; // 0x50
    rl_material_const m_mask_material_consts[4]; // 0xC0
    rl_texture_desc m_mask_material_textures[4]; // 0x100
    rl_render_texture m_mask_render_target; // 0x130
    rl_dev_depth_stencil_target m_mask_ds_surface; // 0x144
    int m_mask_render_target_width; // 0x148
    int m_mask_render_target_height; // 0x14C
};
