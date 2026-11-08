// Original: volition/vfxlib/code/vfx_particle_system_filter.h
// Implementation: volition/vfxlib/code/vfx_particle_system_filter.cpp
//
// The filter base: a serialized filter applied to the particles of an emitter each frame (magnet, orientor, turbulence,
// vortex, wind), with its base type, strength curve and the virtual interface the filters implement.
#pragma once

#include "volition/render_lib/code/rl_base_object.h"
#include "volition/vlib/code/math/matrix.h"

class vector;
class vfx_particle_system_emitter_instance;
class vfx_particle_system_filter_instance;
struct vfx_quat_key;
struct vfx_vector_key;

// size 0x50
class vfx_particle_system_filter : public rl_base_object
{
public:
    enum base_type
    {
        BASE_TYPE_INTERACTOR = 0,
        BASE_TYPE_DEFLECTOR = 1,
        BASE_TYPE_INVALID = -1,
    };

    // size 0x10
    struct serialized_data_block
    {
        int num_position_keys; // 0x00
        vfx_vector_key* position_keys_p; // 0x04
        int num_rotation_keys; // 0x08
        vfx_quat_key* rotation_keys_p; // 0x0C
    };

protected:
    vfx_particle_system_filter(); // 0x00C9BFA0 (.h:117)
    virtual ~vfx_particle_system_filter(); // vtbl+0x00 0x00C9BFC0 (.h:123)
public:
    vfx_particle_system_filter* get_prev() const;
    void set_prev(vfx_particle_system_filter*);
    vfx_particle_system_filter* get_next() const;
    void set_next(vfx_particle_system_filter*);
    virtual int serialize(void* data_p); // vtbl+0x08 0x00CA0690 (:8)
    virtual void process_emitter(vfx_particle_system_emitter_instance*, vfx_particle_system_filter_instance*, float) = 0; // vtbl+0x0C
    virtual void update_instance(vfx_particle_system_filter_instance*, float) = 0; // vtbl+0x10
    base_type get_base_type(); // 0x00C9BF90 (.h:71)
    void get_local_position(vector& result, float animation_time) const; // 0x00C9BFF0 (.h:138)
    const matrix& get_local_rotation(float animation_time) const; // 0x00C9C030 (.h:144)
    float get_animation_duration() const; // 0x00C9BFD0 (.h:128)
    void set_animation_duration(float animation_duration); // 0x00C9BFE0 (.h:133)
    int get_num_position_keys() const;
    void set_num_position_keys(int num_position_keys); // 0x00C9C0A0 (.h:166)
    vfx_vector_key* get_position_keys() const;
    void set_position_keys(vfx_vector_key* position_keys_p); // 0x00C9C0B0 (.h:177)
    int get_num_rotation_keys() const;
    void set_num_rotation_keys(int num_rotation_keys); // 0x00C9C0C0 (.h:188)
    vfx_quat_key* get_rotation_keys() const;
    void set_rotation_keys(vfx_quat_key* rotation_keys_p); // 0x00C9C0D0 (.h:199)

    vfx_particle_system_filter* prev; // 0x08
    vfx_particle_system_filter* next; // 0x0C
protected:
    base_type m_base_type; // 0x10
private:
    float m_animation_duration; // 0x14
    int m_num_position_keys; // 0x18
    vfx_vector_key* m_position_keys_p; // 0x1C
    int m_num_rotation_keys; // 0x20
    vfx_quat_key* m_rotation_keys_p; // 0x24
    matrix m_local_rotation_cached; // 0x28
    bool m_local_rotation_cached_dirty; // 0x4C
};

#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vlib/code/util/endian_type.h"

// Code every filter type inlines (no symbol of its own in rfg.pdb):
// - the key offsets of a serialized block are relative to the block (0 and -1 mean null) and become pointers in place;
template<typename T>
inline void vfx_fixup_block_keys(et_sized_native_pointer<T>& keys, void* block_p)
{
    T* const offset = keys.to_pointer();
    if (offset == (T*)0xFFFFFFFF || offset == nullptr)
    {
        keys = (T*)nullptr;
    }
    else
    {
        keys = (T*)((char*)block_p + (int)offset);
    }
}

// - the filter instance's world transform is the filter's local position and rotation keys (at the instance's
//   animation time) under the parent transform.
inline void vfx_filter_update_world_transform(const vfx_particle_system_filter* filter_p, vfx_particle_system_filter_instance* instance_p)
{
    vector local_position;
    filter_p->get_local_position(local_position, instance_p->get_anim_time());
    const matrix& local_rotation = filter_p->get_local_rotation(instance_p->get_anim_time());
    matrix43 parent;
    parent.set_orientation(instance_p->get_parent_world_rotation());
    parent.set_translation(instance_p->get_parent_world_position());
    matrix43 local;
    local.set_orientation(local_rotation);
    local.set_translation(local_position);
    matrix43 world;
    world.multiply_two(local, parent);
    instance_p->set_world_rotation(world.m_rotation);
    instance_p->set_world_position(world.m_translation);
}
