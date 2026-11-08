// Original: volition/render_lib/code/rl_particle_batch.h
// Implementation: volition/render_lib/code/rl_particle_batch.cpp
//
// rl_particle_batch, particles of one material batched for drawing.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vlib/code/memory/object_pool.h"

class rl_class_dynamic_info;
class rl_renderable_particles;
class rl_renderable_particles_instance;

// size 0x38
class rl_particle_batch : public rl_base_object
{
public:
    // size 0xC
    struct batched_instance
    {
        rl_renderable_particles_instance* instance_p; // 0x00
        batched_instance* prev; // 0x04
        batched_instance* next; // 0x08
    };

    typedef object_pool_client<batched_instance,1> particle_batch_client;

    rl_particle_batch* get_prev() const;
    void set_prev(rl_particle_batch*);
    rl_particle_batch* get_next() const;
    void set_next(rl_particle_batch*);
protected:
    rl_particle_batch(); // 0x00513960 (:48)
    virtual ~rl_particle_batch(); // vtbl+0x00 0x0054C670 (:55)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_particle_batch* create(); // 0x00529790 (:44)
    static bool can_create(); // 0x004C3B90 (:44)
    virtual void release(); // vtbl+0x04 0x004C3BA0 (:44)
    unsigned int get_material_handle() const;
    void set_material_handle(unsigned int);
    bool add_instance(rl_renderable_particles_instance* instance_p); // 0x005297F0 (:61)
    void finalize();
    void render(); // 0x004C3BD0 (:84)
private:
    void prerender(); // 0x004C3BC0 (:79)

public:
    rl_particle_batch* prev; // 0x08
    rl_particle_batch* next; // 0x0C
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x01514F7C
private:
    unsigned int m_material_handle; // 0x10
    rl_renderable_particles* m_renderable_p; // 0x14
    object_pool_client<batched_instance,1> m_batched_instances; // 0x18
};
