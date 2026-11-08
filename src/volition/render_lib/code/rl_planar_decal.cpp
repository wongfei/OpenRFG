// Original: volition/render_lib/code/rl_planar_decal.cpp
//
// rl_planar_decal: a decal projected onto its owner's surface along a box (width x length x depth around the decal's
// position, relative to the owner). VFX.md FX7: ported whole. The owner's mesh is drawn again with the decal's
// material; the decal shader projects the box onto it (rl_planar_decal_setup_decal_for_render and
// _instance_for_render fill the decal vertex constants). Non-permanent decals join a reclaim list, oldest first: when
// the pool is empty, the oldest is taken off its owner and reused.

#include "volition/render_lib/code/rl_planar_decal.h"

#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_renderer.h"
#include "volition/vlib/code/math/vector4.h"

// ---- file-static globals
static rl_planar_decal* Reclaim_list; // 0x01B7BCF0

// :44 (class id 0x21436587, 256 objects: rfg.exe .data 0x01514FB0)
// <rl_planar_decal::factory_init> 0x00513FD0 (:44)
// <rl_planar_decal::release> 0x00514100 (:44)
// <rl_planar_decal::create> 0x0052C610 (:44)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_planar_decal, 0x21436587, 256)

// 0x004F07B0 (:47)
rl_planar_decal::rl_planar_decal()
{
    m_added_to_reclaim_list = false;
    m_decal_type = RLDT_PLANAR;
}

// 0x004C44B0 (:52)
// Off the reclaim list.
rl_planar_decal::~rl_planar_decal()
{
    if (m_added_to_reclaim_list)
    {
        if (Reclaim_list == this && (Reclaim_list = reclaim_next, Reclaim_list == this))
        {
            Reclaim_list = nullptr;
        }
        else
        {
            reclaim_prev->reclaim_next = reclaim_next;
            reclaim_next->reclaim_prev = reclaim_prev;
        }
        reclaim_prev = nullptr;
        reclaim_next = nullptr;
        m_added_to_reclaim_list = false;
    }
}

// 0x004F07F0 (:60)
// rl_decal::init (inlined) and the box, fade ranges and material of the planar parameters; the decal's position and
// orientation relative to the parent (the offset rotated into the parent's frame).
void rl_planar_decal::init(const rl_decal_parameters* params, const vector& parent_pos, const matrix& parent_orient)
{
    const rl_planar_decal_parameters& planar = params->m_planar_params;
    const float half_width = -planar.m_width * 0.5f;
    const float half_length = -planar.m_length * 0.5f;
    const float half_depth = -planar.m_depth * 0.5f;
    m_local_aabb.min.x = half_width;
    m_local_aabb.min.y = half_length;
    m_local_aabb.min.z = half_depth;
    m_local_aabb.max.x = half_width * -1.0f;
    m_local_aabb.max.y = half_length * -1.0f;
    m_local_aabb.max.z = half_depth * -1.0f;
    rl_decal::init(params);
    m_width = planar.m_width;
    m_length = planar.m_length;
    m_depth = planar.m_depth;
    m_min_fade_angle = planar.m_min_fade_angle;
    m_max_fade_angle = planar.m_max_fade_angle;
    m_min_fade_depth = planar.m_min_fade_depth;
    m_max_fade_depth = planar.m_max_fade_depth;
    m_material = planar.m_material_handle;
    m_relative_pos.set(params->m_pos.x - parent_pos.x, params->m_pos.y - parent_pos.y, params->m_pos.z - parent_pos.z);
    m_relative_orient = params->m_transform * parent_orient.copy_transpose();
    m_relative_pos.rotate(parent_orient);
}

// 0x004BCB60 (.h:40)
float rl_planar_decal::get_width() const
{
    return m_width;
}

// 0x004BCB70 (.h:41)
float rl_planar_decal::get_length() const
{
    return m_length;
}

// 0x004BCB80 (.h:42)
float rl_planar_decal::get_depth() const
{
    return m_depth;
}

// 0x004BCB90 (.h:43)
float rl_planar_decal::get_min_fade_angle() const
{
    return m_min_fade_angle;
}

// 0x004BCBA0 (.h:44)
float rl_planar_decal::get_max_fade_angle() const
{
    return m_max_fade_angle;
}

// 0x004BCBB0 (.h:45)
float rl_planar_decal::get_min_fade_depth() const
{
    return m_min_fade_depth;
}

// 0x004BCBC0 (.h:46)
float rl_planar_decal::get_max_fade_depth() const
{
    return m_max_fade_depth;
}

// 0x004C4520 (.h:48)
unsigned int rl_planar_decal::get_material() const
{
    return m_material;
}

// 0x0052C680 (:83)
// A planar decal from the pool (reclaiming the oldest non-permanent one when it is empty), initialized; a
// non-permanent decal goes to the end of the reclaim list.
rl_decal* rl_planar_decal_create(const rl_decal_parameters* params, const vector& parent_pos, const matrix& parent_orient)
{
    if (params->m_type != RLDT_PLANAR)
    {
        return nullptr;
    }
    if (!rl_planar_decal::can_create() && Reclaim_list != nullptr)
    {
        rl_planar_decal* oldest = Reclaim_list;
        if (oldest->get_owner() != nullptr)
        {
            oldest->get_owner()->remove_decal(oldest);
        }
        rl_decal_unregister(oldest);
        oldest->release();
    }
    rl_planar_decal* decal_p = rl_planar_decal::create();
    if (decal_p == nullptr)
    {
        return nullptr;
    }
    decal_p->init(params, parent_pos, parent_orient);
    if (!params->m_permanent)
    {
        if (Reclaim_list == nullptr)
        {
            decal_p->reclaim_next = decal_p;
            decal_p->reclaim_prev = decal_p;
            Reclaim_list = decal_p;
        }
        else
        {
            decal_p->reclaim_prev = Reclaim_list->reclaim_prev;
            decal_p->reclaim_next = Reclaim_list;
            Reclaim_list->reclaim_prev->reclaim_next = decal_p;
            Reclaim_list->reclaim_prev = decal_p;
        }
        decal_p->m_added_to_reclaim_list = true;
    }
    return decal_p;
}

// 0x004F0A10 (:120)
// The decal constants: the tint (color by alpha times fade, alpha squared by the fade), the box's inverse half extents
// (0.5 / (size / 2)) with the age in ms, and the angle and depth fade ramps (scale, offset).
void rl_planar_decal_setup_decal_for_render(rl_renderer& renderer, rl_planar_decal* decal_p)
{
    const rl_color_float& color = decal_p->get_color();
    const float alpha = decal_p->get_fade_pct() * color.alpha;
    const float age_ms = decal_p->get_age_ms();
    const float angle_range = decal_p->get_max_fade_angle() - decal_p->get_min_fade_angle();
    const float depth_range = decal_p->get_min_fade_depth() - decal_p->get_max_fade_depth();
    const vector4 tint(color.red * alpha, color.green * alpha, color.blue * alpha, color.alpha * alpha);
    // Magic numbers: 0.5 / (size * 0.5) maps the box onto [-0.5, 0.5] texture space.
    const vector4 params1(0.5f / (decal_p->get_width() * 0.5f), 0.5f / (decal_p->get_length() * 0.5f), 0.5f / (decal_p->get_depth() * 0.5f), age_ms);
    const vector4 params2(1.0f / angle_range, -decal_p->get_min_fade_angle() / angle_range, 1.0f / depth_range, -decal_p->get_max_fade_depth() / depth_range);
    // rl_renderer::set_decal_params, inlined.
    renderer.set_decal_params(tint, params1, params2);
}

// 0x004F0C60 (:152)
// The decal's position and axes relative to its owner (rl_renderer::set_decal_transform, inlined).
void rl_planar_decal_setup_decal_instance_for_render(rl_renderer& renderer, rl_planar_decal* decal_p)
{
    renderer.set_decal_transform(decal_p->m_relative_pos, decal_p->m_relative_orient);
}
