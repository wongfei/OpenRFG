// Original: volition/vfxlib/code/vfx_particle_system_filter_instance.cpp
//
// A filter instance (VFX.md FX1): the per-effect state of a filter (world transform, force, noise, radius, scale,
// animation time); update hands it to the filter. The functions of vfx_particle_system_filter_instance.h are here too
// (the original has them inline in the header).

#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vfxlib/code/vfx_particle_system_filter.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factory (vfx_particle_system_filter_instance.cpp:8): class id 0xEA5C0501, 500 instances.
// <vfx_particle_system_filter_instance::factory_init> 0x00CB0C10 (:8)
// <vfx_particle_system_filter_instance::release> 0x00CB0D10 (:8)
// <vfx_particle_system_filter_instance::create> 0x00CB7DA0 (:8)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_filter_instance, 0xEA5C0501, 500)

// 0x00C9D3A0 (.h:85)
vfx_particle_system_filter_instance::vfx_particle_system_filter_instance()
{
    m_filter_p = nullptr;
    m_parent_renderable_instance_p = nullptr;
    m_parent_world_position = ZERO_VECTOR;
    m_parent_world_rotation = IDENTITY_MATRIX;
    m_world_rotation = IDENTITY_MATRIX;
    m_world_position = ZERO_VECTOR;
    m_force = 0.0f;
    m_force_noise = 0.0f;
    m_force_vector = ZERO_VECTOR;
    m_scale.set(1.0f, 1.0f, 1.0f);
    m_radius = 0.0f;
    m_anim_time = 0.0f;
}

// 0x00C9D4D0 (.h:101)
vfx_particle_system_filter_instance::~vfx_particle_system_filter_instance()
{
}

// 0x00CA06E0 (:13)
void vfx_particle_system_filter_instance::update(float delta_time)
{
    m_filter_p->update_instance(this, delta_time);
}

// 0x00C9D540 (.h:133)
void vfx_particle_system_filter_instance::set_parent_world_rotation(const matrix& parent_world_rotation)
{
    m_parent_world_rotation = parent_world_rotation;
}

// 0x00743AF0 (.h:155)
void vfx_particle_system_filter_instance::set_world_rotation(const matrix& world_rotation)
{
    m_world_rotation = world_rotation;
}

// 0x00C9D4E0 (.h:106)
vfx_particle_system_filter* vfx_particle_system_filter_instance::get_filter() const
{
    return m_filter_p;
}

// 0x00C9D4F0 (.h:111)
void vfx_particle_system_filter_instance::set_filter(vfx_particle_system_filter* filter_p)
{
    m_filter_p = filter_p;
}

// 0x00C9D500 (.h:117)
const vector& vfx_particle_system_filter_instance::get_parent_world_position() const
{
    return m_parent_world_position;
}

// 0x00C9D510 (.h:122)
void vfx_particle_system_filter_instance::set_parent_world_position(const vector& parent_world_position)
{
    m_parent_world_position = parent_world_position;
}

// 0x00C9D530 (.h:128)
const matrix& vfx_particle_system_filter_instance::get_parent_world_rotation() const
{
    return m_parent_world_rotation;
}

// 0x005D1740 (.h:139)
const vector& vfx_particle_system_filter_instance::get_world_position() const
{
    return m_world_position;
}

// 0x00C9D580 (.h:144)
void vfx_particle_system_filter_instance::set_world_position(const vector& world_position)
{
    m_world_position = world_position;
}

// 0x006E1890 (.h:150)
const matrix& vfx_particle_system_filter_instance::get_world_rotation() const
{
    return m_world_rotation;
}

// 0x00C9D5A0 (.h:172)
rl_renderable_instance* vfx_particle_system_filter_instance::get_parent_renderable_instance() const
{
    return m_parent_renderable_instance_p;
}

// 0x00C9D5B0 (.h:177)
void vfx_particle_system_filter_instance::set_parent_renderable_instance(rl_renderable_instance* parent_renderable_instance_p)
{
    m_parent_renderable_instance_p = parent_renderable_instance_p;
}

// 0x00C9D5C0 (.h:182)
float vfx_particle_system_filter_instance::get_force() const
{
    return m_force;
}

// 0x00743B30 (.h:187)
void vfx_particle_system_filter_instance::set_force(float force)
{
    m_force = force;
}

// 0x00C9D5D0 (.h:193)
float vfx_particle_system_filter_instance::get_force_noise() const
{
    return m_force_noise;
}

// 0x00C9D5E0 (.h:198)
void vfx_particle_system_filter_instance::set_force_noise(float force_noise)
{
    m_force_noise = force_noise;
}

// 0x005D1750 (.h:215)
float vfx_particle_system_filter_instance::get_radius() const
{
    return m_radius;
}

// 0x00C9D600 (.h:220)
void vfx_particle_system_filter_instance::set_radius(float radius)
{
    m_radius = radius;
}

// 0x00C9D620 (.h:226)
const vector& vfx_particle_system_filter_instance::get_scale() const
{
    return m_scale;
}

// 0x00C9D630 (.h:231)
void vfx_particle_system_filter_instance::set_scale(const vector& scale)
{
    m_scale = scale;
}

// 0x00C9D650 (.h:237)
float vfx_particle_system_filter_instance::get_anim_time() const
{
    return m_anim_time;
}

// 0x00C9D660 (.h:242)
void vfx_particle_system_filter_instance::set_anim_time(float anim_time)
{
    m_anim_time = anim_time;
}

