// Original: volition/vfxlib/code/vfx_particle_system_emitter_plane.cpp
//
// The plane emitter (VFX.md FX1): particles born uniformly on the rectangle (half extents width along the right axis,
// length along the forward axis); the velocity comes from the base's initiate_velocity.

#include "volition/vfxlib/code/vfx_particle_system_emitter_plane.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"

// The class factories (vfx_particle_system_emitter_plane.cpp:8): render_lib pool of 200, vfx class id 0xA1CC7F44.
// <vfx_particle_system_emitter_plane::factory_init> 0x00CB0650 (:8)
// <vfx_particle_system_emitter_plane::release> 0x00CB0750 (:8)
// <vfx_particle_system_emitter_plane::create> 0x00CB7A20 (:8)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_emitter_plane, 0xA1CC7F44, 200)
vfx_class_factory_info vfx_particle_system_emitter_plane::m_vfx_class_factory_info(0xA1CC7F44, &vfx_particle_system_emitter_plane::factory_create, &vfx_particle_system_emitter_plane::factory_serialize); // 0x016F0A28

// 0x00CB7AF0 (:8)
void* vfx_particle_system_emitter_plane::factory_create()
{
    return create();
}

// 0x00C9FF40 (:8)
void vfx_particle_system_emitter_plane::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_emitter_plane*)this_p)->serialize(data_p);
}

// 0x00C9FF60 (:11): serialized_data_block.
int vfx_particle_system_emitter_plane::serialize(void* data_p)
{
    const serialized_data_block* block = (const serialized_data_block*)data_p;
    width = block->width;
    length = block->length;
    return sizeof(serialized_data_block);
}

// 0x00CB07D0 (:26)
vfx_particle_system_emitter_plane::vfx_particle_system_emitter_plane()
{
    width = 1.0f;
    length = 1.0f;
}

// 0x00CB0800 (:30)
vfx_particle_system_emitter_plane::~vfx_particle_system_emitter_plane()
{
}

// 0x00C9FF80 (:35): forward first, then right (global generator); the up offset is a literal 0 times the up axis.
void vfx_particle_system_emitter_plane::spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
    const float forward = vfx_float_random(-length, length);
    const float right = vfx_float_random(-width, width);
    const matrix43& world = instance_p->get_current_world_matrix();
    particle_p->position.x = world.m_translation.x + world.m_rotation.rvec.x * right + world.m_rotation.uvec.x * 0.0f + world.m_rotation.fvec.x * forward;
    particle_p->position.y = world.m_translation.y + world.m_rotation.rvec.y * right + world.m_rotation.uvec.y * 0.0f + world.m_rotation.fvec.y * forward;
    particle_p->position.z = world.m_translation.z + world.m_rotation.rvec.z * right + world.m_rotation.uvec.z * 0.0f + world.m_rotation.fvec.z * forward;
}

// 0x005D1940 (.h:17)
float vfx_particle_system_emitter_plane::get_width() const
{
    return width;
}

// 0x005D1950 (.h:19)
float vfx_particle_system_emitter_plane::get_length() const
{
    return length;
}
