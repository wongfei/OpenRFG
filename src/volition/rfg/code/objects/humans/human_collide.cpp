// Original: volition/rfg/code/objects/humans/human_collide.cpp
//
// Collisions of humans with humans, vehicles and movers (decompiled): a car hit damages, ragdolls or flinches the
// human (by relative speed, run-overs at least 500, the player's squad and vips capped), a moving body damages it by
// speed and mass (a head hit and a fall onto it multiply, a facing riot shield quarters it; in multiplayer a client
// reports its own hits by local movers to the server), and two humans touching may knock one down (a human running
// into another's head) or make npcs step aside or wait. Compiled in rfg_objects_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - human_collide_calculate_damage_with_timer returns 0 for every hit within 500 ms of the previous one (the stored
//   damage is raised to the new one before the difference is taken), and each such hit restarts the timer.
// - human_collide_vehicle dereferences the human without checking that the handle resolved to one.
// - player_collide_npc dereferences the npc after its null check.

#include "volition/rfg/code/objects/humans/human_collide.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector2.h"

#include <math.h>

// ---- file-static globals
// const float STRESS_SOUND_MAX_DIST (0x01311000) is used here too; placed in player_meta_data.cpp.
// const float min_correction_diff (0x01315520) is used here too; placed in human.cpp.
// const float FAR_FROM_PLAYER_DIST_SQRD (0x01316B78) is used here too; placed in player.cpp.
static const float NEAR_PLAYER_DIST_SQRD = 3600.0f; // 0x01317CD8
static float Human_vehicle_collision_speed_high; // 0x03017C60, from tweak_table.xtbl (like the rest below)
static float Player_collision_damage_multiplier; // 0x03017C6C
static float Character_collision_falling_multiplier; // 0x03017C74
static float Human_vehicle_collision_normal_velocity_factor; // 0x0301AE44
static float Player_vehicle_collision_damage_max; // 0x03023668
static float Character_collision_damage_min; // 0x03023678
static float Character_collision_head_multiplier; // 0x03023684
// player* Player (0x03023874) is used here too; placed in player.cpp.
static float Human_vehicle_collision_speed_low; // 0x03029B48
static float Human_vehicle_collision_flinch_vehicle_speed; // 0x03029B4C
// Also read by object_mover.cpp, later in the unity TU.
static float Character_collision_damage_speed_max; // 0x0302A160
// Also read by human_combat.cpp (which registers its tweak item), later in the unity TU.
static float Damage_multiplier_player_damage_friendly = 0.3f; // 0x01694330
static tweak_table_item Tweak_table_item_Player_collision_damage_multiplier("Player_collision_damage_multiplier", &Player_collision_damage_multiplier); // 0x0304504C
static tweak_table_item Tweak_table_item_Character_collision_damage_min("Character_collision_damage_min", &Character_collision_damage_min); // 0x030695B0
static tweak_table_item Tweak_table_item_Human_vehicle_collision_speed_high("Human_vehicle_collision_speed_high", &Human_vehicle_collision_speed_high); // 0x03187268
static tweak_table_item Tweak_table_item_Human_vehicle_collision_flinch_vehicle_speed("Human_vehicle_collision_flinch_vehicle_speed", &Human_vehicle_collision_flinch_vehicle_speed); // 0x0319A6A0
static tweak_table_item Tweak_table_item_Player_vehicle_collision_damage_max("Player_vehicle_collision_damage_max", &Player_vehicle_collision_damage_max); // 0x031A2AAC
static tweak_table_item Tweak_table_item_Character_collision_head_multiplier("Character_collision_head_multiplier", &Character_collision_head_multiplier); // 0x031A35C4
static tweak_table_item Tweak_table_item_Character_collision_falling_multiplier("Character_collision_falling_multiplier", &Character_collision_falling_multiplier); // 0x033B1FB4
static tweak_table_item Tweak_table_item_Character_collision_damage_speed_max("Character_collision_damage_speed_max", &Character_collision_damage_speed_max); // 0x033C1430
// const float MIN_HUMAN_IS_SHIELDED_DOT (0x0342A398) is used here too; placed in human.cpp.
static tweak_table_item Tweak_table_item_Human_vehicle_collision_speed_low("Human_vehicle_collision_speed_low", &Human_vehicle_collision_speed_low); // 0x03500C68
static tweak_table_item Tweak_table_item_Human_vehicle_collision_normal_velocity_factor("Human_vehicle_collision_normal_velocity_factor", &Human_vehicle_collision_normal_velocity_factor); // 0x03500C80

// ---- internal functions
static bool human_collide_knockdown(human* hp1, human* hp2); // 0x00AD03A0 (:361)
static void player_collide_npc(player* pp, npc* np); // 0x00A2F020 (:468)
static void npc_collide_npc(npc* first_np, npc* second_np); // 0x00A2F220 (:517)
static human_team_dispositions human_teams_get_disposition_internal(human_teams team1, human_teams team2); // 0x00A0D270, human_teams.cpp, later in the unity TU

// A human is upright (not a ragdoll that has taken over the pose).
static bool human_collide_is_upright(const human* hp)
{
    const human_ragdoll_state rs = hp->ragdoll_state;
    return rs == HUMAN_RAGDOLL_STATE_BLEND_OUT || rs == HUMAN_RAGDOLL_STATE_NONE || rs == HUMAN_RAGDOLL_STATE_FLINCH ||
           (rs == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && !hp->hflags.ragdoll_on_impact_use_ragdoll_pos);
}

// 0x00A2EFD0 (:102): who is blamed for a car ragdolling a human: a rail vehicle's or another driver's last driver, or
// the player for a vehicle the player controls; nobody when the player drove it last.
unsigned int get_ragdoll_car_perp_handle(vehicle* p_car)
{
    player* pp = Player;
    if (p_car != nullptr)
    {
        if (vrail_is_vehicle(p_car->get_handle()))
        {
            return p_car->most_recent_driver;
        }
        if (activity_register_damage_is_player_controlled(p_car))
        {
            return pp->get_handle();
        }
        if (p_car->most_recent_driver != pp->get_handle())
        {
            return p_car->most_recent_driver;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00A093B0 (:127)
// Original bug kept: within the 500 ms window the stored damage is raised to this hit's before the difference is
// taken, so every hit inside the window returns 0 (and restarts the window).
float human_collide_calculate_damage_with_timer(human* hp, float damage)
{
    if (hp->collision_damage_timer.elapsed())
    {
        hp->current_collision_damage = damage;
        hp->collision_damage_timer.set(500);
        return damage;
    }
    float cur = hp->current_collision_damage;
    if ((float)((double)cur - (double)damage) < 0.0f)
    {
        cur = damage;
    }
    hp->current_collision_damage = cur;
    hp->collision_damage_timer.set(500);
    const float extra = (float)((double)damage - (double)hp->current_collision_damage);
    if ((float)(0.0 - (double)extra) >= 0.0f)
    {
        return 0.0f;
    }
    return extra;
}

// 0x00ACF910 (:152): a vehicle hits a human. Damage grows with the relative speed (walkers above 12 m/s, others from
// Human_vehicle_collision_speed_low, or 12 m/s standing on the vehicle); a run-over (also a wheel coming down on a
// human on the ground) does at least 500 except to the player, its squad and vips, who are capped and not hit again
// while ragdolled. Without damage, a hit near the middle of an upright human's capsule still ragdolls it. A damaging
// hit ragdolls and throws the human along the car's point velocity and leaves a blood splatter; fast cars play the
// impact sound and flinch the human.
void human_collide_vehicle(const vector& hit_pos, const vector& hit_normal, float rel_speed, unsigned int human_handle, unsigned int car_handle, unsigned int component_idx, bool run_over)
{
    float damage = 0.0f;
    human* hp = nullptr;
    if (human_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(human_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN)
        {
            hp = (human*)objp;
        }
    }
    if (car_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* car_objp = World.object_from_handle(car_handle);
    if (car_objp == nullptr || car_objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    vehicle* vp = (vehicle*)car_objp;
    human* driver = vehicle_most_recent_driver(vp);
    // Original: hp is used unchecked from here on.
    if (human_in_protected_vehicle_pos(hp))
    {
        return;
    }
    vector car_vel;
    havok_body_get_linear_velocity(vp->havok_handle, car_vel);
    float car_speed_sq = (float)((double)car_vel.x * (double)car_vel.x + (double)car_vel.y * (double)car_vel.y + (double)car_vel.z * (double)car_vel.z);
    // A wheel coming down on a human on the ground runs it over at the speed squared of NEAR_PLAYER_DIST_SQRD (60 m/s).
    if ((double)hit_normal.y <= (double)-0.9f && hp->ground_obj_handle != 0xFFFFFFFF && component_idx != 0xFFFFFFFF && vp->rdd.is_loaded())
    {
        vector point_vel;
        havok_body_get_point_velocity(vp->rdd.get_data()->m_components[component_idx].havok_handle, hit_pos, point_vel);
        if ((double)point_vel.y <= (double)-0.05f)
        {
            run_over = true;
            car_speed_sq = NEAR_PLAYER_DIST_SQRD;
        }
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        // Magic numbers: walkers hurt from 12 m/s, 4 per (m/s)^2 above it.
        if (rel_speed > 12.0f)
        {
            damage = (float)(((double)rel_speed - 12.0) * ((double)rel_speed - 12.0) * 4.0);
        }
    }
    else
    {
        // Magic number: standing on the vehicle, hits count from 12 m/s.
        const float min_speed = hp->ground_obj_handle == vp->get_handle() ? 12.0f : Human_vehicle_collision_speed_low;
        if ((double)rel_speed >= (double)min_speed)
        {
            // Magic numbers: 0..500 from the low to the high speed, 500..2500 up to twice the high speed.
            if ((double)Human_vehicle_collision_speed_high > (double)rel_speed)
            {
                damage = fl_parameter(rel_speed, Human_vehicle_collision_speed_low, Human_vehicle_collision_speed_high, 0.0f, 500.0f, true);
            }
            else
            {
                damage = fl_parameter(rel_speed, Human_vehicle_collision_speed_high, (float)((double)Human_vehicle_collision_speed_high * 2.0), 500.0f, 2500.0f, true);
            }
        }
    }
    // The player, vips and the player's squad.
    bool important;
    if ((hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER) || hp->h_info->flags.vip)
    {
        important = true;
    }
    else if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && Player != nullptr)
    {
        const object_squad* squad = ((npc*)hp)->get_ai()->get_squad();
        important = squad != nullptr && squad->get_player() == Player;
    }
    else
    {
        important = false;
    }
    bool check_contact = false;
    if (run_over && !important)
    {
        // Magic number: a run-over does at least 500.
        if ((float)((double)damage - 500.0) < 0.0f)
        {
            damage = 500.0f;
        }
        else
        {
            check_contact = damage == 0.0f;
        }
    }
    else if (important && human_is_ragdolled(hp))
    {
        damage = 0.0f;
        check_contact = true;
    }
    else
    {
        check_contact = damage == 0.0f;
    }
    if (check_contact && (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) &&
        (double)hp->orient.uvec.y > (double)0.98f && hp->rdd.is_loaded())
    {
        // A hit within a fifth of the radius of the capsule's upper half.
        vector bottom;
        vector top;
        float radius;
        hp->rdd.get_data()->controller->get_capsule_data(bottom, top, radius);
        vector mid;
        mid.x = (top.x + bottom.x) * 0.5f;
        mid.y = (bottom.y + top.y) * 0.5f;
        mid.z = (bottom.z + top.z) * 0.5f;
        radius = (float)((double)radius * (double)0.2f);
        const vector p1 = hp->pos + top;
        const vector p0 = hp->pos + mid;
        vector hit_point;
        if (ix_linesegment_sphere(hit_point, p0, p1, hit_pos, radius))
        {
            human_ragdoll_start(hp, false, get_ragdoll_car_perp_handle(vp));
        }
    }
    if (damage != 0.0f)
    {
        if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            damage = (float)((double)Player_collision_damage_multiplier * (double)damage);
        }
        if (important && (float)((double)damage - (double)Player_vehicle_collision_damage_max) >= 0.0f)
        {
            damage = Player_vehicle_collision_damage_max;
        }
        if (hp->h_info->flags.guerrilla_tech)
        {
            damage = (float)((double)damage * 0.5);
        }
        // The player running over a friendly npc does half the friendly damage share.
        if (Player != nullptr && driver != nullptr && driver->get_sub_type() == OT_SUB_HUMAN_PLAYER && hp->get_sub_type() == OT_SUB_HUMAN_NPC &&
            human_teams_are_friendly(Player->current_team, hp->current_team))
        {
            damage = (float)((double)Damage_multiplier_player_damage_friendly * 0.5 * (double)damage);
        }
        // Magic number: house arrest hostages take at most 50 (STRESS_SOUND_MAX_DIST in rfg.exe).
        if (hp->hflags.activity_ha_hostage && (float)((double)damage - 50.0) >= 0.0f)
        {
            damage = 50.0f;
        }
        // A ragdoll the local player knocked down is the player's kill.
        human* attacker = driver;
        if ((human*)Player != hp && human_is_ragdolled(hp) && object_is_local_player(hp->ragdoll_source_human_handle))
        {
            attacker = Player;
        }
        human_damage(hp, attacker, damage, DBT_CAR, nullptr, nullptr, true, false);
        hp->ragdoll_damaged.set(750);
    }
    if (damage > 0.0f && !human_is_ragdolled(hp))
    {
        human_ragdoll_start(hp, false, get_ragdoll_car_perp_handle(vp));
        // Thrown along the car's point velocity, scaled by how head-on the hit is: 60% at the hit, 40% to the body.
        vector vel;
        havok_body_get_point_velocity(vp->havok_handle, hit_pos, vel);
        const vector dir = vel.copy_normalize_safe();
        float facing = (float)((double)dir.y * (double)hit_normal.y + (double)hit_normal.x * (double)dir.x + (double)dir.z * (double)hit_normal.z);
        if ((float)(0.0 - (double)facing) >= 0.0f)
        {
            facing = 0.0f;
        }
        vel.x = (float)((double)facing * (double)vel.x);
        vel.y = (float)((double)vel.y * (double)facing);
        vel.z = (float)((double)vel.z * (double)facing);
        vector push;
        push.x = (float)((double)vel.x * (double)0.6f);
        push.y = (float)((double)vel.y * (double)0.6f);
        push.z = (float)((double)vel.z * (double)0.6f);
        havok_ragdoll_apply_point_velocity(hp->ragdoll_idx, push, hit_pos);
        // 0x01317CD0: (double)0.39999998f, 1 - 0.6 in float.
        push.x = (float)((double)vel.x * (double)0.39999998f);
        push.y = (float)((double)vel.y * (double)0.39999998f);
        push.z = (float)((double)vel.z * (double)0.39999998f);
        havok_ragdoll_apply_velocity(hp->ragdoll_idx, push, true);
        const bool killed = hp->is_killed();
        const vector splatter_dir = -hit_normal;
        const vector splatter_pos = hit_pos + hit_normal;
        object_decal_create_blood_splatter_decal(hp, driver, splatter_pos, splatter_dir, killed, DBT_MELEE);
    }
    if ((double)rel_speed >= (double)Human_vehicle_collision_speed_low || run_over == true)
    {
        // Magic numbers: the volume is the car's speed squared over 60^2, between 0.33 and 1.
        float volume = (float)((double)car_speed_sq / 3600.0);
        if ((float)((double)volume - (double)0.33f) < 0.0f)
        {
            volume = 0.33f;
        }
        if ((float)((double)volume - 1.0) >= 0.0f)
        {
            volume = 1.0f;
        }
        hp->play_vehicle_impact_sound(hit_pos, volume, run_over);
    }
    if ((double)car_speed_sq > (double)Human_vehicle_collision_flinch_vehicle_speed * (double)Human_vehicle_collision_flinch_vehicle_speed &&
        !hp->char_inst->is_playing_action(ANIM_ALIAS_FLINCH) && !human_is_ragdolled(hp) && !human_is_dead(hp))
    {
        object_hit_location hit_location = human_get_point_hit_location(hp, hit_pos, 0.5f, nullptr);
        if (hit_location == HIT_NONE)
        {
            hit_location = HIT_GENERAL;
        }
        // A large flinch in proportion to the relative speed over the low speed (100% at it).
        int large_flinch_percent = (int)((double)rel_speed * 100.0 / (double)Human_vehicle_collision_speed_low);
        if (large_flinch_percent >= 100)
        {
            large_flinch_percent = 100;
        }
        human_combat_play_flinch(hp, hit_pos, hit_normal, hit_location, (unsigned int)large_flinch_percent);
        npc* np = World.object_cast<npc>(hp);
        if (np != nullptr)
        {
            np->get_ai()->register_hit_by_car(vp, driver);
        }
    }
}

// 0x00AD03A0 (:361, LTCG: hp1 in EDI, hp2 in ESI): hp1 runs into hp2's head (the head no more than 0.1 m above hp1's
// feet). A non-player hp2 that is not flinching is ragdolled (hit from above 15 degrees, or not standing) and thrown
// along hp1's velocity, else flinches; either way it takes damage from hp1's speed. A player and a house arrest
// hostage push each other apart instead. hp1 on something else only knocks down a human ahead of it within the reach
// of its ground contact, or within half of that reach.
static bool human_collide_knockdown(human* hp1, human* hp2)
{
    if (hp2 != nullptr && hp2->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        return false;
    }
    if (hp1->ground_obj_handle != 0xFFFFFFFF && hp1->ground_obj_handle != hp2->get_handle())
    {
        const float contact_x = (float)((double)hp1->ground_obj_contact_pos.x - (double)hp1->pos.x);
        const float contact_z = (float)((double)hp1->ground_obj_contact_pos.z - (double)hp1->pos.z);
        const float other_x = (float)((double)hp2->pos.x - (double)hp1->pos.x);
        const float other_z = (float)((double)hp2->pos.z - (double)hp1->pos.z);
        const float contact_sq = (float)((double)contact_x * (double)contact_x + (double)contact_z * (double)contact_z);
        const float other_sq = (float)((double)other_x * (double)other_x + (double)other_z * (double)other_z);
        // The flattened y components (0 * 0) are added in too.
        const float ahead = (float)((double)other_x * (double)contact_x + 0.0 * 0.0 + (double)contact_z * (double)other_z);
        const bool within_reach = ahead > 0.0f && (double)contact_sq > (double)other_sq;
        if (!within_reach && !((double)contact_sq * 0.25 > (double)other_sq))
        {
            return false;
        }
    }
    if (!human_collide_is_upright(hp2))
    {
        return false;
    }
    if (hp2->combat.flinch_timestamp.valid() && !hp2->combat.flinch_timestamp.elapsed())
    {
        return false;
    }
    if (human_in_protected_vehicle_pos(hp2))
    {
        return false;
    }
    vector head;
    human_get_head_pos(hp2, head);
    vector diff = head - hp1->pos;
    // Magic number: the head at most 0.1 m above hp1's position.
    if ((double)diff.y > (double)0.1f)
    {
        return false;
    }
    diff.normalize_safe();
    if (hp2->hflags.activity_ha_hostage)
    {
        human* pushed = nullptr;
        human* pusher = nullptr;
        if (object_is_player(hp1))
        {
            pushed = hp1;
            pusher = hp2;
        }
        else if (object_is_player(hp2))
        {
            pushed = hp2;
            pusher = hp1;
        }
        if (pushed != nullptr && pushed->rdd.is_loaded())
        {
            character_controller* cc = pushed->rdd.get_data()->controller;
            cc->flags.is_pushed = 1;
            cc->pushed_vec = pushed->pos - pusher->pos;
            pushed->rdd.get_data()->controller->pushed_vec.flatten_normalize_safe();
            return false;
        }
    }
    float damage_scale = 2.0f;
    // Magic number: 0.25881904 = sin(15 degrees).
    if (-diff.y > 0.25881904f || hp2->cur_state != HS_STANDARD)
    {
        human_ragdoll_start(hp2, false, hp1->get_handle());
        vector vel;
        vel.x = (float)((double)(float)((double)diff.x * 2.0) + (double)(float)((double)hp1->actual_velocity.x * 1.5));
        vel.y = (float)((double)(float)((double)diff.y * 2.0) + (double)(float)((double)hp1->actual_velocity.y * 1.5));
        vel.z = (float)((double)(float)((double)diff.z * 2.0) + (double)(float)((double)hp1->actual_velocity.z * 1.5));
        havok_ragdoll_apply_velocity(hp2->ragdoll_idx, vel, true);
        havok_ragdoll_apply_point_impulse(hp2->ragdoll_idx, vel, head);
        damage_scale = 5.0f;
    }
    else if (!hp2->char_inst->is_playing_action(ANIM_ALIAS_FLINCH))
    {
        // Magic number: 100% large flinch.
        human_combat_play_flinch(hp2, hp1->pos, diff, HIT_HEAD, 100);
    }
    human_damage(hp2, hp1, (float)(((double)hp1->actual_velocity.mag() + 1.0) * (double)damage_scale), DBT_MELEE, nullptr, nullptr, true, false);
    return true;
}

// 0x00A2F020 (:468): the player bumps into an npc. A hostile or unfriendly npc notices it (at most every 500 ms); a
// squad member the player walks toward steps aside (at most every second), out of cover along the cover vector.
static void player_collide_npc(player* pp, npc* np)
{
    if (np != nullptr && pp != nullptr && (human*)np != (human*)pp &&
        (int)human_teams_get_disposition_internal(np->current_team, pp->current_team) <= TEAM_DISPOSITION_UNFRIENDLY &&
        pp->bump_into_propagate_time.elapsed())
    {
        pp->bump_into_propagate_time.set(500);
        np->get_ai()->perception.register_bumped_into(pp);
    }
    // Original: np is used unchecked from here on.
    if (np->avoidance_check_timer.valid() && !np->avoidance_check_timer.elapsed())
    {
        return;
    }
    np->avoidance_check_timer.set(1000);
    if (np->get_ai()->is_in_player_squad(true) != pp)
    {
        return;
    }
    vector diff;
    diff.x = np->pos.x - pp->pos.x;
    diff.y = np->pos.y - pp->pos.y;
    diff.z = np->pos.z - pp->pos.z;
    const vector dir = diff.copy_normalize_safe();
    const vector& steer = pp->steering_vector;
    if ((float)((double)steer.y * (double)dir.y + (double)steer.x * (double)dir.x + (double)steer.z * (double)dir.z) > 0.0f)
    {
        vector request_dir;
        vector hint_dir;
        if (pp->cur_state == HS_COVER)
        {
            request_dir = dir;
            hint_dir = pp->cover_vector;
        }
        else
        {
            request_dir = pp->steering_vector;
            hint_dir = np->cur_movement_state > HMS_IDLE ? np->steering_vector : dir;
        }
        np->request_avoidance(pp, request_dir, hint_dir);
    }
}

// 0x00A2F220 (:517, LTCG: first_np in ESI, second_np in EDI): first_np (pathfinding, or moving in the standard state)
// runs into second_np (standing, not prone or ragdolled), at most every second for each. Npcs of hostile teams, an
// unfriendly non-combatant or beatdown target and formation members behind the leader are left alone. When both
// paths cross, the npc farther from the crossing waits 2 s, or second_np steps aside from a path coming head-on;
// side by side paths make the one behind wait; otherwise second_np steps aside when first_np walks into it.
static void npc_collide_npc(npc* first_np, npc* second_np)
{
    if (first_np->movement_mode != HMM_PATHFINDING && (first_np->cur_state != HS_STANDARD || first_np->cur_movement_state == HMS_IDLE))
    {
        return;
    }
    if (second_np->cur_state != HS_STANDARD || second_np->cur_stance == HST_PRONE || human_is_ragdolled(second_np))
    {
        return;
    }
    ai_system* first_ai = first_np->get_ai();
    ai_system* second_ai = second_np->get_ai();
    const human_team_dispositions disposition = human_teams_get_disposition(first_np, second_np);
    if ((int)disposition <= TEAM_DISPOSITION_HOSTILE)
    {
        return;
    }
    if ((int)disposition <= TEAM_DISPOSITION_UNFRIENDLY)
    {
        if (human_team_is_non_combatant(first_np) || first_ai->perception.get_belief(AI_BELIEF_BEATDOWN_TARGET, second_np) != nullptr)
        {
            return;
        }
    }
    const unsigned int formation_handle = first_ai->get_formation_handle();
    if (formation_handle != 0xFFFFFFFF && second_ai->get_formation_handle() != 0xFFFFFFFF && formation_handle == second_ai->get_formation_handle() &&
        !ai_formation_is_leader(first_np->get_handle(), formation_handle))
    {
        return;
    }
    if (first_np->avoidance_check_timer.valid() && !first_np->avoidance_check_timer.elapsed())
    {
        return;
    }
    if (second_np->avoidance_check_timer.valid() && !second_np->avoidance_check_timer.elapsed())
    {
        return;
    }
    first_np->avoidance_check_timer.set(1000);
    second_np->avoidance_check_timer.set(1000);
    // first_np heads for its next path node, or one second along its velocity.
    vector target;
    if (first_np->pf.path != nullptr)
    {
        target = first_np->pf.path->pos;
    }
    else
    {
        target = first_np->pos + first_np->actual_velocity;
    }
    vector first_dir = target - first_np->pos;
    first_dir.normalize_safe();
    // Magic numbers: -0.9 is about head-on, 0.70710677 = cos(45 degrees).
    if (second_np->movement_mode == HMM_PATHFINDING && second_np->pf.path != nullptr)
    {
        const path_node* node = second_np->pf.path;
        vector second_dir = node->pos - second_np->pos;
        second_dir.normalize_safe();
        const vector2 first_start(first_np->pos.x, first_np->pos.z);
        const vector2 first_end(target.x, target.z);
        const vector2 second_start(second_np->pos.x, second_np->pos.z);
        const vector2 second_end(node->pos.x, node->pos.z);
        const float dot = (float)((double)second_dir.y * (double)first_dir.y + (double)second_dir.x * (double)first_dir.x + (double)second_dir.z * (double)first_dir.z);
        vector2 crossing;
        if (ix2d_linesegment_linesegment(first_start, first_end, second_start, second_end, &crossing))
        {
            const float first_dist_sq = vec2_dist_squared(first_start, crossing);
            const float second_dist_sq = vec2_dist_squared(second_start, crossing);
            if ((double)dot > (double)-0.9f)
            {
                if ((double)second_dist_sq > (double)first_dist_sq)
                {
                    second_np->do_avoidance_pause(2000);
                    return;
                }
                first_np->do_avoidance_pause(2000);
                return;
            }
            second_np->request_avoidance(first_np, -second_dir, ZERO_VECTOR);
            first_np->do_avoidance_pause(500);
            return;
        }
        if (dot > 0.70710677f)
        {
            vector to_second = second_np->pos - first_np->pos;
            to_second.normalize_safe();
            if ((float)((double)to_second.y * (double)first_dir.y + (double)to_second.x * (double)first_dir.x + (double)to_second.z * (double)first_dir.z) > 0.0f)
            {
                first_np->do_avoidance_pause(2000);
                return;
            }
            second_np->do_avoidance_pause(2000);
            return;
        }
        if ((double)-0.9f > (double)dot)
        {
            second_np->request_avoidance(first_np, -second_dir, ZERO_VECTOR);
            first_np->do_avoidance_pause(500);
        }
        return;
    }
    vector to_second = second_np->pos - first_np->pos;
    to_second.normalize_safe();
    if ((float)((double)to_second.y * (double)first_dir.y + (double)to_second.x * (double)first_dir.x + (double)to_second.z * (double)first_dir.z) > 0.70710677f)
    {
        second_np->request_avoidance(first_np, to_second, ZERO_VECTOR);
        first_np->do_avoidance_pause(500);
    }
}

// 0x00AD0850 (:659): two humans' character proxies touch (MyCharacterListener::characterInteractionCallback).
void human_collide_human(human* hp1, human* hp2)
{
    if (human_collide_knockdown(hp1, hp2))
    {
        return;
    }
    if (hp2 == nullptr || hp2->get_obj_type() != OT_HUMAN || hp2->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return;
    }
    if (hp1->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        npc_collide_npc((npc*)hp1, (npc*)hp2);
    }
    else if (hp1->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        player_collide_npc((player*)hp1, (npc*)hp2);
    }
}

// 0x00AD08A0 (:685): a moving body (falling debris, a mover) hits a human (MyCharacterListener::objectInteractionCallback
// and havok_collide_ragdoll). Bodies slower than 0.5 m/s do nothing. The damage grows with the body's speed (capped at
// Character_collision_damage_speed_max) and mass (capped at 10000): sqrt(E) / 2 + E * 0.0008 with E = speed^2 * mass,
// quartered by a facing riot shield, multiplied for a head hit and a fall onto the human, by the player multiplier, the
// backpack's damage taken factor and the super debris cheat, and by the square of how directly the body moves into
// the hit. Below Character_collision_damage_min nothing happens. In multiplayer a client sends the hits of its own
// general movers on the local player to the server, and the server applies the rest (with the debris kill info).
// Returns the damage before the 500 ms damage timer (the callers ragdoll or flinch by it).
float human_collide_mover_damage(human* hp, object* other_obj, const vector& pos, const vector& object_velocity, const vector& collision_normal)
{
    if (hp->is_killed() || other_obj->obj_flags.destroyed)
    {
        return 0.0f;
    }
    const float speed_sq = (float)((double)object_velocity.x * (double)object_velocity.x + (double)object_velocity.y * (double)object_velocity.y +
                                   (double)object_velocity.z * (double)object_velocity.z);
    if (0.25f > speed_sq || human_in_protected_vehicle_pos(hp))
    {
        return 0.0f;
    }
    float damage_taken_factor = 1.0f;
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && ((player*)hp)->backpack != nullptr)
    {
        damage_taken_factor = ((player*)hp)->backpack->get_backpack_abilities()->damage_taken_factor;
    }
    bool send_to_server = false;
    if (Multiplayer)
    {
        const bool local_mover = !other_obj->remote_obj_flags.remote_object && other_obj->get_obj_type() == OT_MOVER && other_obj->get_sub_type() == OT_SUB_MOVER_GENERAL;
        send_to_server = Multiplayer == true && !multi_is_server() && local_mover && object_is_local_player(hp);
        const bool apply = Multiplayer == true && multi_is_server() && (!local_mover || object_is_local_player(hp));
        if (Multiplayer && !send_to_server && !apply)
        {
            return 0.0f;
        }
    }
    vector dir = object_velocity;
    float speed = dir.mag_and_normalize_safe();
    if (!((float)(Character_collision_damage_speed_max - speed) >= 0.0f))
    {
        speed = Character_collision_damage_speed_max;
    }
    float mass = other_obj->get_mass();
    // Magic number: masses above 10000 count as 10000 (FAR_FROM_PLAYER_DIST_SQRD in rfg.exe).
    if (!((float)(10000.0 - (double)mass) >= 0.0f))
    {
        mass = 10000.0f;
    }
    const float energy = (float)((double)speed * (double)speed * (double)mass);
    // 0x01317CE0: (double)0.0008f.
    float damage = (float)((double)(float)sqrt((double)energy) * 0.5 + (double)energy * (double)0.0008f);
    bool shielded = false;
    if (hp->hflags.riot_shield && hp->shield_handle != 0xFFFFFFFF)
    {
        object* shield = World.object_from_handle(hp->shield_handle);
        if (shield != nullptr && shield->get_obj_type() == OT_ITEM && human_collide_is_upright(hp))
        {
            vector head;
            human_get_head_pos(hp, head);
            const vector to_hit = (pos - (hp->pos + head) * 0.5f).copy_normalize_safe();
            // Item flag 0x2000: the shield's face is its uvec, not its fvec.
            const vector facing = ((((item*)shield)->info->flags >> 13) & 1) == 0 ? shield->orient.fvec.copy_normalize_safe() : shield->orient.uvec.copy_normalize_safe();
            if (to_hit * facing >= MIN_HUMAN_IS_SHIELDED_DOT)
            {
                damage = (float)((double)damage * 0.25);
                shielded = true;
            }
        }
    }
    // How directly the body moves into the hit.
    float directness = (float)((double)collision_normal.y * (double)dir.y + (double)collision_normal.x * (double)dir.x + (double)collision_normal.z * (double)dir.z);
    if ((float)(0.0 - (double)directness) >= 0.0f)
    {
        directness = 0.0f;
    }
    vector head;
    human_get_head_pos(hp, head);
    if (!shielded)
    {
        // Magic number: a hit within sqrt(0.3) m of the head.
        const vector to_head = head - pos;
        if (!(0.3f < to_head.mag_squared()))
        {
            damage = (float)((double)Character_collision_head_multiplier * (double)damage);
            // Magic numbers: a head hit counts as at least 0.2 direct, more for bodies up to 5 m/s.
            const float t = fl_parameter(speed, 0.0f, 5.0f, true);
            const float min_directness = (float)((double)0.2f - (double)t) >= 0.0f ? min_correction_diff : t;
            if (!((float)((double)directness - (double)min_directness) >= 0.0f))
            {
                directness = min_directness;
            }
        }
    }
    // Magic numbers: a body falling (moving down at over 30 degrees) directly (over 0.8) onto the human.
    if (-0.5f > dir.y && (double)directness > (double)0.8f)
    {
        damage = (float)((double)Character_collision_falling_multiplier * (double)damage);
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        damage = (float)((double)Player_collision_damage_multiplier * (double)damage);
    }
    damage = (float)((double)damage * (double)damage_taken_factor);
    // Magic number: the super debris cheat triples it.
    if (stats_cheat_enabled(CHEATS_SUPER_DEBRIS))
    {
        damage = (float)((double)damage * 3.0);
    }
    damage = (float)((double)directness * (double)directness * (double)damage);
    if ((double)Character_collision_damage_min > (double)damage)
    {
        return 0.0f;
    }
    // Magic number: house arrest hostages take at most 50 (STRESS_SOUND_MAX_DIST in rfg.exe).
    if (hp->hflags.activity_ha_hostage && (float)((double)damage - 50.0) >= 0.0f)
    {
        damage = 50.0f;
    }
    float applied = human_collide_calculate_damage_with_timer(hp, damage);
    if (applied > 0.0f)
    {
        if (send_to_server)
        {
            multi_gw_send_collision_hit(hp, other_obj, applied);
            return 0.0f;
        }
        human* attacker = nullptr;
        const weapon_info* w_info = nullptr;
        if (Multiplayer == true)
        {
            multi_gw_get_debris_kill_info(hp, other_obj, &attacker, &w_info);
        }
        else
        {
            applied = difficulty_get_value(DIFFICULTY_PHYSICS_COLLISIONS, NUM_DIFFICULTY_LEVELS) * applied;
        }
        human_damage(hp, attacker, applied, DBT_LEVEL_OBJECT, w_info, other_obj, true, false);
    }
    return damage;
}

// 0x00A09480 (:832): empty in the shipped build.
void human_collide_level_init()
{
}

// 0x00A09490 (:849): empty in the shipped build.
void human_collide_level_shutdown()
{
}
