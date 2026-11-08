// Original: volition/rfg/code/ai/combat/ai_thrown_weapon_logic.cpp

#include "volition/rfg/code/ai/combat/ai_thrown_weapon_logic.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <math.h>

// Read from rfg.exe's .data. The throw start and roll offsets are in the npc's local space.
static const vector STAND_LOCAL_THROW_OFFSET(0.3f, 2.0f, 0.4f); // 0x016425D0
static const vector CROUCH_LOCAL_THROW_OFFSET(-0.15f, 1.5f, 0.3f); // 0x016425DC
static const vector LOCAL_ROLL_OFFSET(0.3f, 0.7f, 0.7f); // 0x016425E8
static const vector COVER_LOCAL_THROW_OFFSETS[4] = // 0x016425F4, by cover_node_stances
{
    vector(-1.0f, 1.2f, 0.0f),
    vector(1.0f, 1.2f, 0.0f),
    vector(0.0f, 1.2f, 0.0f),
    vector(0.0f, 1.2f, 0.0f),
};
static const float MAX_RAYCAST_HIT_TARGET_DIST_SQ = 1.0f; // 0x01642624

struct ai_throw_error_info
{
    float displacement_min; // m, along the throw
    float displacement_max;
    float angle_dot; // cosine of the largest angle off the throw direction
};

// The dynamic initializer in rfg.exe takes cosines of floats widened to double (0x01062840).
static const float THROW_ERROR_7_5_DEGREES = (float)cos((double)0.13089969f);
static const float THROW_ERROR_5_DEGREES = (float)cos((double)0.08726646f);
static const float THROW_ERROR_3_DEGREES = (float)cos((double)0.05235988f);

static const ai_throw_error_info Throw_error_table[NUM_AI_THROW_SITUATIONS][NUM_AI_THROW_TYPES] = // 0x01642FC0
{
    // LOB, DIRECT, BOUNCE, ROLL
    {{-2.5f, 1.5f, THROW_ERROR_7_5_DEGREES}, {0.0f, 0.0f, THROW_ERROR_5_DEGREES}, {-2.0f, 1.0f, THROW_ERROR_5_DEGREES}, {-2.0f, 1.0f, THROW_ERROR_5_DEGREES}},
    {{-1.0f, 1.5f, THROW_ERROR_3_DEGREES}, {0.0f, 0.0f, THROW_ERROR_3_DEGREES}, {-1.0f, 1.5f, THROW_ERROR_3_DEGREES}, {-1.0f, 1.5f, THROW_ERROR_3_DEGREES}},
    {{-1.5f, 1.5f, THROW_ERROR_7_5_DEGREES}, {0.0f, 0.0f, THROW_ERROR_5_DEGREES}, {-1.0f, 1.0f, THROW_ERROR_5_DEGREES}, {-0.5f, 0.5f, THROW_ERROR_3_DEGREES}},
};

// 0x00657270
ai_thrown_weapon_info::ai_thrown_weapon_info()
{
}

// 0x006572A0
ai_thrown_weapon_info::~ai_thrown_weapon_info()
{
}

// 0x005E23A0 (:300). LTCG: np EAX, return slot ESI.
static vector get_npc_throw_pos(const npc* np)
{
    vector offset;
    if (human_combat_cover_is_using_node(np))
    {
        const cover_node_stances stance = cover_stance_from_state(np);
        if (stance < 0)
        {
            return np->pos;
        }
        offset = COVER_LOCAL_THROW_OFFSETS[stance];
    }
    else if (np->cur_stance == HST_STAND)
    {
        offset = STAND_LOCAL_THROW_OFFSET;
    }
    else if (np->cur_stance == HST_CROUCH)
    {
        offset = CROUCH_LOCAL_THROW_OFFSET;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return np->pos;
    }
    vec_change_to_global_coords(offset, np->pos, np->orient);
    return offset;
}

// 0x005E24A0 (:335). LTCG: np EAX, return slot ESI. calculate_roll_throw inlines it.
static vector get_npc_roll_pos(const npc* np)
{
    vector pos = LOCAL_ROLL_OFFSET;
    vec_change_to_global_coords(pos, np->pos, np->orient);
    return pos;
}

// 0x006923F0 (:352). LTCG: info EDI. Written from the asm: the decompile shifts every argument and shows a wrong struct.
static bool init_thrown_weapon_info(ai_thrown_weapon_info& info, const npc* attacker, const weapon_info* w_info, const human* target, ai_throw_situations situation, bool lead_target)
{
    const ai_system* ai = attacker->get_ai();
    info.attacker = attacker;
    info.w_info = w_info;
    info.max_dist = w_info->projectile_info.max_throw_dist;
    info.start_speed = w_info->projectile_info.start_speed;
    info.fuse_time = (float)w_info->projectile_info.fuse_time / 1000.0f; // ms to s
    info.throw_start_pos = get_npc_throw_pos(attacker);
    info.situation = situation;
    info.check_flags = ai_thrown_weapon_info::ai_thrown_weapon_info_flags();
    info.result_flags = ai_thrown_weapon_info::ai_thrown_weapon_info_flags();
    info.target_nav_cell = nullptr;
    info.target = target;
    info.target_belief = ai->perception.get_belief(AI_BELIEF_ENEMY_POSITION, static_cast<const object*>(target));
    if (info.target_belief == nullptr)
    {
        return false;
    }
    switch (situation)
    {
    case AI_THROW_SITUATION_NORMAL:
        info.check_cover_nodes = false;
        if (lead_target)
        {
            vector velocity;
            if (!ai->perception.target_tracking_extrapolate(info.target_pos, velocity, target, info.fuse_time * 0.75f, true))
            {
                return false;
            }
        }
        else
        {
            info.target_pos = target->pos;
        }
        info.available_throw_types.add(AI_THROW_TYPE_BOUNCE);
        info.available_throw_types.add(AI_THROW_TYPE_DIRECT);
        break;
    case AI_THROW_SITUATION_ACQUIRE_VISUAL:
        info.check_cover_nodes = true;
        info.target_pos = info.target_belief->pos;
        info.available_throw_types.add(AI_THROW_TYPE_BOUNCE);
        info.available_throw_types.add(AI_THROW_TYPE_DIRECT);
        break;
    case AI_THROW_SITUATION_VEHICLE:
    {
        info.check_cover_nodes = false;
        if (target->vehicle_handle == 0xFFFFFFFF)
        {
            return false;
        }
        object* objp = World.object_from_handle(target->vehicle_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
        {
            return false;
        }
        vehicle* vp = static_cast<vehicle*>(objp);
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        info.target_pos = vp->pos;
        info.target_pos.y += bmin.y;
        if (lead_target)
        {
            vector velocity;
            vehicle_get_velocity(vp, velocity);
            const float lead_time = attacker->current_team == HUMAN_TEAM_MARAUDER ? vec_dist(info.target_pos, attacker->pos) / 10.0f : info.fuse_time * 0.75f;
            info.target_pos += velocity * lead_time;
            float ground;
            if (havok_terrain_height(ground, info.target_pos.x, info.target_pos.z, false, nullptr) && info.target_pos.y - ground < 0.0f)
            {
                info.target_pos.y = ground;
            }
        }
        if (attacker->current_team == HUMAN_TEAM_MARAUDER)
        {
            info.available_throw_types.add(AI_THROW_TYPE_DIRECT);
        }
        info.available_throw_types.add(AI_THROW_TYPE_BOUNCE);
        break;
    }
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return false;
    }
    info.available_throw_types.add(AI_THROW_TYPE_LOB);
    if (vec_dist(info.throw_start_pos, info.target_pos) > info.max_dist * 0.9f)
    {
        return false;
    }
    info.throw_end_pos = info.target_pos;
    info.target_cover_stance = COVER_STANCE_UNDEFINED;
    info.target_cover_stepout = (cover_node_firing_positions)-1;
    info.target_cover_node = nullptr;
    info.throw_velocity = ZERO_VECTOR;
    const explosion_info* explosion = w_info->m_ai_explosion_info != nullptr ? w_info->m_ai_explosion_info : w_info->m_explosion_info;
    const float safe_dist = explosion->m_knockdown_radius + 1.5f;
    info.safe_dist_sq = safe_dist * safe_dist;
    return true;
}

// 0x00616E30 (:483). cb_data is the attacker.
static bool throw_callback(object* hit_objp, void* cb_data)
{
    if (hit_objp == nullptr || hit_objp->get_obj_type() != OT_HUMAN)
    {
        return true;
    }
    const human* attacker = static_cast<const human*>(cb_data);
    return attacker != hit_objp && human_teams_get_disposition(attacker, static_cast<const human*>(hit_objp)) > TEAM_DISPOSITION_UNFRIENDLY;
}

static void init_throw_raycast(raycast_input& input, const ai_thrown_weapon_info& info)
{
    input.radius = 0.0f;
    input.filter_params = const_cast<npc*>(info.attacker);
    input.filter_callback = throw_callback;
    input.m_filter_info = COLLIDE_PROJECTILE;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
}

// 0x00692860 (:505). LTCG: info ESI.
static bool check_direct_raycast(ai_thrown_weapon_info& info)
{
    if (info.check_flags.direct_raycast)
    {
        return info.result_flags.direct_raycast;
    }
    info.result_flags.direct_raycast = false;
    info.check_flags.direct_raycast = true;
    raycast_input input;
    init_throw_raycast(input, info);
    input.start = info.throw_start_pos;
    input.end = info.throw_end_pos;
    if (info.target_cover_node == nullptr)
    {
        input.end.y += 0.5f;
    }
    raycast_output output;
    if (havok_raycast(input, output) && vec_dist_squared(output.m_hits[0].hit_pos, input.end) >= MAX_RAYCAST_HIT_TARGET_DIST_SQ && !human_combat_hit_human(World.object_from_handle(output.m_hits[0].hit_object_handle), info.target))
    {
        return false;
    }
    info.result_flags.direct_raycast = true;
    return true;
}

// 0x006758F0 (:555). LTCG: info ESI.
static bool check_lob_raycast(ai_thrown_weapon_info& info)
{
    if (info.check_flags.lob_raycast)
    {
        return info.result_flags.lob_raycast;
    }
    info.result_flags.lob_raycast = false;
    info.check_flags.lob_raycast = true;
    vector dir = info.throw_end_pos - info.throw_start_pos;
    const float dy = dir.y;
    const float dist = dir.flatten_mag_and_normalize_safe();
    const float fuse = info.fuse_time;
    const float vy = dy / fuse - fuse * -4.9f; // half of 9.8
    const float vxz = dist / fuse;
    if (info.start_speed < sqrtf(vxz * vxz + vy * vy))
    {
        return false;
    }
    const float apex_height = vy * vy * 0.5f / 9.8f;
    const float apex_time = vy / 9.8f;
    // Original bug kept: the unit direction is scaled by the time to the apex, not the horizontal distance to it.
    const vector apex = info.throw_start_pos + dir * apex_time + UP_VECTOR * apex_height;
    raycast_input input;
    init_throw_raycast(input, info);
    input.start = info.throw_start_pos;
    input.end = apex;
    raycast_output output;
    if (havok_raycast(input, output))
    {
        return false;
    }
    input.start = apex;
    input.end = info.throw_end_pos;
    if (havok_raycast(input, output) && vec_dist_squared(output.m_hits[0].hit_pos, input.end) >= MAX_RAYCAST_HIT_TARGET_DIST_SQ && !human_combat_hit_human(World.object_from_handle(output.m_hits[0].hit_object_handle), info.target))
    {
        return false;
    }
    info.result_flags.lob_raycast = true;
    return true;
}

// 0x005E24E0 (:636). LTCG: info ESI.
static bool check_target_on_navmesh(ai_thrown_weapon_info& info)
{
    if (info.check_flags.target_on_navmesh)
    {
        return info.result_flags.target_on_navmesh;
    }
    info.result_flags.target_on_navmesh = false;
    info.check_flags.target_on_navmesh = true;
    vector pos = info.throw_end_pos;
    nav_cell* cell = bb_pf_get_nav_cell(pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell == nullptr && !bb_pf_find_closest_point_on_nav_mesh(info.throw_end_pos, pos, cell, false))
    {
        return false;
    }
    navpoint* closest_navpoint = nullptr;
    float navpoint_dist_sq;
    if (bb_pf_find_closest_navpoint(info.throw_end_pos, closest_navpoint, navpoint_dist_sq) && navpoint_dist_sq < vec_xz_dist_squared(info.throw_end_pos, pos))
    {
        return false;
    }
    info.target_navmesh_pos = nav_cell_force_point_to_cell(cell, pos);
    info.result_flags.target_on_navmesh = true;
    info.target_nav_cell = cell;
    return true;
}

// The longest line of sight from the target back to the throw start, the common part of the two navmesh path checks.
static void find_navmesh_los(nav_cell_longest_los_output& output, const ai_thrown_weapon_info& info, float scale)
{
    nav_cell_longest_los_input input;
    input.start_pos = info.target_navmesh_pos;
    input.start_nav_cell = info.target_nav_cell;
    input.flags.use_disabled_nav_cells = 0;
    input.flags.ignore_detour_hulls = 0;
    input.flags.check_start_in_detour_hull = 0;
    input.delta_2d.x = (info.throw_start_pos.x - info.target_navmesh_pos.x) * scale;
    input.delta_2d.y = (info.throw_start_pos.z - info.target_navmesh_pos.z) * scale;
    nav_cell_find_longest_line_of_sight_point(output, input);
}

// 0x005E25E0 (:680). LTCG: info ESI.
static bool check_full_navmesh_path(ai_thrown_weapon_info& info)
{
    if (info.check_flags.full_navmesh_path)
    {
        return info.result_flags.full_navmesh_path;
    }
    info.result_flags.half_navmesh_path = false;
    info.result_flags.full_navmesh_path = false;
    info.check_flags.half_navmesh_path = true;
    info.check_flags.full_navmesh_path = true;
    if (!check_target_on_navmesh(info))
    {
        return false;
    }
    const float dx = info.throw_start_pos.x - info.target_navmesh_pos.x;
    const float dz = info.throw_start_pos.z - info.target_navmesh_pos.z;
    const float dist = sqrtf(dx * dx + dz * dz);
    nav_cell_longest_los_output output;
    find_navmesh_los(output, info, 1.0f);
    const bool reached = output.result == NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION;
    if (reached || dist * 0.5f <= output.length + 1.0f)
    {
        info.result_flags.half_navmesh_path = true;
    }
    if (reached || dist - output.length <= 1.0f)
    {
        info.result_flags.full_navmesh_path = true;
        return true;
    }
    return false;
}

// 0x005E2790 (:741). LTCG: info ESI.
static bool check_half_navmesh_path(ai_thrown_weapon_info& info)
{
    if (info.check_flags.half_navmesh_path)
    {
        return info.result_flags.half_navmesh_path;
    }
    info.result_flags.half_navmesh_path = false;
    info.check_flags.half_navmesh_path = true;
    if (!check_target_on_navmesh(info))
    {
        return false;
    }
    const float dx = (info.throw_start_pos.x - info.target_navmesh_pos.x) * 0.5f;
    const float dz = (info.throw_start_pos.z - info.target_navmesh_pos.z) * 0.5f;
    nav_cell_longest_los_output output;
    find_navmesh_los(output, info, 0.5f);
    if (output.result == NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION || sqrtf(dx * dx + dz * dz) - output.length <= 1.0f)
    {
        info.result_flags.half_navmesh_path = true;
        return true;
    }
    return false;
}

// 0x00675C50 (:793). LTCG: info ESI. True when a wall stands within 5 m behind the target to bounce off.
static bool check_wall_raycast(ai_thrown_weapon_info& info)
{
    if (info.check_flags.wall_raycast)
    {
        return info.result_flags.wall_raycast;
    }
    info.result_flags.wall_raycast = false;
    info.check_flags.wall_raycast = true;
    vector dir = info.throw_end_pos - info.throw_start_pos;
    dir.flatten_normalize_safe();
    raycast_input input;
    init_throw_raycast(input, info);
    input.start = info.throw_end_pos;
    if (info.target_cover_node == nullptr)
    {
        input.start.y += 0.5f;
    }
    input.end = input.start + dir * 5.0f;
    raycast_output output;
    if (!havok_raycast(input, output))
    {
        return false;
    }
    info.result_flags.wall_raycast = true;
    return true;
}

// 0x006929B0 (:840). LTCG: info ESI.
static bool calculate_direct_throw(ai_thrown_weapon_info& info)
{
    if (!check_direct_raycast(info))
    {
        return false;
    }
    if (human_in_vehicle(info.target) == nullptr && !check_wall_raycast(info) && check_target_on_navmesh(info))
    {
        return false; // the rolling or bouncing throws do better on open ground
    }
    float target_y = info.throw_end_pos.y;
    if (info.target_cover_node == nullptr)
    {
        target_y += 0.5f;
    }
    const float angle_dot = Throw_error_table[info.situation][AI_THROW_TYPE_DIRECT].angle_dot;
    const float dy = target_y - info.throw_start_pos.y;
    vector dir(info.throw_end_pos.x - info.throw_start_pos.x, dy, info.throw_end_pos.z - info.throw_start_pos.z);
    const float dist = dir.flatten_mag_and_normalize_safe();
    const float speed = fl_rand_range(0.8f, 1.0f) * info.w_info->projectile_info.start_speed;
    // The launch angle of a throw at this speed: a quadratic in sin(2 * angle).
    const float k = dist * dist * -9.8f / (speed * speed) - dy;
    const float a = dy * dy + dist * dist;
    const float b = dist * 2.0f * k;
    const float disc = b * b - (k * k - dy * dy) * a * 4.0f;
    if (disc < 0.0f)
    {
        return false;
    }
    const float root = sqrtf(disc);
    const float roots[2] = {(-b - root) / (a * 2.0f), (root - b) / (a * 2.0f)};
    float angles[4];
    int num_angles = 0;
    for (int i = 0; i < 2; i++)
    {
        if (-1.0f <= roots[i] && roots[i] <= 1.0f)
        {
            const float twice_angle = (float)asin(roots[i]);
            angles[num_angles++] = twice_angle * 0.5f;
            angles[num_angles++] = (3.1415927f - twice_angle) * 0.5f;
        }
    }
    bool found = false;
    float best_time = -1.0f;
    float best_cos = 0.0f;
    float best_sin = 0.0f;
    for (int i = 0; i < num_angles; i++)
    {
        const float c = (float)cos(angles[i]);
        const float s = (float)sin(angles[i]);
        const float time = dist / (c * speed);
        // Half of 9.8. Only the angle that really reaches the target's height counts, and the fastest of those.
        if (fabsf(s * speed * time + info.throw_start_pos.y - time * time * 4.9f - target_y) < 0.01f && (!found || time < best_time))
        {
            found = true;
            best_cos = c;
            best_sin = s;
            best_time = time;
        }
    }
    if (!found)
    {
        return false;
    }
    vector velocity = UP_VECTOR * (best_sin * speed) + dir * (best_cos * speed);
    velocity.normalize();
    info.throw_velocity.rand_around_dir(velocity, angle_dot, 1.0f);
    info.throw_velocity *= speed;
    return true;
}

// 0x00675E30 (:962). LTCG: info ESI.
static bool calculate_lob_throw(ai_thrown_weapon_info& info)
{
    if (!check_lob_raycast(info))
    {
        return false;
    }
    const ai_throw_error_info& error = Throw_error_table[info.situation][AI_THROW_TYPE_LOB];
    const float displacement = fl_rand_range(error.displacement_min, error.displacement_max);
    vector dir = info.throw_end_pos - info.throw_start_pos;
    const float dy = dir.y;
    const float dist = dir.flatten_mag_and_normalize_safe();
    const float time = info.fuse_time * fl_rand_range(0.7f, 0.9f);
    const float vxz = (dist + displacement) / time;
    const float vy = dy / time - time * -4.9f; // half of 9.8
    vector velocity = UP_VECTOR * vy + dir * vxz;
    const float speed = velocity.mag_and_normalize_safe();
    if (speed > info.start_speed)
    {
        return false;
    }
    info.throw_velocity.rand_around_dir(velocity, error.angle_dot, 1.0f);
    info.throw_velocity *= speed;
    return true;
}

// 0x00693020 (:1004). LTCG: info ESI.
static bool calculate_bounce_throw(ai_thrown_weapon_info& info)
{
    if (!check_direct_raycast(info) || !check_target_on_navmesh(info) || !check_half_navmesh_path(info))
    {
        return false;
    }
    const ai_throw_error_info& error = Throw_error_table[info.situation][AI_THROW_TYPE_BOUNCE];
    const float displacement = fl_rand_range(error.displacement_min, error.displacement_max);
    const float time = info.fuse_time * 0.85f;
    vector dir = info.throw_end_pos - info.throw_start_pos;
    const float dy = dir.y;
    const float dist = dir.flatten_mag_and_normalize_safe();
    const float first_bounce = fl_rand_range(0.5f, 0.75f);
    const float vxz = (dist + displacement) / time;
    const float bounce_time = first_bounce * dist / (dist + displacement) * time;
    const float vy = first_bounce * dy / bounce_time - bounce_time * -4.9f; // half of 9.8
    vector velocity = UP_VECTOR * vy + dir * vxz;
    const float speed = velocity.mag_and_normalize_safe();
    if (speed > info.start_speed)
    {
        return false;
    }
    info.throw_velocity.rand_around_dir(velocity, error.angle_dot, 1.0f);
    info.throw_velocity *= speed;
    return true;
}

// 0x005E2930 (:1062). LTCG: info ESI.
static bool calculate_roll_throw(ai_thrown_weapon_info& info)
{
    if (!check_target_on_navmesh(info) || !check_full_navmesh_path(info))
    {
        return false;
    }
    const ai_throw_error_info& error = Throw_error_table[info.situation][AI_THROW_TYPE_ROLL];
    const float displacement = fl_rand_range(error.displacement_min, error.displacement_max);
    vector dir = info.throw_end_pos - get_npc_roll_pos(info.attacker);
    dir.y += 0.5f;
    const float dist = dir.mag_and_normalize();
    const float speed = (dist + displacement) / (info.fuse_time * 0.9f);
    info.throw_velocity.rand_around_dir(dir, error.angle_dot, 1.0f);
    info.throw_velocity *= speed;
    return true;
}

// 0x012C85C4, by ai_throw_types.
static bool (* const Throw_calc_handlers[NUM_AI_THROW_TYPES])(ai_thrown_weapon_info&) =
{
    calculate_lob_throw,
    calculate_direct_throw,
    calculate_bounce_throw,
    calculate_roll_throw,
};

// 0x006571B0 (:1104)
static bool friends_in_blast_area(const ai_thrown_weapon_info& info)
{
    const human_teams team = info.attacker->current_team;
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        const human* hp = World.get_object<human>(i);
        if (human_teams_get_disposition(hp, team) > TEAM_DISPOSITION_NONE && hp != nullptr && !hp->is_killed() && hp->hflags.hidden == 0 && vec_dist_squared(hp->pos, info.target_pos) < info.safe_dist_sq)
        {
            return true;
        }
    }
    return false;
}

// 0x00693350 (:1145). LTCG: info ESI.
static bool adjust_target_position_using_cover(ai_thrown_weapon_info& info)
{
    if (check_direct_raycast(info))
    {
        return true;
    }
    info.check_flags.direct_raycast = false;
    info.target_cover_node = human_combat_cover_get_node(info.target);
    if (info.target_cover_node == nullptr)
    {
        unsigned int handles[10];
        const int num = World.collect_objects<cover_node>(info.target_pos, 5.0f, handles, 10, nullptr, nullptr);
        float best_dist_sq = -1.0f;
        for (int i = 0; i < num; i++)
        {
            const object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
            if (objp == nullptr || objp->get_obj_type() != OT_COVER_NODE || fabsf(objp->pos.y - info.target_pos.y) > 1.5f)
            {
                continue;
            }
            const cover_node* node = static_cast<const cover_node*>(objp);
            if (vec_dist_squared(node->pos, info.attacker->pos) <= info.safe_dist_sq || !node->has_standing_stepout() || node->check_cover_type(info.target, info.throw_start_pos, false, true, true, nullptr) != CCR_OFFENSIVE)
            {
                continue;
            }
            const float dist_sq = vec_xz_dist_squared(info.target_pos, node->pos);
            if (info.target_cover_node == nullptr || dist_sq < best_dist_sq)
            {
                info.target_cover_node = node;
                best_dist_sq = dist_sq;
                if (dist_sq < 1.0f) // 1 m: good enough
                {
                    break;
                }
            }
        }
    }
    if (info.target_cover_node == nullptr || !info.target_cover_node->get_approximate_firing_position(info.throw_end_pos, info.attacker->pos, &info.target_cover_stance, &info.target_cover_stepout))
    {
        return false;
    }
    if (info.target_cover_stepout == COVER_FIRE_POPUP)
    {
        info.available_throw_types.clear();
        info.available_throw_types.add(AI_THROW_TYPE_DIRECT);
        info.available_throw_types.add(AI_THROW_TYPE_LOB);
        info.check_flags.wall_raycast = true;
        info.result_flags.wall_raycast = true;
    }
    return true;
}

// 0x00616E60 (:1238). LTCG: info EDI, throw_type_out on the stack.
static bool pick_and_calculate_throw_type(ai_thrown_weapon_info& info, ai_throw_types* throw_type_out)
{
    if (throw_type_out != nullptr)
    {
        *throw_type_out = AI_THROW_TYPE_UNDEFINED;
    }
    for (int i = 0; i < info.available_throw_types.size(); i++)
    {
        const ai_throw_types type = info.available_throw_types[i];
        if (Throw_calc_handlers[type] != nullptr && Throw_calc_handlers[type](info))
        {
            if (throw_type_out != nullptr)
            {
                *throw_type_out = type;
            }
            return true;
        }
    }
    return false;
}

// 0x00693570 (:1295)
bool ai_thrown_weapon_calculate_throw(vector& throw_velocity, const npc* np, const weapon* wp, const human* target, ai_throw_situations situation, bool lead_target, ai_throw_types* throw_type)
{
    ai_thrown_weapon_info info;
    if (!init_thrown_weapon_info(info, np, wp->w_info, target, situation, lead_target) || friends_in_blast_area(info))
    {
        return false;
    }
    if (info.check_cover_nodes && !adjust_target_position_using_cover(info))
    {
        return false;
    }
    if (!pick_and_calculate_throw_type(info, throw_type))
    {
        return false;
    }
    throw_velocity = info.throw_velocity;
    return true;
}
