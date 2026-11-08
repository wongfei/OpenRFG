// Original: volition/vfxlib/code/vfx_particle_system_emitter_cylinder.cpp
//
// The cylinder emitter (VFX.md FX1): particles born on the cylinder's mantle (radius, half height along the up axis),
// moving outward, optionally tilted toward the up axis by a random angle between the yaw and pitch variances.

#include "volition/vfxlib/code/vfx_particle_system_emitter_cylinder.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// The class factories (vfx_particle_system_emitter_cylinder.cpp:9): render_lib pool of 425, vfx class id 0xFAA8D302.
// <vfx_particle_system_emitter_cylinder::factory_init> 0x00CAFB90 (:9)
// <vfx_particle_system_emitter_cylinder::release> 0x00CAFC90 (:9)
// <vfx_particle_system_emitter_cylinder::create> 0x00CB5CF0 (:9)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_emitter_cylinder, 0xFAA8D302, 425)
vfx_class_factory_info vfx_particle_system_emitter_cylinder::m_vfx_class_factory_info(0xFAA8D302, &vfx_particle_system_emitter_cylinder::factory_create, &vfx_particle_system_emitter_cylinder::factory_serialize); // 0x016F0A78

// The inlined float comparison with 0: within 0.0001 times max(|value|, 1) (decompiled).
static bool vfx_cylinder_is_zero(float value)
{
    float scale = fabsf(value);
    if (scale - 0.0f < 0.0f)
    {
        scale = 0.0f;
    }
    if (0.0f <= 1.0f - scale)
    {
        scale = 1.0f;
    }
    return 0.0f - scale * 0.0001f <= value && value <= scale * 0.0001f + 0.0f;
}

// 0x00CB5DC0 (:9)
void* vfx_particle_system_emitter_cylinder::factory_create()
{
    return create();
}

// 0x00C9F600 (:9)
void vfx_particle_system_emitter_cylinder::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_emitter_cylinder*)this_p)->serialize(data_p);
}

// 0x00C9F620 (:12): serialized_data_block.
int vfx_particle_system_emitter_cylinder::serialize(void* data_p)
{
    const serialized_data_block* block = (const serialized_data_block*)data_p;
    radius = block->radius;
    height = block->height;
    return sizeof(serialized_data_block);
}

// 0x00CAFD10 (:27)
vfx_particle_system_emitter_cylinder::vfx_particle_system_emitter_cylinder()
{
    radius = 1.0f;
    height = 1.0f;
}

// 0x00CAFD40 (:31)
vfx_particle_system_emitter_cylinder::~vfx_particle_system_emitter_cylinder()
{
}

// 0x00C9F640 (:36): the spread is off when both variances are zero.
void vfx_particle_system_emitter_cylinder::init()
{
    m_spread_enabled = !(vfx_cylinder_is_zero(yaw_variance) && vfx_cylinder_is_zero(pitch_variance));
}

// 0x00C9F790 (:48): a random direction in the emitter's horizontal plane (two fl_rand calls: z first) is the outward
// velocity and the mantle point; the spread rotates the velocity about direction x up by a random angle, the height is
// a third random number (global generator).
void vfx_particle_system_emitter_cylinder::spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
    const float random_z = fl_rand();
    const float random_x = fl_rand();
    vector direction;
    direction.y = 0.0f;
    direction.x = (random_x + random_x) - 1.0f;
    direction.z = (random_z + random_z) - 1.0f;
    direction.normalize_safe();
    const matrix43& world = instance_p->get_current_world_matrix();
    const matrix& rotation = world.m_rotation;
    vector velocity;
    velocity.x = rotation.rvec.x * direction.x + rotation.uvec.x * direction.y + rotation.fvec.x * direction.z;
    velocity.y = rotation.rvec.y * direction.x + rotation.uvec.y * direction.y + rotation.fvec.y * direction.z;
    velocity.z = rotation.rvec.z * direction.x + rotation.uvec.z * direction.y + rotation.fvec.z * direction.z;
    velocity.normalize_safe();
    if (m_spread_enabled)
    {
        const float angle = vfx_float_random(yaw_variance, pitch_variance);
        vector axis;
        axis.x = rotation.uvec.z * velocity.y - rotation.uvec.y * velocity.z;
        axis.z = rotation.uvec.y * velocity.x - rotation.uvec.x * velocity.y;
        axis.y = rotation.uvec.x * velocity.z - rotation.uvec.z * velocity.x;
        matrix spread;
        spread.set(axis, angle);
        velocity.rotate(spread);
        velocity.normalize();
    }
    const float up = vfx_float_random(-height, height);
    const float right = radius * direction.x;
    const float forward = radius * direction.z;
    // The translation is added last for x and first for y and z (decompiled order).
    particle_p->position.x = rotation.rvec.x * right + rotation.uvec.x * up + rotation.fvec.x * forward + world.m_translation.x;
    particle_p->position.y = world.m_translation.y + rotation.rvec.y * right + rotation.uvec.y * up + rotation.fvec.y * forward;
    particle_p->position.z = world.m_translation.z + rotation.rvec.z * right + rotation.uvec.z * up + rotation.fvec.z * forward;
    particle_p->velocity = velocity;
}

// 0x00C9FCA0 (:81): spawn_particle sets the velocity.
void vfx_particle_system_emitter_cylinder::initiate_velocity(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
}

// 0x005D18E0 (.h:16)
float vfx_particle_system_emitter_cylinder::get_radius()
{
    return radius;
}

// 0x005D18F0 (.h:18)
float vfx_particle_system_emitter_cylinder::get_height()
{
    return height;
}
