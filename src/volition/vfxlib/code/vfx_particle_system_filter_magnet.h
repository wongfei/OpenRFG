// Original: volition/vfxlib/code/vfx_particle_system_filter_magnet.h
// Implementation: volition/vfxlib/code/vfx_particle_system_filter_magnet.cpp
//
// Magnet filter: attracts or repels particles from a point.
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
class vfx_particle_system_filter_magnet : public vfx_particle_system_filter
{
public:
    // size 0x20
    struct serialized_data_block
    {
        int num_force_keys; // 0x00
        et_sized_native_pointer<vfx_float_key> force_keys_p; // 0x08
        int num_radius_keys; // 0x10
        et_sized_native_pointer<vfx_float_key> radius_keys_p; // 0x18
    };

protected:
    vfx_particle_system_filter_magnet(); // 0x00CA0720 (:31)
    virtual ~vfx_particle_system_filter_magnet(); // vtbl+0x00 0x00CA0740 (:36)
private:
    static const pool_list<vfx_particle_system_filter_magnet*>& get_object_pool(); // 0x00C9D680 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D690 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB0D90 (:12)
    static vfx_particle_system_filter_magnet* create_placement(vfx_particle_system_filter_magnet* _data_p); // 0x00CA74E0 (:12)
    static vfx_particle_system_filter_magnet* create(); // 0x00CB7E00 (:12)
    static bool can_create(); // 0x00CA7520 (:12)
    virtual void release(); // vtbl+0x04 0x00CB0E90 (:12)
    static void* factory_create(); // 0x00CB7E80 (:12)
    static void factory_serialize(void* this_p, void* data_p); // 0x00CA0700 (:12)
    virtual int serialize(void* data_p); // vtbl+0x08 0x00CA7530 (:15)
    virtual void process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time); // vtbl+0x0C 0x00CA75C0 (:42)
    virtual void update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time); // vtbl+0x10 0x00CA78A0 (:71)
    int get_num_force_keys() const;
    void set_num_force_keys(int);
    vfx_float_key* get_force_keys() const;
    void set_force_keys(vfx_float_key*);
    int get_num_radius_keys() const;
    void set_num_radius_keys(int);
    vfx_float_key* get_radius_keys() const;
    void set_radius_keys(vfx_float_key*);

    static const unsigned int m___class_id; // 0x01343DD0
    static rl_class_factory_info ms_class_factory_info; // 0x016F113C
protected:
    static pool_list<vfx_particle_system_filter_magnet*> ms_object_pool; // 0x016F116C
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0AA0
private:
    int m_num_force_keys; // 0x50
    vfx_float_key* m_force_keys_p; // 0x54
    int m_num_radius_keys; // 0x58
    vfx_float_key* m_radius_keys_p; // 0x5C
};
