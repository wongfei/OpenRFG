// Original: volition/vfxlib/code/vfx_particle_system_emitter_sphere.h
// Implementation: volition/vfxlib/code/vfx_particle_system_emitter_sphere.cpp
//
// Sphere emitter: particles born inside or on the surface of a sphere.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"

class mempool;
class rl_class_factory_info;
struct vfx_class_factory_info;
class vfx_particle_base;
class vfx_particle_system_emitter_instance;

// size 0x2D4
class vfx_particle_system_emitter_sphere : public vfx_particle_system_emitter
{
public:
    // size 0x4
    struct serialized_data_block
    {
        float radius; // 0x00
    };

protected:
    vfx_particle_system_emitter_sphere(); // 0x00CB0BA0 (:25)
    virtual ~vfx_particle_system_emitter_sphere(); // vtbl+0x00 0x00CB0BD0 (:30)
private:
    static const pool_list<vfx_particle_system_emitter_sphere*>& get_object_pool(); // 0x00C9D320 (.h:13)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D330 (.h:13)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB0A20 (:8)
    static vfx_particle_system_emitter_sphere* create_placement(vfx_particle_system_emitter_sphere* _data_p); // 0x00CB7CF0 (:8)
    static vfx_particle_system_emitter_sphere* create(); // 0x00CB7C70 (:8)
    static bool can_create(); // 0x00CA74C0 (:8)
    virtual void release(); // vtbl+0x04 0x00CB0B20 (:8)
    static void* factory_create(); // 0x00CB7D30 (:8)
    static void factory_serialize(void* this_p, void* data_p); // 0x00CA0220 (:8)
    virtual int serialize(void* data_p); // vtbl+0x14 0x00CA0240 (:12)
    virtual void spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x0C 0x00CA0260 (:35)
    void set_radius(float);
    float get_radius(); // 0x005D1900 (.h:16)
    virtual void initiate_velocity(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x10 0x00CA0680 (:57)

    static const unsigned int m___class_id; // 0x012C708C
    static rl_class_factory_info ms_class_factory_info; // 0x016F10AC
protected:
    static pool_list<vfx_particle_system_emitter_sphere*> ms_object_pool; // 0x016F10DC
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0A50
private:
    float radius; // 0x2D0
};
