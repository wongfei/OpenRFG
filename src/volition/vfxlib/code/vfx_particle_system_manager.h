// Original: volition/vfxlib/code/vfx_particle_system_manager.h
// Implementation: volition/vfxlib/code/vfx_particle_system_manager.cpp
//
// The particle system manager: the local mempool and the pools of parametric particles, iterative particles and tails
// (sizes from the static maxima), and the particle system they feed.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vlib/code/memory/vmemory.h"

// size 0x7C
class vfx_particle_system_manager
{
protected:
    vfx_particle_system_manager(); // 0x00CA8BC0 (:20)
    virtual ~vfx_particle_system_manager(); // vtbl+0x00 0x00CA8C00 (:25)
public:
    static vfx_particle_system_manager* create(); // 0x00CA8C10 (:31)
    virtual void init(); // vtbl+0x04 0x00CA8C60 (:42)
    virtual void release(); // vtbl+0x08 0x00CA0C00 (:80)
    linked_list_p<vfx_particle_iterative>& get_particle_iterative_list(); // 0x00C9B870 (.h:38)
    linked_list_p<vfx_particle_parametric>& get_particle_parametric_list(); // 0x006E18A0 (.h:39)
    linked_list_p<vfx_particle_iterative::tail_data>& get_particle_tails_list(); // 0x00C9B880 (.h:40)
    mempool& get_local_mempool();
    static void set_mempool_size(int);
    static int get_mempool_size();
    static void set_particles_iterative_max(int _particles_iterative_max); // 0x007C1F40 (.h:47)
    static int get_particles_iterative_max();
    static void set_particles_parametric_max(int _particles_parametric_max); // 0x007C1F50 (.h:50)
    static int get_particles_parametric_max(); // 0x006E18B0 (.h:51)
    static void set_particle_tails_max(int _particle_tails_max); // 0x007C1F60 (.h:53)
    static int get_particle_tails_max();
    int get_num_particles_iterative_free(); // 0x00CA8DF0 (:91)
    int get_num_particles_parametric_free(); // 0x00CA8E00 (:96)
    int get_num_particle_tails_free(); // 0x00CA8E10 (:101)

    // 0x00 vfptr
    static const int local_mempool_size_default; // 0x012C705C
    static const int particle_pool_align; // 0x012C7060
    static const int particles_iterative_max_default; // 0x012C7064
    static const int particles_parametric_max_default; // 0x012C7068
    static const int particle_tails_max_default; // 0x012C706C
private:
    static int local_mempool_size; // 0x016F0A18
    static const int local_mempool_alignment; // 0x012C7070
protected:
    static int particles_iterative_max; // 0x016F0A1C
    static int particles_parametric_max; // 0x016F0A20
    static int particle_tails_max; // 0x016F0A24
    mempool local_mempool; // 0x04
    bool is_initialized; // 0x54
    vfx_particle_iterative* particle_iterative_pool; // 0x58
    vfx_particle_parametric* particle_parametric_pool; // 0x5C
    linked_list_p<vfx_particle_iterative> particles_iterative_freelist; // 0x60
    linked_list_p<vfx_particle_parametric> particles_parametric_freelist; // 0x68
    vfx_particle_iterative::tail_data* particle_tails_pool; // 0x70
    linked_list_p<vfx_particle_iterative::tail_data> particle_tails_freelist; // 0x74
};
