// Original: volition/vfxlib/code/vfx_particle_system_emitter_point.h
// Implementation: volition/vfxlib/code/vfx_particle_system_emitter_point.cpp
//
// Point emitter: particles born at the emitter origin.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"

class mempool;
class rl_class_factory_info;
struct vfx_class_factory_info;
class vfx_particle_base;
class vfx_particle_system_emitter_instance;

// size 0x2D0
class vfx_particle_system_emitter_point : public vfx_particle_system_emitter
{
public:
    // size 0x1
    struct serialized_data_block
    {
    };

protected:
    vfx_particle_system_emitter_point(); // 0x00CB09C0 (:16)
    virtual ~vfx_particle_system_emitter_point(); // vtbl+0x00 0x00CB09E0 (:21)
private:
    static const pool_list<vfx_particle_system_emitter_point*>& get_object_pool(); // 0x00C9D300 (.h:14)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D310 (.h:14)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB0840 (:8)
    static vfx_particle_system_emitter_point* create_placement(vfx_particle_system_emitter_point* _data_p); // 0x00CB7BD0 (:8)
    static vfx_particle_system_emitter_point* create(); // 0x00CB7B60 (:8)
    static bool can_create(); // 0x00CA74B0 (:8)
    virtual void release(); // vtbl+0x04 0x00CB0940 (:8)
    static void* factory_create(); // 0x00CB7C00 (:8)
    static void factory_serialize(void* this_p, void* data_p); // 0x00CA01D0 (:8)
    virtual int serialize(void* data_p); // vtbl+0x14 0x00CA01F0 (:10)
    virtual void spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x0C 0x00CA0200 (:28)

    static const unsigned int m___class_id; // 0x012C7090
    static rl_class_factory_info ms_class_factory_info; // 0x016F1064
protected:
    static pool_list<vfx_particle_system_emitter_point*> ms_object_pool; // 0x016F1094
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0A3C
};
