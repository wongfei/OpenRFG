// Original: volition/rfg/code/code_effects/tracers.cpp
//
// Bullet tracers (VFX.md FX4 item 13, 1.4). A tracer is a looping particle effect whose template emitters are primed to
// emit their particles at once (emission rate 2,000,000, a fixed particle count), updated by a tiny step and switched
// off again; the tracer then keeps pointers to three parametric particles (start, end, trail) with zero velocities and
// moves them itself each frame: linearly for the straight tracers (rail and sniper also spread the particles of one
// emitter along the path), along a Catmull-Rom spline for the curved enforcer tracer. The frame after it arrives the
// effect stops emitting and the tracer goes back to the free list. The vfxlib classes befriend these functions.

#include "volition/rfg/code/code_effects/tracers.h"

#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_animation.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_manager.h"
#include "volition/vlib/code/math/matrix.h"

// ---- globals
tracer Tracer_buf[64]; // 0x01E14310
tracer* Tracer_used_list; // 0x01DEB6E8
tracer* Tracer_free_list; // 0x01DEE424
unsigned int Tracer_effect_handle; // 0x01DEE3F0
unsigned int Curved_tracer_effect_handle; // 0x01DEB6EC

// ---- file-static globals
// Tracer_buf (0x01E14310) ends at 0x01E1A010, which Ghidra names Map_library_entry_pool (material_effects.cpp).
// Milliseconds of flight per metre.
static float Tracer_velocity_scale = 3.0f; // 0x01649000
// The particles of the curved tracer's spline emitter.
static int num_curved_particles = 5; // 0x01649004

// The tracer emitters' emission rate while primed: every particle is emitted in the 0.0001 s prime step.
static const float TRACER_PRIME_EMISSION_RATE = 2000000.0f;
// The prime step of the tracer effect's first update, in seconds.
static const float TRACER_PRIME_STEP = 0.0001f;

// ---- the free and used lists (circular through tracer::next/prev; inlined list macros in rfg.exe)

// Unlinks t from the list; the next tracer becomes the head when t was it.
static void tracer_list_remove(tracer*& list, tracer* t)
{
    if (list == t && (list = t->next) == t)
    {
        t->next = nullptr;
        t->prev = nullptr;
        list = nullptr;
        return;
    }
    if (t->prev != nullptr)
    {
        t->prev->next = t->next;
    }
    if (t->next != nullptr)
    {
        t->next->prev = t->prev;
    }
    t->next = nullptr;
    t->prev = nullptr;
}

// Links t in front of the list's head and makes it the head.
static void tracer_list_insert_front(tracer*& list, tracer* t)
{
    if (list == nullptr)
    {
        t->next = t;
        t->prev = t;
    }
    else
    {
        list->prev->next = t;
        t->prev = list->prev;
        list->prev = t;
        t->next = list;
    }
    list = t;
}

// 0x006E4BD0: the members' constructors only (the age timer's, inlined there, unsets it)
tracer::tracer()
{
}

// 0x006E4BA0 (:61)
bool tracer::is_curved() const
{
    return tracer_type == WEAPON_TRACER_ENFORCER;
}

// 0x006E4BB0 (:62)
bool tracer::is_rail() const
{
    return tracer_type == WEAPON_TRACER_RAIL;
}

// 0x006E4BC0 (:63)
bool tracer::is_sniper() const
{
    return tracer_type == WEAPON_TRACER_SNIPER;
}

// 0x006E4BF0 (:84)
void tracers_system_init()
{
    Tracer_effect_handle = effects_get_handle("wep_bullet_tracer_a");
    Curved_tracer_effect_handle = effects_get_handle("wep_edf_enforcer_trail_a");
}

// 0x006E4C20 (:90)
void tracers_level_init_pre()
{
    Tracer_free_list = nullptr;
    Tracer_used_list = nullptr;
    for (int i = 0; i < 64; i++)
    {
        tracer_list_insert_front(Tracer_free_list, &Tracer_buf[i]);
    }
}

// Takes the head of the free list into the used list and fills it in (shared by both creators; inlined in rfg.exe).
static tracer* tracer_alloc(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int effect_handle, weapon_tracer_type w_tracer_type)
{
    tracer* tp = Tracer_free_list;
    tracer_list_remove(Tracer_free_list, tp);
    tracer_list_insert_front(Tracer_used_list, tp);
    tp->age_timer.set(-21); // Magic number: the tracer starts 21 ms into its flight
    tp->end_pos = end_pos;
    tp->start_pos = start_pos;
    const vector path(tp->end_pos.x - tp->start_pos.x, tp->end_pos.y - tp->start_pos.y, tp->end_pos.z - tp->start_pos.z);
    tp->duration = path.mag() * Tracer_velocity_scale;
    tp->pos = tp->start_pos;
    tp->fvec = tp->start_pos; // Original: the start position, not a direction
    tp->tracer_effect = effect_handle;
    tp->tracer_type = w_tracer_type;
    tp->owner = owner;
    tp->destroy_next_frame = false;
    tp->one_frame_delay = true;
    return tp;
}

// 0x006EDBE0 (:102)
void tracers_create(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type)
{
    if (tracer_effect == 0xFFFFFFFF)
    {
        tracer_effect = Tracer_effect_handle;
    }
    if (Tracer_free_list == nullptr)
    {
        return;
    }
    const vector dir = vector(end_pos.x - start_pos.x, end_pos.y - start_pos.y, end_pos.z - start_pos.z).copy_normalize_safe();
    matrix orient;
    orient.make_quick(dir);
    const unsigned int handle = effects_play(tracer_effect, start_pos, orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* oe = World.object_from_handle<object_effect>(handle);
    if (oe == nullptr)
    {
        return;
    }
    vfx_effect* vfx = oe->rdd.is_loaded() ? oe->rdd.get_data()->vfx_effect_p : nullptr;
    if (vfx != nullptr)
    {
        // A sniper tracer's first emitter gets one extra particle per metre of path (the trail spread along it).
        int num_trail_particles = 0;
        if (w_tracer_type == WEAPON_TRACER_SNIPER)
        {
            num_trail_particles = (int)vec_dist(start_pos, end_pos);
        }
        // Magic numbers: the four tracer particles beyond the trail, and two emitter instances plus headroom.
        if (num_trail_particles + 4 <= vfx_system::get_particle_system_manager()->get_num_particles_parametric_free() && vfx_particle_system_emitter_instance::get_objects_free() > 4)
        {
            tracer* tp = tracer_alloc(owner, start_pos, end_pos, handle, w_tracer_type);
            if (w_tracer_type != WEAPON_TRACER_RAIL)
            {
                oe->pos = tp->start_pos;
            }
            vfx_effect_template::emitter* emitters = vfx->m_effect_template_p->m_vfx_data_p->emitters;
            vfx_particle_system_emitter* trail_emitter = emitters[0].particle_system_emitter_p;
            vfx_particle_system_emitter* head_emitter = emitters[1].particle_system_emitter_p;
            if (tp->tracer_type == WEAPON_TRACER_RAIL && emitters[2].particle_system_emitter_p != nullptr)
            {
                // The rail's third emitter (the spread particles) stands still too.
                vfx_particle_system_emitter_instance* spread = vfx->m_emitters.first()->next->next;
                vfx_particle_parametric* p = spread->m_particle_parametric_list.first();
                while (p != nullptr)
                {
                    p->velocity = ZERO_VECTOR;
                    p = p->next;
                    if (p == spread->m_particle_parametric_list.first())
                    {
                        break;
                    }
                }
            }
            trail_emitter->emission_rate_keys->time = 0.0f;
            trail_emitter->emission_rate_keys->value = TRACER_PRIME_EMISSION_RATE;
            trail_emitter->max_particles = num_trail_particles + 2;
            vfx_particle_system_emitter_instance* trail_inst = vfx->m_emitters.first();
            trail_inst->reset();
            trail_inst->emission_enabled = true;
            head_emitter->emission_rate_keys->time = 0.0f;
            head_emitter->emission_rate_keys->value = TRACER_PRIME_EMISSION_RATE;
            head_emitter->max_particles = 2;
            vfx_particle_system_emitter_instance* head_inst = vfx->m_emitters.first()->next;
            head_inst->reset();
            head_inst->emission_enabled = true;
            oe->update(TRACER_PRIME_STEP);
            trail_inst->emission_enabled = false;
            vfx_particle_parametric* trail_first = trail_inst->m_particle_parametric_list.first();
            vfx_particle_parametric* trail_p = trail_first->next;
            if (tp->tracer_type == WEAPON_TRACER_SNIPER)
            {
                // The sniper's trail particle is the last one; those before it stand still.
                while (trail_p->next != trail_first)
                {
                    trail_p->velocity = ZERO_VECTOR;
                    trail_p = trail_p->next;
                }
                trail_emitter->lifetime = 2.5f; // Magic number: the sniper trail lasts 2.5 s
            }
            trail_first->velocity = ZERO_VECTOR;
            trail_p->velocity = ZERO_VECTOR;
            head_inst->emission_enabled = false;
            vfx_particle_parametric* end_p = head_inst->m_particle_parametric_list.first();
            end_p->velocity = ZERO_VECTOR;
            vfx_particle_parametric* start_p = head_inst->m_particle_parametric_list.first()->next;
            start_p->velocity = ZERO_VECTOR;
            tp->trail_particle_p = trail_p;
            tp->tracer_particle_end_p = end_p;
            tp->tracer_particle_start_p = start_p;
            oe->pos = tp->pos;
            return;
        }
    }
    oe->stop(object_effect::SS_STOP_EMITTING);
}

// 0x006EE0E0 (:263)
void curved_tracers_create(unsigned int owner, const vector& start_pos, const vector& inner_spline_pos, const vector& inner_spline_pos2, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type)
{
    if (tracer_effect == 0xFFFFFFFF)
    {
        tracer_effect = Curved_tracer_effect_handle;
    }
    if (Tracer_free_list == nullptr)
    {
        return;
    }
    const vector dir = vector(end_pos.x - start_pos.x, end_pos.y - start_pos.y, end_pos.z - start_pos.z).copy_normalize_safe();
    matrix orient;
    orient.make_quick(dir);
    const unsigned int handle = effects_play(tracer_effect, start_pos, orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* oe = World.object_from_handle<object_effect>(handle);
    if (oe == nullptr)
    {
        return;
    }
    vfx_effect* vfx = oe->rdd.is_loaded() ? oe->rdd.get_data()->vfx_effect_p : nullptr;
    // Magic numbers: four parametric particles and five emitter instances free (the curved tracer does not count its
    // spline particles).
    if (vfx == nullptr || vfx_system::get_particle_system_manager()->get_num_particles_parametric_free() < 4 || vfx_particle_system_emitter_instance::get_objects_free() < 5)
    {
        oe->stop(object_effect::SS_STOP_EMITTING);
        return;
    }
    tracer* tp = tracer_alloc(owner, start_pos, end_pos, handle, w_tracer_type);
    vfx_effect_template::emitter* emitters = vfx->m_effect_template_p->m_vfx_data_p->emitters;
    vfx_particle_system_emitter* spline_emitter = emitters[0].particle_system_emitter_p;
    vfx_particle_system_emitter* head_emitter = emitters[1].particle_system_emitter_p;
    spline_emitter->emission_rate_keys->time = 0.0f;
    spline_emitter->emission_rate_keys->value = TRACER_PRIME_EMISSION_RATE;
    spline_emitter->max_particles = num_curved_particles;
    vfx_particle_system_emitter_instance* spline_inst = vfx->m_emitters.first();
    spline_inst->reset();
    spline_inst->emission_enabled = true;
    head_emitter->emission_rate_keys->time = 0.0f;
    head_emitter->emission_rate_keys->value = TRACER_PRIME_EMISSION_RATE;
    head_emitter->max_particles = 2;
    vfx_particle_system_emitter_instance* head_inst = vfx->m_emitters.first()->next;
    head_inst->reset();
    head_inst->emission_enabled = true;
    head_emitter->lifetime = 1.5f; // Magic number: the head particles last 1.5 s
    oe->update(TRACER_PRIME_STEP);
    spline_inst->emission_enabled = false;
    bool primed = false;
    if (spline_inst->m_particle_parametric_list.get_elem_count() == num_curved_particles)
    {
        vfx_particle_parametric* first = spline_inst->m_particle_parametric_list.first();
        vfx_particle_parametric* last = spline_inst->m_particle_parametric_list.get_indexed(num_curved_particles - 1);
        if (last == nullptr)
        {
            // Original: unreachable with the count checked; the tracer would stay in the used list unprimed.
            return;
        }
        vfx_particle_parametric* p = first;
        while (p != nullptr)
        {
            p->velocity = ZERO_VECTOR;
            p = p->next;
            if (p == spline_inst->m_particle_parametric_list.first())
            {
                break;
            }
        }
        first->velocity = ZERO_VECTOR;
        last->velocity = ZERO_VECTOR;
        head_inst->emission_enabled = false;
        if (head_inst->m_particle_parametric_list.get_elem_count() == 2)
        {
            vfx_particle_parametric* end_p = head_inst->m_particle_parametric_list.first();
            end_p->velocity = ZERO_VECTOR;
            vfx_particle_parametric* start_p = head_inst->m_particle_parametric_list.first()->next;
            start_p->velocity = ZERO_VECTOR;
            tp->tracer_particle_end_p = end_p;
            tp->trail_particle_p = last;
            tp->tracer_particle_start_p = start_p;
            // The path: 8 m back along the first leg, the start, the two inner points, the end, 8 m past it.
            const vector in_dir = vector(inner_spline_pos.x - tp->start_pos.x, inner_spline_pos.y - tp->start_pos.y, inner_spline_pos.z - tp->start_pos.z).copy_normalize_safe();
            tp->spline_points[0] = vector(tp->start_pos.x - in_dir.x * 8.0f, tp->start_pos.y - in_dir.y * 8.0f, tp->start_pos.z - in_dir.z * 8.0f);
            tp->spline_points[1] = tp->start_pos;
            tp->spline_points[2] = inner_spline_pos;
            tp->spline_points[3] = inner_spline_pos2;
            tp->spline_points[4] = tp->end_pos;
            const vector out_dir = vector(tp->end_pos.x - inner_spline_pos2.x, tp->end_pos.y - inner_spline_pos2.y, tp->end_pos.z - inner_spline_pos2.z).copy_normalize_safe();
            tp->spline_points[5] = vector(tp->end_pos.x + out_dir.x * 8.0f, tp->end_pos.y + out_dir.y * 8.0f, tp->end_pos.z + out_dir.z * 8.0f);
            tp->path_spline.set_points(6, tp->spline_points);
            primed = true;
        }
    }
    if (!primed)
    {
        // Not every particle was emitted: give the tracer back.
        oe->stop(object_effect::SS_STOP_EMITTING);
        tracer_list_remove(Tracer_used_list, tp);
        tracer_list_insert_front(Tracer_free_list, tp);
    }
}

// 0x006E9AA0 (:411)
// LTCG: oe in ECX, tp in EDI, new_pos on the stack.
// Spreads the particles of the rail's third emitter (every particle) or the sniper's first emitter (all but the head and
// the last one) along the path from the start, one step apart (4 per metre for the rail, 0.1 for the sniper), and puts
// the ones beyond new_pos (the head of the tracer) at new_pos. Deviation: external linkage (a file static in rfg.exe),
// so that the vfxlib classes can befriend it.
void expanding_tracer_particle_spread(object_effect* oe, tracer* tp, vector new_pos)
{
    if (tp->tracer_type != WEAPON_TRACER_RAIL && tp->tracer_type != WEAPON_TRACER_SNIPER)
    {
        return;
    }
    // Original: no check that the effect is loaded (a null visual crashes here).
    vfx_effect* vfx = oe->rdd.is_loaded() ? oe->rdd.get_data()->vfx_effect_p : nullptr;
    vfx_particle_system_emitter_instance* inst = vfx->m_emitters.first();
    if (tp->tracer_type == WEAPON_TRACER_RAIL)
    {
        inst = inst->next->next;
    }
    if (inst->m_particle_parametric_list.get_elem_count() <= 0)
    {
        return;
    }
    const float spacing = tp->tracer_type == WEAPON_TRACER_RAIL ? 4.0f : 0.1f; // Magic numbers: steps per metre
    const int num_steps = (int)(spacing * vec_dist(tp->start_pos, tp->end_pos)) + 1;
    vector step;
    step.x = (tp->end_pos.x - tp->start_pos.x) / (float)num_steps;
    step.y = (tp->end_pos.y - tp->start_pos.y) / (float)num_steps;
    step.z = (tp->end_pos.z - tp->start_pos.z) / (float)num_steps;
    int index = 1;
    const float head_dist = vec_dist(new_pos, tp->start_pos);
    vfx_particle_parametric* p = inst->m_particle_parametric_list.first()->next;
    if (p == nullptr)
    {
        return;
    }
    for (;;)
    {
        vfx_particle_parametric* next = p->next;
        if (next == inst->m_particle_parametric_list.first() && tp->tracer_type != WEAPON_TRACER_RAIL)
        {
            return;
        }
        const float f = (float)index;
        if (step.mag() * f <= head_dist)
        {
            p->position = vector(tp->start_pos.x + f * step.x, tp->start_pos.y + step.y * f, tp->start_pos.z + step.z * f);
        }
        else
        {
            p->position = new_pos;
        }
        index++;
        if (next == inst->m_particle_parametric_list.first()->next)
        {
            return;
        }
        p = next;
        if (p == nullptr)
        {
            return;
        }
    }
}

// 0x006E9D10 (:446)
void tracers_process()
{
    tracer* next = Tracer_used_list;
    while (next != nullptr)
    {
        tracer* tp = next;
        next = tp->next != Tracer_used_list ? tp->next : nullptr;
        object_effect* oe = nullptr;
        if (tp->tracer_effect != 0xFFFFFFFF)
        {
            oe = World.object_from_handle<object_effect>(tp->tracer_effect);
        }
        if (tp->destroy_next_frame)
        {
            if (oe != nullptr)
            {
                oe->stop(object_effect::SS_STOP_EMITTING);
                tp->tracer_effect = 0xFFFFFFFF;
            }
            tracer_list_remove(Tracer_used_list, tp);
            tracer_list_insert_front(Tracer_free_list, tp);
            continue;
        }

        // A straight tracer arrives when its flight time is up, the curved one when the spline reaches the end point
        // (original: at spline parameter 4, i.e. after three times its flight time, see below).
        bool arrived;
        if (tp->tracer_type != WEAPON_TRACER_ENFORCER)
        {
            arrived = (float)tp->age_timer.time_since() > tp->duration;
        }
        else
        {
            arrived = tp->pos.x == tp->end_pos.x && tp->pos.y == tp->end_pos.y && tp->pos.z == tp->end_pos.z;
        }
        if (arrived)
        {
            tp->tracer_particle_start_p->position = tp->end_pos;
            tp->tracer_particle_end_p->position = tp->end_pos;
            tp->trail_particle_p->position = tp->end_pos;
            if (oe != nullptr)
            {
                oe->pos = tp->end_pos;
                if (tp->tracer_type == WEAPON_TRACER_RAIL || tp->tracer_type == WEAPON_TRACER_SNIPER)
                {
                    expanding_tracer_particle_spread(oe, tp, oe->pos);
                }
            }
            tp->tracer_particle_start_p->velocity = ZERO_VECTOR;
            tp->tracer_particle_end_p->velocity = ZERO_VECTOR;
            tp->trail_particle_p->velocity = ZERO_VECTOR;
            tp->destroy_next_frame = true;
            continue;
        }

        vector new_pos;
        if (tp->tracer_type != WEAPON_TRACER_ENFORCER)
        {
            const vector path(tp->end_pos.x - tp->start_pos.x, tp->end_pos.y - tp->start_pos.y, tp->end_pos.z - tp->start_pos.z);
            const float t = (float)tp->age_timer.time_since() / tp->duration;
            new_pos = vector(path.x * t + tp->start_pos.x, tp->start_pos.y + path.y * t, path.z * t + tp->start_pos.z);
            if (oe != nullptr)
            {
                if (tp->tracer_type == WEAPON_TRACER_RAIL || tp->tracer_type == WEAPON_TRACER_SNIPER)
                {
                    expanding_tracer_particle_spread(oe, tp, new_pos);
                }
                oe->pos = new_pos;
            }
        }
        else
        {
            if (oe == nullptr)
            {
                continue;
            }
            // Original: the head moves over the first leg of the spline (parameter 1 to 2) in the flight time, so it
            // reaches the end point (parameter 4) only after three times the flight time.
            const float t = (float)tp->age_timer.time_since() / tp->duration;
            tp->path_spline.get_point(new_pos, t + 1.0f);
            vfx_effect* vfx = oe->rdd.is_loaded() ? oe->rdd.get_data()->vfx_effect_p : nullptr;
            if (vfx == nullptr || vfx->m_emitters.first() == nullptr)
            {
                continue;
            }
            vfx_particle_system_emitter_instance* spline_inst = vfx->m_emitters.first();
            oe->update(0.005f); // Magic number: a 5 ms step for the spline particles
            vfx_particle_parametric* p = spline_inst->m_particle_parametric_list.first();
            int index = 0;
            while (p != nullptr)
            {
                tp->path_spline.get_point(p->position, ((float)index / (float)(num_curved_particles - 1)) * t + 1.0f);
                p = p->next;
                index++;
                if (p == spline_inst->m_particle_parametric_list.first())
                {
                    break;
                }
            }
        }
        tp->tracer_particle_start_p->position = tp->tracer_particle_end_p->position;
        tp->trail_particle_p->position = new_pos;
        tp->tracer_particle_end_p->position = tp->trail_particle_p->position;
        tp->pos = new_pos;
    }
}

// 0x006E4D80 (:533)
// Empty in retail (the tracers are particle effects).
void tracers_render()
{
}
