// Original: volition/vfxlib/code/vfx_particle_system_filter_turbulence.cpp
//
// The turbulence filter (VFX.md FX1): every iterative particle's direction is jittered inside a cone (the wider the
// noise) and its speed scaled by the force.

#include "volition/vfxlib/code/vfx_particle_system_filter_turbulence.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factories (vfx_particle_system_filter_turbulence.cpp:11): render_lib pool of 100, vfx class id 0x6BAB63EA.
// <vfx_particle_system_filter_turbulence::factory_init> 0x00CB1090 (:11)
// <vfx_particle_system_filter_turbulence::release> 0x00CB1190 (:11)
// <vfx_particle_system_filter_turbulence::create> 0x00CB7F20 (:11)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_filter_turbulence, 0x6BAB63EA, 100)
vfx_class_factory_info vfx_particle_system_filter_turbulence::m_vfx_class_factory_info(0x6BAB63EA, &vfx_particle_system_filter_turbulence::factory_create, &vfx_particle_system_filter_turbulence::factory_serialize); // 0x016F0AB4

// 0x00CB7FA0 (:11)
void* vfx_particle_system_filter_turbulence::factory_create()
{
    return create();
}

// 0x00CA0980 (:11)
void vfx_particle_system_filter_turbulence::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_filter_turbulence*)this_p)->serialize(data_p);
}

// 0x00CA7B10 (:14): serialized_data_block; its key offsets (from the block) become pointers in place.
int vfx_particle_system_filter_turbulence::serialize(void* data_p)
{
    serialized_data_block* block = (serialized_data_block*)data_p;
    m_num_force_keys = block->num_force_keys;
    vfx_fixup_block_keys(block->force_keys_p, data_p);
    m_force_keys_p = block->force_keys_p;
    m_num_noise_keys = block->num_noise_keys;
    vfx_fixup_block_keys(block->noise_keys_p, data_p);
    m_noise_keys_p = block->noise_keys_p;
    return sizeof(serialized_data_block);
}

// 0x00CA09A0 (:31)
vfx_particle_system_filter_turbulence::vfx_particle_system_filter_turbulence()
{
    m_base_type = BASE_TYPE_INTERACTOR;
}

// 0x00CA09C0 (:36)
vfx_particle_system_filter_turbulence::~vfx_particle_system_filter_turbulence()
{
}

// 0x00CA7BA0 (:42): the new direction is a random one around the old (rand_around_dir with the same vector as source
// and target, global generator); speed x force keeps the magnitude scaled.
void vfx_particle_system_filter_turbulence::process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time)
{
    const float force = filter_instance_p->get_force();
    linked_list_p<vfx_particle_iterative>& particles = instance_p->get_particle_iterative_list();
    const float noise = filter_instance_p->get_force_noise();
    vfx_particle_iterative* particle_p = particles.first();
    while (particle_p != nullptr)
    {
        vector direction = particle_p->velocity;
        const float speed = direction.mag_and_normalize_safe();
        direction.rand_around_dir(direction, 1.0f - noise, 1.0f);
        particle_p->velocity.x = direction.x * speed * force;
        particle_p->velocity.y = direction.y * speed * force;
        particle_p->velocity.z = direction.z * speed * force;
        particle_p = particle_p->next;
        if (particle_p == particles.first())
        {
            break;
        }
    }
}

// 0x00CA7D10 (:68): the world transform, then the force and noise keys.
void vfx_particle_system_filter_turbulence::update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time)
{
    vfx_filter_update_world_transform(this, instance_p);
    const float anim_time = instance_p->get_anim_time();
    float force;
    vfx_interpolate_varkey<vfx_float_key, float>(force, m_num_force_keys, m_force_keys_p, anim_time);
    float noise;
    vfx_interpolate_varkey<vfx_float_key, float>(noise, m_num_noise_keys, m_noise_keys_p, anim_time);
    instance_p->set_force(force);
    instance_p->set_force_noise(noise);
}
