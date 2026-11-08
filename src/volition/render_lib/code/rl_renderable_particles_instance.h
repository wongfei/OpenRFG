// Original: volition/render_lib/code/rl_renderable_particles_instance.h
// Implementation: volition/render_lib/code/rl_renderable_particles_instance.cpp
//
// rl_renderable_particles_instance, a placed particle system (inline methods in the .inl).
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_particle.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"

class mempool;
class rl_class_factory_info;
class rl_renderable_particles;

// size 0xDC
class rl_renderable_particles_instance : public rl_base_object
{
public:
    // size 0x110
    struct parametric_constant_data
    {
        parametric_constant_data(); // 0x004D9600

        rl_color_float m_color_start; // 0x00
        rl_color_float m_color_mid_A; // 0x10
        rl_color_float m_color_mid_B; // 0x20
        rl_color_float m_color_end; // 0x30
        vector m_acceleration_start; // 0x40
        float m_length_start; // 0x4C
        vector m_acceleration_start_variance; // 0x50
        float m_length_end; // 0x5C
        vector m_acceleration_end; // 0x60
        float m_length_mid_A_offset; // 0x6C
        vector m_acceleration_end_variance; // 0x70
        float m_length_mid_B_offset; // 0x7C
        float m_color_saturation_start; // 0x80
        float m_rotation_start; // 0x84
        float m_size_start; // 0x88
        float m_additive_key_start; // 0x8C
        float m_color_saturation_mid_A; // 0x90
        float m_length_mid_A; // 0x94
        float m_size_mid_A; // 0x98
        float m_additive_key_mid_A; // 0x9C
        float m_color_saturation_mid_B; // 0xA0
        float m_length_mid_B; // 0xA4
        float m_size_mid_B; // 0xA8
        float m_additive_key_mid_B; // 0xAC
        float m_color_saturation_end; // 0xB0
        float m_rotation_end; // 0xB4
        float m_size_end; // 0xB8
        float m_additive_key_end; // 0xBC
        float m_color_mid_A_offset; // 0xC0
        float m_color_mid_B_offset; // 0xC4
        float m_size_mid_A_offset; // 0xC8
        float m_size_mid_B_offset; // 0xCC
        float m_time; // 0xD0
        float m_is_local_space; // 0xD4
        float m_density; // 0xD8
        float m_density_variance; // 0xDC
        float m_rotation_start_variance; // 0xE0
        float m_rotation_end_variance; // 0xE4
        float m_size_start_variance; // 0xE8
        float m_size_mid_A_variance; // 0xEC
        float m_size_mid_B_variance; // 0xF0
        float m_size_end_variance; // 0xF4
        float m_texture_anim_rate; // 0xF8
        float m_texture_anim_random_enabled; // 0xFC
        float m_length_start_variance; // 0x100
        float m_length_mid_A_variance; // 0x104
        float m_length_mid_B_variance; // 0x108
        float m_length_end_variance; // 0x10C
    };

protected:
    rl_renderable_particles_instance(); // 0x00515AA0 (:17)
    virtual ~rl_renderable_particles_instance(); // vtbl+0x00 0x0052CF70 (:38)
private:
    static const pool_list<rl_renderable_particles_instance*>& get_object_pool(); // 0x004BD590 (.h:25)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BD5A0 (.h:25)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00515990 (:10)
    static rl_renderable_particles_instance* create_placement(rl_renderable_particles_instance*);
    static rl_renderable_particles_instance* create(); // 0x0052CF10 (:10)
    static bool can_create(); // 0x004F1750 (:10)
    virtual void release(); // vtbl+0x04 0x00543930 (:10)
    rl_renderable_particles_instance* get_prev() const;
    void set_prev(rl_renderable_particles_instance*);
    rl_renderable_particles_instance* get_next() const;
    void set_next(rl_renderable_particles_instance*);
    void render(); // 0x0054CC10 (:54)
    unsigned short get_sort_priority() const; // 0x004BD5B0 (rl_renderable_particles_instance.inl:2)
    void set_sort_priority(unsigned short sort_priority); // 0x00C9E250 (rl_renderable_particles_instance.inl:7)
    bool get_is_visible() const; // 0x004BD5C0 (rl_renderable_particles_instance.inl:13)
    void set_is_visible(bool is_visible); // 0x006E18C0 (rl_renderable_particles_instance.inl:18)
    rl_renderable_particles* get_renderable_particles() const; // 0x004BD5D0 (rl_renderable_particles_instance.inl:23)
    void set_renderable_particles(rl_renderable_particles* renderable_particles_p); // 0x0052CFE0 (:101)
    bool alloc_iterative_particles(int count); // 0x0054CC80 (:75)
    bool alloc_parametric_particles(int count); // 0x0054CD60 (:88)
    pool_list<rl_particle_iterative>& get_particle_iterative_list(); // 0x004BD5F0 (rl_renderable_particles_instance.inl:40)
    pool_list<rl_particle_parametric>& get_particle_parametric_list(); // 0x004BD600 (rl_renderable_particles_instance.inl:46)
    int get_tail_spawn_count() const; // 0x004BD5E0 (rl_renderable_particles_instance.inl:29)
    void set_tail_spawn_count(int tail_spawn_count); // 0x00C9E260 (rl_renderable_particles_instance.inl:34)
    bool get_break_link_chain() const;
    void set_break_link_chain(bool);
    const vector& get_local_emission_position() const; // 0x004BD610 (rl_renderable_particles_instance.inl:64)
    void set_local_emission_position(const vector& local_emission_position); // 0x00C9E270 (rl_renderable_particles_instance.inl:69)
    const matrix& get_local_emission_rotation() const; // 0x004BD620 (rl_renderable_particles_instance.inl:75)
    void set_local_emission_rotation(const matrix& local_emission_rotation); // 0x00C9E290 (rl_renderable_particles_instance.inl:80)
    const matrix43& get_alternate_view() const; // 0x004BD630 (rl_renderable_particles_instance.inl:86)
    void set_alternate_view(const matrix43& alternate_view); // 0x00C9E2D0 (rl_renderable_particles_instance.inl:91)
    bool get_has_alternate_view() const; // 0x004BD640 (rl_renderable_particles_instance.inl:97)
    void set_has_alternate_view(bool has_alternate_view); // 0x00C9E330 (rl_renderable_particles_instance.inl:102)
    float get_current_elapsed_time() const; // 0x004BD650 (rl_renderable_particles_instance.inl:108)
    void set_current_elapsed_time(float current_elapsed_time); // 0x00C9E340 (rl_renderable_particles_instance.inl:113)
    const unsigned char* get_parametric_constant_data() const; // 0x004BD660 (rl_renderable_particles_instance.inl:118)
    int get_num_parametric_constants() const; // 0x004BD670 (rl_renderable_particles_instance.inl:124)
    float get_parametric_time() const;
    void set_parametric_time(float parametric_time); // 0x00C9E350 (rl_renderable_particles_instance.inl:139)
    bool get_is_local_space() const; // 0x004BD680 (rl_renderable_particles_instance.inl:146)
    void set_is_local_space(bool is_local_space); // 0x00C9E370 (rl_renderable_particles_instance.inl:151)
    float get_density() const;
    void set_density(float density); // 0x00C9E3A0 (rl_renderable_particles_instance.inl:162)
    float get_density_variance() const;
    void set_density_variance(float density_variance); // 0x00C9E3C0 (rl_renderable_particles_instance.inl:173)
    const rl_color_float& get_color_start() const;
    void set_color_start(const rl_color_float& color_start); // 0x00C9E3E0 (rl_renderable_particles_instance.inl:194)
    const rl_color_float& get_color_mid_A() const;
    void set_color_mid_A(const rl_color_float& color_mid_A); // 0x00C9E400 (rl_renderable_particles_instance.inl:207)
    const rl_color_float& get_color_mid_B() const;
    void set_color_mid_B(const rl_color_float& color_mid_B); // 0x00C9E420 (rl_renderable_particles_instance.inl:220)
    const rl_color_float& get_color_end() const;
    void set_color_end(const rl_color_float& color_end); // 0x00C9E440 (rl_renderable_particles_instance.inl:233)
    float get_color_mid_A_offset() const;
    void set_color_mid_A_offset(float color_mid_A_offset); // 0x00C9E460 (rl_renderable_particles_instance.inl:246)
    float get_color_mid_B_offset() const;
    void set_color_mid_B_offset(float color_mid_B_offset); // 0x00C9E480 (rl_renderable_particles_instance.inl:259)
    const vector& get_acceleration_start() const;
    void set_acceleration_start(const vector& acceleration_start); // 0x00C9E760 (rl_renderable_particles_instance.inl:557)
    const vector& get_acceleration_end() const;
    void set_acceleration_end(const vector& acceleration_end); // 0x00C9E780 (rl_renderable_particles_instance.inl:570)
    const vector& get_acceleration_start_variance() const;
    void set_acceleration_start_variance(const vector& acceleration_start_variance); // 0x00C9E7A0 (rl_renderable_particles_instance.inl:583)
    const vector& get_acceleration_end_variance() const;
    void set_acceleration_end_variance(const vector& acceleration_end_variance); // 0x00C9E7C0 (rl_renderable_particles_instance.inl:596)
    float get_rotation_start() const;
    void set_rotation_start(float rotation_start); // 0x00C9E4A0 (rl_renderable_particles_instance.inl:272)
    float get_rotation_end() const;
    void set_rotation_end(float rotation_end); // 0x00C9E4C0 (rl_renderable_particles_instance.inl:285)
    float get_rotation_start_variance() const;
    void set_rotation_start_variance(float rotation_start_variance); // 0x00C9E7E0 (rl_renderable_particles_instance.inl:613)
    float get_rotation_end_variance() const;
    void set_rotation_end_variance(float rotation_end_variance); // 0x00C9E800 (rl_renderable_particles_instance.inl:626)
    float get_size_start() const;
    void set_size_start(float size_start); // 0x00C9E4E0 (rl_renderable_particles_instance.inl:297)
    float get_size_mid_A() const;
    void set_size_mid_A(float size_mid_A); // 0x00C9E500 (rl_renderable_particles_instance.inl:310)
    float get_size_mid_B() const;
    void set_size_mid_B(float size_mid_B); // 0x00C9E520 (rl_renderable_particles_instance.inl:323)
    float get_size_end() const;
    void set_size_end(float size_end); // 0x00C9E540 (rl_renderable_particles_instance.inl:336)
    float get_size_start_variance() const;
    void set_size_start_variance(float size_start_variance); // 0x00C9E820 (rl_renderable_particles_instance.inl:638)
    float get_size_mid_A_variance() const;
    void set_size_mid_A_variance(float size_mid_A_variance); // 0x00C9E840 (rl_renderable_particles_instance.inl:651)
    float get_size_mid_B_variance() const;
    void set_size_mid_B_variance(float size_mid_B_variance); // 0x00C9E860 (rl_renderable_particles_instance.inl:664)
    float get_size_end_variance() const;
    void set_size_end_variance(float size_end_variance); // 0x00C9E880 (rl_renderable_particles_instance.inl:677)
    float get_size_mid_A_offset() const;
    void set_size_mid_A_offset(float size_mid_A_offset); // 0x00C9E560 (rl_renderable_particles_instance.inl:349)
    float get_size_mid_B_offset() const;
    void set_size_mid_B_offset(float size_mid_B_offset); // 0x00C9E580 (rl_renderable_particles_instance.inl:362)
    float get_length_start() const;
    void set_length_start(float length_start); // 0x00C9E5A0 (rl_renderable_particles_instance.inl:375)
    float get_length_mid_A() const;
    void set_length_mid_A(float length_mid_A); // 0x00C9E5C0 (rl_renderable_particles_instance.inl:388)
    float get_length_mid_B() const;
    void set_length_mid_B(float length_mid_B); // 0x00C9E5E0 (rl_renderable_particles_instance.inl:401)
    float get_length_end() const;
    void set_length_end(float length_end); // 0x00C9E600 (rl_renderable_particles_instance.inl:414)
    float get_length_start_variance() const;
    void set_length_start_variance(float length_start_variance); // 0x00C9E8A0 (rl_renderable_particles_instance.inl:689)
    float get_length_mid_A_variance() const;
    void set_length_mid_A_variance(float length_mid_A_variance); // 0x00C9E8C0 (rl_renderable_particles_instance.inl:702)
    float get_length_mid_B_variance() const;
    void set_length_mid_B_variance(float length_mid_B_variance); // 0x00C9E8E0 (rl_renderable_particles_instance.inl:715)
    float get_length_end_variance() const;
    void set_length_end_variance(float length_end_variance); // 0x00C9E900 (rl_renderable_particles_instance.inl:728)
    float get_length_mid_A_offset() const;
    void set_length_mid_A_offset(float length_mid_A_offset); // 0x00C9E620 (rl_renderable_particles_instance.inl:427)
    float get_length_mid_B_offset() const;
    void set_length_mid_B_offset(float length_mid_B_offset); // 0x00C9E640 (rl_renderable_particles_instance.inl:440)
    float get_color_saturation_start() const;
    void set_color_saturation_start(float saturation_start); // 0x00C9E660 (rl_renderable_particles_instance.inl:453)
    float get_color_saturation_mid_A() const;
    void set_color_saturation_mid_A(float saturation_mid_A); // 0x00C9E680 (rl_renderable_particles_instance.inl:466)
    float get_color_saturation_mid_B() const;
    void set_color_saturation_mid_B(float saturation_mid_B); // 0x00C9E6A0 (rl_renderable_particles_instance.inl:479)
    float get_color_saturation_end() const;
    void set_color_saturation_end(float saturation_end); // 0x00C9E6C0 (rl_renderable_particles_instance.inl:492)
    float get_additive_key_start() const;
    void set_additive_key_start(float saturation_start); // 0x00C9E6E0 (rl_renderable_particles_instance.inl:505)
    float get_additive_key_mid_A() const;
    void set_additive_key_mid_A(float saturation_mid_A); // 0x00C9E700 (rl_renderable_particles_instance.inl:518)
    float get_additive_key_mid_B() const;
    void set_additive_key_mid_B(float saturation_mid_B); // 0x00C9E720 (rl_renderable_particles_instance.inl:531)
    float get_additive_key_end() const;
    void set_additive_key_end(float saturation_end); // 0x00C9E740 (rl_renderable_particles_instance.inl:544)
    int get_frame_number() const;
    void set_frame_number(int frame_number); // 0x00C9E920 (rl_renderable_particles_instance.inl:740)
    float get_texture_anim_rate() const; // 0x004BD6A0 (rl_renderable_particles_instance.inl:746)
    void set_texture_anim_rate(float texture_anim_rate); // 0x00C9E930 (rl_renderable_particles_instance.inl:751)
    bool get_texture_anim_random_enabled() const;
    void set_texture_anim_random_enabled(bool texture_anim_random_enabled); // 0x00C9E950 (rl_renderable_particles_instance.inl:765)
    float get_opacity() const; // 0x004BD6B0 (rl_renderable_particles_instance.inl:771)
    void set_opacity(float opacity); // 0x004BD6C0 (rl_renderable_particles_instance.inl:777)
    const rl_color_float& get_effect_material_tint() const; // 0x004BD6E0 (rl_renderable_particles_instance.inl:799)
    void set_effect_material_tint(const rl_color_float& effect_material_tint); // 0x004BD6F0 (rl_renderable_particles_instance.inl:804)
    static int get_parametric_constant_pool_available(); // 0x004F1770 (:142)
    void set_tails(int num_tails, rl_particle_base::tail_data* tail_data); // 0x00C9E980 (rl_renderable_particles_instance.inl:782)
    int get_num_tails() const;
    rl_particle_base::tail_data* get_tails() const;
    void clear(); // 0x00515C00 (:47)

    static const unsigned int m___class_id; // 0x012BED30
    static rl_class_factory_info ms_class_factory_info; // 0x01515104
protected:
    static pool_list<rl_renderable_particles_instance*> ms_object_pool; // 0x01515134
public:
    rl_renderable_particles_instance* prev; // 0x08
    rl_renderable_particles_instance* next; // 0x0C
private:
    int m_tail_spawn_count; // 0x10
    float m_current_elapsed_time; // 0x14
    float m_texture_anim_rate; // 0x18
    bool m_is_visible; // 0x1C
    bool m_has_alternate_view; // 0x1D
    rl_color_float m_effect_material_tint; // 0x20
    parametric_constant_data* m_parametric_constant_data_p; // 0x30
    // vfxlib's vfx_particle_system_emitter_instance::copy_particles fills the particle lists and tails directly (friends
    // are not recorded in rfg.pdb).
    friend class vfx_particle_system_emitter_instance;
    // vfxlib's vfx_effect (create_emitter, update_render) sets the render state and parametric constants directly (the
    // .inl setters are inlined there; friends are not recorded in rfg.pdb).
    friend class vfx_effect;
    pool_list<rl_particle_iterative> m_particle_iterative_list; // 0x34
    pool_list<rl_particle_parametric> m_particle_parametric_list; // 0x4C
    unsigned short m_sort_priority; // 0x64
    bool m_break_link_chain; // 0x66
    vector m_local_emission_position; // 0x68
    matrix m_local_emission_rotation; // 0x74
    matrix43 m_alternate_view; // 0x98
    int m_frame_number; // 0xC8
    rl_renderable_particles* m_renderable_particles_p; // 0xCC
    static const int PARAMETRIC_CONSTANT_DATA_ENTRIES_MAX; // 0x012BED34
    static object_pool<parametric_constant_data,1400,1> ms_parametric_constants_data_pool; // 0x01BBBE80
    float m_opacity; // 0xD0
    int m_num_tails; // 0xD4
    rl_particle_base::tail_data* m_tails; // 0xD8
};

#include "volition/render_lib/code/rl_renderable_particles_instance.inl"
