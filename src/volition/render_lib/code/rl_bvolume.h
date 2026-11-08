// Original: volition/render_lib/code/rl_bvolume.h
// Implementation: volition/render_lib/code/rl_bvolume.cpp
//
// rl_bvolume, a bounding volume (sphere plus box half extents) and rl_bounding_cone. Placement of rl_bounding_cone
// inferred. rl_bvolume is 16-byte aligned (rl_scene_entity::m_world_bvolume at 0x50 after members ending at 0x4C;
// tools/hdrgen/aligncheck.py); alignas(16) is inferred.
#pragma once

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class rl_aabb;

// size 0x31C
class rl_bbox_set
{
public:
    rl_bbox_set();
    void push_back(const bbox&);
    void push_back(const vector&, const vector&);
    int size();
    void clear();
    bbox* begin();
    const bbox* begin() const;
    bbox* end();
    const bbox* end() const;
    const bbox& bounds() const;

private:
    bbox m_boxes[32]; // 0x00
    int m_num_boxes; // 0x300
    bbox m_bounds; // 0x304
};

// size 0x20
class alignas(16) rl_bvolume
{
public:
    rl_bvolume(const rl_sphere&);
    rl_bvolume(const rl_aabb&);
    rl_bvolume(); // 0x004B80F0 (.h:46)
    ~rl_bvolume(); // 0x004B83E0 (.h:101)
    void get_sphere(rl_sphere& sphere) const; // 0x004B8100 (.h:60)
    const rl_sphere& get_sphere() const; // 0x004B8200 (.h:72)
    void get_aabb(rl_aabb& aabb) const; // 0x004B8120 (.h:66)
    rl_aabb get_aabb() const; // 0x00508DA0 (.h:80)
    void set(const rl_sphere& sphere); // 0x004B83A0 (.h:94)
    void set(const rl_aabb& aabb); // 0x004B8240 (.h:87)

    rl_sphere m_sphere; // 0x00
    vector m_half_vec; // 0x10
    float m_pad; // 0x1C
};

// ---- inline members (no .cpp home)

// 0x004B80F0 (.h:46)
// Uninitialized.
inline rl_bvolume::rl_bvolume()
{
}

// 0x004B83E0 (.h:101)
inline rl_bvolume::~rl_bvolume()
{
}

// 0x004B8100 (.h:60)
inline void rl_bvolume::get_sphere(rl_sphere& sphere) const
{
    sphere.m_position = m_sphere.m_position;
    sphere.m_radius = m_sphere.m_radius;
}

// 0x004B8200 (.h:72)
inline const rl_sphere& rl_bvolume::get_sphere() const
{
    return m_sphere;
}

// 0x004B8120 (.h:66)
// The box around the sphere centre with the half extents.
inline void rl_bvolume::get_aabb(rl_aabb& aabb) const
{
    bbox& box = (bbox&)aabb;
    box.min.x = m_sphere.m_position.x - m_half_vec.x;
    box.min.y = m_sphere.m_position.y - m_half_vec.y;
    box.min.z = m_sphere.m_position.z - m_half_vec.z;
    box.max.x = m_sphere.m_position.x + m_half_vec.x;
    box.max.y = m_sphere.m_position.y + m_half_vec.y;
    box.max.z = m_sphere.m_position.z + m_half_vec.z;
}

// 0x00508DA0 (.h:80)
inline rl_aabb rl_bvolume::get_aabb() const
{
    rl_aabb aabb;
    get_aabb(aabb);
    return aabb;
}

// 0x004B8240 (.h:87)
// The box centre and half extents; the sphere radius is the half diagonal.
inline void rl_bvolume::set(const rl_aabb& aabb)
{
    const bbox& box = (const bbox&)aabb;
    m_half_vec.x = (box.max.x - box.min.x) * 0.5f;
    m_half_vec.y = (box.max.y - box.min.y) * 0.5f;
    m_half_vec.z = (box.max.z - box.min.z) * 0.5f;
    m_sphere.m_position.x = m_half_vec.x + box.min.x;
    m_sphere.m_position.y = box.min.y + m_half_vec.y;
    m_sphere.m_position.z = box.min.z + m_half_vec.z;
    // The squares are summed in double and rounded to float before the square root (0x004B8240).
    const double radius_sq = ((double)m_half_vec.y * (double)m_half_vec.y + (double)m_half_vec.x * (double)m_half_vec.x) + (double)m_half_vec.z * (double)m_half_vec.z;
    m_sphere.m_radius = (float)sqrt((double)(float)radius_sq);
}

// 0x004B83A0 (.h:94)
// The half extents become the radius on all axes.
inline void rl_bvolume::set(const rl_sphere& sphere)
{
    m_sphere.m_position = sphere.m_position;
    m_sphere.m_radius = sphere.m_radius;
    m_half_vec.x = sphere.m_radius;
    m_half_vec.y = sphere.m_radius;
    m_half_vec.z = sphere.m_radius;
}

// size 0x5C
struct rl_bounding_cone
{
    rl_bounding_cone(); // 0x004C8AD0

    vector apex; // 0x00
    matrix44 look_at; // 0x0C
    float max_x; // 0x4C
    float max_y; // 0x50
    float near_clip; // 0x54
    float far_clip; // 0x58
};
