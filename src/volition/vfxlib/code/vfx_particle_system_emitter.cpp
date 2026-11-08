// Original: volition/vfxlib/code/vfx_particle_system_emitter.cpp
//
// The emitter base (VFX.md FX1): defaults, the emit node list and the default birth velocity (a random direction in
// the yaw/pitch cone around the emitter's up axis). The accessors are in vfx_particle_system_emitter.inl.

#include "volition/vfxlib/code/vfx_particle_system_emitter.h"

#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// ---- file-static globals
// The defaults the constructor points at until the template sets its keys: one texture animation rate key of 1 and one
// unit scale key. set_scale_x/y/z write through these (shared by every emitter without keys).
static vfx_float_key Default_texture_anim_rate_keys = {0.0f, 1.0f}; // 0x016F0A10
static vfx_vector_key Temp_scale_keys = {0.0f, vector(1.0f, 1.0f, 1.0f)}; // 0x016F0F28

// 0x00CAF5A0 (:111)
vfx_particle_system_emitter::vfx_particle_system_emitter()
{
    num_tails = 4;
    max_tails = 600;
    m_weight_scale = 1.0f;
    iterations_per_sec = 30.0f;
    m_weight_start = 0.0f;
    m_weight_end = 0.0f;
    density_variance = 0.0f;
    density = 0.1f;
    m_particle_type = rl_particle_base::RENDER_TYPE_BILLBOARD;
    use_length = false;
    use_length_mid_value = false;
    local_space_emission = false;
    use_weight_variance = false;
    use_size_mid_value = false;
    use_color_mid_value = false;
    use_color_saturation_value = false;
    use_additive_key_value = false;
    use_density_variance = false;
    is_iterative = true;
    color_start = rl_color_float(1.0f, 1.0f, 1.0f, 1.0f);
    color_start_variance = rl_color_float(0.0f, 0.0f, 0.0f, 0.0f);
    color_end = rl_color_float(0.0f, 0.0f, 0.0f, 0.0f);
    color_end_variance = rl_color_float(0.0f, 0.0f, 0.0f, 0.0f);
    color_mid_A = rl_color_float(0.0f, 1.0f, 0.0f, 1.0f);
    color_mid_A_variance = rl_color_float(0.0f, 0.0f, 0.0f, 0.0f);
    color_mid_A_offset = 0.25f;
    color_mid_B = rl_color_float(0.0f, 0.0f, 1.0f, 1.0f);
    color_mid_B_variance = rl_color_float(0.0f, 0.0f, 0.0f, 0.0f);
    color_mid_B_offset = 0.75f;
    alpha_mid_A_offset = 0.25f;
    alpha_mid_B_offset = 0.75f;
    color_saturation_start = 1.0f;
    color_saturation_start_variance = 0.0f;
    color_saturation_end = 1.0f;
    color_saturation_end_variance = 0.0f;
    color_saturation_mid_A = 0.25f;
    color_saturation_mid_A_variance = 0.0f;
    color_saturation_mid_B = 0.75f;
    color_saturation_mid_B_variance = 0.0f;
    acceleration_start.set(0.0f, 0.0f, 0.0f);
    acceleration_start_variance.set(0.0f, 0.0f, 0.0f);
    acceleration_mid.set(0.0f, 0.0f, 0.0f);
    acceleration_mid_variance.set(0.0f, 0.0f, 0.0f);
    acceleration_end.set(0.0f, 0.0f, 0.0f);
    acceleration_end_variance.set(0.0f, 0.0f, 0.0f);
    additive_key_start = 1.0f;
    m_size_scale = 1.0f;
    m_emission_rate_scale = 1.0f;
    m_opacity_scale = 1.0f;
    m_lod_emission_distance = -1.0f;
    use_bounce_variance = false;
    use_lifetime_variance = false;
    m_renderable_p = nullptr;
    m_inherit_velocity = false;
    yaw_variance = 0.0f;
    pitch_variance = 0.0f;
    speed_variance = 0.0f;
    is_reverse_render = false;
    use_accel_values = true;
    use_accel_variance = false;
    use_accel_mid_value = false;
    use_color_variance = true;
    is_color_variance_by_value = true;
    additive_key_start_variance = 0.0f;
    additive_key_end = 0.0f;
    additive_key_end_variance = 0.0f;
    additive_key_mid_A = 0.25f;
    additive_key_mid_A_variance = 0.0f;
    additive_key_mid_B = 0.75f;
    additive_key_mid_B_variance = 0.0f;
    is_rotation_enabled = false;
    is_rotation_dir_dependant = true;
    is_random_start_rotation = true;
    angular_speed_start_variance = 0.0f;
    angular_speed_end_variance = 0.0f;
    use_size_variance = true;
    use_length_variance = true;
    bounce = 0.1f;
    bounce_variance = 0.0f;
    max_emission = 100;
    lifetime = 2.0f;
    lifetime_variance = 0.0f;
    max_particles = 100;
    num_scale_keys = 1;
    scale_keys = &Temp_scale_keys;
    m_num_texture_anim_rate_keys = 1;
    m_texture_anim_rate_keys = &Default_texture_anim_rate_keys;
}

// 0x00CAF930 (:116)
vfx_particle_system_emitter::~vfx_particle_system_emitter()
{
}

// 0x00C9F0A0 (:121)
void vfx_particle_system_emitter::init()
{
}

// 0x00C9F0B0 (:127)
void vfx_particle_system_emitter::finalize()
{
}

// 0x00C9F0C0 (:133): a direction in the cone (yaw about the up axis, pitch off it) in the emitter's world rotation.
// Notes (decompiled behaviour, kept as is): every angle is a fresh random number, so the yaw of the x part and of the
// up part and the pitch of the length and of the forward part all differ (four fl_rand calls, global generator).
// The direction is unscaled: the emitter types set the speed.
void vfx_particle_system_emitter::initiate_velocity(vfx_particle_base* particle_p, vfx_particle_system_emitter_instance* instance_p)
{
    const float pitch_length = pitch_variance * (fl_rand() * 2.0f - 1.0f);
    const float cos_pitch = (float)cos((double)pitch_length);
    const float yaw_x = yaw_variance * (fl_rand() * 2.0f - 1.0f);
    const float x = (float)sin((double)yaw_x) * cos_pitch;
    const float yaw_up = yaw_variance * (fl_rand() * 2.0f - 1.0f);
    const float up = (float)cos((double)yaw_up) * cos_pitch;
    const float pitch_forward = pitch_variance * (fl_rand() * 2.0f - 1.0f);
    const float forward = (float)sin((double)pitch_forward);
    const matrix& rotation = instance_p->get_current_world_matrix().m_rotation;
    particle_p->velocity.x = rotation.rvec.x * x + rotation.uvec.x * up + forward * rotation.fvec.x;
    particle_p->velocity.y = rotation.uvec.y * up + rotation.rvec.y * x + rotation.fvec.y * forward;
    particle_p->velocity.z = rotation.rvec.z * x + rotation.uvec.z * up + forward * rotation.fvec.z;
}

// 0x00C9F2F0 (:151): the base has no serialized data.
int vfx_particle_system_emitter::serialize(void* data_p)
{
    return 0;
}

// 0x00CAF970 (:159): the list uses the template's array (emitter pointers in place of the indices).
void vfx_particle_system_emitter::set_emit_node_emitter_list(et_sized_native_pointer<vfx_particle_system_emitter>* emitters, int num_emitters)
{
    m_emit_node_emitter_list.init(emitters, num_emitters, num_emitters);
}
