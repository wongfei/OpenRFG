// Original: volition/render_lib/code/rl_renderable_particles_instance.cpp
//
// A placed particle system (VFX.md FX2): the particles vfxlib copies in each frame (from the particle renderer's
// pools), the parametric constant block of a parametric emitter, and its submission to the particle renderer. The
// inline members are in rl_renderable_particles_instance.inl.

#include "volition/render_lib/code/rl_renderable_particles_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderable_particles.h"
#include "volition/render_lib/code/rl_renderer_core.h"

// The class factory (:10): class id 0x68F93FD7, 1600 instances.
// <rl_renderable_particles_instance::factory_init> 0x00515990 (:10)
// <rl_renderable_particles_instance::create> 0x0052CF10 (:10)
RL_FACTORY_CLASS_CORE(rl_renderable_particles_instance, 0x68F93FD7, 1600)

// The parametric constant blocks of all parametric instances (one each).
object_pool<rl_renderable_particles_instance::parametric_constant_data, 1400, 1> rl_renderable_particles_instance::ms_parametric_constants_data_pool; // 0x01BBBE80

// 0x004D9600
rl_renderable_particles_instance::parametric_constant_data::parametric_constant_data()
{
}

// 0x00515AA0 (:17)
rl_renderable_particles_instance::rl_renderable_particles_instance()
{
    m_tail_spawn_count = 0;
    m_break_link_chain = false;
    m_local_emission_position = ZERO_VECTOR;
    m_local_emission_rotation = IDENTITY_MATRIX;
    m_has_alternate_view = false;
    m_parametric_constant_data_p = nullptr;
    m_frame_number = 0;
    m_texture_anim_rate = 1.0f;
    m_particle_iterative_list.clear();
    m_particle_parametric_list.clear();
    m_opacity = 1.0f;
    m_effect_material_tint.red = 0.0f;
    m_effect_material_tint.green = 0.0f;
    m_renderable_particles_p = nullptr;
    m_num_tails = 0;
    m_tails = nullptr;
    m_is_visible = true;
    m_effect_material_tint.blue = 0.0f;
    m_effect_material_tint.alpha = 0.0f;
}

// 0x0052CF70 (:38): the parametric constant block goes back to its pool (the particle arrays belong to the renderer's
// pools, reset each frame).
rl_renderable_particles_instance::~rl_renderable_particles_instance()
{
    if (m_parametric_constant_data_p != nullptr)
    {
        ms_parametric_constants_data_pool.free(m_parametric_constant_data_p);
        m_parametric_constant_data_p = nullptr;
    }
}

// 0x00543930 (:10): deferred while render_lib is locked.
void rl_renderable_particles_instance::release()
{
    if (rl_is_locked())
    {
        rl_add_deferred_op(RLDO_RELEASE_OBJECT, this);
        return;
    }
    if (ms_object_pool.size() < 1 && ms_class_factory_info.get_display_exhausted_factories_warning())
    {
        rl_class_factory_info::decrement_num_exhausted_factories();
    }
    ms_object_pool.add(this);
    this->~rl_renderable_particles_instance();
}

// 0x00515C00 (:47)
void rl_renderable_particles_instance::clear()
{
    m_particle_parametric_list.clear();
    m_particle_iterative_list.clear();
}

// 0x0054CC10 (:54): queued with the particle renderer when there is something to draw.
void rl_renderable_particles_instance::render()
{
    if ((0 < m_particle_iterative_list.size() || 0 < m_particle_parametric_list.size()) && m_is_visible)
    {
        rl_particle_renderer::get_instance()->render_for_deferred(this);
    }
}

// 0x0054CC80 (:75): count particles of 128 bytes from the renderer's iterative pool (reset each frame).
bool rl_renderable_particles_instance::alloc_iterative_particles(int count)
{
    rl_particle_renderer::get_instance();
    // Magic number: 128 bytes per iterative particle (sizeof(rl_particle_iterative)).
    if (count == 0 || !rl_particle_renderer::ms_particle_iterative_pool.can_alloc(count << 7, 0))
    {
        return false;
    }
    rl_particle_iterative* particles_p = (rl_particle_iterative*)rl_particle_renderer::ms_particle_iterative_pool.alloc(count << 7, 0);
    if (particles_p == nullptr)
    {
        return false;
    }
    m_particle_iterative_list.init(particles_p, count, count);
    return true;
}

// 0x0054CD60 (:88): count particles of 64 bytes from the renderer's parametric pool (reset each frame).
bool rl_renderable_particles_instance::alloc_parametric_particles(int count)
{
    rl_particle_renderer::get_instance();
    // Magic number: 64 bytes per parametric particle (sizeof(rl_particle_parametric)).
    if (count == 0 || !rl_particle_renderer::ms_particle_parametric_pool.can_alloc(count << 6, 0))
    {
        return false;
    }
    rl_particle_parametric* particles_p = (rl_particle_parametric*)rl_particle_renderer::ms_particle_parametric_pool.alloc(count << 6, 0);
    if (particles_p == nullptr)
    {
        return false;
    }
    m_particle_parametric_list.init(particles_p, count, count);
    return true;
}

// 0x0052CFE0 (:101): a parametric renderable needs a constant block, an iterative one gives it back.
void rl_renderable_particles_instance::set_renderable_particles(rl_renderable_particles* renderable_particles_p)
{
    m_renderable_particles_p = renderable_particles_p;
    if (!renderable_particles_p->get_is_iterative())
    {
        if (m_parametric_constant_data_p == nullptr)
        {
            m_parametric_constant_data_p = ms_parametric_constants_data_pool.alloc(0);
        }
    }
    else if (m_parametric_constant_data_p != nullptr)
    {
        ms_parametric_constants_data_pool.free(m_parametric_constant_data_p);
        m_parametric_constant_data_p = nullptr;
    }
}

// 0x004F1770 (:142)
int rl_renderable_particles_instance::get_parametric_constant_pool_available()
{
    return ms_parametric_constants_data_pool.get_free();
}
