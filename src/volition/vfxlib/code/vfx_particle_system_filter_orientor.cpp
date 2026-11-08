// Original: volition/vfxlib/code/vfx_particle_system_filter_orientor.cpp
//
// The orientor filter (VFX.md FX1): the emitter instance renders its particles facing the filter's transform (the
// alternate view) instead of the camera.

#include "volition/vfxlib/code/vfx_particle_system_filter_orientor.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factories (vfx_particle_system_filter_orientor.cpp:11): render_lib pool of 100, vfx class id 0x7EDA2EE8.
// <vfx_particle_system_filter_orientor::factory_init> 0x00CB0F10 (:11)
// <vfx_particle_system_filter_orientor::release> 0x00CB1010 (:11)
// <vfx_particle_system_filter_orientor::create> 0x00CB7E90 (:11)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_filter_orientor, 0x7EDA2EE8, 100)
vfx_class_factory_info vfx_particle_system_filter_orientor::m_vfx_class_factory_info(0x7EDA2EE8, &vfx_particle_system_filter_orientor::factory_create, &vfx_particle_system_filter_orientor::factory_serialize); // 0x016F0ADC

// 0x00CB7F10 (:11)
void* vfx_particle_system_filter_orientor::factory_create()
{
    return create();
}

// 0x00CA0750 (:11)
void vfx_particle_system_filter_orientor::factory_serialize(void* this_p, void* data_p)
{
    ((vfx_particle_system_filter_orientor*)this_p)->serialize(data_p);
}

// 0x00CA0770 (:14): no serialized data.
int vfx_particle_system_filter_orientor::serialize(void* data_p)
{
    return 0;
}

// 0x00CA0780 (:21)
vfx_particle_system_filter_orientor::vfx_particle_system_filter_orientor()
{
    m_base_type = BASE_TYPE_INTERACTOR;
}

// 0x00CA07A0 (:26)
vfx_particle_system_filter_orientor::~vfx_particle_system_filter_orientor()
{
}

// 0x00CA07B0 (:32)
void vfx_particle_system_filter_orientor::process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time)
{
    instance_p->set_has_alternate_view(true);
    instance_p->set_alternate_view(matrix43(filter_instance_p->get_world_position(), filter_instance_p->get_world_rotation()));
}

// 0x00CA0830 (:39)
void vfx_particle_system_filter_orientor::update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time)
{
    vfx_filter_update_world_transform(this, instance_p);
}
