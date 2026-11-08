// Original: volition/render_lib/code/rl_fog_state.h
// Implementation: volition/render_lib/code/rl_fog_state.cpp
//
// rl_fog_state, atmospheric fog parameters.
#pragma once

#include "volition/render_lib/code/rl_color.h"

// size 0x20
class rl_fog_state
{
public:
    rl_fog_state(const rl_fog_state& __that); // 0x004C7610
    rl_fog_state(); // 0x004BBD60 (.h:64)
    ~rl_fog_state(); // 0x004BBDA0 (.h:68)
    void set_height_density(float height_density); // 0x007C0240 (.h:79)
    float get_height_density() const; // 0x004BBDB0 (.h:74)
    void set_atmosphere_scale(float atmosphere_scale); // 0x007C0250 (.h:90)
    float get_atmosphere_scale() const; // 0x004BBDC0 (.h:85)
    float get_density() const; // 0x004BBDD0 (.h:96)
    void set_density(float density); // 0x007C0260 (.h:101)
    float get_density_offset() const; // 0x004BBDE0 (.h:107)
    void set_density_offset(float density_offset); // 0x007C0270 (.h:112)
    const rl_color_float& get_color() const; // 0x004BBDF0 (.h:118)
    void set_color(const rl_color_float& _color); // 0x004BBE00 (.h:123)

private:
    float m_heigt_density; // 0x00
    float m_atmosphere_scale; // 0x04
    float m_density; // 0x08
    float m_density_offset; // 0x0C
    rl_color_float m_color; // 0x10
};

// ---- inline members (no .cpp home)

// 0x004C7610 (compiler-generated)
inline rl_fog_state::rl_fog_state(const rl_fog_state& __that)
    : m_heigt_density(__that.m_heigt_density)
    , m_atmosphere_scale(__that.m_atmosphere_scale)
    , m_density(__that.m_density)
    , m_density_offset(__that.m_density_offset)
    , m_color(__that.m_color)
{
}

// 0x004BBD60 (.h:64)
// Magic numbers: density 0.01, atmosphere scale 1, white.
inline rl_fog_state::rl_fog_state()
    : m_heigt_density(0.0f)
    , m_atmosphere_scale(1.0f)
    , m_density(0.01f)
    , m_density_offset(0.0f)
    , m_color(1.0f, 1.0f, 1.0f, 1.0f)
{
}

// 0x004BBDA0 (.h:68)
inline rl_fog_state::~rl_fog_state()
{
}

// 0x007C0240 (.h:79)
inline void rl_fog_state::set_height_density(float height_density)
{
    m_heigt_density = height_density;
}

// 0x004BBDB0 (.h:74)
inline float rl_fog_state::get_height_density() const
{
    return m_heigt_density;
}

// 0x007C0250 (.h:90)
inline void rl_fog_state::set_atmosphere_scale(float atmosphere_scale)
{
    m_atmosphere_scale = atmosphere_scale;
}

// 0x004BBDC0 (.h:85)
inline float rl_fog_state::get_atmosphere_scale() const
{
    return m_atmosphere_scale;
}

// 0x004BBDD0 (.h:96)
inline float rl_fog_state::get_density() const
{
    return m_density;
}

// 0x007C0260 (.h:101)
inline void rl_fog_state::set_density(float density)
{
    m_density = density;
}

// 0x004BBDE0 (.h:107)
inline float rl_fog_state::get_density_offset() const
{
    return m_density_offset;
}

// 0x007C0270 (.h:112)
inline void rl_fog_state::set_density_offset(float density_offset)
{
    m_density_offset = density_offset;
}

// 0x004BBDF0 (.h:118)
inline const rl_color_float& rl_fog_state::get_color() const
{
    return m_color;
}

// 0x004BBE00 (.h:123)
inline void rl_fog_state::set_color(const rl_color_float& _color)
{
    m_color = _color;
}
