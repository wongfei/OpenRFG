// Original: volition/render_lib/code/rl_occluder.cpp
//
// rl_occluder: the base of the occluder volumes (rl_terrain_occluder). An occluder adds itself to its scene's occluder
// list and keeps its index there as its handle. The header-inline members of rl_occluder.h are defined here as well.

#include "volition/render_lib/code/rl_occluder.h"

#include "volition/render_lib/code/rl_scene.h"
#include "volition/vlib/code/math/bbox.h"

// 0x00541A00 (:8)
// rl_scene::add_occluder (inlined).
rl_occluder::rl_occluder()
{
    m_occluded = false;
    set_category_id(3);
    m_handle = get_scene()->add_occluder(this);
}

// 0x00541A80 (:17)
// rl_scene::remove_occluder (inlined).
rl_occluder::~rl_occluder()
{
    if (get_scene() != nullptr)
    {
        get_scene()->remove_occluder(this);
    }
    m_handle.invalidate();
}

// 0x00513760 (:28)
// The squared distance from the viewer to the world box (the front to back sort key).
void rl_occluder::update(const vector& viewer_position)
{
    rl_aabb box;
    get_world_aabb(box);
    set_camera_distance_squared(bbox_find_distance_squared(box.min, box.max, viewer_position));
}

// 0x00513870 (:36)
// Stripped drawing: only the debug color and the bounding volume update are left.
void rl_occluder::debug_render() const
{
    get_world_bvolume();
}

// 0x004BBD20 (.h:68)
bool rl_occluder::get_occluded() const
{
    return m_occluded;
}

// 0x004BBD30 (.h:73)
void rl_occluder::set_occluded(bool occluded)
{
    m_occluded = occluded;
}

// 0x004BBD40 (.h:78)
const rl_handle& rl_occluder::get_handle() const
{
    return m_handle;
}

// 0x004BBD50 (.h:83)
void rl_occluder::set_handle(const rl_handle& handle_inst)
{
    m_handle = handle_inst;
}
