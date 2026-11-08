// Original: volition/vfxlib/code/vfx_particle_system_emitter_instance.h
// Implementation: volition/vfxlib/code/vfx_particle_system_emitter_instance.cpp
//
// An emitter instance of an effect: its emitter, transform, emission accumulator, the particles it owns, its filter
// instances and render metrics. Most methods are inline in vfx_particle_system_emitter_instance.inl.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

class mempool;
class rl_class_factory_info;
class rl_color_float;
class rl_renderable_instance;
class rl_renderable_particles_instance;
class vfx_particle_system_emitter;
class vfx_particle_system_filter_instance;
class object_effect;
class rl_renderer;
struct tracer;
enum weapon_tracer_type; // rfg's tracers.h, for the tracers' friend declarations (MSVC opaque enum, as in the Havok headers)

// size 0x170
class vfx_particle_system_emitter_instance : public rl_base_object
{
public:
    typedef void (*update_callback)(vfx_particle_system_emitter_instance*, void*);
    typedef void (*particle_create_callback)(vfx_particle_iterative*, vfx_particle_system_emitter_instance*, void*, bool);

    // size 0x10
    struct metrics
    {
        int iteration_time; // 0x00
        int spawning_time; // 0x04
        int simulation_time; // 0x08
        int particles_render_count; // 0x0C
    };

protected:
    vfx_particle_system_emitter_instance(); // 0x00CAFF00 (:54)
    virtual ~vfx_particle_system_emitter_instance(); // vtbl+0x00 0x00CB5EB0 (:59)
private:
    static const pool_list<vfx_particle_system_emitter_instance*>& get_object_pool(); // 0x00C9DAA0 (.h:19)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9DAB0 (.h:19)
    static int get_objects_used();
    static int get_objects_free(); // 0x006E6F70 (.h:19)
    static void factory_init(mempool& factory_mempool); // 0x00CAFD80 (:17)
    static vfx_particle_system_emitter_instance* create_placement(vfx_particle_system_emitter_instance* _data_p); // 0x00CB5E90 (:17)
    static vfx_particle_system_emitter_instance* create(); // 0x00CB5E30 (:17)
    static bool can_create(); // 0x00CA4600 (:17)
    virtual void release(); // vtbl+0x04 0x00CAFE80 (:17)
    vfx_particle_system_emitter_instance* get_prev() const;
    void set_prev(vfx_particle_system_emitter_instance*);
    vfx_particle_system_emitter_instance* get_next() const;
    void set_next(vfx_particle_system_emitter_instance*);
    vfx_particle_system_emitter_instance* get_emit_node_prev() const;
    void set_emit_node_prev(vfx_particle_system_emitter_instance*);
    vfx_particle_system_emitter_instance* get_emit_node_next() const;
    void set_emit_node_next(vfx_particle_system_emitter_instance*);
    const metrics& get_metrics() const;
    void init(vfx_particle_system_emitter* emitter_p); // 0x00CB5F10 (:91)
    void finalize(); // 0x00CB00E0 (:79)
    void reset(); // 0x00CB0130 (:119)
    int update(float delta_time, float anim_time); // 0x00CB6090 (:292)
    void copy_particles(rl_renderable_particles_instance* renderable_instance_p) const; // 0x00CB0350 (:1112)
    bool get_update_parametric() const; // 0x00C9DBC0 (vfx_particle_system_emitter_instance.inl:4)
    void set_update_parametric(bool update_parametric); // 0x00C9DBD0 (vfx_particle_system_emitter_instance.inl:9)
    void set_emission_enabled(bool _emission_enabled); // 0x006E1870 (.h:49)
    bool get_emission_enabled();
    void set_emitter_template(vfx_particle_system_emitter*);
    vfx_particle_system_emitter* get_emitter(); // 0x005D1880 (.h:53)
    linked_list_p<vfx_particle_iterative>* get_particle_iterative_list_shared();
    linked_list_p<vfx_particle_parametric>* get_particle_parametric_list_shared();
    linked_list_p<vfx_particle_iterative>& get_particle_iterative_list(); // 0x005D1890 (.h:56)
    linked_list_p<vfx_particle_parametric>& get_particle_parametric_list(); // 0x005D18A0 (.h:57)
    float get_last_spawn_time(); // 0x00C9DAC0 (.h:58)
    float get_current_elapsed_time(); // 0x00C9DAD0 (.h:59)
    float get_current_anim_time(); // 0x005D18B0 (.h:60)
    void set_tail_spawn_count(int);
    int get_tail_spawn_count(); // 0x00C9DAE0 (.h:62)
    const matrix43& get_current_world_matrix(); // 0x005D18C0 (.h:66)
    const matrix& get_current_world_rotation(); // 0x00C9DAF0 (.h:67)
    const vector& get_current_world_position(); // 0x005D18D0 (.h:68)
    const vector& get_current_spawn_position(); // 0x00C9DB00 (.h:69)
    const vector& get_previous_spawn_position() const;
    void set_previous_spawn_position(const vector&);
    const matrix& get_parent_world_rotation(); // 0x00C9DB10 (.h:75)
    const vector& get_parent_world_position(); // 0x00C9DB20 (.h:76)
    void set_parent_world_rotation(const matrix& parent_world_rotation); // 0x00C9DB30 (.h:77)
    void set_parent_world_position(const vector& parent_world_position); // 0x00C9DB80 (.h:78)
    rl_renderable_instance* get_parent_renderable_instance() const; // 0x00C9DBA0 (.h:80)
    void set_parent_renderable_instance(rl_renderable_instance* parent_renderable_instance); // 0x00C9DBB0 (.h:81)
    void get_local_position(vector& result, float elapsed_time); // 0x00C9DC40 (vfx_particle_system_emitter_instance.inl:181)
    void get_local_rotation(matrix& result, float elapsed_time); // 0x00C9DC90 (vfx_particle_system_emitter_instance.inl:188)
    void get_local_scale(vector& result, float elapsed_time); // 0x00C9DD30 (vfx_particle_system_emitter_instance.inl:195)
    void (*get_pre_update_cb() const)(vfx_particle_system_emitter_instance*, void*);
    void set_pre_update_cb(void (*)(vfx_particle_system_emitter_instance*, void*));
    void (*get_post_update_cb() const)(vfx_particle_system_emitter_instance*, void*);
    void set_post_update_cb(void (*post_update_cb)(vfx_particle_system_emitter_instance*, void*)); // 0x00C9E050 (vfx_particle_system_emitter_instance.inl:239)
    void* get_update_cb_data() const;
    void set_update_cb_data(void* update_cb_data_p); // 0x00C9E060 (vfx_particle_system_emitter_instance.inl:250)
    void (*get_particle_create_cb() const)(vfx_particle_iterative*, vfx_particle_system_emitter_instance*, void*, bool);
    void set_particle_create_cb(void (*particle_create_cb)(vfx_particle_iterative*, vfx_particle_system_emitter_instance*, void*, bool)); // 0x00C9E070 (vfx_particle_system_emitter_instance.inl:261)
    void* get_particle_create_cb_data() const;
    void set_particle_create_cb_data(void* particle_create_cb_data); // 0x00C9E080 (vfx_particle_system_emitter_instance.inl:272)
    rl_renderable_particles_instance* get_renderable_particles_instance() const; // 0x006E1880 (vfx_particle_system_emitter_instance.inl:328)
    void set_renderable_particles_instance(rl_renderable_particles_instance* renderable_particles_instance_p); // 0x00C9E160 (vfx_particle_system_emitter_instance.inl:333)
    void add_filter(vfx_particle_system_filter_instance* filter_instance_p); // 0x00CB02D0 (:1093)
    void remove_filter(vfx_particle_system_filter_instance* filter_instance_p); // 0x00CB0330 (:1102)
    const matrix43& get_alternate_view() const; // 0x00C9E090 (vfx_particle_system_emitter_instance.inl:278)
    void set_alternate_view(const matrix43& alternate_view); // 0x00C9E0A0 (vfx_particle_system_emitter_instance.inl:283)
    bool get_has_alternate_view() const; // 0x00C9E100 (vfx_particle_system_emitter_instance.inl:289)
    void set_has_alternate_view(bool has_alternate_view); // 0x00C9E110 (vfx_particle_system_emitter_instance.inl:294)
    int get_filter_count() const; // 0x00746120 (vfx_particle_system_emitter_instance.inl:300)
    bool get_is_visible() const; // 0x00C9E120 (vfx_particle_system_emitter_instance.inl:306)
    void set_is_visible(bool);
    float get_texture_anim_rate() const; // 0x00C9E130 (vfx_particle_system_emitter_instance.inl:317)
    void set_texture_anim_rate(float texture_anim_rate); // 0x00C9E140 (vfx_particle_system_emitter_instance.inl:322)
    const rl_color_float& get_color_start(); // 0x00C9DBE0 (vfx_particle_system_emitter_instance.inl:14)
    const rl_color_float& get_color_end(); // 0x00C9DBF0 (vfx_particle_system_emitter_instance.inl:19)
    const rl_color_float& get_color_start_variance();
    const rl_color_float& get_color_end_variance();
    const rl_color_float& get_color_mid_A(); // 0x00C9DC00 (vfx_particle_system_emitter_instance.inl:34)
    const rl_color_float& get_color_mid_B(); // 0x00C9DC10 (vfx_particle_system_emitter_instance.inl:39)
    const rl_color_float& get_color_mid_A_variance();
    const rl_color_float& get_color_mid_B_variance();
    float get_angular_speed_start(float elapsed_time); // 0x00CA3650 (vfx_particle_system_emitter_instance.inl:54)
    float get_angular_speed_end(float elapsed_time); // 0x00CA3690 (vfx_particle_system_emitter_instance.inl:59)
    float get_angular_speed_start_variance(); // 0x00C9DC20 (vfx_particle_system_emitter_instance.inl:64)
    float get_angular_speed_end_variance(); // 0x00C9DC30 (vfx_particle_system_emitter_instance.inl:69)
private:
    float get_emission_rate(); // 0x00CA4710 (:272)
public:
    float get_emission_rate(float);
    float get_speed(float);
    float get_speed_variance();
    float get_size_start(float elapsed_time); // 0x00CA36D0 (vfx_particle_system_emitter_instance.inl:89)
    float get_size_end(float elapsed_time); // 0x00CA3720 (vfx_particle_system_emitter_instance.inl:94)
    float get_size_start_variance(float elapsed_time); // 0x00CA3770 (vfx_particle_system_emitter_instance.inl:99)
    float get_size_end_variance(float elapsed_time); // 0x00CA37C0 (vfx_particle_system_emitter_instance.inl:104)
    float get_size_mid_A(float elapsed_time); // 0x00CA3810 (vfx_particle_system_emitter_instance.inl:109)
    float get_size_mid_B(float elapsed_time); // 0x00CA3860 (vfx_particle_system_emitter_instance.inl:114)
    float get_size_mid_A_variance(float elapsed_time); // 0x00CA38B0 (vfx_particle_system_emitter_instance.inl:119)
    float get_size_mid_B_variance(float elapsed_time); // 0x00CA3900 (vfx_particle_system_emitter_instance.inl:124)
    float get_length_start(float elapsed_time); // 0x00CA3950 (vfx_particle_system_emitter_instance.inl:129)
    float get_length_end(float elapsed_time); // 0x00CA3990 (vfx_particle_system_emitter_instance.inl:134)
    float get_length_start_variance(float elapsed_time); // 0x00CA39D0 (vfx_particle_system_emitter_instance.inl:139)
    float get_length_end_variance(float elapsed_time); // 0x00CA3A10 (vfx_particle_system_emitter_instance.inl:144)
    float get_length_mid_A(float elapsed_time); // 0x00CA3A50 (vfx_particle_system_emitter_instance.inl:149)
    float get_length_mid_B(float elapsed_time); // 0x00CA3A90 (vfx_particle_system_emitter_instance.inl:154)
    float get_length_mid_A_variance(float elapsed_time); // 0x00CA3AD0 (vfx_particle_system_emitter_instance.inl:159)
    float get_length_mid_B_variance(float elapsed_time); // 0x00CA3B10 (vfx_particle_system_emitter_instance.inl:164)
    bool get_is_emitted_node() const; // 0x00C9E170 (vfx_particle_system_emitter_instance.inl:339)
    void set_is_emitted_node(bool update_parent_transform); // 0x00C9E180 (vfx_particle_system_emitter_instance.inl:344)
    vfx_particle_iterative* get_emit_node_parent() const;
    void set_emit_node_parent(vfx_particle_iterative* emit_node_parent_p); // 0x00C9E190 (vfx_particle_system_emitter_instance.inl:355)
    unsigned short get_sort_priority() const; // 0x00C9E1A0 (vfx_particle_system_emitter_instance.inl:361)
    void set_sort_priority(unsigned short sort_priority); // 0x00C9E1B0 (vfx_particle_system_emitter_instance.inl:366)
    int get_emit_number() const;
    void set_emit_number(int);
private:
    void update_internal_transforms(); // 0x00C9FCB0 (:248)
    int spawn_internal(int num, float elapsed_time); // 0x00CA4860 (:697)
    void release_tails(); // 0x00CB0030 (:65)
    void compute_iteration(float elapsed_time, float delta_time); // 0x00CB5F80 (:155)
    void create_particles(int num_particles, float elapsed_time); // 0x00CB0280 (:666)
    void apply_filter_list(const base_array<vfx_particle_system_filter_instance*>& filter_list, float delta_time); // 0x00CA7420 (:1068)
    void apply_interactors(float delta_time); // 0x00CA7460 (:1076)
    void apply_deflectors(float delta_time); // 0x00C9FF30 (:1084)
    void param_particle_death_check(); // 0x00CA4610 (:223)
    rl_color_float compute_variant_color(const rl_color_float& original_color, const rl_color_float& variant_color, bool use_value); // 0x00C9DD80 (vfx_particle_system_emitter_instance.inl:202)

public:
    static const unsigned int m___class_id; // 0x012C7084
    static rl_class_factory_info ms_class_factory_info; // 0x016F0FC8
protected:
    static pool_list<vfx_particle_system_emitter_instance*> ms_object_pool; // 0x016F0FF8
public:
    vfx_particle_system_emitter_instance* prev; // 0x08
    vfx_particle_system_emitter_instance* next; // 0x0C
    vfx_particle_system_emitter_instance* emit_node_prev; // 0x10
    vfx_particle_system_emitter_instance* emit_node_next; // 0x14
private:
    // vfx_effect sets the parent transform, callbacks and interactors and reads the render state directly (friends are
    // not recorded in rfg.pdb).
    friend class vfx_effect;
    // rfg's object_effect_profile::update walks the particles and reads the emitter and interactors.
    friend class object_effect_profile;
    // rfg's tracers.cpp primes the tracer effect's emitters and moves its particles directly (friends are not recorded
    // in rfg.pdb).
    friend void tracers_create(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void curved_tracers_create(unsigned int owner, const vector& start_pos, const vector& inner_spline_pos, const vector& inner_spline_pos2, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void tracers_process();
    // rfg's dust.cpp (dust_update_render) hides the skirting effect's particles and puts decals at them (friends are not
    // recorded in rfg.pdb).
    friend void dust_update_render(void*, rl_renderer*);
    friend void expanding_tracer_particle_spread(object_effect* oe, tracer* tp, vector new_pos);
    matrix43 current_world_matrix; // 0x18
    vector current_spawn_position; // 0x48
    vector previous_spawn_position; // 0x54
    static const vector SCALE_IDENTITY_VECTOR; // 0x016F1010
    void (*m_pre_update_cb)(vfx_particle_system_emitter_instance*, void*); // 0x60
    void (*m_post_update_cb)(vfx_particle_system_emitter_instance*, void*); // 0x64
    void* m_update_cb_data_p; // 0x68
    void (*m_particle_create_cb)(vfx_particle_iterative*, vfx_particle_system_emitter_instance*, void*, bool); // 0x6C
    void* m_particle_create_cb_data; // 0x70
    vfx_particle_system_emitter* m_emitter_p; // 0x74
    bool m_has_alternate_view; // 0x78
    int tail_spawn_count; // 0x7C
    float current_elapsed_time; // 0x80
    float m_texture_anim_rate; // 0x84
    bool m_is_visible; // 0x88
    bool m_update_parametric; // 0x89
    bool m_last_emission_rate_non_zero; // 0x8A
    linked_list_p<vfx_particle_iterative> m_particle_iterative_list; // 0x8C
    linked_list_p<vfx_particle_parametric> m_particle_parametric_list; // 0x94
    linked_list_p<vfx_particle_iterative>* m_particle_iterative_list_shared_p; // 0x9C
    linked_list_p<vfx_particle_parametric>* m_particle_parametric_list_shared_p; // 0xA0
    linked_list_p<vfx_particle_iterative::tail_data>* particle_tails_list_shared; // 0xA4
    matrix43 m_parent_transform; // 0xA8
    rl_renderable_instance* m_parent_renderable_instance_p; // 0xD8
    rl_renderable_particles_instance* m_renderable_particles_instance_p; // 0xDC
    int num_alive; // 0xE0
    int num_dead; // 0xE4
    bool emission_enabled; // 0xE8
    float last_spawn_time; // 0xEC
    float current_anim_time; // 0xF0
    bool m_first_update; // 0xF4
    bool m_emission_rate_calculated; // 0xF5
    float m_emission_rate_cached; // 0xF8
    int m_emit_number; // 0xFC
    unsigned int m_update_tail_count; // 0x100
    matrix43 m_alternate_view; // 0x104
    bool m_is_emitted_node; // 0x134
    vfx_particle_iterative* m_emit_node_parent_p; // 0x138
    unsigned short m_sort_priority; // 0x13C
    metrics m_metrics; // 0x140
    farray<vfx_particle_system_filter_instance*,4> m_interactors; // 0x150
    static float ms_random_float; // 0x03A23C90
    static vector ms_random_vector; // 0x03A23C94
};

#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.inl"
