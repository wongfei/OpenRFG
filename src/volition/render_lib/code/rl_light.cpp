// Original: volition/render_lib/code/rl_light.cpp
//
// rl_light: a point, spot or directional light in a scene (a factory class, 100 objects). A new light adds itself to
// the active scene (rl_scene::add_light). fillLighting*ShaderData write the lighting constant buffers (slot 1) that
// rl_renderer::set_light uploads. The header-inline members of rl_light.h are defined here as well.

#include "volition/render_lib/code/rl_light.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/render_lib/code/shader/lightingshaderdata.hpp"
#include "volition/vlib/code/memory/vmemory.h"

#include <math.h>

// <rl_light::factory_init> 0x00510B20 (:11)
// <rl_light::create> 0x0054BBE0 (:11)
RL_FACTORY_CLASS_CORE(rl_light, 0x2B7C11A4, 100)

// 0x0053E910 (:11)
// Deferred while render_lib is locked. Notes (decompiled behaviour, kept as is): unlike the factory macro's release,
// the object goes back to the pool without checking that it came from the factory mempool.
void rl_light::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_light();
}

// 0x0053E990 (:24)
// A white point light, added to its scene (the active scene, rl_scene_entity's constructor).
rl_light::rl_light()
{
    m_light_type = LIGHT_TYPE_POINT;
    m_color.set(1.0f, 1.0f, 1.0f, 1.0f);
    get_scene()->add_light(this);
    set_visibility_list_index(2);
    m_clip_mesh = nullptr;
    set_category_id(1);
}

// 0x0053EA10 (:33)
// Removes the light from its scene (rl_scene::remove_light, inlined).
rl_light::~rl_light()
{
    rl_scene* scene_p = get_scene();
    if (scene_p != nullptr)
    {
        scene_p->remove_light(this);
    }
}

// 0x004C23F0 (:43)
// Inserts into or removes from the light partition system and updates FLAGS_NOT_VISIBLE.
void rl_light::visible_update(bool is_visible)
{
    rl_partition_system* partition_system_p = get_partition_system();
    if (partition_system_p != nullptr)
    {
        if (!is_visible)
        {
            if (get_is_visible())
            {
                partition_system_p->remove(this);
            }
        }
        else if (!get_is_visible())
        {
            partition_system_p->insert(this);
        }
    }
    rl_scene_entity::set_is_visible(is_visible);
}

// 0x004C2460 (:121)
// The cosines of the hot spot and falloff angles and the reciprocal of their difference. Magic number: 1e-5 keeps the
// falloff cosine below the hot spot cosine.
keen::float4 createHotSpotInfo(float hotSpotSize, float hotSpotFallOffSize)
{
    const float cos_hotspot = cosf(hotSpotSize);
    float cos_falloff = cosf(hotSpotFallOffSize);
    if (cos_hotspot <= cos_falloff)
    {
        cos_falloff = (float)((double)cos_hotspot - 1e-05);
    }
    keen::float4 info;
    info.x = cos_hotspot;
    info.y = cos_falloff;
    info.z = (float)(1.0 / ((double)cos_hotspot - (double)cos_falloff));
    info.w = 1.0f;
    return info;
}

// 0x004C2510 (:134)
// Directional lights pass their direction; point and spot lights their position relative to the render offset, the
// spot direction and hot spot, and the linear attenuation (end / (end - start), 1 / (end - start)).
void rl_light::fillLightingVertexShaderData(keen::LightingVertexShaderConstants* pVertexConstants, const vector& renderOffset) const
{
    if (m_light_type != LIGHT_TYPE_DIRECTIONAL)
    {
        const vector& position = get_position();
        pVertexConstants->lightDirOrPos.x = position.x - renderOffset.x;
        pVertexConstants->lightDirOrPos.y = position.y - renderOffset.y;
        pVertexConstants->lightDirOrPos.z = position.z - renderOffset.z;
        pVertexConstants->__dummy0 = 0.0f;
        if (m_light_type == LIGHT_TYPE_SPOT_LIGHT)
        {
            const vector& fvec = get_rotation().fvec;
            pVertexConstants->spotLightDir.x = fvec.x;
            pVertexConstants->spotLightDir.y = fvec.y;
            pVertexConstants->spotLightDir.z = fvec.z;
            pVertexConstants->spotLightDir.w = 0.0f;
            pVertexConstants->hotspot = createHotSpotInfo(m_hotspot_size, m_hotspot_falloff_size);
        }
        else
        {
            pVertexConstants->spotLightDir.x = 0.0f;
            pVertexConstants->spotLightDir.y = 0.0f;
            pVertexConstants->spotLightDir.z = 0.0f;
            pVertexConstants->spotLightDir.w = 0.0f;
            pVertexConstants->hotspot.x = 0.0f;
            pVertexConstants->hotspot.y = 0.0f;
            pVertexConstants->hotspot.z = 0.0f;
            pVertexConstants->hotspot.w = 0.0f;
        }
        const float range = m_attenuation_end - m_attenuation_start;
        pVertexConstants->attenuation.x = m_attenuation_end / range;
        pVertexConstants->attenuation.y = 1.0f / range;
        pVertexConstants->attenuation.z = 0.0f;
        pVertexConstants->attenuation.w = 0.0f;
        return;
    }
    pVertexConstants->lightDirOrPos.x = m_light_direction.x;
    pVertexConstants->lightDirOrPos.y = m_light_direction.y;
    pVertexConstants->lightDirOrPos.z = m_light_direction.z;
    pVertexConstants->__dummy0 = 0.0f;
    pVertexConstants->spotLightDir.x = 0.0f;
    pVertexConstants->spotLightDir.y = 0.0f;
    pVertexConstants->spotLightDir.z = 0.0f;
    pVertexConstants->spotLightDir.w = 0.0f;
    pVertexConstants->attenuation.x = 0.0f;
    pVertexConstants->attenuation.y = 0.0f;
    pVertexConstants->attenuation.z = 0.0f;
    pVertexConstants->attenuation.w = 0.0f;
    pVertexConstants->hotspot.x = 0.0f;
    pVertexConstants->hotspot.y = 0.0f;
    pVertexConstants->hotspot.z = 0.0f;
    pVertexConstants->hotspot.w = 0.0f;
}

// 0x004C27E0 (:171)
// The color times colorScale, and for point and spot lights the attenuation and spot hot spot.
void rl_light::fillLightingFragmentShaderData(keen::LightingFragmentShaderConstants* pFragmentConstants, float colorScale) const
{
    pFragmentConstants->lightColor.x = colorScale * m_color.red;
    pFragmentConstants->lightColor.y = m_color.green * colorScale;
    pFragmentConstants->lightColor.z = m_color.blue * colorScale;
    pFragmentConstants->lightColor.w = m_color.alpha * colorScale;
    if (m_light_type == LIGHT_TYPE_DIRECTIONAL)
    {
        pFragmentConstants->attenuation.x = 0.0f;
        pFragmentConstants->attenuation.y = 0.0f;
        pFragmentConstants->attenuation.z = 0.0f;
        pFragmentConstants->attenuation.w = 0.0f;
    }
    else
    {
        const float range = m_attenuation_end - m_attenuation_start;
        pFragmentConstants->attenuation.x = m_attenuation_end / range;
        pFragmentConstants->attenuation.y = 1.0f / range;
        pFragmentConstants->attenuation.z = 0.0f;
        pFragmentConstants->attenuation.w = 0.0f;
        if (m_light_type == LIGHT_TYPE_SPOT_LIGHT)
        {
            pFragmentConstants->hotspot = createHotSpotInfo(m_hotspot_size, m_hotspot_falloff_size);
            return;
        }
    }
    pFragmentConstants->hotspot.x = 0.0f;
    pFragmentConstants->hotspot.y = 0.0f;
    pFragmentConstants->hotspot.z = 0.0f;
    pFragmentConstants->hotspot.w = 0.0f;
}

// 0x004BBB80 (.h:115)
rl_light::light_type rl_light::get_light_type() const
{
    return m_light_type;
}

// 0x004BBB90 (.h:120)
void rl_light::set_light_type(light_type light_type)
{
    m_light_type = light_type;
}

// 0x007C0220 (.h:181)
void rl_light::set_is_visible(bool is_visible)
{
    if (is_visible != get_is_visible())
    {
        visible_update(is_visible);
    }
}

const rl_color_float& rl_light::get_color() const
{
    return m_color;
}

// 0x004BBBA0 (.h:131)
void rl_light::set_color(const rl_color_float& color)
{
    m_color = color;
}

float rl_light::get_attenuation_start() const
{
    return m_attenuation_start;
}

// 0x004BBBC0 (.h:143)
void rl_light::set_attenuation_start(float attenuation_start)
{
    m_attenuation_start = attenuation_start;
}

// 0x004BBBE0 (.h:149)
float rl_light::get_attenuation_end() const
{
    return m_attenuation_end;
}

// 0x004BBBF0 (.h:154)
void rl_light::set_attenuation_end(float attenuation_end)
{
    m_attenuation_end = attenuation_end;
}

// 0x004BBC10 (.h:160)
float rl_light::get_hotspot_falloff_size() const
{
    return m_hotspot_falloff_size;
}

// 0x00A19970 (.h:165)
void rl_light::set_hotspot_falloff_size(float hotspot_falloff_size)
{
    m_hotspot_falloff_size = hotspot_falloff_size;
}

float rl_light::get_hotspot_size() const
{
    return m_hotspot_size;
}

// 0x00A19990 (.h:176)
void rl_light::set_hotspot_size(float hotspot_size)
{
    m_hotspot_size = hotspot_size;
}

// 0x004BBC20 (.h:191)
void rl_light::get_spotlight_fvec(vector& _fvec) const
{
    _fvec = get_rotation().fvec;
}

// 0x004BBCE0 (.h:253)
void rl_light::get_light_direction(vector& light_dir) const
{
    light_dir = m_light_direction;
}

// 0x004BBD00 (.h:258)
void rl_light::set_light_direction(const vector& light_dir)
{
    m_light_direction = light_dir;
}

// 0x004BBC40 (.h:196)
void rl_light::set_vis_pass_indices(unsigned int render_index, unsigned int shadow_index)
{
    m_vis_pass_index = (unsigned short)render_index;
    m_shadow_vis_pass_index = (unsigned short)shadow_index;
}

// 0x004BBC60 (.h:202)
// All passes (0xFFFFFFFF) without a shadow pass index.
unsigned int rl_light::get_shadow_vis_pass_flags() const
{
    if (m_shadow_vis_pass_index != 0xFFFF)
    {
        return 1u << (m_shadow_vis_pass_index & 0x1F);
    }
    return 0xFFFFFFFF;
}

// 0x004BBC80 (.h:212)
unsigned int rl_light::get_render_vis_pass_flags() const
{
    if (m_vis_pass_index != 0xFFFF)
    {
        return 1u << (m_vis_pass_index & 0x1F);
    }
    return 0xFFFFFFFF;
}

const vector& rl_light::get_clip_min() const
{
    return m_clip_min;
}

// 0x004BBCA0 (.h:228)
void rl_light::set_clip_min(const vector& clip_min)
{
    m_clip_min = clip_min;
}

const vector& rl_light::get_clip_max() const
{
    return m_clip_max;
}

// 0x004BBCC0 (.h:238)
void rl_light::set_clip_max(const vector& clip_max)
{
    m_clip_max = clip_max;
}

// 0x00A199B0 (.h:243)
void rl_light::set_clip_mesh(rl_renderable_instance* clip_mesh)
{
    m_clip_mesh = clip_mesh;
}

rl_renderable_instance* rl_light::get_clip_mesh()
{
    return m_clip_mesh;
}
