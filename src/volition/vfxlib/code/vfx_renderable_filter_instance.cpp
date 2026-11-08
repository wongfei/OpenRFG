// Original: volition/vfxlib/code/vfx_renderable_filter_instance.cpp
//
// A renderable filter instance (VFX.md FX1): an effect mesh driven by a rigid body. Each update computes the force and
// spin the filter keys ask for (omni: away from the filter, falling off with the distance squared; directional: along
// the filter's forward axis), hands them to the game's rigid body update callback (effect debris, VFX.md 1.9) and moves
// the mesh's renderable instance to the body. The functions of vfx_renderable_filter_instance.h are here too (the
// original has them inline in the header).

#include "volition/vfxlib/code/vfx_renderable_filter_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/vmemory.h"

// The class factory (vfx_renderable_filter_instance.cpp:7): class id 0x349BE7F6, 100 instances.
// <vfx_renderable_filter_instance::factory_init> 0x00CB1520 (:7)
// <vfx_renderable_filter_instance::release> 0x00CB1620 (:7)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_renderable_filter_instance, 0x349BE7F6, 100)

// Magic number: the force and spin keys are per tenth of a second (scaled by delta_time x 10).
static const float VFX_RENDERABLE_FORCE_SCALE = 10.0f;

// 0x00C9D760 (.h:132)
vfx_renderable_filter_instance::vfx_renderable_filter_instance()
{
    m_rigid_body_handle.invalidate();
    m_renderable_instance_p = nullptr;
    m_parent_renderable_instance_p = nullptr;
    m_force_type = FORCE_TYPE_OMNI;
    m_position_keys_p = nullptr;
    m_rotation_keys_p = nullptr;
    m_force_keys_p = nullptr;
    m_noise_keys_p = nullptr;
    m_angular_velocity_keys_p = nullptr;
    m_angular_velocity_variance_keys_p = nullptr;
    m_first_update = true;
    m_center_of_mass = ZERO_VECTOR;
    m_effect_index = -1;
}

// 0x00C9D7D0 (.h:149)
vfx_renderable_filter_instance::~vfx_renderable_filter_instance()
{
}

// 0x00CA8E30 (:13): the random spin axis (rand_sphere) and spin spread (fl_rand) use the global generator.
void vfx_renderable_filter_instance::update(float delta_time)
{
    const float anim_time = m_anim_time;
    vfx_physics_rigid_body_update_data data;
    float force;
    vfx_interpolate_varkey<vfx_float_key, float>(force, m_num_force_keys, m_force_keys_p, anim_time);
    force = force * delta_time * VFX_RENDERABLE_FORCE_SCALE;
    if (force <= 0.0f || m_first_update)
    {
        data.m_force_vector = ZERO_VECTOR;
        m_first_update = false;
        data.m_angular_force_vector = ZERO_VECTOR;
    }
    else
    {
        vector local_position;
        get_local_position(local_position, anim_time);
        const matrix& local_rotation = get_local_rotation(m_anim_time);
        matrix43 parent;
        parent.set_orientation(m_parent_world_rotation);
        parent.set_translation(m_parent_world_position);
        matrix43 local;
        local.set_orientation(local_rotation);
        local.set_translation(local_position);
        vector world_position;
        world_position.x = parent.m_translation.x + local.m_translation.x * parent.m_rotation.rvec.x + parent.m_rotation.uvec.x * local.m_translation.y + parent.m_rotation.fvec.x * local.m_translation.z;
        world_position.y = parent.m_translation.y + local.m_translation.x * parent.m_rotation.rvec.y + parent.m_rotation.uvec.y * local.m_translation.y + parent.m_rotation.fvec.y * local.m_translation.z;
        world_position.z = parent.m_translation.z + local.m_translation.x * parent.m_rotation.rvec.z + parent.m_rotation.uvec.z * local.m_translation.y + parent.m_rotation.fvec.z * local.m_translation.z;
        const matrix43 world(world_position, local.m_rotation * parent.m_rotation);
        vector direction = world.m_rotation.fvec;
        const vector& body_position = m_renderable_instance_p->get_position();
        vector offset;
        offset.x = (body_position.x + m_center_of_mass.x) - world.m_translation.x;
        offset.y = (m_center_of_mass.y + body_position.y) - world.m_translation.y;
        offset.z = (m_center_of_mass.z + body_position.z) - world.m_translation.z;
        float distance_squared = offset.y * offset.y + offset.x * offset.x + offset.z * offset.z;
        float noise;
        vfx_interpolate_varkey<vfx_float_key, float>(noise, m_num_noise_keys, m_noise_keys_p, m_anim_time);
        vector spin_axis = ZERO_VECTOR;
        spin_axis.rand_sphere();
        float spin_variance;
        vfx_interpolate_varkey<vfx_float_key, float>(spin_variance, m_num_angular_velocity_variance_keys, m_angular_velocity_variance_keys_p, m_anim_time);
        const float spin_spread = vfx_float_random(-spin_variance, spin_variance);
        float spin;
        vfx_interpolate_varkey<vfx_float_key, float>(spin, m_num_angular_velocity_keys, m_angular_velocity_keys_p, m_anim_time);
        spin = (spin + spin_spread) * delta_time * VFX_RENDERABLE_FORCE_SCALE;
        if (m_force_type == FORCE_TYPE_OMNI)
        {
            if (distance_squared <= 0.0f || fl_is_nan(distance_squared))
            {
                data.m_force_vector = ZERO_VECTOR;
                data.m_angular_force_vector = ZERO_VECTOR;
            }
            else
            {
                float falloff = 1.0f / distance_squared;
                if (falloff - 0.0f < 0.0f)
                {
                    falloff = 0.0f;
                }
                if (0.0f <= falloff - 1.0f)
                {
                    falloff = 1.0f;
                }
                direction = offset;
                direction.normalize_safe();
                direction.rand_around_dir(direction, 1.0f - noise, 1.0f);
                const float scaled_force = falloff * force;
                data.m_force_vector.x = scaled_force * direction.x;
                data.m_force_vector.y = direction.y * scaled_force;
                data.m_force_vector.z = direction.z * scaled_force;
                const float scaled_spin = falloff * spin;
                data.m_angular_force_vector.x = spin_axis.x * scaled_spin;
                data.m_angular_force_vector.y = spin_axis.y * scaled_spin;
                data.m_angular_force_vector.z = spin_axis.z * scaled_spin;
            }
        }
        else if (m_force_type == FORCE_TYPE_DIRECTIONAL)
        {
            direction.rand_around_dir(direction, 1.0f - noise, 1.0f);
            data.m_force_vector.x = direction.x * force;
            data.m_force_vector.y = direction.y * force;
            data.m_force_vector.z = direction.z * force;
            data.m_angular_force_vector.x = spin_axis.x * spin;
            data.m_angular_force_vector.y = spin_axis.y * spin;
            data.m_angular_force_vector.z = spin_axis.z * spin;
        }
    }
    data.m_handle = m_rigid_body_handle;
    vfx_system::get_rigid_body_update_cb()(data);
    m_renderable_instance_p->set_position(data.m_transform.m_translation);
    m_renderable_instance_p->set_rotation(data.m_transform.m_rotation);
    m_renderable_instance_p->update_partition_system();
}

// 0x00C9D800 (.h:170)
void vfx_renderable_filter_instance::set_parent_world_rotation(const matrix& parent_world_rotation)
{
    m_parent_world_rotation = parent_world_rotation;
}

// 0x00C9DA40 (.h:369)
rl_handle vfx_renderable_filter_instance::get_rigid_body_handle() const
{
    return m_rigid_body_handle;
}

// 0x00C9D850 (.h:186)
void vfx_renderable_filter_instance::get_local_position(vector& result, float animation_time) const
{
    vfx_interpolate_vector(result, animation_time / m_animation_duration, m_num_position_keys, m_position_keys_p);
}

// 0x00C9D890 (.h:192): cached while there is a single key.
const matrix& vfx_renderable_filter_instance::get_local_rotation(float animation_time) const
{
    vfx_renderable_filter_instance* self = const_cast<vfx_renderable_filter_instance*>(this);
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

// 0x00C9D7E0 (.h:159)
void vfx_renderable_filter_instance::set_parent_world_position(const vector& parent_world_position)
{
    m_parent_world_position = parent_world_position;
}

// 0x00C9D910 (.h:212)
void vfx_renderable_filter_instance::set_num_force_keys(int num_force_keys)
{
    m_num_force_keys = num_force_keys;
}

// 0x00C9D920 (.h:223)
void vfx_renderable_filter_instance::set_force_keys(vfx_float_key* force_keys_p)
{
    m_force_keys_p = force_keys_p;
}

// 0x00C9D930 (.h:233)
void vfx_renderable_filter_instance::set_num_noise_keys(int num_noise_keys)
{
    m_num_noise_keys = num_noise_keys;
}

// 0x00C9D940 (.h:244)
void vfx_renderable_filter_instance::set_noise_keys(vfx_float_key* noise_keys_p)
{
    m_noise_keys_p = noise_keys_p;
}

// 0x00C9D950 (.h:255)
void vfx_renderable_filter_instance::set_num_angular_velocity_keys(int num_angular_velocity_keys)
{
    m_num_angular_velocity_keys = num_angular_velocity_keys;
}

// 0x00C9D960 (.h:265)
void vfx_renderable_filter_instance::set_angular_velocity_keys(vfx_float_key* angular_velocity_keys_p)
{
    m_angular_velocity_keys_p = angular_velocity_keys_p;
}

// 0x00C9D970 (.h:276)
void vfx_renderable_filter_instance::set_num_angular_velocity_variance_keys(int num_angular_velocity_variance_keys)
{
    m_num_angular_velocity_variance_keys = num_angular_velocity_variance_keys;
}

// 0x00C9D980 (.h:286)
void vfx_renderable_filter_instance::set_angular_velocity_variance_keys(vfx_float_key* angular_velocity_variance_keys_p)
{
    m_angular_velocity_variance_keys_p = angular_velocity_variance_keys_p;
}

// 0x00C9D990 (.h:296)
void vfx_renderable_filter_instance::set_num_position_keys(int num_position_keys)
{
    m_num_position_keys = num_position_keys;
}

// 0x00C9D9A0 (.h:307)
void vfx_renderable_filter_instance::set_position_keys(vfx_vector_key* position_keys_p)
{
    m_position_keys_p = position_keys_p;
}

// 0x00C9D9B0 (.h:318)
void vfx_renderable_filter_instance::set_num_rotation_keys(int num_rotation_keys)
{
    m_num_rotation_keys = num_rotation_keys;
}

// 0x00C9D9C0 (.h:329)
void vfx_renderable_filter_instance::set_rotation_keys(vfx_quat_key* rotation_keys_p)
{
    m_rotation_keys_p = rotation_keys_p;
    m_local_rotation_cached_dirty = true;
}

// 0x00C9D840 (.h:181)
void vfx_renderable_filter_instance::set_animation_duration(float animation_duration)
{
    m_animation_duration = animation_duration;
}

// 0x00C9D9E0 (.h:341)
void vfx_renderable_filter_instance::set_anim_time(float anim_time)
{
    m_anim_time = anim_time;
}

// 0x00C9DA00 (.h:347)
rl_renderable_instance* vfx_renderable_filter_instance::get_renderable_instance() const
{
    return m_renderable_instance_p;
}

// 0x00C9DA10 (.h:352)
void vfx_renderable_filter_instance::set_renderable_instance(rl_renderable_instance* renderable_instance_p)
{
    m_renderable_instance_p = renderable_instance_p;
}

// 0x00C9DA20 (.h:358)
rl_renderable_instance* vfx_renderable_filter_instance::get_parent_renderable_instance() const
{
    return m_parent_renderable_instance_p;
}

// 0x00C9DA30 (.h:363)
void vfx_renderable_filter_instance::set_parent_renderable_instance(rl_renderable_instance* parent_renderable_instance_p)
{
    m_parent_renderable_instance_p = parent_renderable_instance_p;
}

// 0x00C9DA50 (.h:374)
void vfx_renderable_filter_instance::set_rigid_body_handle(rl_handle rigid_body_handle)
{
    m_rigid_body_handle = rigid_body_handle;
}

// 0x00C9DA60 (.h:385)
void vfx_renderable_filter_instance::set_center_of_mass(const vector& center_of_mass)
{
    m_center_of_mass = center_of_mass;
}

// 0x00C9DA80 (.h:391)
int vfx_renderable_filter_instance::get_effect_index() const
{
    return m_effect_index;
}

// 0x00C9DA90 (.h:396)
void vfx_renderable_filter_instance::set_effect_index(int effect_index)
{
    m_effect_index = effect_index;
}

