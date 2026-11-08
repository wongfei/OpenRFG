// Original: volition/render_lib/code/rl_renderable_particles_instance.inl (inline members; included at the end of
// rl_renderable_particles_instance.h)
//
// The render state of a particle system instance and its parametric constants (VFX.md FX2). The parametric setters
// write the instance's constant block without a null check (only parametric emitters set them; set_parametric_time
// and set_texture_anim_rate check).
#pragma once

// 0x004BD5B0 (:2)
inline unsigned short rl_renderable_particles_instance::get_sort_priority() const
{
    return m_sort_priority;
}

// 0x00C9E250 (:7)
inline void rl_renderable_particles_instance::set_sort_priority(unsigned short sort_priority)
{
    m_sort_priority = sort_priority;
}

// 0x004BD5C0 (:13)
inline bool rl_renderable_particles_instance::get_is_visible() const
{
    return m_is_visible;
}

// 0x006E18C0 (:18)
inline void rl_renderable_particles_instance::set_is_visible(bool is_visible)
{
    m_is_visible = is_visible;
}

// 0x004BD5D0 (:23)
inline rl_renderable_particles* rl_renderable_particles_instance::get_renderable_particles() const
{
    return m_renderable_particles_p;
}

// 0x004BD5E0 (:29)
inline int rl_renderable_particles_instance::get_tail_spawn_count() const
{
    return m_tail_spawn_count;
}

// 0x00C9E260 (:34)
inline void rl_renderable_particles_instance::set_tail_spawn_count(int tail_spawn_count)
{
    m_tail_spawn_count = tail_spawn_count;
}

// 0x004BD5F0 (:40)
inline pool_list<rl_particle_iterative>& rl_renderable_particles_instance::get_particle_iterative_list()
{
    return m_particle_iterative_list;
}

// 0x004BD600 (:46)
inline pool_list<rl_particle_parametric>& rl_renderable_particles_instance::get_particle_parametric_list()
{
    return m_particle_parametric_list;
}

// 0x004BD610 (:64)
inline const vector& rl_renderable_particles_instance::get_local_emission_position() const
{
    return m_local_emission_position;
}

// 0x00C9E270 (:69)
inline void rl_renderable_particles_instance::set_local_emission_position(const vector& local_emission_position)
{
    m_local_emission_position = local_emission_position;
}

// 0x004BD620 (:75)
inline const matrix& rl_renderable_particles_instance::get_local_emission_rotation() const
{
    return m_local_emission_rotation;
}

// 0x00C9E290 (:80)
inline void rl_renderable_particles_instance::set_local_emission_rotation(const matrix& local_emission_rotation)
{
    m_local_emission_rotation = local_emission_rotation;
}

// 0x004BD630 (:86)
inline const matrix43& rl_renderable_particles_instance::get_alternate_view() const
{
    return m_alternate_view;
}

// 0x00C9E2D0 (:91)
inline void rl_renderable_particles_instance::set_alternate_view(const matrix43& alternate_view)
{
    m_alternate_view = alternate_view;
}

// 0x004BD640 (:97)
inline bool rl_renderable_particles_instance::get_has_alternate_view() const
{
    return m_has_alternate_view;
}

// 0x00C9E330 (:102)
inline void rl_renderable_particles_instance::set_has_alternate_view(bool has_alternate_view)
{
    m_has_alternate_view = has_alternate_view;
}

// 0x004BD650 (:108)
inline float rl_renderable_particles_instance::get_current_elapsed_time() const
{
    return m_current_elapsed_time;
}

// 0x00C9E340 (:113)
inline void rl_renderable_particles_instance::set_current_elapsed_time(float current_elapsed_time)
{
    m_current_elapsed_time = current_elapsed_time;
}

// 0x004BD660 (:118)
inline const unsigned char* rl_renderable_particles_instance::get_parametric_constant_data() const
{
    return (const unsigned char*)m_parametric_constant_data_p;
}

// 0x004BD670 (:124): the constant block in float4 registers (0x110 bytes / 16).
inline int rl_renderable_particles_instance::get_num_parametric_constants() const
{
    return 0x11;
}

// 0x00C9E350 (:139)
inline void rl_renderable_particles_instance::set_parametric_time(float parametric_time)
{
    if (m_parametric_constant_data_p != nullptr)
    {
        m_parametric_constant_data_p->m_time = parametric_time;
    }
}

// 0x004BD680 (:146)
inline bool rl_renderable_particles_instance::get_is_local_space() const
{
    return 0.0f < m_parametric_constant_data_p->m_is_local_space;
}

// 0x00C9E370 (:151): the shader reads local space as a float above 1 (1.5 is stored).
inline void rl_renderable_particles_instance::set_is_local_space(bool is_local_space)
{
    m_parametric_constant_data_p->m_is_local_space = is_local_space ? 1.5f : 0.0f;
}

// 0x00C9E3A0 (:162)
inline void rl_renderable_particles_instance::set_density(float density)
{
    m_parametric_constant_data_p->m_density = density;
}

// 0x00C9E3C0 (:173)
inline void rl_renderable_particles_instance::set_density_variance(float density_variance)
{
    m_parametric_constant_data_p->m_density_variance = density_variance;
}

// 0x00C9E3E0 (:194)
inline void rl_renderable_particles_instance::set_color_start(const rl_color_float& color_start)
{
    m_parametric_constant_data_p->m_color_start = color_start;
}

// 0x00C9E400 (:207)
inline void rl_renderable_particles_instance::set_color_mid_A(const rl_color_float& color_mid_A)
{
    m_parametric_constant_data_p->m_color_mid_A = color_mid_A;
}

// 0x00C9E420 (:220)
inline void rl_renderable_particles_instance::set_color_mid_B(const rl_color_float& color_mid_B)
{
    m_parametric_constant_data_p->m_color_mid_B = color_mid_B;
}

// 0x00C9E440 (:233)
inline void rl_renderable_particles_instance::set_color_end(const rl_color_float& color_end)
{
    m_parametric_constant_data_p->m_color_end = color_end;
}

// 0x00C9E460 (:246)
inline void rl_renderable_particles_instance::set_color_mid_A_offset(float color_mid_A_offset)
{
    m_parametric_constant_data_p->m_color_mid_A_offset = color_mid_A_offset;
}

// 0x00C9E480 (:259)
inline void rl_renderable_particles_instance::set_color_mid_B_offset(float color_mid_B_offset)
{
    m_parametric_constant_data_p->m_color_mid_B_offset = color_mid_B_offset;
}

// 0x00C9E4A0 (:272)
inline void rl_renderable_particles_instance::set_rotation_start(float rotation_start)
{
    m_parametric_constant_data_p->m_rotation_start = rotation_start;
}

// 0x00C9E4C0 (:285)
inline void rl_renderable_particles_instance::set_rotation_end(float rotation_end)
{
    m_parametric_constant_data_p->m_rotation_end = rotation_end;
}

// 0x00C9E4E0 (:297)
inline void rl_renderable_particles_instance::set_size_start(float size_start)
{
    m_parametric_constant_data_p->m_size_start = size_start;
}

// 0x00C9E500 (:310)
inline void rl_renderable_particles_instance::set_size_mid_A(float size_mid_A)
{
    m_parametric_constant_data_p->m_size_mid_A = size_mid_A;
}

// 0x00C9E520 (:323)
inline void rl_renderable_particles_instance::set_size_mid_B(float size_mid_B)
{
    m_parametric_constant_data_p->m_size_mid_B = size_mid_B;
}

// 0x00C9E540 (:336)
inline void rl_renderable_particles_instance::set_size_end(float size_end)
{
    m_parametric_constant_data_p->m_size_end = size_end;
}

// 0x00C9E560 (:349)
inline void rl_renderable_particles_instance::set_size_mid_A_offset(float size_mid_A_offset)
{
    m_parametric_constant_data_p->m_size_mid_A_offset = size_mid_A_offset;
}

// 0x00C9E580 (:362)
inline void rl_renderable_particles_instance::set_size_mid_B_offset(float size_mid_B_offset)
{
    m_parametric_constant_data_p->m_size_mid_B_offset = size_mid_B_offset;
}

// 0x00C9E5A0 (:375)
inline void rl_renderable_particles_instance::set_length_start(float length_start)
{
    m_parametric_constant_data_p->m_length_start = length_start;
}

// 0x00C9E5C0 (:388)
inline void rl_renderable_particles_instance::set_length_mid_A(float length_mid_A)
{
    m_parametric_constant_data_p->m_length_mid_A = length_mid_A;
}

// 0x00C9E5E0 (:401)
inline void rl_renderable_particles_instance::set_length_mid_B(float length_mid_B)
{
    m_parametric_constant_data_p->m_length_mid_B = length_mid_B;
}

// 0x00C9E600 (:414)
inline void rl_renderable_particles_instance::set_length_end(float length_end)
{
    m_parametric_constant_data_p->m_length_end = length_end;
}

// 0x00C9E620 (:427)
inline void rl_renderable_particles_instance::set_length_mid_A_offset(float length_mid_A_offset)
{
    m_parametric_constant_data_p->m_length_mid_A_offset = length_mid_A_offset;
}

// 0x00C9E640 (:440)
inline void rl_renderable_particles_instance::set_length_mid_B_offset(float length_mid_B_offset)
{
    m_parametric_constant_data_p->m_length_mid_B_offset = length_mid_B_offset;
}

// 0x00C9E660 (:453)
inline void rl_renderable_particles_instance::set_color_saturation_start(float saturation_start)
{
    m_parametric_constant_data_p->m_color_saturation_start = saturation_start;
}

// 0x00C9E680 (:466)
inline void rl_renderable_particles_instance::set_color_saturation_mid_A(float saturation_mid_A)
{
    m_parametric_constant_data_p->m_color_saturation_mid_A = saturation_mid_A;
}

// 0x00C9E6A0 (:479)
inline void rl_renderable_particles_instance::set_color_saturation_mid_B(float saturation_mid_B)
{
    m_parametric_constant_data_p->m_color_saturation_mid_B = saturation_mid_B;
}

// 0x00C9E6C0 (:492)
inline void rl_renderable_particles_instance::set_color_saturation_end(float saturation_end)
{
    m_parametric_constant_data_p->m_color_saturation_end = saturation_end;
}

// 0x00C9E6E0 (:505)
inline void rl_renderable_particles_instance::set_additive_key_start(float saturation_start)
{
    m_parametric_constant_data_p->m_additive_key_start = saturation_start;
}

// 0x00C9E700 (:518)
inline void rl_renderable_particles_instance::set_additive_key_mid_A(float saturation_mid_A)
{
    m_parametric_constant_data_p->m_additive_key_mid_A = saturation_mid_A;
}

// 0x00C9E720 (:531)
inline void rl_renderable_particles_instance::set_additive_key_mid_B(float saturation_mid_B)
{
    m_parametric_constant_data_p->m_additive_key_mid_B = saturation_mid_B;
}

// 0x00C9E740 (:544)
inline void rl_renderable_particles_instance::set_additive_key_end(float saturation_end)
{
    m_parametric_constant_data_p->m_additive_key_end = saturation_end;
}

// 0x00C9E760 (:557)
inline void rl_renderable_particles_instance::set_acceleration_start(const vector& acceleration_start)
{
    m_parametric_constant_data_p->m_acceleration_start = acceleration_start;
}

// 0x00C9E780 (:570)
inline void rl_renderable_particles_instance::set_acceleration_end(const vector& acceleration_end)
{
    m_parametric_constant_data_p->m_acceleration_end = acceleration_end;
}

// 0x00C9E7A0 (:583)
inline void rl_renderable_particles_instance::set_acceleration_start_variance(const vector& acceleration_start_variance)
{
    m_parametric_constant_data_p->m_acceleration_start_variance = acceleration_start_variance;
}

// 0x00C9E7C0 (:596)
inline void rl_renderable_particles_instance::set_acceleration_end_variance(const vector& acceleration_end_variance)
{
    m_parametric_constant_data_p->m_acceleration_end_variance = acceleration_end_variance;
}

// 0x00C9E7E0 (:613)
inline void rl_renderable_particles_instance::set_rotation_start_variance(float rotation_start_variance)
{
    m_parametric_constant_data_p->m_rotation_start_variance = rotation_start_variance;
}

// 0x00C9E800 (:626)
inline void rl_renderable_particles_instance::set_rotation_end_variance(float rotation_end_variance)
{
    m_parametric_constant_data_p->m_rotation_end_variance = rotation_end_variance;
}

// 0x00C9E820 (:638)
inline void rl_renderable_particles_instance::set_size_start_variance(float size_start_variance)
{
    m_parametric_constant_data_p->m_size_start_variance = size_start_variance;
}

// 0x00C9E840 (:651)
inline void rl_renderable_particles_instance::set_size_mid_A_variance(float size_mid_A_variance)
{
    m_parametric_constant_data_p->m_size_mid_A_variance = size_mid_A_variance;
}

// 0x00C9E860 (:664)
inline void rl_renderable_particles_instance::set_size_mid_B_variance(float size_mid_B_variance)
{
    m_parametric_constant_data_p->m_size_mid_B_variance = size_mid_B_variance;
}

// 0x00C9E880 (:677)
inline void rl_renderable_particles_instance::set_size_end_variance(float size_end_variance)
{
    m_parametric_constant_data_p->m_size_end_variance = size_end_variance;
}

// 0x00C9E8A0 (:689)
inline void rl_renderable_particles_instance::set_length_start_variance(float length_start_variance)
{
    m_parametric_constant_data_p->m_length_start_variance = length_start_variance;
}

// 0x00C9E8C0 (:702)
inline void rl_renderable_particles_instance::set_length_mid_A_variance(float length_mid_A_variance)
{
    m_parametric_constant_data_p->m_length_mid_A_variance = length_mid_A_variance;
}

// 0x00C9E8E0 (:715)
inline void rl_renderable_particles_instance::set_length_mid_B_variance(float length_mid_B_variance)
{
    m_parametric_constant_data_p->m_length_mid_B_variance = length_mid_B_variance;
}

// 0x00C9E900 (:728)
inline void rl_renderable_particles_instance::set_length_end_variance(float length_end_variance)
{
    m_parametric_constant_data_p->m_length_end_variance = length_end_variance;
}

// 0x00C9E920 (:740)
inline void rl_renderable_particles_instance::set_frame_number(int frame_number)
{
    m_frame_number = frame_number;
}

// 0x004BD6A0 (:746)
inline float rl_renderable_particles_instance::get_texture_anim_rate() const
{
    return m_texture_anim_rate;
}

// 0x00C9E930 (:751): mirrored into the parametric constants when there are any.
inline void rl_renderable_particles_instance::set_texture_anim_rate(float texture_anim_rate)
{
    m_texture_anim_rate = texture_anim_rate;
    if (m_parametric_constant_data_p != nullptr)
    {
        m_parametric_constant_data_p->m_texture_anim_rate = texture_anim_rate;
    }
}

// 0x00C9E950 (:765)
inline void rl_renderable_particles_instance::set_texture_anim_random_enabled(bool texture_anim_random_enabled)
{
    m_parametric_constant_data_p->m_texture_anim_random_enabled = texture_anim_random_enabled ? 1.0f : 0.0f;
}

// 0x004BD6B0 (:771)
inline float rl_renderable_particles_instance::get_opacity() const
{
    return m_opacity;
}

// 0x004BD6C0 (:777)
inline void rl_renderable_particles_instance::set_opacity(float opacity)
{
    m_opacity = opacity;
}

// 0x00C9E980 (:782)
inline void rl_renderable_particles_instance::set_tails(int num_tails, rl_particle_base::tail_data* tail_data)
{
    m_num_tails = num_tails;
    m_tails = tail_data;
}

// 0x004BD6E0 (:799)
inline const rl_color_float& rl_renderable_particles_instance::get_effect_material_tint() const
{
    return m_effect_material_tint;
}

// 0x004BD6F0 (:804)
inline void rl_renderable_particles_instance::set_effect_material_tint(const rl_color_float& effect_material_tint)
{
    m_effect_material_tint = effect_material_tint;
}
