// Original: volition/vfxlib/code/vfx_renderable_filter_instance.h
// Implementation: volition/vfxlib/code/vfx_renderable_filter_instance.cpp
//
// A renderable filter instance: turns the particles of an emitter instance into a render_lib particle instance (material,
// texture animation, sort and blend settings).
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_handle.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

class mempool;
class rl_class_factory_info;
class rl_renderable_instance;
struct vfx_float_key;
struct vfx_quat_key;
struct vfx_vector_key;

// size 0xC4
class vfx_renderable_filter_instance : public rl_base_object
{
public:
    enum force_type
    {
        FORCE_TYPE_OMNI = 0,
        FORCE_TYPE_DIRECTIONAL = 1,
    };

protected:
    vfx_renderable_filter_instance(); // 0x00C9D760 (.h:132)
    virtual ~vfx_renderable_filter_instance(); // vtbl+0x00 0x00C9D7D0 (.h:149)
private:
    static const pool_list<vfx_renderable_filter_instance*>& get_object_pool(); // 0x00C9D740 (.h:16)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D750 (.h:16)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB1520 (:7)
    static vfx_renderable_filter_instance* create_placement(vfx_renderable_filter_instance* _data_p); // 0x00CA0C10 (:7)
    static vfx_renderable_filter_instance* create(); // 0x00CB80D0 (:7)
    static bool can_create(); // 0x00CA8E20 (:7)
    virtual void release(); // vtbl+0x04 0x00CB1620 (:7)
    vfx_renderable_filter_instance* get_prev() const;
    void set_prev(vfx_renderable_filter_instance*);
    vfx_renderable_filter_instance* get_next() const;
    void set_next(vfx_renderable_filter_instance*);
    void update(float delta_time); // 0x00CA8E30 (:13)
    const vector& get_parent_world_position() const;
    void set_parent_world_position(const vector& parent_world_position); // 0x00C9D7E0 (.h:159)
    const matrix& get_parent_world_rotation() const;
    void set_parent_world_rotation(const matrix& parent_world_rotation); // 0x00C9D800 (.h:170)
    int get_num_force_keys() const;
    void set_num_force_keys(int num_force_keys); // 0x00C9D910 (.h:212)
    vfx_float_key* get_force_keys() const;
    void set_force_keys(vfx_float_key* force_keys_p); // 0x00C9D920 (.h:223)
    int get_num_noise_keys() const;
    void set_num_noise_keys(int num_noise_keys); // 0x00C9D930 (.h:233)
    vfx_float_key* get_noise_keys() const;
    void set_noise_keys(vfx_float_key* noise_keys_p); // 0x00C9D940 (.h:244)
    int get_num_angular_velocity_keys() const;
    void set_num_angular_velocity_keys(int num_angular_velocity_keys); // 0x00C9D950 (.h:255)
    vfx_float_key* get_angular_velocity_keys() const;
    void set_angular_velocity_keys(vfx_float_key* angular_velocity_keys_p); // 0x00C9D960 (.h:265)
    int get_num_angular_velocity_variance_keys() const;
    void set_num_angular_velocity_variance_keys(int num_angular_velocity_variance_keys); // 0x00C9D970 (.h:276)
    vfx_float_key* get_angular_velocity_variance_keys() const;
    void set_angular_velocity_variance_keys(vfx_float_key* angular_velocity_variance_keys_p); // 0x00C9D980 (.h:286)
    int get_num_position_keys() const;
    void set_num_position_keys(int num_position_keys); // 0x00C9D990 (.h:296)
    vfx_vector_key* get_position_keys() const;
    void set_position_keys(vfx_vector_key* position_keys_p); // 0x00C9D9A0 (.h:307)
    int get_num_rotation_keys() const;
    void set_num_rotation_keys(int num_rotation_keys); // 0x00C9D9B0 (.h:318)
    vfx_quat_key* get_rotation_keys() const;
    void set_rotation_keys(vfx_quat_key* rotation_keys_p); // 0x00C9D9C0 (.h:329)
    float get_animation_duration() const;
    void set_animation_duration(float animation_duration); // 0x00C9D840 (.h:181)
    float get_anim_time() const;
    void set_anim_time(float anim_time); // 0x00C9D9E0 (.h:341)
    rl_renderable_instance* get_renderable_instance() const; // 0x00C9DA00 (.h:347)
    void set_renderable_instance(rl_renderable_instance* renderable_instance_p); // 0x00C9DA10 (.h:352)
    rl_renderable_instance* get_parent_renderable_instance() const; // 0x00C9DA20 (.h:358)
    void set_parent_renderable_instance(rl_renderable_instance* parent_renderable_instance_p); // 0x00C9DA30 (.h:363)
    rl_handle get_rigid_body_handle() const; // 0x00C9DA40 (.h:369)
    void set_rigid_body_handle(rl_handle rigid_body_handle); // 0x00C9DA50 (.h:374)
    const vector& get_center_of_mass() const;
    void set_center_of_mass(const vector& center_of_mass); // 0x00C9DA60 (.h:385)
    int get_effect_index() const; // 0x00C9DA80 (.h:391)
    void set_effect_index(int effect_index); // 0x00C9DA90 (.h:396)
private:
    void get_local_position(vector& result, float animation_time) const; // 0x00C9D850 (.h:186)
    const matrix& get_local_rotation(float animation_time) const; // 0x00C9D890 (.h:192)

public:
    static const unsigned int m___class_id; // 0x01343DE0
    static rl_class_factory_info ms_class_factory_info; // 0x016F12A4
protected:
    static pool_list<vfx_renderable_filter_instance*> ms_object_pool; // 0x016F12D4
public:
    vfx_renderable_filter_instance* prev; // 0x08
    vfx_renderable_filter_instance* next; // 0x0C
private:
    // vfx_effect (init, release, update_render) fills the keys, renderables and rigid body directly (friends are not
    // recorded in rfg.pdb).
    friend class vfx_effect;
    vector m_parent_world_position; // 0x10
    matrix m_parent_world_rotation; // 0x1C
    int m_num_force_keys; // 0x40
    vfx_float_key* m_force_keys_p; // 0x44
    int m_num_noise_keys; // 0x48
    vfx_float_key* m_noise_keys_p; // 0x4C
    int m_num_angular_velocity_keys; // 0x50
    vfx_float_key* m_angular_velocity_keys_p; // 0x54
    int m_num_angular_velocity_variance_keys; // 0x58
    vfx_float_key* m_angular_velocity_variance_keys_p; // 0x5C
    float m_animation_duration; // 0x60
    int m_num_position_keys; // 0x64
    vfx_vector_key* m_position_keys_p; // 0x68
    int m_num_rotation_keys; // 0x6C
    vfx_quat_key* m_rotation_keys_p; // 0x70
    matrix m_local_rotation_cached; // 0x74
    bool m_local_rotation_cached_dirty; // 0x98
    rl_renderable_instance* m_renderable_instance_p; // 0x9C
    rl_renderable_instance* m_parent_renderable_instance_p; // 0xA0
    rl_handle m_rigid_body_handle; // 0xA4
    vector m_center_of_mass; // 0xA8
    force_type m_force_type; // 0xB4
    float m_anim_time; // 0xB8
    bool m_first_update; // 0xBC
    int m_effect_index; // 0xC0
};
