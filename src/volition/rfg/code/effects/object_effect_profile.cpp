// Original: volition/rfg/code/effects/object_effect_profile.cpp
//
// object_effect_profile (decompiled; header comment in object_effect_profile.h). Compiled in the effect library
// (rfg_effects, VFX.md 2.6).

#include "volition/rfg/code/effects/object_effect_profile.h"

#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/objects/object.inl"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"

// Emitters iterating faster than this count their particles as extra iterated.
static const float EXTRA_ITERATIONS_PER_SEC = 30.0f;
// A particle with alpha within this fraction of zero (relative to its alpha, clamped to [0, 1]) is invisible.
static const float ALPHA0_TOLERANCE = 0.05f;

// ---- file-static globals
static bool Vfx_profiling_enabled; // 0x02182E0D

// 0x0074AC80 (:11)
object_effect_profile::object_effect_profile()
{
    m_object_effect_p = nullptr;
}

// 0x00744160 (:16)
object_effect_profile::~object_effect_profile()
{
}

// 0x0074ACA0 (:20)
void object_effect_profile::init(object_effect* object_effect_p)
{
    // Original: a fresh data block is built and dropped (the statistics are not reset).
    data fresh_data;
    (void)fresh_data;
    m_object_effect_p = object_effect_p;
}

// 0x0074ACD0 (:26)
void object_effect_profile::update()
{
    if (!Vfx_profiling_enabled)
    {
        return;
    }
    vfx_effect* effect_p = m_object_effect_p->get_vfx_effect();
    if (effect_p == nullptr)
    {
        return;
    }
    int simulated = 0;
    int extra_iterated = 0;
    int alpha0 = 0;
    int filtered = 0;
    vfx_particle_system_emitter_instance* emitter_head_p = effect_p->get_emitters().first();
    for (vfx_particle_system_emitter_instance* emitter_p = emitter_head_p; emitter_p != nullptr;)
    {
        const bool extra = emitter_p->m_emitter_p->is_iterative && EXTRA_ITERATIONS_PER_SEC < emitter_p->m_emitter_p->iterations_per_sec;
        vfx_particle_iterative* particle_head_p = emitter_p->m_particle_iterative_list.first();
        for (vfx_particle_iterative* particle_p = particle_head_p; particle_p != nullptr;)
        {
            const float alpha = particle_p->current_color.alpha;
            float tolerance = fabsf(alpha);
            if (fabsf(alpha) - 0.0f < 0.0f)
            {
                tolerance = 0.0f;
            }
            if (0.0f <= 1.0f - tolerance)
            {
                tolerance = 1.0f;
            }
            if (0.0f - tolerance * ALPHA0_TOLERANCE <= alpha && alpha <= tolerance * ALPHA0_TOLERANCE + 0.0f)
            {
                ++alpha0;
            }
            filtered += emitter_p->m_interactors.size();
            ++simulated;
            if (extra)
            {
                ++extra_iterated;
            }
            particle_p = particle_p->next;
            if (particle_p == particle_head_p)
            {
                break;
            }
        }
        emitter_p = emitter_p->next;
        if (emitter_p == emitter_head_p)
        {
            break;
        }
    }
    m_profile_data.particles_simulated_count.add_new(simulated);
    m_profile_data.particles_filtered.add_new(filtered);
    m_profile_data.extra_iterated_particle_count.add_new(extra_iterated);
    m_profile_data.simulating_alpha0_particles.add_new(alpha0);
    int num_emitters = effect_p->get_emitters().get_elem_count();
    if (num_emitters < 1)
    {
        num_emitters = 1;
    }
    m_profile_data.particles_to_emitter_ratio = m_profile_data.particles_simulated_count.avg / num_emitters;
}
