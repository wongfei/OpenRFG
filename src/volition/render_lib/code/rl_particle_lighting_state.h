// Original: volition/render_lib/code/rl_particle_lighting_state.h
//
// rl_particle_lighting_state, lights affecting particles.
#pragma once

#include "volition/render_lib/code/rl_color.h"
#include "volition/vlib/code/math/vector3.h"

// size 0x2C
class rl_particle_lighting_state
{
public:
    rl_particle_lighting_state(); // 0x004C74A0

    rl_color_float m_ambient_color; // 0x00
    rl_color_float m_tod_color; // 0x10
    vector m_tod_light_dir; // 0x20
};

// 0x004C74A0 (compiler-generated)
inline rl_particle_lighting_state::rl_particle_lighting_state()
{
}
