// Original: volition/vfxlib/code/vfx_particle_system_emitter_point.cpp
//
// The point emitter (VFX.md FX1): particles born at the instance's spawn position; the velocity comes from the base's
// initiate_velocity.

#include "volition/vfxlib/code/vfx_particle_system_emitter_point.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"

// The class factories (vfx_particle_system_emitter_point.cpp:8): render_lib pool of 1220, vfx class id 0xEC276F11.
// <vfx_particle_system_emitter_point::factory_init> 0x00CB0840 (:8)
// <vfx_particle_system_emitter_point::release> 0x00CB0940 (:8)
// <vfx_particle_system_emitter_point::create> 0x00CB7B60 (:8)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_emitter_point, 0xEC276F11, 1220)
vfx_class_factory_info vfx_particle_system_emitter_point::m_vfx_class_factory_info(0xEC276F11, &vfx_particle_system_emitter_point::factory_create, &vfx_particle_system_emitter_point::factory_serialize); // 0x016F0A3C

// 0x00CB7C00 (:8)
void* vfx_particle_system_emitter_point::factory_create()
{
    return create();
}

// 0x00CA01D0 (:8)
void vfx_particle_system_emitter_point::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_emitter_point*)this_p)->serialize(data_p);
}

// 0x00CA01F0 (:10): no serialized data.
int vfx_particle_system_emitter_point::serialize(void* data_p)
{
    return 0;
}

// 0x00CB09C0 (:16)
vfx_particle_system_emitter_point::vfx_particle_system_emitter_point()
{
}

// 0x00CB09E0 (:21)
vfx_particle_system_emitter_point::~vfx_particle_system_emitter_point()
{
}

// 0x00CA0200 (:28)
void vfx_particle_system_emitter_point::spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
    particle_p->position = instance_p->get_current_spawn_position();
}
