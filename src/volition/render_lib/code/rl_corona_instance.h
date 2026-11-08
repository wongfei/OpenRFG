// Original: volition/render_lib/code/rl_corona_instance.h
// Implementation: volition/render_lib/code/rl_corona_instance.cpp
//
// rl_corona_instance, light coronas and flares with occlusion fading.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_class_factory_info;
class rl_renderer;

// size 0x118
class rl_corona_instance : private rl_base_object
{
public:
    enum type
    {
        TYPE_SCREEN_SPACE = 0,
        TYPE_PERSPECTIVE = 1,
    };

protected:
    rl_corona_instance(); // 0x004BD760 (:11)
    virtual ~rl_corona_instance(); // vtbl+0x00 0x0050C460 (:58)
private:
    static const pool_list<rl_corona_instance*>& get_object_pool(); // 0x004BD0F0 (.h:24)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x004BD100 (.h:24)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x0050C360 (:7)
    static rl_corona_instance* create_placement(rl_corona_instance*);
    static rl_corona_instance* create(); // 0x00523650 (:7)
    static bool can_create(); // 0x004E3F50 (:7)
    virtual void release(); // vtbl+0x04 0x00539000 (:7)
    rl_corona_instance* get_prev() const;
    void set_prev(rl_corona_instance*);
    rl_corona_instance* get_next() const;
    void set_next(rl_corona_instance*);
    void render(); // 0x005497D0 (:69)
    int get_type() const; // 0x004BD110 (rl_corona_instance.inl:3)
    void set_type(int type); // 0x00C9EB10 (rl_corona_instance.inl:8)
    const vector& get_position() const; // 0x004BD120 (rl_corona_instance.inl:13)
    void set_position(const vector& position); // 0x00C9EB20 (rl_corona_instance.inl:18)
    const matrix& get_rotation() const; // 0x004BD130 (rl_corona_instance.inl:24)
    void set_rotation(const matrix& rotation); // 0x00C9EB40 (rl_corona_instance.inl:29)
    const rl_color_float& get_color_start() const; // 0x004BD140 (rl_corona_instance.inl:35)
    void set_color_start(const rl_color_float& color_start); // 0x00C9EB80 (rl_corona_instance.inl:40)
    const rl_color_float& get_color_end() const; // 0x004BD150 (rl_corona_instance.inl:46)
    void set_color_end(const rl_color_float& color_end); // 0x00C9EBA0 (rl_corona_instance.inl:51)
    float get_saturation() const; // 0x004BD160 (rl_corona_instance.inl:57)
    void set_saturation(float saturation); // 0x00C9EBC0 (rl_corona_instance.inl:62)
    float get_additive_key() const; // 0x004BD170 (rl_corona_instance.inl:68)
    void set_additive_key(float);
    float get_radius_start() const; // 0x004BD180 (rl_corona_instance.inl:79)
    void set_radius_start(float radius_start); // 0x00C9EBD0 (rl_corona_instance.inl:84)
    float get_radius_end() const; // 0x004BD190 (rl_corona_instance.inl:90)
    void set_radius_end(float radius_end); // 0x00C9EBE0 (rl_corona_instance.inl:95)
    float get_width_scale() const; // 0x004BD1A0 (rl_corona_instance.inl:101)
    void set_width_scale(float width_scale); // 0x00C9EBF0 (rl_corona_instance.inl:106)
    float get_current_elapsed_time() const; // 0x004BD1B0 (rl_corona_instance.inl:112)
    void set_current_elapsed_time(float);
    bool get_random_anim_frames() const; // 0x004BD1C0 (rl_corona_instance.inl:123)
    void set_random_anim_frames(bool);
    float get_anim_rate() const; // 0x004BD1D0 (rl_corona_instance.inl:134)
    void set_anim_rate(float);
    bool get_infinite_depth() const; // 0x004BD1E0 (rl_corona_instance.inl:145)
    void set_infinite_depth(bool infinite_depth); // 0x00743B60 (rl_corona_instance.inl:150)
    float get_visibility_fade() const; // 0x004BD1F0 (rl_corona_instance.inl:155)
    void set_visibility_fade(float visibility_fade); // 0x00743B70 (rl_corona_instance.inl:160)
    float get_rotation_scale() const; // 0x004BD200 (rl_corona_instance.inl:166)
    void set_rotation_scale(float rotation_scale); // 0x00C9EC00 (rl_corona_instance.inl:171)
    float get_rotation_offset() const; // 0x004BD210 (rl_corona_instance.inl:177)
    void set_rotation_offset(float rotation_offset); // 0x00C9EC20 (rl_corona_instance.inl:182)
    float get_position_offset() const; // 0x004BD220 (rl_corona_instance.inl:188)
    void set_position_offset(float position_offset); // 0x00C9EC40 (rl_corona_instance.inl:193)
    float get_intensity_scale() const; // 0x004BD230 (rl_corona_instance.inl:199)
    void set_intensity_scale(float intensity_scale); // 0x00C9EC60 (rl_corona_instance.inl:204)
    float get_intensity_exponential_decay() const; // 0x004BD240 (rl_corona_instance.inl:210)
    void set_intensity_exponential_decay(float intensity_exponential_decay); // 0x00C9EC80 (rl_corona_instance.inl:215)
    float get_opacity_exponential_decay() const; // 0x004BD250 (rl_corona_instance.inl:221)
    void set_opacity_exponential_decay(float opacity_exponential_decay); // 0x00C9ECA0 (rl_corona_instance.inl:226)
    float get_radius_distance_start() const; // 0x004BD280 (rl_corona_instance.inl:253)
    void set_radius_distance_start(float radius_distance_start); // 0x00C9ED00 (rl_corona_instance.inl:258)
    float get_radius_distance_end() const; // 0x004BD290 (rl_corona_instance.inl:264)
    void set_radius_distance_end(float radius_distance_end); // 0x00C9ED20 (rl_corona_instance.inl:269)
    float get_color_distance_start() const; // 0x004BD260 (rl_corona_instance.inl:231)
    void set_color_distance_start(float color_distance_start); // 0x00C9ECC0 (rl_corona_instance.inl:236)
    float get_color_distance_end() const; // 0x004BD270 (rl_corona_instance.inl:242)
    void set_color_distance_end(float color_distance_end); // 0x00C9ECE0 (rl_corona_instance.inl:247)
    float get_view_fade_scale() const; // 0x004BD2A0 (rl_corona_instance.inl:275)
    void set_view_fade_scale(float view_fade_scale); // 0x00C9ED40 (rl_corona_instance.inl:280)
    float get_view_fade_decay() const; // 0x004BD2B0 (rl_corona_instance.inl:286)
    void set_view_fade_decay(float view_fade_decay); // 0x00C9ED60 (rl_corona_instance.inl:291)
    bool get_view_fade_intensity_enabled() const; // 0x004BD2C0 (rl_corona_instance.inl:297)
    void set_view_fade_intensity_enabled(bool view_fade_intensity_enabled); // 0x00C9ED80 (rl_corona_instance.inl:302)
    bool get_view_fade_opacity_enabled() const; // 0x004BD2D0 (rl_corona_instance.inl:308)
    void set_view_fade_opacity_enabled(bool view_fade_opacity_enabled); // 0x00C9ED90 (rl_corona_instance.inl:313)
    bool get_view_fade_radius_enabled() const; // 0x004BD2E0 (rl_corona_instance.inl:319)
    void set_view_fade_radius_enabled(bool view_fade_radius_enabled); // 0x00C9EDA0 (rl_corona_instance.inl:324)
    int get_texture_handle() const; // 0x004BD2F0 (rl_corona_instance.inl:330)
    void set_texture_handle(int texture_handle); // 0x00C9EDB0 (rl_corona_instance.inl:335)
    bool get_is_soft() const;
    void set_is_soft(bool);
    const matrix& get_color_correction_matrix() const; // 0x004BD300 (rl_corona_instance.inl:341)
    void set_color_correction_matrix(const matrix&);
    float get_visibility_radius() const; // 0x004BD310 (rl_corona_instance.inl:352)
    void set_visibility_radius(float visibility_radius); // 0x00C9EDC0 (rl_corona_instance.inl:357)
    rl_stamped_handle get_query_no_depth_handle() const; // 0x004BD320 (rl_corona_instance.inl:363)
    void set_query_no_depth_handle(rl_stamped_handle query_no_depth_handle); // 0x004BD340 (rl_corona_instance.inl:368)
    rl_stamped_handle get_query_depth_handle() const; // 0x004BD360 (rl_corona_instance.inl:374)
    void set_query_depth_handle(rl_stamped_handle query_depth_handle); // 0x004BD380 (rl_corona_instance.inl:379)
    int get_depth_pixels_drawn() const; // 0x004BD3A0 (rl_corona_instance.inl:385)
    void set_depth_pixels_drawn(int depth_pixels_drawn); // 0x004BD3B0 (rl_corona_instance.inl:390)
    int get_no_depth_pixels_drawn() const; // 0x004BD3C0 (rl_corona_instance.inl:396)
    void set_no_depth_pixels_drawn(int no_depth_pixels_drawn); // 0x004BD3D0 (rl_corona_instance.inl:401)
    float get_opacity() const; // 0x004BD400 (rl_corona_instance.inl:412)
    void set_opacity(float opacity); // 0x004BD3E0 (rl_corona_instance.inl:407)
    float get_opacity_visibility_decay() const; // 0x004BD410 (rl_corona_instance.inl:417)
    void set_opacity_visibility_decay(float opacity_visibility_decay); // 0x00C9EDE0 (rl_corona_instance.inl:422)
    float get_radius_visibility_decay() const; // 0x004BD420 (rl_corona_instance.inl:428)
    void set_radius_visibility_decay(float radius_visibility_decay); // 0x00C9EE00 (rl_corona_instance.inl:433)
    float get_visibility() const; // 0x004BD430 (rl_corona_instance.inl:439)
    void set_visibility(float visibility); // 0x004BD440 (rl_corona_instance.inl:444)

    static const unsigned int m___class_id; // 0x012BED24
    static rl_class_factory_info ms_class_factory_info; // 0x01514960
protected:
    static pool_list<rl_corona_instance*> ms_object_pool; // 0x01514990
public:
    rl_corona_instance* prev; // 0x08
    rl_corona_instance* next; // 0x0C
private:
    // vfxlib's vfx_effect::init sets the texture handle directly (the .inl setter is inlined there; friends are not
    // recorded in rfg.pdb).
    friend class vfx_effect;
    // The group passes its opacity down, the particle renderer reads and updates the visibility state directly (no
    // accessors for them in rfg.pdb).
    friend class rl_renderable_particles_instance_group;
    friend class rl_particle_renderer;
    friend void update_corona(rl_renderer& renderer, rl_corona_instance* instance_p);
    int m_type; // 0x10
    vector m_position; // 0x14
    matrix m_rotation; // 0x20
    rl_color_float m_color_start; // 0x44
    rl_color_float m_color_end; // 0x54
    float m_saturation; // 0x64
    float m_additive_key; // 0x68
    float m_radius_start; // 0x6C
    float m_radius_end; // 0x70
    float m_width_scale; // 0x74
    float m_current_elapsed_time; // 0x78
    float m_anim_rate; // 0x7C
    float m_rotation_scale; // 0x80
    float m_rotation_offset; // 0x84
    float m_position_offset; // 0x88
    float m_intensity_scale; // 0x8C
    float m_intensity_exponential_decay; // 0x90
    float m_opacity_exponential_decay; // 0x94
    float m_color_distance_start; // 0x98
    float m_color_distance_end; // 0x9C
    float m_radius_distance_start; // 0xA0
    float m_radius_distance_end; // 0xA4
    float m_view_fade_scale; // 0xA8
    float m_view_fade_decay; // 0xAC
    bool m_view_fade_intensity_enabled; // 0xB0
    bool m_view_fade_opacity_enabled; // 0xB1
    bool m_view_fade_radius_enabled; // 0xB2
    bool m_random_anim_frames; // 0xB3
    bool m_infinite_depth; // 0xB4
    float m_visibility_fade; // 0xB8
    int m_texture_handle; // 0xBC
    bool m_rear_view; // 0xC0
    float m_visibility; // 0xC4
    float m_intensity; // 0xC8
    float m_visibility_radius; // 0xCC
    rl_stamped_handle m_query_no_depth_handle; // 0xD0
    rl_stamped_handle m_query_depth_handle; // 0xD8
    int m_depth_pixels_drawn; // 0xE0
    int m_no_depth_pixels_drawn; // 0xE4
    matrix m_color_correction_matrix; // 0xE8
    float m_opacity_visibility_decay; // 0x10C
    float m_radius_visibility_decay; // 0x110
    float m_opacity; // 0x114
};

#include "volition/render_lib/code/rl_corona_instance.inl"
