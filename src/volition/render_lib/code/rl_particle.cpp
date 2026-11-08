// Original: volition/render_lib/code/rl_particle.cpp
//
// The render-side particles (VFX.md FX2): an iterative particle's tails come from the particle renderer's tail pool
// (reset each frame). The functions of rl_particle.h are here too (the original has them inline in the header).

#include "volition/render_lib/code/rl_particle.h"

#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/vlib/code/memory/vmemory.h"

// 0x0054C5C0 (:7): count tails of 40 bytes, or none when the pool is full.
bool rl_particle_iterative::alloc_particle_tails(int count)
{
    rl_particle_renderer::get_instance();
    // Magic number: 40 bytes per tail (sizeof(rl_particle_base::tail_data)).
    if (count != 0 && rl_particle_renderer::ms_particle_tail_pool.can_alloc(count * 0x28, 0))
    {
        rl_particle_base::tail_data* tails_p = (rl_particle_base::tail_data*)rl_particle_renderer::ms_particle_tail_pool.alloc(count * 0x28, 0);
        if (tails_p != nullptr)
        {
            m_num_tails = count;
            m_tails_list = tails_p;
            return true;
        }
    }
    m_num_tails = 0;
    m_tails_list = nullptr;
    return false;
}

// 0x004BD460 (.h:127)
int rl_particle_iterative::get_particle_tail_list_size() const
{
    return m_num_tails;
}

// 0x004BD480 (.h:137)
const rl_particle_base::tail_data* rl_particle_iterative::get_particle_tail_list() const
{
    return m_tails_list;
}

// 0x004BD470 (.h:132)
rl_particle_base::tail_data* rl_particle_iterative::get_particle_tail_list()
{
    return m_tails_list;
}
