// Original: volition/vfxlib/code/vfx_particle_system_emitter_instance.cpp
//
// An emitter instance (VFX.md FX1): its particles (taken from and returned to the particle system manager's free lists),
// the emission accumulator, spawning (random draws from the global generator, VFX.md 2.5), the iterative simulation
// (velocity, drag, acceleration, rotation, size, length, color, saturation and additive key over the particle's life),
// tails, and the copy into the renderable particles instance. The accessors are in the .inl.
//
// Precision: parts of this file were x87 code in rfg.exe (53-bit precision); where the decompile shows double
// arithmetic, the port computes in double (interpolate_two_values<float>, interpolate_four_values<float> and their
// inlined uses).

#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_particle_system_filter.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_manager.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector4.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <math.h>

// ---- statics
const vector vfx_particle_system_emitter_instance::SCALE_IDENTITY_VECTOR(1.0f, 1.0f, 1.0f); // 0x016F1010
// The random draws of the particle being spawned (spawn_internal sets them; compute_variant_color reads them).
float vfx_particle_system_emitter_instance::ms_random_float; // 0x03A23C90
vector vfx_particle_system_emitter_instance::ms_random_vector; // 0x03A23C94

// The class factory (vfx_particle_system_emitter_instance.cpp:17): class id 0x5EE95F7B, 1600 instances.
// <vfx_particle_system_emitter_instance::factory_init> 0x00CAFD80 (:17)
// <vfx_particle_system_emitter_instance::release> 0x00CAFE80 (:17)
// <vfx_particle_system_emitter_instance::create> 0x00CB5E30 (:17)
RL_FACTORY_CLASS_IMPLEMENTATION(vfx_particle_system_emitter_instance, 0x5EE95F7B, 1600)

// Magic numbers: rotation start angles span [-2 pi, 2 pi]; an angle of exactly 0 that must turn negative becomes 2 pi.
static const float VFX_TWO_PI = 6.2831855f;
static const float VFX_FOUR_PI = 12.566371f;

// 0x006E6F70 (.h:19)
int vfx_particle_system_emitter_instance::get_objects_free()
{
    return ms_object_pool.size();
}

// Interpolation over a particle's life (:194 and :200 in the original, member templates there). The float versions
// were x87 code: computed in double, rounded to float on return.
static float interpolate_two_values(const float& _start, const float& _end, float _ratio) // <float> 0x00CA2860 (:194)
{
    return (float)((1.0 - (double)_ratio) * (double)_start + (double)_end * (double)_ratio);
}

// <vector> 0x00CA2890 (:194)
static vector interpolate_two_values(const vector& _start, const vector& _end, float _ratio)
{
    const float inverse = 1.0f - _ratio;
    return vector(_start.x * inverse + _end.x * _ratio, _end.y * _ratio + _start.y * inverse, _start.z * inverse + _end.z * _ratio);
}

// <rl_color_float> 0x00CA2A50 (:194)
static rl_color_float interpolate_two_values(const rl_color_float& _start, const rl_color_float& _end, float _ratio)
{
    const float inverse = 1.0f - _ratio;
    return rl_color_float(_start.red * inverse + _end.red * _ratio, _start.green * inverse + _end.green * _ratio, _start.blue * inverse + _end.blue * _ratio, _start.alpha * inverse + _end.alpha * _ratio);
}

// <float> 0x00CA2980 (:200): start, mid A, mid B and end at 0, the two offsets and 1; the segment ratio is rounded to
// float before the blend.
static float interpolate_four_values(const float& _start, const float& _mid_A, const float& _mid_B, const float& _end, float _mid_A_offset, float _mid_B_offset, float _ratio)
{
    if (_ratio <= _mid_A_offset)
    {
        const float t = (float)((double)_ratio / (double)_mid_A_offset);
        return (float)((1.0 - (double)t) * (double)_start + (double)_mid_A * (double)t);
    }
    if (_ratio <= _mid_B_offset)
    {
        const float t = (float)(((double)_ratio - (double)_mid_A_offset) / ((double)_mid_B_offset - (double)_mid_A_offset));
        return (float)((1.0 - (double)t) * (double)_mid_A + (double)_mid_B * (double)t);
    }
    const float t = (float)(((double)_ratio - (double)_mid_B_offset) / (1.0 - (double)_mid_B_offset));
    return (float)((double)_mid_B * (1.0 - (double)t) + (double)_end * (double)t);
}

// <rl_color_float> 0x00CAB390 (:200)
static rl_color_float interpolate_four_values(const rl_color_float& _start, const rl_color_float& _mid_A, const rl_color_float& _mid_B, const rl_color_float& _end, float _mid_A_offset, float _mid_B_offset, float _ratio)
{
    const rl_color_float* from;
    const rl_color_float* to;
    float t;
    if (_ratio <= _mid_A_offset)
    {
        t = _ratio / _mid_A_offset;
        from = &_start;
        to = &_mid_A;
    }
    else if (_ratio <= _mid_B_offset)
    {
        t = (_ratio - _mid_A_offset) / (_mid_B_offset - _mid_A_offset);
        from = &_mid_A;
        to = &_mid_B;
    }
    else
    {
        t = (_ratio - _mid_B_offset) / (1.0f - _mid_B_offset);
        from = &_mid_B;
        to = &_end;
    }
    const float inverse = 1.0f - t;
    return rl_color_float(from->red * inverse + to->red * t, from->green * inverse + to->green * t, from->blue * inverse + to->blue * t, from->alpha * inverse + to->alpha * t);
}

// 0x00CAFF00 (:54)
vfx_particle_system_emitter_instance::vfx_particle_system_emitter_instance()
{
    current_elapsed_time = 0.0f;
    m_pre_update_cb = nullptr;
    m_post_update_cb = nullptr;
    m_particle_create_cb = nullptr;
    m_emitter_p = nullptr;
    m_has_alternate_view = false;
    tail_spawn_count = 0;
    m_texture_anim_rate = 1.0f;
    m_is_visible = true;
    m_update_parametric = true;
    m_last_emission_rate_non_zero = false;
    m_particle_iterative_list.init();
    m_particle_parametric_list.init();
    m_particle_iterative_list_shared_p = nullptr;
    m_particle_parametric_list_shared_p = nullptr;
    particle_tails_list_shared = nullptr;
    m_parent_transform = IDENTITY_MATRIX43;
    m_parent_renderable_instance_p = nullptr;
    m_renderable_particles_instance_p = nullptr;
    num_alive = 0;
    num_dead = 0;
    m_emission_rate_calculated = false;
    m_update_tail_count = 0;
    m_is_emitted_node = false;
    m_emit_node_parent_p = nullptr;
    emission_enabled = true;
    last_spawn_time = 0.0f;
    current_anim_time = 0.0f;
}

// 0x00CB5EB0 (:59)
vfx_particle_system_emitter_instance::~vfx_particle_system_emitter_instance()
{
    finalize();
}

// 0x00CB0030 (:65): every particle's tails back to the shared tail list (only when tails were spawned at the start).
void vfx_particle_system_emitter_instance::release_tails()
{
    const int spawned_tails = tail_spawn_count;
    vfx_particle_iterative* particle_p = m_particle_iterative_list.first();
    while (particle_p != nullptr)
    {
        vfx_particle_iterative* next_p = particle_p->next != m_particle_iterative_list.first() ? particle_p->next : nullptr;
        if (spawned_tails > 0)
        {
            tail_spawn_count -= particle_p->tails_list.get_elem_count();
            particle_tails_list_shared->concat(particle_p->tails_list);
        }
        particle_p = next_p;
    }
}

// 0x00CB00E0 (:79): every particle back to the shared free lists.
void vfx_particle_system_emitter_instance::finalize()
{
    release_tails();
    num_dead += num_alive;
    num_alive = 0;
    m_particle_iterative_list_shared_p->concat(m_particle_iterative_list);
    m_particle_parametric_list_shared_p->concat(m_particle_parametric_list);
}

// 0x00CB5F10 (:91)
void vfx_particle_system_emitter_instance::init(vfx_particle_system_emitter* emitter_p)
{
    m_emitter_p = emitter_p;
    m_particle_iterative_list.init();
    m_particle_parametric_list.init();
    vfx_particle_system_manager* manager_p = vfx_system::get_particle_system_manager();
    m_particle_iterative_list_shared_p = &manager_p->get_particle_iterative_list();
    m_particle_parametric_list_shared_p = &manager_p->get_particle_parametric_list();
    particle_tails_list_shared = &manager_p->get_particle_tails_list();
    reset();
    last_spawn_time = 0.0f;
    m_first_update = true;
}

// 0x00CB0130 (:119): kills every particle (lifetime 0, they die on the next update) and returns the tails.
void vfx_particle_system_emitter_instance::reset()
{
    vfx_particle_iterative* iterative_p = m_particle_iterative_list.first();
    while (iterative_p != nullptr)
    {
        iterative_p->lifetime = 0.0f;
        iterative_p = iterative_p->next;
        if (iterative_p == m_particle_iterative_list.first())
        {
            break;
        }
    }
    vfx_particle_parametric* parametric_p = m_particle_parametric_list.first();
    while (parametric_p != nullptr)
    {
        parametric_p->lifetime = 0.0f;
        parametric_p = parametric_p->next;
        if (parametric_p == m_particle_parametric_list.first())
        {
            break;
        }
    }
    release_tails();
    num_alive = 0;
    tail_spawn_count = 0;
    m_emit_number = 0;
    num_dead = m_emitter_p->max_particles;
    current_elapsed_time = 0.0f;
    current_anim_time = 0.0f;
    last_spawn_time = 0.0f;
    emission_enabled = true;
}

// 0x00CB5F80 (:155): the emission accumulator (capped at the particle lifetime) spawns rate x time particles; the
// remainder carries over.
void vfx_particle_system_emitter_instance::compute_iteration(float elapsed_time, float delta_time)
{
    last_spawn_time = delta_time + last_spawn_time;
    if (m_emitter_p->lifetime < last_spawn_time)
    {
        last_spawn_time = m_emitter_p->lifetime;
    }
    float emission_rate = get_emission_rate();
    const int count = (int)(emission_rate * last_spawn_time);
    create_particles(count, elapsed_time);
    // Magic number: a zero or negative rate divides by 1e-6 (the accumulator then empties).
    if (emission_rate <= 0.0f)
    {
        emission_rate = 1e-06f;
    }
    last_spawn_time = last_spawn_time - (float)count / emission_rate;
    if (last_spawn_time < 0.0f)
    {
        last_spawn_time = 0.0f;
    }
}

// 0x00CA4610 (:223): parametric particles die in birth order, so the scan stops at the first live one (ribbons scan
// them all).
void vfx_particle_system_emitter_instance::param_particle_death_check()
{
    vfx_particle_parametric* particle_p = m_particle_parametric_list.first();
    while (particle_p != nullptr)
    {
        vfx_particle_parametric* next_p = particle_p->next != m_particle_parametric_list.first() ? particle_p->next : nullptr;
        if (particle_p->lifetime + particle_p->time_created < current_elapsed_time)
        {
            num_alive--;
            num_dead++;
            m_particle_parametric_list.remove(particle_p);
            m_particle_parametric_list_shared_p->append(particle_p);
        }
        else if (m_emitter_p->m_particle_type != rl_particle_base::RENDER_TYPE_RIBBON)
        {
            return;
        }
        particle_p = next_p;
    }
}

// 0x00C9FCB0 (:248): local position, rotation and scale from the emitter's keys at the current animation time, in
// world space (through the parent transform) unless the emitter emits in local space; the scale scales the axes.
void vfx_particle_system_emitter_instance::update_internal_transforms()
{
    matrix43 local;
    m_emitter_p->get_local_position(local.m_translation, current_anim_time);
    m_emitter_p->get_local_rotation(local.m_rotation, current_anim_time);
    vector scale;
    m_emitter_p->get_local_scale(scale, current_anim_time);
    if (!m_emitter_p->local_space_emission)
    {
        current_world_matrix.multiply_two(local, m_parent_transform);
    }
    else
    {
        current_world_matrix = local;
    }
    current_world_matrix.m_rotation.rvec.x = scale.x * current_world_matrix.m_rotation.rvec.x;
    current_world_matrix.m_rotation.rvec.y = current_world_matrix.m_rotation.rvec.y * scale.x;
    current_world_matrix.m_rotation.rvec.z = current_world_matrix.m_rotation.rvec.z * scale.x;
    current_world_matrix.m_rotation.uvec.x = scale.y * current_world_matrix.m_rotation.uvec.x;
    current_world_matrix.m_rotation.uvec.y = current_world_matrix.m_rotation.uvec.y * scale.y;
    current_world_matrix.m_rotation.uvec.z = current_world_matrix.m_rotation.uvec.z * scale.y;
    current_world_matrix.m_rotation.fvec.x = current_world_matrix.m_rotation.fvec.x * scale.z;
    current_world_matrix.m_rotation.fvec.y = current_world_matrix.m_rotation.fvec.y * scale.z;
    current_world_matrix.m_rotation.fvec.z = current_world_matrix.m_rotation.fvec.z * scale.z;
}

// 0x00CA4710 (:272): once per update; zero beyond the emitter's LOD emission distance from the view (measured from the
// parent position).
float vfx_particle_system_emitter_instance::get_emission_rate()
{
    if (!m_emission_rate_calculated)
    {
        const vector& view_position = vfx_system::get_view_position();
        if (0.0f <= m_emitter_p->m_lod_emission_distance)
        {
            const float dx = m_parent_transform.m_translation.x - view_position.x;
            const float dy = m_parent_transform.m_translation.y - view_position.y;
            const float dz = m_parent_transform.m_translation.z - view_position.z;
            if (m_emitter_p->m_lod_emission_distance * m_emitter_p->m_lod_emission_distance < dx * dx + dy * dy + dz * dz)
            {
                m_emission_rate_cached = 0.0f;
                m_emission_rate_calculated = true;
                return m_emission_rate_cached;
            }
        }
        m_emission_rate_cached = m_emitter_p->get_emission_rate(current_anim_time);
        m_emission_rate_calculated = true;
    }
    return m_emission_rate_cached;
}

// 0x00CB6090 (:292): one frame; returns the live particle count.
int vfx_particle_system_emitter_instance::update(float delta_time, float anim_time)
{
    vfx_particle_iterative* const node_parent_p = m_emit_node_parent_p;
    m_emission_rate_calculated = false;
    float step_time = delta_time;
    if (node_parent_p == nullptr)
    {
        // The animation wrapped (the inlined float comparison: anim_time below current_anim_time by more than 0.0001
        // times the larger magnitude, at least 1): the accumulator restarts when the rate is animated.
        float scale = fabsf(anim_time);
        if (fabsf(anim_time) - fabsf(current_anim_time) < 0.0f)
        {
            scale = fabsf(current_anim_time);
        }
        if (0.0f <= 1.0f - scale)
        {
            scale = 1.0f;
        }
        if (anim_time < current_anim_time - scale * 0.0001f && m_emitter_p->get_num_emission_rate_keys() > 1)
        {
            last_spawn_time = 0.0f;
        }
    }
    current_anim_time = anim_time;
    current_elapsed_time = delta_time + current_elapsed_time;
    if (node_parent_p != nullptr)
    {
        // An emitted node follows its parent particle's life.
        current_anim_time = m_emitter_p->animation_duration * node_parent_p->iter_life_ratio;
        if (current_anim_time < 0.0f)
        {
            current_anim_time = 0.0f;
        }
    }
    const float emission_rate = get_emission_rate();
    if (m_first_update || (!m_last_emission_rate_non_zero && emission_rate != 0.0f))
    {
        m_first_update = false;
        update_internal_transforms();
        previous_spawn_position = current_world_matrix.m_translation;
        current_spawn_position = current_world_matrix.m_translation;
    }
    m_last_emission_rate_non_zero = emission_rate != 0.0f;
    if (m_pre_update_cb != nullptr)
    {
        m_pre_update_cb(this, m_update_cb_data_p);
    }
    vfx_particle_system_emitter* emitter_p = m_emitter_p;
    m_texture_anim_rate = emitter_p->get_texture_anim_rate(current_anim_time);
    if (emitter_p->lifetime < delta_time)
    {
        step_time = emitter_p->lifetime;
    }
    const bool has_tails = emitter_p->num_tails > 0 || tail_spawn_count > 0;
    if (!emitter_p->is_iterative)
    {
        compute_iteration((current_elapsed_time - step_time) + step_time, step_time);
        param_particle_death_check();
    }
    else
    {
        int iterations;
        if (emitter_p->iterations_per_sec == 0.0f)
        {
            iterations = 1;
        }
        else
        {
            iterations = (int)(emitter_p->iterations_per_sec * step_time + 0.5f);
            if (iterations == 0)
            {
                iterations = 1;
            }
        }
        const float start_time = current_elapsed_time - step_time;
        const float iteration_time = step_time / (float)iterations;
        for (int iteration = 1; iteration <= iterations; iteration++)
        {
            compute_iteration((float)iteration * iteration_time + start_time, iteration_time);
            apply_interactors(iteration_time);
            vfx_particle_iterative* particle_p = m_particle_iterative_list.first();
            while (particle_p != nullptr)
            {
                vfx_particle_iterative* next_p = particle_p->next != m_particle_iterative_list.first() ? particle_p->next : nullptr;
                emitter_p = m_emitter_p;

                // A tail sample (drops keep 8, sampled every round(length) updates when they have a length).
                int max_tails = emitter_p->num_tails;
                if (emitter_p->m_particle_type == rl_particle_base::RENDER_TYPE_DROP)
                {
                    max_tails = 8;
                }
                bool sample_tail = true;
                if (emitter_p->m_particle_type == rl_particle_base::RENDER_TYPE_DROP && emitter_p->use_length)
                {
                    unsigned int period = (unsigned int)(particle_p->current_length + 0.5f);
                    if ((int)period < 1)
                    {
                        period = 1;
                    }
                    sample_tail = m_update_tail_count % period == 0;
                }
                if (max_tails > 0 && sample_tail && particle_tails_list_shared->first() != nullptr)
                {
                    vfx_particle_iterative::tail_data* tail_p = particle_tails_list_shared->first();
                    tail_p->set(*particle_p);
                    particle_tails_list_shared->remove(tail_p);
                    particle_p->tails_list.append(tail_p);
                    if (max_tails < particle_p->tails_list.get_elem_count())
                    {
                        vfx_particle_iterative::tail_data* oldest_p = particle_p->tails_list.first();
                        particle_p->tails_list.remove(oldest_p);
                        particle_tails_list_shared->append(oldest_p);
                    }
                    else
                    {
                        tail_spawn_count++;
                    }
                }

                const float lifetime = particle_p->lifetime;
                const float time_created = particle_p->time_created;
                const float death_time = time_created + lifetime;
                if (current_elapsed_time <= death_time)
                {
                    float ratio = (start_time - time_created) / lifetime;
                    if (current_elapsed_time + step_time <= death_time)
                    {
                        // Magic number: a particle born this frame gets a tiny positive ratio.
                        if (ratio <= 0.0f)
                        {
                            ratio = 1e-05f;
                        }
                    }
                    else
                    {
                        ratio = 1.0f;
                    }
                    particle_p->iter_life_ratio = ratio;
                    particle_p->position.x = particle_p->position.x + particle_p->velocity.x * iteration_time;
                    particle_p->position.y = particle_p->position.y + particle_p->velocity.y * iteration_time;
                    particle_p->position.z = particle_p->velocity.z * iteration_time + particle_p->position.z;
                    emitter_p = m_emitter_p;
                    const double inverse = 1.0 - (double)ratio;
                    particle_p->weight = interpolate_two_values(emitter_p->m_weight_start, emitter_p->m_weight_end, ratio) * emitter_p->m_weight_scale;
                    // Drag: the density slows the particle over the frame time (delta_time, not the iteration time).
                    const float density = particle_p->density;
                    particle_p->velocity.x = particle_p->velocity.x - particle_p->velocity.x * density * delta_time;
                    particle_p->velocity.y = particle_p->velocity.y - density * particle_p->velocity.y * delta_time;
                    particle_p->velocity.z = particle_p->velocity.z - density * particle_p->velocity.z * delta_time;
                    if (m_emitter_p->use_accel_values)
                    {
                        const float inverse_f = (float)inverse;
                        particle_p->velocity.x = particle_p->velocity.x + particle_p->acceleration_start.x * iteration_time * inverse_f + iteration_time * particle_p->acceleration_end.x * ratio;
                        particle_p->velocity.y = particle_p->acceleration_start.y * iteration_time * inverse_f + particle_p->acceleration_end.y * iteration_time * ratio + particle_p->velocity.y;
                        particle_p->velocity.z = particle_p->acceleration_start.z * iteration_time * inverse_f + particle_p->acceleration_end.z * iteration_time * ratio + particle_p->velocity.z;
                    }
                    particle_p->rotation_angle = interpolate_two_values(particle_p->rotation_angular_speed_start, particle_p->rotation_angular_speed_end, ratio) * iteration_time + particle_p->rotation_angle;

                    emitter_p = m_emitter_p;
                    if (!emitter_p->use_size_mid_value)
                    {
                        particle_p->current_size = interpolate_two_values(particle_p->size_start, particle_p->size_end, ratio);
                    }
                    else
                    {
                        const float mid_A_offset = emitter_p->get_size_mid_A_offset(current_anim_time);
                        const float mid_B_offset = emitter_p->get_size_mid_B_offset(current_anim_time);
                        particle_p->current_size = interpolate_four_values(particle_p->size_start, particle_p->size_mid_A, particle_p->size_mid_B, particle_p->size_end, mid_A_offset, mid_B_offset, ratio);
                    }
                    emitter_p = m_emitter_p;
                    if (!emitter_p->use_length)
                    {
                        particle_p->current_length = particle_p->current_size;
                    }
                    else if (!emitter_p->use_length_mid_value)
                    {
                        particle_p->current_length = interpolate_two_values(particle_p->length_start, particle_p->length_end, ratio);
                    }
                    else
                    {
                        const float mid_A_offset = emitter_p->get_length_mid_A_offset(current_anim_time);
                        const float mid_B_offset = emitter_p->get_length_mid_B_offset(current_anim_time);
                        particle_p->current_length = interpolate_four_values(particle_p->length_start, particle_p->length_mid_A, particle_p->length_mid_B, particle_p->length_end, mid_A_offset, mid_B_offset, ratio);
                    }

                    emitter_p = m_emitter_p;
                    if (!emitter_p->use_color_mid_value)
                    {
                        particle_p->current_color = interpolate_two_values(particle_p->color_start, particle_p->color_end, ratio);
                        if (m_emitter_p->use_color_saturation_value)
                        {
                            particle_p->current_color_saturation = interpolate_two_values(particle_p->color_saturation_start, particle_p->color_saturation_end, ratio);
                        }
                        if (m_emitter_p->use_additive_key_value)
                        {
                            particle_p->current_additive_key = interpolate_two_values(particle_p->additive_key_start, particle_p->additive_key_end, ratio);
                        }
                    }
                    else
                    {
                        // Color through the color mid offsets, alpha through the alpha mid offsets.
                        const float color_mid_A_offset = emitter_p->color_mid_A_offset;
                        const float color_mid_B_offset = emitter_p->color_mid_B_offset;
                        particle_p->current_color = interpolate_four_values(particle_p->color_start, particle_p->color_mid_A, particle_p->color_mid_B, particle_p->color_end, color_mid_A_offset, color_mid_B_offset, ratio);
                        particle_p->current_color.alpha = interpolate_four_values(particle_p->color_start.alpha, particle_p->color_mid_A.alpha, particle_p->color_mid_B.alpha, particle_p->color_end.alpha, emitter_p->alpha_mid_A_offset, emitter_p->alpha_mid_B_offset, ratio);
                        if (m_emitter_p->use_color_saturation_value)
                        {
                            particle_p->current_color_saturation = interpolate_four_values(particle_p->color_saturation_start, particle_p->color_saturation_mid_A, particle_p->color_saturation_mid_B, particle_p->color_saturation_end, color_mid_A_offset, color_mid_B_offset, ratio);
                        }
                        if (m_emitter_p->use_additive_key_value)
                        {
                            particle_p->current_additive_key = interpolate_four_values(particle_p->additive_key_start, particle_p->additive_key_mid_A, particle_p->additive_key_mid_B, particle_p->additive_key_end, color_mid_A_offset, color_mid_B_offset, ratio);
                        }
                    }
                }
                else
                {
                    // Dead: the tails and the particle go back to the shared free lists.
                    num_alive--;
                    num_dead++;
                    if (m_particle_create_cb != nullptr)
                    {
                        m_particle_create_cb(particle_p, this, m_particle_create_cb_data, false);
                    }
                    if (has_tails)
                    {
                        tail_spawn_count -= particle_p->tails_list.get_elem_count();
                        particle_tails_list_shared->concat(particle_p->tails_list);
                    }
                    m_particle_iterative_list.remove(particle_p);
                    m_particle_iterative_list_shared_p->append(particle_p);
                }
                particle_p = next_p;
            }
        }
    }
    m_update_tail_count++;
    if (m_post_update_cb != nullptr)
    {
        m_post_update_cb(this, m_update_cb_data_p);
    }
    return num_alive;
}

// 0x00CB0280 (:666): limited by the dead (free) slots of this instance.
void vfx_particle_system_emitter_instance::create_particles(int num_particles, float elapsed_time)
{
    if (emission_enabled && num_particles != 0)
    {
        if (num_dead < tail_spawn_count + num_particles)
        {
            num_particles = num_dead;
        }
        const int spawned = spawn_internal(num_particles, elapsed_time);
        num_dead -= spawned;
        num_alive += spawned;
    }
}

// 0x00CA4860 (:697): spawns up to num particles spread evenly over the frame (birth time and, outside local space, the
// spawn position move along the emitter's path); returns how many it could take from the free lists.
int vfx_particle_system_emitter_instance::spawn_internal(int num, float elapsed_time)
{
    // Function static (0x03A23C88-ish in .bss): set while the free list had a particle, cleared when it ran out.
    static int do_once;
    if (num < 1)
    {
        return 0;
    }
    update_internal_transforms();
    float emission_rate = get_emission_rate();
    const float start_time = elapsed_time - last_spawn_time;
    previous_spawn_position = current_spawn_position;
    const float num_f = (float)num;
    vector spawn_step;
    spawn_step.x = (current_world_matrix.m_translation.x - previous_spawn_position.x) / num_f;
    spawn_step.y = (current_world_matrix.m_translation.y - previous_spawn_position.y) / num_f;
    spawn_step.z = (current_world_matrix.m_translation.z - previous_spawn_position.z) / (float)num;
    current_spawn_position.x = spawn_step.x + previous_spawn_position.x;
    current_spawn_position.y = spawn_step.y + previous_spawn_position.y;
    current_spawn_position.z = spawn_step.z + previous_spawn_position.z;
    vfx_particle_system_emitter* emitter_p = m_emitter_p;
    if (emitter_p->local_space_emission)
    {
        current_spawn_position = current_world_matrix.m_translation;
    }
    const bool is_iterative = emitter_p->is_iterative;
    for (int index = 0; index < num; index++)
    {
        vfx_particle_base* particle_p;
        vfx_particle_iterative* iterative_p = nullptr;
        if (!is_iterative)
        {
            vfx_particle_parametric* parametric_p = m_particle_parametric_list_shared_p->first();
            if (parametric_p == nullptr)
            {
                if (do_once != 0)
                {
                    do_once = 0;
                }
                return index;
            }
            do_once = 1;
            m_particle_parametric_list_shared_p->remove(parametric_p);
            m_particle_parametric_list.append(parametric_p);
            particle_p = parametric_p;
        }
        else
        {
            iterative_p = m_particle_iterative_list_shared_p->first();
            if (iterative_p == nullptr)
            {
                if (do_once != 0)
                {
                    do_once = 0;
                }
                return index;
            }
            do_once = 1;
            m_particle_iterative_list_shared_p->remove(iterative_p);
            m_particle_iterative_list.append(iterative_p);
            iterative_p->tails_list.init();
            particle_p = iterative_p;
        }

        // Four random draws per particle (global generator): the float, then x, y, z.
        ms_random_float = fl_rand_range(0.0f, 1.0f);
        const float random_x = fl_rand_range(0.0f, 1.0f);
        const float random_y = fl_rand_range(0.0f, 1.0f);
        ms_random_vector.z = fl_rand_range(0.0f, 1.0f);
        ms_random_vector.y = random_y;
        ms_random_vector.x = random_x;
        emitter_p = m_emitter_p;
        if (!emitter_p->use_lifetime_variance)
        {
            particle_p->lifetime = emitter_p->lifetime;
        }
        else
        {
            particle_p->lifetime = vfx_variance_plus_base(emitter_p->lifetime_variance, ms_random_float, emitter_p->lifetime);
        }
        // Notes (decompiled behaviour, kept as is): the clamped rate is not used afterwards.
        if (emission_rate - 1e-05f < 0.0f)
        {
            emission_rate = 1e-05f;
        }
        particle_p->time_created = ((elapsed_time - start_time) / num_f) * (float)(index + 1) + start_time;
        particle_p->random_vector = ms_random_vector;
        particle_p->random_float = ms_random_float;

        if (is_iterative)
        {
            emitter_p = m_emitter_p;
            const float anim_time = current_anim_time;
            // Sizes: the key value, or a spread of the variance key around it, never negative.
            if (!emitter_p->use_size_variance)
            {
                iterative_p->size_start = emitter_p->get_size_start(anim_time);
                iterative_p->size_end = emitter_p->get_size_end(current_anim_time);
                if (emitter_p->use_size_mid_value)
                {
                    iterative_p->size_mid_A = emitter_p->get_size_mid_A(current_anim_time);
                    iterative_p->size_mid_B = emitter_p->get_size_mid_B(current_anim_time);
                }
            }
            else
            {
                const float start_variance = emitter_p->get_size_start_variance(anim_time);
                const float end_variance = emitter_p->get_size_end_variance(anim_time);
                float value = vfx_variance_plus_base(start_variance, ms_random_float, emitter_p->get_size_start(anim_time));
                iterative_p->size_start = 0.0f <= 0.0f - value ? 0.0f : value;
                value = vfx_variance_plus_base(end_variance, ms_random_float, emitter_p->get_size_end(current_anim_time));
                iterative_p->size_end = 0.0f <= 0.0f - value ? 0.0f : value;
                if (emitter_p->use_size_mid_value)
                {
                    const float mid_anim_time = current_anim_time;
                    const float mid_A_variance = emitter_p->get_size_mid_A_variance(mid_anim_time);
                    const float mid_B_variance = emitter_p->get_size_mid_B_variance(mid_anim_time);
                    value = vfx_variance_plus_base(mid_A_variance, ms_random_float, emitter_p->get_size_mid_A(mid_anim_time));
                    iterative_p->size_mid_A = 0.0f <= 0.0f - value ? 0.0f : value;
                    value = vfx_variance_plus_base(mid_B_variance, ms_random_float, emitter_p->get_size_mid_B(current_anim_time));
                    iterative_p->size_mid_B = 0.0f <= 0.0f - value ? 0.0f : value;
                }
            }
            // Lengths (same scheme, unscaled).
            emitter_p = m_emitter_p;
            if (emitter_p->use_length)
            {
                const float length_anim_time = current_anim_time;
                if (!emitter_p->use_length_variance)
                {
                    iterative_p->length_start = emitter_p->get_length_start(length_anim_time);
                    iterative_p->length_end = m_emitter_p->get_length_end(current_anim_time);
                    if (m_emitter_p->use_length_mid_value)
                    {
                        iterative_p->length_mid_A = m_emitter_p->get_length_mid_A(current_anim_time);
                        iterative_p->length_mid_B = m_emitter_p->get_length_mid_B(current_anim_time);
                    }
                }
                else
                {
                    const float start_variance = emitter_p->get_length_start_variance(length_anim_time);
                    const float end_variance = emitter_p->get_length_end_variance(length_anim_time);
                    float value = vfx_variance_plus_base(start_variance, ms_random_float, emitter_p->get_length_start(length_anim_time));
                    iterative_p->length_start = 0.0f <= 0.0f - value ? 0.0f : value;
                    value = vfx_variance_plus_base(end_variance, ms_random_float, m_emitter_p->get_length_end(current_anim_time));
                    iterative_p->length_end = 0.0f <= 0.0f - value ? 0.0f : value;
                    emitter_p = m_emitter_p;
                    if (emitter_p->use_length_mid_value)
                    {
                        // Here the base comes first: (mid + low) + spread (decompiled order).
                        const float mid_anim_time = current_anim_time;
                        const float mid_A_variance = emitter_p->get_length_mid_A_variance(mid_anim_time);
                        const float mid_B_variance = emitter_p->get_length_mid_B_variance(mid_anim_time);
                        float low = -mid_A_variance;
                        float high = mid_A_variance;
                        if (mid_A_variance < low)
                        {
                            low = mid_A_variance;
                            high = -mid_A_variance;
                        }
                        value = emitter_p->get_length_mid_A(mid_anim_time) + low + ms_random_float * (high - low);
                        iterative_p->length_mid_A = 0.0f <= 0.0f - value ? 0.0f : value;
                        low = -mid_B_variance;
                        high = mid_B_variance;
                        if (mid_B_variance < low)
                        {
                            low = mid_B_variance;
                            high = -mid_B_variance;
                        }
                        value = m_emitter_p->get_length_mid_B(current_anim_time) + low + ms_random_float * (high - low);
                        iterative_p->length_mid_B = 0.0f - value < 0.0f ? value : 0.0f;
                    }
                }
            }
            iterative_p->current_color_saturation = 1.0f;
            iterative_p->current_additive_key = 0.0f;

            // Colors (opacity scaled), saturation and additive keys.
            emitter_p = m_emitter_p;
            if (!emitter_p->use_color_variance)
            {
                iterative_p->color_start = emitter_p->color_start;
                iterative_p->color_start.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_start.alpha;
                iterative_p->color_end = m_emitter_p->color_end;
                iterative_p->color_end.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_end.alpha;
                if (m_emitter_p->use_color_saturation_value)
                {
                    iterative_p->color_saturation_start = m_emitter_p->color_saturation_start;
                    iterative_p->color_saturation_end = m_emitter_p->color_saturation_end;
                }
                if (m_emitter_p->use_additive_key_value)
                {
                    iterative_p->additive_key_start = m_emitter_p->additive_key_start;
                    iterative_p->additive_key_end = m_emitter_p->additive_key_end;
                }
                emitter_p = m_emitter_p;
                if (emitter_p->use_color_mid_value)
                {
                    iterative_p->color_mid_A = emitter_p->color_mid_A;
                    iterative_p->color_mid_A.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_mid_A.alpha;
                    iterative_p->color_mid_B = m_emitter_p->color_mid_B;
                    iterative_p->color_mid_B.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_mid_B.alpha;
                    if (m_emitter_p->use_color_saturation_value)
                    {
                        iterative_p->color_saturation_mid_A = m_emitter_p->color_saturation_mid_A;
                        iterative_p->color_saturation_mid_B = m_emitter_p->color_saturation_mid_B;
                    }
                    if (m_emitter_p->use_additive_key_value)
                    {
                        iterative_p->additive_key_mid_A = m_emitter_p->additive_key_mid_A;
                        iterative_p->additive_key_mid_B = m_emitter_p->additive_key_mid_B;
                    }
                }
            }
            else
            {
                // compute_variant_color inlined; the red channel of the start and mid A colors adds in the other order
                // (decompiled), so the per-channel form is spelled out.
                const bool by_value = emitter_p->is_color_variance_by_value;
                if (!by_value)
                {
                    iterative_p->color_start.red = vfx_variance_plus_base(emitter_p->color_start_variance.red, ms_random_vector.x, emitter_p->color_start.red);
                    iterative_p->color_start.green = vfx_base_plus_variance(emitter_p->color_start.green, emitter_p->color_start_variance.green, ms_random_vector.y);
                    iterative_p->color_start.blue = vfx_base_plus_variance(emitter_p->color_start.blue, emitter_p->color_start_variance.blue, ms_random_vector.z);
                    iterative_p->color_start.alpha = vfx_base_plus_variance(emitter_p->color_start.alpha, emitter_p->color_start_variance.alpha, ms_random_float);
                }
                else
                {
                    iterative_p->color_start = compute_variant_color(emitter_p->color_start, emitter_p->color_start_variance, true);
                }
                iterative_p->color_start.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_start.alpha;
                emitter_p = m_emitter_p;
                iterative_p->color_end = compute_variant_color(emitter_p->color_end, emitter_p->color_end_variance, by_value);
                iterative_p->color_end.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_end.alpha;
                emitter_p = m_emitter_p;
                if (emitter_p->use_color_saturation_value)
                {
                    iterative_p->color_saturation_start = vfx_variance_plus_base(emitter_p->color_saturation_start_variance, ms_random_float, emitter_p->color_saturation_start);
                    iterative_p->color_saturation_end = vfx_variance_plus_base(emitter_p->color_saturation_end_variance, ms_random_float, m_emitter_p->color_saturation_end);
                }
                emitter_p = m_emitter_p;
                if (emitter_p->use_additive_key_value)
                {
                    iterative_p->additive_key_start = vfx_variance_plus_base(emitter_p->additive_key_start_variance, ms_random_float, emitter_p->additive_key_start);
                    iterative_p->additive_key_end = vfx_variance_plus_base(emitter_p->additive_key_end_variance, ms_random_float, m_emitter_p->additive_key_end);
                }
                emitter_p = m_emitter_p;
                if (emitter_p->use_color_mid_value)
                {
                    if (!by_value)
                    {
                        iterative_p->color_mid_A.red = vfx_variance_plus_base(emitter_p->color_mid_A_variance.red, ms_random_vector.x, emitter_p->color_mid_A.red);
                        iterative_p->color_mid_A.green = vfx_base_plus_variance(emitter_p->color_mid_A.green, emitter_p->color_mid_A_variance.green, ms_random_vector.y);
                        iterative_p->color_mid_A.blue = vfx_base_plus_variance(emitter_p->color_mid_A.blue, emitter_p->color_mid_A_variance.blue, ms_random_vector.z);
                        iterative_p->color_mid_A.alpha = vfx_base_plus_variance(emitter_p->color_mid_A.alpha, emitter_p->color_mid_A_variance.alpha, ms_random_float);
                    }
                    else
                    {
                        iterative_p->color_mid_A = compute_variant_color(emitter_p->color_mid_A, emitter_p->color_mid_A_variance, true);
                    }
                    iterative_p->color_mid_A.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_mid_A.alpha;
                    emitter_p = m_emitter_p;
                    iterative_p->color_mid_B = compute_variant_color(emitter_p->color_mid_B, emitter_p->color_mid_B_variance, by_value);
                    iterative_p->color_mid_B.alpha = m_emitter_p->m_opacity_scale * iterative_p->color_mid_B.alpha;
                    emitter_p = m_emitter_p;
                    if (emitter_p->use_color_saturation_value)
                    {
                        iterative_p->color_saturation_mid_A = vfx_variance_plus_base(emitter_p->color_saturation_mid_A_variance, ms_random_float, emitter_p->color_saturation_mid_A);
                        iterative_p->color_saturation_mid_B = vfx_variance_plus_base(emitter_p->color_saturation_mid_B_variance, ms_random_float, m_emitter_p->color_saturation_mid_B);
                    }
                    emitter_p = m_emitter_p;
                    if (emitter_p->use_additive_key_value)
                    {
                        iterative_p->additive_key_mid_A = vfx_variance_plus_base(emitter_p->additive_key_mid_A_variance, ms_random_float, emitter_p->additive_key_mid_A);
                        iterative_p->additive_key_mid_B = vfx_variance_plus_base(emitter_p->additive_key_mid_B_variance, ms_random_float, m_emitter_p->additive_key_mid_B);
                    }
                }
            }
            iterative_p->weight = m_emitter_p->m_weight_start;
            emitter_p = m_emitter_p;
            if (!emitter_p->use_density_variance)
            {
                iterative_p->density = emitter_p->density;
            }
            else
            {
                iterative_p->density = vfx_variance_plus_base(emitter_p->density_variance, ms_random_float, emitter_p->density);
            }
        }

        // The emitter shape places the particle and sets the birth direction.
        m_emitter_p->spawn_particle(particle_p, this);
        m_emitter_p->initiate_velocity(particle_p, this);
        if (!m_emitter_p->local_space_emission)
        {
            current_spawn_position.x = spawn_step.x + current_spawn_position.x;
            current_spawn_position.y = current_spawn_position.y + spawn_step.y;
            current_spawn_position.z = current_spawn_position.z + spawn_step.z;
        }
        if (is_iterative)
        {
            iterative_p->emission_position = particle_p->position;
        }
        particle_p->emit_number = m_emit_number;
        m_emit_number++;

        // Speed: the key value plus the variance spread; inherited velocity follows the emitter path (or the parent
        // particle of an emitted node), otherwise the birth direction is scaled.
        emitter_p = m_emitter_p;
        const float speed = vfx_variance_plus_base(emitter_p->speed_variance, ms_random_float, emitter_p->get_speed(current_anim_time));
        if (emitter_p->m_inherit_velocity == true)
        {
            if (m_emit_node_parent_p == nullptr)
            {
                particle_p->velocity = vector(spawn_step.x * speed, spawn_step.y * speed, spawn_step.z * speed);
            }
            else
            {
                const vector& parent_velocity = m_emit_node_parent_p->velocity;
                particle_p->velocity = vector(parent_velocity.x * speed, parent_velocity.y * speed, parent_velocity.z * speed);
            }
        }
        else
        {
            particle_p->velocity.x = particle_p->velocity.x * speed;
            particle_p->velocity.y = speed * particle_p->velocity.y;
            particle_p->velocity.z = speed * particle_p->velocity.z;
        }

        // Rotation: the start angle, the angular speeds (iterative), and the spin direction from which side of the
        // view the velocity x up axis faces.
        if (!m_emitter_p->is_rotation_enabled)
        {
            particle_p->rotation_angle = 0.0f;
            if (!is_iterative)
            {
                continue;
            }
            iterative_p->rotation_angular_speed_start = 0.0f;
            iterative_p->rotation_angular_speed_end = 0.0f;
        }
        else
        {
            particle_p->rotation_angle = m_emitter_p->is_random_start_rotation ? ms_random_float * VFX_FOUR_PI - VFX_TWO_PI : 0.0f;
            if (is_iterative)
            {
                const float anim_time = current_anim_time;
                const float speed_start = m_emitter_p->get_angular_speed_start(anim_time);
                emitter_p = m_emitter_p;
                const float speed_start_variance = emitter_p->angular_speed_start_variance;
                const float speed_end = emitter_p->get_angular_speed_end(anim_time);
                const float speed_end_variance = m_emitter_p->angular_speed_end_variance;
                iterative_p->rotation_angular_speed_start = vfx_variance_plus_base(speed_start_variance, ms_random_float, speed_start);
                iterative_p->rotation_angular_speed_end = vfx_variance_plus_base(speed_end_variance, ms_random_float, speed_end);
            }
            const vector direction = particle_p->velocity.copy_normalize();
            const vector& up = current_world_matrix.m_rotation.uvec;
            vector axis;
            axis.y = up.x * direction.z - direction.x * up.z;
            axis.z = up.y * direction.x - direction.y * up.x;
            axis.x = direction.y * up.z - up.y * direction.z;
            const vector4 axis4(axis);
            const vector4 view_forward(vfx_system::get_view_orientation().fvec);
            if (0.0f <= axis4.y * view_forward.y + view_forward.x * axis4.x + axis4.z * view_forward.z + axis4.w * view_forward.w)
            {
                if (!is_iterative)
                {
                    particle_p->rotation_angle = fabsf(particle_p->rotation_angle);
                    continue;
                }
            }
            else if (!is_iterative)
            {
                if (particle_p->rotation_angle == 0.0f)
                {
                    particle_p->rotation_angle = VFX_TWO_PI;
                }
                particle_p->rotation_angle = fabsf(particle_p->rotation_angle) * -1.0f;
                continue;
            }
            else
            {
                iterative_p->rotation_angular_speed_start = iterative_p->rotation_angular_speed_start * -1.0f;
                iterative_p->rotation_angular_speed_end = iterative_p->rotation_angular_speed_end * -1.0f;
            }
        }

        // Iterative only from here: the acceleration (with the per-axis spread) and the create callback.
        emitter_p = m_emitter_p;
        if (emitter_p->use_accel_values)
        {
            if (!emitter_p->use_accel_variance)
            {
                iterative_p->acceleration_start = emitter_p->acceleration_start;
                iterative_p->acceleration_end = emitter_p->acceleration_end;
            }
            else
            {
                iterative_p->acceleration_start.x = vfx_base_plus_variance(emitter_p->acceleration_start.x, emitter_p->acceleration_start_variance.x, ms_random_vector.x);
                iterative_p->acceleration_start.y = vfx_base_plus_variance(emitter_p->acceleration_start.y, emitter_p->acceleration_start_variance.y, ms_random_vector.y);
                iterative_p->acceleration_start.z = vfx_base_plus_variance(emitter_p->acceleration_start.z, emitter_p->acceleration_start_variance.z, ms_random_vector.z);
                iterative_p->acceleration_end.x = vfx_base_plus_variance(emitter_p->acceleration_end.x, emitter_p->acceleration_end_variance.x, ms_random_vector.x);
                iterative_p->acceleration_end.y = vfx_base_plus_variance(emitter_p->acceleration_end.y, emitter_p->acceleration_end_variance.y, ms_random_vector.y);
                iterative_p->acceleration_end.z = vfx_base_plus_variance(emitter_p->acceleration_end.z, emitter_p->acceleration_end_variance.z, ms_random_vector.z);
            }
        }
        if (m_particle_create_cb != nullptr)
        {
            m_particle_create_cb(iterative_p, this, m_particle_create_cb_data, true);
        }
    }
    current_spawn_position = current_world_matrix.m_translation;
    return num;
}

// 0x00CA7420 (:1068)
void vfx_particle_system_emitter_instance::apply_filter_list(const base_array<vfx_particle_system_filter_instance*>& filter_list, float delta_time)
{
    for (int i = 0; i < filter_list.size(); i++)
    {
        filter_list[i]->get_filter()->process_emitter(this, filter_list[i], delta_time);
    }
}

// 0x00CA7460 (:1076)
void vfx_particle_system_emitter_instance::apply_interactors(float delta_time)
{
    for (int i = 0; i < m_interactors.size(); i++)
    {
        m_interactors[i]->get_filter()->process_emitter(this, m_interactors[i], delta_time);
    }
}

// 0x00C9FF30 (:1084): deflectors are not implemented.
void vfx_particle_system_emitter_instance::apply_deflectors(float delta_time)
{
}

// 0x00CB02D0 (:1093): only interactors are kept.
void vfx_particle_system_emitter_instance::add_filter(vfx_particle_system_filter_instance* filter_instance_p)
{
    if (filter_instance_p->get_filter()->get_base_type() == vfx_particle_system_filter::BASE_TYPE_INTERACTOR)
    {
        m_interactors.add(filter_instance_p);
    }
}

// 0x00CB0330 (:1102)
void vfx_particle_system_emitter_instance::remove_filter(vfx_particle_system_filter_instance* filter_instance_p)
{
    if (filter_instance_p->get_filter()->get_base_type() == vfx_particle_system_filter::BASE_TYPE_INTERACTOR)
    {
        m_interactors.remove_matching(filter_instance_p);
    }
}

// 0x00CB0350 (:1112): the particles (and the iterative particles' tails) into the renderable instance's arrays. A
// ribbon gets one more slot allocated than it uses (the renderer's closing point).
void vfx_particle_system_emitter_instance::copy_particles(rl_renderable_particles_instance* renderable_instance_p) const
{
    const bool is_iterative = m_emitter_p->is_iterative;
    renderable_instance_p->m_frame_number = (int)m_update_tail_count;
    const int extra = m_emitter_p->m_particle_type == rl_particle_base::RENDER_TYPE_RIBBON ? 1 : 0;
    if (!is_iterative)
    {
        if (!renderable_instance_p->alloc_parametric_particles(m_particle_parametric_list.get_elem_count() + extra))
        {
            renderable_instance_p->m_particle_parametric_list.resize(0);
            return;
        }
        renderable_instance_p->m_particle_parametric_list.resize(renderable_instance_p->m_particle_parametric_list.size() - extra);
    }
    else
    {
        if (!renderable_instance_p->alloc_iterative_particles(m_particle_iterative_list.get_elem_count() + extra))
        {
            renderable_instance_p->m_particle_iterative_list.resize(0);
            return;
        }
        renderable_instance_p->m_particle_iterative_list.resize(renderable_instance_p->m_particle_iterative_list.size() - extra);
    }
    int num_tails = 0;
    rl_particle_base::tail_data* tails = nullptr;
    if (!is_iterative)
    {
        int index = 0;
        const vfx_particle_parametric* particle_p = m_particle_parametric_list.first();
        while (particle_p != nullptr)
        {
            particle_p->copy_to_rl_particle(renderable_instance_p->m_particle_parametric_list[index]);
            index++;
            particle_p = particle_p->next;
            if (particle_p == m_particle_parametric_list.first())
            {
                break;
            }
        }
    }
    else
    {
        int index = 0;
        const vfx_particle_iterative* particle_p = m_particle_iterative_list.first();
        while (particle_p != nullptr)
        {
            rl_particle_iterative& rl_particle = renderable_instance_p->m_particle_iterative_list[index];
            particle_p->copy_to_rl_particle(rl_particle);
            const int count = particle_p->tails_list.get_elem_count();
            if (count > 0)
            {
                if (!rl_particle.alloc_particle_tails(count))
                {
                    renderable_instance_p->m_particle_iterative_list.resize(0);
                    return;
                }
                num_tails += rl_particle.get_particle_tail_list_size();
                if (tails == nullptr)
                {
                    tails = rl_particle.get_particle_tail_list();
                }
                rl_particle_base::tail_data* rl_tail_p = rl_particle.get_particle_tail_list();
                const vfx_particle_iterative::tail_data* tail_p = particle_p->tails_list.first();
                while (tail_p != nullptr)
                {
                    tail_p->copy_to_rl_tail(*rl_tail_p);
                    rl_tail_p++;
                    tail_p = tail_p->next;
                    if (tail_p == particle_p->tails_list.first())
                    {
                        break;
                    }
                }
            }
            index++;
            particle_p = particle_p->next;
            if (particle_p == m_particle_iterative_list.first())
            {
                break;
            }
        }
    }
    renderable_instance_p->m_num_tails = num_tails;
    renderable_instance_p->m_tails = tails;
}
