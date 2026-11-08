// Original: volition/vfxlib/code/vfx_particle_system_emitter.inl
//
// The emitter accessors (VFX.md FX1), inline as in the original: the .inl functions and the ones defined in
// vfx_particle_system_emitter.h itself (their PDB locations are kept in the VA comments). Curves read with
// vfx_interpolate_varkey (keys with their own times); sizes are scaled by m_size_scale, emission by
// m_emission_rate_scale.
#pragma once

#include "volition/vfxlib/code/vfx_animation.h"

#include <math.h>

// 0x00CA2E80 (vfx_particle_system_emitter.inl:115)
inline float vfx_particle_system_emitter::get_size_mid_A_offset(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_mida_offset_keys, size_mida_offset_keys, elapsed_time);
    return value;
}

// 0x00CA2EB0 (vfx_particle_system_emitter.inl:131)
inline float vfx_particle_system_emitter::get_size_mid_B_offset(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_midb_offset_keys, size_midb_offset_keys, elapsed_time);
    return value;
}

// 0x00CA2F40 (vfx_particle_system_emitter.inl:158)
inline float vfx_particle_system_emitter::get_length_start_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_start_var_keys, length_start_var_keys, elapsed_time);
    return value;
}

// 0x00C9D040 (vfx_particle_system_emitter.inl:1021): position keys spread evenly over the animation.
inline void vfx_particle_system_emitter::get_local_position(vector& result, float elapsed_time)
{
    vfx_interpolate_vector(result, elapsed_time / animation_duration, num_position_keys, position_keys);
}

// 0x00C9D080 (vfx_particle_system_emitter.inl:1027): a single key uses the matrix set_rotation_keys extracted.
// Notes (decompiled behaviour, kept as is): the single-key copy is overwritten by the interpolation that follows.
inline void vfx_particle_system_emitter::get_local_rotation(matrix& result, float elapsed_time)
{
    if (num_rotation_keys == 1)
    {
        result = local_rotation_lazy;
    }
    quaternion rotation;
    vfx_interpolate_quaternion(rotation, elapsed_time / animation_duration, num_rotation_keys, rotation_keys);
    rotation.extract_matrix(result);
}

// 0x00C9D120 (vfx_particle_system_emitter.inl:1040)
inline void vfx_particle_system_emitter::get_local_scale(vector& result, float elapsed_time)
{
    vfx_interpolate_vector(result, elapsed_time / animation_duration, num_scale_keys, scale_keys);
}

// 0x00C9CA90 (vfx_particle_system_emitter.inl:473)
inline void vfx_particle_system_emitter::set_rotation_keys(vfx_quat_key* _rotation_keys, int _num_rotation_keys)
{
    num_rotation_keys = _num_rotation_keys;
    rotation_keys = _rotation_keys;
    if (_num_rotation_keys == 1)
    {
        _rotation_keys->value.extract_matrix(local_rotation_lazy);
    }
}

// 0x00C9D1D0 (vfx_particle_system_emitter.inl:1081)
inline const base_array<et_sized_native_pointer<vfx_particle_system_emitter> >& vfx_particle_system_emitter::get_emit_node_emitter_list() const
{
    return m_emit_node_emitter_list;
}

// 0x00C9C0E0 (.h:72)
inline void vfx_particle_system_emitter::set_particle_type(rl_particle_base::render_type _particle_type)
{
    m_particle_type = _particle_type;
}

// 0x00C9C0F0 (.h:73)
inline rl_particle_base::render_type vfx_particle_system_emitter::get_particle_type()
{
    return m_particle_type;
}

// 0x00C9C960 (vfx_particle_system_emitter.inl:331)
inline void vfx_particle_system_emitter::set_is_iterative(bool _is_iterative)
{
    is_iterative = _is_iterative;
}

// 0x00741DD0 (vfx_particle_system_emitter.inl:336)
inline bool vfx_particle_system_emitter::get_is_iterative()
{
    return is_iterative;
}

// 0x00C9C970 (vfx_particle_system_emitter.inl:342)
inline void vfx_particle_system_emitter::set_iterations_per_sec(float _iterations_per_sec)
{
    iterations_per_sec = _iterations_per_sec;
}

// 0x00741DE0 (vfx_particle_system_emitter.inl:347)
inline float vfx_particle_system_emitter::get_iterations_per_sec()
{
    return iterations_per_sec;
}

// 0x00C9C720 (vfx_particle_system_emitter.inl:3)
inline void vfx_particle_system_emitter::set_yaw_variance(float _yaw_variance)
{
    yaw_variance = _yaw_variance;
}

// 0x00C9C740 (vfx_particle_system_emitter.inl:13)
inline void vfx_particle_system_emitter::set_pitch_variance(float _pitch_variance)
{
    pitch_variance = _pitch_variance;
}

// 0x005D4400 (vfx_particle_system_emitter.inl:58)
inline float vfx_particle_system_emitter::get_speed(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_speed_keys, speed_keys, elapsed_time);
    return value;
}

// 0x005D43A0 (vfx_particle_system_emitter.inl:24)
inline float vfx_particle_system_emitter::get_angular_speed_start(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_angular_speed_start_keys, angular_speed_start_keys, elapsed_time);
    return value;
}

// 0x005D43D0 (vfx_particle_system_emitter.inl:29)
inline float vfx_particle_system_emitter::get_angular_speed_end(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_angular_speed_end_keys, angular_speed_end_keys, elapsed_time);
    return value;
}

// 0x005D1770 (vfx_particle_system_emitter.inl:35)
inline float vfx_particle_system_emitter::get_angular_speed_start_variance()
{
    return angular_speed_start_variance;
}

// 0x00C9C760 (vfx_particle_system_emitter.inl:40)
inline void vfx_particle_system_emitter::set_angular_speed_start_variance(float _angular_speed_start_variance)
{
    angular_speed_start_variance = fabsf(_angular_speed_start_variance);
}

// 0x005D1780 (vfx_particle_system_emitter.inl:46)
inline float vfx_particle_system_emitter::get_angular_speed_end_variance()
{
    return angular_speed_end_variance;
}

// 0x00C9C780 (vfx_particle_system_emitter.inl:51)
inline void vfx_particle_system_emitter::set_angular_speed_end_variance(float _angular_speed_end_variance)
{
    angular_speed_end_variance = fabsf(_angular_speed_end_variance);
}

// 0x00C9C7D0 (vfx_particle_system_emitter.inl:74)
inline bool vfx_particle_system_emitter::get_is_rotation_enabled()
{
    return is_rotation_enabled;
}

// 0x00C9C7E0 (vfx_particle_system_emitter.inl:79)
inline void vfx_particle_system_emitter::set_is_rotation_enabled(bool _is_rotation_enabled)
{
    is_rotation_enabled = _is_rotation_enabled;
}

// 0x00C9C100 (.h:94)
inline void vfx_particle_system_emitter::set_is_random_start_rotation(bool _is_random_start_rotation)
{
    is_random_start_rotation = _is_random_start_rotation;
}

// 0x00C9C110 (.h:95)
inline bool vfx_particle_system_emitter::get_is_random_start_rotation()
{
    return is_random_start_rotation;
}

// 0x006E1830 (vfx_particle_system_emitter.inl:301)
inline void vfx_particle_system_emitter::set_max_particles(int _max_particles)
{
    max_particles = _max_particles;
}

// 0x00C9C930 (vfx_particle_system_emitter.inl:296)
inline int vfx_particle_system_emitter::get_max_particles()
{
    return max_particles;
}

// 0x005D1810 (vfx_particle_system_emitter.inl:306)
inline float vfx_particle_system_emitter::get_lifetime(float elapsed_time)
{
    return lifetime;
}

// 0x006E1840 (vfx_particle_system_emitter.inl:313)
inline void vfx_particle_system_emitter::set_lifetime(float _lifetime)
{
    lifetime = _lifetime;
}

// 0x005D1820 (vfx_particle_system_emitter.inl:319)
inline float vfx_particle_system_emitter::get_lifetime_variance(float elapsed_time)
{
    return lifetime_variance;
}

// 0x00C9C940 (vfx_particle_system_emitter.inl:326)
inline void vfx_particle_system_emitter::set_lifetime_variance(float _lifetime_variance)
{
    lifetime_variance = _lifetime_variance;
}

// 0x00C9C120 (.h:102)
inline void vfx_particle_system_emitter::set_use_lifetime_variance(bool _use_lifetime_variance)
{
    use_lifetime_variance = _use_lifetime_variance;
}

// 0x00C9C130 (.h:103)
inline bool vfx_particle_system_emitter::get_use_lifetime_variance()
{
    return use_lifetime_variance;
}

// 0x00C9C980 (vfx_particle_system_emitter.inl:353)
inline void vfx_particle_system_emitter::set_is_reverse_render(bool _is_reverse_render)
{
    is_reverse_render = _is_reverse_render;
}

// 0x005D4630 (vfx_particle_system_emitter.inl:989)
inline float vfx_particle_system_emitter::get_emission_rate(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_emission_rate_keys, emission_rate_keys, elapsed_time);
    return m_emission_rate_scale * value;
}

// 0x00CA3100 (vfx_particle_system_emitter.inl:1016)
inline float vfx_particle_system_emitter::get_texture_anim_rate(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, m_num_texture_anim_rate_keys, m_texture_anim_rate_keys, elapsed_time);
    return value;
}

// 0x00C9C140 (.h:109)
inline void vfx_particle_system_emitter::set_use_size_variance(bool _use_size_variance)
{
    use_size_variance = _use_size_variance;
}

// 0x00C9C150 (.h:110)
inline bool vfx_particle_system_emitter::get_use_size_variance()
{
    return use_size_variance;
}

// 0x00C9C160 (.h:111)
inline void vfx_particle_system_emitter::set_use_size_mid_value(bool _use_size_mid_value)
{
    use_size_mid_value = _use_size_mid_value;
}

// 0x00C9C170 (.h:112)
inline bool vfx_particle_system_emitter::get_use_size_mid_value()
{
    return use_size_mid_value;
}

// 0x00C9C180 (.h:113)
inline void vfx_particle_system_emitter::set_use_length_mid_value(bool _use_length_mid_value)
{
    use_length_mid_value = _use_length_mid_value;
}

// 0x00C9C190 (.h:114)
inline bool vfx_particle_system_emitter::get_use_length_mid_value()
{
    return use_length_mid_value;
}

// 0x00C9C1A0 (.h:116)
inline void vfx_particle_system_emitter::set_use_length(bool _use_length)
{
    use_length = _use_length;
}

// 0x00C9C1B0 (.h:117)
inline bool vfx_particle_system_emitter::get_use_length()
{
    return use_length;
}

// 0x00C9C1C0 (.h:119)
inline bool vfx_particle_system_emitter::get_use_length_variance()
{
    return use_length_variance;
}

// 0x005D4430 (vfx_particle_system_emitter.inl:85)
inline float vfx_particle_system_emitter::get_size_start(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_start_keys, size_start_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x005D4470 (vfx_particle_system_emitter.inl:90)
inline float vfx_particle_system_emitter::get_size_end(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_end_keys, size_end_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x005D44B0 (vfx_particle_system_emitter.inl:95)
inline float vfx_particle_system_emitter::get_size_start_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_start_var_keys, size_start_var_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x005D44F0 (vfx_particle_system_emitter.inl:100)
inline float vfx_particle_system_emitter::get_size_end_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_end_var_keys, size_end_var_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x005D4530 (vfx_particle_system_emitter.inl:105)
inline float vfx_particle_system_emitter::get_size_mid_A(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_mida_keys, size_mida_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x005D45B0 (vfx_particle_system_emitter.inl:121)
inline float vfx_particle_system_emitter::get_size_mid_B(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_midb_keys, size_midb_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x005D4570 (vfx_particle_system_emitter.inl:110)
inline float vfx_particle_system_emitter::get_size_mid_A_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_mida_var_keys, size_mida_var_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x005D45F0 (vfx_particle_system_emitter.inl:126)
inline float vfx_particle_system_emitter::get_size_mid_B_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_size_midb_var_keys, size_midb_var_keys, elapsed_time);
    return m_size_scale * value;
}

// 0x00CA2EE0 (vfx_particle_system_emitter.inl:148)
inline float vfx_particle_system_emitter::get_length_start(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_start_keys, length_start_keys, elapsed_time);
    return value;
}

// 0x00CA2F10 (vfx_particle_system_emitter.inl:153)
inline float vfx_particle_system_emitter::get_length_end(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_end_keys, length_end_keys, elapsed_time);
    return value;
}

// 0x00CA2F70 (vfx_particle_system_emitter.inl:163)
inline float vfx_particle_system_emitter::get_length_end_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_end_var_keys, length_end_var_keys, elapsed_time);
    return value;
}

// 0x00CA2FA0 (vfx_particle_system_emitter.inl:168)
inline float vfx_particle_system_emitter::get_length_mid_A(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_mida_keys, length_mida_keys, elapsed_time);
    return value;
}

// 0x00CA3030 (vfx_particle_system_emitter.inl:184)
inline float vfx_particle_system_emitter::get_length_mid_B(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_midb_keys, length_midb_keys, elapsed_time);
    return value;
}

// 0x00CA2FD0 (vfx_particle_system_emitter.inl:173)
inline float vfx_particle_system_emitter::get_length_mid_A_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_mida_var_keys, length_mida_var_keys, elapsed_time);
    return value;
}

// 0x00CA3060 (vfx_particle_system_emitter.inl:189)
inline float vfx_particle_system_emitter::get_length_mid_B_variance(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_midb_var_keys, length_midb_var_keys, elapsed_time);
    return value;
}

// 0x00CA3000 (vfx_particle_system_emitter.inl:178)
inline float vfx_particle_system_emitter::get_length_mid_A_offset(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_mida_offset_keys, length_mida_offset_keys, elapsed_time);
    return value;
}

// 0x00CA3090 (vfx_particle_system_emitter.inl:194)
inline float vfx_particle_system_emitter::get_length_mid_B_offset(float elapsed_time)
{
    float value;
    vfx_interpolate_varkey<vfx_float_key, float>(value, num_length_midb_offset_keys, length_midb_offset_keys, elapsed_time);
    return value;
}

// 0x00C9C1D0 (.h:145)
inline void vfx_particle_system_emitter::set_use_color_variance(bool _use_color_variance)
{
    use_color_variance = _use_color_variance;
}

// 0x00C9C1E0 (.h:146)
inline bool vfx_particle_system_emitter::get_use_color_variance()
{
    return use_color_variance;
}

// 0x00C9C1F0 (.h:147)
inline void vfx_particle_system_emitter::set_is_color_variance_by_value(bool _is_color_variance_by_value)
{
    is_color_variance_by_value = _is_color_variance_by_value;
}

// 0x00C9C200 (.h:148)
inline bool vfx_particle_system_emitter::get_is_color_variance_by_value()
{
    return is_color_variance_by_value;
}

// 0x00C9C210 (.h:149)
inline void vfx_particle_system_emitter::set_use_color_mid_value(bool _use_color_mid_value)
{
    use_color_mid_value = _use_color_mid_value;
}

// 0x00C9C220 (.h:150)
inline bool vfx_particle_system_emitter::get_use_color_mid_value()
{
    return use_color_mid_value;
}

// 0x00C9C230 (.h:151)
inline void vfx_particle_system_emitter::set_use_color_saturation_value(bool _use_color_saturation_value)
{
    use_color_saturation_value = _use_color_saturation_value;
}

// 0x00C9C240 (.h:152)
inline bool vfx_particle_system_emitter::get_use_color_saturation_value()
{
    return use_color_saturation_value;
}

// 0x00C9C250 (.h:153)
inline void vfx_particle_system_emitter::set_use_additive_key_value(bool _use_additive_key_value)
{
    use_additive_key_value = _use_additive_key_value;
}

// 0x00C9C260 (.h:154)
inline bool vfx_particle_system_emitter::get_use_additive_key_value()
{
    return use_additive_key_value;
}

// 0x005D1790 (vfx_particle_system_emitter.inl:201)
inline const rl_color_float& vfx_particle_system_emitter::get_color_start()
{
    return color_start;
}

// 0x00C9C810 (vfx_particle_system_emitter.inl:206)
inline void vfx_particle_system_emitter::set_color_start(const rl_color_float& _color_start)
{
    color_start = _color_start;
}

// 0x005D17A0 (vfx_particle_system_emitter.inl:211)
inline const rl_color_float& vfx_particle_system_emitter::get_color_end()
{
    return color_end;
}

// 0x00C9C830 (vfx_particle_system_emitter.inl:216)
inline void vfx_particle_system_emitter::set_color_end(const rl_color_float& _color_end)
{
    color_end = _color_end;
}

// 0x005D17B0 (vfx_particle_system_emitter.inl:221)
inline const rl_color_float& vfx_particle_system_emitter::get_color_start_variance()
{
    return color_start_variance;
}

// 0x00C9C850 (vfx_particle_system_emitter.inl:226)
inline void vfx_particle_system_emitter::set_color_start_variance(const rl_color_float& _color_start_variance)
{
    color_start_variance = _color_start_variance;
}

// 0x005D17C0 (vfx_particle_system_emitter.inl:231)
inline const rl_color_float& vfx_particle_system_emitter::get_color_end_variance()
{
    return color_end_variance;
}

// 0x00C9C870 (vfx_particle_system_emitter.inl:236)
inline void vfx_particle_system_emitter::set_color_end_variance(const rl_color_float& _color_end_variance)
{
    color_end_variance = _color_end_variance;
}

// 0x005D17D0 (vfx_particle_system_emitter.inl:242)
inline const rl_color_float& vfx_particle_system_emitter::get_color_mid_A()
{
    return color_mid_A;
}

// 0x00C9C890 (vfx_particle_system_emitter.inl:247)
inline void vfx_particle_system_emitter::set_color_mid_A(const rl_color_float& _color_mid_A)
{
    color_mid_A = _color_mid_A;
}

// 0x005D17E0 (vfx_particle_system_emitter.inl:252)
inline const rl_color_float& vfx_particle_system_emitter::get_color_mid_B()
{
    return color_mid_B;
}

// 0x00C9C8B0 (vfx_particle_system_emitter.inl:257)
inline void vfx_particle_system_emitter::set_color_mid_B(const rl_color_float& _color_mid_B)
{
    color_mid_B = _color_mid_B;
}

// 0x005D17F0 (vfx_particle_system_emitter.inl:262)
inline const rl_color_float& vfx_particle_system_emitter::get_color_mid_A_variance()
{
    return color_mid_A_variance;
}

// 0x00C9C8D0 (vfx_particle_system_emitter.inl:267)
inline void vfx_particle_system_emitter::set_color_mid_A_variance(const rl_color_float& _color_mid_A_variance)
{
    color_mid_A_variance = _color_mid_A_variance;
}

// 0x005D1800 (vfx_particle_system_emitter.inl:272)
inline const rl_color_float& vfx_particle_system_emitter::get_color_mid_B_variance()
{
    return color_mid_B_variance;
}

// 0x00C9C8F0 (vfx_particle_system_emitter.inl:277)
inline void vfx_particle_system_emitter::set_color_mid_B_variance(const rl_color_float& _color_mid_B_variance)
{
    color_mid_B_variance = _color_mid_B_variance;
}

// 0x00C9C270 (.h:171)
inline void vfx_particle_system_emitter::set_color_mid_A_offset(float _color_mid_A_offset)
{
    color_mid_A_offset = _color_mid_A_offset;
}

// 0x00C9C290 (.h:172)
inline float vfx_particle_system_emitter::get_color_mid_A_offset()
{
    return color_mid_A_offset;
}

// 0x00C9C2A0 (.h:173)
inline void vfx_particle_system_emitter::set_color_mid_B_offset(float _color_mid_B_offset)
{
    color_mid_B_offset = _color_mid_B_offset;
}

// 0x00C9C2C0 (.h:174)
inline float vfx_particle_system_emitter::get_color_mid_B_offset()
{
    return color_mid_B_offset;
}

// 0x00C9C2D0 (.h:175)
inline void vfx_particle_system_emitter::set_alpha_mid_A_offset(float _alpha_mid_A_offset)
{
    alpha_mid_A_offset = _alpha_mid_A_offset;
}

// 0x00C9C2F0 (.h:176)
inline float vfx_particle_system_emitter::get_alpha_mid_A_offset()
{
    return alpha_mid_A_offset;
}

// 0x00C9C300 (.h:177)
inline void vfx_particle_system_emitter::set_alpha_mid_B_offset(float _alpha_mid_B_offset)
{
    alpha_mid_B_offset = _alpha_mid_B_offset;
}

// 0x00C9C320 (.h:178)
inline float vfx_particle_system_emitter::get_alpha_mid_B_offset()
{
    return alpha_mid_B_offset;
}

// 0x00C9C330 (.h:180)
inline void vfx_particle_system_emitter::set_color_saturation_start(float _color_saturation_start)
{
    color_saturation_start = _color_saturation_start;
}

// 0x00C9C350 (.h:181)
inline float vfx_particle_system_emitter::get_color_saturation_start()
{
    return color_saturation_start;
}

// 0x00C9C360 (.h:182)
inline void vfx_particle_system_emitter::set_color_saturation_start_variance(float _color_saturation_start_variance)
{
    color_saturation_start_variance = _color_saturation_start_variance;
}

// 0x00C9C380 (.h:183)
inline float vfx_particle_system_emitter::get_color_saturation_start_variance()
{
    return color_saturation_start_variance;
}

// 0x00C9C390 (.h:184)
inline void vfx_particle_system_emitter::set_color_saturation_end(float _color_saturation_end)
{
    color_saturation_end = _color_saturation_end;
}

// 0x00C9C3B0 (.h:185)
inline float vfx_particle_system_emitter::get_color_saturation_end()
{
    return color_saturation_end;
}

// 0x00C9C3C0 (.h:186)
inline void vfx_particle_system_emitter::set_color_saturation_end_variance(float _color_saturation_end_variance)
{
    color_saturation_end_variance = _color_saturation_end_variance;
}

// 0x00C9C3E0 (.h:187)
inline float vfx_particle_system_emitter::get_color_saturation_end_variance()
{
    return color_saturation_end_variance;
}

// 0x00C9C3F0 (.h:188)
inline void vfx_particle_system_emitter::set_color_saturation_mid_A(float _color_saturation_mid_A)
{
    color_saturation_mid_A = _color_saturation_mid_A;
}

// 0x00C9C410 (.h:189)
inline float vfx_particle_system_emitter::get_color_saturation_mid_A()
{
    return color_saturation_mid_A;
}

// 0x00C9C420 (.h:190)
inline void vfx_particle_system_emitter::set_color_saturation_mid_A_variance(float _color_saturation_mid_A_variance)
{
    color_saturation_mid_A_variance = _color_saturation_mid_A_variance;
}

// 0x00C9C440 (.h:191)
inline float vfx_particle_system_emitter::get_color_saturation_mid_A_variance()
{
    return color_saturation_mid_A_variance;
}

// 0x00C9C450 (.h:192)
inline void vfx_particle_system_emitter::set_color_saturation_mid_B(float _color_saturation_mid_B)
{
    color_saturation_mid_B = _color_saturation_mid_B;
}

// 0x00C9C470 (.h:193)
inline float vfx_particle_system_emitter::get_color_saturation_mid_B()
{
    return color_saturation_mid_B;
}

// 0x00C9C480 (.h:194)
inline void vfx_particle_system_emitter::set_color_saturation_mid_B_variance(float _color_saturation_mid_B_variance)
{
    color_saturation_mid_B_variance = _color_saturation_mid_B_variance;
}

// 0x00C9C4A0 (.h:195)
inline float vfx_particle_system_emitter::get_color_saturation_mid_B_variance()
{
    return color_saturation_mid_B_variance;
}

// 0x00C9C4B0 (.h:197)
inline void vfx_particle_system_emitter::set_additive_key_start(float _additive_key_start)
{
    additive_key_start = _additive_key_start;
}

// 0x00C9C4D0 (.h:198)
inline float vfx_particle_system_emitter::get_additive_key_start()
{
    return additive_key_start;
}

// 0x00C9C4E0 (.h:199)
inline void vfx_particle_system_emitter::set_additive_key_start_variance(float _additive_key_start_variance)
{
    additive_key_start_variance = _additive_key_start_variance;
}

// 0x00C9C500 (.h:200)
inline float vfx_particle_system_emitter::get_additive_key_start_variance()
{
    return additive_key_start_variance;
}

// 0x00C9C510 (.h:201)
inline void vfx_particle_system_emitter::set_additive_key_end(float _additive_key_end)
{
    additive_key_end = _additive_key_end;
}

// 0x00C9C530 (.h:202)
inline float vfx_particle_system_emitter::get_additive_key_end()
{
    return additive_key_end;
}

// 0x00C9C540 (.h:203)
inline void vfx_particle_system_emitter::set_additive_key_end_variance(float _additive_key_end_variance)
{
    additive_key_end_variance = _additive_key_end_variance;
}

// 0x00C9C560 (.h:204)
inline float vfx_particle_system_emitter::get_additive_key_end_variance()
{
    return additive_key_end_variance;
}

// 0x00C9C570 (.h:205)
inline void vfx_particle_system_emitter::set_additive_key_mid_A(float _additive_key_mid_A)
{
    additive_key_mid_A = _additive_key_mid_A;
}

// 0x00C9C590 (.h:206)
inline float vfx_particle_system_emitter::get_additive_key_mid_A()
{
    return additive_key_mid_A;
}

// 0x00C9C5A0 (.h:207)
inline void vfx_particle_system_emitter::set_additive_key_mid_A_variance(float _additive_key_mid_A_variance)
{
    additive_key_mid_A_variance = _additive_key_mid_A_variance;
}

// 0x00C9C5C0 (.h:208)
inline float vfx_particle_system_emitter::get_additive_key_mid_A_variance()
{
    return additive_key_mid_A_variance;
}

// 0x00C9C5D0 (.h:209)
inline void vfx_particle_system_emitter::set_additive_key_mid_B(float _additive_key_mid_B)
{
    additive_key_mid_B = _additive_key_mid_B;
}

// 0x00C9C5F0 (.h:210)
inline float vfx_particle_system_emitter::get_additive_key_mid_B()
{
    return additive_key_mid_B;
}

// 0x00C9C600 (.h:211)
inline void vfx_particle_system_emitter::set_additive_key_mid_B_variance(float _additive_key_mid_B_variance)
{
    additive_key_mid_B_variance = _additive_key_mid_B_variance;
}

// 0x00C9C620 (.h:212)
inline float vfx_particle_system_emitter::get_additive_key_mid_B_variance()
{
    return additive_key_mid_B_variance;
}

// 0x00C9C7A0 (vfx_particle_system_emitter.inl:63)
inline float vfx_particle_system_emitter::get_speed_variance()
{
    return speed_variance;
}

// 0x00C9C7B0 (vfx_particle_system_emitter.inl:68)
inline void vfx_particle_system_emitter::set_speed_variance(float _speed_variance)
{
    speed_variance = _speed_variance;
}

// 0x00C9C630 (.h:218)
inline void vfx_particle_system_emitter::set_local_space_emission(bool _local_space_emission)
{
    local_space_emission = _local_space_emission;
}

// 0x005D1760 (.h:219)
inline bool vfx_particle_system_emitter::get_local_space_emission()
{
    return local_space_emission;
}

// 0x00C9C640 (.h:221)
inline void vfx_particle_system_emitter::set_weight_start(float weight_start)
{
    m_weight_start = weight_start;
}

// 0x00C9C650 (.h:222)
inline float vfx_particle_system_emitter::get_weight_start()
{
    return m_weight_start;
}

// 0x00C9C660 (.h:223)
inline void vfx_particle_system_emitter::set_weight_end(float weight_end)
{
    m_weight_end = weight_end;
}

// 0x00C9C670 (.h:224)
inline float vfx_particle_system_emitter::get_weight_end()
{
    return m_weight_end;
}

// 0x00C9C680 (.h:225)
inline void vfx_particle_system_emitter::set_weight_scale(float weight_scale)
{
    m_weight_scale = weight_scale;
}

// 0x00C9C690 (.h:226)
inline float vfx_particle_system_emitter::get_weight_scale()
{
    return m_weight_scale;
}

// 0x00C9C6A0 (.h:231)
inline void vfx_particle_system_emitter::set_density(float _density)
{
    density = _density;
}

// 0x00C9C6B0 (.h:232)
inline float vfx_particle_system_emitter::get_density()
{
    return density;
}

// 0x00C9C6C0 (.h:233)
inline void vfx_particle_system_emitter::set_density_variance(float _density_variance)
{
    density_variance = _density_variance;
}

// 0x00C9C6D0 (.h:234)
inline float vfx_particle_system_emitter::get_density_variance()
{
    return density_variance;
}

// 0x00C9C6E0 (.h:235)
inline void vfx_particle_system_emitter::set_use_density_variance(bool _use_density_variance)
{
    use_density_variance = _use_density_variance;
}

// 0x00C9C6F0 (.h:236)
inline bool vfx_particle_system_emitter::get_use_density_variance()
{
    return use_density_variance;
}

// 0x00C9C700 (.h:245)
inline void vfx_particle_system_emitter::set_num_tails(int _num_tails)
{
    num_tails = _num_tails;
}

// 0x00C9C710 (.h:246)
inline int vfx_particle_system_emitter::get_num_tails()
{
    return num_tails;
}

// 0x00C9C910 (vfx_particle_system_emitter.inl:284)
inline bool vfx_particle_system_emitter::get_use_accel_values()
{
    return use_accel_values;
}

// 0x00C9C920 (vfx_particle_system_emitter.inl:289)
inline void vfx_particle_system_emitter::set_use_accel_values(bool _use_accel_values)
{
    use_accel_values = _use_accel_values;
}

// 0x00C9C990 (vfx_particle_system_emitter.inl:376)
inline const vector& vfx_particle_system_emitter::get_acceleration_start()
{
    return acceleration_start;
}

// 0x00C9C9A0 (vfx_particle_system_emitter.inl:381)
inline void vfx_particle_system_emitter::set_acceleration_start(const vector& _acceleration_start)
{
    acceleration_start = _acceleration_start;
}

// 0x00C9C9C0 (vfx_particle_system_emitter.inl:386)
inline const vector& vfx_particle_system_emitter::get_acceleration_end()
{
    return acceleration_end;
}

// 0x00C9C9D0 (vfx_particle_system_emitter.inl:391)
inline void vfx_particle_system_emitter::set_acceleration_end(const vector& _acceleration_end)
{
    acceleration_end = _acceleration_end;
}

// 0x00C9C9F0 (vfx_particle_system_emitter.inl:409)
inline bool vfx_particle_system_emitter::get_use_accel_variance()
{
    return use_accel_variance;
}

// 0x00C9CA00 (vfx_particle_system_emitter.inl:414)
inline void vfx_particle_system_emitter::set_use_accel_variance(bool _use_accel_variance)
{
    use_accel_variance = _use_accel_variance;
}

// 0x00C9CA10 (vfx_particle_system_emitter.inl:422)
inline const vector& vfx_particle_system_emitter::get_acceleration_start_variance()
{
    return acceleration_start_variance;
}

// 0x00C9CA20 (vfx_particle_system_emitter.inl:427)
inline void vfx_particle_system_emitter::set_acceleration_start_variance(const vector& _acceleration_start_variance)
{
    acceleration_start_variance = _acceleration_start_variance;
}

// 0x00C9CA40 (vfx_particle_system_emitter.inl:432)
inline const vector& vfx_particle_system_emitter::get_acceleration_end_variance()
{
    return acceleration_end_variance;
}

// 0x00C9CA50 (vfx_particle_system_emitter.inl:437)
inline void vfx_particle_system_emitter::set_acceleration_end_variance(const vector& _acceleration_end_variance)
{
    acceleration_end_variance = _acceleration_end_variance;
}

// 0x00C9CA70 (vfx_particle_system_emitter.inl:461)
inline void vfx_particle_system_emitter::set_position_keys(vfx_vector_key* _position_keys, int _num_position_keys)
{
    num_position_keys = _num_position_keys;
    position_keys = _position_keys;
}

// 0x00C9CAC0 (vfx_particle_system_emitter.inl:488)
inline void vfx_particle_system_emitter::set_scale_keys(vfx_vector_key* _scale_keys, int _num_scale_keys)
{
    num_scale_keys = _num_scale_keys;
    scale_keys = _scale_keys;
}

// 0x00CA30C0 (vfx_particle_system_emitter.inl:505)
inline void vfx_particle_system_emitter::set_scale(float scale)
{
    (scale_keys->value).x = scale;
    (scale_keys->value).y = scale;
    (scale_keys->value).z = scale;
    num_scale_keys = 1;
}

// 0x00C9CAE0 (vfx_particle_system_emitter.inl:518)
inline void vfx_particle_system_emitter::set_scale_x(float scale_x)
{
    (scale_keys->value).x = scale_x;
    num_scale_keys = 1;
}

// 0x00C9CB00 (vfx_particle_system_emitter.inl:530)
inline void vfx_particle_system_emitter::set_scale_y(float scale_y)
{
    (scale_keys->value).y = scale_y;
    num_scale_keys = 1;
}

// 0x00C9CB20 (vfx_particle_system_emitter.inl:542)
inline void vfx_particle_system_emitter::set_scale_z(float scale_z)
{
    (scale_keys->value).z = scale_z;
    num_scale_keys = 1;
}

// 0x005D1830 (vfx_particle_system_emitter.inl:549)
inline float vfx_particle_system_emitter::get_opacity_scale() const
{
    return m_opacity_scale;
}

// 0x00C9CB40 (vfx_particle_system_emitter.inl:554)
inline void vfx_particle_system_emitter::set_opacity_scale(float opacity_scale)
{
    m_opacity_scale = opacity_scale;
}

// 0x00C9CB60 (vfx_particle_system_emitter.inl:565)
inline void vfx_particle_system_emitter::set_opacity_start(float opacity_start)
{
    color_start.alpha = opacity_start;
}

// 0x00C9CB70 (vfx_particle_system_emitter.inl:576)
inline void vfx_particle_system_emitter::set_opacity_mid_A(float opacity_mid_A)
{
    color_mid_A.alpha = opacity_mid_A;
}

// 0x00C9CB90 (vfx_particle_system_emitter.inl:587)
inline void vfx_particle_system_emitter::set_opacity_mid_B(float opacity_mid_B)
{
    color_mid_B.alpha = opacity_mid_B;
}

// 0x00C9CBB0 (vfx_particle_system_emitter.inl:598)
inline void vfx_particle_system_emitter::set_opacity_end(float opacity_end)
{
    color_end.alpha = opacity_end;
}

// 0x00C9CBC0 (vfx_particle_system_emitter.inl:604)
inline void vfx_particle_system_emitter::set_emission_rate_keys(vfx_float_key* _emission_rate_keys, int _num_emission_rate_keys)
{
    num_emission_rate_keys = _num_emission_rate_keys;
    emission_rate_keys = _emission_rate_keys;
}

// 0x006E1860 (vfx_particle_system_emitter.inl:610)
inline vfx_float_key* vfx_particle_system_emitter::get_emission_rate_keys()
{
    return emission_rate_keys;
}

// 0x00C9CBE0 (vfx_particle_system_emitter.inl:615)
inline int vfx_particle_system_emitter::get_num_emission_rate_keys() const
{
    return num_emission_rate_keys;
}

// 0x00C9CBF0 (vfx_particle_system_emitter.inl:620)
inline void vfx_particle_system_emitter::set_angular_speed_start_keys(vfx_float_key* _angular_speed_start_keys, int _num_angular_speed_start_keys)
{
    num_angular_speed_start_keys = _num_angular_speed_start_keys;
    angular_speed_start_keys = _angular_speed_start_keys;
}

// 0x00C9CC30 (vfx_particle_system_emitter.inl:633)
inline vfx_float_key* vfx_particle_system_emitter::get_angular_speed_start_keys()
{
    return angular_speed_start_keys;
}

// 0x00C9CC10 (vfx_particle_system_emitter.inl:626)
inline void vfx_particle_system_emitter::set_angular_speed_end_keys(vfx_float_key* _angular_speed_end_keys, int _num_angular_speed_end_keys)
{
    num_angular_speed_end_keys = _num_angular_speed_end_keys;
    angular_speed_end_keys = _angular_speed_end_keys;
}

// 0x00C9CC40 (vfx_particle_system_emitter.inl:638)
inline vfx_float_key* vfx_particle_system_emitter::get_angular_speed_end_keys()
{
    return angular_speed_end_keys;
}

// 0x00C9CC50 (vfx_particle_system_emitter.inl:644)
inline void vfx_particle_system_emitter::set_speed_keys(vfx_float_key* _speed_keys, int _num_speed_keys)
{
    num_speed_keys = _num_speed_keys;
    speed_keys = _speed_keys;
}

// 0x00C9CC70 (vfx_particle_system_emitter.inl:650)
inline vfx_float_key* vfx_particle_system_emitter::get_speed_keys()
{
    return speed_keys;
}

// 0x00C9D1A0 (vfx_particle_system_emitter.inl:1070)
inline void vfx_particle_system_emitter::set_texture_anim_rate_keys(vfx_float_key* texture_anim_rate_keys, int num_texture_anim_rate_keys)
{
    m_num_texture_anim_rate_keys = num_texture_anim_rate_keys;
    m_texture_anim_rate_keys = texture_anim_rate_keys;
}

// 0x00C9D1C0 (vfx_particle_system_emitter.inl:1076)
inline vfx_float_key* vfx_particle_system_emitter::get_texture_anim_rate_keys() const
{
    return m_texture_anim_rate_keys;
}

// 0x00C9CC80 (vfx_particle_system_emitter.inl:657)
inline void vfx_particle_system_emitter::set_size_start_keys(vfx_float_key* _size_start_keys, int _num_size_start_keys)
{
    num_size_start_keys = _num_size_start_keys;
    size_start_keys = _size_start_keys;
}

// 0x00C9CCA0 (vfx_particle_system_emitter.inl:663)
inline vfx_float_key* vfx_particle_system_emitter::get_size_start_keys()
{
    return size_start_keys;
}

// 0x00C9CCB0 (vfx_particle_system_emitter.inl:668)
inline void vfx_particle_system_emitter::set_size_start_var_keys(vfx_float_key* _size_start_var_keys, int _num_size_start_var_keys)
{
    num_size_start_var_keys = _num_size_start_var_keys;
    size_start_var_keys = _size_start_var_keys;
}

// 0x00C9CCD0 (vfx_particle_system_emitter.inl:679)
inline void vfx_particle_system_emitter::set_size_mida_keys(vfx_float_key* _size_mida_keys, int _num_size_mida_keys)
{
    num_size_mida_keys = _num_size_mida_keys;
    size_mida_keys = _size_mida_keys;
}

// 0x00C9CCF0 (vfx_particle_system_emitter.inl:685)
inline vfx_float_key* vfx_particle_system_emitter::get_size_mida_keys()
{
    return size_mida_keys;
}

// 0x00C9CD00 (vfx_particle_system_emitter.inl:690)
inline void vfx_particle_system_emitter::set_size_mida_var_keys(vfx_float_key* _size_mida_var_keys, int _num_size_mida_var_keys)
{
    num_size_mida_var_keys = _num_size_mida_var_keys;
    size_mida_var_keys = _size_mida_var_keys;
}

// 0x00C9CD20 (vfx_particle_system_emitter.inl:701)
inline void vfx_particle_system_emitter::set_size_mida_offset_keys(vfx_float_key* _size_mida_offset_keys, int _num_size_mida_offset_keys)
{
    num_size_mida_offset_keys = _num_size_mida_offset_keys;
    size_mida_offset_keys = _size_mida_offset_keys;
}

// 0x00C9CD40 (vfx_particle_system_emitter.inl:712)
inline void vfx_particle_system_emitter::set_size_midb_keys(vfx_float_key* _size_midb_keys, int _num_size_midb_keys)
{
    num_size_midb_keys = _num_size_midb_keys;
    size_midb_keys = _size_midb_keys;
}

// 0x00C9CD60 (vfx_particle_system_emitter.inl:718)
inline vfx_float_key* vfx_particle_system_emitter::get_size_midb_keys()
{
    return size_midb_keys;
}

// 0x00C9CD70 (vfx_particle_system_emitter.inl:723)
inline void vfx_particle_system_emitter::set_size_midb_var_keys(vfx_float_key* _size_midb_var_keys, int _num_size_midb_var_keys)
{
    num_size_midb_var_keys = _num_size_midb_var_keys;
    size_midb_var_keys = _size_midb_var_keys;
}

// 0x00C9CD90 (vfx_particle_system_emitter.inl:734)
inline void vfx_particle_system_emitter::set_size_midb_offset_keys(vfx_float_key* _size_midb_offset_keys, int _num_size_midb_offset_keys)
{
    num_size_midb_offset_keys = _num_size_midb_offset_keys;
    size_midb_offset_keys = _size_midb_offset_keys;
}

// 0x00C9CDB0 (vfx_particle_system_emitter.inl:745)
inline void vfx_particle_system_emitter::set_size_end_keys(vfx_float_key* _size_end_keys, int _num_size_end_keys)
{
    num_size_end_keys = _num_size_end_keys;
    size_end_keys = _size_end_keys;
}

// 0x00C9CDD0 (vfx_particle_system_emitter.inl:751)
inline vfx_float_key* vfx_particle_system_emitter::get_size_end_keys()
{
    return size_end_keys;
}

// 0x00C9CDE0 (vfx_particle_system_emitter.inl:756)
inline void vfx_particle_system_emitter::set_size_end_var_keys(vfx_float_key* _size_end_var_keys, int _num_size_end_var_keys)
{
    num_size_end_var_keys = _num_size_end_var_keys;
    size_end_var_keys = _size_end_var_keys;
}

// 0x00C9CE00 (vfx_particle_system_emitter.inl:774)
inline void vfx_particle_system_emitter::set_size_start_variance(float size_start_variance)
{
    size_start_var_keys->value = size_start_variance;
    num_size_start_var_keys = 1;
}

// 0x00C9CE20 (vfx_particle_system_emitter.inl:786)
inline void vfx_particle_system_emitter::set_size_end_variance(float size_end_variance)
{
    size_end_var_keys->value = size_end_variance;
    num_size_end_var_keys = 1;
}

// 0x00C9CE40 (vfx_particle_system_emitter.inl:798). Notes (original bug, kept): sets the mid A size key count, not
// the variance key count.
inline void vfx_particle_system_emitter::set_size_mida_variance(float size_mida_variance)
{
    size_mida_var_keys->value = size_mida_variance;
    num_size_mida_keys = 1;
}

// 0x00C9CE60 (vfx_particle_system_emitter.inl:810)
inline void vfx_particle_system_emitter::set_size_midb_variance(float size_midb_variance)
{
    size_midb_var_keys->value = size_midb_variance;
    num_size_midb_var_keys = 1;
}

// 0x00C9C7F0 (vfx_particle_system_emitter.inl:142)
inline void vfx_particle_system_emitter::set_size_scale(float size_scale)
{
    m_size_scale = size_scale;
}

// 0x00C9CFF0 (vfx_particle_system_emitter.inl:999)
inline void vfx_particle_system_emitter::set_emission_rate_scale(float emission_rate_scale)
{
    m_emission_rate_scale = emission_rate_scale;
}

// 0x00C9D010 (vfx_particle_system_emitter.inl:1005)
inline float vfx_particle_system_emitter::get_lod_emission_distance() const
{
    return m_lod_emission_distance;
}

// 0x00C9D020 (vfx_particle_system_emitter.inl:1010)
inline void vfx_particle_system_emitter::set_lod_emission_distance(float lod_emission_distance)
{
    m_lod_emission_distance = lod_emission_distance;
}

// 0x00C9CE80 (vfx_particle_system_emitter.inl:820)
inline void vfx_particle_system_emitter::set_length_start_keys(vfx_float_key* _length_start_keys, int _num_length_start_keys)
{
    num_length_start_keys = _num_length_start_keys;
    length_start_keys = _length_start_keys;
}

// 0x00C9CEA0 (vfx_particle_system_emitter.inl:831)
inline void vfx_particle_system_emitter::set_length_start_var_keys(vfx_float_key* _length_start_var_keys, int _num_length_start_var_keys)
{
    num_length_start_var_keys = _num_length_start_var_keys;
    length_start_var_keys = _length_start_var_keys;
}

// 0x00C9CEC0 (vfx_particle_system_emitter.inl:842)
inline void vfx_particle_system_emitter::set_length_mida_keys(vfx_float_key* _length_mida_keys, int _num_length_mida_keys)
{
    num_length_mida_keys = _num_length_mida_keys;
    length_mida_keys = _length_mida_keys;
}

// 0x00C9CEE0 (vfx_particle_system_emitter.inl:853)
inline void vfx_particle_system_emitter::set_length_mida_var_keys(vfx_float_key* _length_mida_var_keys, int _num_length_mida_var_keys)
{
    num_length_mida_var_keys = _num_length_mida_var_keys;
    length_mida_var_keys = _length_mida_var_keys;
}

// 0x00C9CF00 (vfx_particle_system_emitter.inl:864)
inline void vfx_particle_system_emitter::set_length_mida_offset_keys(vfx_float_key* _length_mida_offset_keys, int _num_length_mida_offset_keys)
{
    num_length_mida_offset_keys = _num_length_mida_offset_keys;
    length_mida_offset_keys = _length_mida_offset_keys;
}

// 0x00C9CF20 (vfx_particle_system_emitter.inl:875)
inline void vfx_particle_system_emitter::set_length_midb_keys(vfx_float_key* _length_midb_keys, int _num_length_midb_keys)
{
    num_length_midb_keys = _num_length_midb_keys;
    length_midb_keys = _length_midb_keys;
}

// 0x00C9CF40 (vfx_particle_system_emitter.inl:886)
inline void vfx_particle_system_emitter::set_length_midb_var_keys(vfx_float_key* _length_midb_var_keys, int _num_length_midb_var_keys)
{
    num_length_midb_var_keys = _num_length_midb_var_keys;
    length_midb_var_keys = _length_midb_var_keys;
}

// 0x00C9CF60 (vfx_particle_system_emitter.inl:897)
inline void vfx_particle_system_emitter::set_length_midb_offset_keys(vfx_float_key* _length_midb_offset_keys, int _num_length_midb_offset_keys)
{
    num_length_midb_offset_keys = _num_length_midb_offset_keys;
    length_midb_offset_keys = _length_midb_offset_keys;
}

// 0x00C9CF80 (vfx_particle_system_emitter.inl:908)
inline void vfx_particle_system_emitter::set_length_end_keys(vfx_float_key* _length_end_keys, int _num_length_end_keys)
{
    num_length_end_keys = _num_length_end_keys;
    length_end_keys = _length_end_keys;
}

// 0x00C9CFA0 (vfx_particle_system_emitter.inl:919)
inline void vfx_particle_system_emitter::set_length_end_var_keys(vfx_float_key* _length_end_var_keys, int _num_length_end_var_keys)
{
    num_length_end_var_keys = _num_length_end_var_keys;
    length_end_var_keys = _length_end_var_keys;
}

// 0x00C9CFC0 (vfx_particle_system_emitter.inl:979)
inline void vfx_particle_system_emitter::set_animation_duration(float _animation_duration)
{
    animation_duration = _animation_duration;
}

// 0x00C9CFE0 (vfx_particle_system_emitter.inl:984)
inline float vfx_particle_system_emitter::get_animation_duration()
{
    return animation_duration;
}

// 0x00C9D160 (vfx_particle_system_emitter.inl:1048)
inline rl_renderable_particles* vfx_particle_system_emitter::get_renderable_particles() const
{
    return m_renderable_p;
}

// 0x00C9D170 (vfx_particle_system_emitter.inl:1053)
inline void vfx_particle_system_emitter::set_renderable(rl_renderable_particles* renderable_p)
{
    m_renderable_p = renderable_p;
}

// 0x00C9D180 (vfx_particle_system_emitter.inl:1059)
inline bool vfx_particle_system_emitter::get_inherit_velocity() const
{
    return m_inherit_velocity;
}

// 0x00C9D190 (vfx_particle_system_emitter.inl:1064)
inline void vfx_particle_system_emitter::set_inherit_velocity(bool inherit_velocity)
{
    m_inherit_velocity = inherit_velocity;
}

