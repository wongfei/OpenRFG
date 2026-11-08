// Original: volition/vfxlib/code/vfx_particle_system_filter_vortex.cpp
//
// The vortex filter (VFX.md FX1): iterative particles are turned about the filter, by an angle that falls off as
// 1 / (d^2 + 1) with the distance (in filter space) and scales with the filter's up axis and scale keys.

#include "volition/vfxlib/code/vfx_particle_system_filter_vortex.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factories (vfx_particle_system_filter_vortex.cpp:12): render_lib pool of 10, vfx class id 0xDB1A7FC6.
// <vfx_particle_system_filter_vortex::factory_init> 0x00CB1210 (:12)
// <vfx_particle_system_filter_vortex::release> 0x00CB1300 (:12)
// <vfx_particle_system_filter_vortex::create> 0x00CB7FB0 (:12)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_filter_vortex, 0xDB1A7FC6, 10)
vfx_class_factory_info vfx_particle_system_filter_vortex::m_vfx_class_factory_info(0xDB1A7FC6, &vfx_particle_system_filter_vortex::factory_create, &vfx_particle_system_filter_vortex::factory_serialize); // 0x016F0AC8

// 0x00CB8030 (:12)
void* vfx_particle_system_filter_vortex::factory_create()
{
    return create();
}

// 0x00CA09D0 (:12)
void vfx_particle_system_filter_vortex::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_filter_vortex*)this_p)->serialize(data_p);
}

// 0x00CA7F10 (:15): serialized_data_block; the scale key offset (from the block) becomes a pointer in place.
int vfx_particle_system_filter_vortex::serialize(void* data_p)
{
    serialized_data_block* block = (serialized_data_block*)data_p;
    m_num_scale_keys = block->num_scale_keys;
    vfx_fixup_block_keys(block->scale_keys_p, data_p);
    m_scale_keys_p = block->scale_keys_p;
    return sizeof(serialized_data_block);
}

// 0x00CA09F0 (:27)
vfx_particle_system_filter_vortex::vfx_particle_system_filter_vortex()
{
    m_base_type = BASE_TYPE_INTERACTOR;
}

// 0x00CA0A10 (:32)
vfx_particle_system_filter_vortex::~vfx_particle_system_filter_vortex()
{
}

// 0x00CA7F70 (:38): each particle into filter space, rotated by the pitch/bank/heading of (falloff x scale x up axis),
// and back.
void vfx_particle_system_filter_vortex::process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time)
{
    const vector& up = filter_instance_p->get_world_rotation().uvec;
    const vector& scale = filter_instance_p->get_scale();
    const matrix43 world(filter_instance_p->get_world_position(), filter_instance_p->get_world_rotation());
    const matrix43 inverse = world.invert_orthogonal();
    linked_list_p<vfx_particle_iterative>& particles = instance_p->get_particle_iterative_list();
    vfx_particle_iterative* particle_p = particles.first();
    while (particle_p != nullptr)
    {
        const vector& position = particle_p->position;
        const float lx = inverse.m_translation.x + inverse.m_rotation.rvec.x * position.x + inverse.m_rotation.uvec.x * position.y + inverse.m_rotation.fvec.x * position.z;
        const float ly = inverse.m_translation.y + inverse.m_rotation.rvec.y * position.x + inverse.m_rotation.uvec.y * position.y + inverse.m_rotation.fvec.y * position.z;
        const float lz = inverse.m_translation.z + inverse.m_rotation.rvec.z * position.x + inverse.m_rotation.uvec.z * position.y + inverse.m_rotation.fvec.z * position.z;
        const float falloff = 1.0f / (ly * ly + lx * lx + lz * lz + 1.0f);
        const matrix spin(falloff * scale.x * up.x, falloff * scale.z * up.z, falloff * scale.y * up.y);
        const float rx = spin.rvec.x * lx + spin.uvec.x * ly + spin.fvec.x * lz;
        const float ry = spin.rvec.y * lx + spin.uvec.y * ly + spin.fvec.y * lz;
        const float rz = spin.rvec.z * lx + spin.uvec.z * ly + spin.fvec.z * lz;
        particle_p->position.x = world.m_translation.x + world.m_rotation.rvec.x * rx + world.m_rotation.uvec.x * ry + world.m_rotation.fvec.x * rz;
        particle_p->position.y = world.m_translation.y + world.m_rotation.rvec.y * rx + world.m_rotation.uvec.y * ry + world.m_rotation.fvec.y * rz;
        particle_p->position.z = world.m_translation.z + world.m_rotation.rvec.z * rx + world.m_rotation.uvec.z * ry + world.m_rotation.fvec.z * rz;
        particle_p = particle_p->next;
        if (particle_p == particles.first())
        {
            break;
        }
    }
}

// 0x00CA0A20 (:90): the world transform, then the scale keys (spread over the animation).
void vfx_particle_system_filter_vortex::update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time)
{
    vfx_filter_update_world_transform(this, instance_p);
    vector scale;
    vfx_interpolate_vector(scale, instance_p->get_anim_time() / get_animation_duration(), m_num_scale_keys, m_scale_keys_p);
    instance_p->set_scale(scale);
}
