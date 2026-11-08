// Original: volition/render_lib/code/rl_terrain_occluder.cpp
//
// rl_terrain_occluder: a vertical wall occluder from the terrain data (obj_zone creates one per terrain_occluder: two
// ground points, a base and a top height). update() builds a frustum behind the wall as seen from the viewer: the
// wall plane (facing away from the viewer) and the four planes through the viewer and the wall's edges, plus an
// optional bottom plane. An entity or sector completely inside that frustum is hidden.

#include "volition/render_lib/code/rl_terrain_occluder.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_partition_sector.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <math.h>

// <rl_terrain_occluder::factory_init> 0x005196B0 (:12)
// <rl_terrain_occluder::release> 0x005197B0 (:12)
// <rl_terrain_occluder::create> 0x00546F40 (:12)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_terrain_occluder, 0x59564738, 300)

// The occluder's frustum also gets the bottom plane (off in retail).
bool Rl_terrain_occluder_use_bottom_plane = false; // 0x01B7BD36
// Debug switch for the occluder volumes (not read in retail).
bool Rl_terrain_show_volumes = false; // 0x01B7BD37
// An occluder is useful when its projected size, as is_useful measures it, reaches this.
float Rl_terrain_occluder_useful_threshold = 500.0f; // 0x0150A5E8

// 0x00546F00 (.h:117)
rl_terrain_occluder::rl_terrain_occluder()
{
    m_updated = false;
}

// 0x00546F30 (.h:122)
rl_terrain_occluder::~rl_terrain_occluder()
{
}

// 0x00519830 (:27)
// The wall plane and its points are computed once per change of the wall; the side planes every update, wound so that
// they face the space behind the wall from the viewer's side.
// Notes (decompiled behaviour, kept as is): m_abc5, the bottom plane's normal, is never computed; it is only used
// with Rl_terrain_occluder_use_bottom_plane on.
void rl_terrain_occluder::update(const vector& viewer_position)
{
    rl_occluder::update(viewer_position);
    const float height = m_height;
    const float height_base = m_height_base;
    m_view_position = viewer_position;
    if (!m_updated)
    {
        m_plane_pos.set((m_point2.x + m_point1.x) * 0.5f, (height + height_base) * 0.5f, (m_point2.y + m_point1.y) * 0.5f);
        const vector up(m_point2.x - m_point2.x, height_base - height, m_point2.y - m_point2.y);
        const vector along(m_point2.x - m_point1.x, height_base - height_base, m_point2.y - m_point1.y);
        m_occlude_plane_normal = up ^ along;
        m_occlude_plane_normal.normalize();
        m_occlude_plane.set(m_occlude_plane_normal, m_plane_pos);
        m_top_point1.set(m_point1.x, height, m_point1.y);
        m_top_point2.set(m_point2.x, height, m_point2.y);
    }
    const bool in_front = m_occlude_plane.normal.y * m_view_position.y + m_occlude_plane.normal.x * m_view_position.x + m_occlude_plane.normal.z * m_view_position.z + m_occlude_plane.offset > 0.0f;

    // The wall plane: from the second base point to the first top point, and along the base.
    const vector base2_to_top1(m_point2.x - m_point1.x, height_base - height, m_point2.y - m_point1.y);
    const vector base1_to_base2(m_point2.x - m_point1.x, height_base - height_base, m_point2.y - m_point1.y);
    m_abc1 = in_front ? base2_to_top1 ^ base1_to_base2 : base1_to_base2 ^ base2_to_top1;
    m_abc1.normalize();
    // The top plane: through the viewer and both top points.
    const vector top1_to_view(m_view_position.x - m_point1.x, m_view_position.y - height, m_view_position.z - m_point1.y);
    const vector top2_to_view(m_view_position.x - m_point2.x, m_view_position.y - height, m_view_position.z - m_point2.y);
    m_abc2 = in_front ? top2_to_view ^ top1_to_view : top1_to_view ^ top2_to_view;
    m_abc2.normalize();
    // The side planes: through the viewer and each wall edge.
    const vector base1_to_view(m_view_position.x - m_point1.x, m_view_position.y - height_base, m_view_position.z - m_point1.y);
    m_abc3 = in_front ? top1_to_view ^ base1_to_view : base1_to_view ^ top1_to_view;
    m_abc3.normalize();
    const vector base2_to_view(m_view_position.x - m_point2.x, m_view_position.y - height_base, m_view_position.z - m_point2.y);
    m_abc4 = in_front ? base2_to_view ^ top2_to_view : top2_to_view ^ base2_to_view;
    m_abc4.normalize();

    plane planes[5];
    planes[0].set(m_abc1, m_plane_pos);
    planes[1].set(m_abc2, m_view_position);
    planes[2].set(m_abc3, m_view_position);
    planes[3].set(m_abc4, m_view_position);
    planes[4].set(m_abc5, m_view_position);
    for (int i = 0; i < 5; i++)
    {
        m_frustum.set_plane(i, planes[i]);
    }
    m_frustum.set_num_active_planes(Rl_terrain_occluder_use_bottom_plane ? 5 : 4);
    m_frustum.update();
    m_updated = true;
}

// 0x004D3DD0 (:159)
void rl_terrain_occluder::debug_render() const
{
}

// 0x0051A480 (:233)
bool rl_terrain_occluder::is_occluded(const rl_scene_entity* entity_p) const
{
    return m_frustum.is_completely_inside(entity_p->get_world_bvolume());
}

// 0x0051A4B0 (:249)
bool rl_terrain_occluder::is_occluded(const rl_partition_sector* partition_sector_p) const
{
    return m_frustum.is_completely_inside(partition_sector_p->get_world_bvolume());
}

// 0x004D3DE0 (:263)
// Notes (decompiled behaviour, kept as is): the other occluder is taken to be a terrain occluder (its top points).
bool rl_terrain_occluder::is_occluded(const rl_occluder* occluder_p) const
{
    const rl_terrain_occluder* other_p = (const rl_terrain_occluder*)occluder_p;
    return m_frustum.is_completely_inside(other_p->m_top_point1) && m_frustum.is_completely_inside(other_p->m_top_point2);
}

// 0x004D3E20 (:281)
bool rl_terrain_occluder::is_occluded(const rl_aabb& aabb) const
{
    return m_frustum.is_completely_inside(aabb);
}

// 0x004D3E40 (:290)
// The wall's normal, scaled by the length of top1 + top2 times the wall height, projected on the direction to the
// viewer and divided by that direction's squared length.
// Notes (decompiled behaviour, kept as is): the scale is the length of the sum of the top points (not their
// distance), and the direction is normalized before its squared length is taken.
bool rl_terrain_occluder::is_useful(const rl_3d_entity* viewer_p) const
{
    vector to_wall((m_top_point1.x + m_top_point2.x) * 0.5f - viewer_p->get_position().x, (m_top_point2.y + m_top_point1.y) * 0.5f - viewer_p->get_position().y, (m_top_point2.z + m_top_point1.z) * 0.5f - viewer_p->get_position().z);
    to_wall.normalize();
    const float sum_x = m_top_point1.x + m_top_point2.x;
    const float sum_y = m_top_point2.y + m_top_point1.y;
    const float sum_z = m_top_point2.z + m_top_point1.z;
    const float scale = sqrtf(sum_x * sum_x + sum_y * sum_y + sum_z * sum_z) * (m_height - m_height_base);
    const float projected = (m_occlude_plane_normal.y * scale * to_wall.y + scale * m_occlude_plane_normal.x * to_wall.x + m_occlude_plane_normal.z * scale * to_wall.z) / (to_wall.x * to_wall.x + to_wall.y * to_wall.y + to_wall.z * to_wall.z);
    return fabsf(projected) >= Rl_terrain_occluder_useful_threshold;
}

// 0x004D40F0 (:316)
// Notes (decompiled behaviour, kept as is): the merge is not finished; the closest points on the two side planes and
// the wall direction are computed and dropped, so the occluder is unchanged.
void rl_terrain_occluder::merge(rl_occluder* occluder_p) const
{
    const rl_terrain_occluder* other_p = (const rl_terrain_occluder*)occluder_p;
    const plane side2 = m_frustum.get_plane(2);
    const plane side3 = m_frustum.get_plane(3);
    const vector* point_p = &other_p->m_top_point1;
    if (m_frustum.is_completely_inside(*point_p))
    {
        point_p = &other_p->m_top_point2;
    }
    side2.find_closest_point(*point_p);
    side3.find_closest_point(*point_p);
    vector direction = m_top_point2 - m_top_point1;
    direction.normalize();
}

// 0x00547020 (:370)
// A new occluder over the same wall, with this occluder's world box and transform, merged with occluder_p.
// Notes (decompiled behaviour, kept as is): the result of create() is not checked.
rl_occluder* rl_terrain_occluder::create_merged(rl_occluder* occluder_p) const
{
    rl_terrain_occluder* merged_p = create();
    merged_p->set_height_base(m_height_base);
    merged_p->set_height(m_height);
    merged_p->set_point1(m_point1);
    merged_p->set_point2(m_point2);
    merged_p->set_world_aabb(get_world_aabb());
    merged_p->set_position(get_position());
    merged_p->set_rotation(get_rotation());
    merged_p->set_bounding_volume_dirty(false);
    merged_p->update_partition_system();
    merged_p->merge(occluder_p);
    return merged_p;
}

// 0x0051A4D0 (:390)
bool rl_terrain_occluder::is_partially_occluded(rl_scene_entity* entity_p) const
{
    return !m_frustum.is_occluded(entity_p->get_world_bvolume());
}

// 0x0051A500 (:406)
bool rl_terrain_occluder::is_partially_occluded(rl_partition_sector* partition_sector_p) const
{
    return !m_frustum.is_occluded(partition_sector_p->get_world_bvolume());
}

// 0x004D4240 (:422)
// One top point of the other (terrain) occluder inside the frustum and the other outside.
bool rl_terrain_occluder::is_partially_occluded(rl_occluder* occluder_p) const
{
    const rl_terrain_occluder* other_p = (const rl_terrain_occluder*)occluder_p;
    if (m_frustum.is_completely_inside(other_p->m_top_point1) && !m_frustum.is_completely_inside(other_p->m_top_point2))
    {
        return true;
    }
    if (m_frustum.is_completely_inside(other_p->m_top_point2) && !m_frustum.is_completely_inside(other_p->m_top_point1))
    {
        return true;
    }
    return false;
}

// 0x004D3CD0 (.h:37) and 0x004D3CE0 (.h:37) are the factory macro's get_object_pool and get_objects_max.

// 0x004D3CF0 (.h:128)
const vector2& rl_terrain_occluder::get_point1() const
{
    return m_point1;
}

// 0x004D3D00 (.h:133)
void rl_terrain_occluder::set_point1(const vector2& point1)
{
    m_point1 = point1;
    m_updated = false;
}

// 0x004D3D20 (.h:140)
const vector2& rl_terrain_occluder::get_point2() const
{
    return m_point2;
}

// 0x004D3D30 (.h:145)
void rl_terrain_occluder::set_point2(const vector2& point2)
{
    m_point2 = point2;
    m_updated = false;
}

// 0x004D3D50 (.h:152)
float rl_terrain_occluder::get_height_base() const
{
    return m_height_base;
}

// 0x004D3D60 (.h:157)
void rl_terrain_occluder::set_height_base(float height_base)
{
    m_height_base = height_base;
    m_updated = false;
}

// 0x004D3D80 (.h:164)
float rl_terrain_occluder::get_height() const
{
    return m_height;
}

// 0x004D3D90 (.h:169)
void rl_terrain_occluder::set_height(float height)
{
    m_height = height;
    m_updated = false;
}

// 0x004D3DB0 (.h:176)
const vector& rl_terrain_occluder::get_top_point1() const
{
    return m_top_point1;
}

// 0x004D3DC0 (.h:182)
const vector& rl_terrain_occluder::get_top_point2() const
{
    return m_top_point2;
}
