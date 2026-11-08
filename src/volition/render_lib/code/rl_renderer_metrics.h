// Original: volition/render_lib/code/rl_renderer_metrics.h
// Implementation: volition/render_lib/code/rl_renderer_metrics.cpp
//
// rl_renderer_metrics, per-renderer counters (draw calls, primitives, state changes).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"

class rl_class_dynamic_info;

// size 0x60
class rl_renderer_metrics : public rl_base_object
{
protected:
    rl_renderer_metrics(); // 0x004C5CE0 (:7)
    virtual ~rl_renderer_metrics(); // vtbl+0x00 0x004C5D10 (:18)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_renderer_metrics* create(); // 0x004F31D0 (:4)
    static bool can_create(); // 0x004C5CB0 (:4)
    virtual void release(); // vtbl+0x04 0x004C5CC0 (:4)
    int get_particle_batches() const;
    void set_particle_batches(int particle_batches); // 0x004BA2E0 (.h:177)
    int get_particle_batch_average() const;
    void set_particle_batch_average(int particle_batch_average); // 0x004BA2F0 (.h:188)
    int get_warning_flags() const;
    void set_warning_flag(int flag); // 0x004BA2C0 (.h:159)
    void clear_warning_flags(); // 0x004BA2D0 (.h:166)
    float get_average_draw_prims_per_group() const;
    int get_max_draw_prims_per_group() const;
    void add_draw_prims_per_group_val(int);
    void reset_average_draw_prims_per_group(); // 0x004BA300 (.h:212)
    float get_cpu_render_time() const;
    void set_cpu_render_time(float);
    float get_gpu_render_time() const;
    void set_gpu_render_time(float);
    float get_cpu_scene_render_time() const;
    void set_cpu_scene_render_time(float);
    int get_num_draw_calls() const;
    void reset_num_draw_calls(); // 0x004BA250 (.h:72)
    void increment_num_draw_calls(int);
    int get_num_dynamic_indices() const;
    void set_num_dynamic_indices(int num_dynamic_indices); // 0x004BA260 (.h:77)
    int get_num_dynamic_vert_bytes() const;
    void set_num_dynamic_vert_bytes(int num_dynamic_vert_bytes); // 0x004BA270 (.h:80)
    int get_num_dynamic_inst_bytes() const;
    void set_num_dynamic_inst_bytes(int num_dynamic_inst_bytes); // 0x004BA280 (.h:83)
    unsigned int get_num_entities_in_frustum() const;
    void set_num_entities_in_frustum(unsigned int);
    unsigned int get_num_entities_occluded() const;
    void set_num_entities_occluded(unsigned int);
    unsigned int get_num_entities_lod_offed() const;
    void set_num_entities_lod_offed(unsigned int);
    unsigned int get_occlusion_accuracy_total_insts() const;
    void set_occlusion_accuracy_total_insts(unsigned int);
    unsigned int get_occlusion_accuracy_visible_insts() const;
    void set_occlusion_accuracy_visible_insts(unsigned int);
    int get_num_clone_mesh_widgets() const;
    void reset_num_clone_mesh_widgets(); // 0x004BA290 (.h:104)
    void increment_num_clone_mesh_widgets(int);
    int get_num_clone_mesh_vertices() const;
    void reset_num_clone_mesh_vertices(); // 0x004BA2A0 (.h:108)
    void increment_num_clone_mesh_vertices(int);
    int get_num_occlusion_queries() const;
    void increment_num_occlusion_queries(int);
    int get_num_gpu_vis_entries() const;
    void reset_num_gpu_vis_entries(); // 0x004BA2B0 (.h:115)
    void increment_num_gpu_vis_entries(int);

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x015151E4
private:
    unsigned int m_warning_flags; // 0x08
    int m_particle_batches; // 0x0C
    int m_particle_batch_average; // 0x10
    volatile unsigned int m_num_dps_per_group_vals; // 0x14
    volatile unsigned int m_max_dps_per_group; // 0x18
    volatile unsigned int m_total_dps_per_group; // 0x1C
    float m_cpu_render_time; // 0x20
    float m_gpu_render_time; // 0x24
    float m_cpu_scene_render_time; // 0x28
    int m_num_draw_calls; // 0x2C
    int m_num_dynamic_indices; // 0x30
    int m_num_dynamic_vert_bytes; // 0x34
    int m_num_dynamic_inst_bytes; // 0x38
    unsigned int m_num_entities_in_frustum; // 0x3C
    unsigned int m_num_entities_occluded; // 0x40
    unsigned int m_num_entities_lod_offed; // 0x44
    unsigned int m_occlusion_accuracy_total_insts; // 0x48
    unsigned int m_occlusion_accuracy_visible_insts; // 0x4C
    volatile unsigned int m_num_clone_mesh_widgets; // 0x50
    volatile unsigned int m_num_clone_mesh_vertices; // 0x54
    volatile unsigned int m_num_occlusion_queries; // 0x58
    volatile unsigned int m_num_gpu_vis_entries; // 0x5C
};
