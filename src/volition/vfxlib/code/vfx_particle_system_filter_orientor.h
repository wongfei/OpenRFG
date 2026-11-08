// Original: volition/vfxlib/code/vfx_particle_system_filter_orientor.h
// Implementation: volition/vfxlib/code/vfx_particle_system_filter_orientor.cpp
//
// Orientor filter: orients particles along their velocity or a fixed axis.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/vfxlib/code/vfx_particle_system_filter.h"

class mempool;
class rl_class_factory_info;
struct vfx_class_factory_info;
class vfx_particle_system_emitter_instance;
class vfx_particle_system_filter_instance;

// size 0x50
class vfx_particle_system_filter_orientor : public vfx_particle_system_filter
{
protected:
    vfx_particle_system_filter_orientor(); // 0x00CA0780 (:21)
    virtual ~vfx_particle_system_filter_orientor(); // vtbl+0x00 0x00CA07A0 (:26)
private:
    static const pool_list<vfx_particle_system_filter_orientor*>& get_object_pool(); // 0x00C9D6E0 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D6F0 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB0F10 (:11)
    static vfx_particle_system_filter_orientor* create_placement(vfx_particle_system_filter_orientor* _data_p); // 0x00CA7A50 (:11)
    static vfx_particle_system_filter_orientor* create(); // 0x00CB7E90 (:11)
    static bool can_create(); // 0x00CA7A90 (:11)
    virtual void release(); // vtbl+0x04 0x00CB1010 (:11)
    static void* factory_create(); // 0x00CB7F10 (:11)
    static void factory_serialize(void* this_p, void* data_p); // 0x00CA0750 (:11)
    virtual int serialize(void* data_p); // vtbl+0x08 0x00CA0770 (:14)
    virtual void process_emitter(vfx_particle_system_emitter_instance* instance_p, vfx_particle_system_filter_instance* filter_instance_p, float delta_time); // vtbl+0x0C 0x00CA07B0 (:32)
    virtual void update_instance(vfx_particle_system_filter_instance* instance_p, float delta_time); // vtbl+0x10 0x00CA0830 (:39)

    static const unsigned int m___class_id; // 0x01343DDC
    static rl_class_factory_info ms_class_factory_info; // 0x016F1184
protected:
    static pool_list<vfx_particle_system_filter_orientor*> ms_object_pool; // 0x016F11B4
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0ADC
};
