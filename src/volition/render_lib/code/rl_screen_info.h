// Original: volition/render_lib/code/rl_screen_info.h
//
// rl_screen_info, the screen and level-of-detail settings (resolution, antialiasing, LOD scales) render_lib renders
// with.
#pragma once

#include "keen/graphics/graphicssystem.hpp"
#include "volition/render_lib/code/rl_particle.h"
#include "volition/render_lib/code/rl_shadow_system.h"

#include <string.h>

enum rl_anisotropic_filtering_lod
{
    RL_AF_LOD_OFF = 0,
    RL_AF_LOD_LOW = 1,
    RL_AF_LOD_MED = 2,
    RL_AF_LOD_HIGH = 3,
    RL_NUM_AF_LODS = 4,
};

enum rl_anti_alias_level
{
    RLAA_NONE = 0,
    RLAA_2X = 1,
    RLAA_4X = 2,
    RLAA_8X = 3,
    RLAA_16X = 4,
    RL_NUM_AA_LEVELS = 5,
};

enum rl_antialias_lod
{
    RL_AA_LOD_OFF = 0,
    RL_AA_LOD_2X = 1,
    RL_AA_LOD_4X = 2,
    RL_AA_LOD_8X = 3,
    RL_AA_LOD_16X = 4,
    RL_AA_LOD_8XQ = 5,
    RL_AA_LOD_16XQ = 6,
    RL_NUM_AA_LODS = 7,
};

// size 0x1
struct new_rl_screen_info_settings
{
};

// size 0xA0
class rl_screen_info
{
public:
    rl_screen_info(); // 0x00459600 (.h:48)
    void set_screen_width(int width); // 0x004DC850 (.h:55)
    int get_screen_width() const; // 0x00458AE0 (.h:56)
    void set_screen_height(int height); // 0x004DC880 (.h:59)
    int get_screen_height() const; // 0x00458AF0 (.h:60)
    float get_aspect_ratio() const; // 0x004B6820 (.h:63)
    void set_window_mode(keen::graphics::WindowMode window_mode); // 0x004B6830 (.h:66)
    keen::graphics::WindowMode get_window_mode() const; // 0x00458B00 (.h:67)
    void set_refresh_rate(int num, int den); // 0x004B6840 (.h:70)
    void get_refresh_rate(int& num, int& den) const; // 0x00458B10 (.h:71)
    unsigned int get_monitor() const; // 0x007B9970 (.h:73)
    void set_monitor(unsigned int monitor); // 0x004B6860 (.h:74)
    bool get_vsync_enabled() const; // 0x00458B30 (.h:76)
    void set_vsync_enabled(bool enabled); // 0x004B6870 (.h:77)
private:
    void update_aspect_ratio(); // 0x00458B40 (.h:81)

    int m_current_width; // 0x00
    int m_current_height; // 0x04
    int m_rr_num; // 0x08
    int m_rr_den; // 0x0C
    float m_aspect_ratio; // 0x10
    unsigned int m_monitor; // 0x14
    keen::graphics::WindowMode m_window_mode; // 0x18
    bool m_vsync_enabled; // 0x1C
    new_rl_screen_info_settings m_new_info; // 0x1D
    unsigned char padding[127]; // 0x1E
};

// size 0x8
struct new_rl_level_of_detail_settings
{
    unsigned char m_texture_reduction; // 0x00
    rl_anisotropic_filtering_lod m_anisotropic_filtering; // 0x04
};

// size 0x94
struct rl_level_of_detail
{
    rl_level_of_detail(); // 0x00458B70 (.h:127)

    rl_shadow_lod m_shadow_lod; // 0x00
    rl_antialias_lod m_aa_lod; // 0x04
    rl_particle_lod m_particle_lod; // 0x08
    bool m_soft_shadows; // 0x0C
    bool m_bloom_enabled; // 0x0D
    bool m_motion_blur_enabled; // 0x0E
    bool m_depth_of_field_enabled; // 0x0F
    bool m_distortion_enabled; // 0x10
    bool m_ssao_enabled; // 0x11
    bool m_sun_shafts_enabled; // 0x12
    new_rl_level_of_detail_settings m_new_data; // 0x14
    unsigned char padding[120]; // 0x1C
};

// ---- inline members (no .cpp home)

// 0x00459600 (.h:48)
// Magic numbers: the default screen is 1280x720, windowed. m_vsync_enabled is left as is.
inline rl_screen_info::rl_screen_info()
{
    m_rr_num = 0;
    m_monitor = 0;
    m_window_mode = keen::graphics::WindowMode_Windowed;
    m_current_width = 1280;
    m_current_height = 720;
    m_rr_den = 1;
    m_new_info = new_rl_screen_info_settings();
    memset(padding, 0, sizeof(padding));
    m_aspect_ratio = (float)m_current_width / (float)m_current_height;
}

// 0x004DC850 (.h:55)
inline void rl_screen_info::set_screen_width(int width)
{
    m_current_width = width;
    update_aspect_ratio();
}

// 0x00458AE0 (.h:56)
inline int rl_screen_info::get_screen_width() const
{
    return m_current_width;
}

// 0x004DC880 (.h:59)
inline void rl_screen_info::set_screen_height(int height)
{
    m_current_height = height;
    update_aspect_ratio();
}

// 0x00458AF0 (.h:60)
inline int rl_screen_info::get_screen_height() const
{
    return m_current_height;
}

// 0x004B6820 (.h:63)
inline float rl_screen_info::get_aspect_ratio() const
{
    return m_aspect_ratio;
}

// 0x004B6830 (.h:66)
inline void rl_screen_info::set_window_mode(keen::graphics::WindowMode window_mode)
{
    m_window_mode = window_mode;
}

// 0x00458B00 (.h:67)
inline keen::graphics::WindowMode rl_screen_info::get_window_mode() const
{
    return m_window_mode;
}

// 0x004B6840 (.h:70)
inline void rl_screen_info::set_refresh_rate(int num, int den)
{
    m_rr_num = num;
    m_rr_den = den;
}

// 0x00458B10 (.h:71)
inline void rl_screen_info::get_refresh_rate(int& num, int& den) const
{
    num = m_rr_num;
    den = m_rr_den;
}

// 0x007B9970 (.h:73)
inline unsigned int rl_screen_info::get_monitor() const
{
    return m_monitor;
}

// 0x004B6860 (.h:74)
inline void rl_screen_info::set_monitor(unsigned int monitor)
{
    m_monitor = monitor;
}

// 0x00458B30 (.h:76)
inline bool rl_screen_info::get_vsync_enabled() const
{
    return m_vsync_enabled;
}

// 0x004B6870 (.h:77)
inline void rl_screen_info::set_vsync_enabled(bool enabled)
{
    m_vsync_enabled = enabled;
}

// 0x00458B40 (.h:81)
inline void rl_screen_info::update_aspect_ratio()
{
    m_aspect_ratio = (float)m_current_width / (float)m_current_height;
}

// 0x00458B70 (.h:127)
// The padding is left as is.
inline rl_level_of_detail::rl_level_of_detail()
{
    m_shadow_lod = RL_SHADOW_LOD_MED;
    m_aa_lod = RL_AA_LOD_OFF;
    m_particle_lod = RL_PARTICLE_LOD_LOW;
    m_soft_shadows = true;
    m_bloom_enabled = true;
    m_motion_blur_enabled = true;
    m_depth_of_field_enabled = true;
    m_distortion_enabled = true;
    m_ssao_enabled = true;
    m_sun_shafts_enabled = true;
    m_new_data.m_texture_reduction = 0;
    m_new_data.m_anisotropic_filtering = RL_AF_LOD_OFF;
}
