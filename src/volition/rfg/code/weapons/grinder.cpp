// Original: volition/rfg/code/weapons/grinder.cpp
//
// The grinder (player_minimal.md P4, weapons.md WP1): charging on the trigger (the blade fires at full charge), the
// charge and charged effects and sounds, stopping them, and the heat parameter of the charge effect's meshes.
// Compiled in rfg_weapons_unity.cpp.
// The external interface is declared in weapons.h.

#include "volition/rfg/code/weapons/weapons.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/air_bombing.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_player.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vlib/code/os/frametime.h"

// ---- file-static globals
static float Grinder_increase_rate_hold = 3.0f; // 0x016EBC34, charge per second while the trigger is held (1/3 s to fire)
static float Grinder_increase_rate_hold_upgrade = 5.0f; // 0x016EBC38, the same with the fast grinder (single player only)
static float Grinder_cool_rate = 7.0f; // 0x016EBC3C, heat lost per second when not charging
static float Grinder_low_vibration = 0.1f; // 0x016EBC40, camera shake scale at no charge
static float Grinder_high_vibration = 0.8f; // 0x016EBC44, camera shake scale at full charge
static bool Full_auto_grinder; // 0x03882CB5, debug: release the trigger every frame

// A public global of air_bombing.cpp. The single player air strike turns the grinder
// into the target designator (its charge calls the strike instead of firing).
// bool Air_bomb_weapon (0x02229602) is in air_bombing.cpp, gameflow.md GF3.

// 0x00BE4940 (:55)
// The local player's grinder charges while the trigger is held and fires (returns true) at full charge; released, it
// cools. Remote owners return true (their shots come from the server). A grinder that is not its owner's current
// weapon, or whose owner is dead, stops its sound and effects. In multiplayer the charge state goes to the handshake
// (PMPF_CHARGE_WEAPON: 1 charging, 2 charged or released) so that remote copies play the effects.
bool weapon::charge_grinder(bool charge_up)
{
    human* hp = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        hp = (human*)World.object_from_handle(get_owner_handle());
        if (hp != nullptr && hp->get_obj_type() != OT_HUMAN)
        {
            hp = nullptr;
        }
    }
    bool fired = false;
    if (hp == nullptr || human_weapons_get_weapon(hp) != this || hp->is_killed())
    {
        grinder_speed_percent = 0.0f;
        if (special_sound_id != -1)
        {
            audiolib_play_fade_out(special_sound_id, 800); // Magic number: an 800 ms fade
            special_sound_id = -1;
        }
        stop_all_grinder_effects();
        return false;
    }
    if (!object_is_local_player(hp))
    {
        return true;
    }
    const float previous_speed = grinder_speed_percent;
    if (Full_auto_grinder && is_loaded() && flags.trigger_down)
    {
        if (cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func != nullptr)
        {
            cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func(WEAPON_CB_IDX_RELEASE_TRIGGER, this, cb[WEAPON_CB_IDX_RELEASE_TRIGGER].user_handle);
        }
        flags.trigger_down = false;
        num_rounds_fired = 0;
    }
    if (is_loaded())
    {
        // A held trigger that already fired does not charge again; otherwise the trigger is released (the grinder fires
        // from its charge, not from the trigger).
        if (charge_up && flags.trigger_down)
        {
            charge_up = false;
        }
        else
        {
            if (!charge_up && flags.trigger_down && cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func != nullptr)
            {
                cb[WEAPON_CB_IDX_RELEASE_TRIGGER].func(WEAPON_CB_IDX_RELEASE_TRIGGER, this, cb[WEAPON_CB_IDX_RELEASE_TRIGGER].user_handle);
            }
            flags.trigger_down = false;
            num_rounds_fired = 0;
        }
    }
    else
    {
        if (charge_up && !flags.trigger_down)
        {
            gameaudio_play_cross_linked_sound(w_info->no_ammo_sound, hp->get_handle(), nullptr, nullptr);
            set_trigger_pulled(true);
        }
        charge_up = false;
    }
    if (reload_delay_timer.valid())
    {
        charge_up = false;
    }
    if (human_in_vehicle(hp) != nullptr)
    {
        charge_up = false;
        grinder_speed_percent = 0.0f;
    }
    if (charge_up && Air_bomb_weapon && !Multiplayer)
    {
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            air_bomb_destroy_target(pp->aim_target, 0);
        }
        return false;
    }
    if (charge_up)
    {
        float rate = Grinder_increase_rate_hold;
        if (!Multiplayer && upgrade_has_fast_grinder())
        {
            rate = Grinder_increase_rate_hold_upgrade;
        }
        float speed = Frametime * rate + grinder_speed_percent;
        if (speed - 0.0f < 0.0f)
        {
            speed = 0.0f;
        }
        if (speed - 1.0f >= 0.0f)
        {
            speed = 1.0f;
        }
        grinder_speed_percent = speed;
        human_set_combat_ready(hp, true, 0);
        hp->combat.combat_aim_timestamp.set(2000); // Magic number: aiming for 2 s
        grinder_heat_effect = Frametime * rate + grinder_heat_effect;
    }
    else
    {
        grinder_speed_percent = 0.0f;
        grinder_heat_effect = grinder_heat_effect - Grinder_cool_rate * Frametime;
    }
    float heat = grinder_heat_effect;
    if (heat - 0.0f < 0.0f)
    {
        heat = 0.0f;
    }
    if (heat - 1.0f >= 0.0f)
    {
        heat = 1.0f;
    }
    grinder_heat_effect = heat;
    if (w_info->secondary_camera_shake != nullptr && grinder_speed_percent > 0.0f)
    {
        camera_shake_play(w_info->secondary_camera_shake, (Grinder_high_vibration - Grinder_low_vibration) * grinder_speed_percent + Grinder_low_vibration, false);
    }
    const bool started = previous_speed == 0.0f && grinder_speed_percent > 0.0f;
    const bool stopped = previous_speed > 0.0f && grinder_speed_percent == 0.0f;
    if (grinder_speed_percent == 1.0f)
    {
        // Full charge: the blade fires; the charge effect gives way to the charged one.
        weapon_persona = 0;
        fired = true;
        grinder_speed_percent = 0.0f;
        object_effect* effect = World.object_from_handle<object_effect>(special_effect_handle);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
            special_effect_handle = 0xFFFFFFFF;
        }
        vector muzzle_pos;
        matrix muzzle_orient;
        get_muzzle_transform(muzzle_pos, &muzzle_orient);
        if (World.object_from_handle<object_effect>(secondary_special_effect_handle) == nullptr)
        {
            secondary_special_effect_handle = effects_play(w_info->secondary_special_effect, muzzle_pos, muzzle_orient, false, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            World.object_from_handle<object_effect>(secondary_special_effect_handle); // result unused in rfg.exe
        }
        if (Multiplayer)
        {
            multi_game_player_set_mp_flag(hp, PMPF_CHARGE_WEAPON, 2);
            hp->mp_flags.set_charge_weapon(2);
            process_grinder_effects(false);
            return true;
        }
    }
    else if (started)
    {
        if (Multiplayer)
        {
            multi_game_player_set_mp_flag(hp, PMPF_CHARGE_WEAPON, 1);
            hp->mp_flags.set_charge_weapon(1);
        }
        process_grinder_effects(true);
        return false;
    }
    else if (!stopped)
    {
        return false;
    }
    else if (Multiplayer)
    {
        multi_game_player_set_mp_flag(hp, PMPF_CHARGE_WEAPON, 2);
        hp->mp_flags.set_charge_weapon(2);
    }
    process_grinder_effects(false);
    return fired;
}

// 0x00BDB6C0 (:203): the special effects stop emitting.
void weapon::stop_all_grinder_effects()
{
    if (special_effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = (object_effect*)World.object_from_handle(special_effect_handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
            special_effect_handle = 0xFFFFFFFF;
        }
    }
    if (secondary_special_effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = (object_effect*)World.object_from_handle(secondary_special_effect_handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
            secondary_special_effect_handle = 0xFFFFFFFF;
        }
    }
}

// 0x00BE0B80 (:221)
// The charge (charge_up) or the charged/released state of the owner's current grinder: its sound (the upgrade sound
// with the fast grinder) and its looping muzzle effect, stopping the other state's. A remote copy keeps a state whose
// sound still plays.
void weapon::process_grinder_effects(bool charge_up)
{
    if (get_owner_handle() == 0xFFFFFFFF)
    {
        return;
    }
    human* hp = (human*)World.object_from_handle(get_owner_handle());
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || human_weapons_get_weapon(hp) != this)
    {
        return;
    }
    const bool local = object_is_local_player(hp);
    vector muzzle_pos;
    matrix muzzle_orient;
    unsigned int effect_handle;
    if (charge_up)
    {
        if (!local && audiolib_play_get_state(special_sound_id, nullptr) == AUDIOLIB_ERR_NONE && audiolib_is_playing(special_sound_id, true))
        {
            return;
        }
        if (w_info->secondary_sound != -1 && audiolib_play_get_state(secondary_sound_id, nullptr) == AUDIOLIB_ERR_NONE && audiolib_is_playing(secondary_sound_id, true))
        {
            audiolib_play_stop(secondary_sound_id);
            secondary_sound_id = -1;
        }
        const int cue = upgrade_has_fast_grinder() && w_info->upgrade_sound != -1 ? w_info->upgrade_sound : w_info->special_sound;
        special_sound_id = gameaudio_play_cross_linked_sound(cue, hp->get_handle(), nullptr, nullptr);
        if (secondary_special_effect_handle != 0xFFFFFFFF)
        {
            object_effect* effect = (object_effect*)World.object_from_handle(secondary_special_effect_handle);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
                secondary_special_effect_handle = 0xFFFFFFFF;
            }
        }
        get_muzzle_transform(muzzle_pos, &muzzle_orient);
        if (special_effect_handle != 0xFFFFFFFF)
        {
            object* effect = World.object_from_handle(special_effect_handle);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                return;
            }
        }
        effect_handle = effects_play(w_info->special_effect, muzzle_pos, muzzle_orient, false, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        special_effect_handle = effect_handle;
    }
    else
    {
        if (!local && audiolib_play_get_state(secondary_sound_id, nullptr) == AUDIOLIB_ERR_NONE && audiolib_is_playing(secondary_sound_id, true))
        {
            return;
        }
        if (w_info->special_sound != -1 && audiolib_play_get_state(special_sound_id, nullptr) == AUDIOLIB_ERR_NONE && audiolib_is_playing(special_sound_id, true))
        {
            audiolib_play_fade_out(special_sound_id, 150); // Magic number: a 150 ms fade
        }
        secondary_sound_id = gameaudio_play_cross_linked_sound(w_info->secondary_sound, hp->get_handle(), nullptr, nullptr);
        if (special_effect_handle != 0xFFFFFFFF)
        {
            object_effect* effect = (object_effect*)World.object_from_handle(special_effect_handle);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
                special_effect_handle = 0xFFFFFFFF;
            }
        }
        get_muzzle_transform(muzzle_pos, &muzzle_orient);
        if (secondary_special_effect_handle != 0xFFFFFFFF)
        {
            object* effect = World.object_from_handle(secondary_special_effect_handle);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                return;
            }
        }
        effect_handle = effects_play(w_info->secondary_special_effect, muzzle_pos, muzzle_orient, false, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        secondary_special_effect_handle = effect_handle;
    }
    if (effect_handle != 0xFFFFFFFF)
    {
        World.object_from_handle(effect_handle); // result unused in rfg.exe
    }
}

// 0x00BDB730 (:304)
// The charge effect's (else the charged effect's) mesh instances get the heat in x of their instance parameters
// (from weapon::update_render, so the meshes glow while the grinder is hot). Nothing while effects are stand-ins.
void weapon::grinder_update_render()
{
    object_effect* effect = nullptr;
    if (special_effect_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(special_effect_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_EFFECT)
        {
            effect = (object_effect*)objp;
        }
    }
    object_effect* secondary = nullptr;
    if (secondary_special_effect_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(secondary_special_effect_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_EFFECT)
        {
            secondary = (object_effect*)objp;
        }
    }
    if (effect == nullptr)
    {
        effect = secondary;
    }
    if (effect == nullptr || !effect->rdd.is_loaded() || effect->rdd.get_data()->vfx_effect_p == nullptr)
    {
        return;
    }
    // Original: only x is set; y, z and w are an uninitialized local in rfg.exe (y holds a stale copy of the instance
    // count). They are 0 here.
    const vector4 instance_params(grinder_heat_effect, 0.0f, 0.0f, 0.0f);
    const linked_list_p<rl_renderable_instance>& instances = effect->rdd.get_data()->vfx_effect_p->m_renderable_mesh_insts;
    rl_renderable_instance* first = instances.first();
    for (rl_renderable_instance* ri = first; ri != nullptr;)
    {
        ri->set_instance_params(instance_params);
        ri = ri->next;
        if (ri == first)
        {
            break;
        }
    }
}
