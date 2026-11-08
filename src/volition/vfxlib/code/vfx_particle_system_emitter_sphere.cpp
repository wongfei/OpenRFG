// Original: volition/vfxlib/code/vfx_particle_system_emitter_sphere.cpp
//
// The sphere emitter (VFX.md FX1): a random direction (three fl_rand calls in a cube, normalized: not uniform on the
// sphere) gives the birth point on the sphere around the spawn position and the outward velocity.

#include "volition/vfxlib/code/vfx_particle_system_emitter_sphere.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vlib/code/math/rand.h"

// The class factories (vfx_particle_system_emitter_sphere.cpp:8): render_lib pool of 400, vfx class id 0x1D815E4A.
// <vfx_particle_system_emitter_sphere::factory_init> 0x00CB0A20 (:8)
// <vfx_particle_system_emitter_sphere::release> 0x00CB0B20 (:8)
// <vfx_particle_system_emitter_sphere::create> 0x00CB7C70 (:8)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_emitter_sphere, 0x1D815E4A, 400)
vfx_class_factory_info vfx_particle_system_emitter_sphere::m_vfx_class_factory_info(0x1D815E4A, &vfx_particle_system_emitter_sphere::factory_create, &vfx_particle_system_emitter_sphere::factory_serialize); // 0x016F0A50

// 0x00CB7D30 (:8)
void* vfx_particle_system_emitter_sphere::factory_create()
{
    return create();
}

// 0x00CA0220 (:8)
void vfx_particle_system_emitter_sphere::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_emitter_sphere*)this_p)->serialize(data_p);
}

// 0x00CA0240 (:12): serialized_data_block.
int vfx_particle_system_emitter_sphere::serialize(void* data_p)
{
    radius = ((const serialized_data_block*)data_p)->radius;
    return sizeof(serialized_data_block);
}

// 0x00CB0BA0 (:25)
vfx_particle_system_emitter_sphere::vfx_particle_system_emitter_sphere()
{
    radius = 1.0f;
}

// 0x00CB0BD0 (:30)
vfx_particle_system_emitter_sphere::~vfx_particle_system_emitter_sphere()
{
}

// 0x00CA0260 (:35): the random numbers go to z, y, x in call order (global generator).
void vfx_particle_system_emitter_sphere::spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
    const float random_z = fl_rand();
    const float random_y = fl_rand();
    const float random_x = fl_rand();
    vector direction;
    direction.x = (random_x + random_x) - 1.0f;
    direction.y = (random_y + random_y) - 1.0f;
    direction.z = (random_z + random_z) - 1.0f;
    direction.normalize();
    const matrix rotation(instance_p->get_current_world_matrix().m_rotation);
    const vector& spawn_position = instance_p->get_current_spawn_position();
    const float right = radius * direction.x;
    const float up = radius * direction.y;
    const float forward = direction.z * radius;
    particle_p->position.x = spawn_position.x + rotation.rvec.x * right + rotation.uvec.x * up + rotation.fvec.x * forward;
    particle_p->position.y = spawn_position.y + rotation.rvec.y * right + rotation.uvec.y * up + rotation.fvec.y * forward;
    particle_p->position.z = spawn_position.z + rotation.rvec.z * right + rotation.uvec.z * up + rotation.fvec.z * forward;
    const matrix& world_rotation = instance_p->get_current_world_matrix().m_rotation;
    vector velocity;
    velocity.x = world_rotation.rvec.x * direction.x + world_rotation.uvec.x * direction.y + world_rotation.fvec.x * direction.z;
    velocity.y = world_rotation.rvec.y * direction.x + world_rotation.uvec.y * direction.y + world_rotation.fvec.y * direction.z;
    velocity.z = world_rotation.rvec.z * direction.x + world_rotation.uvec.z * direction.y + world_rotation.fvec.z * direction.z;
    velocity.normalize();
    particle_p->velocity = velocity;
}

// 0x00CA0680 (:57): spawn_particle sets the velocity.
void vfx_particle_system_emitter_sphere::initiate_velocity(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
}

// 0x005D1900 (.h:16)
float vfx_particle_system_emitter_sphere::get_radius()
{
    return radius;
}
