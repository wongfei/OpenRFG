// Original: volition/rfg/code/destruction/rfg_collisions.cpp
//
// Material effect candidates (decompiled; header comment in rfg_collisions.h). Compiled in rfg_destruction_unity.cpp
// (first of the destruction sources).
//
// A candidate is a pending material effect (dust, debris, sounds) for an event type and size, taken from the effect
// material (or the object's override effects) and pooled (Candidates, 200; per category and size at most
// Max_candidate_count). A new candidate is dropped when it is too close to the camera (20 m) or not visible beyond
// 75 m (small effects), or when a candidate of the same area is running (Event_filters); a priority candidate
// replaces a smaller pending one. After its delay a candidate starts its effect (or explosion), then lives on while
// the effect plays (its radius shrinks with its remaining time) so it blocks new effects nearby. Rolling and sliding
// effects follow their object and are renewed by new contacts.
// Candidate flags: 0x1 priority, 0x2 pending (the effect has not started), 0x4 has an orientation.

#include "volition/rfg/code/destruction/rfg_collisions.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"

#include <string.h>

static const int MAX_EFFECT_CANDIDATES = 200;
static const int NUM_EVENT_FILTERS = 2;
static const float TWO_PI = 6.2831855f;

static bool candidate_filter_by_visibility(effect_candidate* candidate);
static bool candidate_filter_by_pos(effect_candidate* candidate);

// ---- file-static globals
static bool Effect_candidate_enabled = true; // 0x016500F9
static int Max_candidate_count[NUM_MATERIAL_EVENT_CATEGORIES][3] = { // 0x01650264, [category][size]
    {1, 1, 1}, // default
    {30, 10, 10}, // collision
    {10, 10, 10}, // stress
    {10, 15, 20}, // load balance
};
static float Effect_candidate_shifting_vel_sqr = 1.0f; // 0x01650294
static float Effect_candidate_shifting_vel_multiplier = 20.0f; // 0x01650298
static bool (*Event_filters[NUM_EVENT_FILTERS])(effect_candidate*) = {candidate_filter_by_visibility, candidate_filter_by_pos}; // 0x0165029C
static int Allocated_candidate_count[NUM_MATERIAL_EVENT_CATEGORIES][3]; // 0x01EF8230
static effect_candidate* Cached_index; // 0x01EF8260, where rfg_effect_candidate_post_frame goes on
static object_pool<effect_candidate, MAX_EFFECT_CANDIDATES, 1> Candidates; // 0x01F17390

// 0x00719770
effect_candidate::effect_candidate()
{
    // delay: timestamp() invalidates it
}

// 0x0072B4C0 (:196)
// LTCG: candidate in ESI. Stops its effect.
static void candidate_remove(effect_candidate* candidate)
{
    if (candidate == nullptr)
    {
        return;
    }
    Allocated_candidate_count[candidate->event_cat][candidate->event_size]--;
    if (candidate->effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(candidate->effect_handle);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    if (candidate == Cached_index)
    {
        Cached_index = Candidates.get_next(Cached_index);
    }
    Candidates.free(candidate);
}

// 0x0072B530 (:220)
// For a priority candidate: drops the smallest pending, non priority candidate at least 5 m from the camera.
// The search stops at the first small one.
static bool find_and_remove_low_priority_candidate(bool priority)
{
    if (!priority)
    {
        return false;
    }
    effect_candidate* found = nullptr;
    {
        csspinlock_scope lock(Candidates.thread_access);
        for (effect_candidate* it = Candidates.get_first(0); it != nullptr; it = Candidates.get_next(it))
        {
            if ((it->flags & 2) == 0 || (it->flags & 1) != 0 || (found != nullptr && found->event_size <= it->event_size) || vec_dist_squared(Rfg_camera.real_pos, it->pos) < 25.0f)
            {
                continue;
            }
            found = it;
            if (it->event_size == MATERIAL_EVENT_SIZE_SMALL)
            {
                break;
            }
        }
    }
    if (found == nullptr)
    {
        return false;
    }
    candidate_remove(found);
    return true;
}

// 0x0072B600 (:259)
// LTCG: event_cat in EAX, priority in CL.
static effect_candidate* candidate_alloc(material_event_category event_cat, material_event_size event_size, bool priority)
{
    if (Candidates.get_free() < 1 || !Effect_candidate_enabled)
    {
        return nullptr;
    }
    if (Max_candidate_count[event_cat][event_size] <= Allocated_candidate_count[event_cat][event_size])
    {
        if (!priority)
        {
            return nullptr;
        }
        find_and_remove_low_priority_candidate(priority);
    }
    effect_candidate* candidate = Candidates.alloc(0);
    if (candidate == nullptr)
    {
        return nullptr;
    }
    Allocated_candidate_count[event_cat][event_size]++;
    return candidate;
}

// 0x007111E0 (:290)
static bool should_loop(material_event_type event_type)
{
    return event_type == MATERIAL_EVENT_ROLL || event_type == MATERIAL_EVENT_SLIDE;
}

// 0x0072B660 (:297)
// Whether a running candidate covers this one (true filters it out). Within the larger of both radii: a priority
// candidate replaces pending non priority ones; a larger candidate replaces smaller pending ones; otherwise it is
// covered, and a looping effect of the same owner and type moves to the new contact (500 ms more).
//
// Kept: the looping effect is placed relative to an object looked up by an uninitialised handle.
static bool candidate_filter_by_pos(effect_candidate* candidate)
{
    csspinlock_scope lock(Candidates.thread_access);
    effect_candidate* it = Candidates.get_first(0);
    while (it != nullptr)
    {
        effect_candidate* other = it;
        it = Candidates.get_next(it);
        if (other == candidate)
        {
            continue;
        }
        const float radius = 0.0f <= candidate->radius - other->radius ? candidate->radius : other->radius;
        if ((double)radius * (double)radius < (double)vec_dist_squared(other->pos, candidate->pos))
        {
            continue;
        }
        if ((candidate->flags & 1) != 0 && (other->flags & 1) == 0)
        {
            if ((other->flags & 2) != 0)
            {
                candidate_remove(other);
            }
            continue;
        }
        if (other->radius < candidate->radius)
        {
            if ((other->flags & 2) != 0 && (other->flags & 1) == 0)
            {
                candidate_remove(other);
            }
            continue;
        }
        if ((candidate->flags & 1) != 0)
        {
            continue;
        }
        if ((other->flags & 2) == 0 && candidate->owner_handle == other->owner_handle && candidate->event_type == other->event_type && should_loop(other->event_type))
        {
            object* owner = World.object_from_handle(0xFFFFFFFF); // uninitialised handle in rfg.exe
            object_effect* effect = World.object_from_handle<object_effect>(other->effect_handle);
            if (owner != nullptr && effect != nullptr)
            {
                vector local_pos = candidate->pos - owner->pos;
                local_pos.rotate(owner->orient);
                effect->set_local_position(local_pos); // inlined
                if ((candidate->flags & 4) != 0)
                {
                    effect->set_local_orient(candidate->orient * owner->orient.copy_transpose());
                }
            }
            other->delay.set(500);
        }
        return true;
    }
    return false;
}

// 0x00711200 (:354)
// Closer than 20 m, or beyond 75 m when small or not visible (true filters it out).
static bool candidate_filter_by_visibility(effect_candidate* candidate)
{
    const float dist_sqr = vec_dist_squared(Rfg_camera.real_pos, candidate->pos);
    if (dist_sqr < 400.0f)
    {
        return false; // kept: close candidates pass
    }
    if (5625.0f < dist_sqr && candidate->event_size < 1)
    {
        return true;
    }
    const vector bmin(candidate->pos.x - candidate->radius, candidate->pos.y - candidate->radius, candidate->pos.z - candidate->radius);
    const vector bmax(candidate->pos.x + candidate->radius, candidate->pos.y + candidate->radius, candidate->pos.z + candidate->radius);
    return !game_render_is_object_visible(bmin, bmax);
}

// 0x007237D0 (:383)
// Once per frame.
static void update_pos_and_radius()
{
    static int Last_update = -1;
    if (Last_update == Framecount)
    {
        return;
    }
    Last_update = Framecount;
    csspinlock_scope lock(Candidates.thread_access);
    for (effect_candidate* it = Candidates.get_first(0); it != nullptr; it = Candidates.get_next(it))
    {
        it->update_pos_and_radius();
    }
}

// 0x0072B8E0 (:399)
// LTCG: candidate in ESI. A filtered out candidate is removed.
static bool candidate_should_create(effect_candidate* candidate)
{
    update_pos_and_radius();
    for (int i = 0; i < NUM_EVENT_FILTERS; i++)
    {
        if (Event_filters[i](candidate))
        {
            candidate_remove(candidate); // inlined
            return false;
        }
    }
    return true;
}

// 0x0071B220 (:417)
static const level_object_base_properties* get_override_base_props(const object* const op)
{
    if (op != nullptr && op->get_obj_type() == OT_MOVER && ((const object_mover*)op)->m_level_obj_properties != nullptr)
    {
        return ((const object_mover*)op)->m_level_obj_properties->m_base_properties;
    }
    return nullptr;
}

// A general mover that is killed gives no override effects.
static bool candidate_allows_override(const object* objp)
{
    return objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_GENERAL || !objp->is_killed();
}

// 0x0071B240 (:429)
// LTCG: coll_event in EAX. The objects' own effect for melee hits (per melee group) or impacts, rolls and slides
// (side a first).
static unsigned int candidate_get_override_effect(const collision_event* coll_event, material_event_type event_type, unsigned char melee_group_index)
{
    const level_object_base_properties* props_a = get_override_base_props(coll_event->m_object_a); // inlined
    const level_object_base_properties* props_b = get_override_base_props(coll_event->m_object_b); // inlined
    const bool allow_a = candidate_allows_override(coll_event->m_object_a);
    const bool allow_b = candidate_allows_override(coll_event->m_object_b);
    switch (event_type)
    {
    case MATERIAL_EVENT_MELEE:
        if (allow_a && props_a != nullptr && props_a->m_melee_impact_override != nullptr && props_a->m_melee_impact_override[melee_group_index] != 0xFFFFFFFF)
        {
            return props_a->m_melee_impact_override[melee_group_index];
        }
        if (allow_b && props_b != nullptr && props_b->m_melee_impact_override != nullptr && props_b->m_melee_impact_override[melee_group_index] != 0xFFFFFFFF)
        {
            return props_b->m_melee_impact_override[melee_group_index];
        }
        return 0xFFFFFFFF;
    case MATERIAL_EVENT_IMPACT:
    case MATERIAL_EVENT_ROLL:
    case MATERIAL_EVENT_SLIDE:
        if (allow_a && props_a != nullptr && props_a->m_collision_impact_override != 0xFFFFFFFF)
        {
            return props_a->m_collision_impact_override;
        }
        if (allow_b && props_b != nullptr && props_b->m_collision_impact_override != 0xFFFFFFFF)
        {
            return props_b->m_collision_impact_override;
        }
        return 0xFFFFFFFF;
    default:
        return 0xFFFFFFFF;
    }
}

// 0x0071B370 (:467)
// LTCG: obj_a in EAX, obj_b in ECX (event_type unused). With an override impact energy (side a first), energies at
// least that high make small effects.
static material_event_size candidate_get_override_event_size(material_event_type event_type, const object* const obj_a, const object* const obj_b, float input)
{
    const level_object_base_properties* props_a = get_override_base_props(obj_a); // inlined
    const level_object_base_properties* props_b = get_override_base_props(obj_b); // inlined
    float min_energy;
    if (props_a != nullptr && -1.0f < props_a->m_collision_min_impact_energy)
    {
        min_energy = props_a->m_collision_min_impact_energy;
    }
    else if (props_b != nullptr && -1.0f < props_b->m_collision_min_impact_energy)
    {
        min_energy = props_b->m_collision_min_impact_energy;
    }
    else
    {
        return INVALID_MATERIAL_EVENT_SIZE;
    }
    return input < min_energy ? INVALID_MATERIAL_EVENT_SIZE : MATERIAL_EVENT_SIZE_SMALL;
}

// 0x0072B980 (:487)
// A candidate for an event, unless no effect exists for it (override or effect material) or the pool refuses it.
static bool rfg_effect_candidate_allocate(const collision_event* coll_event, unsigned char material_index, unsigned char other_index, material_event_type event_type, material_event_size event_size, material_shape_type shape_type, float effect_volume, const vector& pos, bool priority, unsigned int delay, float energy, const matrix* orient, unsigned int key, unsigned char melee_group_index)
{
    unsigned int effect = candidate_get_override_effect(coll_event, event_type, melee_group_index);
    if (effect == 0xFFFFFFFF)
    {
        effect = rfg_effect_material_get_event_handle(event_type, event_size, shape_type, material_index, other_index, melee_group_index);
        if (effect == 0xFFFFFFFF)
        {
            return false;
        }
    }
    material_event_category event_cat;
    switch (event_type)
    {
    case MATERIAL_EVENT_EXPLODE:
    case MATERIAL_EVENT_LOAD_BALANCE:
        event_cat = MATERIAL_EVENT_CATEGORY_LOAD_BALANCE;
        break;
    case MATERIAL_EVENT_STRESS:
    case MATERIAL_EVENT_DETACH:
    case MATERIAL_EVENT_GROAN:
        event_cat = MATERIAL_EVENT_CATEGORY_STRESS;
        break;
    case MATERIAL_EVENT_SHATTER:
    case MATERIAL_EVENT_COLLAPSE:
    case MATERIAL_EVENT_MELEE:
    case MATERIAL_EVENT_BULLET:
    case MATERIAL_EVENT_IMPACT:
    case MATERIAL_EVENT_SHIFT:
    case MATERIAL_EVENT_ROLL:
    case MATERIAL_EVENT_SLIDE:
        event_cat = MATERIAL_EVENT_CATEGORY_COLLISION;
        break;
    default:
        event_cat = MATERIAL_EVENT_CATEGORY_DEFAULT;
        break;
    }
    effect_candidate* candidate = candidate_alloc(event_cat, event_size, priority);
    if (candidate == nullptr)
    {
        return false;
    }
    candidate->event_size = event_size;
    candidate->event_type = event_type;
    candidate->event_cat = event_cat;
    candidate->info_handle = effect;
    candidate->effect_volume = effect_volume;
    candidate->effect_material_p = rfg_get_effect_material_by_index(material_index); // inlined
    candidate->flags = priority ? 3 : 2;
    if (orient == nullptr)
    {
        candidate->orient = IDENTITY_MATRIX;
    }
    else
    {
        candidate->flags |= 4;
        candidate->orient = *orient;
    }
    candidate->pos = pos;
    candidate->delay.set(delay);
    candidate->duration = 0.0f;
    candidate->update_pos_and_radius();
    candidate->energy = energy;
    candidate->owner_handle = coll_event->m_object_a != nullptr ? coll_event->m_object_a->get_handle() : 0xFFFFFFFF;
    candidate->shape_key = key;
    candidate->effect_handle = 0xFFFFFFFF;
    return candidate_should_create(candidate);
}

// 0x0071B3F0 (:559)
// LTCG: objp in ECX, event_size in EDX. Removable small general movers (0x200/0x400) and, in heavy destruction,
// small (uber heavy: medium) effects of rfg_movers make none.
static bool ignore_object(object* objp, object* other_objp, material_event_size event_size, const collision_event* event)
{
    if (objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        return (((object_mover*)objp)->mover_flags & 0x600) != 0;
    }
    if (objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return false;
    }
    rfg_mover* rm = (rfg_mover*)objp;
    const rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
    if (shape == nullptr)
    {
        return false;
    }
    return ((shape->flags & 4) != 0 && event_size < 1) || ((shape->flags & 8) != 0 && event_size < 2);
}

// 0x00711310 (:608)
// LTCG: objp in ECX. Humans with a ragdoll, items and movers report their collisions themselves.
static bool will_report_collision(object* objp)
{
    if (objp == nullptr)
    {
        return false;
    }
    switch (objp->get_obj_type())
    {
    case OT_HUMAN:
        return ((human*)objp)->ragdoll_idx != -1;
    case OT_ITEM:
    case OT_MOVER:
        return true;
    default:
        return false;
    }
}

// 0x00732370 (:641)
void rfg_effect_candidate_level_init_pre()
{
    {
        csspinlock_scope lock(Candidates.thread_access);
        effect_candidate* candidate = Candidates.get_first(0);
        while (candidate != nullptr)
        {
            candidate = Candidates.free(candidate);
        }
    }
    Cached_index = nullptr;
    memset(Allocated_candidate_count, 0, sizeof(Allocated_candidate_count));
}

// 0x007323D0 (:653)
// Up to 5 candidates start (or end) per frame, going on from where the last frame stopped.
void rfg_effect_candidate_post_frame()
{
    int count = 0;
    if (Cached_index == nullptr)
    {
        Cached_index = Candidates.get_first(0);
    }
    update_pos_and_radius();
    csspinlock_scope lock(Candidates.thread_access);
    effect_candidate* candidate = Cached_index;
    effect_candidate* next = nullptr;
    while (candidate != nullptr)
    {
        next = Candidates.get_next(candidate);
        if (candidate->do_frame() && 5 <= ++count)
        {
            break;
        }
        candidate = next;
    }
    Cached_index = candidate != nullptr ? next : nullptr;
}

// 0x0072BB70 (:684)
// When its delay is up: a pending candidate starts its effect (a random yaw without an orientation; an explosion for
// explode events; looping for rolls and slides; sized and tinted by its material) and stays for 500 ms (looping) or
// radius x 500 ms; candidates of pieces gone already are dropped. A started candidate is removed. The effect follows
// its owner's speed. Returns whether it did anything.
bool effect_candidate::do_frame()
{
    if (!delay.elapsed())
    {
        return false;
    }
    if (effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(effect_handle);
        if (effect != nullptr)
        {
            object* owner = World.object_from_handle(owner_handle);
            if (owner != nullptr)
            {
                vector velocity;
                if (owner->get_obj_type() == OT_HUMAN)
                {
                    velocity = ((human*)owner)->velocity;
                }
                else if (owner->havok_handle == 0xFFFFFFFF)
                {
                    velocity.zero();
                }
                else
                {
                    havok_body_get_linear_velocity(owner->havok_handle, velocity);
                }
                effect->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, sqrtf(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z));
            }
        }
    }
    if ((flags & 2) == 0)
    {
        candidate_remove(this);
        return true;
    }
    flags &= ~2;
    if (shape_key != 0xFFFFFFFF && owner_handle != 0xFFFFFFFF)
    {
        rfg_mover* rm = World.object_from_handle<rfg_mover>(owner_handle);
        if (rm != nullptr)
        {
            rfg_destroyable_shape* shape = rm->get_shape();
            if (shape != nullptr && !shape->get_shape_state(shape_key))
            {
                candidate_remove(this);
                return true;
            }
        }
    }
    if ((flags & 4) == 0)
    {
        orient.make_ry(fl_rand_range(0.0f, TWO_PI));
    }
    if (event_type == MATERIAL_EVENT_EXPLODE)
    {
        explosion_create(exp_info, nullptr, nullptr, pos, orient, nullptr, nullptr, false);
    }
    else
    {
        effect_handle = effects_play(info_handle, pos, orient, should_loop(event_type), owner_handle, -1, 0xFFFFFFFF, effect_volume, false, EFFECTS_CATEGORY_NONE); // inlined
        if (effect_handle == 0xFFFFFFFF)
        {
            candidate_remove(this);
            return true;
        }
        object_effect* effect = World.object_from_handle<object_effect>(effect_handle);
        if (effect != nullptr)
        {
            effect->set_expression_variable(EFFECTS_VAR_OBJECT_SIZE, (float)event_size);
            if (effect_material_p->effect_tint_enabled)
            {
                effect->set_material_tint(effect_material_p->effect_tint);
            }
        }
    }
    const int lifetime = should_loop(event_type) ? 500 : (int)(radius * 500.0f);
    duration = (float)lifetime;
    delay.set(lifetime);
    return true;
}

// 0x0071B440 (:787)
// The effect's radius (and position once running); a started candidate's radius shrinks with its remaining time.
void effect_candidate::update_pos_and_radius()
{
    if (event_type == MATERIAL_EVENT_EXPLODE)
    {
        radius = exp_info->m_radius;
    }
    else
    {
        object_effect* effect = effect_handle != 0xFFFFFFFF ? World.object_from_handle<object_effect>(effect_handle) : nullptr;
        if (effect == nullptr)
        {
            const effect_info* info = effects_get_effect_info(info_handle);
            if (info == nullptr)
            {
                return;
            }
            const visual_effect_info* visual = effects_get_visual_effect_info(info->visual_handle);
            radius = visual != nullptr ? visual->radius : 1.0f;
        }
        else
        {
            radius = effect->get_visual_radius();
            pos = effect->pos;
        }
    }
    if (0.0f < duration && (flags & 2) == 0 && !should_loop(event_type))
    {
        if (delay.elapsed())
        {
            radius = 0.0f;
            return;
        }
        radius = ((float)delay.time_until() / duration) * radius;
    }
}

// 0x00711340 (:816)
// Partial: debug rendering (event type colour sphere and "flags - energy" text) is not recovered.
void effect_candidate::render_debug()
{
}

// 0x0071B540 (:831)
// The size of an event of the material: input over the event's large / medium / small thresholds.
material_event_size rfg_effect_candidate_get_size(unsigned char mat_index, material_event_type event_type, float input)
{
    const base_event_candidate* candidate = rfg_get_effect_material_by_index(mat_index)->effect_candidate[event_type]; // inlined
    if (candidate != nullptr)
    {
        if (candidate->event_size[2] < input)
        {
            return MATERIAL_EVENT_SIZE_LARGE;
        }
        if (candidate->event_size[1] < input)
        {
            return MATERIAL_EVENT_SIZE_MEDIUM;
        }
        if (candidate->event_size[0] < input)
        {
            return MATERIAL_EVENT_SIZE_SMALL;
        }
    }
    return INVALID_MATERIAL_EVENT_SIZE;
}

// 0x0072BDF0 (:850)
// An effect at pos for an owner. Without a shape type the physical material's default one.
//
// Kept: material_index (an effect material index for the callers) also looks up the physical material for the default
// shape type.
bool rfg_effect_candidate_create(unsigned char material_index, const vector& pos, material_event_type type, material_event_size size, material_shape_type shape_type, bool priority, unsigned int delay, const matrix* orient, object* owner, unsigned int key, unsigned char other_mat, unsigned char melee_group_index)
{
    collision_event coll_event;
    coll_event.m_object_a = owner;
    coll_event.m_object_b = nullptr;
    if (shape_type == INVALID_MATERIAL_EVENT_SHAPE_TYPE)
    {
        shape_type = rfg_get_physical_material_by_index(material_index)->effect_material->default_shape_type; // inlined
    }
    return rfg_effect_candidate_allocate(&coll_event, material_index, other_mat, type, size, shape_type, 1.0f, pos, priority, delay, 0.0f, orient, 0xFFFFFFFF, melee_group_index);
}

// 0x0072BEF0 (:870)
// A Havok collision between two objects (reported once: by the lower address when both report): when one side is at
// rest the moving side impacts, rolls (spinning), slides (moving along the surface) or shifts (slowly; rolls and
// slides of movers only once they hit, flag 0x2000), sized by the moving object; otherwise an impact sized by the
// energy in the centre of mass frame. The energy (x0.5 x the larger energy scale) sizes the effect of side a's
// material (or the objects' override energy); the volume grows with the speed (100 m2/s2 for full volume).
bool rfg_effect_candidate_collide(const collision_event* coll_event)
{
    object* obj_a = coll_event->m_object_a;
    object* obj_b = coll_event->m_object_b;
    if (obj_a == nullptr || obj_b == nullptr || obj_a->obj_flags.destroyed || obj_b->obj_flags.destroyed || (will_report_collision(obj_a) && will_report_collision(obj_b) && obj_b < obj_a))
    {
        return false;
    }
    const collision_event::collision_info& info = coll_event->m_collisions[0];
    const vector& va = info.m_a_point_velocity;
    const vector& vb = info.m_b_point_velocity;
    const bool a_at_rest = va.x * va.x + va.y * va.y + va.z * va.z < 0.010000001f; // 10 cm/s
    const bool b_at_rest = vb.x * vb.x + vb.y * vb.y + vb.z * vb.z < 0.010000001f;
    if (a_at_rest && b_at_rest)
    {
        return false;
    }
    float speed_sqr = info.m_relative_normal_velocity * info.m_relative_normal_velocity;
    material_event_type event_type = MATERIAL_EVENT_IMPACT;
    material_event_size event_size = MATERIAL_EVENT_SIZE_SMALL;
    const float mass_a = obj_a->get_mass();
    const float mass_b = obj_b->get_mass();
    object_mover* moving_mover = nullptr;
    float energy;
    if (a_at_rest || b_at_rest)
    {
        object* moving = a_at_rest ? obj_b : obj_a;
        if (moving != nullptr && moving->get_obj_type() == OT_VEHICLE && moving->get_sub_type() == OT_SUB_VEHICLE_WALKER && walker_should_ignore_collision_effect((walker*)moving))
        {
            return false;
        }
        if (moving != nullptr && moving->get_obj_type() == OT_MOVER)
        {
            moving_mover = (object_mover*)moving;
            vector angular_velocity;
            havok_body_get_angular_velocity(moving->havok_handle, angular_velocity);
            const float tangent_sqr = info.m_relative_point_velocity.mag_squared() - speed_sqr;
            if (speed_sqr < angular_velocity.mag_squared())
            {
                event_type = MATERIAL_EVENT_ROLL;
                speed_sqr = tangent_sqr;
            }
            else if (speed_sqr < tangent_sqr)
            {
                event_type = MATERIAL_EVENT_SLIDE;
                speed_sqr = tangent_sqr;
            }
            if (speed_sqr < Effect_candidate_shifting_vel_sqr)
            {
                speed_sqr = Effect_candidate_shifting_vel_multiplier * speed_sqr;
                event_type = MATERIAL_EVENT_SHIFT;
            }
            if ((moving_mover->mover_flags & 0x2000) == 0 && should_loop(event_type))
            {
                event_type = MATERIAL_EVENT_IMPACT; // rolls and slides once it hit
            }
        }
        vector bmin;
        vector bmax;
        moving->get_local_bbox(bmin, bmax);
        const unsigned int shape_key = a_at_rest ? info.m_shapekey_b : info.m_shapekey_a;
        energy = (a_at_rest ? mass_b : mass_a) * speed_sqr;
        const unsigned char material = moving->get_effect_material(shape_key, nullptr, nullptr);
        float largest = bmax.y - bmin.y;
        if (largest - (bmax.z - bmin.z) < 0.0f)
        {
            largest = bmax.z - bmin.z;
        }
        if (0.0f <= (bmax.x - bmin.x) - largest)
        {
            largest = bmax.x - bmin.x;
        }
        event_size = rfg_effect_candidate_get_size(material, MATERIAL_EVENT_LOAD_BALANCE, largest);
    }
    else
    {
        const float total_mass = mass_b + mass_a;
        const vector v_cm((va.x * mass_a + vb.x * mass_b) / total_mass, (va.y * mass_a + mass_b * vb.y) / total_mass, (va.z * mass_a + mass_b * vb.z) / total_mass);
        float normal_a = 0.0f;
        float normal_b = 0.0f;
        vector rel = va - v_cm;
        if (!rel.is_null())
        {
            const float mag = rel.mag_and_normalize();
            normal_a = (rel * info.m_collision_normal) * mag;
        }
        rel = vb - v_cm;
        if (!rel.is_null())
        {
            const float mag = rel.mag_and_normalize();
            normal_b = (rel * info.m_collision_normal) * mag;
        }
        energy = normal_b * normal_b * mass_b + normal_a * normal_a * mass_a;
    }
    const float scale_a = obj_a->get_energy_scale();
    const float scale_b = obj_b->get_energy_scale();
    energy = (0.0f <= scale_a - scale_b ? scale_a : scale_b) * 0.5f * energy;
    const unsigned char material_a = obj_a->get_effect_material(info.m_shapekey_a, nullptr, nullptr);
    const unsigned char material_b = obj_b->get_effect_material(info.m_shapekey_b, &info.m_collision_point, nullptr);
    const material_shape_type shape_type = rfg_material_get_shape_type(obj_a, info.m_shapekey_a);
    material_event_size size = rfg_effect_candidate_get_size(material_a, event_type, energy);
    if (size == INVALID_MATERIAL_EVENT_SIZE)
    {
        size = candidate_get_override_event_size(event_type, obj_a, obj_b, energy);
        if (size == INVALID_MATERIAL_EVENT_SIZE)
        {
            return false;
        }
    }
    if (event_size <= size)
    {
        event_size = size;
    }
    if (ignore_object(obj_a, obj_b, event_size, coll_event) || ignore_object(obj_b, obj_a, event_size, coll_event))
    {
        return false;
    }
    float volume = speed_sqr / 100.0f;
    if (volume - 0.0f < 0.0f)
    {
        volume = 0.0f;
    }
    if (0.0f <= volume - 1.0f)
    {
        volume = 1.0f;
    }
    const bool created = rfg_effect_candidate_allocate(coll_event, material_a, material_b, event_type, event_size, shape_type, volume, info.m_collision_point, false, 0, energy, nullptr, 0xFFFFFFFF, 0xFF);
    if (created && event_type == MATERIAL_EVENT_IMPACT && moving_mover != nullptr)
    {
        moving_mover->mover_flags |= 0x2000;
    }
    return created;
}

// 0x00711480 (:1044)
void rfg_effect_candidate_render_debug()
{
}
