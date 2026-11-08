// Original: volition/vfxlib/code/vfx_particle_system_filter_turbulence.h
// Implementation: volition/vfxlib/code/vfx_particle_system_filter_turbulence.cpp
//
// Turbulence filter: noise displacement of particle velocities.
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

// size 0x60
class vfx_particle_system_filter_turbulence : public vfx_particle_system_filter
{
public:
    // size 0x20
    struct serialized_data_block
    {
        int num_force_keys; // 0x00
        et_sized_native_pointer<vfx_float_key> force_keys_p; // 0x08
        int num_noise_keys; // 0x10
        et_sized_native_pointer<vfx_float_key> noise_keys_p; // 0x18
    };

protected:
    vfx_particle_system_filter_turbulence(); // 0x00CA09A0 (:31)
    virtual ~vfx_particle_system_filter_turbulence(); // vtbl+0x00 0x00CA09C0 (:36)
private:
    static const pool_list<vfx_particle_system_filter_turbulence*>& get_object_pool(); // 0x00C9D6A0 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D6B0 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB1090 (:11)
    static vfx_particle_system_filter_turbulence* create_placement(vfx_particle_system_filter_turbulence* _data_p); // 0x00CA7AC0 (:11)
    static vfx_particle_system_filter_turbulence* create(); // 0x00CB7F20 (:11)
    static bool can_create(); // 0x00CA7B00 (:11)
    virtual void release(); // vtbl+0x04 0x00CB1190 (:11)
    static void* factory_create(); // 0x00CB7FA0 (:11)
    static void factory_serialize(void* this_p, void* data_p); // 0x00CA0980 (:11)
    virtual int serialize(void* data_p); // vtbl+0x08 0x00CA7B10 (:14)
    virtual void process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time); // vtbl+0x0C 0x00CA7BA0 (:42)
    virtual void update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time); // vtbl+0x10 0x00CA7D10 (:68)
    int get_num_force_keys() const;
    void set_num_force_keys(int);
    vfx_float_key* get_force_keys() const;
    void set_force_keys(vfx_float_key*);
    int get_num_noise_keys() const;
    void set_num_noise_keys(int);
    vfx_float_key* get_noise_keys() const;
    void set_noise_keys(vfx_float_key*);

    static const unsigned int m___class_id; // 0x01343DD4
    static rl_class_factory_info ms_class_factory_info; // 0x016F11CC
protected:
    static pool_list<vfx_particle_system_filter_turbulence*> ms_object_pool; // 0x016F11FC
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0AB4
private:
    int m_num_force_keys; // 0x50
    vfx_float_key* m_force_keys_p; // 0x54
    int m_num_noise_keys; // 0x58
    vfx_float_key* m_noise_keys_p; // 0x5C
};
