// Original: volition/render_lib/code/rl_corona_instance.inl (inline members; included at the end of
// rl_corona_instance.h)
//
// The corona state accessors (VFX.md FX2), plain field reads and writes.
#pragma once

// 0x004BD110 (rl_corona_instance.inl:3)
inline int rl_corona_instance::get_type() const
{
    return m_type;
}

// 0x00C9EB10 (rl_corona_instance.inl:8)
inline void rl_corona_instance::set_type(int type)
{
    m_type = type;
}

// 0x004BD120 (rl_corona_instance.inl:13)
inline const vector& rl_corona_instance::get_position() const
{
    return m_position;
}

// 0x00C9EB20 (rl_corona_instance.inl:18)
inline void rl_corona_instance::set_position(const vector& position)
{
    m_position = position;
}

// 0x004BD130 (rl_corona_instance.inl:24)
inline const matrix& rl_corona_instance::get_rotation() const
{
    return m_rotation;
}

// 0x00C9EB40 (rl_corona_instance.inl:29)
inline void rl_corona_instance::set_rotation(const matrix& rotation)
{
    m_rotation = rotation;
}

// 0x004BD140 (rl_corona_instance.inl:35)
inline const rl_color_float& rl_corona_instance::get_color_start() const
{
    return m_color_start;
}

// 0x00C9EB80 (rl_corona_instance.inl:40)
inline void rl_corona_instance::set_color_start(const rl_color_float& color_start)
{
    m_color_start = color_start;
}

// 0x004BD150 (rl_corona_instance.inl:46)
inline const rl_color_float& rl_corona_instance::get_color_end() const
{
    return m_color_end;
}

// 0x00C9EBA0 (rl_corona_instance.inl:51)
inline void rl_corona_instance::set_color_end(const rl_color_float& color_end)
{
    m_color_end = color_end;
}

// 0x004BD160 (rl_corona_instance.inl:57)
inline float rl_corona_instance::get_saturation() const
{
    return m_saturation;
}

// 0x00C9EBC0 (rl_corona_instance.inl:62)
inline void rl_corona_instance::set_saturation(float saturation)
{
    m_saturation = saturation;
}

// 0x004BD170 (rl_corona_instance.inl:68)
inline float rl_corona_instance::get_additive_key() const
{
    return m_additive_key;
}

// 0x004BD180 (rl_corona_instance.inl:79)
inline float rl_corona_instance::get_radius_start() const
{
    return m_radius_start;
}

// 0x00C9EBD0 (rl_corona_instance.inl:84)
inline void rl_corona_instance::set_radius_start(float radius_start)
{
    m_radius_start = radius_start;
}

// 0x004BD190 (rl_corona_instance.inl:90)
inline float rl_corona_instance::get_radius_end() const
{
    return m_radius_end;
}

// 0x00C9EBE0 (rl_corona_instance.inl:95)
inline void rl_corona_instance::set_radius_end(float radius_end)
{
    m_radius_end = radius_end;
}

// 0x004BD1A0 (rl_corona_instance.inl:101)
inline float rl_corona_instance::get_width_scale() const
{
    return m_width_scale;
}

// 0x00C9EBF0 (rl_corona_instance.inl:106)
inline void rl_corona_instance::set_width_scale(float width_scale)
{
    m_width_scale = width_scale;
}

// 0x004BD1B0 (rl_corona_instance.inl:112)
inline float rl_corona_instance::get_current_elapsed_time() const
{
    return m_current_elapsed_time;
}

// 0x004BD1C0 (rl_corona_instance.inl:123)
inline bool rl_corona_instance::get_random_anim_frames() const
{
    return m_random_anim_frames;
}

// 0x004BD1D0 (rl_corona_instance.inl:134)
inline float rl_corona_instance::get_anim_rate() const
{
    return m_anim_rate;
}

// 0x004BD1E0 (rl_corona_instance.inl:145)
inline bool rl_corona_instance::get_infinite_depth() const
{
    return m_infinite_depth;
}

// 0x00743B60 (rl_corona_instance.inl:150)
inline void rl_corona_instance::set_infinite_depth(bool infinite_depth)
{
    m_infinite_depth = infinite_depth;
}

// 0x004BD1F0 (rl_corona_instance.inl:155)
inline float rl_corona_instance::get_visibility_fade() const
{
    return m_visibility_fade;
}

// 0x00743B70 (rl_corona_instance.inl:160)
inline void rl_corona_instance::set_visibility_fade(float visibility_fade)
{
    m_visibility_fade = visibility_fade;
}

// 0x004BD200 (rl_corona_instance.inl:166)
inline float rl_corona_instance::get_rotation_scale() const
{
    return m_rotation_scale;
}

// 0x00C9EC00 (rl_corona_instance.inl:171)
inline void rl_corona_instance::set_rotation_scale(float rotation_scale)
{
    m_rotation_scale = rotation_scale;
}

// 0x004BD210 (rl_corona_instance.inl:177)
inline float rl_corona_instance::get_rotation_offset() const
{
    return m_rotation_offset;
}

// 0x00C9EC20 (rl_corona_instance.inl:182)
inline void rl_corona_instance::set_rotation_offset(float rotation_offset)
{
    m_rotation_offset = rotation_offset;
}

// 0x004BD220 (rl_corona_instance.inl:188)
inline float rl_corona_instance::get_position_offset() const
{
    return m_position_offset;
}

// 0x00C9EC40 (rl_corona_instance.inl:193)
inline void rl_corona_instance::set_position_offset(float position_offset)
{
    m_position_offset = position_offset;
}

// 0x004BD230 (rl_corona_instance.inl:199)
inline float rl_corona_instance::get_intensity_scale() const
{
    return m_intensity_scale;
}

// 0x00C9EC60 (rl_corona_instance.inl:204)
inline void rl_corona_instance::set_intensity_scale(float intensity_scale)
{
    m_intensity_scale = intensity_scale;
}

// 0x004BD240 (rl_corona_instance.inl:210)
inline float rl_corona_instance::get_intensity_exponential_decay() const
{
    return m_intensity_exponential_decay;
}

// 0x00C9EC80 (rl_corona_instance.inl:215)
inline void rl_corona_instance::set_intensity_exponential_decay(float intensity_exponential_decay)
{
    m_intensity_exponential_decay = intensity_exponential_decay;
}

// 0x004BD250 (rl_corona_instance.inl:221)
inline float rl_corona_instance::get_opacity_exponential_decay() const
{
    return m_opacity_exponential_decay;
}

// 0x00C9ECA0 (rl_corona_instance.inl:226)
inline void rl_corona_instance::set_opacity_exponential_decay(float opacity_exponential_decay)
{
    m_opacity_exponential_decay = opacity_exponential_decay;
}

// 0x004BD280 (rl_corona_instance.inl:253)
inline float rl_corona_instance::get_radius_distance_start() const
{
    return m_radius_distance_start;
}

// 0x00C9ED00 (rl_corona_instance.inl:258)
inline void rl_corona_instance::set_radius_distance_start(float radius_distance_start)
{
    m_radius_distance_start = radius_distance_start;
}

// 0x004BD290 (rl_corona_instance.inl:264)
inline float rl_corona_instance::get_radius_distance_end() const
{
    return m_radius_distance_end;
}

// 0x00C9ED20 (rl_corona_instance.inl:269)
inline void rl_corona_instance::set_radius_distance_end(float radius_distance_end)
{
    m_radius_distance_end = radius_distance_end;
}

// 0x004BD260 (rl_corona_instance.inl:231)
inline float rl_corona_instance::get_color_distance_start() const
{
    return m_color_distance_start;
}

// 0x00C9ECC0 (rl_corona_instance.inl:236)
inline void rl_corona_instance::set_color_distance_start(float color_distance_start)
{
    m_color_distance_start = color_distance_start;
}

// 0x004BD270 (rl_corona_instance.inl:242)
inline float rl_corona_instance::get_color_distance_end() const
{
    return m_color_distance_end;
}

// 0x00C9ECE0 (rl_corona_instance.inl:247)
inline void rl_corona_instance::set_color_distance_end(float color_distance_end)
{
    m_color_distance_end = color_distance_end;
}

// 0x004BD2A0 (rl_corona_instance.inl:275)
inline float rl_corona_instance::get_view_fade_scale() const
{
    return m_view_fade_scale;
}

// 0x00C9ED40 (rl_corona_instance.inl:280)
inline void rl_corona_instance::set_view_fade_scale(float view_fade_scale)
{
    m_view_fade_scale = view_fade_scale;
}

// 0x004BD2B0 (rl_corona_instance.inl:286)
inline float rl_corona_instance::get_view_fade_decay() const
{
    return m_view_fade_decay;
}

// 0x00C9ED60 (rl_corona_instance.inl:291)
inline void rl_corona_instance::set_view_fade_decay(float view_fade_decay)
{
    m_view_fade_decay = view_fade_decay;
}

// 0x004BD2C0 (rl_corona_instance.inl:297)
inline bool rl_corona_instance::get_view_fade_intensity_enabled() const
{
    return m_view_fade_intensity_enabled;
}

// 0x00C9ED80 (rl_corona_instance.inl:302)
inline void rl_corona_instance::set_view_fade_intensity_enabled(bool view_fade_intensity_enabled)
{
    m_view_fade_intensity_enabled = view_fade_intensity_enabled;
}

// 0x004BD2D0 (rl_corona_instance.inl:308)
inline bool rl_corona_instance::get_view_fade_opacity_enabled() const
{
    return m_view_fade_opacity_enabled;
}

// 0x00C9ED90 (rl_corona_instance.inl:313)
inline void rl_corona_instance::set_view_fade_opacity_enabled(bool view_fade_opacity_enabled)
{
    m_view_fade_opacity_enabled = view_fade_opacity_enabled;
}

// 0x004BD2E0 (rl_corona_instance.inl:319)
inline bool rl_corona_instance::get_view_fade_radius_enabled() const
{
    return m_view_fade_radius_enabled;
}

// 0x00C9EDA0 (rl_corona_instance.inl:324)
inline void rl_corona_instance::set_view_fade_radius_enabled(bool view_fade_radius_enabled)
{
    m_view_fade_radius_enabled = view_fade_radius_enabled;
}

// 0x004BD2F0 (rl_corona_instance.inl:330)
inline int rl_corona_instance::get_texture_handle() const
{
    return m_texture_handle;
}

// 0x00C9EDB0 (rl_corona_instance.inl:335)
inline void rl_corona_instance::set_texture_handle(int texture_handle)
{
    m_texture_handle = texture_handle;
}

// 0x004BD300 (rl_corona_instance.inl:341)
inline const matrix& rl_corona_instance::get_color_correction_matrix() const
{
    return m_color_correction_matrix;
}

// 0x004BD310 (rl_corona_instance.inl:352)
inline float rl_corona_instance::get_visibility_radius() const
{
    return m_visibility_radius;
}

// 0x00C9EDC0 (rl_corona_instance.inl:357)
inline void rl_corona_instance::set_visibility_radius(float visibility_radius)
{
    m_visibility_radius = visibility_radius;
}

// 0x004BD320 (rl_corona_instance.inl:363)
inline rl_stamped_handle rl_corona_instance::get_query_no_depth_handle() const
{
    return m_query_no_depth_handle;
}

// 0x004BD340 (rl_corona_instance.inl:368)
inline void rl_corona_instance::set_query_no_depth_handle(rl_stamped_handle query_no_depth_handle)
{
    m_query_no_depth_handle = query_no_depth_handle;
}

// 0x004BD360 (rl_corona_instance.inl:374)
inline rl_stamped_handle rl_corona_instance::get_query_depth_handle() const
{
    return m_query_depth_handle;
}

// 0x004BD380 (rl_corona_instance.inl:379)
inline void rl_corona_instance::set_query_depth_handle(rl_stamped_handle query_depth_handle)
{
    m_query_depth_handle = query_depth_handle;
}

// 0x004BD3A0 (rl_corona_instance.inl:385)
inline int rl_corona_instance::get_depth_pixels_drawn() const
{
    return m_depth_pixels_drawn;
}

// 0x004BD3B0 (rl_corona_instance.inl:390)
inline void rl_corona_instance::set_depth_pixels_drawn(int depth_pixels_drawn)
{
    m_depth_pixels_drawn = depth_pixels_drawn;
}

// 0x004BD3C0 (rl_corona_instance.inl:396)
inline int rl_corona_instance::get_no_depth_pixels_drawn() const
{
    return m_no_depth_pixels_drawn;
}

// 0x004BD3D0 (rl_corona_instance.inl:401)
inline void rl_corona_instance::set_no_depth_pixels_drawn(int no_depth_pixels_drawn)
{
    m_no_depth_pixels_drawn = no_depth_pixels_drawn;
}

// 0x004BD400 (rl_corona_instance.inl:412)
inline float rl_corona_instance::get_opacity() const
{
    return m_opacity;
}

// 0x004BD3E0 (rl_corona_instance.inl:407)
inline void rl_corona_instance::set_opacity(float opacity)
{
    m_opacity = opacity;
}

// 0x004BD410 (rl_corona_instance.inl:417)
inline float rl_corona_instance::get_opacity_visibility_decay() const
{
    return m_opacity_visibility_decay;
}

// 0x00C9EDE0 (rl_corona_instance.inl:422)
inline void rl_corona_instance::set_opacity_visibility_decay(float opacity_visibility_decay)
{
    m_opacity_visibility_decay = opacity_visibility_decay;
}

// 0x004BD420 (rl_corona_instance.inl:428)
inline float rl_corona_instance::get_radius_visibility_decay() const
{
    return m_radius_visibility_decay;
}

// 0x00C9EE00 (rl_corona_instance.inl:433)
inline void rl_corona_instance::set_radius_visibility_decay(float radius_visibility_decay)
{
    m_radius_visibility_decay = radius_visibility_decay;
}

// 0x004BD430 (rl_corona_instance.inl:439)
inline float rl_corona_instance::get_visibility() const
{
    return m_visibility;
}

// 0x004BD440 (rl_corona_instance.inl:444)
inline void rl_corona_instance::set_visibility(float visibility)
{
    m_visibility = visibility;
}

