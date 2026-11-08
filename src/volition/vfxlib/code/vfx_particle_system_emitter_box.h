// Original: volition/vfxlib/code/vfx_particle_system_emitter_box.h
// Implementation: volition/vfxlib/code/vfx_particle_system_emitter_box.cpp
//
// Box emitter: particles born inside or on the surface of a box.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"

class mempool;
class rl_class_factory_info;
struct vfx_class_factory_info;
class vfx_particle_base;
class vfx_particle_system_emitter_instance;

// size 0x2DC
class vfx_particle_system_emitter_box : public vfx_particle_system_emitter
{
public:
    // size 0xC
    struct serialized_data_block
    {
        float width; // 0x00
        float height; // 0x04
        float length; // 0x08
    };

protected:
    vfx_particle_system_emitter_box(); // 0x00CAFB10 (:28)
    virtual ~vfx_particle_system_emitter_box(); // vtbl+0x00 0x00CAFB50 (:33)
private:
    static const pool_list<vfx_particle_system_emitter_box*>& get_object_pool(); // 0x00C9D340 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D350 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CAF990 (:8)
    static vfx_particle_system_emitter_box* create_placement(vfx_particle_system_emitter_box* _data_p); // 0x00CB5C30 (:8)
    static vfx_particle_system_emitter_box* create(); // 0x00CB5BA0 (:8)
    static bool can_create(); // 0x00CA45E0 (:8)
    virtual void release(); // vtbl+0x04 0x00CAFA90 (:8)
    static void* factory_create(); // 0x00CB5C80 (:8)
    static void factory_serialize(void* this_p, void* data_p); // 0x00C9F300 (:8)
    virtual int serialize(void* data_p); // vtbl+0x14 0x00C9F320 (:11)
    virtual void spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x0C 0x00C9F350 (:40)
    void set_width(float);
    float get_width(); // 0x005D1910 (.h:16)
    void set_length(float);
    float get_length(); // 0x005D1920 (.h:18)
    void set_height(float);
    float get_height(); // 0x005D1930 (.h:20)

    static const unsigned int m___class_id; // 0x012C7094
    static rl_class_factory_info ms_class_factory_info; // 0x016F0F38
protected:
    static pool_list<vfx_particle_system_emitter_box*> ms_object_pool; // 0x016F0F68
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0A64
private:
    float width; // 0x2D0
    float height; // 0x2D4
    float length; // 0x2D8
};
