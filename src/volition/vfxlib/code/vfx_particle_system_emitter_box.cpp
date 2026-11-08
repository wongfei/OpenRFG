// Original: volition/vfxlib/code/vfx_particle_system_emitter_box.cpp
//
// The box emitter (VFX.md FX1): particles born uniformly in the box (half extents width, height, length along the
// emitter's right, up and forward axes); the velocity comes from the base's initiate_velocity.

#include "volition/vfxlib/code/vfx_particle_system_emitter_box.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"

// The class factories (vfx_particle_system_emitter_box.cpp:8): render_lib pool of 150, vfx class id 0xB3269ACC.
// <vfx_particle_system_emitter_box::factory_init> 0x00CAF990 (:8)
// <vfx_particle_system_emitter_box::release> 0x00CAFA90 (:8)
// <vfx_particle_system_emitter_box::create> 0x00CB5BA0 (:8)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_emitter_box, 0xB3269ACC, 150)
vfx_class_factory_info vfx_particle_system_emitter_box::m_vfx_class_factory_info(0xB3269ACC, &vfx_particle_system_emitter_box::factory_create, &vfx_particle_system_emitter_box::factory_serialize); // 0x016F0A64

// 0x00CB5C80 (:8)
void* vfx_particle_system_emitter_box::factory_create()
{
    return create();
}

// 0x00C9F300 (:8)
void vfx_particle_system_emitter_box::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_emitter_box*)this_p)->serialize(data_p);
}

// 0x00C9F320 (:11): serialized_data_block.
int vfx_particle_system_emitter_box::serialize(void* data_p)
{
    const serialized_data_block* block = (const serialized_data_block*)data_p;
    width = block->width;
    height = block->height;
    length = block->length;
    return sizeof(serialized_data_block);
}

// 0x00CAFB10 (:28)
vfx_particle_system_emitter_box::vfx_particle_system_emitter_box()
{
    width = 1.0f;
    height = 1.0f;
    length = 1.0f;
}

// 0x00CAFB50 (:33)
vfx_particle_system_emitter_box::~vfx_particle_system_emitter_box()
{
}

// 0x00C9F350 (:40): the random order is forward, up, right (global generator).
void vfx_particle_system_emitter_box::spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
    const float forward = vfx_float_random(-length, length);
    const float up = vfx_float_random(-height, height);
    const float right = vfx_float_random(-width, width);
    const matrix43& world = instance_p->get_current_world_matrix();
    particle_p->position.x = world.m_translation.x + world.m_rotation.rvec.x * right + world.m_rotation.uvec.x * up + world.m_rotation.fvec.x * forward;
    particle_p->position.y = world.m_translation.y + world.m_rotation.rvec.y * right + world.m_rotation.uvec.y * up + world.m_rotation.fvec.y * forward;
    particle_p->position.z = world.m_translation.z + world.m_rotation.rvec.z * right + world.m_rotation.uvec.z * up + world.m_rotation.fvec.z * forward;
}

// 0x005D1910 (.h:16)
float vfx_particle_system_emitter_box::get_width()
{
    return width;
}

// 0x005D1920 (.h:18)
float vfx_particle_system_emitter_box::get_length()
{
    return length;
}

// 0x005D1930 (.h:20)
float vfx_particle_system_emitter_box::get_height()
{
    return height;
}
