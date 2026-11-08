// Original: volition/render_lib/code/rl_renderable_particles_instance_group.cpp
//
// The renderable of an effect (VFX.md FX2): one scene entity for all particle system instances of an effect (kept in
// descending sort priority) and its coronas. Rendering it queues each visible instance with the particle renderer's
// deferred passes, with the group's tint and opacity, and renders the coronas.

#include "volition/render_lib/code/rl_renderable_particles_instance_group.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_particle_renderer.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "keen/base/trace.hpp"

// The class factory (:11): class id 0x74F67CC2, 512 groups.
// <rl_renderable_particles_instance_group::factory_init> 0x00515C50 (:11)
// <rl_renderable_particles_instance_group::create> 0x0054CE40 (:11)
RL_FACTORY_CLASS_CORE(rl_renderable_particles_instance_group, 0x74F67CC2, 512)

// 0x00543A50 (:15): the effect material tint starts white.
rl_renderable_particles_instance_group::rl_renderable_particles_instance_group()
    : m_effect_material_tint(rl_color::WHITE)
{
}

// 0x00543B10 (:20)
rl_renderable_particles_instance_group::~rl_renderable_particles_instance_group()
{
}

// 0x005439D0 (:11): deferred while render_lib is locked.
void rl_renderable_particles_instance_group::release()
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
    this->~rl_renderable_particles_instance_group();
}

// 0x004F1790 (:25): kept in descending sort priority (a new instance goes before the first one of lower or equal
// priority, else at the end).
void rl_renderable_particles_instance_group::add_instance(rl_renderable_particles_instance* instance_p)
{
    if (m_instances.get_elem_count() != 0)
    {
        rl_renderable_particles_instance* head_p = m_instances.first();
        rl_renderable_particles_instance* cursor_p = head_p;
        while (cursor_p != nullptr)
        {
            if (cursor_p->get_sort_priority() <= instance_p->get_sort_priority())
            {
                m_instances.insert_before(cursor_p, instance_p);
                return;
            }
            cursor_p = cursor_p->next;
            if (cursor_p == head_p)
            {
                break;
            }
        }
    }
    m_instances.append(instance_p);
}

// 0x004F18A0 (:53): only an instance of this group is removed.
void rl_renderable_particles_instance_group::remove_instance(rl_renderable_particles_instance* instance_p)
{
    rl_renderable_particles_instance* cursor_p = m_instances.first();
    if (cursor_p == nullptr)
    {
        return;
    }
    while (cursor_p != instance_p)
    {
        cursor_p = cursor_p->next;
        if (cursor_p == m_instances.first() || cursor_p == nullptr)
        {
            return;
        }
    }
    m_instances.remove(instance_p);
}

// 0x004F1920 (:67)
void rl_renderable_particles_instance_group::add_instance(rl_corona_instance* instance_p)
{
    m_coronas.append(instance_p);
}

// 0x004F1980 (:72)
void rl_renderable_particles_instance_group::remove_instance(rl_corona_instance* instance_p)
{
    m_coronas.remove(instance_p);
}

// 0x0054CED0 (:79): every instance takes the group's tint and opacity; the visible ones with particles are queued for
// the deferred particle passes. The coronas follow.
void rl_renderable_particles_instance_group::render(rl_renderer& renderer, rl_camera& camera, unsigned int flags)
{
    rl_renderable_particles_instance* instance_p = m_instances.first();
    while (instance_p != nullptr)
    {
        const int num_iterative = instance_p->get_particle_iterative_list().size();
        instance_p->set_effect_material_tint(m_effect_material_tint);
        instance_p->set_opacity(m_opacity);
        if ((0 < num_iterative || 0 < instance_p->get_particle_parametric_list().size()) && instance_p->get_is_visible())
        {
            rl_particle_renderer::get_instance()->render_for_deferred(instance_p);
        }
        instance_p = instance_p->next;
        if (instance_p == m_instances.first())
        {
            break;
        }
    }
    rl_corona_instance* corona_p = m_coronas.first();
    while (corona_p != nullptr)
    {
        corona_p->m_opacity = m_opacity;
        corona_p->render();
        corona_p = corona_p->next;
        if (corona_p == m_coronas.first())
        {
            break;
        }
    }
}

// 0x00543B00 (.h:39): not batched.
void rl_renderable_particles_instance_group::add_to_batch(rl_batched_pass* pass_p, const rl_camera* pCamera, rl_deferred_batch_info* def_ids)
{
}

// 0x004C4D50 (:107)
void rl_renderable_particles_instance_group::add_to_batch(rl_particle_batched_pass* pass_p)
{
    keen::debug::nativePrintArgs("add_to_batch not implemented\n");
}

// 0x00743B50 (.h:66)
const linked_list_p<rl_corona_instance>& rl_renderable_particles_instance_group::get_corona_list()
{
    return m_coronas;
}

// 0x00C9E230 (.h:77)
void rl_renderable_particles_instance_group::set_effect_material_tint(const rl_color_float& effect_material_tint)
{
    m_effect_material_tint = effect_material_tint;
}
