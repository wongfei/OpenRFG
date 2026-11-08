// Original: volition/render_lib/code/rl_renderer_metrics.cpp
//
// Partial: the class factory members, constructor, destructor and the header-inline setters seen so far.

#include "volition/render_lib/code/rl_renderer_metrics.h"

#include "volition/render_lib/code/rl_factory_rtti.h"

// :4
// <rl_renderer_metrics::release> 0x004C5CC0 (:4)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_renderer_metrics)

// 0x004C5CE0 (:7)
// Only the particle, occlusion, warning and draw-prims counters are cleared.
rl_renderer_metrics::rl_renderer_metrics()
{
    m_particle_batches = 0;
    m_particle_batch_average = 0;
    m_occlusion_accuracy_total_insts = 0;
    m_occlusion_accuracy_visible_insts = 0;
    m_num_occlusion_queries = 0;
    m_warning_flags = 0;
    m_num_dps_per_group_vals = 0;
    m_total_dps_per_group = 0;
    m_max_dps_per_group = 0;
}

// 0x004C5D10 (:18)
rl_renderer_metrics::~rl_renderer_metrics()
{
}

// 0x004BA250 (.h:72)
void rl_renderer_metrics::reset_num_draw_calls()
{
    m_num_draw_calls = 0;
}

// 0x004BA260 (.h:77)
void rl_renderer_metrics::set_num_dynamic_indices(int num_dynamic_indices)
{
    m_num_dynamic_indices = num_dynamic_indices;
}

// 0x004BA270 (.h:80)
void rl_renderer_metrics::set_num_dynamic_vert_bytes(int num_dynamic_vert_bytes)
{
    m_num_dynamic_vert_bytes = num_dynamic_vert_bytes;
}

// 0x004BA280 (.h:83)
void rl_renderer_metrics::set_num_dynamic_inst_bytes(int num_dynamic_inst_bytes)
{
    m_num_dynamic_inst_bytes = num_dynamic_inst_bytes;
}

// 0x004BA290 (.h:104)
void rl_renderer_metrics::reset_num_clone_mesh_widgets()
{
    m_num_clone_mesh_widgets = 0;
}

// 0x004BA2A0 (.h:108)
void rl_renderer_metrics::reset_num_clone_mesh_vertices()
{
    m_num_clone_mesh_vertices = 0;
}

// 0x004BA2B0 (.h:115)
void rl_renderer_metrics::reset_num_gpu_vis_entries()
{
    m_num_gpu_vis_entries = 0;
}

// 0x004BA2C0 (.h:159)
void rl_renderer_metrics::set_warning_flag(int flag)
{
    m_warning_flags |= flag;
}

// 0x004BA2D0 (.h:166)
void rl_renderer_metrics::clear_warning_flags()
{
    m_warning_flags = 0;
}

// 0x004BA2E0 (.h:177)
void rl_renderer_metrics::set_particle_batches(int particle_batches)
{
    m_particle_batches = particle_batches;
}

// 0x004BA2F0 (.h:188)
void rl_renderer_metrics::set_particle_batch_average(int particle_batch_average)
{
    m_particle_batch_average = particle_batch_average;
}

// 0x004BA300 (.h:212)
void rl_renderer_metrics::reset_average_draw_prims_per_group()
{
    m_num_dps_per_group_vals = 0;
    m_total_dps_per_group = 0;
    m_max_dps_per_group = 0;
}
