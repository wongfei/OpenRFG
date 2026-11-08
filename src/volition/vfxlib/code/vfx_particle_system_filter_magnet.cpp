// Original: volition/vfxlib/code/vfx_particle_system_filter_magnet.cpp
//
// The magnet filter (VFX.md FX1): iterative particles accelerate toward the filter position (force / distance^2) and
// slow down inside its radius.

#include "volition/vfxlib/code/vfx_particle_system_filter_magnet.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factories (vfx_particle_system_filter_magnet.cpp:12): render_lib pool of 200, vfx class id 0x00AB1EC0.
// <vfx_particle_system_filter_magnet::factory_init> 0x00CB0D90 (:12)
// <vfx_particle_system_filter_magnet::release> 0x00CB0E90 (:12)
// <vfx_particle_system_filter_magnet::create> 0x00CB7E00 (:12)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_filter_magnet, 0x00AB1EC0, 200)
vfx_class_factory_info vfx_particle_system_filter_magnet::m_vfx_class_factory_info(0x00AB1EC0, &vfx_particle_system_filter_magnet::factory_create, &vfx_particle_system_filter_magnet::factory_serialize); // 0x016F0AA0

// 0x00CB7E80 (:12)
void* vfx_particle_system_filter_magnet::factory_create()
{
    return create();
}

// 0x00CA0700 (:12)
void vfx_particle_system_filter_magnet::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_filter_magnet*)this_p)->serialize(data_p);
}

// 0x00CA7530 (:15): serialized_data_block; its key offsets (from the block) become pointers in place.
int vfx_particle_system_filter_magnet::serialize(void* data_p)
{
    serialized_data_block* block = (serialized_data_block*)data_p;
    m_num_force_keys = block->num_force_keys;
    vfx_fixup_block_keys(block->force_keys_p, data_p);
    m_force_keys_p = block->force_keys_p;
    m_num_radius_keys = block->num_radius_keys;
    vfx_fixup_block_keys(block->radius_keys_p, data_p);
    m_radius_keys_p = block->radius_keys_p;
    return sizeof(serialized_data_block);
}

// 0x00CA0720 (:31)
vfx_particle_system_filter_magnet::vfx_particle_system_filter_magnet()
{
    m_base_type = BASE_TYPE_INTERACTOR;
}

// 0x00CA0740 (:36)
vfx_particle_system_filter_magnet::~vfx_particle_system_filter_magnet()
{
}

// 0x00CA75C0 (:42): the offset to the filter after this step; inside the radius the velocity is damped by
// 1 / (d^2 + 1), outside it gains offset / d^2 * force * dt.
void vfx_particle_system_filter_magnet::process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time)
{
    linked_list_p<vfx_particle_iterative>& particles = instance_p->get_particle_iterative_list();
    const float radius = filter_instance_p->get_radius();
    const float force = filter_instance_p->get_force();
    const vector& filter_position = filter_instance_p->get_world_position();
    vfx_particle_iterative* particle_p = particles.first();
    while (particle_p != nullptr)
    {
        const float dx = (filter_position.x - particle_p->position.x) + particle_p->velocity.x * delta_time;
        const float dy = (filter_position.y - particle_p->position.y) + particle_p->velocity.y * delta_time;
        const float dz = (filter_position.z - particle_p->position.z) + particle_p->velocity.z * delta_time;
        const float distance_squared = dx * dx + dy * dy + dz * dz;
        if (0.0f < distance_squared)
        {
            if (distance_squared <= radius * radius)
            {
                const float damping = 1.0f / (distance_squared + 1.0f);
                particle_p->velocity.x = particle_p->velocity.x * damping;
                particle_p->velocity.y = particle_p->velocity.y * damping;
                particle_p->velocity.z = particle_p->velocity.z * damping;
            }
            else
            {
                const float inverse = 1.0f / distance_squared;
                particle_p->velocity.x = particle_p->velocity.x + dx * inverse * force * delta_time;
                particle_p->velocity.y = particle_p->velocity.y + dy * inverse * force * delta_time;
                particle_p->velocity.z = particle_p->velocity.z + dz * inverse * force * delta_time;
            }
        }
        particle_p = particle_p->next;
        if (particle_p == particles.first())
        {
            break;
        }
    }
}

// 0x00CA78A0 (:71): the world transform (local keys under the parent), then the force and radius keys.
void vfx_particle_system_filter_magnet::update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time)
{
    vfx_filter_update_world_transform(this, instance_p);
    const float anim_time = instance_p->get_anim_time();
    float force;
    vfx_interpolate_varkey<vfx_float_key, float>(force, m_num_force_keys, m_force_keys_p, anim_time);
    float radius;
    vfx_interpolate_varkey<vfx_float_key, float>(radius, m_num_radius_keys, m_radius_keys_p, anim_time);
    instance_p->set_force(force);
    instance_p->set_radius(radius);
}
