// Original: volition/vfxlib/code/vfx_particle_system_emitter_plane.h
// Implementation: volition/vfxlib/code/vfx_particle_system_emitter_plane.cpp
//
// Plane emitter: particles born on a rectangle. vfx_particle_system_emitter_line.cpp, which has no header, holds no
// functions.
#pragma once

#include "volition/render_lib/code/pool_list.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"

class mempool;
class rl_class_factory_info;
struct vfx_class_factory_info;
class vfx_particle_base;
class vfx_particle_system_emitter_instance;

// size 0x2D8
class vfx_particle_system_emitter_plane : public vfx_particle_system_emitter
{
public:
    // size 0x8
    struct serialized_data_block
    {
        float width; // 0x00
        float length; // 0x04
    };

protected:
    vfx_particle_system_emitter_plane(); // 0x00CB07D0 (:26)
    virtual ~vfx_particle_system_emitter_plane(); // vtbl+0x00 0x00CB0800 (:30)
private:
    static const pool_list<vfx_particle_system_emitter_plane*>& get_object_pool(); // 0x00C9D2E0 (.h:14)
public:
    static void set_objects_max(int);
    static int get_objects_max(); // 0x00C9D2F0 (.h:14)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00CB0650 (:8)
    static vfx_particle_system_emitter_plane* create_placement(vfx_particle_system_emitter_plane* _data_p); // 0x00CB7AA0 (:8)
    static vfx_particle_system_emitter_plane* create(); // 0x00CB7A20 (:8)
    static bool can_create(); // 0x00CA74A0 (:8)
    virtual void release(); // vtbl+0x04 0x00CB0750 (:8)
    static void* factory_create(); // 0x00CB7AF0 (:8)
    static void factory_serialize(void* this_p, void* data_p); // 0x00C9FF40 (:8)
    virtual int serialize(void* data_p); // vtbl+0x14 0x00C9FF60 (:11)
    virtual void spawn_particle(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p); // vtbl+0x0C 0x00C9FF80 (:35)
    void set_width(float);
    float get_width() const; // 0x005D1940 (.h:17)
    void set_length(float);
    float get_length() const; // 0x005D1950 (.h:19)

    static const unsigned int m___class_id; // 0x012C7098
    static rl_class_factory_info ms_class_factory_info; // 0x016F101C
protected:
    static pool_list<vfx_particle_system_emitter_plane*> ms_object_pool; // 0x016F104C
public:
    static vfx_class_factory_info m_vfx_class_factory_info; // 0x016F0A28
private:
    float width; // 0x2D0
    float length; // 0x2D4
};
