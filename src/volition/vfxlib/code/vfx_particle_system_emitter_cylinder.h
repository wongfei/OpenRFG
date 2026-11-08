// Original: volition/vfxlib/code/vfx_particle_system_emitter_cylinder.h
// Implementation: volition/vfxlib/code/vfx_particle_system_emitter_cylinder.cpp
//
// Cylinder emitter: particles born inside or on the surface of a cylinder.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"

class mempool;
class rl_class_factory_info;
struct vfx_class_factory_info;
class vfx_particle_base;
class vfx_particle_system_emitter_instance;

// size 0x2DC
class vfx_particle_system_emitter_cylinder : public vfx_particle_system_emitter
{
public:
    // size 0x8
    struct serialized_data_block
    {
        float radius; // 0x00
        float height; // 0x04
    };

protected:
    vfx_particle_system_emitter_cylinder(); // 0x00CAFD10 (:27)
    virtual ~vfx_particle_system_emitter_cylinder(); // vtbl+0x00 0x00CAFD40 (:31)
private:
    static const pool_list<vfx_particle_system_emitter_cylinder*>& get_object_pool(); // 0x00C9D360 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D370 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CAFB90 (:9)
    static vfx_particle_system_emitter_cylinder* create_placement(vfx_particle_system_emitter_cylinder* _data_p); // 0x00CB5D70 (:9)
    static vfx_particle_system_emitter_cylinder* create(); // 0x00CB5CF0 (:9)
    static bool can_create(); // 0x00CA45F0 (:9)
    virtual void release(); // vtbl+0x04 0x00CAFC90 (:9)
    static void* factory_create(); // 0x00CB5DC0 (:9)
    static void factory_serialize(void* this_p, void* data_p); // 0x00C9F600 (:9)
    virtual int serialize(void* data_p); // vtbl+0x14 0x00C9F620 (:12)
    virtual void spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x0C 0x00C9F790 (:48)
    void set_radius(float);
    float get_radius(); // 0x005D18E0 (.h:16)
    void set_height(float);
    float get_height(); // 0x005D18F0 (.h:18)
    virtual void initiate_velocity(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x10 0x00C9FCA0 (:81)
    virtual void init(); // vtbl+0x08 0x00C9F640 (:36)

    static const unsigned int m___class_id; // 0x012C7088
    static rl_class_factory_info ms_class_factory_info; // 0x016F0F80
protected:
    static pool_list<vfx_particle_system_emitter_cylinder*> ms_object_pool; // 0x016F0FB0
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0A78
private:
    float radius; // 0x2D0
    float height; // 0x2D4
    bool m_spread_enabled; // 0x2D8
};
