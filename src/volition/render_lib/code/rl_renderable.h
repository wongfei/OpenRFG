// Original: volition/render_lib/code/rl_renderable.h
// Implementation: volition/render_lib/code/rl_renderable.cpp
//
// rl_renderable, something that can be drawn (meshes, particles): a named object with a local bounding volume, linked
// in a list.
#pragma once

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_bvolume.h"
#include "volition/render_lib/code/rl_named_object.h"

#include <math.h>

class rl_aabb;
class rl_sphere;

// size 0x30
class rl_renderable : public rl_named_object
{
protected:
    rl_renderable(); // 0x004DF6F0 (.h:72)
    virtual ~rl_renderable(); // vtbl+0x00 0x004BA310 (.h:80)
public:
    rl_renderable* get_prev() const;
    void set_prev(rl_renderable*);
    rl_renderable* get_next() const;
    void set_next(rl_renderable*);
    void set_local_aabb(const rl_aabb& local_aabb); // 0x004BA320 (.h:84)
    void get_local_aabb(rl_aabb& aabb) const; // 0x00A05350 (.h:94)
    const rl_aabb get_local_aabb() const; // 0x0053E570 (.h:89)
    void set_local_bounding_sphere(const rl_sphere&);
    void get_local_bounding_sphere(rl_sphere&) const;
    const rl_sphere get_local_bounding_sphere() const;

    rl_renderable* prev; // 0x08
    rl_renderable* next; // 0x0C
protected:
    rl_bvolume m_local_bvolume; // 0x10
};

// ---- inline members

// 0x004DF6F0 (.h:72)
// A unit box around the origin (half extents 1, sphere center half_vec - 1).
// Notes (decompiled behaviour, kept as is): prev and next are left uninitialized.
inline rl_renderable::rl_renderable()
{
    m_local_bvolume.m_half_vec.x = 1.0f;
    m_local_bvolume.m_half_vec.y = 1.0f;
    m_local_bvolume.m_half_vec.z = 1.0f;
    m_local_bvolume.m_sphere.m_position.x = m_local_bvolume.m_half_vec.x - 1.0f;
    m_local_bvolume.m_sphere.m_position.y = m_local_bvolume.m_half_vec.y - 1.0f;
    m_local_bvolume.m_sphere.m_position.z = m_local_bvolume.m_half_vec.z - 1.0f;
    m_local_bvolume.m_sphere.m_radius = sqrtf(m_local_bvolume.m_half_vec.y * m_local_bvolume.m_half_vec.y + m_local_bvolume.m_half_vec.x * m_local_bvolume.m_half_vec.x + m_local_bvolume.m_half_vec.z * m_local_bvolume.m_half_vec.z);
}

// 0x004BA310 (.h:80)
inline rl_renderable::~rl_renderable()
{
}

// 0x004BA320 (.h:84)
// rl_bvolume::set (inlined): half extents, center and the sphere around the box.
inline void rl_renderable::set_local_aabb(const rl_aabb& local_aabb)
{
    m_local_bvolume.set(local_aabb);
}

// 0x00A05350 (.h:94)
// The local box around the bounding volume's center.
// 0x0053E570 (.h:89)
inline const rl_aabb rl_renderable::get_local_aabb() const
{
    rl_aabb aabb;
    get_local_aabb(aabb);
    return aabb;
}

inline void rl_renderable::get_local_aabb(rl_aabb& aabb) const
{
    aabb.min.x = m_local_bvolume.m_sphere.m_position.x - m_local_bvolume.m_half_vec.x;
    aabb.min.y = m_local_bvolume.m_sphere.m_position.y - m_local_bvolume.m_half_vec.y;
    aabb.min.z = m_local_bvolume.m_sphere.m_position.z - m_local_bvolume.m_half_vec.z;
    aabb.max.x = m_local_bvolume.m_sphere.m_position.x + m_local_bvolume.m_half_vec.x;
    aabb.max.y = m_local_bvolume.m_half_vec.y + m_local_bvolume.m_sphere.m_position.y;
    aabb.max.z = m_local_bvolume.m_half_vec.z + m_local_bvolume.m_sphere.m_position.z;
}
