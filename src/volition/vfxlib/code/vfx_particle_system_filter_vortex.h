// Original: volition/vfxlib/code/vfx_particle_system_filter_vortex.h
// Implementation: volition/vfxlib/code/vfx_particle_system_filter_vortex.cpp
//
// Vortex filter: spins particles around an axis.
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

// size 0x58
class vfx_particle_system_filter_vortex : public vfx_particle_system_filter
{
public:
    // size 0x10
    struct serialized_data_block
    {
        int num_scale_keys; // 0x00
        et_sized_native_pointer<vfx_vector_key> scale_keys_p; // 0x08
    };

protected:
    vfx_particle_system_filter_vortex(); // 0x00CA09F0 (:27)
    virtual ~vfx_particle_system_filter_vortex(); // vtbl+0x00 0x00CA0A10 (:32)
private:
    static const pool_list<vfx_particle_system_filter_vortex*>& get_object_pool(); // 0x00C9D6C0 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D6D0 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB1210 (:12)
    static vfx_particle_system_filter_vortex* create_placement(vfx_particle_system_filter_vortex* _data_p); // 0x00CA7EC0 (:12)
    static vfx_particle_system_filter_vortex* create(); // 0x00CB7FB0 (:12)
    static bool can_create(); // 0x00CA7F00 (:12)
    virtual void release(); // vtbl+0x04 0x00CB1300 (:12)
    static void* factory_create(); // 0x00CB8030 (:12)
    static void factory_serialize(void* this_p, void* data_p); // 0x00CA09D0 (:12)
    virtual int serialize(void* data_p); // vtbl+0x08 0x00CA7F10 (:15)
    virtual void process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time); // vtbl+0x0C 0x00CA7F70 (:38)
    virtual void update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time); // vtbl+0x10 0x00CA0A20 (:90)
    int get_num_scale_keys() const;
    void set_num_scale_keys(int);
    vfx_vector_key* get_scale_keys() const;
    void set_scale_keys(vfx_vector_key*);

    static const unsigned int m___class_id; // 0x01343DD8
    static rl_class_factory_info ms_class_factory_info; // 0x016F1214
protected:
    static pool_list<vfx_particle_system_filter_vortex*> ms_object_pool; // 0x016F1244
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0AC8
private:
    int m_num_scale_keys; // 0x50
    vfx_vector_key* m_scale_keys_p; // 0x54
};
