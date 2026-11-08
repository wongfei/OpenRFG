// Original: volition/vfxlib/code/vfx_particle_system_manager.cpp
//
// The particle system manager (VFX.md FX1): the iterative, parametric and tail pools, allocated once from the vfx
// system's mempool (128-byte aligned) and threaded into their free lists by init.

#include "volition/vfxlib/code/vfx_particle_system_manager.h"

#include "volition/vfxlib/code/vfx.h"

#include <new>

// ---- statics
const int vfx_particle_system_manager::local_mempool_size_default = 16384; // 0x012C705C
const int vfx_particle_system_manager::particle_pool_align = 128; // 0x012C7060
const int vfx_particle_system_manager::particles_iterative_max_default = 512; // 0x012C7064
const int vfx_particle_system_manager::particles_parametric_max_default = 3584; // 0x012C7068
const int vfx_particle_system_manager::particle_tails_max_default = 1024; // 0x012C706C
int vfx_particle_system_manager::local_mempool_size = 16384; // 0x016F0A18
const int vfx_particle_system_manager::local_mempool_alignment = 4; // 0x012C7070
int vfx_particle_system_manager::particles_iterative_max = 512; // 0x016F0A1C
int vfx_particle_system_manager::particles_parametric_max = 3584; // 0x016F0A20
int vfx_particle_system_manager::particle_tails_max = 1024; // 0x016F0A24

// 0x00CA8BC0 (:20)
vfx_particle_system_manager::vfx_particle_system_manager()
{
    is_initialized = false;
    particle_iterative_pool = nullptr;
    particle_parametric_pool = nullptr;
    particles_iterative_freelist.init();
    particles_parametric_freelist.init();
    particle_tails_pool = nullptr;
    particle_tails_freelist.init();
}

// 0x00CA8C00 (:25)
vfx_particle_system_manager::~vfx_particle_system_manager()
{
}

// 0x00CA8C10 (:31): 0x7C bytes from the vfx system's mempool (no alignment).
vfx_particle_system_manager* vfx_particle_system_manager::create()
{
    void* data_p = vfx_system::get_mempool()->alloc(sizeof(vfx_particle_system_manager), 0);
    if (data_p == nullptr)
    {
        return nullptr;
    }
    return new (data_p) vfx_particle_system_manager();
}

// 0x00CA8C60 (:42): the pools are allocated on the first init only; every init rebuilds the free lists.
void vfx_particle_system_manager::init()
{
    mempool* pool = vfx_system::get_mempool();
    if (particle_iterative_pool == nullptr)
    {
        particle_iterative_pool = (vfx_particle_iterative*)pool->alloc(particles_iterative_max * sizeof(vfx_particle_iterative), particle_pool_align);
    }
    if (particle_parametric_pool == nullptr)
    {
        particle_parametric_pool = (vfx_particle_parametric*)pool->alloc(particles_parametric_max * sizeof(vfx_particle_parametric), particle_pool_align);
    }
    if (particle_tails_pool == nullptr)
    {
        particle_tails_pool = (vfx_particle_iterative::tail_data*)pool->alloc(particle_tails_max * sizeof(vfx_particle_iterative::tail_data), particle_pool_align);
    }
    particles_iterative_freelist.init();
    for (int i = 0; i < particles_iterative_max; i++)
    {
        particles_iterative_freelist.append(&particle_iterative_pool[i]);
    }
    particles_parametric_freelist.init();
    for (int i = 0; i < particles_parametric_max; i++)
    {
        particles_parametric_freelist.append(&particle_parametric_pool[i]);
    }
    particle_tails_freelist.init();
    for (int i = 0; i < particle_tails_max; i++)
    {
        particle_tails_freelist.append(&particle_tails_pool[i]);
    }
    is_initialized = true;
}

// 0x00CA0C00 (:80): forgets the pools (the memory belongs to the mempool).
void vfx_particle_system_manager::release()
{
    particle_iterative_pool = nullptr;
    particle_parametric_pool = nullptr;
    particle_tails_pool = nullptr;
    is_initialized = false;
}

// 0x00C9B870 (.h:38)
linked_list_p<vfx_particle_iterative>& vfx_particle_system_manager::get_particle_iterative_list()
{
    return particles_iterative_freelist;
}

// 0x006E18A0 (.h:39)
linked_list_p<vfx_particle_parametric>& vfx_particle_system_manager::get_particle_parametric_list()
{
    return particles_parametric_freelist;
}

// 0x00C9B880 (.h:40)
linked_list_p<vfx_particle_iterative::tail_data>& vfx_particle_system_manager::get_particle_tails_list()
{
    return particle_tails_freelist;
}

// 0x007C1F40 (.h:47)
void vfx_particle_system_manager::set_particles_iterative_max(int _particles_iterative_max)
{
    particles_iterative_max = _particles_iterative_max;
}

// 0x007C1F50 (.h:50)
void vfx_particle_system_manager::set_particles_parametric_max(int _particles_parametric_max)
{
    particles_parametric_max = _particles_parametric_max;
}

// 0x006E18B0 (.h:51)
int vfx_particle_system_manager::get_particles_parametric_max()
{
    return particles_parametric_max;
}

// 0x007C1F60 (.h:53)
void vfx_particle_system_manager::set_particle_tails_max(int _particle_tails_max)
{
    particle_tails_max = _particle_tails_max;
}

// 0x00CA8DF0 (:91)
int vfx_particle_system_manager::get_num_particles_iterative_free()
{
    return particles_iterative_freelist.get_elem_count();
}

// 0x00CA8E00 (:96)
int vfx_particle_system_manager::get_num_particles_parametric_free()
{
    return particles_parametric_freelist.get_elem_count();
}

// 0x00CA8E10 (:101)
int vfx_particle_system_manager::get_num_particle_tails_free()
{
    return particle_tails_freelist.get_elem_count();
}
