// Original: volition/render_lib/code/rl_shadow_system_helpers.h
//
// rl_point_set, point sets used to fit shadow cascades.
#pragma once

#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector3.h"
#include "keen/base/trace.hpp"

class matrix;

// size 0xC1C
class rl_point_set
{
public:
    rl_point_set(); // 0x004DFC00 (.h:34)
    void push_back(const vector& v); // 0x004BAC10 (.h:40)
    int size() const; // 0x004BAC70 (.h:50)
    void clear(); // 0x004BAC80 (.h:53)
    void transform(const vector& pos, const matrix& orient); // 0x004DFC80 (.h:60)
    vector* begin(); // 0x004BAD20 (.h:72)
    const vector* begin() const; // 0x004BAD00 (.h:69)
    vector* end(); // 0x004BAD30 (.h:73)
    const vector* end() const; // 0x004BAD10 (.h:70)
    const bbox& bounds() const; // 0x004BAD40 (.h:75)
    const vector& operator[](int i) const; // 0x004BAD50 (.h:78)

private:
    static const int m_max_points; // 0x012BECB8
    vector m_points[256]; // 0x00
    int m_num_points; // 0xC00
    bbox m_bounds; // 0xC04
};

// ---- inline members

// 0x004DFC00 (.h:34)
// Empty, with inverted bounds (3.4028235e+38 = FLT_MAX).
inline rl_point_set::rl_point_set()
{
    m_bounds.min = vector(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f);
    m_bounds.max = vector(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
    m_num_points = 0;
}

// 0x004BAC10 (.h:40)
// Appends the point and grows the bounds; a full set (256 points) only prints the assert.
inline void rl_point_set::push_back(const vector& v)
{
    // Magic number: 256, the size of m_points (m_max_points).
    if (m_num_points < 256)
    {
        m_points[m_num_points] = v;
        m_num_points++;
        bbox_expand(m_bounds, v);
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
}

// 0x004BAC70 (.h:50)
inline int rl_point_set::size() const
{
    return m_num_points;
}

// 0x004BAD20 (.h:72)
inline vector* rl_point_set::begin()
{
    return m_points;
}

// 0x004BAD00 (.h:69)
inline const vector* rl_point_set::begin() const
{
    return m_points;
}

// 0x004BAD30 (.h:73)
inline vector* rl_point_set::end()
{
    return m_points + m_num_points;
}

// 0x004BAD10 (.h:70)
inline const vector* rl_point_set::end() const
{
    return m_points + m_num_points;
}

// 0x004BAD40 (.h:75)
inline const bbox& rl_point_set::bounds() const
{
    return m_bounds;
}
