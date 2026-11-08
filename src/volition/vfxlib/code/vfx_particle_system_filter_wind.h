// Original: volition/vfxlib/code/vfx_particle_system_filter_wind.h
// Implementation: volition/vfxlib/code/vfx_particle_system_filter_wind.cpp
//
// Wind filter: pushes particles along a direction.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle_system_filter.h"
#include "volition/vlib/code/util/endian_type.h"

class mempool;
class rl_class_factory_info;
struct vfx_class_factory_info;
class vfx_particle_system_emitter_instance;
class vfx_particle_system_filter_instance;

// size 0x70
class vfx_particle_system_filter_wind : public vfx_particle_system_filter
{
public:
    // size 0x40
    struct serialized_data_block
    {
        int num_force_keys; // 0x00
        et_sized_native_pointer<vfx_float_key> force_keys_p; // 0x08
        int num_noise_keys; // 0x10
        et_sized_native_pointer<vfx_float_key> noise_keys_p; // 0x18
        int num_angular_velocity_keys; // 0x20
        et_sized_native_pointer<vfx_float_key> angular_velocity_keys_p; // 0x28
        int num_angular_velocity_variance_keys; // 0x30
        et_sized_native_pointer<vfx_float_key> angular_velocity_variance_keys_p; // 0x38
    };

protected:
    vfx_particle_system_filter_wind(); // 0x00CA0BD0 (:40)
    virtual ~vfx_particle_system_filter_wind(); // vtbl+0x00 0x00CA0BF0 (:45)
private:
    static const pool_list<vfx_particle_system_filter_wind*>& get_object_pool(); // 0x00C9D1E0 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D1F0 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB1380 (:11)
    static vfx_particle_system_filter_wind* create_placement(vfx_particle_system_filter_wind* _data_p); // 0x00CA85C0 (:11)
    static vfx_particle_system_filter_wind* create(); // 0x00CB8040 (:11)
    static bool can_create(); // 0x00CA8600 (:11)
    virtual void release(); // vtbl+0x04 0x00CB1470 (:11)
    static void* factory_create(); // 0x00CB80C0 (:11)
    static void factory_serialize(void* this_p, void* data_p); // 0x00CA0BB0 (:11)
    virtual int serialize(void* data_p); // vtbl+0x08 0x00CA8610 (:14)
    virtual void process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time); // vtbl+0x0C 0x00CA86E0 (:51)
    virtual void update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time); // vtbl+0x10 0x00CA8A00 (:78)
    int get_num_force_keys() const;
    void set_num_force_keys(int);
    vfx_float_key* get_force_keys() const;
    void set_force_keys(vfx_float_key*);
    int get_num_noise_keys() const;
    void set_num_noise_keys(int);
    vfx_float_key* get_noise_keys() const;
    void set_noise_keys(vfx_float_key*);

    static const unsigned int m___class_id; // 0x012C7078
    static rl_class_factory_info ms_class_factory_info; // 0x016F125C
protected:
    static pool_list<vfx_particle_system_filter_wind*> ms_object_pool; // 0x016F128C
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0A8C
private:
    int m_num_force_keys; // 0x50
    vfx_float_key* m_force_keys_p; // 0x54
    int m_num_noise_keys; // 0x58
    vfx_float_key* m_noise_keys_p; // 0x5C
    int m_num_angular_velocity_keys; // 0x60
    vfx_float_key* m_angular_velocity_keys_p; // 0x64
    int m_num_angular_velocity_variance_keys; // 0x68
    vfx_float_key* m_angular_velocity_variance_keys_p; // 0x6C
};
