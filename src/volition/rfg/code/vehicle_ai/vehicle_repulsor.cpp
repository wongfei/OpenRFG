// Original: volition/rfg/code/vehicle_ai/vehicle_repulsor.cpp
//
// Obstacle avoidance of AI vehicles (NPC.md NP5). A box ahead of the vehicle collects humans, fixed buildings and
// vehicles every 200 to 1500 ms (vpf_calculate_turn_to_target); each one the driver cannot ignore pushes the drive-to
// point sideways, and a driver that has to wait gets a timer here (vehicle_set_ambient_wait_timer).
//
// Plain float where rfg.exe uses double chains, like the rest of the AI, so not bit-exact.

#include "volition/rfg/code/vehicle_ai/vehicle_repulsor.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_actions.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/intersection_controller.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_lane_common.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/spline.h"

#include <math.h>

// Read by vehicle_navmesh_common_follow_path.
float Collector_cone_half_angle = 15.0f; // 0x016EB8A8
float Collector_start_width_offset = 0.3f; // 0x016EB8AC
static float Collector_cone_length = 20.0f; // 0x016EB8B0

static const vector ADDITIONAL_REPULSOR_BBOX(5.0f, 0.0f, 5.0f); // 0x016EB8F4: a courier or vrail driver looks wider
static const vector ADDITIONAL_REPULSOR_BBOX_2(-5.0f, 0.0f, 5.0f); // 0x016EB900

static bool Collect_humans; // 0x03870CA4
static bool Collect_vehicles; // 0x03870CA5
static bool Collect_buildings; // 0x03870CA6

static const float DEG_TO_RAD = 0.017453f; // the float rfg.exe widens: 0x3F91DF3300000000
static const float PASSING_WIDTH = 3.0f; // 0x01339FA0 VEHICLE_BASE_PASSING_WIDTH; also rfg.exe's merged 3.0 literal
static const float COS_45 = 0.70710677f; // 0x012C92A4
static const float MIN_SPEED = 0.01f;

// Ghidra's __CIsin calls take their argument on the FPU stack.
static inline float sin_f(float angle)
{
    return (float)sin((double)angle);
}

// float max<float>(float, float) 0x004B18E0 (min_max.h:29) is only listed as a comment in min_max.h.
static inline float max_f(float a, float b)
{
    return a > b ? a : b;
}

// UP x v, which the asm spells out at each use.
static inline vector up_cross(const vector& v)
{
    vector out;
    out.x = v.z * UP_VECTOR.y - v.y * UP_VECTOR.z;
    out.y = v.x * UP_VECTOR.z - v.z * UP_VECTOR.x;
    out.z = v.y * UP_VECTOR.x - v.x * UP_VECTOR.y;
    return out;
}

// The building test of the collect callback and the flyer's height pass: a fixed rfg_mover. Sub type 1 is rfg_mover.
static inline bool is_fixed_building(const object* objp)
{
    return objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == 1 && static_cast<const rfg_mover*>(objp)->get_move_type() == RFGM_FIXED;
}

// 0x00B64620
repulsor_lateral_info::repulsor_lateral_info()
{
}

// 0x00B76860
vehicle_repulsor_info::vehicle_repulsor_info()
{
}

// 0x00BC89D0
veh_compound_repulsor::veh_compound_repulsor()
{
}

// 0x00BC7600 (:176): LTCG, vp in EAX, box_pos in EDI; the collector box in the vehicle's frame, placed ahead of it.
static void get_repulsor_collector_bbox(const vehicle* vp, vector& bmin, vector& bmax, vector& box_pos, matrix& box_orient)
{
    vector local_min;
    vector local_max;
    if (!havok_body_get_aabb(vp->havok_handle, ZERO_VECTOR, IDENTITY_MATRIX, local_min, local_max))
    {
        vp->get_local_bbox(local_min, local_max);
        if (vp->get_sub_type() == 5)
        {
            // A flyer with no body. Original bug: local_max.x negates the caller's unset bmin.x (0x00BC7672), it was
            // meant to be 3.95. Harmless, as only y and z of these are read below.
            local_min.x = -3.95f;
            local_min.z = -9.105f;
            local_max.x = -bmin.x;
            local_max.z = 3.105f;
        }
    }
    const float half_height = (local_max.y - local_min.y) * 0.5f + 0.5f;
    const float half_width = sin_f(Collector_cone_half_angle * DEG_TO_RAD) * Collector_cone_length + Collector_start_width_offset;
    const float half_length = Collector_cone_length * 0.5f;
    bmin.set(-half_width, -half_height, -half_length);
    bmax.set(half_width, half_height, half_length);
    box_pos = vp->pos;
    const signed char sub_type = (signed char)vp->get_sub_type();
    if (sub_type == 4)
    {
        // An automobile's box swings with half its steering angle, behind it when it reverses.
        vector offset;
        matrix steer;
        if (automobile_moving_in_reverse(static_cast<const automobile*>(vp)))
        {
            const float t = automobile_get_steering_t(vp->get_handle());
            steer.make_ry(vp->info->max_speed_steering_angle * DEG_TO_RAD * 0.5f * t);
            offset = vp->orient.fvec * (local_min.z - bmax.z + 0.3f);
        }
        else
        {
            const float t = automobile_get_steering_t(vp->get_handle());
            steer.make_ry(vp->info->max_speed_steering_angle * DEG_TO_RAD * -0.5f * t);
            offset = vp->orient.fvec * (local_max.z - bmin.z - 0.3f);
        }
        box_orient = steer * vp->orient;
        offset.unrotate(steer);
        box_pos += offset;
    }
    else if (sub_type == 5)
    {
        // A flyer's box follows its velocity and reaches 5 m lower.
        const vector grow(0.0f, -5.0f, 0.0f);
        bbox_grow(bmin, bmax, grow);
        vector vel;
        vehicle_get_velocity(vp, vel);
        vel.normalize_safe();
        box_orient.make_quick(vel);
        box_pos += vel * (local_max.z - bmin.z - 0.3f);
    }
    // Original: any other sub type leaves box_orient unset. Only automobiles and flyers drive here.
}

// 0x00BC7AF0 (:248): LTCG, vp in ESI, cone_vertex in EDI.
static void get_repulsor_collector_cone_vertex(const vehicle* vp, float cone_half_angle, float end_delta_width, vector& cone_vertex)
{
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    const float back = (bmax.x + end_delta_width) / sin_f(cone_half_angle) - bmax.z;
    cone_vertex = vp->pos - vp->orient.fvec * back;
}

// 0x00BC7BE0 (:270): LTCG, lateral_info in EAX, sphere_pos in ECX.
static bool is_sphere_within_collector_planes(const repulsor_lateral_info& lateral_info, const vector& sphere_pos, float radius)
{
    return lateral_info.plane_left.distance_to_point(sphere_pos) + radius >= 0.0f && lateral_info.plane_right.distance_to_point(sphere_pos) + radius >= 0.0f;
}

// 0x00BC7CD0 (:279): LTCG, vp in EAX. Only an NPC running away refuses to wait.
static bool vehicle_driver_will_wait(const vehicle* vp)
{
    const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver == nullptr || driver->get_sub_type() != 2)
    {
        return true;
    }
    const ai_action* action = static_cast<const npc*>(driver)->get_ai()->planner.get_current_action();
    return action != ambient_veh_flee_action::get_instance() && action != flee_in_vehicle_action::get_instance() && action != flee_in_vehicle_xtreme_action::get_instance();
}

// 0x00BC7D30 (:298): LTCG, vp in EAX, is_vrail in EDI.
static bool vehicle_allowed_to_stop(const vehicle* vp, bool check_ai_modes, bool* is_vrail)
{
    if (is_vrail != nullptr)
    {
        *is_vrail = false;
    }
    npc* np = static_cast<npc*>(vehicle_passenger_in_seat(vp, VSI_DRIVER));
    if (np == nullptr || np->get_sub_type() != 2)
    {
        return false;
    }
    if (vrail_is_driver(np))
    {
        if (is_vrail != nullptr)
        {
            *is_vrail = true;
        }
        return false;
    }
    if (activity_cc_is_courier(np))
    {
        return false;
    }
    if (check_ai_modes)
    {
        const ai_system* ai = np->get_ai();
        if (ai->get_orders_type() == GFM_ORDERS_TYPE_CONVOY || ai->planner.get_current_goal() == reinforcement_goal::get_instance())
        {
            return false;
        }
    }
    return true;
}

// 0x00BCA990 (:340): LTCG, wait_obj_handle in EAX. Walks the chain of vehicles waiting on each other. Original: with
// check_ixc_queue any vehicle in the chain with the same controller counts, 0xFFFF (none) included.
static bool vehicle_ambient_wait_creates_deadlock(const vehicle* vp, unsigned int wait_obj_handle, bool check_ixc_queue)
{
    while (vp->get_handle() != wait_obj_handle)
    {
        if (wait_obj_handle == 0xFFFFFFFF)
        {
            return false;
        }
        const object* objp = World.object_from_handle(wait_obj_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
        {
            return false;
        }
        const vehicle* other = static_cast<const vehicle*>(objp);
        if (check_ixc_queue && vp->vai.active_intersection_controller == other->vai.active_intersection_controller)
        {
            return true;
        }
        if (!other->vai.ambient_wait_timer.valid() || other->vai.ambient_wait_timer.elapsed())
        {
            return false;
        }
        wait_obj_handle = other->vai.ambient_wait_object;
    }
    return true;
}

// 0x00BCAA10 (:363): LTCG, vp in EDI.
static void vehicle_set_ambient_wait_timer(vehicle* vp, int delay_ms, unsigned int wait_obj_handle)
{
    npc* np = static_cast<npc*>(vehicle_passenger_in_seat(vp, VSI_DRIVER));
    if (np == nullptr || np->get_sub_type() != 2 || vrail_is_driver(np) || activity_cc_is_courier(np))
    {
        return;
    }
    if (vp->vai.ambient_wait_timer.valid() && vp->vai.ambient_wait_timer.time_until() > delay_ms)
    {
        return;
    }
    if (!vehicle_ambient_wait_creates_deadlock(vp, wait_obj_handle, intersection_controller_vehicle_at_top_of_queue(vp)))
    {
        vp->vai.ambient_wait_timer.set(delay_ms);
        vp->vai.ambient_wait_object = wait_obj_handle;
    }
}

// 0x00BC7DA0 (:386)
void vehicle_set_repulsor_lateral_limits(vehicle* vp)
{
    repulsor_lateral_info& lateral = vp->vai.repulsor_info.lateral_info;
    vector cone_vertex;
    get_repulsor_collector_cone_vertex(vp, lateral.angle_with_fvec, lateral.start_width_offset, cone_vertex);
    matrix rotation;
    rotation.make_ry(lateral.angle_with_fvec);
    vector vel = ZERO_VECTOR;
    vehicle_get_velocity(vp, vel);
    const bool reversing = vp->orient.fvec.x * vel.x + vp->orient.fvec.y * vel.y + vp->orient.fvec.z * vel.z < 0.0f;

    // The cone's sides, turned either way from the flattened direction of travel.
    vector forward = vp->orient.fvec;
    if (reversing)
    {
        forward = -forward;
    }
    forward.y = 0.0f;
    vector side = forward.copy_rotate(rotation);
    vector normal(side.y * UP_VECTOR.z - side.z * UP_VECTOR.y, side.z * UP_VECTOR.x - side.x * UP_VECTOR.z, side.x * UP_VECTOR.y - side.y * UP_VECTOR.x);
    lateral.plane_left.set(normal.copy_normalize_safe(), cone_vertex);
    side = forward.copy_unrotate(rotation);
    normal = up_cross(side);
    lateral.plane_right.set(normal.copy_normalize_safe(), cone_vertex);

    // The box around the cone. Original: the bbox fetched here (0x00BC80B6) is never read.
    const float half_width = sin_f(lateral.angle_with_fvec) * Collector_cone_length + Collector_start_width_offset;
    vector right = vp->orient.rvec;
    if (reversing)
    {
        right = -right;
    }
    right.y = reversing ? -0.0f : 0.0f;
    right.normalize_safe();
    lateral.box_left.set(right, vp->pos - right * half_width);
    lateral.box_right.set(-right, vp->pos + right * half_width);
    lateral.box_back.set(forward, vp->pos);
}

// 0x00BC82D0 (:443): user_data is the vehicle collecting.
static bool vehicle_repulsor_collect_callback(object* hit_objp, unsigned int, const void* user_data)
{
    if (hit_objp == nullptr)
    {
        return false;
    }
    switch (hit_objp->get_obj_type())
    {
    case OT_HUMAN:
        return Collect_humans && human_in_vehicle(static_cast<human*>(hit_objp)) == nullptr;
    case OT_MOVER:
        return Collect_buildings && is_fixed_building(hit_objp);
    case OT_VEHICLE:
        return Collect_vehicles && user_data != hit_objp;
    default:
        return false;
    }
}

// 0x00BC8350 (:484): LTCG, obj in ESI. How hard an object pushes: 0 means ignore it, a mover by its mass.
static float get_object_repulsor_scalar(const object* obj)
{
    const signed char type = (signed char)obj->get_obj_type();
    if (type == OT_HUMAN || type == OT_VEHICLE)
    {
        return 1.0f;
    }
    if (obj->havok_handle == 0xFFFFFFFF)
    {
        return 0.0f;
    }
    float mass = obj->get_mass();
    if (mass == 0.0f)
    {
        // A fixed body has no mass: a mover falls back on its properties' mass, anything else on the largest float.
        mass = type == OT_MOVER ? static_cast<const object_mover*>(obj)->m_level_obj_properties->m_base_properties->m_mass : 3.4028235e+38f;
    }
    return fl_parameter(mass, 50.0f, 200.0f, true);
}

// 0x00BC8410 (:530): LTCG, obj in EAX, vel in ESI.
static void get_object_linear_velocity(const object* obj, vector& vel)
{
    switch (obj->get_obj_type())
    {
    case OT_HUMAN:
        human_controller_get_vel(static_cast<const human*>(obj), vel);
        return;
    case OT_VEHICLE:
        vehicle_get_velocity(static_cast<const vehicle*>(obj), vel);
        return;
    case OT_MOVER:
    case OT_DEBRIS:
        if (obj->havok_handle != 0xFFFFFFFF)
        {
            havok_body_get_linear_velocity(obj->havok_handle, vel);
            return;
        }
        break;
    default:
        // rfg.exe logs "Int3 critical function" with line 0x226 here.
        break;
    }
    vel.zero();
}

// 0x00BC8490 (:558): LTCG, first in ECX, second in ESI, relative_vel in EDI. Inlines the function above twice.
static void get_repulsor_relative_velocity(const object* first, const object* second, vector& relative_vel)
{
    vector first_vel;
    vector second_vel;
    get_object_linear_velocity(first, first_vel);
    get_object_linear_velocity(second, second_vel);
    relative_vel = first_vel - second_vel;
}

// 0x00BC8610 (:570): LTCG, vp in EAX, repulsor in ECX. Original: the xz_radius of the PDB's signature is not read.
static bool ignore_repulsor_moving_away(const vehicle* vp, const object* repulsor)
{
    vector relative_vel;
    get_repulsor_relative_velocity(repulsor, vp, relative_vel);
    vector direction = vp->orient.fvec;
    vector vel = ZERO_VECTOR;
    vehicle_get_velocity(vp, vel);
    if (vel.x * direction.x + vel.y * direction.y + vel.z * direction.z < 0.0f)
    {
        direction = -direction;
    }
    if (vec_equal_robust(relative_vel, ZERO_VECTOR, 0.001f))
    {
        return false;
    }
    return relative_vel.x * direction.x + relative_vel.y * direction.y + relative_vel.z * direction.z > 0.01f;
}

// 0x00BC8790 (:697): LTCG, vp in ESI, oriented_bbox in EDI.
static bool ignore_repulsor_base_check(const vehicle* vp, const object* repulsor, const repulsor_oriented_bbox& oriented_bbox)
{
    const float xz_radius = bb_pf_get_repulsor_xz_radius(repulsor, oriented_bbox);
    if (!is_sphere_within_collector_planes(vp->vai.repulsor_info.lateral_info, oriented_bbox.pos, xz_radius))
    {
        // Flyers still look at each other outside the cone.
        if (vp->get_sub_type() != 5 || repulsor->get_obj_type() != OT_VEHICLE || repulsor->get_sub_type() != 5)
        {
            return true;
        }
    }
    return ignore_repulsor_moving_away(vp, repulsor);
}

// The block of vehicle_maybe_ignore_repulsor that rfg.exe has twice inline: is the repulsor's box wider than the given
// width either side of the route at its closest point? The caller has checked that the route has segments.
static bool repulsor_straddles_route(const vehicle* vp, const object* repulsor, const repulsor_oriented_bbox& oriented_bbox, float width)
{
    const vehicle_route_info& route = vp->vai.route_info;
    const cubic_bezier_spline* spline = &route.route[route.bumper_info.route_index].cb_spline;
    float u = spline->get_closest_point_approx(repulsor->pos);
    if (u == 1.0f && route.bumper_info.route_index < route.num_route_segments - 1)
    {
        spline = &route.route[route.bumper_info.route_index + 1].cb_spline;
        u = spline->get_closest_point_approx(repulsor->pos);
    }
    vector spline_pos;
    vector tangent;
    spline->get_point(spline_pos, u);
    spline->get_tangent(tangent, u);
    matrix spline_orient;
    spline_orient.make(tangent);
    if (vp->vai.vai_flags.m_road_offset)
    {
        spline_pos += spline_orient.rvec * vp->vai.steer_info.road_offset;
    }
    // Original: the offset is not rotated into the spline's frame.
    const vector offset = spline_pos - oriented_bbox.pos;
    const matrix relative = repulsor->orient * spline_orient.copy_inverse();
    vector bmin;
    vector bmax;
    repulsor->get_local_bbox(bmin, bmax);
    vector out_min;
    vector out_max;
    bbox_transform(out_min, out_max, relative, offset, bmin, bmax);
    return out_min.x < -width && out_max.x > width;
}

// A vehicle's desired speed, three seconds of it, squared against the distance to the repulsor.
static bool repulsor_is_far(const vehicle* vp, const object* repulsor)
{
    const float reach = vp->vai.steer_info.desired_vel * 3.0f;
    return vec_dist_squared(vp->pos, repulsor->pos) > reach * reach;
}

// The case of a human in the way, split out of vehicle_maybe_ignore_repulsor.
static bool maybe_ignore_human(vehicle* vp, human* hp, const repulsor_oriented_bbox& oriented_bbox, bool debug)
{
    if (human_is_dead(hp) || human_in_vehicle(hp) != nullptr || ignore_repulsor_base_check(vp, hp, oriented_bbox))
    {
        return true;
    }
    const vehicle_path_info::vehicle_path_flags& path_flags = vp->vai.current_path_info.flags;
    if (!path_flags.road_spline_path)
    {
        if (vp->is_ambient() && path_flags.off_road && !debug && vec_xz_dist_squared(hp->pos, vp->pos) < 100.0f)
        {
            vehicle_set_ambient_wait_timer(vp, 1000, hp->get_handle());
        }
        return false;
    }
    object_path_road* road = World.object_from_handle<object_path_road>(vp->vai.current_path_info.closest_road_spline);
    if (road == nullptr)
    {
        return false;
    }
    vector closest;
    float u = road->find_closest_point_on_road(vp->pos, closest, false);
    const float vehicle_dist_sq = vec_xz_dist_squared(closest, vp->pos);
    const float width = road->get_width(u);
    // Off the road the test below is vehicle_dist_sq against 0 (XORPS at 0x00BCABAF), so the human is never ignored.
    float limit_sq = vehicle_dist_sq;
    float human_dist_sq = 0.0f;
    if (vehicle_dist_sq < width * width)
    {
        // On the road: a human further from the centre line than the vehicle's own lane is not in the way.
        // Original: the road's width at the human is fetched and thrown away (0x00BCAC09).
        u = road->find_closest_point_on_road(hp->pos, closest, false);
        human_dist_sq = vec_xz_dist_squared(closest, hp->pos);
        vector bmin;
        vector bmax;
        vp->get_local_bbox(bmin, bmax);
        const float reach = sqrtf(vehicle_dist_sq) + (bmax.x - bmin.x) * 0.5f + 1.5f;
        limit_sq = reach * reach;
    }
    if (limit_sq <= human_dist_sq)
    {
        return true;
    }
    if (!debug && vec_xz_dist_squared(hp->pos, vp->pos) < 100.0f)
    {
        vehicle_set_ambient_wait_timer(vp, 1000, hp->get_handle());
    }
    if (vp->is_ambient() && object_is_player(hp) && !debug && vec_xz_dist_squared(hp->pos, vp->pos) < 400.0f)
    {
        vehicle_set_ambient_wait_timer(vp, 1500, hp->get_handle());
        vp->vai.vai_flags.obstructed_by_player = 1;
    }
    return false;
}

// The case of a vehicle in the way that comes after the ambient and tank checks: the road rules. *result is the answer
// when this returns true; false means carry on to the driver's target test.
static bool maybe_ignore_vehicle_on_road(vehicle* vp, vehicle* other, human* driver, human* other_driver_now, bool debug, bool& result)
{
    if (other_driver_now == nullptr)
    {
        // An empty vehicle off the road's width is no obstacle.
        object_path_road* road = World.object_from_handle<object_path_road>(vp->vai.current_path_info.closest_road_spline);
        if (road != nullptr)
        {
            vector closest;
            const float u = road->find_closest_point_on_road(other->pos, closest, false);
            const float width = road->get_width(u);
            vector bmin;
            vector bmax;
            other->get_bbox(bmin, bmax);
            vector box_point;
            if (bbox_find_closest_point(box_point, bmin, bmax, closest) > width * width)
            {
                result = true;
                return true;
            }
        }
        return false;
    }

    const vehicle_path_info::vehicle_path_flags& other_flags = other->vai.current_path_info.flags;
    if (!other_flags.road_spline_path)
    {
        if (vp->vai.current_path_info.flags.on_bridge)
        {
            vehicle_set_ambient_wait_timer(vp, 1500, other->get_handle());
            result = true;
            return true;
        }
    }
    else
    {
        const unsigned int road = vp->vai.current_path_info.closest_road_spline;
        if (road != 0xFFFFFFFF && road == other->vai.current_path_info.closest_road_spline)
        {
            // Both on the same road.
            vector other_vel;
            vector vel;
            vehicle_get_velocity(other, other_vel);
            vehicle_get_velocity(vp, vel);
            vector bmin;
            vector bmax;
            vp->get_local_bbox(bmin, bmax);
            vector other_min;
            vector other_max;
            other->get_local_bbox(other_min, other_max);
            const float both = other_max.z - other_min.z + (bmax.z - bmin.z);
            const float length_sq = both * both;
            bool oncoming = false;
            const float dot = other->orient.fvec * vp->orient.fvec;
            const bool other_turning_alone = other->vai.vai_flags.u_turn_active && !vp->vai.vai_flags.u_turn_active;
            if (dot > 0.1f || other_turning_alone)
            {
                // Same way, or the other is making a u turn.
                if (other_vel.mag_squared() < 0.1f || other_turning_alone)
                {
                    // It is stopped: wait unless there is room to pass. "Room" is its position in vp's frame.
                    vector relative = other->pos - vp->pos;
                    relative.unrotate(vp->orient);
                    const bool room = relative.x > 0.0f && fabsf(relative.z) < 2.0f;
                    if (!debug && !room && (vec_xz_dist_squared(other->pos, vp->pos) < 225.0f || other_turning_alone))
                    {
                        vehicle_set_ambient_wait_timer(vp, 750, other->get_handle());
                    }
                }
                else if (vp->is_ambient() && vel.mag_squared() > other_vel.mag_squared())
                {
                    // Catching up: drop to its speed. Original: 0 when vehicle_get_speed fails.
                    float speed = 0.0f;
                    if (vehicle_get_speed(other, speed))
                    {
                        speed = max_f(speed, other_vel.mag());
                    }
                    ai_vehicle_override_speed(vp, speed);
                }
                else if (vp->is_ambient() && length_sq > vec_xz_dist_squared(vp->pos, other->pos))
                {
                    // Bumper to bumper: half its speed.
                    float speed = 0.0f;
                    if (vehicle_get_speed(other, speed))
                    {
                        speed = max_f(speed * 0.5f, other_vel.mag() * 0.5f);
                    }
                    ai_vehicle_override_speed(vp, speed);
                }
            }
            else if (dot < 0.0f)
            {
                oncoming = true;
                if (other->vai.repulsor_info.affected_by_repulsors)
                {
                    result = false;
                    return true;
                }
            }
            if (vp->is_ambient())
            {
                result = true;
                return true;
            }
            result = false;
            if (driver != nullptr && driver->get_sub_type() == 2 && oncoming)
            {
                const gfm_orders* orders = static_cast<npc*>(driver)->get_ai()->get_orders();
                result = orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DRIVE_BY;
            }
            return true;
        }
    }

    // Different roads, or the other is not on one: make room for a patrol vrail or a convoy.
    npc* other_np = World.object_cast<npc>(other_driver_now);
    if (other_np == nullptr)
    {
        return false;
    }
    object_squad* squad = other_np->get_ai()->get_squad();
    if (squad == nullptr)
    {
        return false;
    }
    const gfm_orders* orders = squad->get_squad_orders();
    if (orders == nullptr)
    {
        return false;
    }
    const gfm_orders_types orders_type = orders->get_orders_type();
    bool make_room = orders_type == GFM_ORDERS_TYPE_CONVOY;
    if (orders_type == GFM_ORDERS_TYPE_PATROL && vrail_is_driver(other_np))
    {
        if (!vrail_in_wait_state() || vp->orient.fvec * other->orient.fvec > COS_45)
        {
            make_room = true;
        }
    }
    if (!make_room || !vp->is_ambient())
    {
        return false;
    }
    result = false;
    if (debug)
    {
        return true;
    }
    const float dot = other->orient.fvec * vp->orient.fvec;
    if (dot > COS_45 || dot < -COS_45)
    {
        const bool already = vp->vai.vai_flags.pull_off != 0;
        vp->vai.vai_flags.pull_off = 1;
        vp->vai.pull_off_timer.set(5000);
        if (driver != nullptr && driver->get_sub_type() == 2 && !already)
        {
            static_cast<npc*>(driver)->get_ai()->planner.request_replan();
        }
    }
    else
    {
        vehicle_set_ambient_wait_timer(vp, 1500, other->get_handle());
    }
    return true;
}

// The case of a vehicle in the way, split out of vehicle_maybe_ignore_repulsor.
static bool maybe_ignore_vehicle(vehicle* vp, vehicle* other, const repulsor_oriented_bbox& oriented_bbox, bool will_wait, bool debug)
{
    if (ignore_repulsor_base_check(vp, other, oriented_bbox))
    {
        return true;
    }
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    human* other_driver = vehicle_passenger_in_seat(other, VSI_DRIVER);
    if (vp->is_ambient())
    {
        if (other_driver != nullptr && object_is_player(other_driver))
        {
            vehicle_set_ambient_wait_timer(vp, 1000, other->get_handle());
            if (will_wait)
            {
                return true;
            }
        }
        if (vp->vai.current_path_info.flags.road_spline_path
            && (other_driver == nullptr || other->vai.ambient_wait_timer.valid() || other->last_forward_velocity < vp->vai.steer_info.desired_vel * 0.25f)
            && other->orient.fvec * vp->orient.fvec > 0.0f
            && vp->vai.route_info.num_route_segments > 0)
        {
            // Stuck or parked ahead on the route: go round if it leaves room, wait if not.
            const float width = other_driver != nullptr ? 1.0f : PASSING_WIDTH;
            if (repulsor_straddles_route(vp, other, oriented_bbox, width))
            {
                if (repulsor_is_far(vp, other) && will_wait)
                {
                    return true;
                }
                vehicle_set_ambient_wait_timer(vp, 500, other->get_handle());
                if (will_wait)
                {
                    return true;
                }
            }
        }
    }
    else if (driver != nullptr && driver->get_sub_type() == 2)
    {
        // A drive by at green alert lets the player's vehicle pass behind it.
        const gfm_orders* orders = static_cast<npc*>(driver)->get_ai()->get_orders();
        if (gsm_get_alert_level() == ALERT_LEVEL_GREEN && orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DRIVE_BY
            && other_driver != nullptr && object_is_player(other_driver) && other->orient.fvec * vp->orient.fvec < 0.0f)
        {
            return true;
        }
    }

    if (driver != nullptr && driver->get_sub_type() == 2)
    {
        if (static_cast<npc*>(driver)->npc_flags.ignore_vehicles)
        {
            return true;
        }
        bool is_vrail = false;
        if (!vehicle_allowed_to_stop(vp, true, &is_vrail) && other->is_ambient())
        {
            const float dot = other->orient.fvec * vp->orient.fvec;
            if (dot > COS_45 || dot < -COS_45)
            {
                // An ambient vehicle in line with a driver who cannot stop pulls off the road.
                const bool already = other->vai.vai_flags.pull_off != 0;
                other->vai.vai_flags.pull_off = 1;
                other->vai.pull_off_timer.set(5000);
                // Original bug: other_driver is read with no null check (0x00BCB560).
                if (other_driver->get_sub_type() == 2 && !already)
                {
                    static_cast<npc*>(other_driver)->get_ai()->planner.request_replan();
                }
            }
            else if (!debug)
            {
                vehicle_set_ambient_wait_timer(vp, 500, other->get_handle());
            }
        }
        // Original: the is_vrail out parameter shares its byte with will_wait, so from here on a vrail driver ignores
        // the vehicle whatever will_wait said.
        if (is_vrail)
        {
            return true;
        }
    }

    if (vehicle_is_tank(vp) && !other->m_flags.m_corpse && driver != nullptr && driver->get_sub_type() == 2)
    {
        // A tank shoots what is in its way.
        const npc* np = static_cast<const npc*>(driver);
        human* other_driver_now = vehicle_passenger_in_seat(other, VSI_DRIVER);
        if (np->npc_flags.fire_on_vehicles && (!np->npc_flags.ignore_player || !object_is_player(other_driver_now)))
        {
            ai_system* ai = static_cast<npc*>(driver)->get_ai();
            if (World.object_from_handle(ai->desired_target) == nullptr)
            {
                const human_teams other_team = other_driver_now != nullptr ? other_driver_now->current_team : other->info->default_team;
                if (!human_teams_are_allies(other_team, driver->current_team))
                {
                    ai->desired_target = other->get_handle();
                    ai->desired_target_recalc_timestamp.set(2000);
                    ai->flags.clear_desired_target_after_timeout = true;
                    return true;
                }
            }
            else if (ai->desired_target == other->get_handle())
            {
                return true;
            }
        }
    }

    if (vp->vai.current_path_info.flags.road_spline_path && !other->m_flags.m_corpse)
    {
        bool result = false;
        if (maybe_ignore_vehicle_on_road(vp, other, driver, vehicle_passenger_in_seat(other, VSI_DRIVER), debug, result))
        {
            return result;
        }
    }

    // A driver does not swerve round the vehicle it is after.
    return driver != nullptr && driver->combat.target_handle != 0xFFFFFFFF && other_driver != nullptr && driver->combat.target_handle == other_driver->get_handle();
}

// 0x00BCAAA0 (:731): LTCG, dest is dropped from the call. debug is false from the one caller. Answers true when the
// repulsor does not push; a driver held up may get its wait timer here. The human, vehicle and mover cases are helpers
// here, where rfg.exe has one switch.
static bool vehicle_maybe_ignore_repulsor(vehicle* vp, const vector&, object* repulsor, const repulsor_oriented_bbox& oriented_bbox, bool debug)
{
    if (repulsor == vp || get_object_repulsor_scalar(repulsor) == 0.0f)
    {
        return true;
    }
    const bool will_wait = vehicle_driver_will_wait(vp);
    switch (repulsor->get_obj_type())
    {
    case OT_HUMAN:
        return maybe_ignore_human(vp, static_cast<human*>(repulsor), oriented_bbox, debug);
    case OT_VEHICLE:
        return maybe_ignore_vehicle(vp, static_cast<vehicle*>(repulsor), oriented_bbox, will_wait, debug);
    case OT_MOVER:
        if (static_cast<const object_mover*>(repulsor)->mover_is_type(MOVER_TYPE_DYNAMIC))
        {
            // A dynamic mover: wait behind one across the route, ignore it otherwise.
            if (vp->vai.route_info.num_route_segments > 0 && repulsor_straddles_route(vp, repulsor, oriented_bbox, PASSING_WIDTH))
            {
                if (will_wait && repulsor_is_far(vp, repulsor))
                {
                    return true;
                }
                vehicle_set_ambient_wait_timer(vp, 500, repulsor->get_handle());
                if (will_wait)
                {
                    return true;
                }
            }
            return false;
        }
        // Only a flyer steers round fixed buildings.
        if (is_fixed_building(repulsor) && vp->get_sub_type() != 5)
        {
            return true;
        }
        if (vp->vai.current_path_info.flags.on_bridge)
        {
            return true;
        }
        // fall through: the debris case
    case OT_DEBRIS:
        if (ignore_repulsor_base_check(vp, repulsor, oriented_bbox))
        {
            return true;
        }
        return vp->vai.current_path_info.flags.on_bridge != 0;
    default:
        return true;
    }
}

// 0x00BCBCC0 (:1328): LTCG, repulsor in ESI, driver in EDI.
static void calculate_compound_repulsor_bounding_sphere(veh_compound_repulsor& repulsor, human* driver)
{
    repulsor.pos = repulsor.oriented_bbox.pos;
    repulsor.radius = bb_pf_get_repulsor_xz_radius(repulsor.objp, repulsor.oriented_bbox);
    npc* np = driver != nullptr && driver->get_obj_type() == OT_HUMAN && driver->get_sub_type() == 2 ? static_cast<npc*>(driver) : nullptr;
    object* objp = repulsor.objp;
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (object_is_player(objp))
        {
            // Give the player room, plenty of it when fleeing from them.
            if (np != nullptr && ai_get_flee_to_safety_belief(np) != nullptr)
            {
                repulsor.radius *= 10.0f;
                return;
            }
        }
        else if (driver == nullptr || driver == objp || human_teams_get_disposition(driver, static_cast<human*>(objp)) < TEAM_DISPOSITION_FRIENDLY)
        {
            return;
        }
        repulsor.radius *= 3.0f;
    }
    else if (objp->get_obj_type() == OT_VEHICLE && vehicle_is_on_fire(static_cast<vehicle*>(objp)))
    {
        repulsor.radius += 1.5f;
    }
}

// 0x00BCBDB0 (:1377): LTCG, repulse_info in EAX; pos and dest are dropped from the call.
static int create_compound_repulsors(veh_compound_repulsor* compound_repulsors, vehicle* vp, const vector& dest, vehicle_repulsor_info& repulse_info, const unsigned int* ignore_handle, int num_ignore_handle, human* driver)
{
    int count = 0;
    const int num_repulsors = repulse_info.num_path_repulsors;
    for (int i = 0; i < num_repulsors; i++)
    {
        const unsigned int handle = repulse_info.vpath_repulsor_handles[i];
        bool skip = false;
        for (int j = 0; j < num_ignore_handle; j++)
        {
            if (ignore_handle[j] == handle)
            {
                skip = true;
            }
        }
        if (skip)
        {
            continue;
        }
        object* repulsor = World.object_from_handle(handle);
        repulsor_oriented_bbox oriented_bbox;
        if (repulsor == nullptr || !bb_pf_get_repulsor_oriented_bounding_box(&oriented_bbox, repulsor))
        {
            continue;
        }
        if (vehicle_maybe_ignore_repulsor(vp, dest, repulsor, oriented_bbox, false))
        {
            // Ignored now: one that pushed in the last half second still fades out.
            if (vp->vai.current_path_info.flags.on_bridge || !repulse_info.vpath_repulsor_time[i].valid() || repulse_info.vpath_repulsor_time[i].time_until() <= 0)
            {
                continue;
            }
        }
        else
        {
            repulse_info.vpath_repulsor_time[i].set(500);
        }
        compound_repulsors[count].objp = repulsor;
        compound_repulsors[count].oriented_bbox = oriented_bbox;
        count++;
    }
    for (int i = 0; i < count; i++)
    {
        calculate_compound_repulsor_bounding_sphere(compound_repulsors[i], driver);
    }
    return count;
}

// The end of both cases of calculate_vehicle_repulsion_vector.
static bool finish_repulsion_vector(vector& repulsion_vector, const vector& steer, float magnitude, const vector& to_repulsor, float dist)
{
    repulsion_vector = up_cross(steer) * magnitude;
    if (vec_delta_ang(to_repulsor, steer, UP_VECTOR) < 0.0f)
    {
        repulsion_vector.negate();
    }
    // Original: dist came from mag_and_normalize, so this is false in every case and the caller ignores it anyway.
    return dist < 0.0f;
}

// The factor both cases compute: how much the repulsor lies along the way to the destination.
static float repulsor_alignment(const vector& closest_on_dest_line, const vector& pos, const vector& dir_to_dest_xz, float dist_sq_to_dest_line, float radius, bool flatten)
{
    if (dist_sq_to_dest_line < radius * radius)
    {
        return 1.0f;
    }
    if (vec_dist_squared(closest_on_dest_line, pos) < 0.01f)
    {
        return 1.0f;
    }
    vector to_closest = closest_on_dest_line - pos;
    if (flatten)
    {
        to_closest.y = 0.0f;
    }
    to_closest.normalize();
    const float alignment = to_closest * dir_to_dest_xz;
    return alignment >= 0.0f ? alignment : 0.0f;
}

// 0x00BD1A40 (:1461): LTCG, vp in EDI. Always clears repulsion_vector first, and leaves it zero on any early return.
static bool calculate_vehicle_repulsion_vector(vector& repulsion_vector, vehicle* vp, const vector& pos, const veh_compound_repulsor& repulsor, const vector& dir_to_dest_xz)
{
    repulsion_vector.zero();
    vector bmin;
    vector bmax;
    vector closest;
    if (vp->get_sub_type() == 5)
    {
        // A flyer works in 3D.
        vector to_repulsor = repulsor.pos - pos;
        const float dist = to_repulsor.mag_and_normalize();
        const vector fvec = vp->orient.fvec;
        const float scalar = get_object_repulsor_scalar(repulsor.objp);
        if (scalar == 0.0f)
        {
            return false;
        }
        vp->get_local_bbox(bmin, bmax);
        vector vel;
        vector repulsor_vel;
        get_object_linear_velocity(vp, vel);
        const float speed = vel.mag();
        get_object_linear_velocity(repulsor.objp, repulsor_vel);
        const vector relative = repulsor_vel - vel;
        if (speed < MIN_SPEED)
        {
            return false;
        }
        const float closing = -relative.x * fvec.x - relative.y * fvec.y - relative.z * fvec.z;
        if (closing < MIN_SPEED)
        {
            return false;
        }
        const vector start = vp->pos;
        const vector end = start - relative;
        if (vec_find_closest_dist_squared_to_line(repulsor.pos, start, end, &closest) > repulsor.radius * repulsor.radius)
        {
            return false;
        }
        float relative_speed = relative.mag();
        if (relative_speed - MIN_SPEED < 0.0f)
        {
            relative_speed = MIN_SPEED;
        }
        const float time = (closest - start).mag() / relative_speed;
        if (time > 3.0f)
        {
            return false;
        }
        const vector vp_future = vp->pos + vel * time;
        const vector repulsor_future = repulsor.pos + repulsor_vel * time;
        const float advance = time * speed;
        vector advanced_pos;
        vehicle_ai_get_advanced_spline_pos(&vp->vai, advance, advanced_pos);
        if ((vp_future - repulsor_future).mag() - repulsor.radius - bmax.x > 4.0f)
        {
            return false;
        }
        float clearance = dist - bmax.x - repulsor.radius;
        if (clearance - 1.0f < 0.0f)
        {
            clearance = 1.0f;
        }
        if (clearance > 25.0f)
        {
            return false;
        }
        vector on_dest_line;
        const float line_dist_sq = vec_find_closest_dist_squared_to_line(repulsor.pos, pos, pos + dir_to_dest_xz, &on_dest_line);
        const float alignment = repulsor_alignment(on_dest_line, pos, dir_to_dest_xz, line_dist_sq, repulsor.radius, false);
        float magnitude = (closing - 2.0f < 0.0f ? 2.0f : closing) * repulsor.radius * alignment * scalar / clearance;
        if (fl_is_nan(magnitude) || magnitude > 10000.0f)
        {
            magnitude = 10000.0f;
        }
        vector steer;
        if (advance > vp->vai.steer_info.lookahead_dist)
        {
            steer = advanced_pos - vp->pos;
            steer.normalize_safe();
        }
        else
        {
            steer = dir_to_dest_xz;
        }
        return finish_repulsion_vector(repulsion_vector, steer, magnitude, to_repulsor, dist);
    }

    // Anything on the ground works flat.
    vector to_repulsor(repulsor.pos.x - pos.x, 0.0f, repulsor.pos.z - pos.z);
    if (to_repulsor == ZERO_VECTOR)
    {
        return false;
    }
    const float dist = to_repulsor.mag_and_normalize();
    const vector fvec(vp->orient.fvec.x, 0.0f, vp->orient.fvec.z);
    if (fvec == ZERO_VECTOR)
    {
        return false;
    }
    const float scalar = get_object_repulsor_scalar(repulsor.objp);
    if (scalar == 0.0f)
    {
        return false;
    }
    vp->get_local_bbox(bmin, bmax);
    vector vel;
    vector repulsor_vel;
    get_object_linear_velocity(vp, vel);
    const float speed = sqrtf(vel.x * vel.x + vel.z * vel.z);
    get_object_linear_velocity(repulsor.objp, repulsor_vel);
    const vector relative(repulsor_vel.x - vel.x, 0.0f, repulsor_vel.z - vel.z);
    if (speed < MIN_SPEED)
    {
        return false;
    }
    const float closing = -relative.x * fvec.x - relative.z * fvec.z;
    if (closing < MIN_SPEED)
    {
        return false;
    }
    const vector start(vp->pos.x, 0.0f, vp->pos.z);
    const vector end = start - relative;
    const vector flat_repulsor(repulsor.pos.x, 0.0f, repulsor.pos.z);
    if (vec_find_closest_dist_squared_to_line(flat_repulsor, start, end, &closest) > 100.0f)
    {
        return false;
    }
    float relative_speed = relative.mag();
    if (relative_speed - MIN_SPEED < 0.0f)
    {
        relative_speed = MIN_SPEED;
    }
    const float time = (closest - start).mag() / relative_speed;
    if (time > 3.0f)
    {
        return false;
    }
    const float advance = time * speed;
    vector advanced_pos;
    vehicle_ai_get_advanced_spline_pos(&vp->vai, advance, advanced_pos);
    float clearance = dist - bmax.x - repulsor.radius;
    if (clearance - 1.0f < 0.0f)
    {
        clearance = 1.0f;
    }
    if (clearance > 25.0f)
    {
        return false;
    }
    vector on_dest_line;
    const float line_dist_sq = vec_find_closest_dist_squared_to_line(repulsor.pos, pos, pos + dir_to_dest_xz, &on_dest_line);
    const float alignment = repulsor_alignment(on_dest_line, pos, dir_to_dest_xz, line_dist_sq, repulsor.radius, true);
    float magnitude = (closing - 2.0f < 0.0f ? 2.0f : closing) * repulsor.radius * alignment * scalar / clearance;
    if (fl_is_nan(magnitude) || magnitude > 10000.0f)
    {
        magnitude = 10000.0f;
    }
    vector steer;
    vector center = vp->pos;
    const vehicle_path_info::vehicle_path_flags& path_flags = vp->vai.current_path_info.flags;
    const bool on_road = path_flags.road_spline_path && !path_flags.off_road;
    if (on_road)
    {
        // Road: measure from the point on the route the vehicle is steering by.
        const route_segment* segment;
        int num_segments;
        vehicle_ai_get_route_segment_info(vp->vai.route_info, segment, num_segments);
        const float u = vp->vai.route_info.center_info.u;
        spline_common_get_position_on_spline(segment->base_spline, u, center);
        vector lateral;
        get_steering_lateral_offset(&vp->vai, segment->base_spline, u, vp->pos, lateral);
        vector vertical;
        get_steering_vertical_offset(vertical, vp, &vp->vai);
        center += lateral + vertical;
    }
    if (on_road || advance > vp->vai.steer_info.lookahead_dist)
    {
        const vector delta = advanced_pos - center;
        steer.set(delta.x, 0.0f, delta.z);
        steer.normalize_safe();
    }
    else
    {
        steer = dir_to_dest_xz;
    }
    return finish_repulsion_vector(repulsion_vector, steer, magnitude, to_repulsor, dist);
}

// 0x00BC88D0 (:1904): LTCG, vp in ECX.
static int get_repulsor_index_from_compound_repulsor(const vehicle* vp, const veh_compound_repulsor& repulsor)
{
    const vehicle_repulsor_info& info = vp->vai.repulsor_info;
    for (int i = 0; i < info.num_path_repulsors; i++)
    {
        if (repulsor.objp->get_handle() == info.vpath_repulsor_handles[i])
        {
            return i;
        }
    }
    return -1;
}

// 0x00BC8910 (:1916): drops the repulsors whose push has ended, then adds the new ones with an unset timer.
static void update_active_repulsor_list(int num_repulsors, const unsigned int* repulsor_handles, vehicle_repulsor_info& repulse_info)
{
    int kept = 0;
    for (int i = 0; i < repulse_info.num_path_repulsors; i++)
    {
        if (repulse_info.vpath_repulsor_time[i].valid() && repulse_info.vpath_repulsor_time[i].time_until() > 0)
        {
            if (kept != i)
            {
                repulse_info.vpath_repulsor_handles[kept] = repulse_info.vpath_repulsor_handles[i];
                repulse_info.vpath_repulsor_time[kept] = repulse_info.vpath_repulsor_time[i];
            }
            kept++;
        }
    }
    int count = kept;
    for (int i = 0; i < num_repulsors; i++)
    {
        // Original: only the kept entries are searched, so a handle listed twice by the collector is added twice.
        bool known = false;
        for (int j = 0; j < kept; j++)
        {
            if (repulse_info.vpath_repulsor_handles[j] == repulsor_handles[i])
            {
                known = true;
                break;
            }
        }
        if (!known && count < 10)
        {
            repulse_info.vpath_repulsor_handles[count] = repulsor_handles[i];
            repulse_info.vpath_repulsor_time[count].invalidate();
            count++;
        }
    }
    repulse_info.num_path_repulsors = (unsigned char)count;
}

// A flyer climbs over the fixed buildings it has collected: the bbox of the building, then vehicle_ai::building_offset.
static void raise_flyer_over_buildings(vehicle* vp, const vehicle_repulsor_info& repulse_info)
{
    vector vp_min;
    vector vp_max;
    vp->get_local_bbox(vp_min, vp_max);
    for (int i = 0; i < repulse_info.num_path_repulsors; i++)
    {
        object* objp = World.object_from_handle(repulse_info.vpath_repulsor_handles[i]);
        if (objp == nullptr || !is_fixed_building(objp))
        {
            continue;
        }
        vector bmin;
        vector bmax;
        objp->get_local_bbox(bmin, bmax);
        const vector size = bmax - bmin;
        const vector center_local = (bmin + bmax) * 0.5f;
        const vector center = center_local.copy_unrotate(objp->orient) + objp->pos;
        // Original: the box's low corner is computed the same way and never read (0x00BD3672).
        vector top = size * 0.5f;
        top.unrotate(objp->orient);
        top += center;
        vehicle_ai* vai = vehicle_get_vehicle_ai(vp->get_handle());
        if (vai != nullptr)
        {
            const float height = vp_max.y - vp_min.y;
            if (vp->pos.y < vp_max.y + top.y - vp_min.y + 5.0f)
            {
                vai->building_offset = max_f(height + top.y + 10.0f - vp->pos.y, vai->building_offset);
            }
        }
    }
}

// 0x00BD2FB0 (:2044)
void vpf_calculate_turn_to_target(vector& drive_to_pos, vehicle* vp, const vector& dest, unsigned int* ignore_handle, int num_ignore_handle, bool ignore_humans, bool ignore_buildings, bool ignore_vehicles)
{
    vehicle_repulsor_info& repulse_info = vp->vai.repulsor_info;
    const bool is_flyer = vp->get_sub_type() == 5;
    if (is_flyer)
    {
        vehicle_get_vehicle_ai(vp->get_handle())->building_offset = 0.0f;
    }
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    npc* np = driver != nullptr && driver->get_sub_type() == 2 ? static_cast<npc*>(driver) : nullptr;

    // Start from the nav mesh under the vehicle, but keep its own height.
    nav_cell* start_cell = nullptr;
    vector start_pos;
    if (!vp->vai.current_path_info.flags.on_bridge)
    {
        start_cell = bb_pf_get_nav_cell_from_hint(&start_pos, vp->m_navcell_info.last_visited_nav_cell_handle, vp->pos, true, &vp->m_navcell_info.last_visited_nav_cell_handle, false, false, NAV_CELL_TRAVERSAL_ENABLED);
    }
    if (start_cell == nullptr)
    {
        start_pos = vp->pos;
    }
    start_pos.y = vp->pos.y;
    vector drive_pos = dest;

    const vector to_dest = dest - start_pos;
    float speed = 0.0f;
    vehicle_get_speed(vp, speed);
    vector dir_to_dest(to_dest.x, is_flyer ? to_dest.y : 0.0f, to_dest.z);
    const float dist_to_dest = dir_to_dest.mag();

    if (dist_to_dest >= 0.1f && (speed != 0.0f || vp->is_ambient()))
    {
        dir_to_dest *= 1.0f / dist_to_dest;
        bool collect_done = true;
        if (!repulse_info.path_repulsors_collection_time.valid() || repulse_info.path_repulsors_collection_time.elapsed())
        {
            if (ignore_humans && ignore_buildings && ignore_vehicles)
            {
                collect_done = false;
            }
            else
            {
                Collect_humans = !ignore_humans;
                Collect_buildings = !ignore_buildings;
                Collect_vehicles = !ignore_vehicles;
                vector bmin;
                vector bmax;
                vector box_pos;
                matrix box_orient;
                get_repulsor_collector_bbox(vp, bmin, bmax, box_pos, box_orient);
                if (np != nullptr && (activity_cc_is_courier(np) || vrail_is_driver(np)))
                {
                    bbox_grow(bmin, bmax, ADDITIONAL_REPULSOR_BBOX);
                    bbox_grow(bmin, bmax, ADDITIONAL_REPULSOR_BBOX_2);
                }
                int num_collected = 0;
                unsigned int collected[10];
                bool (*callback)(object*, unsigned int, const void*) = vehicle_repulsor_collect_callback;
                vehicle_collect_objects_fast(bmin, bmax, box_pos, box_orient, COLLIDE_OBJECT_COLLECTOR_NO_ITEMS, num_collected, 10, collected, callback, vp);
                update_active_repulsor_list(num_collected, collected, repulse_info);
                // The faster the vehicle the sooner it looks again: 200 to 1500 ms. Draws from the random stream.
                int delay = (int)((float)int_rand_range(1000, 1500) / speed);
                if (delay <= 200)
                {
                    delay = 200;
                }
                else if (delay >= 1500)
                {
                    delay = 1500;
                }
                repulse_info.path_repulsors_collection_time.set(delay);
            }
        }

        if (collect_done && repulse_info.num_path_repulsors != 0)
        {
            if (is_flyer)
            {
                raise_flyer_over_buildings(vp, repulse_info);
            }
            vp->vai.vai_flags.obstructed_by_player = 0;
            veh_compound_repulsor compound[10];
            const int num_compound = create_compound_repulsors(compound, vp, dest, repulse_info, ignore_handle, num_ignore_handle, driver);
            if (num_compound > 0)
            {
                vector total(0.0f, 0.0f, 0.0f);
                for (int i = 0; i < num_compound; i++)
                {
                    vector repulsion;
                    calculate_vehicle_repulsion_vector(repulsion, vp, start_pos, compound[i], dir_to_dest);
                    const int index = get_repulsor_index_from_compound_repulsor(vp, compound[i]);
                    if (index != -1)
                    {
                        // Fades over the half second the timer was set to.
                        total += repulsion * ((float)repulse_info.vpath_repulsor_time[index].time_until() / 500.0f);
                    }
                }
                vector offset = dest - start_pos;
                const float length = offset.mag();
                bool moved = false;
                if (total != ZERO_VECTOR)
                {
                    // Original: rfg.exe normalizes copies of fvec and rvec here (0x00BD3B9F) and never reads them.
                    moved = true;
                    const float total_mag = total.mag();
                    if (total_mag > length)
                    {
                        total *= length / total_mag;
                    }
                    offset += total;
                }
                const vector pushed = offset + start_pos;
                drive_pos = pushed;
                if (moved && start_cell != nullptr)
                {
                    // Keep the point on the nav mesh: as far towards it as the mesh allows from the destination.
                    vector dest_on_cell;
                    uint64_t dest_handle;
                    nav_cell* dest_cell = bb_pf_get_nav_cell_from_hint(&dest_on_cell, nav_cell_get_handle(start_cell), dest, true, &dest_handle, true, false, NAV_CELL_TRAVERSAL_ENABLED);
                    if (dest_cell != nullptr)
                    {
                        nav_cell_longest_los_input input;
                        nav_cell_longest_los_output output;
                        input.start_pos = dest_on_cell;
                        input.start_nav_cell = dest_cell;
                        input.delta_2d.x = pushed.x - dest.x;
                        input.delta_2d.y = pushed.z - dest.z;
                        input.flags.ignore_detour_hulls = 1;
                        input.flags.use_light_nav_cells = is_flyer;
                        input.flags.use_disabled_nav_cells = is_flyer;
                        nav_cell_find_longest_line_of_sight_point(output, input);
                        drive_pos = output.end_pos;
                        if (is_flyer)
                        {
                            drive_pos.y = pushed.y;
                        }
                    }
                }
            }
        }
    }
    repulse_info.affected_by_repulsors = drive_pos != dest;
    drive_to_pos = drive_pos;
}

// 0x00BC89E0 (:2360)
void vehicle_update_collector_info(vehicle* vp)
{
    vp->vai.repulsor_info.lateral_info.angle_with_fvec = Collector_cone_half_angle * DEG_TO_RAD;
    vp->vai.repulsor_info.lateral_info.start_width_offset = Collector_start_width_offset;
    vehicle_set_repulsor_lateral_limits(vp);
}
