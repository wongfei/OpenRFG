// Original: volition/rfg/code/ai/hazard_detection.cpp
//
// The static functions are LTCG custom conventions in rfg.exe; each one notes its register mapping, read from the asm of
// the function and its callers.

#include "volition/rfg/code/ai/hazard_detection.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/vlib/code/math/vector2.h"

#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>

static float Civilian_avoid_player_thrown_weapon_dist; // 0x01D65978
static tweak_table_item Tweak_table_item_Civilian_avoid_player_thrown_weapon_dist("Civilian_avoid_player_thrown_weapon_dist", &Civilian_avoid_player_thrown_weapon_dist);
static float MAX_HOSTILE_VEHICLE_EVADE_CONE_LENGTH = 22.5f; // 0x01642EE4
static float MAX_HOSTILE_VEHICLE_EVADE_CONE_RADIUS = 8.75f; // 0x01642EE8
static const vector GRAVITY_COMPONENT(0.0f, -4.9f, 0.0f); // 0x01642EEC: half of g, the t^2 term

// 0x005EAF90. LTCG: info ESI, obj EDI; np unused and not passed, xz_distance and radius on the stack.
static bool get_rolling_physics_info(hazard_physics_info& info, const object* obj, float xz_distance, float radius)
{
    if (info.obj_to_npc.y / xz_distance > 0.5f)
    {
        return false;
    }
    if (info.obj_to_npc.y * info.velocity.y + info.velocity.x * info.obj_to_npc.x + info.obj_to_npc.z * info.velocity.z < 0.0f)
    {
        return false;
    }

    vector2 direction(info.velocity.x, info.velocity.z);
    const float speed = direction.mag_and_normalize_safe();
    if (speed < 4.0f)
    {
        return false;
    }

    const float along = direction.y * info.obj_to_npc.z + direction.x * info.obj_to_npc.x;
    const float to_path_x = direction.x * along - info.obj_to_npc.x;
    const float to_path_z = direction.y * along - info.obj_to_npc.z;
    info.pass_point = obj->pos;
    info.pass_point.x += to_path_x;
    info.pass_point.z += to_path_z;
    info.distance_at_pass = sqrtf(to_path_z * to_path_z + to_path_x * to_path_x);
    info.time_to_pass = xz_distance / speed;
    info.will_impact = radius >= info.distance_at_pass;
    return true;
}

// 0x005EB170. LTCG: info EAX, eff_pos ECX, radius on the stack; np and obj unused and not passed.
static bool get_straight_trajectory_physics_info(hazard_physics_info& info, const vector& eff_pos, float radius)
{
    if (fabsf(info.distance) < 0.0001f || fabsf(info.speed) < 0.0001f)
    {
        return false;
    }
    const float cosine = (info.obj_to_npc.y * info.velocity.y + info.velocity.x * info.obj_to_npc.x + info.obj_to_npc.z * info.velocity.z) / (info.speed * info.distance);
    if (cosine <= 0.0001f)
    {
        return false;
    }

    const float time = info.distance / (info.speed * cosine);
    const float travelled = time * info.speed;
    float miss_squared = travelled * travelled - info.distance * info.distance;
    info.time_to_pass = time;
    if (miss_squared < 0.0f)
    {
        miss_squared = 0.0f;
    }
    info.distance_at_pass = sqrtf(miss_squared);
    info.pass_point.x = eff_pos.x + info.velocity.x * time;
    info.pass_point.y = eff_pos.y + info.velocity.y * time;
    info.pass_point.z = eff_pos.z + info.velocity.z * time;
    info.will_impact = radius >= info.distance_at_pass;
    return true;
}

// 0x00638E00. LTCG: info ESI, obj ECX, np and radius on the stack.
static bool get_ballistic_physics_info(hazard_physics_info& info, const npc* np, const object* obj, float radius)
{
    float times[3];
    vector points[3];
    int count = 0;

    const float vy = info.velocity.y;
    const float inverse_gravity = -0.10204081f; // 1 / -9.8
    const auto point_at = [&](float t) {
        const float t2 = t * t;
        vector p;
        p.x = info.velocity.x * t + obj->pos.x + t2 * GRAVITY_COMPONENT.x;
        p.y = vy * t + obj->pos.y + t2 * GRAVITY_COMPONENT.y;
        p.z = info.velocity.z * t + obj->pos.z + t2 * GRAVITY_COMPONENT.z;
        return p;
    };

    // Times the falling object passes the npc's height: obj_to_npc.y = vy t - 4.9 t^2.
    const float discriminant = vy * vy - -info.obj_to_npc.y * -19.6f;
    if (discriminant == 0.0f)
    {
        const float t = -vy * inverse_gravity;
        if (t > 0.0f)
        {
            times[count] = t;
            points[count] = point_at(t);
            count++;
        }
    }
    else if (discriminant > 0.0f)
    {
        const float root = sqrtf(discriminant);
        const float t = (-vy - root) * inverse_gravity;
        if (t > 0.0f)
        {
            times[count] = t;
            points[count] = point_at(t);
            count++;
            const float t2 = (root - vy) * inverse_gravity;
            if (t2 > 0.0f)
            {
                times[count] = t2;
                points[count] = point_at(t2);
                count++;
            }
        }
    }

    // The closest approach in the plane.
    vector flat = info.velocity;
    const float flat_speed = flat.flatten_mag_and_normalize_safe();
    if (flat_speed >= 0.1f)
    {
        const float along = info.obj_to_npc.y * flat.y + flat.x * info.obj_to_npc.x + info.obj_to_npc.z * flat.z;
        if (along > 0.0f)
        {
            const float t = along / flat_speed;
            times[count] = t;
            points[count] = point_at(t);
            count++;
        }
    }

    vector center;
    np->get_center_pos(center);
    int best = -1;
    float best_dist = 0.0f;
    for (int i = 0; i < count; i++)
    {
        const float dist = vec_dist_squared(center, points[i]);
        if (best == -1 || dist < best_dist)
        {
            best = i;
            best_dist = dist;
        }
    }
    if (best == -1)
    {
        return false;
    }
    info.will_impact = best_dist <= radius * radius;
    info.time_to_pass = times[best];
    info.pass_point = points[best];
    info.distance_at_pass = sqrtf(best_dist);
    return true;
}

// 0x006395D0. LTCG: info EAX, mov EDI, xz_distance XMM1, np on the stack.
static bool get_mover_hazard_info(hazard_physics_info& info, const npc* np, const object_mover* mov, float xz_distance)
{
    if (info.obj_to_npc.y / xz_distance > 0.5f)
    {
        return false;
    }
    vector bmin, bmax;
    mov->get_local_bbox(bmin, bmax);
    const float min_size = sqrtf(bmin.x * bmin.x + bmin.y * bmin.y + bmin.z * bmin.z);
    float size = sqrtf(bmax.x * bmax.x + bmax.y * bmax.y + bmax.z * bmax.z);
    if (size - min_size < 0.0f)
    {
        size = min_size;
    }
    return get_ballistic_physics_info(info, np, mov, size * 1.5f + 1.0f);
}

// 0x006396E0
static bool get_projectile_hazard_info(hazard_physics_info& info, const npc* np, const item* ip)
{
    if (info.speed < 0.1f)
    {
        return false;
    }

    const projectile* pp = (ip != nullptr && ip->get_obj_type() == OT_ITEM && ip->get_sub_type() == OT_SUB_ITEM_PROJECTILE) ? static_cast<const projectile*>(ip) : nullptr;
    const weapon* wp = (ip != nullptr && ip->get_obj_type() == OT_ITEM && ip->get_sub_type() == OT_SUB_ITEM_WEAPON) ? static_cast<const weapon*>(ip) : nullptr;
    const weapon_info* w_info;
    if (pp != nullptr)
    {
        w_info = pp->w_info;
    }
    else if (wp != nullptr)
    {
        w_info = wp->w_info;
    }
    else
    {
        return false;
    }

    float radius = 5.0f;
    if (w_info->is_explosive())
    {
        const float blast = w_info->m_explosion_info->m_knockdown_radius + 1.0f;
        if (5.0f - blast < 0.0f)
        {
            radius = blast;
        }
    }
    if (w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN && human_team_is_non_combatant(np) && object_is_player(ip->owner_handle)
        && radius - Civilian_avoid_player_thrown_weapon_dist >= 0.0f)
    {
        radius = Civilian_avoid_player_thrown_weapon_dist;
    }

    // rfg.exe also fetches the npc's center here and does not use it.
    if (pp != nullptr && pp->w_info->weapon_class == WEAPON_CLASS_LAUNCHER && pp->pflags.propellant_started && !pp->pflags.propellant_expired)
    {
        return get_straight_trajectory_physics_info(info, pp->pos, radius);
    }
    return get_ballistic_physics_info(info, np, ip, radius);
}

// 0x006616E0. LTCG: info EAX, the rest on the stack.
static bool get_vehicle_hazard_info(hazard_physics_info& info, const npc* np, const vehicle* vp, float xz_distance)
{
    const player* local = player_get_local_player();
    if (local != nullptr && vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == local && local_player_is_friendly(np)
        && vec_xz_dist_squared(np->pos, vp->pos) < 100.0f)
    {
        // Friends near the player's own vehicle treat slow driving as three times as fast, up to 15.
        if (info.speed < 15.0f)
        {
            float boosted = info.speed * 3.0f;
            if (15.0f - boosted < 0.0f)
            {
                boosted = 15.0f;
            }
            if (boosted > 0.0f)
            {
                const float scale = boosted / info.speed;
                info.velocity.x *= scale;
                info.velocity.y *= scale;
                info.velocity.z *= scale;
            }
            else
            {
                info.velocity = ZERO_VECTOR;
            }
            info.speed = boosted;
        }
    }
    else if (info.speed < 5.0f)
    {
        return false;
    }

    float npc_ground, vehicle_ground;
    havok_terrain_height(npc_ground, np->pos.x, np->pos.z, false, nullptr);
    havok_terrain_height(vehicle_ground, vp->pos.x, vp->pos.z, false, nullptr);

    // ai + 0x228 is belief_bins[0].beliefs (perception at 0x18, bins at 0x20C, 8 bytes each).
    bool hostile_driver = np->get_ai()->perception.get_belief(AI_BELIEF_ENEMY_EXISTS, vp->most_recent_driver) != nullptr;

    const bool heavy = vp->get_sub_type() == OT_SUB_VEHICLE_WALKER || vehicle_is_tank(vp);
    const bool non_combatant = human_team_is_non_combatant(np);

    if (np->get_manager() == NPC_MANAGER_AMBIENT && np->vehicle_handle == 0xFFFFFFFF)
    {
        const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            hostile_driver = true;
        }
    }

    float height, half_width;
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        height = 5.0f;
        half_width = 2.5f;
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER && vp->get_obj_type() == OT_VEHICLE && static_cast<const flyer*>(vp)->wing_is_detached)
    {
        height = 5.0f;
        half_width = 20.0f;
    }
    else
    {
        vector bmin, bmax;
        vp->get_local_bbox(bmin, bmax);
        height = bmax.y - bmin.y;
        half_width = (bmax.x - bmin.x) * 0.5f;
    }

    vector center;
    np->get_center_pos(center);
    if (vp->pos.y - vehicle_ground <= 1.9f)
    {
        if (height < center.y - npc_ground || !get_rolling_physics_info(info, vp, xz_distance, half_width * 1.2f))
        {
            return false;
        }
        if ((hostile_driver && !info.will_impact && !heavy) || (non_combatant && heavy))
        {
            float cone_length = info.speed * 2.5f;
            const float pass_distance = info.time_to_pass * info.speed;
            if (MAX_HOSTILE_VEHICLE_EVADE_CONE_LENGTH - cone_length < 0.0f)
            {
                cone_length = MAX_HOSTILE_VEHICLE_EVADE_CONE_LENGTH;
            }
            if (pass_distance <= cone_length && info.distance_at_pass <= (pass_distance / cone_length) * MAX_HOSTILE_VEHICLE_EVADE_CONE_RADIUS)
            {
                info.will_impact = true;
            }
        }
    }
    else
    {
        const float radius = half_width * 1.5f + 1.0f;
        if (vp->get_supporting_rigid_body() != nullptr)
        {
            if (!get_straight_trajectory_physics_info(info, vp->pos, radius))
            {
                return false;
            }
        }
        else if (!get_ballistic_physics_info(info, np, vp, radius))
        {
            return false;
        }
        if (info.will_impact)
        {
            np->get_center_pos(center);
            const float above = info.pass_point.y - center.y;
            if (above > 1.9f || -above > height + 1.9f)
            {
                info.will_impact = false;
            }
        }
    }

    if (info.will_impact)
    {
        float reaction_time = 1.5f;
        // 0xC98 compared with 3.
        if (hostile_driver && np->current_team != HUMAN_TEAM_MARAUDER)
        {
            if (!heavy)
            {
                reaction_time = 2.5f;
            }
        }
        else if (non_combatant && heavy)
        {
            reaction_time = 2.5f;
        }
        if (info.time_to_pass > reaction_time)
        {
            info.will_impact = false;
        }
    }
    return true;
}

// 0x006398B0. LTCG: obj EAX, vel ESI.
static bool get_hazard_velocity(vector& vel, const object* obj)
{
    if (obj->get_obj_type() == OT_VEHICLE)
    {
        vehicle_get_velocity(static_cast<const vehicle*>(obj), vel);
        return true;
    }
    if (obj->get_obj_type() == OT_ITEM && obj->get_sub_type() == OT_SUB_ITEM_PROJECTILE && obj->havok_handle == 0xFFFFFFFF)
    {
        const projectile* pp = static_cast<const projectile*>(obj);
        if (pp->w_info->weapon_class != WEAPON_CLASS_LAUNCHER)
        {
            return false;
        }
        vel = pp->get_start_velocity();
        return true;
    }
    if (obj->havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    havok_body_get_linear_velocity(obj->havok_handle, vel);
    return true;
}

// 0x005EB3A0 (:713)
void get_hazard_mover_physics_info(hazard_mover_physics_info& info, const npc* np, const object* obj)
{
    vector velocity;
    havok_body_get_linear_velocity(obj->havok_handle, velocity);
    info.mass = obj->get_mass();
    vector center;
    np->get_center_pos(center);
    info.potential_energy = (center.y - obj->pos.y) * info.mass * -9.8f;
    info.kinetic_energy = (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z) * info.mass * 0.5f;
    info.total_energy = info.potential_energy + info.kinetic_energy;
}

// 0x00661CA0 (:732)
void get_hazard_physics_info(hazard_physics_info& info, const npc* np, const object* obj)
{
    vector center;
    np->get_center_pos(center);
    info.obj_to_npc.x = center.x - obj->pos.x;
    info.obj_to_npc.y = center.y - obj->pos.y;
    info.obj_to_npc.z = center.z - obj->pos.z;
    info.distance = sqrtf(info.obj_to_npc.x * info.obj_to_npc.x + info.obj_to_npc.y * info.obj_to_npc.y + info.obj_to_npc.z * info.obj_to_npc.z);

    if (!get_hazard_velocity(info.velocity, obj))
    {
        info.velocity = ZERO_VECTOR;
        info.time_to_pass = -1.0f;
        info.distance_at_pass = -1.0f;
        info.speed = 0.0f;
        info.will_impact = false;
        return;
    }
    info.speed = sqrtf(info.velocity.x * info.velocity.x + info.velocity.y * info.velocity.y + info.velocity.z * info.velocity.z);

    const float xz_distance = vec_xz_dist(np->pos, obj->pos);
    bool valid;
    switch (obj->get_obj_type())
    {
    case OT_ITEM:
        valid = get_projectile_hazard_info(info, np, static_cast<const item*>(obj));
        break;
    case OT_MOVER:
        valid = get_mover_hazard_info(info, np, static_cast<const object_mover*>(obj), xz_distance);
        break;
    case OT_VEHICLE:
        valid = get_vehicle_hazard_info(info, np, static_cast<const vehicle*>(obj), xz_distance);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        valid = false;
        break;
    }
    if (!valid)
    {
        info.time_to_pass = -1.0f;
        info.distance_at_pass = -1.0f;
        info.will_impact = false;
    }
}
