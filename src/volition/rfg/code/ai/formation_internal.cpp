// Original: volition/rfg/code/ai/formation_internal.cpp

#include "volition/rfg/code/ai/formation_internal.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>

extern object_pool<ai_formation_member, 180, 1> Ai_formation_member_pool;

static const float MINIMUM_MOVE_DISTANCE_SQ = 1.0f; // 0x01642760
static timestamp Check_invalid_follow_pos_timer; // 0x01642768

// 0x01642770. rfg.exe's dynamic initializer writes only the offsets, so every leader_index stays 0: all followers of a rigid
// formation are placed from the leader.
static const formation_position_information Rigid_formation_definitions[6][15] = {
    {
        formation_position_information(vector(0.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-0.8660254f, 0.0f, -0.5f), 0),
        formation_position_information(vector(0.8660254f, 0.0f, -0.5f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.8660254f, 0.0f, -0.5f), 0),
        formation_position_information(vector(-0.8660254f, 0.0f, -0.5f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.8660254f, 0.0f, -0.5f), 0),
        formation_position_information(vector(-0.8660254f, 0.0f, -0.5f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.8660254f, 0.0f, -0.5f), 0),
        formation_position_information(vector(-0.8660254f, 0.0f, -0.5f), 0),
    },
    {
        formation_position_information(vector(0.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
    },
    {
        formation_position_information(vector(0.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
    },
    {
        formation_position_information(vector(0.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(0.70710677f, 0.0f, -0.70710677f), 0),
        formation_position_information(vector(-0.70710677f, 0.0f, -0.70710677f), 0),
    },
    {
        formation_position_information(vector(0.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-2.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(2.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-2.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(2.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-3.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(3.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(-3.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(3.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-4.0f, 0.0f, 0.0f), 0),
        formation_position_information(vector(4.0f, 0.0f, 0.0f), 0),
    },
    {
        formation_position_information(vector(0.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-0.5f, 0.0f, -1.0f), 0),
        formation_position_information(vector(0.5f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, -2.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, -2.0f), 0),
        formation_position_information(vector(-1.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(1.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-1.5f, 0.0f, -2.0f), 0),
        formation_position_information(vector(1.5f, 0.0f, -2.0f), 0),
        formation_position_information(vector(-1.5f, 0.0f, -1.0f), 0),
        formation_position_information(vector(1.5f, 0.0f, -1.0f), 0),
        formation_position_information(vector(-2.0f, 0.0f, -2.0f), 0),
        formation_position_information(vector(2.0f, 0.0f, -2.0f), 0),
        formation_position_information(vector(-2.0f, 0.0f, -1.0f), 0),
        formation_position_information(vector(2.0f, 0.0f, -1.0f), 0),
    },
};

// 0x005E9AC0 (:26)
formation_position_information::formation_position_information(vector offset, int index)
    : offset_from_leader(offset), leader_index(index)
{
}

// 0x005E9AE0 (:46)
member_follow_info::member_follow_info()
    : obj(nullptr), current_is_valid(false), move_dir_is_valid(false)
{
}

// 0x005E99D0 (.h:37)
ghost_history_point::ghost_history_point()
{
}

// 0x005E99E0 (.h:38)
ghost_history_point::ghost_history_point(const vector& p, float d, const vector& v)
    : pos(p), dist(d), dir(v)
{
}

// 0x0064B040
ai_formation_member::ai_formation_member()
{
}

// 0x0064B000
ai_formation::ai_formation()
{
}

// 0x005E9B10 (:346)
static bool object_can_follow(const object* obj)
{
    if (obj == nullptr)
    {
        return false;
    }
    if (obj->get_obj_type() == OT_HUMAN || obj->get_obj_type() == OT_VEHICLE)
    {
        return !obj->is_killed();
    }
    return obj->get_obj_type() == OT_SQUAD && convoy_find_convoy(obj->get_handle()) != nullptr;
}

// 0x005E9B50 (:376). LTCG: obj ESI.
static bool follower_is_stuck(const object* obj)
{
    if (obj->get_obj_type() != OT_VEHICLE)
    {
        return false;
    }
    const vehicle* vp = static_cast<const vehicle*>(obj);
    return vehicle_is_flipped(vp) || vp->vai.stuck.extended_stuck_time.valid();
}

// 0x005E9B80 (:408). LTCG: obj EAX, vel ECX.
bool formation_get_object_velocity(vector& vel, const object* obj)
{
    switch (obj->get_obj_type())
    {
    case OT_HUMAN:
        vel = static_cast<const human*>(obj)->actual_velocity; // 0x270
        return true;
    case OT_VEHICLE:
        vehicle_get_velocity(obj->get_handle(), vel);
        return true;
    case OT_SQUAD:
        vel = static_cast<const object_squad*>(obj)->simulated_velocity; // 0x13C
        return true;
    default:
        if (obj->havok_handle == 0xFFFFFFFF)
        {
            return false;
        }
        havok_body_get_linear_velocity(obj->havok_handle, vel);
        return true;
    }
}

// 0x00632050 (:443). LTCG: dir ESI, obj EDI, speed on the stack.
static bool get_object_movement_dir(vector& dir, const object* obj, float* speed)
{
    if (!formation_get_object_velocity(dir, obj))
    {
        return false;
    }
    const float mag = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (speed != nullptr)
    {
        *speed = mag;
    }
    if (mag >= 0.01f || -mag >= 0.01f)
    {
        const float scale = 1.0f / mag;
        dir.x *= scale;
        dir.y *= scale;
        dir.z *= scale;
    }
    else if (obj != nullptr && obj->get_obj_type() == OT_HUMAN)
    {
        // Standing still: the vehicle's facing, flattened, or the human's own.
        const unsigned int vehicle_handle = static_cast<const human*>(obj)->vehicle_handle;
        const object* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(vehicle_handle) : nullptr;
        if (vp != nullptr && vp->get_obj_type() == OT_VEHICLE)
        {
            dir = vp->orient.fvec;
            dir.flatten_normalize_safe();
            return true;
        }
        dir = obj->orient.fvec;
    }
    return true;
}

// 0x005E9BF0 (:486)
static bool formation_obstructions_callback(object* hit_objp, void* filter_params)
{
    switch (hit_objp->get_obj_type())
    {
    case OT_HUMAN:
    case OT_ZONE:
        return false;
    case OT_VEHICLE:
        return hit_objp != filter_params;
    default:
        return true;
    }
}

// 0x0067B3B0 (:510). LTCG: follow_info ESI, member_info EDI. Spherecast from the member to where its slot is heading, and
// when that is clear, whether the end has ground or a nav cell under it.
static void detect_follower_obstructions(ai_formation_member* member_info, const object* objp, const vehicle* parent_vehicle, bool is_follow_formation)
{
    if (objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return;
    }
    const follow_information& info = member_info->follow_info;
    vector end = info.ideal_formation_pos;
    if (!is_follow_formation)
    {
        end.x += info.formation_dir.x * info.formation_speed * 2.0f;
        end.y += info.formation_dir.y * info.formation_speed * 2.0f;
        end.z += info.formation_dir.z * info.formation_speed * 2.0f;
    }
    else if (info.my_leader != nullptr)
    {
        float step = vec_xz_dist(info.my_leader->pos, info.formation_pos);
        if (step - info.formation_speed * 2.0f >= 0.0f)
        {
            step = info.formation_speed * 2.0f;
        }
        end.x += info.formation_dir.x * step;
        end.y += info.formation_dir.y * step;
        end.z += info.formation_dir.z * step;
    }

    raycast_input input;
    input.start = objp->pos;
    input.start.y += 0.8f;
    input.end = vector(end.x, end.y + 0.8f, end.z);
    input.radius = 0.5f;
    input.filter_callback = formation_obstructions_callback;
    input.filter_params = const_cast<vehicle*>(parent_vehicle);
    input.m_filter_info = COLLIDE_PHYSICAL_OBJECT;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    raycast_output output;
    bool blocked = havok_spherecast(input, output);
    if (!blocked)
    {
        const vector probe(input.end.x + (objp->pos.x - input.start.x), (objp->pos.y - input.start.y) + input.end.y, input.end.z + (objp->pos.z - input.start.z));
        float ground;
        if (!havok_terrain_height(ground, probe.x, probe.z, false, nullptr) || 1.0f < probe.y - ground
            || bb_pf_get_nav_cell(probe, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) == nullptr)
        {
            input.start = probe;
            input.end = vector(probe.x, probe.y - 1.0f, probe.z);
            if (!havok_raycast(input, output))
            {
                blocked = true;
            }
        }
    }
    member_info->follow_flags.blocked = blocked;
    member_info->obstruction_period.set(blocked ? 2000 : 500);
}

// 0x006321A0 (:630). LTCG: member_dir ECX, leader_dir EDX, the rest on the stack. The point about two thirds of dist back
// along the member's ghost history, through a Catmull-Rom spline.
static bool calculate_follow_pos(vector& follow_pos, const object* follower, const ai_formation_member* member, const vector& member_dir, float dist, vector* leader_dir)
{
    ai_formation_member::ghost_history_array& ghosts = const_cast<ai_formation_member::ghost_history_array&>(member->ghost_history);
    const float target_dist = dist * 0.6666667f;
    const object* leader = World.object_from_handle(member->member_handle);
    if (leader == nullptr)
    {
        return false;
    }
    const int num_ghosts = ghosts.size();
    if (num_ghosts < 1)
    {
        return false;
    }

    float from_newest = vec_dist(leader->pos, ghosts[0].pos);
    if (from_newest - member->max_dist_from_ghost_point < 0.0f)
    {
        from_newest = member->max_dist_from_ghost_point;
    }
    const_cast<ai_formation_member*>(member)->max_dist_from_ghost_point = from_newest;

    int segment;
    float t = 0.0f;
    if (from_newest <= target_dist)
    {
        float remaining = target_dist - from_newest;
        segment = -1;
        for (int i = 0; i < num_ghosts - 1; i++)
        {
            const float d = ghosts[i].dist;
            if (remaining <= d)
            {
                segment = i + 1;
                t = remaining / d;
                break;
            }
            remaining -= d;
        }
        if (segment == -1)
        {
            if (follower == nullptr)
            {
                return false;
            }
            const float to_follower = vec_dist(follower->pos, ghosts[num_ghosts - 1].pos);
            if (to_follower <= remaining)
            {
                // Already within the history: stay.
                follow_pos = follower->pos;
                if (leader_dir != nullptr)
                {
                    *leader_dir = member_dir;
                }
                return true;
            }
            t = remaining / to_follower;
            segment = num_ghosts;
        }
    }
    else
    {
        t = target_dist / from_newest;
        segment = 0;
    }

    vector points[14];
    points[0] = vector(leader->pos.x + member_dir.x, leader->pos.y + member_dir.y, leader->pos.z + member_dir.z);
    points[1] = leader->pos;
    for (int i = 0; i < num_ghosts; i++)
    {
        points[i + 2] = ghosts[i].pos;
    }
    int num_points;
    if (follower == nullptr)
    {
        const vector& last = points[num_ghosts + 1];
        const vector& before = points[num_ghosts];
        points[num_ghosts + 2] = vector((last.x - before.x) + last.x, (last.y - before.y) + last.y, (last.z - before.z) + last.z);
        num_points = num_ghosts + 3;
    }
    else
    {
        points[num_ghosts + 2] = follower->pos;
        points[num_ghosts + 3] = vector(follower->pos.x - follower->orient.fvec.x, follower->pos.y - follower->orient.fvec.y, follower->pos.z - follower->orient.fvec.z);
        num_points = num_ghosts + 4;
    }
    catmull_rom_spline spline(num_points, points);
    spline.get_point(follow_pos, static_cast<float>(segment + 1) + t);

    if (leader_dir != nullptr)
    {
        if (segment == 0)
        {
            vec_interp_const_norm(*leader_dir, member_dir, ghosts[0].dir, t);
        }
        else if (segment < num_ghosts)
        {
            vec_interp_const_norm(*leader_dir, ghosts[segment - 1].dir, ghosts[segment].dir, t);
        }
        else
        {
            *leader_dir = ghosts[num_ghosts - 1].dir;
        }
    }
    return true;
}

// 0x005E9C20 (:793)
float find_spline_dist(const vector& pos, int seg, vector* points, int num_points, int dir)
{
    if (seg < 0 || num_points <= seg)
    {
        return 1.0f;
    }
    vector closest;
    const float length = vec_dist(points[seg], points[seg + 1]);
    const float fraction = ix_closest_point_on_line_segment(closest, pos, points[seg], points[seg + 1]) / length;
    if (seg > 0 && dir < 1 && fraction < 0.005f)
    {
        return find_spline_dist(pos, seg - 1, points, num_points, -1);
    }
    if (seg < num_points - 1 && dir > -1 && 0.995f < fraction)
    {
        return find_spline_dist(pos, seg + 1, points, num_points, 1);
    }
    return static_cast<float>(seg) + fraction;
}

// 0x006327A0 (:825)
void ai_formation_member::alloc()
{
    follow_flags.blocked = false;
    follow_flags.wants_wait = false;
    follow_flags.is_leader = false;
    follow_flags.obstruction_check = false;
    member_handle = 0xFFFFFFFF;
    max_dist_from_ghost_point = 0.0f;
    obstruction_period.set(0);
    stuck_time.set(0);
    ghost_history.remove_all();
    follow_info.reset();
}

// 0x00632860 (:849)
void ai_formation_member::add_ghost_point(const vector& p, float d, const vector& v)
{
    ghost_history.add(ghost_history_point(p, d, v));
    max_dist_from_ghost_point = 0.0f;
}

// 0x006328C0 (:882): the last directions, blended, then towards current_dir with the distance from the newest point.
vector ai_formation_member::get_dir(const vector& current_pos, const vector& current_dir) const
{
    ghost_history_array& ghosts = const_cast<ghost_history_array&>(ghost_history);
    int count = ghosts.size();
    if (count > 2)
    {
        count = 3;
    }
    vector result = ghosts[count - 1].dir;
    for (int i = count - 2; i >= 0; i--)
    {
        vector blended;
        vec_interp_const_norm(blended, result, ghosts[i].dir, 0.5f);
        result = blended;
    }
    vector from_newest(current_pos.x - ghosts[0].pos.x, current_pos.y - ghosts[0].pos.y, current_pos.z - ghosts[0].pos.z);
    float amount = from_newest.mag_and_normalize_safe();
    if (amount - 0.0f < 0.0f)
    {
        amount = 0.0f;
    }
    if (amount - 1.0f >= 0.0f)
    {
        amount = 1.0f;
    }
    if (amount >= 0.01f || -amount >= 0.01f)
    {
        vector blended;
        vec_interp_const_norm(blended, result, current_dir, amount * 0.5f);
        result = blended;
    }
    return result;
}

// 0x005E9A20 (.h:99)
bool ai_formation_member::is_stuck() const
{
    return !stuck_time.elapsed();
}

// 0x00632AB0 (:919)
void ai_formation::alloc()
{
    handle = 0xFFFFFFFF;
    leader_handle = 0xFFFFFFFF;
    formation_type = AI_FORMATION_NONE;
    leader_to_follower_dist = 0.0f;
    follower_to_follower_dist = 0.0f;
    formation_history.remove_all();
    max_dist_from_history_point = 0.0f;
    reorder_time.set(0);
}

// 0x0067B830 (:937)
void ai_formation::release()
{
    handle = 0xFFFFFFFF;
    for (int i = 0; i < member_list.size(); i++)
    {
        Ai_formation_member_pool.free(member_list[i]);
    }
    member_list.remove_all();
}

// 0x0067B930 (:965)
bool ai_formation::init(unsigned int new_handle, ai_formation_types new_formation_type, const object* leader, const object* follower)
{
    if (!object_can_follow(leader) || follower == nullptr || !object_can_follow(follower) || new_formation_type >= NUM_AI_FORMATION_TYPES)
    {
        return false;
    }
    handle = new_handle;
    follow_spacing_scale = new_formation_type == AI_FORMATION_VEHICLE_ESCORT ? 1.2f : 3.0f; // get_formation_spacing_scale inlined
    leader_handle = leader->get_handle();
    formation_type = new_formation_type;
    return add_member(leader) && add_member(follower);
}

// 0x0067BA00 (:1005)
bool ai_formation::add_follower(const object* follower)
{
    if (!object_can_follow(follower))
    {
        return false;
    }
    for (int i = 1; i < member_list.size(); i++)
    {
        if (member_list[i]->member_handle == follower->get_handle())
        {
            return true;
        }
    }
    return member_list.size() < member_list.capacity() && add_member(follower);
}

// 0x006A0D80 (:1036)
bool ai_formation::remove_follower(unsigned int follower_handle)
{
    const int index = get_member_index(follower_handle);
    if (index > 0)
    {
        remove_follower_by_index(index);
        return true;
    }
    return false;
}

// 0x005E9A40 (.h:135)
unsigned int ai_formation::get_leader_handle() const
{
    return leader_handle;
}

// 0x00631E80 (.h:137)
int ai_formation::get_num_followers() const
{
    return member_list.size() - 1;
}

// 0x0065E500 (.h:138): the leader counts.
bool ai_formation::is_following(unsigned int follower_handle) const
{
    return get_member_index(follower_handle) != -1;
}

// 0x005E9A50 (.h:139)
bool ai_formation::is_rigid_formation() const
{
    return formation_type > AI_FORMATION_NONE && formation_type < NUM_AI_FORMATION_RIGID_FORMATIONS;
}

// 0x005E9A70 (.h:140)
bool ai_formation::is_flock_formation() const
{
    return formation_type == AI_FORMATION_FLOCK;
}

// 0x005E9A80 (.h:141)
bool ai_formation::is_follow_formation() const
{
    return formation_type == AI_FORMATION_FOLLOW_THE_LEADER || formation_type == AI_FORMATION_FOLLOW_THE_LEADER_TIGHT;
}

// 0x0065E530 (.h:163): the leader does not count.
bool ai_formation::is_formation_member(unsigned int member_handle) const
{
    return get_member_index(member_handle) > 0;
}

// 0x006A0DD0 (:1060)
bool ai_formation::do_frame()
{
    const object* leader = World.object_from_handle(leader_handle);
    if (!object_can_follow(leader) || !remove_derelict_members())
    {
        return false;
    }
    if (formation_type == AI_FORMATION_FOLLOW_THE_LEADER_TIGHT)
    {
        reorder_members();
    }
    calculate_follower_positions();
    update_ghost_history();
    return true;
}

// 0x0065E970 (:1121)
const follow_information* ai_formation::get_follow_position(const object* follower) const
{
    if (follower == nullptr)
    {
        return nullptr;
    }
    const int index = get_member_index(follower->get_handle());
    return index > 0 ? &member_list[index]->follow_info : nullptr;
}

// 0x0065E9C0 (:1142)
void ai_formation::request_wait(const object* follower, bool wait)
{
    const int index = get_member_index(follower->get_handle());
    if (index > 0)
    {
        member_list[index]->follow_flags.wants_wait = wait;
    }
}

// 0x00632AF0 (:1151)
bool ai_formation::wait_requested() const
{
    for (int i = 1; i < member_list.size(); i++)
    {
        if (member_list[i]->follow_flags.wants_wait)
        {
            return true;
        }
    }
    return false;
}

// 0x00632B30 (:1169)
const object* ai_formation::get_member_requesting_wait() const
{
    for (int i = 1; i < member_list.size(); i++)
    {
        if (member_list[i]->follow_flags.wants_wait)
        {
            const object* objp = World.object_from_handle(member_list[i]->member_handle);
            if (objp != nullptr)
            {
                return objp;
            }
        }
    }
    return nullptr;
}

// 0x00632B70 (:1197)
ai_formation_member* ai_formation::get_obstruction_check_member(int& max_time_since) const
{
    ai_formation_member* longest = nullptr;
    for (int i = 1; i < member_list.size(); i++)
    {
        ai_formation_member* member = member_list[i];
        if (member->follow_flags.obstruction_check)
        {
            const int time_since = member->obstruction_period.time_since();
            if (max_time_since < time_since)
            {
                longest = member;
                max_time_since = time_since;
            }
        }
    }
    return longest;
}

// 0x0067BAA0 (:1222)
void ai_formation::check_obstructions(ai_formation_member* member_info)
{
    if (member_info == nullptr || !member_info->follow_flags.obstruction_check)
    {
        return;
    }
    const int index = get_member_index(member_info->member_handle);
    if (index < 0)
    {
        return;
    }
    const object* objp = World.object_from_handle(member_info->member_handle);
    if (objp == nullptr)
    {
        return;
    }
    const unsigned int vehicle_handle = get_member_vehicle_handle(get_leader_index(index));
    const object* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(vehicle_handle) : nullptr;
    const vehicle* parent_vehicle = (vp != nullptr && vp->get_obj_type() == OT_VEHICLE) ? static_cast<const vehicle*>(vp) : nullptr;
    detect_follower_obstructions(member_info, objp, parent_vehicle, is_follow_formation());
}

// 0x00632BD0 (:1255): the leader is collected too.
int ai_formation::collect_followers(unsigned int* followers, int max_collect, bool (*callback)(unsigned int, void*), void* callback_data)
{
    int count = 0;
    for (int i = 0; i < member_list.size(); i++)
    {
        const unsigned int member = member_list[i]->member_handle;
        if (callback == nullptr || callback(member, callback_data))
        {
            followers[count] = member;
            count++;
            if (count == max_collect)
            {
                return count;
            }
        }
    }
    return count;
}

// 0x005E9D00 (:1288)
float ai_formation::get_object_base_distance(const object* obj) const
{
    vector bmin, bmax;
    obj->get_local_bbox(bmin, bmax);
    const double dx = bmax.x - bmin.x;
    const double dz = bmax.z - bmin.z;
    return sqrtf(static_cast<float>(dx * dx + dz * dz)) * 0.5f * follow_spacing_scale;
}

// 0x00632C30 (:1303)
float ai_formation::get_human_base_distance(const human* hp) const
{
    if (human_in_vehicle(hp) != nullptr && hp->vehicle_handle != 0xFFFFFFFF)
    {
        const object* vp = World.object_from_handle(hp->vehicle_handle);
        if (vp != nullptr && vp->get_obj_type() == OT_VEHICLE)
        {
            return get_object_base_distance(vp);
        }
    }
    return object_is_player(hp) ? 2.0f : 1.25f;
}

// 0x00632CA0 (:1326)
float ai_formation::get_min_spacing(const object* obj) const
{
    if (obj->get_obj_type() == OT_HUMAN)
    {
        return get_human_base_distance(static_cast<const human*>(obj));
    }
    return obj->get_obj_type() == OT_SQUAD ? 10.0f : get_object_base_distance(obj);
}

// 0x00632CE0 (:1343)
void ai_formation::calculate_spacing()
{
    if (member_list.size() <= 1)
    {
        return;
    }
    const object* leader = World.object_from_handle(leader_handle);
    if (leader != nullptr)
    {
        member_list[0]->min_spacing = get_min_spacing(leader);
    }
    float follower_spacing = 0.0f;
    for (int i = 1; i < member_list.size(); i++)
    {
        if (follower_spacing - member_list[i]->min_spacing < 0.0f)
        {
            follower_spacing = member_list[i]->min_spacing;
        }
    }
    float leader_spacing = member_list[0]->min_spacing;
    if (leader_spacing - follower_spacing < 0.0f)
    {
        leader_spacing = follower_spacing;
    }
    leader_to_follower_dist = leader_spacing + follower_spacing;
    follower_to_follower_dist = follower_spacing * 2.0f;
}

// 0x0065EA00 (:1380)
bool ai_formation::add_member(const object* member)
{
    ai_formation_member* info = Ai_formation_member_pool.alloc(0);
    if (info == nullptr)
    {
        return false;
    }
    // Original quirk kept: info stays allocated when the object has no velocity.
    vector dir;
    if (!get_object_movement_dir(dir, member, nullptr))
    {
        return false;
    }
    const bool is_follower = member_list.size() != 0;
    info->ghost_history.add(ghost_history_point(member->pos, 0.0f, dir));
    info->max_dist_from_ghost_point = 0.0f;
    info->member_handle = member->get_handle();
    info->min_spacing = get_min_spacing(member);
    if (is_follower)
    {
        info->follow_flags.is_leader = false;
        info->follow_flags.obstruction_check = is_rigid_formation() || formation_type == AI_FORMATION_FLOCK || formation_type == AI_FORMATION_FOLLOW_THE_LEADER_TIGHT;
    }
    else
    {
        if (is_follow_formation())
        {
            formation_history.add(member->pos);
            max_dist_from_history_point = 0.0f;
        }
        info->follow_flags.is_leader = true;
        info->follow_flags.obstruction_check = false;
    }
    member_list.add(info);
    if (is_follower)
    {
        calculate_spacing();
    }
    return true;
}

// 0x0067BB60 (:1437)
void ai_formation::remove_follower_by_index(int member_index)
{
    if (member_index <= 0 || member_index >= member_list.size())
    {
        return;
    }
    Ai_formation_member_pool.free(member_list[member_index]);
    member_list.remove(member_index);
    if (member_list.size() > 1)
    {
        calculate_spacing();
    }
}

// 0x005E9DA0 (:1455)
int ai_formation::get_leader_index(int member_index) const
{
    if (is_rigid_formation())
    {
        return Rigid_formation_definitions[formation_type][member_index].leader_index;
    }
    if (formation_type == AI_FORMATION_FLOCK)
    {
        return 0;
    }
    if (is_follow_formation())
    {
        return member_index - 1;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
    return 0;
}

// 0x00632ED0 (:1487)
int ai_formation::get_member_index(unsigned int member_handle) const
{
    for (int i = 0; i < member_list.size(); i++)
    {
        if (member_list[i]->member_handle == member_handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x00632F00 (:1510)
unsigned int ai_formation::get_member_vehicle_handle(int member_index) const
{
    if (member_index > -1 && member_index < member_list.size())
    {
        const object* objp = World.object_from_handle(member_list[member_index]->member_handle);
        switch (objp->get_obj_type())
        {
        case OT_VEHICLE:
            return objp->get_handle();
        case OT_HUMAN:
            return static_cast<const human*>(objp)->vehicle_handle;
        case OT_SQUAD:
            return static_cast<const object_squad*>(objp)->squad_vehicle;
        default:
            break;
        }
    }
    return 0xFFFFFFFF;
}

// 0x0065EBB0 (:1541): drops the emptied slots, and moves stuck members behind the others.
int ai_formation::compact_member_list()
{
    farray<ai_formation_member*, 15> stuck;
    ai_formation_member* free_members[16];
    int num_free = 0;
    const int old_count = member_list.size();
    for (int i = 0; i < old_count; i++)
    {
        ai_formation_member* member = member_list[i];
        if (member == nullptr)
        {
            continue;
        }
        if (member->stuck_time.elapsed())
        {
            if (num_free + 1 < 16)
            {
                free_members[num_free] = member;
                num_free++;
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            }
        }
        else
        {
            stuck.add(member);
        }
    }
    member_list.remove_all();
    for (int i = 0; i < num_free; i++)
    {
        member_list.add(free_members[i]);
    }
    for (int i = 0; i < stuck.size(); i++)
    {
        member_list.add(stuck[i]);
    }
    const int new_count = member_list.size();
    if (new_count != old_count)
    {
        calculate_spacing();
    }
    return new_count;
}

// 0x0067BBC0 (:1591)
bool ai_formation::remove_derelict_members()
{
    for (int i = member_list.size() - 1; i >= 1; i--)
    {
        ai_formation_member* member = member_list[i];
        if (member == nullptr)
        {
            return false;
        }
        const object* objp = World.object_from_handle(member->member_handle);
        if (objp != nullptr && object_can_follow(objp))
        {
            if (follower_is_stuck(objp))
            {
                member->stuck_time.set(5000);
            }
            continue;
        }
        Ai_formation_member_pool.free(member);
        member_list[i] = nullptr;
    }
    return compact_member_list() > 1;
}

// 0x00632F60 (:1627): keeps the members in order along the leader's path, with 0.25 of a segment of hysteresis.
void ai_formation::reorder_members()
{
    if (!reorder_time.elapsed())
    {
        return;
    }
    reorder_time.set_rand(2000, 3000);
    const object* leader = World.object_from_handle(leader_handle);
    const float first_dist = leader_to_follower_dist * 0.6f;
    const float spacing = follower_to_follower_dist * 0.6f;
    const int history_count = formation_history.size();

    // Original quirk kept: the numerator uses the whole leader_to_follower_dist, the first index below 0.6 of it.
    int on_path = static_cast<int>(floor(static_cast<double>(((static_cast<float>(history_count - 1) * spacing + max_dist_from_history_point) - leader_to_follower_dist) / spacing))) + 1;
    if (member_list.size() - 1 <= on_path)
    {
        on_path = member_list.size() - 1;
    }
    if (on_path <= 1 || !member_list[2]->stuck_time.elapsed())
    {
        return;
    }

    vector points[21];
    points[0] = leader->pos;
    for (int i = 0; i < history_count; i++)
    {
        points[i + 1] = formation_history[i];
    }
    const int first_segment = static_cast<int>(floor(static_cast<double>((first_dist - max_dist_from_history_point) / spacing)));

    float dists[16];
    int count = 0;
    for (int i = 1; i <= on_path; i++)
    {
        if (!member_list[i]->stuck_time.elapsed())
        {
            break;
        }
        count++;
        const object* objp = World.object_from_handle(member_list[i]->member_handle);
        int segment = first_segment - 1 + i;
        float dist;
        if (segment < 0 || history_count <= segment)
        {
            dist = 1.0f;
        }
        else
        {
            vector closest;
            const float length = vec_dist(points[segment], points[segment + 1]);
            const float fraction = ix_closest_point_on_line_segment(closest, objp->pos, points[segment], points[segment + 1]) / length;
            if (segment >= 1 && fraction < 0.005f)
            {
                dist = find_spline_dist(objp->pos, segment - 1, points, history_count, -1);
            }
            else if (segment < history_count - 1 && 0.995f < fraction)
            {
                dist = find_spline_dist(objp->pos, segment + 1, points, history_count, 1);
            }
            else
            {
                dist = static_cast<float>(segment) + fraction;
            }
        }
        dists[i] = dist;
    }

    for (int i = 1; i < count; i++)
    {
        int nearest = i;
        float best = dists[i];
        for (int j = i + 1; j <= count; j++)
        {
            if (dists[j] < best - 0.25f)
            {
                nearest = j;
                best = dists[j];
            }
        }
        if (nearest != i)
        {
            const float swap_dist = dists[i];
            ai_formation_member* swap_member = member_list[i];
            dists[i] = dists[nearest];
            member_list[i] = member_list[nearest];
            dists[nearest] = swap_dist;
            member_list[nearest] = swap_member;
        }
    }
}

// 0x00633490 (:1720)
void ai_formation::update_ghost_history()
{
    if (is_follow_formation())
    {
        const object* leader = World.object_from_handle(leader_handle);
        if (leader == nullptr)
        {
            return;
        }
        const float moved = vec_dist(leader->pos, formation_history[0]);
        if (moved < follower_to_follower_dist * 0.6f)
        {
            if (max_dist_from_history_point - moved < 0.0f)
            {
                max_dist_from_history_point = moved;
            }
        }
        else
        {
            vector dir;
            if (!get_object_movement_dir(dir, leader, nullptr))
            {
                return;
            }
            formation_history.add(leader->pos);
            max_dist_from_history_point = 0.0f;
        }
    }

    const int count = member_list.size();
    for (int i = 0; i < count; i++)
    {
        ai_formation_member* member = member_list[i];
        const object* objp = member != nullptr ? World.object_from_handle(member->member_handle) : nullptr;
        if (objp == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        const float moved_squared = vec_xz_dist_squared(objp->pos, member->ghost_history[0].pos);
        if (MINIMUM_MOVE_DISTANCE_SQ <= moved_squared)
        {
            vector dir;
            if (get_object_movement_dir(dir, objp, nullptr))
            {
                member->add_ghost_point(objp->pos, sqrtf(moved_squared), dir);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            }
        }
    }
}

static float clamp_formation(float value, float low, float high)
{
    if (value - low < 0.0f)
    {
        value = low;
    }
    if (value - high >= 0.0f)
    {
        value = high;
    }
    return value;
}

// 0x006336C0 (:1792)
void ai_formation::calculate_follower_positions_flock()
{
    const int count = member_list.size();
    const object* leader = World.object_from_handle(leader_handle);
    const human* leader_human = (leader != nullptr && leader->get_obj_type() == OT_HUMAN) ? static_cast<const human*>(leader) : nullptr;

    float leader_speed;
    vector leader_dir;
    get_object_movement_dir(leader_dir, leader, &leader_speed);
    const vector formation_dir = member_list[0]->get_dir(leader->pos, leader_dir);

    const object* objects[15];
    vector moves[15];
    objects[0] = leader;
    moves[0] = ZERO_VECTOR;
    for (int i = 1; i < count; i++)
    {
        const object* objp = World.object_from_handle(member_list[i]->member_handle);
        objects[i] = objp;
        vector to_leader(leader->pos.x - objp->pos.x, leader->pos.y - objp->pos.y, leader->pos.z - objp->pos.z);
        const float distance = to_leader.flatten_mag_and_normalize_safe();

        const float keep_up = clamp_formation(distance - leader_to_follower_dist, 0.0f, 0.9f);
        const float catch_up = clamp_formation(distance - leader_to_follower_dist * 2.5f, 0.0f, 10.0f) + keep_up;
        moves[i] = vector(catch_up * to_leader.x, to_leader.y * catch_up, to_leader.z * catch_up);

        vector facing = leader_dir;
        if (leader == player_get_local_player() && Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE)
        {
            facing = Rfg_camera.real_orient.fvec.copy_flatten_normalize_safe();
        }

        // In front of the leader: step out of its line of sight, and back.
        const float in_front = -(facing.y * to_leader.y + facing.x * to_leader.x + facing.z * to_leader.z);
        if (in_front > 0.0f)
        {
            float back_amount = in_front * distance;
            const vector right(facing.z * UP_VECTOR.y - facing.y * UP_VECTOR.z, facing.x * UP_VECTOR.z - facing.z * UP_VECTOR.x, facing.y * UP_VECTOR.x - facing.x * UP_VECTOR.y);
            const float side = right.y * to_leader.y + right.x * to_leader.x + right.z * to_leader.z;
            const float clearance = leader_to_follower_dist - fabsf(side * distance);
            float side_amount = 0.0f - clearance < 0.0f ? clearance : 0.0f;
            if (leader_human != nullptr)
            {
                const timestamp& aim = leader_human->combat.combat_aim_timestamp;
                const timestamp& ready = leader_human->combat.combat_ready_timestamp;
                if (aim.valid() && !aim.elapsed())
                {
                    side_amount *= 1.2f;
                }
                else if (!ready.valid() || ready.elapsed())
                {
                    side_amount *= 0.6f;
                    back_amount *= 0.5f;
                }
            }
            const float sign = side >= 0.0f ? 1.0f : -1.0f;
            back_amount = -back_amount;
            side_amount = -sign * side_amount;
            moves[i].x += back_amount * facing.x + right.x * side_amount;
            moves[i].y += facing.y * back_amount + right.y * side_amount;
            moves[i].z += facing.z * back_amount + right.z * side_amount;
        }

        // Spreads each pair apart.
        for (int j = 0; j < i; j++)
        {
            vector to_other(objects[j]->pos.x - objp->pos.x, objects[j]->pos.y - objp->pos.y, objects[j]->pos.z - objp->pos.z);
            const float apart = to_other.mag_and_normalize_safe();
            const float push = clamp_formation((j == 0 ? leader_to_follower_dist : follower_to_follower_dist) - apart, 0.0f, 0.9f);
            moves[i].x -= to_other.x * push;
            moves[i].y -= to_other.y * push;
            moves[i].z -= to_other.z * push;
            moves[j].x += to_other.x * push;
            moves[j].y += to_other.y * push;
            moves[j].z += to_other.z * push;
        }
    }

    for (int i = 1; i < count; i++)
    {
        follow_information& info = member_list[i]->follow_info;
        info.formation_speed = leader_speed;
        info.my_leader = leader;
        const vector ideal(moves[i].x + objects[i]->pos.x, moves[i].y + objects[i]->pos.y, objects[i]->pos.z + moves[i].z);
        info.ideal_formation_pos = ideal;
        if (member_list[i]->follow_flags.blocked)
        {
            info.follow_mode = AI_FFM_FOLLOW_THE_LEADER;
            calculate_follow_pos(info.formation_pos, objects[i], member_list[0], leader_dir, leader_to_follower_dist, &info.formation_dir);
        }
        else
        {
            info.follow_mode = AI_FFM_FLOCK;
            info.formation_dir = formation_dir;
            info.formation_pos = ideal;
        }
    }
}

// 0x00634070 (:1907)
void ai_formation::calculate_follower_positions_follow()
{
    const int count = member_list.size();
    member_follow_info infos[15];
    infos[0].obj = World.object_from_handle(leader_handle);
    const bool squad_with_vehicle = infos[0].obj->get_obj_type() == OT_SQUAD && static_cast<const object_squad*>(infos[0].obj)->should_spawn_with_vehicle();

    for (int i = 1; i < count; i++)
    {
        const object* objp = World.object_from_handle(member_list[i]->member_handle);
        infos[i].obj = const_cast<object*>(objp);
        const bool stuck = !member_list[i]->stuck_time.elapsed();
        const int leader_index = stuck ? 0 : i - 1;
        member_follow_info& target = infos[leader_index];
        if (!target.current_is_valid)
        {
            get_object_movement_dir(target.current_dir, target.obj, &target.current_speed);
            target.current_is_valid = true;
        }
        follow_information& info = member_list[i]->follow_info;
        info.my_leader = target.obj;
        info.follow_mode = AI_FFM_FOLLOW_THE_LEADER;
        info.formation_speed = target.current_speed;
        const float dist = leader_index == 0 ? leader_to_follower_dist : follower_to_follower_dist;
        calculate_follow_pos(info.ideal_formation_pos, objp, member_list[leader_index], target.current_dir, dist, &info.formation_dir);

        // A stuck member close to the leader stays where it is. rfg.exe compares in doubles.
        if (stuck && vec_dist_squared(objp->pos, target.obj->pos) < static_cast<double>(dist) * dist * 4.0)
        {
            info.formation_pos = objp->pos;
            continue;
        }
        info.formation_pos = info.ideal_formation_pos;
        if (squad_with_vehicle)
        {
            info.ideal_formation_pos = infos[0].obj->pos;
        }
    }
}

// 0x006342B0 (:1983)
void ai_formation::calculate_follower_positions_follow_tight()
{
    const int count = member_list.size();
    const object* leader = World.object_from_handle(leader_handle);
    const float spacing = follower_to_follower_dist;
    const float point_spacing = spacing * 0.6f;
    const float first_dist = leader_to_follower_dist * 0.6f;
    const int history_count = formation_history.size();
    const bool use_spline = history_count > 2;

    catmull_rom_spline spline;
    float end_position = -1.0f;
    float position = 0.0f;
    float last_position = 0.0f;
    vector points[21];
    if (use_spline)
    {
        points[0] = leader->pos;
        for (int i = 0; i < history_count; i++)
        {
            points[i + 1] = formation_history[i];
        }
        spline.set_points(history_count + 1, points);
        position = (first_dist - max_dist_from_history_point) / point_spacing;
        last_position = position + 1.0f;
        end_position = static_cast<float>(history_count - 1);
    }

    member_follow_info infos[15];
    infos[0].obj = World.object_from_handle(leader_handle);
    get_object_movement_dir(infos[0].current_dir, infos[0].obj, &infos[0].current_speed);
    infos[0].current_is_valid = true;

    for (int i = 1; i < count; i++)
    {
        ai_formation_member* member = member_list[i];
        follow_information& info = member->follow_info;
        const object* objp = World.object_from_handle(member->member_handle);
        infos[i].obj = const_cast<object*>(objp);
        const bool stuck = !member->stuck_time.elapsed();
        const int leader_index = stuck ? 0 : i - 1;
        member_follow_info& target = infos[leader_index];
        info.my_leader = target.obj;
        info.formation_speed = infos[0].current_speed;
        info.follow_mode = (formation_type == AI_FORMATION_FOLLOW_THE_LEADER_TIGHT && member->follow_flags.blocked) ? AI_FFM_FOLLOW_THE_LEADER_MUST_PATH : AI_FFM_FOLLOW_THE_LEADER;

        position += 1.0f;
        if (!use_spline || stuck || end_position <= position)
        {
            if (!target.current_is_valid)
            {
                get_object_movement_dir(target.current_dir, target.obj, &target.current_speed);
                target.current_is_valid = true;
            }
            const float dist = leader_index == 0 ? leader_to_follower_dist : follower_to_follower_dist;
            calculate_follow_pos(info.formation_pos, objp, member_list[leader_index], target.current_dir, dist, &info.formation_dir);
        }
        else
        {
            spline.get_point(info.formation_pos, position);
            spline.get_tangent(info.formation_dir, position);
            info.formation_dir.x *= -1.0f;
            info.formation_dir.y *= -1.0f;
            info.formation_dir.z *= -1.0f;
            info.formation_dir.normalize();
            const float over = (info.formation_dir.y * (info.formation_pos.y - objp->pos.y) + info.formation_dir.x * (info.formation_pos.x - objp->pos.x)
                                + info.formation_dir.z * (info.formation_pos.z - objp->pos.z)) - spacing;
            if (over > 0.0f)
            {
                float adjusted = position - (over / point_spacing) * 0.5f;
                if ((last_position + 0.1f) - adjusted >= 0.0f)
                {
                    adjusted = last_position + 0.1f;
                }
                if (last_position - adjusted < 0.0f)
                {
                    last_position = adjusted;
                }
                spline.get_point(info.formation_pos, adjusted);
                spline.get_tangent(info.formation_dir, adjusted);
                info.formation_dir.x *= -1.0f;
                info.formation_dir.y *= -1.0f;
                info.formation_dir.z *= -1.0f;
                info.formation_dir.normalize();
            }
        }

        info.ideal_formation_pos = info.formation_pos;
        // Right above or below the member ahead: its height, for stairs.
        const object* ahead = infos[i - 1].obj;
        if (ahead != nullptr && vec_xz_dist_squared(info.ideal_formation_pos, ahead->pos) < 1.0f && 1.0f < fabsf(info.ideal_formation_pos.y - ahead->pos.y))
        {
            info.ideal_formation_pos.y = ahead->pos.y;
            info.formation_pos.y = ahead->pos.y;
        }
    }
}

// 0x006348F0 (:2094)
void ai_formation::calculate_follower_positions_rigid()
{
    const int count = member_list.size();
    member_follow_info infos[15];
    infos[0].obj = World.object_from_handle(leader_handle);
    const bool vehicle_leader = infos[0].obj->get_obj_type() == OT_VEHICLE;

    for (int i = 1; i < count; i++)
    {
        const object* objp = World.object_from_handle(member_list[i]->member_handle);
        infos[i].obj = const_cast<object*>(objp);
        const int leader_index = Rigid_formation_definitions[formation_type][i].leader_index;
        member_follow_info& target = infos[leader_index];
        if (!target.current_is_valid)
        {
            get_object_movement_dir(target.current_dir, target.obj, &target.current_speed);
            target.current_is_valid = true;
        }
        follow_information& info = member_list[i]->follow_info;
        info.my_leader = target.obj;
        info.formation_speed = target.current_speed;
        vector origin = target.obj->pos;
        if (!target.move_dir_is_valid)
        {
            target.move_dir = member_list[leader_index]->get_dir(origin, target.current_dir);
            target.move_dir_is_valid = true;
        }
        vector bmin, bmax;
        info.my_leader->get_local_bbox(bmin, bmax);
        origin.y += bmin.y;
        info.formation_dir = target.current_dir;

        matrix orient;
        if (vehicle_leader)
        {
            orient = target.obj->orient;
        }
        else
        {
            orient.make(target.move_dir);
        }
        const float dist = leader_index == 0 ? leader_to_follower_dist : follower_to_follower_dist;
        const vector& offset = Rigid_formation_definitions[formation_type][i].offset_from_leader;
        info.ideal_formation_pos = vector(offset.x * dist, offset.y * dist, offset.z * dist);
        vec_change_to_global_coords(info.ideal_formation_pos, origin, orient);

        if (member_list[i]->follow_flags.blocked)
        {
            info.follow_mode = AI_FFM_FOLLOW_THE_LEADER;
            calculate_follow_pos(info.formation_pos, objp, member_list[leader_index], target.current_dir, dist, &info.formation_dir);
        }
        else
        {
            info.formation_pos = info.ideal_formation_pos;
            info.follow_mode = AI_FFM_NORMAL;
        }
    }
}

// 0x00634C80 (:2186)
void ai_formation::calculate_follower_positions()
{
    if (formation_type == AI_FORMATION_FLOCK)
    {
        calculate_follower_positions_flock();
    }
    else if (formation_type == AI_FORMATION_FOLLOW_THE_LEADER)
    {
        calculate_follower_positions_follow();
    }
    else if (formation_type == AI_FORMATION_FOLLOW_THE_LEADER_TIGHT)
    {
        calculate_follower_positions_follow_tight();
    }
    else if (is_rigid_formation())
    {
        calculate_follower_positions_rigid();
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
    }

    // Once a second, for the player's formations: is an ideal position inside a compound hull?
    if (Check_invalid_follow_pos_timer.valid() && !Check_invalid_follow_pos_timer.elapsed())
    {
        return;
    }
    object* leader = World.object_from_handle(leader_handle);
    for (int i = 1; i < member_list.size(); i++)
    {
        follow_information& info = member_list[i]->follow_info;
        const vehicle* vp = World.object_cast<vehicle>(leader);
        if (!object_is_player(leader) && (vp == nullptr || !object_is_player(vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER))))
        {
            continue;
        }
        if (Check_invalid_follow_pos_timer.elapsed())
        {
            info.position_invalid = false;
            uint64_t nav_cell_handle = 0xFFFFFFFFFFFFFFFFull;
            if (bb_pf_get_nav_cell(info.ideal_formation_pos, false, &nav_cell_handle, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) != nullptr)
            {
                nav_cell* cell = nav_cell_from_handle(nav_cell_handle);
                if (cell != nullptr && nav_cell_detour_is_point_in_compound_hull(info.ideal_formation_pos, cell))
                {
                    info.position_invalid = true;
                }
            }
        }
    }
    Check_invalid_follow_pos_timer.set(1000);
}
