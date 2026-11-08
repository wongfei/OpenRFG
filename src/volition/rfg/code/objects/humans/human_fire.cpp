// Original: volition/rfg/code/objects/humans/human_fire.cpp
//
// Humans on fire (multiplayer.md MP8): four burning slots, each with fire effects on the thighs and pelvis. A burning
// human takes 10 damage a second and sets the humans touching it alight (single player, or the server); a corpse burns
// on for 5 s, a player for 20 s. When all slots burn, the slot of a dead or culled human (or the one farthest from
// the camera) is reused. Compiled in rfg_objects_unity.cpp.
// The external interface is declared in human_fire.h.

#include "volition/rfg/code/objects/humans/human_fire.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// ---- file-static globals
static unsigned int Left_leg_fire_effect = 0xFFFFFFFF; // 0x01694384
static unsigned int Right_leg_fire_effect = 0xFFFFFFFF; // 0x01694388
static unsigned int Torso_fire_effect = 0xFFFFFFFF; // 0x0169438C
static human_fire Human_fires[4]; // 0x0301AE78

// 0x00A2FD50 (:117): stops the slot's effects and frees it (LTCG: fire_idx in EAX).
static void human_fire_reset_slot(int fire_idx)
{
    human_fire& fire = Human_fires[fire_idx];
    object* objp = fire.object_handle != 0xFFFFFFFF ? World.object_from_handle(fire.object_handle) : nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
    {
        ((human*)objp)->hflags.on_fire = 0;
    }
    object_effect* torso = World.object_from_handle<object_effect>(fire.torso_fire_handle);
    object_effect* left_leg = World.object_from_handle<object_effect>(fire.left_leg_fire_handle);
    object_effect* right_leg = World.object_from_handle<object_effect>(fire.right_leg_fire_handle);
    if (torso != nullptr)
    {
        torso->stop(object_effect::SS_STOP_EMITTING);
    }
    if (left_leg != nullptr)
    {
        left_leg->stop(object_effect::SS_STOP_EMITTING);
    }
    if (right_leg != nullptr)
    {
        right_leg->stop(object_effect::SS_STOP_EMITTING);
    }
    fire.torso_fire_handle = 0xFFFFFFFF;
    fire.left_leg_fire_handle = 0xFFFFFFFF;
    fire.right_leg_fire_handle = 0xFFFFFFFF;
    fire.object_handle = 0xFFFFFFFF;
}

// 0x00A55B20 (:156)
void human_fire_stop(unsigned int object_handle)
{
    int fire_idx = 0;
    while (Human_fires[fire_idx].object_handle != object_handle)
    {
        fire_idx++;
        if (fire_idx > 3)
        {
            return;
        }
    }
    human_fire_reset_slot(fire_idx);
    if (object_handle == 0xFFFFFFFF)
    {
        return;
    }
    human* hp = (human*)World.object_from_handle(object_handle);
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC || hp->is_killed())
    {
        return;
    }
    // Original: an npc whose fire stops checks for its vehicle and ignores the answer (the reaction was compiled
    // out).
    vehicle* vp = hp->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(hp->vehicle_handle) : nullptr;
    human_in_vehicle(hp, vp);
}

// 0x00A0A4F0 (:183)
void human_fire_system_init()
{
    for (human_fire& fire : Human_fires)
    {
        fire.left_leg_fire_handle = 0xFFFFFFFF;
        fire.right_leg_fire_handle = 0xFFFFFFFF;
        fire.torso_fire_handle = 0xFFFFFFFF;
        fire.object_handle = 0xFFFFFFFF;
    }
    Left_leg_fire_effect = effects_get_handle("human_fire_leg");
    // Original: the right leg burns with the left arm's effect.
    Right_leg_fire_effect = effects_get_handle("char_fire_arm_left_a");
    Torso_fire_effect = effects_get_handle("char_fire_torso_a");
}

// 0x00AD46F0 (:205): sets the human alight (not one already burning, nor an invulnerable local player). A living
// human takes 5 to 10 damage at once and an npc replans. The kill goes to the killer, or to whoever set the burning
// killer alight.
bool human_fire_ignite(human* hp_to_ignite, human* killer_hp)
{
    if (!localization_allow_humans_on_fire())
    {
        return false;
    }
    int fire_idx = 0;
    while (fire_idx < 4 && Human_fires[fire_idx].object_handle != 0xFFFFFFFF)
    {
        fire_idx++;
    }
    if (fire_idx == 4)
    {
        // Magic number: a burning human counts as culled when a 2 m sphere around it is.
        fire_idx = 0;
        while (fire_idx < 4)
        {
            human* hp = World.object_from_handle<human>(Human_fires[fire_idx].object_handle);
            if (hp == nullptr || human_is_dead(hp) || game_render_cull_sphere(hp->pos, 2.0f))
            {
                break;
            }
            fire_idx++;
        }
        if (fire_idx == 4)
        {
            fire_idx = 0;
            float farthest = 0.0f;
            for (int i = 0; i < 4; i++)
            {
                // Original: the human is used unchecked here (the loop above found all four).
                const human* hp = World.object_from_handle<human>(Human_fires[i].object_handle);
                const float dist = vec_dist_squared(hp->pos, Rfg_camera.real_pos);
                if (farthest < dist)
                {
                    fire_idx = i;
                    farthest = dist;
                }
            }
        }
        human_fire_stop(Human_fires[fire_idx].object_handle);
    }
    if (human_is_on_fire(hp_to_ignite))
    {
        return false;
    }
    if (hp_to_ignite->is_invulnerable() && object_is_local_player(hp_to_ignite))
    {
        return false;
    }
    const int left_thigh = animlib_find_character_tag(hp_to_ignite->char_inst->aim_handle, "l-thigh");
    const int right_thigh = animlib_find_character_tag(hp_to_ignite->char_inst->aim_handle, "r-thigh");
    const int pelvis = animlib_find_character_tag(hp_to_ignite->char_inst->aim_handle, "pelvis");
    human_fire& fire = Human_fires[fire_idx];
    fire.left_leg_fire_handle = effects_play(Left_leg_fire_effect, ZERO_VECTOR, IDENTITY_MATRIX, true, hp_to_ignite->get_handle(), left_thigh, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    fire.right_leg_fire_handle = effects_play(Right_leg_fire_effect, ZERO_VECTOR, IDENTITY_MATRIX, true, hp_to_ignite->get_handle(), right_thigh, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    fire.torso_fire_handle = effects_play(Torso_fire_effect, ZERO_VECTOR, IDENTITY_MATRIX, true, hp_to_ignite->get_handle(), pelvis, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    fire.object_handle = hp_to_ignite->get_handle();
    hp_to_ignite->hflags.on_fire = 1;
    if (!object_is_local_player(hp_to_ignite) && !human_is_dead(hp_to_ignite))
    {
        npc* np = World.object_cast<npc>(hp_to_ignite);
        if (np != nullptr)
        {
            np->get_ai()->planner.request_replan();
        }
        // Magic numbers: the ignition burn.
        human_damage(hp_to_ignite, killer_hp, fl_rand_range(5.0f, 10.0f), DBT_NONE, nullptr, nullptr, true, false);
    }
    if (killer_hp == nullptr)
    {
        fire.killer_handle = 0xFFFFFFFF;
    }
    else if (!human_is_on_fire(killer_hp) || object_is_local_player(killer_hp))
    {
        fire.killer_handle = killer_hp->get_handle();
    }
    else
    {
        // Original: a killer that burns in no slot leaves the slot's old killer in place.
        for (const human_fire& other : Human_fires)
        {
            if (other.object_handle == killer_hp->get_handle())
            {
                fire.killer_handle = other.killer_handle;
            }
        }
    }
    hp_to_ignite->turn_off_fire_timestamp.invalidate();
    return true;
}

// 0x00AD4A70 (:340)
void human_fire_do_frame()
{
    for (int fire_idx = 0; fire_idx < 4; fire_idx++)
    {
        const human_fire& fire = Human_fires[fire_idx];
        if (fire.object_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object_effect* torso = World.object_from_handle<object_effect>(fire.torso_fire_handle);
        human* hp = fire.object_handle != 0xFFFFFFFF ? (human*)World.object_from_handle(fire.object_handle) : nullptr;
        if (hp != nullptr && hp->get_obj_type() != OT_HUMAN)
        {
            hp = nullptr;
        }
        human* killer = fire.killer_handle != 0xFFFFFFFF ? (human*)World.object_from_handle(fire.killer_handle) : nullptr;
        if (killer != nullptr && killer->get_obj_type() != OT_HUMAN)
        {
            killer = nullptr;
        }
        if (hp == nullptr)
        {
            human_fire_reset_slot(fire_idx);
            continue;
        }
        if (hp->hflags.hidden || torso == nullptr || hp->turn_off_fire_timestamp.elapsed())
        {
            human_fire_stop(hp->get_handle());
            continue;
        }
        if (hp->is_killed())
        {
            // Magic number: a corpse burns for 5 s.
            if (!hp->turn_off_fire_timestamp.valid())
            {
                hp->turn_off_fire_timestamp.set(5000);
            }
        }
        else
        {
            // Magic number: 10 damage a second.
            human_damage(hp, killer, Frametime * 10.0f, DBT_NONE, nullptr, nullptr, true, false);
            // Magic number: a player burns for 20 s.
            if (!hp->turn_off_fire_timestamp.valid() && object_is_player(hp->get_handle()))
            {
                hp->turn_off_fire_timestamp.set(20000);
            }
        }
        if (Multiplayer && !multi_is_server())
        {
            continue;
        }
        // The fire spreads to living, visible humans on foot within 1.1 m (and 1 m of height).
        const int num_humans = World.get_num_objects<human>();
        for (int i = 0; i < num_humans; i++)
        {
            human* other = World.get_object<human>(i);
            // Original: a null other passes the on_fire test and is dereferenced.
            if (hp == other || (other != nullptr && other->hflags.on_fire) || other->hflags.hidden || other->is_killed() || human_in_vehicle(other) != nullptr)
            {
                continue;
            }
            const vector flat(hp->pos.x - other->pos.x, 0.0f, hp->pos.z - other->pos.z);
            // Magic numbers: 1.21 = 1.1 m squared.
            if (flat.mag_squared() < 1.21f && fabsf(hp->pos.y - other->pos.y) < 1.0f && human_fire_ignite(other, hp))
            {
                human_damage(other, hp, Frametime * 10.0f, DBT_NONE, nullptr, nullptr, true, false);
            }
        }
    }
}

// 0x00A0A580 (:424)
bool human_is_on_fire(human* hp)
{
    if (hp == nullptr)
    {
        return false;
    }
    return hp->hflags.on_fire;
}
