// Original: volition/rfg/code/objects/humans/player_autoaim.cpp
//
// The player's aim point and aim assist. player_autoaim_calculate_assisted_aiming (each gameplay frame) casts the
// reticle ray from the camera to find the local player's aim position and target (pp->aim_pos/aim_target, which a
// player's projectiles, the muzzle adjustment and the look-at target use), keeps the heat seeking target, picks the
// combat target and the best aim assist target (Last_best_target) with its fades. player_autoaim_apply_assisted_aiming
// slows and steers the camera towards that target, single player only.
// The external interface is declared in player_autoaim.h.

#include "volition/rfg/code/objects/humans/player_autoaim.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

static bool Aim_assist_slow = true; // 0x01694494
static bool Aim_assist_steer = true; // 0x01694495
bool Auto_aim_enabled = true; // 0x01694496 (the game options read and reset it)
static float Aim_assist_slow_amount = 0.5f; // 0x01694498
static float Aim_assist_steer_amount_x = 1.5f; // 0x0169449C
static float Aim_assist_steer_amount_y = 1.5f; // 0x016944A0
static float Aim_assist_dist = 45.0f; // 0x016944A4
static float Aim_assist_offset_min = 0.3f; // 0x016944A8
static float Aim_assist_offset_max = 1.0f; // 0x016944AC
static unsigned int Last_best_target = 0xFFFFFFFF; // 0x016944B0
static float RPG_locking_target_min_dot_prod; // 0x03017C58 (a tweak; 0 until the tweak table sets it)
static float RPG_locked_target_min_dot_prod; // 0x0301A228 (a tweak)
// Original: never written, so the reticle ray is the camera's forward vector.
static float Aim_reticle_elevation; // 0x0302F0C0
static float Last_best_dist; // 0x0302F0C4
static float Range_fade_percent; // 0x0302F0C8
static float Offset_fade_percent; // 0x0302F0CC
static tweak_table_item Tweak_table_item_Aim_assist_slow("Aim_assist_slow", &Aim_assist_slow); // 0x030664D8
static tweak_table_item Tweak_table_item_Aim_assist_steer("Aim_assist_steer", &Aim_assist_steer); // 0x03184790
static tweak_table_item Tweak_table_item_Aim_assist_slow_amount("Aim_assist_slow_amount", &Aim_assist_slow_amount); // 0x033B1F90
static tweak_table_item Tweak_table_item_Aim_assist_steer_amount_x("Aim_assist_steer_amount_x", &Aim_assist_steer_amount_x); // 0x03537E70
static tweak_table_item Tweak_table_item_Aim_assist_steer_amount_y("Aim_assist_steer_amount_y", &Aim_assist_steer_amount_y); // 0x030695A0
static tweak_table_item Tweak_table_item_Aim_assist_dist("Aim_assist_dist", &Aim_assist_dist); // 0x03560AA0
static tweak_table_item Tweak_table_item_Aim_assist_offset_min("Aim_assist_offset_min", &Aim_assist_offset_min); // 0x030FCE2C
static tweak_table_item Tweak_table_item_Aim_assist_offset_max("Aim_assist_offset_max", &Aim_assist_offset_max); // 0x0342A38C
static tweak_table_item Tweak_table_item_RPG_locking_target_min_dot_prod("RPG_locking_target_min_dot_prod", &RPG_locking_target_min_dot_prod); // 0x033D66C0
static tweak_table_item Tweak_table_item_RPG_locked_target_min_dot_prod("RPG_locked_target_min_dot_prod", &RPG_locked_target_min_dot_prod); // 0x034264E4

// Magic number: the chest height of a human's aim point when its bones are not transformed this frame.
static const float AUTOAIM_CHEST_HEIGHT = 1.2666667f;

static bool is_target_valid(const human* attacker_hp, const object* objp); // 0x00A5D530 (:90)
static void player_pick_combat_target(player* pp, const object* raycast_target, const human* autoaim_hp); // 0x00A34DF0 (:239)

// 0x00A868D0 (:146)
void player_autoaim_apply_assisted_aiming()
{
    // Magic numbers: the steering turns the camera by at most 0.03 rad a frame (function statics in rfg.exe).
    static float max_aim_rotate_pitch = 0.03f; // 0x016AFD68
    static float max_aim_rotate_heading = 0.03f; // 0x016AFD6C
    player* pp = Player;
    if (Multiplayer || !Auto_aim_enabled || pp == nullptr || Last_best_target == 0xFFFFFFFF)
    {
        return;
    }
    object* target_objp = World.object_from_handle(Last_best_target);
    if (target_objp == nullptr || target_objp->get_obj_type() != OT_HUMAN)
    {
        return;
    }
    const human* target = (const human*)target_objp;
    if ((pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->charge) || pp->pflags.do_sprint)
    {
        return;
    }
    if (Aim_assist_slow)
    {
        Rfg_camera.lookaround_params.rot_p_multiplier = 1.0f - (1.0f - Aim_assist_slow_amount) * Offset_fade_percent;
        Rfg_camera.lookaround_params.rot_h_multiplier = Rfg_camera.lookaround_params.rot_p_multiplier;
    }
    if (!Aim_assist_steer)
    {
        return;
    }
    vector player_vel;
    human_controller_get_vel(pp, player_vel);
    // Magic number: the camera is steered only while it turns or the player moves (0.01).
    if (fabsf(Rfg_camera.lookaround_params.rot_p) < 0.01f && fabsf(Rfg_camera.lookaround_params.rot_h) < 0.01f && fabsf(player_vel.mag()) < 0.01f)
    {
        return;
    }
    const vector local_player_vel = player_vel.copy_rotate(Rfg_camera.real_orient);
    vector target_vel;
    human_controller_get_vel(target, target_vel);
    target_vel.rotate(Rfg_camera.real_orient);
    // Magic number: fine aim steers with 75%.
    float amount = 1.0f;
    if (pp->zoom_state == PZS_FINE_AIM)
    {
        amount = 0.75f;
    }
    if (Multiplayer == true)
    {
        amount = multi_game_rubberbanding_get_stat_value(pp->mp_data, MRS_AIM_ASSIST) * amount;
    }
    const weapon* wp = pp->get_equipped_weapon();
    if (wp != nullptr)
    {
        amount = wp->w_info->aim_assist * amount;
    }
    const float fade = Range_fade_percent * Offset_fade_percent * Frametime;
    float heading = ((Aim_assist_steer_amount_x * amount * (local_player_vel.x - target_vel.x)) / Last_best_dist) * fade;
    float pitch = ((Aim_assist_steer_amount_y * amount * (local_player_vel.y - target_vel.y)) / Last_best_dist) * fade;
    if (heading < -max_aim_rotate_heading)
    {
        heading = -max_aim_rotate_heading;
    }
    if (heading > max_aim_rotate_heading)
    {
        heading = max_aim_rotate_heading;
    }
    if (pitch < -max_aim_rotate_pitch)
    {
        pitch = -max_aim_rotate_pitch;
    }
    if (pitch >= max_aim_rotate_pitch)
    {
        pitch = max_aim_rotate_pitch;
    }
    Rfg_camera.lookaround_params.heading = Rfg_camera.lookaround_params.heading - heading;
    Rfg_camera.lookaround_params.pitch = Rfg_camera.lookaround_params.pitch + pitch;
}

// 0x00A5D530 (:90): an aim assist candidate: a living, visible enemy (or force-targetable npc) on foot, or an
// occupied vehicle.
// LTCG: objp in EAX, attacker_hp on the stack (the port has no register convention).
static bool is_target_valid(const human* attacker_hp, const object* objp)
{
    if (objp == nullptr)
    {
        return false;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        const human* hp = (const human*)objp;
        if (hp == attacker_hp)
        {
            return false;
        }
        const npc* np = hp->get_sub_type() == OT_SUB_HUMAN_NPC ? (const npc*)hp : nullptr;
        if (!human_teams_are_enemies(attacker_hp, hp) && (np == nullptr || !np->npc_flags.force_targetable))
        {
            return false;
        }
        if (human_in_vehicle(hp) != nullptr || hp->hflags.hidden || !hp->char_inst->rdd.is_loaded())
        {
            return false;
        }
        return !human_is_dead(hp);
    }
    if (objp->get_obj_type() == OT_VEHICLE && vehicle_get_num_passengers((const vehicle*)objp) > 0)
    {
        return true;
    }
    return false;
}

// 0x00A34DF0 (:239): once the combat target time is up, the enemy behind the reticle's object (else the aim assist
// target) becomes the combat target; otherwise a living enemy combat target is kept, but dropped once its time is up
// and it is 5 m or more away outside the camera's 30 degree cone.
// LTCG: pp in EDI, raycast_target and autoaim_hp on the stack (the port has no register convention).
static void player_pick_combat_target(player* pp, const object* raycast_target, const human* autoaim_hp)
{
    const bool update = pp->combat_target_update_time.elapsed();
    if (update)
    {
        const human* hp = player_get_enemy_target_from_object(pp, raycast_target);
        if (hp == nullptr)
        {
            hp = autoaim_hp;
        }
        if (hp != nullptr)
        {
            pp->combat.target_handle = hp->get_handle();
            return;
        }
    }
    if (pp->combat.target_handle != 0xFFFFFFFF)
    {
        const object* objp = World.object_from_handle(pp->combat.target_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && !objp->is_killed() && human_teams_are_enemies((const human*)objp, pp))
        {
            if (update)
            {
                vector to_target(objp->pos.x - Rfg_camera.real_pos.x, objp->pos.y - Rfg_camera.real_pos.y, objp->pos.z - Rfg_camera.real_pos.z);
                const float dist = to_target.mag_and_normalize_safe();
                // Magic numbers: 5 m and cos(30 degrees).
                if (dist >= 5.0f && Rfg_camera.real_orient.fvec.x * to_target.x + Rfg_camera.real_orient.fvec.y * to_target.y + Rfg_camera.real_orient.fvec.z * to_target.z < 0.8660254f)
                {
                    pp->combat.target_handle = 0xFFFFFFFF;
                    return;
                }
            }
            return;
        }
    }
    pp->combat.target_handle = 0xFFFFFFFF;
}

// 0x00A5D5E0 (:302)
void player_autoaim_calculate_assisted_aiming()
{
    player* pp = Player;
    if (pp == nullptr || pp->is_killed())
    {
        return;
    }
    weapon* wp = human_weapons_get_weapon(pp);
    // Magic number: 10 m of aim without a weapon.
    float aim_distance = 10.0f;
    bool heat_seeking = false;
    if (wp != nullptr)
    {
        aim_distance = Rfg_camera.m_far_clip_dist;
        // Projectile flag 1: a rocket (the heat seeking upgrade locks on).
        if (upgrade_has_heat_seeking_rockets() && (wp->w_info->projectile_info.flags & 1) != 0)
        {
            heat_seeking = true;
        }
    }
    matrix aim_orient(Rfg_camera.real_orient);
    aim_orient.rotate_around_local_x(Aim_reticle_elevation);
    aim_orient.fvec.normalize_safe();
    if (camera_get_mode() != CAMERA_TURRET_MODE && camera_get_mode() != CAMERA_SATELLITE_MODE)
    {
        // The reticle ray starts level with the player (the camera's horizontal distance to him along the ray), so
        // nothing between the camera and the player is aimed at.
        const float player_dx = pp->pos.x - Rfg_camera.real_pos.x;
        const float player_dz = pp->pos.z - Rfg_camera.real_pos.z;
        const float player_dist = sqrtf(player_dz * player_dz + player_dx * player_dx);
        const float flat_mag = sqrtf(aim_orient.fvec.x * aim_orient.fvec.x + aim_orient.fvec.z * aim_orient.fvec.z);
        vector start;
        start.x = (player_dist * aim_orient.fvec.x) / flat_mag + Rfg_camera.real_pos.x;
        start.y = (aim_orient.fvec.y * player_dist) / flat_mag + Rfg_camera.real_pos.y;
        start.z = (aim_orient.fvec.z * player_dist) / flat_mag + Rfg_camera.real_pos.z;
        weapon_fire_info fire_info;
        fire_info.end_pos.x = aim_orient.fvec.x * aim_distance + start.x;
        fire_info.end_pos.y = aim_orient.fvec.y * aim_distance + start.y;
        fire_info.end_pos.z = aim_orient.fvec.z * aim_distance + start.z;
        fire_info.hit_handle = 0xFFFFFFFF;
        fire_info.w_info = nullptr;
        if (wp != nullptr)
        {
            fire_info.w_info = wp->w_info;
        }
        fire_info.start_pos = start;
        if (weapons_test_bullet(fire_info, pp, true, 0.0f, vector(0.0f, 0.0f, 0.0f)))
        {
            if (wp == nullptr || !wp->w_info->flags.penetrating_bullets)
            {
                const object* hit = World.object_from_handle(fire_info.hit_handle);
                if (hit == nullptr || hit->get_obj_type() != OT_HUMAN || !human_is_dead((const human*)hit))
                {
                    pp->aim_target = fire_info.hit_handle;
                }
                else
                {
                    pp->aim_target = 0xFFFFFFFF;
                }
                pp->aim_pos = fire_info.hit_pos;
            }
            else
            {
                // A penetrating weapon aims at the target the last penetrating shot found.
                pp->aim_target = pp->penetrating_aim_target;
                if (World.object_from_handle(pp->penetrating_aim_target) == nullptr)
                {
                    pp->aim_pos = fire_info.end_pos;
                }
                else
                {
                    pp->aim_pos = pp->penetrating_aim_pos;
                }
                pp->penetrating_aim_target = 0xFFFFFFFF;
            }
        }
        else
        {
            pp->aim_pos = fire_info.end_pos;
            pp->aim_target = 0xFFFFFFFF;
        }
        bool keep_lock = false;
        if (heat_seeking)
        {
            object* lock_objp = World.object_from_handle(pp->heat_seeking_target);
            bool friendly = false;
            bool dead = false;
            if (lock_objp == nullptr)
            {
                pp->heat_seeking_target = 0xFFFFFFFF;
                pp->aim_target_duration.set(0);
            }
            else
            {
                if (lock_objp->get_obj_type() == OT_HUMAN)
                {
                    dead = human_is_dead((const human*)lock_objp);
                    friendly = human_teams_are_friendly(pp, (const human*)lock_objp);
                }
                if (lock_objp->get_obj_type() == OT_VEHICLE)
                {
                    const vehicle* vp = (const vehicle*)lock_objp;
                    dead = vehicle_is_corpse(vp);
                    // An empty vehicle counts as friendly.
                    friendly = vehicle_get_num_passengers(vp) > 0 ? human_teams_are_friendly(pp, vp->team_of_most_recent_driver) : true;
                }
                if (dead || friendly)
                {
                    pp->heat_seeking_target = 0xFFFFFFFF;
                    pp->aim_target_duration.set(0);
                }
            }
            if (pp->heat_seeking_target != pp->aim_target)
            {
                const object* aim_objp = World.object_from_handle(pp->aim_target);
                if (aim_objp == nullptr || (aim_objp->get_obj_type() != OT_HUMAN && aim_objp->get_obj_type() != OT_VEHICLE) || pp->target_is_locked())
                {
                    // Original: the lock is kept on the previous target object even when it was just found dead or
                    // friendly above (lock_objp is not refetched).
                    if (lock_objp != nullptr && (lock_objp->get_obj_type() == OT_HUMAN || lock_objp->get_obj_type() == OT_VEHICLE))
                    {
                        const float min_dot = pp->target_is_locked() ? RPG_locked_target_min_dot_prod : RPG_locking_target_min_dot_prod;
                        const vector to_target(lock_objp->pos.x - start.x, lock_objp->pos.y - start.y, lock_objp->pos.z - start.z);
                        if (min_dot <= vec_dot_to_point(to_target.copy_normalize(), start, pp->aim_pos))
                        {
                            keep_lock = true;
                        }
                    }
                    if (!keep_lock)
                    {
                        pp->heat_seeking_target = 0xFFFFFFFF;
                        pp->aim_target_duration.set(0);
                    }
                }
                else
                {
                    pp->aim_target_duration.set(0);
                    pp->heat_seeking_target = pp->aim_target;
                }
            }
        }
        else
        {
            pp->heat_seeking_target = 0xFFFFFFFF;
            pp->aim_target_duration.set(0);
        }
    }
    vector player_chest(pp->pos.x + UP_VECTOR.x * AUTOAIM_CHEST_HEIGHT, pp->pos.y + UP_VECTOR.y * AUTOAIM_CHEST_HEIGHT, pp->pos.z + UP_VECTOR.z * AUTOAIM_CHEST_HEIGHT);
    if (pp->hflags.bones_transformed_this_frame && pp->char_inst->aim_handle >= 0)
    {
        animlib_get_tag_world_coords(pp->char_inst->aim_handle, pp->h_info->upper_spine_bone_index, pp->pos, pp->orient, player_chest, nullptr);
    }
    const float player_cam_dist = sqrtf((player_chest.y - Rfg_camera.real_pos.y) * (player_chest.y - Rfg_camera.real_pos.y) + (player_chest.x - Rfg_camera.real_pos.x) * (player_chest.x - Rfg_camera.real_pos.x) +
                                        (player_chest.z - Rfg_camera.real_pos.z) * (player_chest.z - Rfg_camera.real_pos.z));
    const float aim_dx = Rfg_camera.real_pos.x - pp->aim_pos.x;
    const float aim_dy = Rfg_camera.real_pos.y - pp->aim_pos.y;
    const float aim_dz = Rfg_camera.real_pos.z - pp->aim_pos.z;
    const float aim_pos_dist = sqrtf(aim_dy * aim_dy + aim_dx * aim_dx + aim_dz * aim_dz);
    float best_offset = 0.0f;
    float best_dist = Aim_assist_dist;
    human* best = nullptr;
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = World.get_object<human>(i);
        vector chest(UP_VECTOR.x * AUTOAIM_CHEST_HEIGHT + hp->pos.x, hp->pos.y + UP_VECTOR.y * AUTOAIM_CHEST_HEIGHT, hp->pos.z + UP_VECTOR.z * AUTOAIM_CHEST_HEIGHT);
        if (hp->hflags.bones_transformed_this_frame && hp->char_inst->aim_handle >= 0)
        {
            animlib_get_tag_world_coords(hp->char_inst->aim_handle, hp->h_info->upper_spine_bone_index, hp->pos, hp->orient, chest, nullptr);
        }
        if (!is_target_valid(pp, hp))
        {
            continue;
        }
        // Not a ragdoll (but blending out, flinching, or ragdolling on impact at the animated position).
        const human_ragdoll_state rs = hp->ragdoll_state;
        if (rs != HUMAN_RAGDOLL_STATE_BLEND_OUT && rs != HUMAN_RAGDOLL_STATE_NONE && rs != HUMAN_RAGDOLL_STATE_FLINCH &&
            (rs != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->hflags.ragdoll_on_impact_use_ragdoll_pos))
        {
            continue;
        }
        vector to_chest(chest.x - Rfg_camera.real_pos.x, chest.y - Rfg_camera.real_pos.y, chest.z - Rfg_camera.real_pos.z);
        const float dist = to_chest.mag_and_normalize_safe();
        // Only targets beyond the player.
        if (dist < player_cam_dist)
        {
            continue;
        }
        float dot = to_chest.x * aim_orient.fvec.x + to_chest.y * aim_orient.fvec.y + to_chest.z * aim_orient.fvec.z;
        if (dot < -1.0f)
        {
            dot = -1.0f;
        }
        if (dot < 1.0f)
        {
            if (dot <= 0.0f)
            {
                continue;
            }
        }
        else
        {
            dot = 1.0f;
        }
        // The squared distance of the target from the reticle ray (tan^2 * dist^2).
        const float offset_sq = (1.0f / (dot * dot) - 1.0f) * dist * dist;
        if (offset_sq < Aim_assist_offset_max * Aim_assist_offset_max && dist < best_dist &&
            (dist < aim_pos_dist || pp->aim_target == 0xFFFFFFFF || hp->get_handle() == pp->aim_target))
        {
            best_offset = 1.0f - (sqrtf(offset_sq) - Aim_assist_offset_min) / (Aim_assist_offset_max - Aim_assist_offset_min);
            if (best_offset < 0.0f)
            {
                best_offset = 0.0f;
            }
            best = hp;
            best_dist = dist;
            if (best_offset >= 1.0f)
            {
                best_offset = 1.0f;
            }
        }
    }
    if (best != nullptr)
    {
        Last_best_target = best->get_handle();
        Last_best_dist = best_dist;
    }
    player_pick_combat_target(pp, World.object_from_handle(pp->aim_target), best);
    unsigned int weapon_handle = 0xFFFFFFFF;
    bool turret_weapon = false;
    if (pp->combat.reserved_turret != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(pp->combat.reserved_turret);
        if (objp != nullptr && objp->get_obj_type() == OT_TURRET)
        {
            const turret* tp = (const turret*)objp;
            object* op = tp->current_operator != 0xFFFFFFFF ? World.object_from_handle(tp->current_operator) : nullptr;
            if (op != nullptr && op->get_obj_type() != OT_HUMAN)
            {
                op = nullptr;
            }
            if (op == pp)
            {
                weapon_handle = tp->primary_weapon_handle;
                turret_weapon = true;
            }
        }
    }
    if (!turret_weapon)
    {
        if (pp->equipped_inv_item == nullptr)
        {
            Range_fade_percent = 0.0f;
            Offset_fade_percent = 0.0f;
            return;
        }
        weapon_handle = pp->equipped_inv_item->weapon_handle;
    }
    object* weapon_objp = weapon_handle != 0xFFFFFFFF ? World.object_from_handle(weapon_handle) : nullptr;
    if (weapon_objp != nullptr && (weapon_objp->get_obj_type() != OT_ITEM || weapon_objp->get_sub_type() != OT_SUB_ITEM_WEAPON))
    {
        weapon_objp = nullptr;
    }
    const weapon* assist_wp = (const weapon*)weapon_objp;
    if (assist_wp == nullptr || assist_wp->w_info == nullptr || pp->zoom_state == PZS_SCOPE)
    {
        Offset_fade_percent = 0.0f;
        Range_fade_percent = 0.0f;
        return;
    }
    const weapon_class_type wc = assist_wp->w_info->weapon_class;
    if (wc != WEAPON_CLASS_BULLET && wc != WEAPON_CLASS_LAUNCHER && wc != WEAPON_CLASS_THROWN_MINE && wc != WEAPON_CLASS_THROWN_GRENADE && wc != WEAPON_CLASS_MELEE && wc != WEAPON_CLASS_SLEDGEHAMMER)
    {
        Offset_fade_percent = 0.0f;
        Range_fade_percent = 0.0f;
        return;
    }
    if (best != nullptr && best->get_sub_type() == OT_SUB_HUMAN_PLAYER && ((player*)best)->backpack != nullptr && ((player*)best)->backpack->is_fully_stealthed())
    {
        // Magic number: a fully stealthed player gets a quarter of the assist.
        Offset_fade_percent = 0.25f;
        Range_fade_percent = 1.0f;
        return;
    }
    if (best_offset < 0.0f)
    {
        best_offset = 0.0f;
    }
    if (best_offset >= 1.0f)
    {
        best_offset = 1.0f;
    }
    Offset_fade_percent = best_offset;
    Range_fade_percent = 1.0f - Last_best_dist / Aim_assist_dist;
    if (Range_fade_percent < 0.0f)
    {
        Range_fade_percent = 0.0f;
    }
    if (Range_fade_percent >= 1.0f)
    {
        Range_fade_percent = 1.0f;
    }
}

// 0x00A114D0 (:540): empty in the retail build.
void player_autoaim_render_debug()
{
}

// 0x00A114E0 (:589)
vector player_autoaim_get_aim_vector()
{
    matrix aim_orient(Rfg_camera.real_orient);
    aim_orient.rotate_around_local_x(Aim_reticle_elevation);
    aim_orient.fvec.normalize_safe();
    return aim_orient.fvec;
}

// 0x00A11540 (:597)
float player_get_aim_assist_dist()
{
    return Aim_assist_dist;
}

// 0x00A11550 (:602)
void player_set_autoaim_enabled(bool enabled)
{
    Auto_aim_enabled = enabled;
}

// 0x00A11560 (:607)
bool player_get_autoaim_enabled()
{
    return Auto_aim_enabled;
}
