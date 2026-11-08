// Original: volition/render_lib/code/rl_sphere.h
// Implementation: volition/render_lib/code/rl_sphere.cpp
//
// rl_sphere and rl_fp_sphere, bounding spheres.
#pragma once

#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector3.h"

#include <math.h>

class rl_aabb;

// size 0x10
class rl_sphere
{
public:
    rl_sphere(const rl_sphere& __that); // 0x004B8710
    rl_sphere(const vector& position, float radius); // 0x004B7FD0 (.h:43)
    rl_sphere(); // 0x004B7FC0 (.h:37)
    ~rl_sphere(); // 0x004B8000 (.h:47)
    void compute_from_aabb(const rl_aabb& aabb); // 0x00741E20 (.h:51)
    void merge(const rl_sphere&);

    vector m_position; // 0x00
    float m_radius; // 0x0C
};

// 0x004B8710
inline rl_sphere::rl_sphere(const rl_sphere& __that)
    : m_position(__that.m_position)
    , m_radius(__that.m_radius)
{
}

// 0x004B7FD0 (.h:43)
inline rl_sphere::rl_sphere(const vector& position, float radius)
    : m_position(position)
    , m_radius(radius)
{
}

// 0x004B7FC0 (.h:37)
// Uninitialized.
inline rl_sphere::rl_sphere()
{
}

// 0x004B8000 (.h:47)
inline rl_sphere::~rl_sphere()
{
}

// size 0x8
class rl_fp_sphere
{
public:
    rl_fp_sphere(short, short, short, short);
    rl_fp_sphere(); // 0x004B8010 (.h:63)
    void set(const rl_fp_sphere&);
    void set(const rl_sphere& sphere); // 0x004DCD00 (.h:82)
    void get(rl_sphere&);
    void set_pos(const vector& pos); // 0x004B8020 (.h:115)
    void set_radius(float _radius); // 0x004B80A0 (.h:123)
    void get_pos(vector&);
    float get_radius();

    short pt_x; // 0x00
    short pt_y; // 0x02
    short pt_z; // 0x04
    short radius; // 0x06
};

// ---- inline members (no .cpp home)

// 0x00741E20 (.h:51)
// The sphere around the box: its centre, half the diagonal.
inline void rl_sphere::compute_from_aabb(const rl_aabb& aabb)
{
    const bbox& box = (const bbox&)aabb;
    m_position.x = (box.min.x + box.max.x) * 0.5f;
    m_position.y = (box.max.y + box.min.y) * 0.5f;
    m_position.z = (box.max.z + box.min.z) * 0.5f;
    const float dx = box.max.x - box.min.x;
    const float dy = box.max.y - box.min.y;
    const float dz = box.max.z - box.min.z;
    m_radius = sqrtf(dx * dx + dy * dy + dz * dz) * 0.5f;
}

// 0x004B8010 (.h:63)
// Uninitialized.
inline rl_fp_sphere::rl_fp_sphere()
{
}

// 0x004B8020 (.h:115)
// Magic number: 16, the 12.4 fixed point scale.
inline void rl_fp_sphere::set_pos(const vector& pos)
{
    pt_x = (short)fl_round(pos.x * 16.0f);
    pt_y = (short)fl_round(pos.y * 16.0f);
    pt_z = (short)fl_round(pos.z * 16.0f);
}

// 0x004B80A0 (.h:123)
// Rounded up.
inline void rl_fp_sphere::set_radius(float _radius)
{
    radius = (short)(int)ceil((double)(_radius * 16.0f));
}

// 0x004DCD00 (.h:82)
// The centre is rounded to the nearest fixed point position; the radius grows by the rounding error and is rounded up,
// so the fixed point sphere contains the float sphere.
inline void rl_fp_sphere::set(const rl_sphere& sphere)
{
    pt_x = (short)fl_round(sphere.m_position.x * 16.0f);
    pt_y = (short)fl_round(sphere.m_position.y * 16.0f);
    pt_z = (short)fl_round(sphere.m_position.z * 16.0f);
    const float dx = (float)pt_x * 0.0625f - sphere.m_position.x;
    const float dy = (float)pt_y * 0.0625f - sphere.m_position.y;
    const float dz = (float)pt_z * 0.0625f - sphere.m_position.z;
    radius = (short)(int)ceil((double)((sqrtf(dx * dx + dy * dy + dz * dz) + sphere.m_radius) * 16.0f));
}
