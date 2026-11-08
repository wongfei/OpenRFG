// Original: volition/render_lib/code/rl_particle_batch.cpp
//
// Particles of one material batched for drawing (VFX.md FX2). Only the bookkeeping exists in rfg.exe: prerender and
// render are empty (the particle renderer draws instance by instance).

#include "volition/render_lib/code/rl_particle_batch.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"

// ---- file-static globals
static object_pool<rl_particle_batch::batched_instance,1000,1> Batch_pool; // 0x01C230E0

// :44
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_particle_batch)

// 0x00513960 (:48): the batched instances come from Batch_pool.
rl_particle_batch::rl_particle_batch()
{
    m_material_handle = 0xFFFFFFFF;
    m_renderable_p = nullptr;
    m_batched_instances.init(&Batch_pool);
}

// 0x0054C670 (:55): the batched instances go back to the pool.
rl_particle_batch::~rl_particle_batch()
{
    m_batched_instances.clear();
}

// 0x005297F0 (:61): false when the pool is full; the batch takes the instance's renderable.
bool rl_particle_batch::add_instance(rl_renderable_particles_instance* instance_p)
{
    batched_instance* entry_p = m_batched_instances.alloc(0);
    if (entry_p == nullptr)
    {
        return false;
    }
    entry_p->instance_p = instance_p;
    m_renderable_p = instance_p->get_renderable_particles();
    return true;
}

// 0x004C3BC0 (:79): empty in rfg.exe.
void rl_particle_batch::prerender()
{
}

// 0x004C3BD0 (:84): empty in rfg.exe.
void rl_particle_batch::render()
{
}
