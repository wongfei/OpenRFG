// Original: volition/vfxlib/code/vfx_particle_system_filter_wind.cpp
//
// The wind filter (VFX.md FX1): iterative particles are pushed along the filter's forward axis (jittered by the noise)
// by force x weight. vfx_system keeps one global wind filter instance the game drives (weather).

#include "volition/vfxlib/code/vfx_particle_system_filter_wind.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factories (vfx_particle_system_filter_wind.cpp:11): render_lib pool of 50, vfx class id 0x4A8FB92D.
// <vfx_particle_system_filter_wind::factory_init> 0x00CB1380 (:11)
// <vfx_particle_system_filter_wind::release> 0x00CB1470 (:11)
// <vfx_particle_system_filter_wind::create> 0x00CB8040 (:11)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_filter_wind, 0x4A8FB92D, 50)
vfx_class_factory_info vfx_particle_system_filter_wind::m_vfx_class_factory_info(0x4A8FB92D, &vfx_particle_system_filter_wind::factory_create, &vfx_particle_system_filter_wind::factory_serialize); // 0x016F0A8C

// 0x00CB80C0 (:11)
void* vfx_particle_system_filter_wind::factory_create()
{
    return create();
}

// 0x00CA0BB0 (:11)
void vfx_particle_system_filter_wind::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_filter_wind*)this_p)->serialize(data_p);
}

// 0x00CA8610 (:14): serialized_data_block; its key offsets (from the block) become pointers in place.
int vfx_particle_system_filter_wind::serialize(void* data_p)
{
    serialized_data_block* block = (serialized_data_block*)data_p;
    m_num_force_keys = block->num_force_keys;
    vfx_fixup_block_keys(block->force_keys_p, data_p);
    m_force_keys_p = block->force_keys_p;
    m_num_noise_keys = block->num_noise_keys;
    vfx_fixup_block_keys(block->noise_keys_p, data_p);
    m_noise_keys_p = block->noise_keys_p;
    m_num_angular_velocity_keys = block->num_angular_velocity_keys;
    vfx_fixup_block_keys(block->angular_velocity_keys_p, data_p);
    m_angular_velocity_keys_p = block->angular_velocity_keys_p;
    m_num_angular_velocity_variance_keys = block->num_angular_velocity_variance_keys;
    vfx_fixup_block_keys(block->angular_velocity_variance_keys_p, data_p);
    m_angular_velocity_variance_keys_p = block->angular_velocity_variance_keys_p;
    return sizeof(serialized_data_block);
}

// 0x00CA0BD0 (:40)
vfx_particle_system_filter_wind::vfx_particle_system_filter_wind()
{
    m_base_type = BASE_TYPE_INTERACTOR;
}

// 0x00CA0BF0 (:45)
vfx_particle_system_filter_wind::~vfx_particle_system_filter_wind()
{
}

// 0x00CA86E0 (:51): the direction is the filter's forward axis (into the emitter's parent space for local-space
// emitters); each particle gets a random direction around it (global generator) times weight x force x dt.
void vfx_particle_system_filter_wind::process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time)
{
    vector direction = filter_instance_p->get_world_rotation().fvec.copy_normalize();
    if (instance_p->get_emitter()->get_local_space_emission() == true)
    {
        const matrix transposed = instance_p->get_parent_world_rotation().copy_transpose();
        const float x = transposed.rvec.x * direction.x + transposed.uvec.x * direction.y + transposed.fvec.x * direction.z;
        const float y = transposed.rvec.y * direction.x + transposed.uvec.y * direction.y + transposed.fvec.y * direction.z;
        direction.z = transposed.rvec.z * direction.x + transposed.uvec.z * direction.y + transposed.fvec.z * direction.z;
        direction.y = y;
        direction.x = x;
        direction.normalize();
    }
    const float force = filter_instance_p->get_force();
    const float noise = filter_instance_p->get_force_noise();
    linked_list_p<vfx_particle_iterative>& particles = instance_p->get_particle_iterative_list();
    vfx_particle_iterative* particle_p = particles.first();
    while (particle_p != nullptr)
    {
        vector gust = direction;
        gust.rand_around_dir(direction, 1.0f - noise, 1.0f);
        const float push = particle_p->weight * force * delta_time;
        particle_p->velocity.x = particle_p->velocity.x + gust.x * push;
        particle_p->velocity.y = particle_p->velocity.y + gust.y * push;
        particle_p->velocity.z = gust.z * push + particle_p->velocity.z;
        particle_p = particle_p->next;
        if (particle_p == particles.first())
        {
            break;
        }
    }
}

// 0x00CA8A00 (:78): the world transform is the local one (no parent), then the force and noise keys.
void vfx_particle_system_filter_wind::update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time)
{
    vector local_position;
    get_local_position(local_position, instance_p->get_anim_time());
    const matrix local_rotation(get_local_rotation(instance_p->get_anim_time()));
    matrix43 local;
    local.set_orientation(local_rotation);
    local.set_translation(local_position);
    instance_p->set_world_rotation(local.m_rotation);
    instance_p->set_world_position(local.m_translation);
    const float anim_time = instance_p->get_anim_time();
    float force;
    vfx_interpolate_varkey<vfx_float_key, float>(force, m_num_force_keys, m_force_keys_p, anim_time);
    float noise;
    vfx_interpolate_varkey<vfx_float_key, float>(noise, m_num_noise_keys, m_noise_keys_p, anim_time);
    instance_p->set_force(force);
    instance_p->set_force_noise(noise);
}
