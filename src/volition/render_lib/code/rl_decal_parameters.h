// Original: volition/render_lib/code/rl_decal_parameters.h
//
// rl_decal_parameters, how to create a decal.
#pragma once

#include "volition/render_lib/code/rl_clone_mesh_decal.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_decal_defines.h"
#include "volition/render_lib/code/rl_planar_decal.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

// size 0x80
class rl_decal_parameters
{
public:
    rl_decal_parameters(const rl_decal_parameters& __that); // 0x00743A10
    rl_decal_parameters(); // 0x004BCBD0 (.h:33)

    rl_decal_type m_type; // 0x00
    vector m_pos; // 0x04
    matrix m_transform; // 0x10
    bool m_permanent; // 0x34
    bool m_double_sided; // 0x35
    unsigned int m_filter; // 0x38
    void* m_filter_data; // 0x3C
    unsigned int (*m_filter_cb)(void*, unsigned int); // 0x40
    rl_color_float m_color; // 0x44
    int m_fade_time; // 0x54
    int m_lifetime; // 0x58
    union
    {
        rl_planar_decal_parameters m_planar_params; // 0x5C
        rl_clone_mesh_decal_parameters m_clone_mesh_params; // 0x5C
    };
};

// ---- inline members (VFX.md FX7)

// 0x00743A10
// Compiler-generated: memberwise (the union is copied whole).
inline rl_decal_parameters::rl_decal_parameters(const rl_decal_parameters& __that) = default;

// 0x004BCBD0 (.h:33)
// No type, at the origin with the identity transform, white, no fade, an infinite lifetime, not permanent, single
// sided, no filter.
inline rl_decal_parameters::rl_decal_parameters()
{
    m_pos = ZERO_VECTOR;
    m_transform = IDENTITY_MATRIX;
    m_color.red = 1.0f;
    m_color.green = 1.0f;
    m_filter = 0xFFFFFFFF;
    m_filter_data = nullptr;
    m_filter_cb = nullptr;
    m_type = RLDT_INVALID;
    m_color.blue = 1.0f;
    m_color.alpha = 1.0f;
    m_fade_time = 0;
    m_lifetime = -1;
    m_permanent = false;
    m_double_sided = false;
}
