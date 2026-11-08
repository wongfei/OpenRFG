// Original: volition/vfxlib/code/vfx_particle_system_filter_instance.h
// Implementation: volition/vfxlib/code/vfx_particle_system_filter_instance.cpp
//
// A filter instance of an emitter instance: its filter and per-instance state.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_class_factory_info;
class rl_renderable_instance;
class vfx_particle_system_filter;

// size 0xD0
class vfx_particle_system_filter_instance : public rl_base_object
{
protected:
    vfx_particle_system_filter_instance(); // 0x00C9D3A0 (.h:85)
    virtual ~vfx_particle_system_filter_instance(); // vtbl+0x00 0x00C9D4D0 (.h:101)
private:
    static const pool_list<vfx_particle_system_filter_instance*>& get_object_pool(); // 0x00C9D380 (.h:19)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D390 (.h:19)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB0C10 (:8)
    static vfx_particle_system_filter_instance* create_placement(vfx_particle_system_filter_instance* _data_p); // 0x00CA06C0 (:8)
    static vfx_particle_system_filter_instance* create(); // 0x00CB7DA0 (:8)
    static bool can_create(); // 0x00CA74D0 (:8)
    virtual void release(); // vtbl+0x04 0x00CB0D10 (:8)
    vfx_particle_system_filter_instance* get_prev() const;
    void set_prev(vfx_particle_system_filter_instance*);
    vfx_particle_system_filter_instance* get_next() const;
    void set_next(vfx_particle_system_filter_instance*);
    void update(float delta_time); // 0x00CA06E0 (:13)
    vfx_particle_system_filter* get_filter() const; // 0x00C9D4E0 (.h:106)
    void set_filter(vfx_particle_system_filter* filter_p); // 0x00C9D4F0 (.h:111)
    const vector& get_parent_world_position() const; // 0x00C9D500 (.h:117)
    void set_parent_world_position(const vector& parent_world_position); // 0x00C9D510 (.h:122)
    const matrix& get_parent_world_rotation() const; // 0x00C9D530 (.h:128)
    void set_parent_world_rotation(const matrix& parent_world_rotation); // 0x00C9D540 (.h:133)
    const vector& get_world_position() const; // 0x005D1740 (.h:139)
    void set_world_position(const vector& world_position); // 0x00C9D580 (.h:144)
    const matrix& get_world_rotation() const; // 0x006E1890 (.h:150)
    void set_world_rotation(const matrix& world_rotation); // 0x00743AF0 (.h:155)
    const matrix43& get_world_matrix() const;
    void set_world_matrix(const matrix43&);
    rl_renderable_instance* get_parent_renderable_instance() const; // 0x00C9D5A0 (.h:172)
    void set_parent_renderable_instance(rl_renderable_instance* parent_renderable_instance_p); // 0x00C9D5B0 (.h:177)
    float get_force() const; // 0x00C9D5C0 (.h:182)
    void set_force(float force); // 0x00743B30 (.h:187)
    float get_force_noise() const; // 0x00C9D5D0 (.h:193)
    void set_force_noise(float force_noise); // 0x00C9D5E0 (.h:198)
    const vector& get_force_vector() const;
    void set_force_vector(const vector&);
    float get_radius() const; // 0x005D1750 (.h:215)
    void set_radius(float radius); // 0x00C9D600 (.h:220)
    const vector& get_scale() const; // 0x00C9D620 (.h:226)
    void set_scale(const vector& scale); // 0x00C9D630 (.h:231)
    float get_anim_time() const; // 0x00C9D650 (.h:237)
    void set_anim_time(float anim_time); // 0x00C9D660 (.h:242)

    static const unsigned int m___class_id; // 0x012C7074
    static rl_class_factory_info ms_class_factory_info; // 0x016F10F4
protected:
    static pool_list<vfx_particle_system_filter_instance*> ms_object_pool; // 0x016F1124
public:
    vfx_particle_system_filter_instance* prev; // 0x08
    vfx_particle_system_filter_instance* next; // 0x0C
private:
    // vfx_effect (init, update) sets the filter, parent and parent transform directly (friends are not recorded in
    // rfg.pdb).
    friend class vfx_effect;
    vfx_particle_system_filter* m_filter_p; // 0x10
    rl_renderable_instance* m_parent_renderable_instance_p; // 0x14
    vector m_parent_world_position; // 0x18
    matrix m_parent_world_rotation; // 0x24
    vector m_world_position; // 0x48
    matrix m_world_rotation; // 0x54
    matrix43 m_world_matrix; // 0x78
    float m_force; // 0xA8
    float m_force_noise; // 0xAC
    vector m_force_vector; // 0xB0
    float m_radius; // 0xBC
    vector m_scale; // 0xC0
    float m_anim_time; // 0xCC
};
