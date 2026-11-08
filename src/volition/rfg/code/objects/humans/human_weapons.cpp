// Original: volition/rfg/code/objects/humans/human_weapons.cpp
//
// Partial (player_minimal.md P5, multiplayer.md MP8): the equipped weapon queries, the weapon state and equip
// animations, the melee move of an attack, the per-frame weapon passes (human_weapons_process_weapons_pre/_post,
// human_weapons_process_human), the firearm trigger, firing and reloading, thrown weapons and dropping weapons and
// shields. Compiled in rfg_objects_unity.cpp.
// The external interface is declared in human_weapons.h.

#include "volition/rfg/code/objects/humans/human_weapons.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/weapons/weapons.h"

// ---- file-static globals
// const float COVER_NODE_MAX_NAVMESH_ATTACH_DIST (0x01311864) is used here too; placed in human_spinebend.cpp.
// const float min_correction_diff (0x01315520) is used here too; placed in human.cpp.
// inventory_item* Inv_item_free_list (0x0302366C) is used here too; placed in human.cpp.
// player* Player (0x03023874) is used here too; placed in player.cpp.
// int Num_inv_items (0x0302F0E8) is used here too; placed in human.cpp.

static float Spread_prone_multiplier = 0.6f; // 0x016AFD4C
static float Spread_crouch_multiplier = 0.8f; // 0x016AFD50

// Firearm trigger helpers (LTCG register arguments), defined below.
static bool human_weapons_firearm_release_trigger_common(weapon* wp, bool hard_stop_effects);
static void human_weapons_firearm_process_common(human* hp, weapon* wp);

// Thrown weapons and firing (multiplayer.md MP8), defined at the end. Static in rfg.exe.
static void process_thrown_weapon(human* hp); // 0x00ACA410 (:276)
static void human_weapons_firearm_play_single_fire_anim(human* hp, weapon* wp); // 0x00ABE230 (:2637)
static bool human_weapons_firearm_pull_trigger_common(human* hp, weapon* wp); // 0x00ACC4B0 (:2729, LTCG: weapon in EAX, hp in ECX)

// 0x00A83670 (:639)
// The weapon's spread for the human: zero while a player is scoped, the fine aim, normal or npc spread, widened by
// moving (per 5.75 m/s), jumping and falling, crouching or lying, and for an npc by its target's speed (and the
// player's camera shake); the minimum never passes the maximum.
void human_weapons_get_spread(float& min_spread, float& max_spread, const human* hp, const weapon_info* w_info)
{
    const player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)hp : nullptr;
    const npc* np = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC ? (const npc*)hp : nullptr;
    if (pp != nullptr && pp->zoom_state == PZS_SCOPE)
    {
        max_spread = 0.0f;
        min_spread = 0.0f;
        return;
    }
    if (human_combat_get_manned_turret(hp) != nullptr)
    {
        if (pp == nullptr)
        {
            min_spread = w_info->npc_min_spread;
            max_spread = w_info->npc_max_spread;
        }
        else if (pp->zoom_state != PZS_FINE_AIM)
        {
            min_spread = w_info->min_spread;
            max_spread = w_info->max_spread;
        }
        else
        {
            min_spread = w_info->fine_aim_min_spread;
            max_spread = w_info->fine_aim_max_spread;
        }
        return;
    }
    float move_spread;
    switch (hp->movement_mode)
    {
    case HMM_DIRECT:
    case HMM_PATHFINDING:
    case HMM_PATHFINDING_TO_VEHICLE:
    case HMM_FP_MOVE:
        // Magic number: 5.75 m/s, the speed of the full running spread.
        move_spread = (float)((double)hp->actual_velocity_mag / 5.75 * (double)w_info->spread_multiplier_run);
        break;
    case HMM_JUMP:
    case HMM_FALL:
        move_spread = (float)((double)w_info->spread_multiplier_run * 2.0);
        break;
    default:
        move_spread = 0.0f;
        break;
    }
    float scale = 1.0f;
    if (np != nullptr)
    {
        float target_scale = 1.0f;
        const object* target = np->get_ai()->perception.get_attention_target();
        if (target != nullptr && target->get_obj_type() == OT_HUMAN)
        {
            // Magic number: 8 m/s of target speed doubles the spread. The squared length is rounded to float before
            // the double square root.
            const vector& v = ((const human*)target)->actual_velocity;
            const float length_sq = (float)((double)v.x * (double)v.x + (double)v.y * (double)v.y + (double)v.z * (double)v.z);
            float speed = (float)((double)(float)sqrt((double)length_sq) * 0.125);
            if ((float)((double)speed - 0.0) < 0.0f)
            {
                speed = 0.0f;
            }
            if ((float)((double)speed - 1.0) >= 0.0f)
            {
                speed = 1.0f;
            }
            target_scale = (float)((double)speed + 1.0);
        }
        else
        {
            target = nullptr;
        }
        scale = target_scale;
        if (target == Player)
        {
            // Magic number: a full camera shake makes the spread six times wider.
            scale = (float)(((double)camera_shake_get_intensity() * 5.0 + 1.0) * (double)target_scale);
        }
    }
    const human_stances stance = hp->cur_stance;
    if (stance == HST_CROUCH)
    {
        scale = Spread_crouch_multiplier * scale;
    }
    else if (stance == HST_PRONE)
    {
        scale = Spread_prone_multiplier * scale;
    }
    if (pp == nullptr)
    {
        min_spread = w_info->npc_min_spread;
        max_spread = w_info->npc_max_spread;
    }
    else
    {
        const human_move_submodes submode = hp->movement_submode;
        if (submode == HMSM_COVER_RIGHT_BLINDFIRE || submode == HMSM_COVER_LEFT_BLINDFIRE || submode == HMSM_COVER_RIGHT_BLINDFIRE2 || submode == HMSM_COVER_LEFT_BLINDFIRE2)
        {
            min_spread = w_info->min_spread;
            max_spread = w_info->max_spread;
        }
        else if (pp->zoom_state == PZS_FINE_AIM)
        {
            min_spread = w_info->fine_aim_min_spread;
            max_spread = w_info->fine_aim_max_spread;
        }
        else if (stance == HST_CROUCH || (unsigned int)(submode - HMSM_COVER_RIGHT_LEAN) < 4)
        {
            // Crouching or leaning out of cover: halfway between the fine aim and the normal spread.
            min_spread = (float)(((double)w_info->fine_aim_min_spread + (double)w_info->min_spread) * 0.5);
            max_spread = (float)(((double)w_info->fine_aim_max_spread + (double)w_info->max_spread) * 0.5);
        }
        else
        {
            min_spread = w_info->min_spread;
            max_spread = w_info->max_spread;
        }
    }
    // Magic number: 0.017453, one degree in radians (the move spread is in degrees).
    float spread = min_spread;
    const float widened = (float)((double)spread * (double)scale + (double)move_spread * (double)0.017453f);
    if ((float)((double)widened - (double)spread) >= 0.0f)
    {
        spread = widened;
    }
    if ((float)((double)spread - (double)max_spread) >= 0.0f)
    {
        spread = max_spread;
    }
    min_spread = spread;
}

// 0x00A5A4F0 (:755): the equipped weapon, or the manned turret's primary weapon.
weapon* human_weapons_get_weapon(const human* hp)
{
    // human::get_equipped_weapon, inlined.
    return hp->get_equipped_weapon();
}

// 0x00A5A540 (:770)
const weapon_info* human_weapons_get_weapon_info(const human* hp)
{
    const weapon* wp = hp->get_equipped_weapon();
    return wp != nullptr ? wp->w_info : nullptr;
}

// 0x00ACB320 (:785)
// A human's weapon frame: a player's empty firearm reloads after the weapon's out of ammo delay (a turret gunner's
// secondary weapon too), and a thrown weapon in hand is processed; a thrown charge weapon whose charge is gone
// unequips.
void human_weapons_process_human(human* hp)
{
    weapon* wp = human_weapons_get_weapon(hp);
    if (wp == nullptr)
    {
        if (hp->combat.thrown_weapon != 0xFFFFFFFF)
        {
            process_thrown_weapon(hp);
        }
        return;
    }
    player* pp = nullptr;
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        pp = (player*)hp;
        if (wp->w_info->is_firearm() && !wp->is_loaded())
        {
            if (!hp->reload_timer.valid())
            {
                hp->reload_timer.set(wp->w_info->out_of_ammo_reload_delay);
            }
        }
        else
        {
            hp->reload_timer.invalidate();
        }
    }
    else
    {
        hp->reload_timer.invalidate();
    }
    if (hp->reload_timer.valid() && hp->reload_timer.elapsed())
    {
        // In a vehicle only a human with a weapon of its own plays the reload.
        const bool play_anim = human_in_vehicle(hp) == nullptr || human_weapons_get_weapon_info(hp) != nullptr;
        human_weapons_firearm_reload(hp, play_anim, nullptr);
    }
    if (pp != nullptr && wp->turret_handle != 0xFFFFFFFF)
    {
        turret* tp = (turret*)World.object_from_handle(wp->turret_handle);
        if (tp != nullptr && tp->get_obj_type() == OT_TURRET && tp->get_secondary_weapon_inst() != nullptr)
        {
            weapon* secondary = tp->get_secondary_weapon_inst();
            if (secondary->w_info->is_firearm() && !secondary->is_loaded())
            {
                human_weapons_firearm_reload(hp, false, secondary);
            }
        }
    }
    if (hp->combat.thrown_weapon != 0xFFFFFFFF)
    {
        process_thrown_weapon(hp);
        return;
    }
    if (wp->w_info->weapon_class != WEAPON_CLASS_THROWN_CHARGE || hp->offhand_projectile_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* charge = World.object_from_handle(hp->offhand_projectile_handle);
    if (charge == nullptr || charge->get_obj_type() != OT_ITEM || charge->obj_flags._dont_use_me || !can_fire_charge(hp->get_handle(), false) || hp->cur_state == HS_AMMO_BOX)
    {
        return;
    }
    hp->set_equipped_inv_item(nullptr);
}

// 0x00A5A5A0 (:840)
bool human_weapons_is_weapon_raised(human* hp)
{
    return human_weapons_get_weapon_info(hp) != nullptr;
}

// 0x00AB8040 (:974): the weapon goes away at once (no unequip animation); it is remembered as the last weapon
void human_weapons_immediately_holster_weapon(human* hp)
{
    inventory_item* equipped = hp->equipped_inv_item;
    if (equipped == nullptr)
    {
        if (hp->desired_equipped_inv_item != nullptr)
        {
            hp->last_equipped_weapon = hp->desired_equipped_inv_item;
            hp->desired_equipped_inv_item = nullptr;
        }
        return;
    }
    hp->second_last_equipped_weapon = hp->last_equipped_weapon;
    hp->last_equipped_weapon = equipped;
    if (hp->desired_equipped_inv_item != nullptr && hp->second_last_equipped_weapon == equipped)
    {
        hp->last_equipped_weapon = hp->desired_equipped_inv_item;
    }
    hp->set_equipped_inv_item(nullptr);
    hp->desired_equipped_inv_item = nullptr;
}

// 0x00A5A610 (:1006)
bool human_weapons_is_melee_weapon_equipped(const human* hp)
{
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    return w_info != nullptr && w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE;
}

// 0x00A5A640 (:1012)
bool human_weapons_is_firearm_equipped(const human* hp)
{
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    return w_info != nullptr && w_info->is_firearm();
}

// 0x00A5A680 (:1018)
bool human_weapons_is_penetrating_weapon_equipped(const human* hp)
{
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    return w_info != nullptr && w_info->flags.penetrating_bullets;
}

// 0x00A5A6A0 (:1024): a firearm with both grip tags.
bool human_weapons_is_two_handed_firearm_equipped(const human* hp)
{
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    if (w_info == nullptr || !w_info->is_firearm() || hp->equipped_inv_item == nullptr)
    {
        return false;
    }
    weapon* wp = nullptr;
    const unsigned int handle = hp->equipped_inv_item->weapon_handle;
    if (handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            wp = (weapon*)objp;
        }
    }
    // Original: wp is dereferenced unchecked (a manned turret's weapon info can come without an equipped weapon).
    return wp->get_grip_tag_index() >= 0 && wp->get_secondary_grip_tag_index() >= 0;
}

// 0x00A5A750 (:1117)
bool human_weapons_is_reloading(const human* hp, bool secondary_weapon)
{
    weapon* wp = human_weapons_get_weapon(hp);
    // Original: wp is dereferenced unchecked when the secondary weapon is asked for.
    if (secondary_weapon && wp->turret_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(wp->turret_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_TURRET)
        {
            wp = ((turret*)objp)->get_secondary_weapon_inst();
        }
    }
    return wp != nullptr && wp->is_reloading();
}

// 0x00A5A840 (:1132)
// Stops a reload: the reload animation's prop triggers run to the end, and the ammo is given if the animation got that
// far (a thermobaric launcher's half-loaded rocket goes).
void human_weapons_cancel_reload(human* hp)
{
    if (!human_weapons_is_reloading(hp, false))
    {
        return;
    }
    weapon* wp = human_weapons_get_weapon(hp);
    if (wp != nullptr)
    {
        wp->cancel_reload();
    }
    if (!hp->char_inst->is_playing_action(ANIM_ALIAS_RELOAD))
    {
        return;
    }
    vanim_fast_forward_prop_triggers(hp->char_inst->vp, ANIM_ALIAS_RELOAD, hp);
    if (wp != nullptr)
    {
        if (vanim_has_trigger_elapsed(hp->char_inst->vp, Trigger_reload_give_ammo, ANIM_ALIAS_RELOAD, -1))
        {
            wp->do_reload();
        }
        else if (wp->w_info->weapon_anim_group == ANIMATION_GROUP_THERMOBARIC_ROCKET_LAUNCHER)
        {
            object* first = wp->child_ptr;
            for (object* child = first; child != nullptr;)
            {
                // The loaded rocket: the clip prop attached to the launcher (weapon::add_ammo).
                if (child->get_obj_type() == OT_ITEM && ((item*)child)->m_prop_type == PROP_TYPE_CLIP)
                {
                    World.destroy_object(child, false);
                    break;
                }
                child = child->child_next;
                if (child == first)
                {
                    break;
                }
            }
        }
    }
    hp->char_inst->stop_action(ANIM_ALIAS_RELOAD, 0, false);
}

// 0x00A0E780 (:1753)
// The state animation for a weapon's animation flags: prone, crouching, walking, running, driving and riding, each in
// its aim, ready and down ready variants and directions. Pistols cannot drive-by; launchers cannot ride either.
void human_weapons_get_state_animation(weapon_class_type weapon_class, weapon_anim_flags animation_flags, animation_state& anim_state, vanim_aliases& anim_alias,
                                       float& blend_time)
{
    anim_state = ANIMATION_STATE_NONE;
    if (weapon_class == WEAPON_CLASS_LAUNCHER)
    {
        animation_flags.ride = 0;
        animation_flags.ride_left = 0;
        animation_flags.drive = 0;
    }
    else if (weapon_class == WEAPON_CLASS_BULLET)
    {
        animation_flags.drive = 0;
    }
    const weapon_anim_flags& f = animation_flags;
    if (f.prone)
    {
        if (!f.walk && !f.run)
        {
            anim_state = ANIMATION_STATE_PRONE;
            return;
        }
        anim_state = ANIMATION_STATE_PRONE_CRAWL;
        anim_alias = ANIM_ALIAS_STATE_WALK;
        return;
    }
    if (f.crouch)
    {
        blend_time = 0.3f;
        if (!f.walk && !f.run)
        {
            if (f.fine_aim)
            {
                anim_state = ANIMATION_STATE_FINE_AIM_CROUCH;
            }
            else if (f.aim)
            {
                anim_state = ANIMATION_STATE_CROUCH_AIM;
            }
            else if (f.down_ready)
            {
                anim_state = ANIMATION_STATE_CROUCH_DOWNREADY;
            }
            else if (f.ready)
            {
                anim_state = ANIMATION_STATE_CROUCH_READY;
            }
            else
            {
                anim_state = f.no_ammo ? ANIMATION_STATE_CROUCH_NO_AMMO : ANIMATION_STATE_CROUCH;
            }
            return;
        }
        anim_alias = ANIM_ALIAS_STATE_WALK;
        if (f.fine_aim)
        {
            anim_state = f.left ? ANIMATION_STATE_FINE_AIM_CROUCH_WALK_LEFT
                         : f.right ? ANIMATION_STATE_FINE_AIM_CROUCH_WALK_RIGHT
                         : (f.back || f.backpedal) ? ANIMATION_STATE_FINE_AIM_CROUCH_WALK_BACK
                                                   : ANIMATION_STATE_FINE_AIM_CROUCH_WALK;
            return;
        }
        if (f.aim)
        {
            anim_state = f.left ? ANIMATION_STATE_CROUCH_WALK_AIM_LEFT
                         : f.right ? ANIMATION_STATE_CROUCH_WALK_AIM_RIGHT
                         : (f.back || f.backpedal) ? ANIMATION_STATE_CROUCH_WALK_AIM_BACK
                                                   : ANIMATION_STATE_CROUCH_WALK_AIM_FWD;
            return;
        }
        if (f.down_ready)
        {
            if (!f.walk)
            {
                anim_state = ANIMATION_STATE_CROUCH_RUN_DOWNREADY;
                anim_alias = ANIM_ALIAS_STATE_RUN;
                return;
            }
            anim_state = ANIMATION_STATE_CROUCH_WALK_DOWNREADY;
            return;
        }
        if (f.ready)
        {
            anim_state = f.left ? ANIMATION_STATE_CROUCH_WALK_READY_LEFT
                         : f.right ? ANIMATION_STATE_CROUCH_WALK_READY_RIGHT
                         : (f.back || f.backpedal) ? ANIMATION_STATE_CROUCH_WALK_READY_BACK
                                                   : ANIMATION_STATE_CROUCH_WALK_READY_FWD;
            return;
        }
        if (f.run)
        {
            anim_state = f.no_ammo ? ANIMATION_STATE_CROUCH_WALK_NO_AMMO : ANIMATION_STATE_CROUCH_RUN;
            anim_alias = ANIM_ALIAS_STATE_RUN;
            return;
        }
        anim_state = f.left ? ANIMATION_STATE_CROUCH_WALK_LEFT
                     : f.right ? ANIMATION_STATE_CROUCH_WALK_RIGHT
                     : (f.back || f.backpedal) ? ANIMATION_STATE_CROUCH_WALK_BACK
                                               : ANIMATION_STATE_CROUCH_WALK;
        return;
    }
    if (f.walk)
    {
        blend_time = 0.25f;
        anim_alias = ANIM_ALIAS_STATE_WALK;
        if (f.fine_aim)
        {
            anim_state = f.left ? ANIMATION_STATE_FINE_AIM_WALK_LEFT
                         : f.right ? ANIMATION_STATE_FINE_AIM_WALK_RIGHT
                         : (f.back || f.backpedal) ? ANIMATION_STATE_FINE_AIM_WALK_BACK
                                                   : ANIMATION_STATE_FINE_AIM_WALK;
            return;
        }
        if (f.aim)
        {
            anim_state = f.left ? ANIMATION_STATE_RUN_AIM_LEFT
                         : f.right ? ANIMATION_STATE_RUN_AIM_RIGHT
                         : (f.back || f.backpedal) ? ANIMATION_STATE_RUN_AIM_BACK
                                                   : ANIMATION_STATE_RUN_AIM;
            return;
        }
        if (f.down_ready)
        {
            anim_state = ANIMATION_STATE_WALK_DOWNREADY;
            return;
        }
        if (f.ready)
        {
            anim_state = f.left ? ANIMATION_STATE_WALK_READY_LEFT
                         : f.right ? ANIMATION_STATE_WALK_READY_RIGHT
                         : f.back ? ANIMATION_STATE_WALK_READY_BACK
                         : f.backpedal ? ANIMATION_STATE_WALK_READY_BACKPEDAL
                                       : ANIMATION_STATE_WALK_READY;
            return;
        }
        // Original: walking without ammo plays the running no ammo state.
        anim_state = f.left ? ANIMATION_STATE_WALK_LEFT
                     : f.right ? ANIMATION_STATE_WALK_RIGHT
                     : (f.back || f.backpedal) ? ANIMATION_STATE_WALK_BACK
                     : f.no_ammo ? ANIMATION_STATE_RUN_NO_AMMO
                                 : ANIMATION_STATE_WALK;
        return;
    }
    if (f.run)
    {
        blend_time = 0.25f;
        anim_alias = ANIM_ALIAS_STATE_RUN;
        if (f.fine_aim)
        {
            anim_state = f.left ? ANIMATION_STATE_FINE_AIM_WALK_LEFT
                         : f.right ? ANIMATION_STATE_FINE_AIM_WALK_RIGHT
                         : (f.back || f.backpedal) ? ANIMATION_STATE_FINE_AIM_WALK_BACK
                                                   : ANIMATION_STATE_FINE_AIM_WALK;
            return;
        }
        if (f.aim)
        {
            anim_state = f.left ? ANIMATION_STATE_RUN_AIM_LEFT
                         : f.right ? ANIMATION_STATE_RUN_AIM_RIGHT
                         : (f.back || f.backpedal) ? ANIMATION_STATE_RUN_AIM_BACK
                                                   : ANIMATION_STATE_RUN_AIM;
            return;
        }
        if (f.down_ready)
        {
            anim_state = ANIMATION_STATE_RUN_DOWNREADY;
            return;
        }
        if (f.ready)
        {
            anim_state = f.left ? ANIMATION_STATE_RUN_READY_LEFT
                         : f.right ? ANIMATION_STATE_RUN_READY_RIGHT
                         : f.backpedal ? ANIMATION_STATE_RUN_READY_BACKPEDAL
                         : f.back ? ANIMATION_STATE_RUN_READY_BACK
                                  : ANIMATION_STATE_RUN_READY;
            return;
        }
        anim_state = f.left ? ANIMATION_STATE_RUN_LEFT
                     : f.right ? ANIMATION_STATE_RUN_RIGHT
                     : f.back ? ANIMATION_STATE_RUN_BACK
                     : f.backpedal ? ANIMATION_STATE_RUN_BACKPEDAL
                     : f.no_ammo ? ANIMATION_STATE_RUN_NO_AMMO
                                 : ANIMATION_STATE_RUN;
        return;
    }
    if (f.drive)
    {
        if (!f.ready)
        {
            anim_state = ANIMATION_STATE_DRIVING_FORWARD;
        }
        else if (f.left)
        {
            anim_state = ANIMATION_STATE_DRIVE_READY_LEFT;
        }
        else if (f.right)
        {
            anim_state = ANIMATION_STATE_DRIVE_READY_RIGHT;
        }
        else
        {
            // Original: looking back picks DRIVE_READY + 1, the left one.
            anim_state = (animation_state)(ANIMATION_STATE_DRIVE_READY + f.back);
        }
        return;
    }
    if (f.ride)
    {
        if (!f.ready)
        {
            anim_state = ANIMATION_STATE_RIDING_READY;
        }
        else if (Spinebend_passengers)
        {
            anim_state = ANIMATION_STATE_RIDE_READY;
        }
        else
        {
            anim_state = f.left ? ANIMATION_STATE_RIDE_READY_LEFT : f.right ? ANIMATION_STATE_RIDE_READY_RIGHT : f.back ? ANIMATION_STATE_RIDE_READY_BACK : ANIMATION_STATE_RIDE_READY;
        }
        return;
    }
    if (f.ride_left)
    {
        if (!f.ready)
        {
            anim_state = ANIMATION_STATE_RIDING_LEFT_READY;
        }
        else if (Spinebend_passengers)
        {
            anim_state = ANIMATION_STATE_RIDE_LEFT_READY;
        }
        else
        {
            anim_state = f.left    ? ANIMATION_STATE_RIDE_LEFT_READY_LEFT
                         : f.right ? ANIMATION_STATE_RIDE_LEFT_READY_RIGHT
                         : f.back  ? ANIMATION_STATE_RIDE_LEFT_READY_BACK
                                   : ANIMATION_STATE_RIDE_LEFT_READY;
        }
        return;
    }
    if (f.switching)
    {
        anim_state = ANIMATION_STATE_STAND_READY;
    }
    else if (f.fine_aim)
    {
        anim_state = ANIMATION_STATE_FINE_AIM;
    }
    else if (f.aim)
    {
        anim_state = ANIMATION_STATE_STAND_AIM;
    }
    else if (f.down_ready)
    {
        anim_state = ANIMATION_STATE_STAND_DOWNREADY;
    }
    else if (f.ready)
    {
        anim_state = f.left ? ANIMATION_STATE_STAND_READY_LEFT : f.right ? ANIMATION_STATE_STAND_READY_RIGHT : ANIMATION_STATE_STAND_READY;
    }
    else
    {
        anim_state = f.no_ammo ? ANIMATION_STATE_STAND_NO_AMMO : ANIMATION_STATE_STAND;
    }
}

// 0x00A0EF60 (:2471): the weapon's standing or crouching move for the attack (anything else is the primary).
human_melee_combat_move_id human_weapons_melee_get_combat_move(const human* hp, const weapon_info* wip, human_melee_attack_types attack_type)
{
    if (hp->cur_stance != HST_CROUCH)
    {
        switch (attack_type)
        {
        case MELEE_ATTACK_SECONDARY:
            return wip->standing_secondary_melee_attack;
        case MELEE_ATTACK_TERTIARY:
            return wip->standing_tertiary_melee_attack;
        default:
            return wip->standing_primary_melee_attack;
        }
    }
    switch (attack_type)
    {
    case MELEE_ATTACK_SECONDARY:
        return wip->crouching_secondary_melee_attack;
    case MELEE_ATTACK_TERTIARY:
        return wip->crouching_tertiary_melee_attack;
    default:
        return wip->crouching_primary_melee_attack;
    }
}

// 0x00A0EFD0 (:2501): the equip action (the moving one while moving), or the cover one for the cover side.
animation_action human_weapons_get_equip_animation(weapon_class_type, human* hp)
{
    if (hp->cur_state != HS_COVER)
    {
        // Magic number: the moving equip action follows two after ANIMATION_ACTION_EQUIP.
        return (animation_action)(ANIMATION_ACTION_EQUIP + (hp->cur_movement_state != HMS_IDLE ? 2 : 0));
    }
    const bool crouched = hp->cur_stance == HST_CROUCH || hp->hflags.crouch_cover;
    switch (hp->movement_submode)
    {
    case HMSM_COVER_RIGHT:
    case HMSM_COVER_RIGHT_EDGE:
    case HMSM_COVER_RIGHT_LEAN:
    case HMSM_COVER_RIGHT_LEANUP:
        return crouched ? ANIMATION_ACTION_COVER_CROUCH_EQUIP_RIGHT : ANIMATION_ACTION_COVER_STAND_EQUIP_RIGHT;
    case HMSM_COVER_LEFT:
    case HMSM_COVER_LEFT_EDGE:
    case HMSM_COVER_LEFT_LEAN:
    case HMSM_COVER_LEFT_LEANUP:
        if (crouched)
        {
            return ANIMATION_ACTION_COVER_CROUCH_EQUIP_LEFT;
        }
        return ANIMATION_ACTION_COVER_STAND_EQUIP_LEFT;
    default:
        return ANIMATION_ACTION_COVER_STAND_EQUIP_LEFT;
    }
}

// 0x00A0F080 (:2885, LTCG: wp in ESI, hard_stop_effects on the stack; hp is not passed)
// Releases a firearm's trigger (the grinder winds down, the looping effects of an automatic stop). Returns whether the
// trigger was down.
static bool human_weapons_firearm_release_trigger_common(weapon* wp, bool hard_stop_effects)
{
    if (wp == nullptr || !wp->w_info->is_firearm())
    {
        return false;
    }
    if (wp->w_info->weapon_class == WEAPON_CLASS_GRINDER)
    {
        wp->charge_grinder(false);
    }
    if (!wp->is_trigger_pulled())
    {
        return false;
    }
    if (wp->w_info->flags.looping_effects && wp->w_info->trigger_type == WEAPON_TRIGGER_TYPE_AUTOMATIC && wp->get_num_rounds_fired() > 0)
    {
        wp->end_looping_effects(hard_stop_effects);
    }
    wp->set_trigger_pulled(false);
    return true;
}

// 0x00ABE330 (:2924, LTCG: hp in EAX, wp in EDI)
// A firearm with its trigger down fires while it is ready and loaded (an automatic keeps firing; a single shot only
// once per pull for a player); an empty or overheated one, or one the human no longer holds, releases the trigger.
static void human_weapons_firearm_process_common(human* hp, weapon* wp)
{
    if (!wp->is_trigger_pulled())
    {
        return;
    }
    const bool not_held = hp == nullptr || (wp->get_owner_handle() != hp->get_handle() && human_weapons_get_weapon(hp) != wp);
    if (!gameplay_can_player_attack(wp->w_info, DBT_BULLET) || not_held)
    {
        human_weapons_firearm_release_trigger_common(wp, true);
        return;
    }
    if (wp->w_info->trigger_type != WEAPON_TRIGGER_TYPE_AUTOMATIC && hp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return;
    }
    if (hp->hflags.playing_equip_anim || hp->hflags.playing_unequip_anim || hp->combat.cur_melee_combat_move != nullptr)
    {
        human_weapons_firearm_release_trigger_common(wp, true);
        return;
    }
    if ((!wp->is_loaded() && !wp->has_unlimited_magazine_ammo()) || wp->is_overheated())
    {
        human_weapons_firearm_release_trigger_common(wp, false);
        return;
    }
    if (Multiplayer && !wp->is_linked())
    {
        if (Framecount - 1 != hp->last_trigger_down_frame)
        {
            hp->mp_flags.firearm_trigger_down_id++;
        }
        hp->last_trigger_down_frame = Framecount;
    }
    player* pp = World.object_cast<player>(hp);
    if (!wp->ready_to_fire() || wp->do_one_shot_delay())
    {
        return;
    }
    if (pp != nullptr)
    {
        if (hp->cur_state == HS_COVER)
        {
            pp->pflags.cover_firing = 1;
        }
        else
        {
            // Magic numbers: 15 s in the combat ready pose and 3 s aiming after a shot.
            human_set_combat_ready(hp, true, 15000);
            hp->combat.combat_aim_timestamp.set(3000);
        }
    }
    if (wp->w_info->flags.looping_effects && wp->get_num_rounds_fired() == 0)
    {
        wp->start_looping_effects();
    }
    if (!wp->is_linked())
    {
        human_weapons_firearm_play_single_fire_anim(hp, wp);
    }
    // Original: a single shot weapon plays the fire animation twice.
    if (wp->w_info->trigger_type == WEAPON_TRIGGER_TYPE_SINGLE && !wp->is_linked())
    {
        human_weapons_firearm_play_single_fire_anim(hp, wp);
    }
    while (wp->ready_to_fire() && (wp->is_loaded() || wp->has_unlimited_magazine_ammo()))
    {
        wp->fire(ZERO_VECTOR);
    }
}

// 0x00A0F100 (:3097)
void human_weapons_unequip_weapon(human* hp, weapon* wp)
{
    if (hp == nullptr || wp == nullptr)
    {
        return;
    }
    if (wp->w_info->is_firearm())
    {
        human_weapons_firearm_release_trigger_common(wp, true);
    }
    wp->unequip();
}

// 0x00A5A950 (:3109): every weapon's frame start.
void human_weapons_process_weapons_pre()
{
    const int count = World.get_num_objects<weapon>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<weapon>(i)->do_frame_pre();
    }
}

// 0x00ACC9C0 (:3116)
// Every weapon's frame end: a linked firearm (a turret's second weapon) follows its owner's trigger, then the weapon
// frame, then the firing of firearms.
void human_weapons_process_weapons_post()
{
    const int count = World.get_num_objects<weapon>();
    for (int i = 0; i < count; i++)
    {
        weapon* wp = World.get_object<weapon>(i);
        const bool firearm = wp->w_info->is_firearm();
        if (firearm && wp->is_linked() && wp->get_owner_handle() != 0xFFFFFFFF)
        {
            human* owner = (human*)World.object_from_handle(wp->get_owner_handle());
            if (owner != nullptr && owner->get_obj_type() == OT_HUMAN)
            {
                weapon* held = human_weapons_get_weapon(owner);
                if (held != nullptr && held != wp)
                {
                    if (held->is_trigger_pulled())
                    {
                        human_weapons_firearm_pull_trigger_common(owner, wp);
                    }
                    else
                    {
                        human_weapons_firearm_release_trigger_common(wp, false);
                    }
                }
            }
        }
        wp->do_frame_post();
        if (firearm)
        {
            human* owner = nullptr;
            if (wp->get_owner_handle() != 0xFFFFFFFF)
            {
                object* objp = World.object_from_handle(wp->get_owner_handle());
                if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
                {
                    owner = (human*)objp;
                }
            }
            human_weapons_firearm_process_common(owner, wp);
        }
    }
    // A player's trigger release (for remote machines) catches up with the trigger pulls of earlier frames.
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->mp_flags.firearm_trigger_down_id != pp->mp_flags.firearm_trigger_up_id && pp->last_trigger_down_frame != Framecount)
        {
            pp->mp_flags.firearm_trigger_up_id = pp->mp_flags.firearm_trigger_down_id;
        }
    }
}

// 0x00A5A9C0 (:3230)
bool human_weapons_is_trigger_pulled(const human* hp)
{
    if (hp == nullptr)
    {
        return false;
    }
    const weapon* wp = human_weapons_get_weapon(hp);
    return wp != nullptr && wp->is_trigger_pulled(); // inlined
}

// 0x00A5AA60 (:3260): releases the held firearm's trigger (and a player's cover firing).
void human_weapons_firearm_release_trigger(human* hp)
{
    weapon* wp = human_weapons_get_weapon(hp);
    if (wp == nullptr || !wp->w_info->is_firearm())
    {
        return;
    }
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && hp->cur_state == HS_COVER)
    {
        ((player*)hp)->pflags.cover_firing = 0;
    }
    human_weapons_firearm_release_trigger_common(wp, false);
}

// ---- firing, reloading, thrown weapons and dropping weapons (multiplayer.md MP8)
static const float MIN_FIX_TURRET_DOT = 0.866025f; // 0x01311C80
// Throws (human_weapons_start_throw): a player's throw starts 12 degrees above the camera's facing.
static float Throw_default_strength_on_foot = 0.7f; // 0x01694430
static float Throw_default_place_strength_on_foot = 0.1f; // 0x01694434
static float Throw_inherit_velocity_scale = 0.8f; // 0x01694438
static float Throw_pitch_offset = 0.2094395f; // 0x01695420
// The release check of a player's throw (process_thrown_weapon).
static float velocity_multiplier = 0.5f; // 0x016AFE40
static float radius_multiplier = 1.0f; // 0x016AFE44
static float extra_distance_check = 1.5f; // 0x016AFE48
static int rc_test_frame = 8; // 0x016AFE4C
static bool Play_offscreen_fire_animations; // 0x0302F0A0

// The zoom of a player who reloads or drops a weapon goes off (inlined in both): the scope's transparency and HUD go
// with it.
static void human_weapons_player_zoom_off(human* hp)
{
    if (!object_is_player(hp))
    {
        return;
    }
    player* pp = (player*)hp;
    turret* tp;
    if (pp->zoom_state == PZS_OFF || ((tp = human_combat_get_manned_turret(pp)) != nullptr && tp->t_info->flags.force_fine_aim))
    {
        return;
    }
    const player_zoom_state zoom = pp->zoom_state;
    pp->previous_zoom_state = zoom;
    pp->zoom_state = PZS_OFF;
    if (pp != Player)
    {
        return;
    }
    pp->get_equipped_weapon(); // Original: the result is not used.
    if (pp->previous_zoom_state == PZS_SCOPE)
    {
        pp->set_opacity_plus_child_items(1.0f);
        pp->set_casts_transparent_shadows_plus_child_items(false);
        hud_ui_rail_driver_set_visible(false);
        hud_ui_sniper_set_visible(false);
    }
    camera_set_view_player(PZS_OFF, false);
}

// 0x00A33150 (:2704): the reload sound (3d at the weapon for others than the local player).
static void human_weapons_firearm_play_reload_sound(const human* hp, const weapon* wp)
{
    if (hp == nullptr || wp == nullptr)
    {
        return;
    }
    audiolib_params params;
    params.delay_ms = wp->w_info->reload_sound_delay;
    if (hp != Player)
    {
        params.pos = wp->pos;
        params.flags &= 1;
    }
    audiolib_cue_play(wp->w_info->reload_sound, &params, nullptr);
}

// 0x00A331D0 (:3086)
void human_weapons_firearm_start_reload(const human* hp, weapon* wp, int delay)
{
    human_weapons_firearm_play_reload_sound(hp, wp);
    wp->start_reload(delay);
}

// 0x00AB80B0 (:2690, LTCG: hp in EAX, wp in EDI): the dry fire click (3d along the look for others).
static void human_weapons_firearm_play_out_of_ammo_sound(const human* hp, const weapon* wp)
{
    if (hp == nullptr || wp == nullptr)
    {
        return;
    }
    if (hp == Player)
    {
        audiolib_cue_play(wp->w_info->no_ammo_sound, nullptr, nullptr);
        return;
    }
    vector look;
    human_get_look_at_vector(hp, look);
    audiolib_cue_play_3d(wp->w_info->no_ammo_sound, &hp->pos, &look, nullptr, nullptr);
}

// 0x00A0ECA0 (:2298): the reload action for the stance and movement (cover reloads by side); none in a vehicle.
animation_action human_weapons_get_reload_animation(weapon_class_type weapon_class, human* hp)
{
    const unsigned int vehicle_handle = hp->vehicle_handle;
    const bool crouched = hp->cur_stance == HST_CROUCH;
    bool walking = false;
    // Actions 0x3EA/0x3EB: the shuffle steps.
    if (hp->cur_movement_state == HMS_WALK || hp->char_inst->is_playing_action((animation_action)0x3EA) || hp->char_inst->is_playing_action((animation_action)0x3EB))
    {
        walking = true;
    }
    if (hp->cur_state == HS_COVER)
    {
        const bool crouch_cover = hp->cur_stance == HST_CROUCH || hp->hflags.crouch_cover;
        switch (hp->movement_submode)
        {
        case HMSM_COVER_RIGHT:
        case HMSM_COVER_RIGHT_EDGE:
        case HMSM_COVER_RIGHT_LEAN:
        case HMSM_COVER_RIGHT_LEANUP:
        case HMSM_COVER_RIGHT_BLINDFIRE:
        case HMSM_COVER_RIGHT_BLINDFIRE2:
            return crouch_cover ? ANIMATION_ACTION_COVER_CROUCH_RELOAD_RIGHT : ANIMATION_ACTION_COVER_STAND_RELOAD_RIGHT;
        case HMSM_COVER_LEFT:
        case HMSM_COVER_LEFT_EDGE:
        case HMSM_COVER_LEFT_LEAN:
        case HMSM_COVER_LEFT_LEANUP:
        case HMSM_COVER_LEFT_BLINDFIRE:
        case HMSM_COVER_LEFT_BLINDFIRE2:
            return crouch_cover ? ANIMATION_ACTION_COVER_CROUCH_RELOAD_LEFT : ANIMATION_ACTION_COVER_STAND_RELOAD_LEFT;
        default:
            return ANIMATION_ACTION_RELOAD;
        }
    }
    switch (weapon_class)
    {
    case WEAPON_CLASS_BULLET:
    case WEAPON_CLASS_LAUNCHER:
    case WEAPON_CLASS_GRINDER:
    case WEAPON_CLASS_ARC_WELDER:
        if (vehicle_handle != 0xFFFFFFFF)
        {
            return ANIMATION_ACTION_NONE;
        }
        // Each crouched action is 3 (2 for running) after the standing one.
        if (walking)
        {
            return (animation_action)((crouched ? 3 : 0) + ANIMATION_ACTION_WALK_RELOAD);
        }
        if (hp->cur_movement_state == HMS_RUN)
        {
            return (animation_action)((unsigned int)crouched * 2 + ANIMATION_ACTION_RUN_RELOAD);
        }
        return (animation_action)((crouched ? 3 : 0) + ANIMATION_ACTION_RELOAD);
    default:
        return ANIMATION_ACTION_RELOAD;
    }
}

// 0x00A9D970 (:3298)
// Starts a reload of the weapon (the held one by default): not while reloading, at an ammo box or action node, ragdolled
// or getting up, or in a melee move. The local player without reserve rounds unequips (npcs keep trying), others need
// reserve rounds. The zoom goes off and the trigger is released; the reload takes its animation's length (scaled by a
// player's backpack and the multiplayer reload speed), or the weapon's default reload delay without an animation.
bool human_weapons_firearm_reload(human* hp, bool play_animation, weapon* special_weapon)
{
    weapon* wp = special_weapon;
    if (wp == nullptr && (wp = human_weapons_get_weapon(hp)) == nullptr)
    {
        return false;
    }
    if (!wp->w_info->is_firearm() || wp->is_reloading() || hp->cur_state == HS_AMMO_BOX || hp->cur_state == HS_ACTION_NODE || human_is_ragdolled(hp) ||
        hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT || hp->combat.cur_melee_combat_move != nullptr)
    {
        return false;
    }
    const bool local = object_is_local_player(hp);
    if (!local && wp->get_rounds_in_reserve() < 1)
    {
        return false;
    }
    if (!wp->can_reload() && local)
    {
        if (object_is_player(hp))
        {
            return false;
        }
        human_unequip_inventory_item(hp);
        return false;
    }
    human_weapons_player_zoom_off(hp);
    human_weapons_firearm_release_trigger_common(wp, true);
    weapon_class_type weapon_class = WEAPON_CLASS_NONE;
    if ((hp->hflags.playing_equip_anim || hp->hflags.playing_unequip_anim) && hp->desired_equipped_inv_item != nullptr && hp->desired_equipped_inv_item->weapon_handle != 0xFFFFFFFF)
    {
        const weapon_info* w_info = World.object_from_handle<weapon>(hp->desired_equipped_inv_item->weapon_handle)->w_info;
        if (w_info->is_firearm())
        {
            weapon_class = w_info->weapon_class;
        }
    }
    else if (human_weapons_is_firearm_equipped(hp))
    {
        weapon_class = human_weapons_get_weapon_info(hp)->weapon_class;
    }
    const animation_action anim_index = human_weapons_get_reload_animation(weapon_class, hp);
    if (anim_index == ANIMATION_ACTION_NONE || !play_animation)
    {
        human_weapons_firearm_start_reload(hp, wp, wp->w_info->default_reload_delay);
        return true;
    }
    v_animate* vp = hp->char_inst->vp;
    const anim* ap = anim_get_anim_from_handle(vanim_get_action_handle(vp, vp->default_group_index, anim_index));
    float length = 0.0f;
    if (ap != nullptr)
    {
        length = animlib_get_animation_length(ap->animlib_handle);
    }
    float rate = 1.0f;
    float speed;
    player* pp = World.object_cast<player>(hp);
    if (pp != nullptr)
    {
        if (pp->backpack != nullptr)
        {
            rate = pp->backpack->get_backpack_abilities()->reload_speed_factor;
        }
        if (Multiplayer == true)
        {
            rate = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_RELOAD_SPEED) * rate;
        }
    }
    if (pp != nullptr && rate == 0.0f)
    {
        speed = 1.0f;
    }
    else
    {
        speed = 1.0f / rate;
        length = rate * length;
    }
    hp->char_inst->play_action(anim_index, ANIM_ALIAS_RELOAD, speed, 0x20);
    human_weapons_firearm_start_reload(hp, wp, (int)(length * 1000.0f));
    if (!human_is_combat_ready(hp))
    {
        return false;
    }
    human_set_combat_ready(hp, true, (int)(length * 1000.0f));
    return true;
}

// 0x00ABDD00 (:2047)
// The fire action: the custom one (cover, overrides), fine aim fires for an aiming player (by direction), crouched,
// walking, running (by direction), driving and riding (only bullet and launcher weapons can be fired from a car seat
// and the second passenger seat), else the standing fire. A run jump start re-reads the animation flags.
animation_action human_weapons_firearm_get_attack_animation(weapon_class_type weapon_class, human* hp)
{
    player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    const human_states state = hp->cur_state;
    if (hp->hflags.override_default_fire_anim || state == HS_COVER)
    {
        return hp->custom_fire_animation;
    }
    const human_movement_states m_state = hp->cur_movement_state;
    const bool standing = hp->cur_stance != HST_CROUCH;
    bool driving = state == HS_VEHICLE_DRIVING;
    bool riding = state == HS_VEHICLE_SITTING && hp->vehicle_seat_idx != VSI_PASSENGER_2;
    bool riding_left = state == HS_VEHICLE_SITTING && hp->vehicle_seat_idx == VSI_PASSENGER_2;
    if (weapon_class == WEAPON_CLASS_BULLET)
    {
        driving = false;
    }
    else if (weapon_class == WEAPON_CLASS_LAUNCHER)
    {
        riding = false;
        riding_left = false;
        driving = false;
    }
    // Original: for other weapon classes the seat flags stay as computed.
    const animation_state anim_state = hp->char_inst->get_animation_state();
    // Zoom state 2: fine aim.
    if (pp != nullptr && pp->zoom_state == (player_zoom_state)2)
    {
        if (standing)
        {
            switch (anim_state)
            {
            case ANIMATION_STATE_FINE_AIM:
            case ANIMATION_STATE_FINE_AIM_WALK:
                return ANIMATION_ACTION_FINE_AIM_FIRE;
            case ANIMATION_STATE_FINE_AIM_WALK_LEFT:
                return ANIMATION_ACTION_FINE_AIM_FIRE_LEFT;
            case ANIMATION_STATE_FINE_AIM_WALK_RIGHT:
                return ANIMATION_ACTION_FINE_AIM_FIRE_RIGHT;
            case ANIMATION_STATE_FINE_AIM_WALK_BACK:
                return ANIMATION_ACTION_FINE_AIM_FIRE_BACK;
            default:
                return ANIMATION_ACTION_NONE;
            }
        }
        switch (anim_state)
        {
        case ANIMATION_STATE_FINE_AIM_CROUCH:
        case ANIMATION_STATE_FINE_AIM_CROUCH_WALK:
            return ANIMATION_ACTION_FINE_AIM_CROUCH_FIRE;
        case ANIMATION_STATE_FINE_AIM_CROUCH_WALK_LEFT:
            return ANIMATION_ACTION_FINE_AIM_CROUCH_FIRE_LEFT;
        case ANIMATION_STATE_FINE_AIM_CROUCH_WALK_RIGHT:
            return ANIMATION_ACTION_FINE_AIM_CROUCH_FIRE_RIGHT;
        case ANIMATION_STATE_FINE_AIM_CROUCH_WALK_BACK:
            return ANIMATION_ACTION_FINE_AIM_CROUCH_FIRE_BACK;
        default:
            return ANIMATION_ACTION_NONE;
        }
    }
    if (!standing)
    {
        if (m_state != HMS_WALK && m_state != HMS_RUN)
        {
            return ANIMATION_ACTION_CROUCH_FIRE;
        }
        if (anim_state == ANIMATION_STATE_CROUCH_WALK_AIM_LEFT)
        {
            return ANIMATION_ACTION_CROUCH_WALK_FIRE_LEFT;
        }
        if (anim_state == ANIMATION_STATE_CROUCH_WALK_AIM_RIGHT)
        {
            return ANIMATION_ACTION_CROUCH_WALK_FIRE_RIGHT;
        }
        if (anim_state == ANIMATION_STATE_CROUCH_WALK_AIM_BACK)
        {
            return ANIMATION_ACTION_CROUCH_WALK_FIRE_BACK;
        }
        return ANIMATION_ACTION_CROUCH_WALK_FIRE;
    }
    weapon_anim_flags flags;
    *(unsigned int*)&flags = 0;
    if (m_state == HMS_WALK)
    {
        switch (anim_state)
        {
        case ANIMATION_STATE_WALK_READY_LEFT:
        case ANIMATION_STATE_RUN_AIM_LEFT:
            return ANIMATION_ACTION_WALK_FIRE_LEFT;
        case ANIMATION_STATE_WALK_READY_RIGHT:
        case ANIMATION_STATE_RUN_AIM_RIGHT:
            return ANIMATION_ACTION_WALK_FIRE_RIGHT;
        case ANIMATION_STATE_WALK_READY_BACK:
        case ANIMATION_STATE_RUN_AIM_BACK:
            return ANIMATION_ACTION_WALK_FIRE_BACK;
        case ANIMATION_STATE_RUN_JUMP_START:
            human_set_combat_ready(hp, true, 0);
            human_get_animation_flags(hp, hp->cur_state, hp->cur_movement_state, flags);
            break;
        default:
            return ANIMATION_ACTION_WALK_FIRE;
        }
    }
    else if (m_state == HMS_RUN)
    {
        switch (anim_state)
        {
        case ANIMATION_STATE_RUN_READY_LEFT:
        case ANIMATION_STATE_RUN_AIM_LEFT:
            return ANIMATION_ACTION_RUN_FIRE_LEFT;
        case ANIMATION_STATE_RUN_READY_RIGHT:
        case ANIMATION_STATE_RUN_AIM_RIGHT:
            return ANIMATION_ACTION_RUN_FIRE_RIGHT;
        case ANIMATION_STATE_RUN_READY_BACK:
        case ANIMATION_STATE_RUN_READY_BACKPEDAL:
        case ANIMATION_STATE_RUN_AIM_BACK:
            return ANIMATION_ACTION_RUN_FIRE_BACK;
        case ANIMATION_STATE_RUN_JUMP_START:
            human_set_combat_ready(hp, true, 0);
            human_get_animation_flags(hp, hp->cur_state, hp->cur_movement_state, flags);
            break;
        default:
            return ANIMATION_ACTION_RUN_FIRE;
        }
    }
    else
    {
        if (driving)
        {
            switch (anim_state)
            {
            case ANIMATION_STATE_DRIVE_READY_LEFT:
            case ANIMATION_STATE_DRIVE_LEFT_READY_LEFT:
            case ANIMATION_STATE_DRIVE_RIGHT_READY_LEFT:
                return ANIMATION_ACTION_DRIVE_FIRE_LEFT;
            case ANIMATION_STATE_DRIVE_READY_RIGHT:
            case ANIMATION_STATE_DRIVE_LEFT_READY_RIGHT:
            case ANIMATION_STATE_DRIVE_RIGHT_READY_RIGHT:
                return ANIMATION_ACTION_DRIVE_FIRE_RIGHT;
            default:
                return ANIMATION_ACTION_DRIVE_FIRE;
            }
        }
        if (riding)
        {
            if (anim_state == ANIMATION_STATE_RIDE_READY_LEFT)
            {
                return ANIMATION_ACTION_RIDE_FIRE_LEFT;
            }
            if (anim_state == ANIMATION_STATE_RIDE_READY_RIGHT)
            {
                return ANIMATION_ACTION_RIDE_FIRE_RIGHT;
            }
            if (anim_state == ANIMATION_STATE_RIDE_READY_BACK)
            {
                return ANIMATION_ACTION_RIDE_FIRE_BACK;
            }
            return ANIMATION_ACTION_RIDE_FIRE;
        }
        if (!riding_left)
        {
            return ANIMATION_ACTION_STAND_FIRE;
        }
        if (anim_state == ANIMATION_STATE_RIDE_LEFT_READY_LEFT)
        {
            return ANIMATION_ACTION_RIDE_LEFT_FIRE_LEFT;
        }
        if (anim_state == ANIMATION_STATE_RIDE_LEFT_READY_RIGHT)
        {
            return ANIMATION_ACTION_RIDE_LEFT_FIRE_RIGHT;
        }
        if (anim_state == ANIMATION_STATE_RIDE_LEFT_READY_BACK)
        {
            return ANIMATION_ACTION_RIDE_LEFT_FIRE_BACK;
        }
        return ANIMATION_ACTION_RIDE_LEFT_FIRE;
    }
    // A run jump start: the direction from the re-read flags (walking falls through to the run fire too).
    if (flags.left)
    {
        return ANIMATION_ACTION_RUN_FIRE_LEFT;
    }
    if (flags.right)
    {
        return ANIMATION_ACTION_RUN_FIRE_RIGHT;
    }
    if (flags.back || flags.backpedal)
    {
        return ANIMATION_ACTION_RUN_FIRE_BACK;
    }
    return ANIMATION_ACTION_RUN_FIRE;
}

// 0x00ABE230 (:2637)
// The fire action on the human (and the facial fire), not from a turret, not over a melee attack (0x3E4), and only for
// visible humans unless Play_offscreen_fire_animations.
void human_weapons_firearm_play_single_fire_anim(human* hp, weapon* wp)
{
    const weapon_info* w_info = wp->w_info;
    if (human_combat_get_manned_turret(hp) != nullptr)
    {
        return;
    }
    if (hp != nullptr && hp->char_inst != nullptr && hp->char_inst->is_playing_action((animation_action)0x3E4))
    {
        return;
    }
    if (!Play_offscreen_fire_animations && hp->is_offscreen())
    {
        return;
    }
    const animation_action anim = human_weapons_firearm_get_attack_animation(w_info->weapon_class, hp);
    if (anim == ANIMATION_ACTION_NONE)
    {
        return;
    }
    hp->char_inst->stop_action((animation_action)0x3DC, 1, true);
    hp->char_inst->play_action(anim, ANIM_ALIAS_FIRE, 1.0f, 0x2000);
    if (!hp->char_inst->is_playing_action((animation_action)0x3D5))
    {
        hp->char_inst->stop_action((animation_action)0x3FD, 3, false);
        hp->char_inst->play_action(ANIMATION_ACTION_FACIAL_FIRE, ANIM_ALIAS_FACIAL_MORPH, 1.0f, 0);
    }
}

// 0x00ACC4B0 (:2729, LTCG: hp in ECX, wp in EAX)
// Pulls a firearm's trigger: a grinder charges, single player multi rockets queue. A weapon that cannot fire clicks dry
// (once per press for the local player's primary attack). A player out of ammo swaps weapons. A player in the open
// who is not aiming first turns to the camera (a quick turn) and the shot waits; then the trigger goes down and single
// shot weapons fire at once (in multiplayer the trigger ids count the shot).
bool human_weapons_firearm_pull_trigger_common(human* hp, weapon* wp)
{
    if (hp == nullptr || wp == nullptr || !wp->w_info->is_firearm() || hp->combat.cur_melee_combat_move != nullptr)
    {
        return false;
    }
    if (wp->get_owner_handle() != hp->get_handle() && human_weapons_get_weapon(hp) != wp)
    {
        return false;
    }
    if (hp->hflags.playing_equip_anim || hp->hflags.playing_unequip_anim)
    {
        return false;
    }
    if (wp->w_info->weapon_class == WEAPON_CLASS_GRINDER && !wp->charge_grinder(true))
    {
        return false;
    }
    if ((wp->w_info->projectile_info.flags & 1) != 0 && upgrade_get_multi_rockets() > 1)
    {
        return wp->process_multi_rocket_trigger_down();
    }
    if (!wp->can_fire())
    {
        if (!wp->is_overheated() && (wp->is_loaded() || wp->has_unlimited_magazine_ammo()))
        {
            return false;
        }
        if (!controls_is_button_just_pressed(CBA_OFC_ATTACK_PRIMARY, Player_controller_index))
        {
            if (!controls_is_button_down(CBA_OFC_ATTACK_PRIMARY, Player_controller_index) || wp->played_dry_fire())
            {
                return false;
            }
        }
        human_weapons_firearm_play_out_of_ammo_sound(hp, wp);
        wp->set_dry_fire_played(true);
        return false;
    }
    player* pp = World.object_cast<player>(hp);
    if (pp != nullptr && !wp->is_trigger_pulled() && !wp->has_unlimited_magazine_ammo() && wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine() == 0 &&
        player_weapon_autoswap((player*)hp))
    {
        return false;
    }
    if (!wp->is_loaded() && !wp->has_unlimited_magazine_ammo())
    {
        return false;
    }
    wp->set_dry_fire_played(false);
    if (pp != nullptr)
    {
        const human_states state = pp->cur_state;
        if ((state == HS_FALL || state == HS_JUMP) && !human_is_combat_ready(pp))
        {
            wp->set_shot_delayed(true);
            wp->one_shot_delay_timestamp.set(0);
            human_set_combat_ready(hp, true, 0);
            return false;
        }
        if (pp->cur_state == HS_STANDARD && !human_is_combat_aiming(pp) && !pp->animation_flags.fine_aim)
        {
            if (pp->pflags.quick_turn)
            {
                // Actions 0x3EA/0x3EB: the quick turn's steps.
                if (!pp->char_inst->is_playing_action((animation_action)0x3EA) && !pp->char_inst->is_playing_action((animation_action)0x3EB))
                {
                    pp->combat.combat_aim_timestamp.set(2000);
                    wp->set_shot_delayed(true);
                    return false;
                }
                if (vanim_is_action_ending_this_frame(pp->char_inst->vp, (animation_action)0x3EA) || vanim_is_action_ending_this_frame(pp->char_inst->vp, (animation_action)0x3EB))
                {
                    pp->combat.combat_aim_timestamp.set(2000);
                }
                wp->set_shot_delayed(true);
                wp->one_shot_delay_timestamp.set(0);
                if (hp->char_inst->get_animation_group() == ANIMATION_GROUP_TURRET_MACHINEGUN)
                {
                    human_set_look_at_target(pp, pp->aim_pos, true, -1.0f);
                }
                return false;
            }
            const bool ready = human_is_combat_ready(pp);
            wp->set_shot_delayed(true);
            if (ready)
            {
                wp->one_shot_delay_timestamp.set(0);
                pp->combat.combat_aim_timestamp.set(2000);
                return false;
            }
            wp->one_shot_delay_timestamp.set(0);
            pp->pflags.quick_turn = 1;
            const vector facing = Rfg_camera.real_orient.fvec.copy_flatten_normalize();
            pp->quick_turn_orient.make_quick(facing);
            pp->quick_turn_speed = 1.0f;
            human_set_combat_ready(hp, true, 0);
            return false;
        }
        if (wp->do_one_shot_delay())
        {
            return true;
        }
        pp->pflags.quick_turn = 0;
        pp->quick_turn_orient = pp->orient;
        pp->quick_turn_speed = -1.0f;
    }
    wp->set_trigger_pulled(true);
    const weapon_trigger_type trigger = wp->w_info->trigger_type;
    if (trigger == WEAPON_TRIGGER_TYPE_SINGLE || trigger == WEAPON_TRIGGER_TYPE_CHARGE)
    {
        if (!wp->is_linked())
        {
            if (Multiplayer)
            {
                hp->mp_flags.firearm_trigger_down_id++;
                hp->mp_flags.firearm_trigger_up_id = hp->mp_flags.firearm_trigger_down_id;
            }
            human_weapons_firearm_play_single_fire_anim(hp, wp);
        }
        wp->fire(hp->aim_override_dir);
        hp->aim_override_dir = ZERO_VECTOR;
    }
    return true;
}

// 0x00ACCC90 (:3184)
// The held firearm's trigger (if attacks are allowed). A player who fires stays combat ready for 15 s and aims for 3 s
// (in cover: cover firing); an empty weapon ends cover firing.
bool human_weapons_firearm_pull_trigger(human* hp)
{
    if (hp == nullptr)
    {
        return false;
    }
    weapon* wp = human_weapons_get_weapon(hp);
    if (wp == nullptr || !gameplay_can_player_attack(wp->w_info, DBT_BULLET))
    {
        return false;
    }
    const bool fired = human_weapons_firearm_pull_trigger_common(hp, wp);
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    if (fired)
    {
        if (pp != nullptr)
        {
            if (hp->cur_state != HS_COVER)
            {
                // Magic numbers: 15 s in the combat ready pose and 3 s aiming after a shot.
                human_set_combat_ready(hp, true, 15000);
                hp->combat.combat_aim_timestamp.set(3000);
                return fired;
            }
            pp->pflags.cover_firing = 1;
        }
    }
    else if (pp != nullptr && !wp->is_loaded())
    {
        pp->pflags.cover_firing = 0;
    }
    return fired;
}

// 0x00ACCBC0 (:3143): the manned turret's secondary weapon.
bool human_weapons_firearm_pull_secondary_weapon_trigger(human* hp)
{
    if (hp == nullptr)
    {
        return false;
    }
    turret* tp = human_combat_get_manned_turret(hp);
    if (tp == nullptr || tp->secondary_weapon_handle == 0xFFFFFFFF)
    {
        return false;
    }
    weapon* wp = (weapon*)World.object_from_handle(tp->secondary_weapon_handle);
    if (wp == nullptr || wp->get_obj_type() != OT_ITEM || wp->get_sub_type() != OT_SUB_ITEM_WEAPON || !gameplay_can_player_attack(wp->w_info, DBT_BULLET))
    {
        return false;
    }
    const bool fired = human_weapons_firearm_pull_trigger_common(hp, wp);
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    if (fired && pp != nullptr)
    {
        if (hp->cur_state == HS_COVER)
        {
            pp->pflags.cover_firing = 1;
            return true;
        }
        human_set_combat_ready(hp, true, 15000);
        hp->combat.combat_aim_timestamp.set(3000);
    }
    return fired;
}

// 0x00A5A9F0 (:3244)
void human_weapons_firearm_release_secondary_weapon_trigger(human* hp)
{
    turret* tp = human_combat_get_manned_turret(hp);
    if (tp == nullptr || tp->secondary_weapon_handle == 0xFFFFFFFF)
    {
        return;
    }
    weapon* wp = (weapon*)World.object_from_handle(tp->secondary_weapon_handle);
    if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON && wp->w_info->is_firearm())
    {
        human_weapons_firearm_release_trigger_common(wp, false);
    }
}

// 0x00A0E670 (:780)
void human_weapons_level_init_post()
{
}

// 0x00A0E760 (:1731)
bool human_is_throwing_weapon(const human* hp)
{
    return hp->combat.thrown_weapon != 0xFFFFFFFF;
}

// 0x00A5A5E0 (:1000)
bool human_weapons_is_thrown_weapon_equipped(human* hp)
{
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    return w_info != nullptr && w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN;
}

// 0x00A5A930 (:2677, LTCG: hp in EAX, wp in ESI)
static bool human_weapons_is_weapon_being_used(const human* hp, const weapon* wp)
{
    if (wp->get_owner_handle() == hp->get_handle())
    {
        return true;
    }
    return human_weapons_get_weapon(hp) == wp;
}

// 0x00AB2DF0 (:119, LTCG: hp in EDI, wp in ESI, equip on the stack)
// A thrown weapon held in the off hand: attached to the off hand tag (its physics removed, hidden) or detached.
static void equip_offhand_weapon(human* hp, weapon* wp, bool equip)
{
    if (equip)
    {
        hp->set_child(wp, hp->equip_offhand_tag_index, wp->get_grip_tag_index(), false, true);
        const unsigned int handle = wp->havok_handle;
        wp->owner_handle = hp->get_handle();
        if (handle != 0xFFFFFFFF)
        {
            havok_body_free(handle, false, true, false);
            wp->havok_handle = 0xFFFFFFFF;
        }
        object_set_visibility_plus_children(wp, false);
        if (wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            weapon_stop_pickup_effect(wp);
        }
        wp->hold_item(hp, true);
        return;
    }
    hp->remove_child(wp);
    wp->hold_item(hp, false);
}

// 0x00AB2E80 (:130, LTCG: hp in EDI, wp in ESI, equip on the stack): the same on the equip tag (a thrown weapon in
// the main hand while another is equipped).
static void equip_temp_weapon(human* hp, weapon* wp, bool equip)
{
    if (equip)
    {
        hp->set_child(wp, hp->equip_tag_index, wp->get_grip_tag_index(), false, true);
        const unsigned int handle = wp->havok_handle;
        wp->owner_handle = hp->get_handle();
        if (handle != 0xFFFFFFFF)
        {
            havok_body_free(handle, false, true, false);
            wp->havok_handle = 0xFFFFFFFF;
        }
        object_set_visibility_plus_children(wp, false);
        if (wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            weapon_stop_pickup_effect(wp);
        }
        wp->hold_item(hp, true);
        return;
    }
    hp->remove_child(wp);
    wp->hold_item(hp, false);
}

// 0x00A33070 (:144, LTCG: pp in EAX, proj_pos in EDX)
// A throw is let go only in front of the player's camera facing or within 0.5 m of its feet (horizontally).
static bool allow_projectile_release(player* const pp, const vector& proj_pos)
{
    const matrix* cam = pp != nullptr && pp == Player ? &Rfg_camera.real_orient : &pp->mp_camera_orient;
    const float dx = proj_pos.x - pp->pos.x;
    const float dz = proj_pos.z - pp->pos.z;
    if (dx * cam->fvec.x + (proj_pos.y - pp->pos.y) * 0.0f + dz * cam->fvec.z < 0.0f && 0.25f <= dx * dx + 0.0f + dz * dz)
    {
        return false;
    }
    return true;
}

// 0x00A83530 (:184, LTCG: hp in ESI, orient in EDI, pos and wp on the stack)
// Where a held projectile is: the off hand projectile item, the weapon itself (held in the hand), or the hand's tag (the
// off hand one for left handed weapons; the equip tag without a weapon).
static void get_held_projectile_pos_orient(human* hp, vector& pos, matrix& orient, const weapon* wp)
{
    if (hp->offhand_projectile_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(hp->offhand_projectile_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM)
        {
            pos = objp->pos;
            orient = objp->orient;
            return;
        }
    }
    if (wp == nullptr && (wp = hp->get_equipped_weapon()) == nullptr)
    {
        animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->equip_tag_index, hp->pos, hp->orient, pos, &orient);
        hp->get_tag_world_coords(hp->equip_tag_index, pos, &orient, nullptr, nullptr);
        return;
    }
    if (!wp->w_info->flags.draw_proj_in_opposite_hand)
    {
        pos = wp->pos;
        orient = wp->orient;
        return;
    }
    // The off hand tag when the weapon is held in the left hand or drawn in the opposite hand, not both.
    const int tag_index = wp->w_info->flags.held_in_left_hand != wp->w_info->flags.draw_proj_in_opposite_hand ? hp->equip_offhand_tag_index : hp->equip_tag_index;
    animlib_get_tag_world_coords(hp->char_inst->aim_handle, tag_index, hp->pos, hp->orient, pos, &orient);
}

// 0x00ABDB60 (:1188)
// The throw action: in a car by the camera's angle to the car (none behind), the custom one, the singularity bomb's
// plant, the grenade throw, a standing (or crouched) throw, else a moving one by direction.
animation_action human_weapons_get_throw_animation(human* hp, const weapon_info* w_info)
{
    const human_movement_states m_state = hp->cur_movement_state;
    hp->hflags.aim_at_pos = 1;
    weapon_anim_flags flags;
    *(unsigned int*)&flags = 0;
    human_get_animation_flags(hp, hp->cur_state, m_state, flags);
    const human_states state = hp->cur_state;
    if (state == HS_VEHICLE_DRIVING)
    {
        object* vp = World.object_from_handle(hp->vehicle_handle);
        const float angle = vec_delta_ang(Rfg_camera.real_orient.fvec, vp->orient.fvec, UP_VECTOR);
        const float abs_angle = fabsf(angle);
        // Magic numbers: 45 and 135 degrees.
        if (0.7853982f <= abs_angle && abs_angle < 2.3561945f)
        {
            return angle < 0.0f ? ANIMATION_ACTION_THROW_ITEM_DRIVE_RIGHT : ANIMATION_ACTION_THROW_ITEM_DRIVE_LEFT;
        }
        if (abs_angle < 2.3561945f)
        {
            return ANIMATION_ACTION_THROW_ITEM_DRIVE;
        }
        return ANIMATION_ACTION_NONE;
    }
    if (hp->hflags.override_default_fire_anim || state == HS_COVER)
    {
        return hp->custom_fire_animation;
    }
    if (w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
    {
        return ANIMATION_ACTION_PLANT_BOMB;
    }
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_GRENADE)
    {
        return ANIMATION_ACTION_THROW_GRENADE;
    }
    if (!flags.run && !flags.walk && state != HS_JUMP && state != HS_FALL)
    {
        // The crouched throw is 10 after the standing one.
        return (animation_action)((flags.crouch ? 10 : 0) + ANIMATION_ACTION_THROW_ITEM);
    }
    if (flags.right)
    {
        return ANIMATION_ACTION_THROW_ITEM_MOVE_RIGHT;
    }
    if (flags.left)
    {
        return ANIMATION_ACTION_THROW_ITEM_MOVE_LEFT;
    }
    if (!flags.back && !flags.backpedal)
    {
        return ANIMATION_ACTION_THROW_ITEM_MOVE_FORWARD;
    }
    // Original: backwards is the back throw standing and the forward one crouched.
    return (animation_action)((flags.crouch ? 0 : 1) | ANIMATION_ACTION_THROW_ITEM_MOVE_FORWARD);
}

// 0x00ACB520 (:1288)
// Starts a throw: npcs with their velocity (or a roll), players with the start speed 12 degrees up in camera space
// (scaled by the throw strength), planting placeable charges against a wall 2 m ahead or at the feet, the moving throws
// steering the player, a crouched charge behind turning to the camera. Not while another throw, place or melee runs, with
// an empty weapon (players swap) or a held object; in multiplayer at most two singularity bombs per player. The throw
// counts in the mp flags.
bool human_weapons_start_throw(human* hp, weapon* wp, const vector* npc_throw_vel, bool roll)
{
    if (hp == nullptr || wp == nullptr || inv_item_get_item_from_weapon(hp, wp) == nullptr)
    {
        return false;
    }
    if ((hp->get_sub_type() == OT_SUB_HUMAN_NPC && npc_throw_vel == nullptr) || hp->combat.cur_melee_combat_move != nullptr || !gameplay_can_player_attack(wp->w_info, DBT_EXPLOSION))
    {
        return false;
    }
    if (!wp->is_loaded())
    {
        if (object_is_player(hp))
        {
            player_weapon_autoswap((player*)hp);
        }
        return false;
    }
    // Actions 0x3DB (throw), 0x3DE and 0x3E3.
    if (hp->char_inst->is_playing_action((animation_action)0x3DB) || hp->char_inst->is_playing_action((animation_action)0x3DE) || hp->char_inst->is_playing_action((animation_action)0x3E3))
    {
        return false;
    }
    player* pp = World.object_cast<player>(hp);
    if (pp != nullptr)
    {
        if (pp->hold_object_handle != 0xFFFFFFFF)
        {
            return false;
        }
        if (Player_hold_throw_timestamp.valid() && !Player_hold_throw_timestamp.elapsed())
        {
            return false;
        }
    }
    if (Multiplayer == true && pp != nullptr && wp->w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
    {
        int count = 0;
        const int num = World.get_num_objects<projectile>();
        for (int i = 0; i < num; i++)
        {
            const projectile* proj = World.get_object<projectile>(i);
            if (proj->get_owner_handle() == pp->get_handle() && proj->w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
            {
                count++;
            }
        }
        if (count > 1)
        {
            return false;
        }
    }
    hp->combat.thrown_weapon = wp->get_handle();
    hp->combat.thrown_weapon_released = false;
    // Original: the persona index is not checked (-1 reads before the sound table).
    audiolib_cue_play_3d(wp->w_info->npc_fire_sounds[wp->get_weapon_persona()], &hp->pos, nullptr, nullptr, nullptr);
    if (pp == nullptr)
    {
        hp->anim_prop_info = wp->info;
        hp->combat.throw_velocity = *npc_throw_vel;
        const animation_action anim = roll ? ANIMATION_ACTION_ROLL_GRENADE : human_weapons_get_throw_animation(hp, wp->w_info);
        hp->char_inst->play_action(anim, ANIM_ALIAS_THROW, 1.0f, 0x20);
        hp->mp_flags.throw_attack_id++;
        return true;
    }
    float cos_pitch;
    const float sin_pitch = fl_sincos(Throw_pitch_offset, &cos_pitch);
    const float speed = wp->w_info->projectile_info.start_speed;
    hp->combat.throw_velocity = vector((float)((double)speed * 0.0), speed * sin_pitch, speed * cos_pitch);
    matrix held_orient;
    get_held_projectile_pos_orient(hp, pp->thrown_weapon_check_pos, held_orient, wp);
    raycast_input in;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    vector center;
    hp->get_center_pos(center);
    // Magic number: a wall within 2 m ahead takes a placed charge.
    in.start = center;
    in.end = vector(center.x + hp->orient.fvec.x * 2.0f, center.y + hp->orient.fvec.y * 2.0f, center.z + hp->orient.fvec.z * 2.0f);
    const unsigned int proj_flags = wp->w_info->projectile_info.flags;
    // Projectile flag 0x80: placeable (0x100: not on the ground).
    const bool place_on_wall = (proj_flags & 0x80) != 0 && havok_raycast(in, out);
    if (place_on_wall || ((proj_flags & 0x80) != 0 && (proj_flags & 0x100) == 0))
    {
        human_set_combat_ready(hp, true, 15000);
        hp->char_inst->play_action(ANIMATION_ACTION_PLACE_ITEM, ANIM_ALIAS_THROW, 1.0f, 0x20);
        item* charge = World.object_from_handle<item>(hp->offhand_projectile_handle);
        if (charge != nullptr && charge->char_inst != nullptr)
        {
            charge->char_inst->play_action(ANIMATION_ACTION_PLACE_ITEM, ANIM_ALIAS_THROW, 1.0f, 0x20);
        }
        if (place_on_wall)
        {
            const float strength = Throw_default_place_strength_on_foot;
            hp->combat.throw_velocity = vector(hp->combat.throw_velocity.x * strength, strength * hp->combat.throw_velocity.y, strength * hp->combat.throw_velocity.z);
        }
        else
        {
            hp->combat.throw_velocity = ZERO_VECTOR;
        }
        hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
        hp->block_movement_anim = 0x3DB;
        if (Multiplayer)
        {
            hp->block_steering_vector = pp->mp_camera_orient.fvec.copy_flatten_normalize();
        }
        else if (Rfg_camera.mode == CAMERA_SLEW_MODE)
        {
            hp->block_steering_vector = hp->steering_vector;
        }
        else
        {
            hp->block_steering_vector = Rfg_camera.real_orient.fvec.copy_flatten_normalize_safe();
        }
        hp->mp_flags.throw_attack_id++;
        return true;
    }
    human_set_combat_ready(hp, true, 15000);
    animation_action anim;
    if (player_has_non_inventory(pp))
    {
        anim = ANIMATION_ACTION_THROW_ITEM;
    }
    else
    {
        anim = human_weapons_get_throw_animation(hp, wp->w_info);
    }
    if (anim != ANIMATION_ACTION_NONE)
    {
        hp->char_inst->play_action(anim, ANIM_ALIAS_THROW, 1.0f, 0x20);
    }
    if (anim == ANIMATION_ACTION_NONE || !vanim_is_playing_action(hp->char_inst->vp, (animation_action)0x3DB))
    {
        human_in_vehicle_interaction(hp, nullptr);
        hp->combat.combat_ready_timestamp.invalidate();
        return false;
    }
    const float strength = Throw_default_strength_on_foot;
    hp->combat.throw_velocity = vector(hp->combat.throw_velocity.x * strength, strength * hp->combat.throw_velocity.y, strength * hp->combat.throw_velocity.z);
    hp->block_movement_anim = 0x3DB;
    hp->block_movement_type = BLOCK_MOVEMENT_ORIENT;
    matrix start_orient;
    float blend;
    if (pp->steering_vector.is_null() || anim == ANIMATION_ACTION_THROW_ITEM)
    {
        if (object_is_local_player(pp))
        {
            pp->block_steering_vector = Rfg_camera.real_orient.fvec.copy_flatten_normalize();
        }
        else
        {
            pp->block_steering_vector = pp->orient.fvec;
        }
        start_orient.make_quick(pp->block_steering_vector);
        blend = 0.1f;
    }
    else
    {
        if (player_has_non_inventory(pp))
        {
            // Original: the movement anim becomes the throw item action's id (0x270), not the throw (0x3DB).
            pp->block_movement_anim = ANIMATION_ACTION_THROW_ITEM;
            pp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
            hp->block_steering_vector = Rfg_camera.mode == CAMERA_SLEW_MODE ? hp->steering_vector : Rfg_camera.real_orient.fvec;
            hp->mp_flags.throw_attack_id++;
            return true;
        }
        const vector fwd = Rfg_camera.real_orient.fvec.copy_flatten_normalize();
        const vector right = Rfg_camera.real_orient.rvec.copy_flatten_normalize();
        pp->block_steering_vector = pp->steering_vector;
        pp->block_steering_heading_offset = 0.0f;
        bool forced = true;
        switch (anim)
        {
        case ANIMATION_ACTION_THROW_ITEM_MOVE_FORWARD:
            if (pp->block_steering_vector * fwd < 0.7f)
            {
                pp->block_steering_vector = fwd;
                pp->block_steering_heading_offset = 0.0f;
            }
            break;
        case ANIMATION_ACTION_THROW_ITEM_MOVE_BACK:
            if (-0.7f < pp->block_steering_vector * fwd)
            {
                pp->block_steering_vector = -fwd;
                pp->block_steering_heading_offset = 0.0f;
            }
            break;
        case ANIMATION_ACTION_THROW_ITEM_MOVE_LEFT:
            if (-0.7f < pp->block_steering_vector * right)
            {
                pp->block_steering_vector = -right;
                pp->block_steering_heading_offset = 0.0f;
            }
            break;
        case ANIMATION_ACTION_THROW_ITEM_MOVE_RIGHT:
            if (pp->block_steering_vector * right < 0.7f)
            {
                pp->block_steering_vector = right;
                pp->block_steering_heading_offset = 0.0f;
            }
            break;
        default:
            forced = false;
            break;
        }
        if (forced)
        {
            pp->hflags.block_forced_movement = 1;
        }
        if (!pp->animation_flags.crouch || wp->w_info->weapon_class != WEAPON_CLASS_THROWN_CHARGE || pp->cur_state == HS_COVER)
        {
            hp->mp_flags.throw_attack_id++;
            return true;
        }
        // A crouched charge thrown more than 135 degrees from the camera turns to it.
        const vector cam_fwd = Rfg_camera.real_orient.fvec.copy_flatten_normalize();
        if (-0.707f <= cam_fwd * pp->orient.fvec)
        {
            hp->mp_flags.throw_attack_id++;
            return true;
        }
        const float cam_heading = fl_atan2_safe(Rfg_camera.real_orient.fvec.x, Rfg_camera.real_orient.fvec.z);
        pp->block_steering_heading_offset = cam_heading - fl_atan2_safe(pp->steering_vector.x, pp->steering_vector.z);
        start_orient.make_quick(Rfg_camera.real_orient.fvec.copy_flatten_normalize());
        blend = 0.2f;
    }
    vanim_set_action_starting_orient(hp->char_inst->vp, hp->pos, hp->orient, hp->pos, start_orient, (animation_action)0x3DB, blend);
    hp->mp_flags.throw_attack_id++;
    return true;
}

// 0x00ACA410 (:276)
// The throw animation's frame: the thrown weapon moves to its hand at the impact end trigger, the local player's moving
// throws keep steering (and stop when turned away), and at the impact start trigger (for a player earlier, from frame 8
// or the tag trigger, as soon as the projectile is clear of walls in front) the projectile is launched along the throw
// velocity (in camera space, plus a camera-line correction and the player's velocity). A held object bounces off the
// wall it would go through. The weapon leaves the hand; an empty thrown weapon goes from the inventory and the player
// equips its last weapon.
void process_thrown_weapon(human* hp)
{
    player* pp = hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    if (hp->char_inst == nullptr || !hp->char_inst->is_playing_action((animation_action)0x3DB))
    {
        hp->combat.thrown_weapon_released = false;
        hp->combat.thrown_weapon = 0xFFFFFFFF;
        return;
    }
    weapon* wp = nullptr;
    if (hp->combat.thrown_weapon != 0xFFFFFFFF)
    {
        wp = (weapon*)World.object_from_handle(hp->combat.thrown_weapon);
    }
    if (wp == nullptr || wp->get_obj_type() != OT_ITEM || wp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        hp->combat.thrown_weapon = 0xFFFFFFFF;
        return;
    }
    const weapon_info* w_info = wp->w_info;
    if (w_info->ammo_type != WEAPON_AMMO_TYPE_THROWN)
    {
        return;
    }
    inventory_item* iip = inv_item_get_item_from_weapon(hp, wp);
    if (iip == nullptr)
    {
        return;
    }
    const bool equipped = hp->get_equipped_weapon() == wp;
    const bool left_hand = !equipped && w_info->flags.held_in_left_hand && !w_info->flags.draw_proj_in_opposite_hand;
    const bool temp = !equipped && !left_hand;
    vector pos;
    matrix orient;
    get_held_projectile_pos_orient(hp, pos, orient, wp);
    bool release = true;
    if (!vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_impact_start, (animation_action)0x3DB, -1))
    {
        release = false;
        if (w_info->flags.draw_proj_in_opposite_hand)
        {
            if (can_fire_charge(hp->get_handle(), false) && vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_impact_end, (animation_action)0x3DB, -1))
            {
                item* charge = World.object_from_handle<item>(hp->offhand_projectile_handle);
                if (charge != nullptr)
                {
                    charge->hold_item(hp, true);
                }
            }
        }
        else if (left_hand)
        {
            if (vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_impact_end, (animation_action)0x3DB, -1))
            {
                const animation_group group = hp->char_inst->vp->default_group_index;
                if (group == ANIMATION_GROUP_HAMMER || group == ANIMATION_GROUP_GUTTER)
                {
                    equip_temp_weapon(hp, wp, true);
                }
                else
                {
                    equip_offhand_weapon(hp, wp, true);
                }
            }
        }
        else if (temp && vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_impact_end, (animation_action)0x3DB, -1))
        {
            equip_temp_weapon(hp, wp, true);
        }
        if (object_is_local_player(hp))
        {
            if (hp->block_movement_anim == -1)
            {
                if (hp->block_steering_vector.is_null())
                {
                    // Magic number: a charge's hold from frame 12.
                    if (vanim_get_anim_frame(hp->char_inst->vp, (animation_action)0x3DB) > 11)
                    {
                        item* charge = World.object_from_handle<item>(hp->offhand_projectile_handle);
                        if (charge != nullptr && can_fire_charge(hp->get_handle(), false))
                        {
                            charge->hold_item(hp, true);
                        }
                    }
                }
                else
                {
                    const float dot = hp->orient.fvec * hp->block_steering_vector;
                    const int index = vanim_get_anim_index(hp->char_inst->vp, (animation_action)0x3DB);
                    bool stop;
                    // Anim indices 0x270/0x26F: the standing throws stop when the player moves.
                    if (index == 0x270 || index == 0x26F)
                    {
                        stop = 0.0f < hp->move_speed;
                    }
                    else
                    {
                        stop = dot < 0.85f || hp->move_speed == 0.0f;
                    }
                    if (stop)
                    {
                        hp->char_inst->stop_action((animation_action)0x3DB, 6, false);
                        hp->block_steering_vector.zero();
                    }
                }
            }
            const double heading = (double)Rfg_camera.real_orient.extract_heading();
            hp->block_steering_vector.rotate_around_y_axis((float)(heading - (double)Rfg_camera.last_orient.extract_heading()));
        }
    }
    vector bounce = ZERO_VECTOR;
    bool bounces = false;
    if (pp == nullptr || hp->combat.thrown_weapon_released)
    {
        if (!release)
        {
            return;
        }
    }
    else
    {
        const int frame = vanim_get_anim_frame(hp->char_inst->vp, (animation_action)0x3DB);
        const bool in_front = allow_projectile_release(pp, pos);
        // The tag trigger's frame at 30 frames per second (8 without one).
        int tag_frame = (int)((double)vanim_get_trigger_time(hp->char_inst->vp, Trigger_tag_start, (animation_action)0x3DB, 0) * 30.0 + 0.01);
        if ((float)tag_frame <= 0.0f)
        {
            tag_frame = 8;
        }
        bool clear_release = false;
        if (((!w_info->flags.non_inventory && rc_test_frame < frame) || tag_frame < frame) && in_front)
        {
            weapon_fire_info check(pp->thrown_weapon_check_pos, pos, w_info);
            vector bmin;
            vector bmax;
            wp->get_local_bbox(bmin, bmax);
            const float size = vec_dist(bmax, bmin);
            const vector ahead = Rfg_camera.real_orient.fvec * extra_distance_check;
            check.end_pos = vector(ahead.x * size + pos.x, ahead.y * size + pos.y, ahead.z * size + pos.z);
            if (weapons_test_bullet(check, hp, false, fabsf(bmax.x - bmin.x) * radius_multiplier, ZERO_VECTOR) && check.hit_handle != wp->get_handle())
            {
                pos = pp->thrown_weapon_check_pos;
                if (w_info->flags.non_inventory)
                {
                    bounce = vector(-check.hit_normal.x * velocity_multiplier, -check.hit_normal.y * velocity_multiplier, -check.hit_normal.z * velocity_multiplier);
                    bounces = true;
                }
                clear_release = true;
            }
        }
        if (!clear_release && !release)
        {
            pp->thrown_weapon_check_pos = pos;
            return;
        }
    }
    if (hp->combat.thrown_weapon_released)
    {
        return;
    }
    vector vel = hp->combat.throw_velocity;
    if (pp != nullptr)
    {
        gameplay_register_player_attack(pp, wp->w_info, DBT_NONE, false);
        if (object_is_local_player(pp))
        {
            vel.unrotate(Rfg_camera.real_orient);
            // The throw aims along the camera's line: the offset to it 20 m out is added sideways.
            const vector far_pos(Rfg_camera.real_orient.fvec.x * 20.0f + Rfg_camera.real_pos.x, Rfg_camera.real_orient.fvec.y * 20.0f + Rfg_camera.real_pos.y,
                             Rfg_camera.real_orient.fvec.z * 20.0f + Rfg_camera.real_pos.z);
            vector closest;
            ix_closest_point_on_line_segment(closest, pos, Rfg_camera.real_pos, far_pos);
            const float side = (closest - pos).mag();
            vel = vector(side * Rfg_camera.real_orient.rvec.x + vel.x, side * Rfg_camera.real_orient.rvec.y + vel.y, side * Rfg_camera.real_orient.rvec.z + vel.z);
        }
        else
        {
            vel.unrotate(pp->mp_camera_orient);
        }
        vector own_vel;
        human_controller_get_vel(hp, own_vel);
        vel = vector(own_vel.x * Throw_inherit_velocity_scale + vel.x, own_vel.y * Throw_inherit_velocity_scale + vel.y, own_vel.z * Throw_inherit_velocity_scale + vel.z);
    }
    if (!Multiplayer || object_is_local_player(hp))
    {
        if (bounces)
        {
            const float speed = vel.mag();
            vel = vector(speed * bounce.x, bounce.y * speed, speed * bounce.z);
        }
        projectile* proj = wp->launch_thrown_weapon(pos, orient, vel);
        if (proj != nullptr)
        {
            if (hp->get_sub_type() == OT_SUB_HUMAN_NPC)
            {
                ((npc*)hp)->get_ai()->perception.threw_grenade(proj);
            }
            proj->update_animlib(0.0f, true);
            if (proj->w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
            {
                npc_sound_info_bomb_set sound(proj, hp);
                npc_sound_info::queue<npc_sound_info_bomb_set>(sound);
            }
        }
        hp->combat.thrown_weapon_released = true;
        audiolib_cue_play(wp->w_info->fire_sound, nullptr, nullptr);
    }
    if (w_info->flags.draw_proj_in_opposite_hand)
    {
        item* charge = World.object_from_handle<item>(hp->offhand_projectile_handle);
        if (charge != nullptr)
        {
            charge->hold_item(hp, false);
        }
    }
    else if (left_hand || temp)
    {
        hp->remove_child(wp);
        wp->hold_item(hp, false);
    }
    else if (w_info->flags.non_inventory && object_is_player(hp))
    {
        player_weapon_autoswap(pp);
    }
    if (wp->is_loaded() || w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
    {
        return;
    }
    if (pp == nullptr)
    {
        if (iip->count > 1)
        {
            iip->count--;
            return;
        }
        human_delete_item_from_inventory(hp, iip, true, false);
        return;
    }
    int num_items = 0;
    inventory_item* first = pp->inventory;
    if (first == nullptr)
    {
        return;
    }
    for (inventory_item* it = first;;)
    {
        it = it->next;
        num_items++;
        if (it == first || it == nullptr)
        {
            break;
        }
    }
    if (num_items < 2)
    {
        return;
    }
    pp->set_equipped_inv_item(nullptr);
    pp->desired_equipped_inv_item = nullptr;
    if (Multiplayer && !multi_is_server())
    {
        multi_game_player_send_remove_weapon(iip);
    }
    else
    {
        human_remove_item_from_inventory(pp, iip, iip->count, false); // inlined
    }
    player_make_last_equipped_weapon_valid(pp);
    player_equip_item(pp, pp->last_equipped_weapon, nullptr, false);
}

// 0x00ACC140 (:1591)
// A human whose throw is interrupted (dying, stunned) drops the thrown weapon in a random direction at up to 3 m/s plus
// its own velocity (a live grenade); the local or an npc only. An empty weapon leaves the inventory (a player equips its
// last weapon); npcs remember the dropped grenade.
bool human_weapons_drop_thrown_weapon(human* hp)
{
    if (hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && hp != Player)
    {
        return false;
    }
    if (hp->combat.thrown_weapon == 0xFFFFFFFF || hp->combat.thrown_weapon_released)
    {
        return false;
    }
    weapon* wp = (weapon*)World.object_from_handle(hp->combat.thrown_weapon);
    if (wp == nullptr || wp->get_obj_type() != OT_ITEM || wp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return false;
    }
    const weapon_info* w_info = wp->w_info;
    if (w_info->ammo_type != WEAPON_AMMO_TYPE_THROWN)
    {
        return false;
    }
    inventory_item* iip = inv_item_get_item_from_weapon(hp, wp);
    if (iip == nullptr)
    {
        return false;
    }
    const weapon* equipped = hp->get_equipped_weapon();
    const bool left_hand = equipped != wp && w_info->flags.held_in_left_hand && !w_info->flags.draw_proj_in_opposite_hand;
    bool temp = false;
    // The weapon must already be in the hand it is thrown from, else the throw is just cancelled.
    if (left_hand)
    {
        if (wp->attach_info == nullptr || wp->attach_info->parent_prop_point != hp->equip_offhand_tag_index)
        {
            hp->combat.thrown_weapon = 0xFFFFFFFF;
            hp->combat.thrown_weapon_released = false;
            hp->char_inst->stop_action((animation_action)0x3DB, 5, false);
            return false;
        }
    }
    else if (equipped != wp)
    {
        temp = true;
        if (wp->attach_info == nullptr || wp->attach_info->parent_prop_point != hp->equip_tag_index)
        {
            hp->combat.thrown_weapon = 0xFFFFFFFF;
            hp->combat.thrown_weapon_released = false;
            hp->char_inst->stop_action((animation_action)0x3DB, 5, false);
            return false;
        }
    }
    if (w_info->flags.draw_proj_in_opposite_hand)
    {
        item* charge = World.object_from_handle<item>(hp->offhand_projectile_handle);
        if (charge != nullptr)
        {
            charge->hold_item(hp, false);
        }
    }
    else if (left_hand || temp)
    {
        hp->remove_child(wp);
        wp->hold_item(hp, false);
    }
    if (w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
    {
        return false;
    }
    vector pos;
    matrix orient;
    get_held_projectile_pos_orient(hp, pos, orient, wp);
    vector vel;
    vel.rand_sphere();
    const float speed = fl_rand_range(0.0f, 3.0f);
    vel = vector(vel.x * speed, vel.y * speed, speed * vel.z);
    vector own_vel;
    human_controller_get_vel(hp, own_vel);
    vel = vector(own_vel.x + vel.x, vel.y + own_vel.y, own_vel.z + vel.z);
    projectile* proj = wp->launch_thrown_weapon(pos, orient, vel);
    if (wp->is_loaded())
    {
        if (w_info->flags.draw_proj_in_opposite_hand)
        {
            item* charge = World.object_from_handle<item>(hp->offhand_projectile_handle);
            if (charge != nullptr)
            {
                charge->hold_item(hp, true);
            }
        }
    }
    else
    {
        bool remove = true;
        if (object_is_player(hp))
        {
            if (w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
            {
                remove = false;
            }
            else
            {
                hp->set_equipped_inv_item(hp->last_equipped_weapon);
                hp->desired_equipped_inv_item = hp->last_equipped_weapon;
                hp->last_equipped_weapon = hp->grenade_weapon;
            }
        }
        if (remove && w_info->weapon_class != WEAPON_CLASS_THROWN_CHARGE && (Multiplayer != true || multi_is_server()))
        {
            human_delete_item_from_inventory(hp, iip, false, false);
        }
    }
    hp->combat.thrown_weapon = 0xFFFFFFFF;
    hp->combat.thrown_weapon_released = false;
    hp->char_inst->stop_action((animation_action)0x3DB, 5, false);
    npc* np = World.object_cast<npc>(hp);
    if (proj != nullptr && np != nullptr && !human_is_dead(np))
    {
        np->get_ai()->perception.dropped_grenade(proj);
    }
    return true;
}

// 0x00A9D710 (:1545)
// The human's shield flies off (a virtual shield is destroyed): up to 30 degrees around its facing at 2-3 m/s plus the
// human's velocity, as an ultra light debris that ignores the human.
void human_weapons_drop_shield(human* hp)
{
    if (hp->shield_handle == 0xFFFFFFFF)
    {
        return;
    }
    item* ip = (item*)World.object_from_handle(hp->shield_handle);
    if (ip == nullptr || ip->get_obj_type() != OT_ITEM)
    {
        return;
    }
    // Item info flag 0x2000: a virtual shield.
    if (((unsigned int)ip->info->flags >> 13 & 1) != 0)
    {
        hp->destroy_virtual_shield();
        return;
    }
    const vector facing = ip->orient.fvec.copy_normalize_safe();
    vector vel;
    vel.rand_around_dir(facing, 0.8660254f, 1.0f);
    const float speed = fl_rand_range(2.0f, 3.0f);
    vel = vector(speed * vel.x, vel.y * speed, speed * vel.z);
    vector own_vel;
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->get_velocity(own_vel);
    }
    else
    {
        own_vel.zero();
    }
    vel = vector(own_vel.x + vel.x, own_vel.y + vel.y, own_vel.z + vel.z);
    hp->shield_handle = 0xFFFFFFFF;
    ip->item_flags.item_held = 0;
    hp->remove_child(ip);
    if (ip->havok_handle == 0xFFFFFFFF)
    {
        create_physics_object(ip);
    }
    const unsigned int handle = ip->havok_handle;
    if (handle != 0xFFFFFFFF)
    {
        hkpRigidBody* rb = havok_body_get_pointer(handle);
        if (rb->getNumActions() != 0)
        {
            hkpAction* action = rb->getAction(0);
            if (action != nullptr)
            {
                Havok_world->removeAction(action);
            }
            havok_body_set_collision_type(rb, COLLIDE_RFG_ULTRA_LITE, true, false);
            havok_set_ignore_property(rb, hp->get_handle());
            havok_body_set_linear_velocity(rb, vel);
        }
    }
}

// 0x00ABD280 (:849)
// Drops an inventory weapon: it leaves the hand at the human's chest height (unless it was the held one), gets its
// physics back and is shown (npcs' weapons get random dropped ammo in single player). In multiplayer the server drops it
// as a remote object (only with ammo, or a weapon without ammo), and it is opaque and not picked up for 2.5 s. The server
// then takes it out of the inventory; a weapon dropped in a kill zone (or flagged so) is destroyed.
void human_weapons_drop_weapon(human* hp, inventory_item* iip)
{
    if (iip == nullptr)
    {
        return;
    }
    const bool held = iip == hp->equipped_inv_item;
    if (held)
    {
        human_weapons_immediately_holster_weapon(hp);
    }
    weapon* wp = nullptr;
    if (iip->weapon_handle != 0xFFFFFFFF)
    {
        wp = (weapon*)World.object_from_handle(iip->weapon_handle);
        if (wp != nullptr && (wp->get_obj_type() != OT_ITEM || wp->get_sub_type() != OT_SUB_ITEM_WEAPON))
        {
            wp = nullptr;
        }
    }
    if (wp != nullptr)
    {
        wp->obj_flags.special_lifetime = 0;
        weapon_stop_material_effect(wp);
        wp->set_stealth_plus_child_items(0.0f);
        const auto place = [&]() {
            if (!held)
            {
                // Magic numbers: half of 1.9 m above the feet.
                const vector up = UP_VECTOR * 1.9f;
                wp->pos = vector(hp->pos.x + UP_VECTOR.x * 1.9f * 0.5f, hp->pos.y + up.y * 0.5f, hp->pos.z + up.z * 0.5f);
                wp->last_pos = vector(UP_VECTOR.x * 1.9f * 0.5f + hp->last_pos.x, hp->last_pos.y + UP_VECTOR.y * 1.9f * 0.5f, hp->last_pos.z + UP_VECTOR.z * 1.9f * 0.5f);
            }
            wp->owner_handle = 0xFFFFFFFF;
            if (!wp->obj_flags.streamed)
            {
                wp->item_flags.no_physics = 0;
            }
            else if (wp->havok_handle == 0xFFFFFFFF)
            {
                create_physics_object(wp);
            }
            object_set_visibility_plus_children(wp, true);
        };
        if (!Multiplayer)
        {
            place();
            if (hp != Player)
            {
                wp->adjust_dropped_ammo();
            }
        }
        else if (wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine() > 0 || !wp->uses_ammo())
        {
            if (Multiplayer != true || multi_is_server())
            {
                place();
                wp->remote_obj_flags.remote_object = 1;
                multi_core_object_create_remote_object(3, wp->get_handle());
            }
            else if (hp->equipped_inv_item != nullptr)
            {
                human_weapons_immediately_holster_weapon(hp);
            }
        }
        if (Multiplayer == true)
        {
            wp->set_opacity_plus_child_items(1.0f);
            // Magic number: a dropped weapon cannot be picked up for 2.5 s.
            wp->dropped_timer.set(2500);
        }
    }
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    human_remove_item_from_inventory(hp, iip, iip->count, false); // inlined
    if (wp != nullptr && (hp->hflags.killed_by_killzone || wp->w_info->flags.dont_drop_on_death))
    {
        World.destroy_object(wp, false);
    }
}

// 0x00ABD9B0 (:917)
// Drops the held weapon (or the desired or best one): guerrillas in single player keep theirs; in multiplayer the
// class's unique weapons stay. The zoom goes off; the weapon is marked as dropped (by a player) and dropped.
void human_weapons_drop_equipped_weapon(human* hp)
{
    inventory_item* iip = hp->equipped_inv_item;
    const bool choose = Multiplayer ? iip == nullptr || multi_game_classes_get_default_class()->is_unique_weapon_info(iip->info) : iip == nullptr;
    if (choose)
    {
        if (!Multiplayer && human_teams_are_friendly(hp, HUMAN_TEAM_GUERILLA))
        {
            return;
        }
        iip = hp->desired_equipped_inv_item;
        if (iip == nullptr)
        {
            iip = inv_item_get_best_weapon(hp, false);
        }
    }
    if (Multiplayer == true)
    {
        if (iip == nullptr || multi_game_classes_get_default_class()->is_unique_weapon_info(iip->info))
        {
            return;
        }
    }
    if (iip == nullptr)
    {
        return;
    }
    human_weapons_cancel_reload(hp);
    if (hp == Player)
    {
        human_weapons_player_zoom_off(hp);
    }
    if (iip->weapon_handle == 0xFFFFFFFF)
    {
        return;
    }
    weapon* wp = (weapon*)World.object_from_handle(iip->weapon_handle);
    if (wp != nullptr && wp->get_obj_type() == OT_ITEM && wp->get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        wp->item_flags.simulate = 1;
        wp->obj_flags.special_lifetime = 0;
        wp->item_flags.dropped_by_player = hp->get_sub_type() == OT_SUB_HUMAN_PLAYER;
        human_weapons_drop_weapon(hp, iip);
    }
}

// 0x00A5A730 (:1041)
bool human_weapons_is_obvious_weapon_equipped(const human* hp)
{
    const weapon_info* w_info = human_weapons_get_weapon_info(hp);
    return w_info != nullptr && w_info->flags.is_obvious_weapon;
}

// 0x00A9D910 (:1736)
void human_weapons_unattach_projectiles(human* hp)
{
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (proj->get_parent() == hp->get_handle())
        {
            proj->detach();
        }
    }
}
