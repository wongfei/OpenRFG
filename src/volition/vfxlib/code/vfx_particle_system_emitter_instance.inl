// Original: volition/vfxlib/code/vfx_particle_system_emitter_instance.inl
//
// The emitter instance accessors (VFX.md FX1), inline as in the original: the .inl functions and the ones defined in
// vfx_particle_system_emitter_instance.h itself (their PDB locations are kept in the VA comments). The curve readers
// go through the instance's emitter (sizes scaled by its m_size_scale).
#pragma once

#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"

// The inlined random spread of rfg.exe: a value in [-variance, variance] at the given random percentage (the bounds
// sorted first), added to a base. The two helpers keep the two addition orders the decompile shows: (base + spread) +
// low and (spread + low) + base (float addition is not associative).
inline float vfx_base_plus_variance(float base, float variance, float random)
{
    float low = -variance;
    float high = variance;
    if (variance < low)
    {
        low = variance;
        high = -variance;
    }
    return base + (high - low) * random + low;
}

inline float vfx_variance_plus_base(float variance, float random, float base)
{
    float low = -variance;
    float high = variance;
    if (variance < low)
    {
        low = variance;
        high = -variance;
    }
    return (high - low) * random + low + base;
}

// 0x00C9DAF0 (.h:67)
inline const matrix& vfx_particle_system_emitter_instance::get_current_world_rotation()
{
    return current_world_matrix.m_rotation;
}

// 0x005D18D0 (.h:68)
inline const vector& vfx_particle_system_emitter_instance::get_current_world_position()
{
    return current_world_matrix.m_translation;
}

// 0x00C9DB10 (.h:75)
inline const matrix& vfx_particle_system_emitter_instance::get_parent_world_rotation()
{
    return m_parent_transform.m_rotation;
}

// 0x00C9DB20 (.h:76)
inline const vector& vfx_particle_system_emitter_instance::get_parent_world_position()
{
    return m_parent_transform.m_translation;
}

// 0x00C9DB30 (.h:77)
inline void vfx_particle_system_emitter_instance::set_parent_world_rotation(const matrix& parent_world_rotation)
{
    m_parent_transform.m_rotation = parent_world_rotation;
}

// 0x00C9DB80 (.h:78)
inline void vfx_particle_system_emitter_instance::set_parent_world_position(const vector& parent_world_position)
{
    m_parent_transform.m_translation = parent_world_position;
}

// 0x00C9DC40 (vfx_particle_system_emitter_instance.inl:181)
inline void vfx_particle_system_emitter_instance::get_local_position(vector& result, float elapsed_time)
{
    m_emitter_p->get_local_position(result, elapsed_time);
}

// 0x00C9DC90 (vfx_particle_system_emitter_instance.inl:188): the emitter's (with its fall-through, see there).
inline void vfx_particle_system_emitter_instance::get_local_rotation(matrix& result, float elapsed_time)
{
    m_emitter_p->get_local_rotation(result, elapsed_time);
}

// 0x00C9DD30 (vfx_particle_system_emitter_instance.inl:195)
inline void vfx_particle_system_emitter_instance::get_local_scale(vector& result, float elapsed_time)
{
    m_emitter_p->get_local_scale(result, elapsed_time);
}

// 0x00C9E0A0 (vfx_particle_system_emitter_instance.inl:283)
inline void vfx_particle_system_emitter_instance::set_alternate_view(const matrix43& alternate_view)
{
    m_alternate_view = alternate_view;
}

// 0x00746120 (vfx_particle_system_emitter_instance.inl:300)
inline int vfx_particle_system_emitter_instance::get_filter_count() const
{
    return m_interactors.size();
}

// 0x00C9DBE0 (vfx_particle_system_emitter_instance.inl:14)
inline const rl_color_float& vfx_particle_system_emitter_instance::get_color_start()
{
    return m_emitter_p->get_color_start();
}

// 0x00C9DBF0 (vfx_particle_system_emitter_instance.inl:19)
inline const rl_color_float& vfx_particle_system_emitter_instance::get_color_end()
{
    return m_emitter_p->get_color_end();
}

// 0x00C9DC00 (vfx_particle_system_emitter_instance.inl:34)
inline const rl_color_float& vfx_particle_system_emitter_instance::get_color_mid_A()
{
    return m_emitter_p->get_color_mid_A();
}

// 0x00C9DC10 (vfx_particle_system_emitter_instance.inl:39)
inline const rl_color_float& vfx_particle_system_emitter_instance::get_color_mid_B()
{
    return m_emitter_p->get_color_mid_B();
}

// 0x00CA3650 (vfx_particle_system_emitter_instance.inl:54)
inline float vfx_particle_system_emitter_instance::get_angular_speed_start(float elapsed_time)
{
    return m_emitter_p->get_angular_speed_start(elapsed_time);
}

// 0x00CA3690 (vfx_particle_system_emitter_instance.inl:59)
inline float vfx_particle_system_emitter_instance::get_angular_speed_end(float elapsed_time)
{
    return m_emitter_p->get_angular_speed_end(elapsed_time);
}

// 0x00C9DC20 (vfx_particle_system_emitter_instance.inl:64)
inline float vfx_particle_system_emitter_instance::get_angular_speed_start_variance()
{
    return m_emitter_p->get_angular_speed_start_variance();
}

// 0x00C9DC30 (vfx_particle_system_emitter_instance.inl:69)
inline float vfx_particle_system_emitter_instance::get_angular_speed_end_variance()
{
    return m_emitter_p->get_angular_speed_end_variance();
}

// 0x00CA36D0 (vfx_particle_system_emitter_instance.inl:89)
inline float vfx_particle_system_emitter_instance::get_size_start(float elapsed_time)
{
    return m_emitter_p->get_size_start(elapsed_time);
}

// 0x00CA3720 (vfx_particle_system_emitter_instance.inl:94)
inline float vfx_particle_system_emitter_instance::get_size_end(float elapsed_time)
{
    return m_emitter_p->get_size_end(elapsed_time);
}

// 0x00CA3770 (vfx_particle_system_emitter_instance.inl:99)
inline float vfx_particle_system_emitter_instance::get_size_start_variance(float elapsed_time)
{
    return m_emitter_p->get_size_start_variance(elapsed_time);
}

// 0x00CA37C0 (vfx_particle_system_emitter_instance.inl:104)
inline float vfx_particle_system_emitter_instance::get_size_end_variance(float elapsed_time)
{
    return m_emitter_p->get_size_end_variance(elapsed_time);
}

// 0x00CA3810 (vfx_particle_system_emitter_instance.inl:109)
inline float vfx_particle_system_emitter_instance::get_size_mid_A(float elapsed_time)
{
    return m_emitter_p->get_size_mid_A(elapsed_time);
}

// 0x00CA3860 (vfx_particle_system_emitter_instance.inl:114)
inline float vfx_particle_system_emitter_instance::get_size_mid_B(float elapsed_time)
{
    return m_emitter_p->get_size_mid_B(elapsed_time);
}

// 0x00CA38B0 (vfx_particle_system_emitter_instance.inl:119)
inline float vfx_particle_system_emitter_instance::get_size_mid_A_variance(float elapsed_time)
{
    return m_emitter_p->get_size_mid_A_variance(elapsed_time);
}

// 0x00CA3900 (vfx_particle_system_emitter_instance.inl:124)
inline float vfx_particle_system_emitter_instance::get_size_mid_B_variance(float elapsed_time)
{
    return m_emitter_p->get_size_mid_B_variance(elapsed_time);
}

// 0x00CA3950 (vfx_particle_system_emitter_instance.inl:129)
inline float vfx_particle_system_emitter_instance::get_length_start(float elapsed_time)
{
    return m_emitter_p->get_length_start(elapsed_time);
}

// 0x00CA3990 (vfx_particle_system_emitter_instance.inl:134)
inline float vfx_particle_system_emitter_instance::get_length_end(float elapsed_time)
{
    return m_emitter_p->get_length_end(elapsed_time);
}

// 0x00CA39D0 (vfx_particle_system_emitter_instance.inl:139)
inline float vfx_particle_system_emitter_instance::get_length_start_variance(float elapsed_time)
{
    return m_emitter_p->get_length_start_variance(elapsed_time);
}

// 0x00CA3A10 (vfx_particle_system_emitter_instance.inl:144)
inline float vfx_particle_system_emitter_instance::get_length_end_variance(float elapsed_time)
{
    return m_emitter_p->get_length_end_variance(elapsed_time);
}

// 0x00CA3A50 (vfx_particle_system_emitter_instance.inl:149)
inline float vfx_particle_system_emitter_instance::get_length_mid_A(float elapsed_time)
{
    return m_emitter_p->get_length_mid_A(elapsed_time);
}

// 0x00CA3A90 (vfx_particle_system_emitter_instance.inl:154)
inline float vfx_particle_system_emitter_instance::get_length_mid_B(float elapsed_time)
{
    return m_emitter_p->get_length_mid_B(elapsed_time);
}

// 0x00CA3AD0 (vfx_particle_system_emitter_instance.inl:159)
inline float vfx_particle_system_emitter_instance::get_length_mid_A_variance(float elapsed_time)
{
    return m_emitter_p->get_length_mid_A_variance(elapsed_time);
}

// 0x00CA3B10 (vfx_particle_system_emitter_instance.inl:164)
inline float vfx_particle_system_emitter_instance::get_length_mid_B_variance(float elapsed_time)
{
    return m_emitter_p->get_length_mid_B_variance(elapsed_time);
}

// 0x00C9DD80 (vfx_particle_system_emitter_instance.inl:202): by value, one random offset (fl_rand) from the red
// variance moves red, green and blue together and a second one from the alpha variance moves alpha; otherwise each
// channel takes its own offset from the per-particle random vector and float.
inline rl_color_float vfx_particle_system_emitter_instance::compute_variant_color(const rl_color_float& original_color, const rl_color_float& variant_color, bool use_value)
{
    rl_color_float result;
    if (use_value)
    {
        const float rgb_offset = vfx_float_random(-variant_color.red, variant_color.red);
        const float alpha_offset = vfx_float_random(-variant_color.alpha, variant_color.alpha);
        result.red = original_color.red + rgb_offset;
        result.green = original_color.green + rgb_offset;
        result.alpha = original_color.alpha + alpha_offset;
        result.blue = original_color.blue + rgb_offset;
        return result;
    }
    result.red = vfx_base_plus_variance(original_color.red, variant_color.red, ms_random_vector.x);
    result.green = vfx_base_plus_variance(original_color.green, variant_color.green, ms_random_vector.y);
    result.blue = vfx_base_plus_variance(original_color.blue, variant_color.blue, ms_random_vector.z);
    result.alpha = vfx_base_plus_variance(original_color.alpha, variant_color.alpha, ms_random_float);
    return result;
}

// 0x00C9DBC0 (vfx_particle_system_emitter_instance.inl:4)
inline bool vfx_particle_system_emitter_instance::get_update_parametric() const
{
    return m_update_parametric;
}

// 0x00C9DBD0 (vfx_particle_system_emitter_instance.inl:9)
inline void vfx_particle_system_emitter_instance::set_update_parametric(bool update_parametric)
{
    m_update_parametric = update_parametric;
}

// 0x006E1870 (.h:49)
inline void vfx_particle_system_emitter_instance::set_emission_enabled(bool _emission_enabled)
{
    emission_enabled = _emission_enabled;
}

// 0x005D1880 (.h:53)
inline vfx_particle_system_emitter* vfx_particle_system_emitter_instance::get_emitter()
{
    return m_emitter_p;
}

// 0x005D1890 (.h:56)
inline linked_list_p<vfx_particle_iterative>& vfx_particle_system_emitter_instance::get_particle_iterative_list()
{
    return m_particle_iterative_list;
}

// 0x005D18A0 (.h:57)
inline linked_list_p<vfx_particle_parametric>& vfx_particle_system_emitter_instance::get_particle_parametric_list()
{
    return m_particle_parametric_list;
}

// 0x00C9DAC0 (.h:58)
inline float vfx_particle_system_emitter_instance::get_last_spawn_time()
{
    return last_spawn_time;
}

// 0x00C9DAD0 (.h:59)
inline float vfx_particle_system_emitter_instance::get_current_elapsed_time()
{
    return current_elapsed_time;
}

// 0x005D18B0 (.h:60)
inline float vfx_particle_system_emitter_instance::get_current_anim_time()
{
    return current_anim_time;
}

// 0x00C9DAE0 (.h:62)
inline int vfx_particle_system_emitter_instance::get_tail_spawn_count()
{
    return tail_spawn_count;
}

// 0x005D18C0 (.h:66)
inline const matrix43& vfx_particle_system_emitter_instance::get_current_world_matrix()
{
    return current_world_matrix;
}

// 0x00C9DB00 (.h:69)
inline const vector& vfx_particle_system_emitter_instance::get_current_spawn_position()
{
    return current_spawn_position;
}

// 0x00C9DBA0 (.h:80)
inline rl_renderable_instance* vfx_particle_system_emitter_instance::get_parent_renderable_instance() const
{
    return m_parent_renderable_instance_p;
}

// 0x00C9DBB0 (.h:81)
inline void vfx_particle_system_emitter_instance::set_parent_renderable_instance(rl_renderable_instance* parent_renderable_instance)
{
    m_parent_renderable_instance_p = parent_renderable_instance;
}

// 0x00C9E050 (vfx_particle_system_emitter_instance.inl:239)
inline void vfx_particle_system_emitter_instance::set_post_update_cb(void (*post_update_cb)(vfx_particle_system_emitter_instance*, void*))
{
    m_post_update_cb = post_update_cb;
}

// 0x00C9E060 (vfx_particle_system_emitter_instance.inl:250)
inline void vfx_particle_system_emitter_instance::set_update_cb_data(void* update_cb_data_p)
{
    m_update_cb_data_p = update_cb_data_p;
}

// 0x00C9E070 (vfx_particle_system_emitter_instance.inl:261)
inline void vfx_particle_system_emitter_instance::set_particle_create_cb(void (*particle_create_cb)(vfx_particle_iterative*, vfx_particle_system_emitter_instance*, void*, bool))
{
    m_particle_create_cb = particle_create_cb;
}

// 0x00C9E080 (vfx_particle_system_emitter_instance.inl:272)
inline void vfx_particle_system_emitter_instance::set_particle_create_cb_data(void* particle_create_cb_data)
{
    m_particle_create_cb_data = particle_create_cb_data;
}

// 0x006E1880 (vfx_particle_system_emitter_instance.inl:328)
inline rl_renderable_particles_instance* vfx_particle_system_emitter_instance::get_renderable_particles_instance() const
{
    return m_renderable_particles_instance_p;
}

// 0x00C9E160 (vfx_particle_system_emitter_instance.inl:333)
inline void vfx_particle_system_emitter_instance::set_renderable_particles_instance(rl_renderable_particles_instance* renderable_particles_instance_p)
{
    m_renderable_particles_instance_p = renderable_particles_instance_p;
}

// 0x00C9E090 (vfx_particle_system_emitter_instance.inl:278)
inline const matrix43& vfx_particle_system_emitter_instance::get_alternate_view() const
{
    return m_alternate_view;
}

// 0x00C9E100 (vfx_particle_system_emitter_instance.inl:289)
inline bool vfx_particle_system_emitter_instance::get_has_alternate_view() const
{
    return m_has_alternate_view;
}

// 0x00C9E110 (vfx_particle_system_emitter_instance.inl:294)
inline void vfx_particle_system_emitter_instance::set_has_alternate_view(bool has_alternate_view)
{
    m_has_alternate_view = has_alternate_view;
}

// 0x00C9E120 (vfx_particle_system_emitter_instance.inl:306)
inline bool vfx_particle_system_emitter_instance::get_is_visible() const
{
    return m_is_visible;
}

// 0x00C9E130 (vfx_particle_system_emitter_instance.inl:317)
inline float vfx_particle_system_emitter_instance::get_texture_anim_rate() const
{
    return m_texture_anim_rate;
}

// 0x00C9E140 (vfx_particle_system_emitter_instance.inl:322)
inline void vfx_particle_system_emitter_instance::set_texture_anim_rate(float texture_anim_rate)
{
    m_texture_anim_rate = texture_anim_rate;
}

// 0x00C9E170 (vfx_particle_system_emitter_instance.inl:339)
inline bool vfx_particle_system_emitter_instance::get_is_emitted_node() const
{
    return m_is_emitted_node;
}

// 0x00C9E180 (vfx_particle_system_emitter_instance.inl:344)
inline void vfx_particle_system_emitter_instance::set_is_emitted_node(bool update_parent_transform)
{
    m_is_emitted_node = update_parent_transform;
}

// 0x00C9E190 (vfx_particle_system_emitter_instance.inl:355)
inline void vfx_particle_system_emitter_instance::set_emit_node_parent(vfx_particle_iterative* emit_node_parent_p)
{
    m_emit_node_parent_p = emit_node_parent_p;
}

// 0x00C9E1A0 (vfx_particle_system_emitter_instance.inl:361)
inline unsigned short vfx_particle_system_emitter_instance::get_sort_priority() const
{
    return m_sort_priority;
}

// 0x00C9E1B0 (vfx_particle_system_emitter_instance.inl:366)
inline void vfx_particle_system_emitter_instance::set_sort_priority(unsigned short sort_priority)
{
    m_sort_priority = sort_priority;
}

