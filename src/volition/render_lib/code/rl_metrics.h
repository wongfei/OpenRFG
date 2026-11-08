// Original: volition/render_lib/code/rl_metrics.h
// Implementation: volition/render_lib/code/rl_metrics.cpp
//
// rl_metrics, per-frame timings of the render phases (visibility, base pass, lighting, sort, deferred).
#pragma once

#include "volition/render_lib/code/rl_base_object.h"

class rl_class_dynamic_info;

// size 0x68
class rl_metrics : public rl_base_object
{
protected:
    rl_metrics(); // 0x004C3720 (:15)
    virtual ~rl_metrics(); // vtbl+0x00 0x004C3730 (:19)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_metrics* create(); // 0x004E99F0 (:11)
    static bool can_create(); // 0x004C36F0 (:11)
    virtual void release(); // vtbl+0x04 0x004C3700 (:11)
    void profile_start_visibility();
    void profile_end_visibility();
    int get_visibility_time() const;
    void profile_start_base_pass();
    void profile_end_base_pass();
    int get_base_pass_time() const;
    void profile_start_multipass_lighting();
    void profile_end_multipass_lighting();
    int get_multipass_lighting_time() const;
    void profile_start_sort();
    void profile_end_sort();
    int get_sort_time() const;
    void profile_start_deferred_sorted();
    void profile_end_deferred_sorted();
    int get_deferred_sorted_time() const;
    void profile_start_deferred_instanced();
    void profile_end_deferred_instanced();
    int get_deferred_instanced_time() const;
    void profile_start_total();
    void profile_end_total();
    int get_total_time() const;
    int get_potential_occluders_count() const;
    void set_potential_occluders_count(int);
    int get_actual_occluders_count() const;
    void set_actual_occluders_count(int);
    int get_occlusion_query_pixels_drawn() const;
    void set_occlusion_query_pixels_drawn(int);
    void profile_start_shadow_visibility();
    void profile_end_shadow_visibility();
    int get_shadow_visibility_time() const;
    void profile_start_shadow_pass();
    void profile_end_shadow_pass();
    int get_shadow_pass_time() const;
    void profile_start_overlay_pass();
    void profile_end_overlay_pass();
    int get_overlay_pass_time() const;
    int get_warning_flags() const;
    void set_warning_flag(int);
    void clear_warning_flags();

    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514E74
private:
    int m_start_visibility; // 0x08
    int m_visibility_time; // 0x0C
    int m_start_base_pass; // 0x10
    int m_base_pass_time; // 0x14
    int m_start_multipass_lighting; // 0x18
    int m_multipass_lighting_time; // 0x1C
    int m_start_sort; // 0x20
    int m_sort_time; // 0x24
    int m_start_deferred_sorted; // 0x28
    int m_deferred_sorted_time; // 0x2C
    int m_start_deferred_instanced; // 0x30
    int m_deferred_instanced_time; // 0x34
    int m_start_total; // 0x38
    int m_total_time; // 0x3C
    int m_potential_occluders_count; // 0x40
    int m_actual_occluders_count; // 0x44
    int m_occlusion_query_pixels_drawn; // 0x48
    int m_start_shadow_visibility; // 0x4C
    int m_shadow_visibility_time; // 0x50
    int m_start_shadow_pass; // 0x54
    int m_shadow_pass_time; // 0x58
    int m_start_overlay_pass; // 0x5C
    int m_overlay_pass_time; // 0x60
    unsigned int m_warning_flags; // 0x64
};
