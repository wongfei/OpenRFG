// Original: volition/render_lib/code/rl_renderable_particles.h
// Implementation: volition/render_lib/code/rl_renderable_particles.cpp
//
// rl_renderable_particles, a particle system renderable.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_particle.h"

class matrix;
class mempool;
class rl_class_factory_info;
struct rl_particle_material_data;

// size 0x54
class rl_renderable_particles : public rl_base_object
{
public:
    enum flags_e
    {
        FLAGS_ROTATION_ENABLED = 1,
        FLAGS_LOCAL_SPACE_EMISSION = 2,
        FLAGS_REVERSE_RENDER_ORDER = 4,
        FLAGS_IS_ITERATIVE = 8,
        FLAGS_IS_COLOR_CORRECTION_ENABLED = 64,
    };

protected:
    rl_renderable_particles(); // 0x004C4BF0 (:9)
    virtual ~rl_renderable_particles(); // vtbl+0x00 0x004C4CA0 (:25)
private:
    static const pool_list<rl_renderable_particles*>& get_object_pool(); // 0x004BD490 (.h:23)
public:
    static void set_objects_max(int objects_max); // 0x007C1EE0 (.h:23)
    static int get_objects_max(); // 0x004BD4A0 (.h:23)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00515820 (:6)
    static rl_renderable_particles* create_placement(rl_renderable_particles* _data_p); // 0x004F1700 (:6)
    static rl_renderable_particles* create(); // 0x0052CEB0 (:6)
    static bool can_create(); // 0x004F1720 (:6)
    virtual void release(); // vtbl+0x04 0x00515910 (:6)
    rl_particle_material_data* get_material() const; // 0x004BD4B0 (.h:112)
    void set_material(rl_particle_material_data* material_p); // 0x00C9E9A0 (.h:117)
    rl_particle_base::render_type get_particle_type() const; // 0x004BD4C0 (.h:122)
    void set_particle_type(rl_particle_base::render_type particle_type); // 0x00C9E9B0 (.h:127)
    bool get_is_rotation_enabled() const; // 0x004BD4D0 (.h:132)
    void set_is_rotation_enabled(bool is_rotation_enabled); // 0x00C9E9C0 (.h:137)
    bool get_local_space_emission() const; // 0x004BD4E0 (.h:147)
    void set_local_space_emission(bool local_space_emission); // 0x00C9E9E0 (.h:152)
    bool get_is_reverse_render_order() const; // 0x004BD4F0 (.h:163)
    void set_is_reverse_render_order(bool is_reverse_render_order); // 0x00C9EA00 (.h:168)
    int get_tail_length() const; // 0x004BD500 (.h:178)
    void set_tail_length(int tail_length); // 0x00C9EA20 (.h:183)
    bool get_is_iterative() const; // 0x004BD510 (.h:188)
    void set_is_iterative(bool is_iterative); // 0x00C9EA30 (.h:193)
    bool get_is_always_on_top() const; // 0x004C4CE0 (:58)
    void set_is_always_on_top(bool is_always_on_top); // 0x004C4CF0 (:63)
    bool get_is_soft_render() const; // 0x004C4D10 (:73)
    void set_is_soft_render(bool is_soft_render); // 0x004C4D20 (:78)
    bool get_texture_anim_random_frames() const; // 0x004BD520 (.h:203)
    void set_texture_anim_random_frames(bool texture_anim_random_frames); // 0x00C9EA50 (.h:208)
    bool get_is_color_correction_enabled() const;
    void set_is_color_correction_enabled(bool is_color_correction_enabled); // 0x00C9EA60 (.h:218)
    const matrix* get_color_correction_matrix() const; // 0x004C4CB0 (:30)
    matrix* get_color_correction_matrix_data() const; // 0x004C4CC0 (:39)
    void set_color_correction_matrix(matrix* color_correction_matrix_p); // 0x004C4CD0 (:49)
    float get_lod_large_particle_fade_min() const; // 0x004BD530 (.h:228)
    void set_lod_large_particle_fade_min(float lod_fade_min); // 0x00C9EA80 (.h:233)
    float get_lod_large_particle_fade_max() const; // 0x004BD540 (.h:239)
    void set_lod_large_particle_fade_max(float lod_fade_max); // 0x00C9EA90 (.h:244)
    float get_effect_material_percent() const; // 0x004BD550 (.h:250)
    void set_effect_material_percent(float effect_material_percent); // 0x00C9EAA0 (.h:255)
    float get_self_illum() const;
    void set_self_illum(float self_illum); // 0x00C9EAB0 (.h:265)
    const rl_color_float& get_ambient_color() const; // 0x004BD560 (.h:271)
    void set_ambient_color(const rl_color_float& ambient_color); // 0x00C9EAC0 (.h:276)
    const rl_color_float& get_diffuse_color() const; // 0x004BD570 (.h:282)
    void set_diffuse_color(const rl_color_float& diffuse_color); // 0x00C9EAE0 (.h:287)
    float get_soft_thickness() const; // 0x004BD580 (.h:292)
    void set_soft_thickness(float soft_thickness); // 0x00C9EB00 (.h:297)

    static const unsigned int m___class_id; // 0x012BED2C
    static rl_class_factory_info ms_class_factory_info; // 0x015150BC
protected:
    static pool_list<rl_renderable_particles*> ms_object_pool; // 0x015150EC
private:
    // vfxlib's vfx_effect_template::init sets the render state of each emitter's renderable directly (friends are not
    // recorded in rfg.pdb).
    friend class vfx_effect_template;
    // vfxlib's vfx_effect::update_render reads the random-frames flag directly (the getter is inlined there).
    friend class vfx_effect;
    unsigned int m_flags; // 0x08
    rl_particle_base::render_type m_particle_type; // 0x0C
    rl_particle_material_data* m_material_p; // 0x10
    int m_tail_length; // 0x14
    bool m_is_iterative; // 0x18
    bool m_texture_anim_random_frames; // 0x19
    matrix* m_color_correction_matrix_p; // 0x1C
    float m_lod_large_particle_fade_min; // 0x20
    float m_lod_large_particle_fade_max; // 0x24
    float m_effect_material_percent; // 0x28
    float m_self_illum; // 0x2C
    rl_color_float m_ambient_color; // 0x30
    rl_color_float m_diffuse_color; // 0x40
    float m_soft_thickness; // 0x50
};
