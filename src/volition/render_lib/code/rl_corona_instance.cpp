// Original: volition/render_lib/code/rl_corona_instance.cpp
//
// A corona (VFX.md FX2): a glow sprite of an effect, faded by occlusion queries (the depth and no-depth pixel counts
// update_corona compares). The accessors are in rl_corona_instance.inl.

#include "volition/render_lib/code/rl_corona_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_core.h"

// The class factory (:7): class id 0xC0902451, 300 coronas.
// <rl_corona_instance::factory_init> 0x0050C360 (:7)
// <rl_corona_instance::create> 0x00523650 (:7)
RL_FACTORY_CLASS_CORE(rl_corona_instance, 0xC0902451, 300)

// 0x004BD760 (:11)
rl_corona_instance::rl_corona_instance()
{
    m_type = TYPE_SCREEN_SPACE;
    m_position = ZERO_VECTOR;
    m_rotation = IDENTITY_MATRIX;
    m_color_start = rl_color_float(1.0f, 1.0f, 1.0f, 1.0f);
    // Magic numbers: the default intensity and opacity decay exponents.
    m_intensity_exponential_decay = 4.0f;
    m_color_end = rl_color_float(1.0f, 1.0f, 1.0f, 1.0f);
    m_opacity_exponential_decay = 10.0f;
    m_width_scale = 1.0f;
    m_saturation = 1.0f;
    m_additive_key = 1.0f;
    m_texture_handle = -1;
    m_rear_view = true;
    m_visibility = 1.0f;
    m_intensity = 1.0f;
    m_radius_start = 1.0f;
    m_radius_end = 0.1f;
    m_current_elapsed_time = 0.0f;
    m_anim_rate = 1.0f;
    m_rotation_scale = 1.0f;
    m_rotation_offset = 0.0f;
    m_position_offset = 0.0f;
    m_intensity_scale = 0.0f;
    m_opacity = 1.0f;
    // Magic numbers: the default color and radius distance ranges (0 to 100 m).
    m_color_distance_start = 0.0f;
    m_color_distance_end = 100.0f;
    m_radius_distance_start = 0.0f;
    m_radius_distance_end = 100.0f;
    m_view_fade_scale = 0.0f;
    m_view_fade_decay = 1.0f;
    m_view_fade_intensity_enabled = false;
    m_view_fade_opacity_enabled = false;
    m_view_fade_radius_enabled = true;
    m_random_anim_frames = false;
    m_infinite_depth = false;
    m_visibility_fade = 1.0f;
    m_color_correction_matrix = IDENTITY_MATRIX;
    m_visibility_radius = 0.1f;
    m_query_no_depth_handle.invalidate();
    m_query_depth_handle.invalidate();
    m_depth_pixels_drawn = 0;
    m_no_depth_pixels_drawn = 0;
    m_opacity_visibility_decay = 1.0f;
    m_radius_visibility_decay = 1.0f;
}

// 0x0050C460 (:58): pending occlusion queries of this frame stamp are freed.
rl_corona_instance::~rl_corona_instance()
{
    if (m_query_depth_handle.is_valid())
    {
        rl_render_lib::get_instance()->occlusion_query_release_handle(m_query_depth_handle);
    }
    if (m_query_no_depth_handle.is_valid())
    {
        rl_render_lib::get_instance()->occlusion_query_release_handle(m_query_no_depth_handle);
    }
}

// 0x00539000 (:7): deferred while render_lib is locked.
void rl_corona_instance::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_corona_instance();
}

// 0x005497D0 (:69): queued with the particle renderer's coronas (rl_particle_renderer::add_corona, inlined).
void rl_corona_instance::render()
{
    rl_particle_renderer::get_instance()->add_corona(this);
}
