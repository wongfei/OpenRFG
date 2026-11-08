// Original: volition/render_lib/code/rl_color_correct_state.h
//
// rl_color_correct_state, color correction parameters (saturation, contrast, tint).
#pragma once

#include "volition/render_lib/code/rl_color.h"

// size 0x28
class rl_color_correct_state
{
public:
    rl_color_correct_state(const rl_color_correct_state& __that); // 0x00893F40
    rl_color_correct_state(); // 0x004BBF00 (.h:60)
    ~rl_color_correct_state(); // 0x004BBF60 (.h:71)
    const rl_color_float& get_tint() const; // 0x004BBF70 (.h:76)
    void set_tint(const rl_color_float& tint); // 0x007C0280 (.h:81)
    float get_saturation() const; // 0x004BBF80 (.h:87)
    void set_saturation(float saturation); // 0x007C02A0 (.h:92)
    float get_levels_pregamma_offset() const;
    void set_levels_pregamma_offset(float levels_pregamma_offset); // 0x007C02B0 (.h:103)
    float get_levels_pregamma_scale() const;
    void set_levels_pregamma_scale(float levels_pregamma_scale); // 0x007C02C0 (.h:114)
    float get_levels_postgamma_offset() const;
    void set_levels_postgamma_offset(float levels_postgamma_offset); // 0x007C02D0 (.h:125)
    float get_levels_postgamma_scale() const;
    void set_levels_postgamma_scale(float levels_postgamma_scale); // 0x007C02E0 (.h:136)
    float get_gamma() const;
    void set_gamma(float gamma); // 0x007C02F0 (.h:147)

private:
    rl_color_float m_tint; // 0x00
    float m_saturation; // 0x10
    float m_gamma; // 0x14
    float m_levels_pregamma_offset; // 0x18
    float m_levels_pregamma_scale; // 0x1C
    float m_levels_postgamma_offset; // 0x20
    float m_levels_postgamma_scale; // 0x24
};

// ---- inline members (no .cpp home)

// 0x00893F40 (compiler-generated)
inline rl_color_correct_state::rl_color_correct_state(const rl_color_correct_state& __that)
    : m_tint(__that.m_tint)
    , m_saturation(__that.m_saturation)
    , m_gamma(__that.m_gamma)
    , m_levels_pregamma_offset(__that.m_levels_pregamma_offset)
    , m_levels_pregamma_scale(__that.m_levels_pregamma_scale)
    , m_levels_postgamma_offset(__that.m_levels_postgamma_offset)
    , m_levels_postgamma_scale(__that.m_levels_postgamma_scale)
{
}

// 0x004BBF00 (.h:60)
// White tint, no saturation change, gamma 1, identity levels.
inline rl_color_correct_state::rl_color_correct_state()
    : m_tint(1.0f, 1.0f, 1.0f, 1.0f)
    , m_saturation(1.0f)
    , m_gamma(1.0f)
    , m_levels_pregamma_offset(0.0f)
    , m_levels_pregamma_scale(1.0f)
    , m_levels_postgamma_offset(0.0f)
    , m_levels_postgamma_scale(1.0f)
{
}

// 0x004BBF60 (.h:71)
inline rl_color_correct_state::~rl_color_correct_state()
{
}

// 0x004BBF70 (.h:76)
inline const rl_color_float& rl_color_correct_state::get_tint() const
{
    return m_tint;
}

// 0x007C0280 (.h:81)
inline void rl_color_correct_state::set_tint(const rl_color_float& tint)
{
    m_tint = tint;
}

// 0x004BBF80 (.h:87)
inline float rl_color_correct_state::get_saturation() const
{
    return m_saturation;
}

// 0x007C02A0 (.h:92)
inline void rl_color_correct_state::set_saturation(float saturation)
{
    m_saturation = saturation;
}

inline float rl_color_correct_state::get_levels_pregamma_offset() const
{
    return m_levels_pregamma_offset;
}

// 0x007C02B0 (.h:103)
inline void rl_color_correct_state::set_levels_pregamma_offset(float levels_pregamma_offset)
{
    m_levels_pregamma_offset = levels_pregamma_offset;
}

inline float rl_color_correct_state::get_levels_pregamma_scale() const
{
    return m_levels_pregamma_scale;
}

// 0x007C02C0 (.h:114)
inline void rl_color_correct_state::set_levels_pregamma_scale(float levels_pregamma_scale)
{
    m_levels_pregamma_scale = levels_pregamma_scale;
}

inline float rl_color_correct_state::get_levels_postgamma_offset() const
{
    return m_levels_postgamma_offset;
}

// 0x007C02D0 (.h:125)
inline void rl_color_correct_state::set_levels_postgamma_offset(float levels_postgamma_offset)
{
    m_levels_postgamma_offset = levels_postgamma_offset;
}

inline float rl_color_correct_state::get_levels_postgamma_scale() const
{
    return m_levels_postgamma_scale;
}

// 0x007C02E0 (.h:136)
inline void rl_color_correct_state::set_levels_postgamma_scale(float levels_postgamma_scale)
{
    m_levels_postgamma_scale = levels_postgamma_scale;
}

inline float rl_color_correct_state::get_gamma() const
{
    return m_gamma;
}

// 0x007C02F0 (.h:147)
inline void rl_color_correct_state::set_gamma(float gamma)
{
    m_gamma = gamma;
}
