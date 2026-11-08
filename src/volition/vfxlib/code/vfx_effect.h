// Original: volition/vfxlib/code/vfx_effect.h
// Implementation: volition/vfxlib/code/vfx_effect.cpp
//
// An effect instance: its template, transform, elapsed time, emitter instances, mesh instances, dynamic lights and
// coronas, alpha fade, material tint, world bounds and the expression variables the game sets (set_variable_value is
// in vfx_effect_expressions.cpp). alignas(16) is inferred: the PDB size (0xD0) pads the last member's end (0xC4) to 16
// bytes, which only a 16-byte class alignment explains (the PDB does not record alignment; the layout check found it).
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vfxlib/code/vfx_renderable_filter_instance.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class rl_renderable_particles_instance_group;
class rl_renderer;
class vfx_effect_template;
class vfx_particle_iterative;
class vfx_particle_system_emitter;
class object_effect;
struct tracer;
enum weapon_tracer_type; // rfg's tracers.h, for the tracers' friend declarations (MSVC opaque enum, as in the Havok headers)

// size 0xD0
class alignas(16) vfx_effect
{
public:
    enum render_flags
    {
        RENDER_FLAGS_NONE = 0,
        RENDER_FLAGS_MESHES_ENABLED = 1,
        RENDER_FLAGS_PARTICLES_ENABLED = 2,
        RENDER_FLAGS_UPDATE_PARTITION_SYSTEM = 4,
        RENDER_FLAGS_CORONAS_ENABLED = 8,
        RENDER_FLAGS_ALL = -1,
    };

    typedef int handle;

    // size 0x18
    struct metrics
    {
        int emitter_update_time; // 0x00
        int emitter_update_spawning_time; // 0x04
        int emitter_update_simulation_time; // 0x08
        int emitter_update_iteration_time; // 0x0C
        int lights_update_time; // 0x10
        int render_update_time; // 0x14
    };

protected:
    vfx_effect(); // 0x00CA3B50 (:53)
    ~vfx_effect(); // 0x00CA3C60 (:64)
public:
    static vfx_effect* create(vfx_effect_template* template_p); // 0x00CBA670 (:69)
    void release(); // 0x00CB1BC0 (:396)
    void update(float delta_time); // 0x00CB8210 (:485)
    void increment_time(float elapsed); // 0x00741F60 (.h:57)
    void enable_emission(bool enabled); // 0x00CA3D80 (:569)
    void enable_coronas(bool enabled); // 0x00C9EE70 (:579)
    void set_handle(int insthandle); // 0x00C9D2A0 (.h:61)
    int get_handle() const; // 0x00C9D2B0 (.h:62)
    void set_template(vfx_effect_template* template_p); // 0x00C9D2C0 (.h:63)
    const vfx_effect_template* get_template() const; // 0x005D1850 (.h:64)
    void set_position(const vector& position); // 0x00741F90 (.h:65)
    const vector& get_position() const; // 0x00B23950 (.h:66)
    void set_rotation(const matrix& rotation); // 0x00741FB0 (.h:67)
    const matrix& get_rotation() const; // 0x00C9D2D0 (.h:68)
    const rl_aabb& get_world_aabb() const;
    void set_world_aabb(const rl_aabb& world_aabb); // 0x00742040 (.h:165)
    const rl_sphere& get_world_bounding_sphere() const; // 0x00742070 (.h:172)
    void set_world_bounding_sphere(const rl_sphere& world_bounding_sphere); // 0x00742080 (.h:177)
    void set_elapsed_time(float elapsed); // 0x00741FF0 (.h:73)
    float get_elapsed_time() const; // 0x00742000 (.h:74)
    bool get_is_particle_system_active() const; // 0x00742010 (.h:75)
    int get_num_particle_systems() const;
    void set_use_alpha_fade(bool enable); // 0x00B23960 (.h:77)
    bool get_use_alpha_fade() const;
    void set_alpha_fade(float fade_amount); // 0x00742020 (.h:79)
    float get_alpha_fade() const;
    void set_render_flags(int);
    int get_render_flags() const;
    int get_num_particles();
    void update_render(float delta_time, bool is_visible, rl_renderer* pRenderer); // 0x00CB1F10 (:664)
    bool get_is_visible() const;
    void set_is_visible(bool);
    const linked_list_p<vfx_particle_system_emitter_instance>& get_emitters() const; // 0x005D1860 (.h:90)
    const linked_list_p<vfx_particle_system_filter_instance>& get_filters() const; // 0x005D1870 (.h:91)
    const linked_list_p<vfx_renderable_filter_instance>& get_mesh_filters() const;
    const linked_list_p<rl_renderable_instance>& get_mesh_instances() const; // 0x006C7D40 (.h:93)
    rl_renderable_particles_instance_group* get_renderable_particles_instance_group() const; // 0x00742030 (.h:95)
    void kill_lights(); // 0x00CA3CC0 (:450)
    int get_expression_count() const; // 0x007420A0 (.h:195)
    int get_variable_count(int expression_index) const; // 0x007420B0 (.h:200)
    unsigned int get_variable_handle(int expression_index, int variable_index) const; // 0x007476E0 (.h:206)
    void set_variable_value(int expression_index, int variable_index, float variable_value); // 0x00CA44C0 (vfx_effect_expressions.cpp:238)
    const rl_color_float& get_effect_material_tint() const; // 0x00C9EEA0 (:1207)
    void set_effect_material_tint(const rl_color_float& effect_material_tint); // 0x00C9EEB0 (:1212)
private:
    void init(); // 0x00CB9AC0 (:150)
    vfx_particle_system_emitter_instance* create_emitter(vfx_particle_system_emitter* emitter_p, int render_priority); // 0x00CB9920 (:86)
    void release_emitter(vfx_particle_system_emitter_instance* emitter_instance_p); // 0x00CA3C70 (:375)
    void kill_emitter(vfx_particle_system_emitter_instance* emitter_instance_p); // 0x00C9EE20 (:390)
    void update_dynamic_lights(); // 0x00CA3E60 (:1121)
    void update_expressions(); // 0x00CB1E20 (:461)
    void emit_node_emitters_spawn(vfx_particle_iterative* particle_p, vfx_particle_system_emitter_instance* this_p, bool birth); // 0x00CB8510 (:588)
    void emit_node_emitters_update(vfx_particle_system_emitter_instance* this_p); // 0x00CA3DB0 (:644)
    static void emit_node_emitters_spawn_cb(vfx_particle_iterative* particle_p, vfx_particle_system_emitter_instance* this_p, void* data_p, bool birth); // 0x00CB8780 (:638)
    static void emit_node_emitters_update_cb(vfx_particle_system_emitter_instance* this_p, void* data_p); // 0x00CA3E50 (:658)

public:
    static const int HANDLE_INVALID; // 0x012C7080
private:
    // rfg's grinder.cpp (weapon::grinder_update_render) walks the mesh instances (friends are not recorded in rfg.pdb).
    friend class weapon;
    // rfg's camera_view.cpp (camera_view_data::process_effects) sets up the scope effect's mesh instances.
    friend class camera_view_data;
    // rfg's object_effect sets the elapsed time, alpha fade, render flags and world bounds and reads the template
    // directly (the .h accessors are inlined there).
    friend class object_effect;
    // weapons.cpp's weapon_create_arc_effect reads the template.
    friend void weapon_create_arc_effect(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    friend void weapon_create_curved_tracer(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    // rfg's tracers.cpp primes the tracer effect's emitters and moves its particles directly (friends are not recorded
    // in rfg.pdb).
    friend void tracers_create(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void curved_tracers_create(unsigned int owner, const vector& start_pos, const vector& inner_spline_pos, const vector& inner_spline_pos2, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void tracers_process();
    // rfg's dust.cpp (dust_update_render) hides the skirting effect's particles and puts decals at them (friends are not
    // recorded in rfg.pdb).
    friend void dust_update_render(void*, rl_renderer*);
    friend void expanding_tracer_particle_spread(object_effect* oe, tracer* tp, vector new_pos);
    int m_instance_handle; // 0x00
    float m_elapsed_time; // 0x04
    vector m_position; // 0x08
    matrix m_rotation; // 0x14
    bool m_is_particle_system_active; // 0x38
    int num_particle_systems; // 0x3C
    int num_particles; // 0x40
    bool use_alpha_fade; // 0x44
    float alpha_fade; // 0x48
    bool m_is_visible; // 0x4C
    vfx_effect_template* m_effect_template_p; // 0x50
    int m_render_flags; // 0x54
    rl_aabb m_world_aabb; // 0x58
    rl_sphere m_world_bounding_sphere; // 0x70
    rl_color_float m_effect_material_tint; // 0x80
    float m_eps; // 0x90
    vfx_particle_system_emitter_instance* m_emitter_kill_list; // 0x94
    linked_list_p<vfx_particle_system_emitter_instance> m_emitters; // 0x98
    linked_list_p<vfx_particle_system_filter_instance> m_filters; // 0xA0
    linked_list_p<vfx_renderable_filter_instance> m_mesh_filters; // 0xA8
    linked_list_p<rl_renderable_instance> m_renderable_mesh_insts; // 0xB0
    linked_list_p<rl_light> m_lights; // 0xB8
    rl_renderable_particles_instance_group* m_renderable_particles_instance_group_p; // 0xC0
};

vfx_effect* vfx_effect_create(vfx_effect_template* effect_template_p); // 0x00CBA700 (:1220)
