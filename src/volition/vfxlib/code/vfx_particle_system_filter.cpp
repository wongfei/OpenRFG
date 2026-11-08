// Original: volition/vfxlib/code/vfx_particle_system_filter.cpp
//
// The filter base (VFX.md FX1): the animation duration, the position and rotation keys (the rotation matrix is cached
// while there is a single key) and the base serialized block. The functions of vfx_particle_system_filter.h are here
// too (the original has them inline in the header).

#include "volition/vfxlib/code/vfx_particle_system_filter.h"

#include "volition/vfxlib/code/vfx_animation.h"

// 0x00C9BFA0 (.h:117)
vfx_particle_system_filter::vfx_particle_system_filter()
{
    m_animation_duration = 0.0f;
    m_local_rotation_cached_dirty = true;
}

// 0x00C9BFC0 (.h:123)
vfx_particle_system_filter::~vfx_particle_system_filter()
{
}

// 0x00CA0690 (:8): serialized_data_block (the filter types serialize their own blocks instead).
int vfx_particle_system_filter::serialize(void* data_p)
{
    const serialized_data_block* block = (const serialized_data_block*)data_p;
    m_num_position_keys = block->num_position_keys;
    m_position_keys_p = block->position_keys_p;
    m_num_rotation_keys = block->num_rotation_keys;
    m_rotation_keys_p = block->rotation_keys_p;
    return sizeof(serialized_data_block);
}

// 0x00C9BF90 (.h:71)
vfx_particle_system_filter::base_type vfx_particle_system_filter::get_base_type()
{
    return m_base_type;
}

// 0x00C9BFF0 (.h:138)
void vfx_particle_system_filter::get_local_position(vector& result, float animation_time) const
{
    vfx_interpolate_vector(result, animation_time / m_animation_duration, m_num_position_keys, m_position_keys_p);
}

// 0x00C9C030 (.h:144): the cached matrix while it is valid and there is a single key.
const matrix& vfx_particle_system_filter::get_local_rotation(float animation_time) const
{
    vfx_particle_system_filter* self = const_cast<vfx_particle_system_filter*>(this);
    if (!m_local_rotation_cached_dirty && m_num_rotation_keys == 1)
    {
        return m_local_rotation_cached;
    }
    quaternion rotation;
    vfx_interpolate_quaternion(rotation, animation_time / m_animation_duration, m_num_rotation_keys, m_rotation_keys_p);
    rotation.extract_matrix(self->m_local_rotation_cached);
    self->m_local_rotation_cached_dirty = false;
    return m_local_rotation_cached;
}

// 0x00C9BFD0 (.h:128)
float vfx_particle_system_filter::get_animation_duration() const
{
    return m_animation_duration;
}

// 0x00C9BFE0 (.h:133)
void vfx_particle_system_filter::set_animation_duration(float animation_duration)
{
    m_animation_duration = animation_duration;
}

// 0x00C9C0A0 (.h:166)
void vfx_particle_system_filter::set_num_position_keys(int num_position_keys)
{
    m_num_position_keys = num_position_keys;
}

// 0x00C9C0B0 (.h:177)
void vfx_particle_system_filter::set_position_keys(vfx_vector_key* position_keys_p)
{
    m_position_keys_p = position_keys_p;
}

// 0x00C9C0C0 (.h:188)
void vfx_particle_system_filter::set_num_rotation_keys(int num_rotation_keys)
{
    m_num_rotation_keys = num_rotation_keys;
}

// 0x00C9C0D0 (.h:199)
void vfx_particle_system_filter::set_rotation_keys(vfx_quat_key* rotation_keys_p)
{
    m_rotation_keys_p = rotation_keys_p;
    m_local_rotation_cached_dirty = true;
}
