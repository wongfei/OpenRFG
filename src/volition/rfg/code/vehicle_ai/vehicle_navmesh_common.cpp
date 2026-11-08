// Original: volition/rfg/code/vehicle_ai/vehicle_navmesh_common.cpp

#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_common.h"

#include <math.h>

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/vehicle_ai_actions.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/intersection_controller.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/vehicle_ai/vehicle_navmesh_path_create.h"
#include "volition/rfg/code/vehicle_ai/vehicle_offscreen_pathfollow.h"
#include "volition/rfg/code/vehicle_ai/vehicle_path_request.h"
#include "volition/rfg/code/vehicle_ai/vehicle_repulsor.h"
#include "volition/vlib/code/math/spline.h"

static const float COS_135_DEGREES = -0.70710677f;

// Placed in vehicle_repulsor.cpp in rfg.exe: 15 degrees and 0.3 m at 0x016EB8A8 and 0x016EB8AC.
extern float Collector_cone_half_angle;
extern float Collector_start_width_offset;

// 0x00BCEA00 (:179): the first two nodes go into the route, the rest wait in navmesh_path.
void vehicle_navmesh_common_init_path_follow(vehicle_ai* vai, navmesh_lane_spline_node* path)
{
    if (path == NULL)
    {
        return;
    }
    vehicle_free_ls_path(&vai->navmesh_path);
    vehicle_ai_flush_route(vai->route_info);
    navmesh_lane_spline_node* node = path;
    for (int i = 0; i < 2 && node != NULL; i++)
    {
        vehicle_ai_add_route_segment(vai->route_info, &node->lane, false, true, false);
        vehicle_free_ls_node(&path, node);
        node = path;
    }
    vai->navmesh_path = node;
}

// 0x00BD17D0 (:224)
navmesh_lane_spline_node* vehicle_navmesh_common_create_path(vehicle_ai* vai, const pf_calc_params& params, vehicle_path_info& path_info)
{
    vehicle_navmesh_path_service_request(vai->vehicle_handle);
    vehicle* vp = World.object_from_handle<vehicle>(vai->vehicle_handle);
    if (vp == NULL)
    {
        return NULL;
    }
    path_info.flags.off_road = 0;
    navmesh_lane_spline_node* path = vehicle_get_navmesh_path_internal(params, path_info, vp);
    if (path != NULL)
    {
        if (path_info.flags.road_spline_path)
        {
            // A road path that starts behind the vehicle: turn around first.
            cubic_bezier_spline cbs;
            cbs.set_points(2, path->lane.knots.to_pointer(), path->lane.in_handles.to_pointer(), path->lane.out_handles.to_pointer());
            const float u = cbs.get_closest_point_approx(vp->pos);
            vector tangent;
            cbs.get_tangent(tangent, u);
            tangent.flatten_normalize_safe();
            if (vp->orient.fvec.y * tangent.y + vp->orient.fvec.x * tangent.x + vp->orient.fvec.z * tangent.z < COS_135_DEGREES && !vp->vai.vai_flags.u_turn_active)
            {
                vp->vai.vai_flags.backing_up = 0;
                vp->vai.vai_flags.u_turn_active = 1;
                vp->vai.vai_flags.slowing_for_u_turn = 1;
                vp->vai.vai_flags.turning = 0;
            }
        }
        if (path_info.flags.use_road_offset)
        {
            if (path_info.closest_road_spline == 0xFFFFFFFF)
            {
                return path;
            }
            object_path_road* road = World.object_from_handle<object_path_road>(path_info.closest_road_spline);
            if (road == NULL)
            {
                return path;
            }
            if (!road->road_flags.one_way)
            {
                const float width = road->get_width(path_info.closest_road_index);
                vector bmin, bmax;
                vp->get_local_bbox(bmin, bmax);
                if (width <= bmax.x - bmin.x)
                {
                    vehicle_ai_set_road_offset(vai, width - (bmax.x - bmin.x) * 0.5f);
                    return path;
                }
                vehicle_ai_set_road_offset(vai, width * 0.5f);
                return path;
            }
        }
    }
    vai->vai_flags.m_road_offset = 0;
    return path;
}

// 0x00BD1A10 (:302)
bool vehicle_navmesh_common_update_path(vehicle_ai* vai, const pf_calc_params& params, vehicle_path_info& path_info)
{
    navmesh_lane_spline_node* path = vehicle_navmesh_common_create_path(vai, params, path_info);
    if (path == NULL)
    {
        return false;
    }
    vehicle_navmesh_common_init_path_follow(vai, path);
    return true;
}

// 0x00BC5A60 (:325)
uint64_t vehicle_navmesh_common_update_nav_cell_handle(const vector& pos, uint64_t nav_cell_handle_hint)
{
    vector adjusted;
    uint64_t handle;
    if (nav_cell_handle_hint == 0xFFFFFFFFFFFFFFFFULL)
    {
        // Pushes -1 for the hint and answers the result, -1 or not.
        bb_pf_get_nav_cell_from_hint(&adjusted, 0xFFFFFFFFFFFFFFFFULL, pos, true, &handle, false, true, NAV_CELL_TRAVERSAL_ENABLED);
        return handle;
    }
    bb_pf_get_nav_cell_from_hint(&adjusted, nav_cell_handle_hint, pos, true, &handle, false, true, NAV_CELL_TRAVERSAL_ENABLED);
    return handle == 0xFFFFFFFFFFFFFFFFULL ? nav_cell_handle_hint : handle;
}

// 0x00BD5530 (:348)
void vehicle_navmesh_common_follow_path(vehicle* vp)
{
    vehicle_ai* vai = &vp->vai;
    havok_vehicle_ai_info info;
    info.flags = 0xFFFF; // every field
    vehicle_get_ai_info(vai->vehicle_handle, info);
    unsigned int bridge_navpoint;
    const bool on_bridge = is_vehicle_on_bridge(vp, bridge_navpoint);
    vai->vai_flags.turning_in_place = 0;
    vai->current_path_info.flags.on_bridge = on_bridge;
    vehicle_ai_process_route(vai, vai->route_info, info, true, true);
    if (vehicle_ai_get_center_route_segment(vai->route_info) == NULL)
    {
        vai->set_state(VS_AI_IDLE);
        return;
    }
    if (vai->get_state() == VS_AI_IDLE)
    {
        return;
    }
    if (vai->swerve_timer.valid())
    {
        if (vai->swerve_timer.elapsed())
        {
            vai->vai_flags.m_lateral_offset = 0;
            vai->steer_info.lateral_offset = 0.0f;
            vai->swerve_timer.invalidate();
            vai->initial_swerve_timer.invalidate();
        }
        if (vai->initial_swerve_timer.elapsed())
        {
            vai->steer_info.lateral_offset *= -0.5f;
            vai->initial_swerve_timer.invalidate();
        }
    }
    process_vehicle_steering(vp, vai, info);
    if (vehicle_is_hidden(vp))
    {
        vehicle_hidden_do_movement(vai);
        return;
    }
    vehicle_ai_steer_toward_goal(vai, info);
    float curve_speed;
    float braking_decel;
    vehicle_ai_get_curvature_speed_decel(vai, curve_speed, braking_decel);
    vai->vai_flags.m_override_stop = vai->vai_flags.m_override_speed && vai->override_speed == 0.0f;
    float target_speed = curve_speed;
    if (vai->get_state() == VS_AI_PATHFIND)
    {
        target_speed = vai->vai_flags.m_override_speed ? vai->override_speed : 1000.0f;
        if (target_speed >= curve_speed)
        {
            target_speed = curve_speed;
        }
    }
    intersection_controller_update_vehicle_accel_decel(vp, target_speed, braking_decel);
    if (vai->vai_flags.m_jumping && target_speed < 40.0f)
    {
        target_speed = 40.0f;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO)
    {
        const unsigned int handle = vp->get_handle();
        automobile_set_handbrake(handle, false);
        if (vai->vai_flags.u_turn_active)
        {
            const float u_turn_speed = sqrtf(1.0f / vai->active_control_info.max_steer_curvature * vai->active_control_info.max_radial_accel * 0.5f);
            if (vai->vai_flags.slowing_for_u_turn)
            {
                if (info.speed <= u_turn_speed)
                {
                    vai->vai_flags.backing_up = 0;
                    vai->vai_flags.slowing_for_u_turn = 0;
                    vai->vai_flags.turning = 1;
                }
                else
                {
                    automobile_set_forward_or_reverse_accelerator(handle, 0.0f);
                    automobile_set_brake_pedal_input(handle, 0.7f);
                }
            }
            if (vai->vai_flags.turning)
            {
                vehicle_ai_set_accel_for_spline_path(vai, info, u_turn_speed, 0.0f);
            }
            else if (vai->vai_flags.backing_up)
            {
                automobile_set_brake_pedal_input(handle, 0.0f);
                automobile_set_forward_or_reverse_accelerator(handle, -0.75f);
            }
        }
        else if (vai->swerve_timer.valid() && vai->initial_swerve_timer.valid())
        {
            automobile_set_forward_or_reverse_accelerator(handle, 0.01f);
            automobile_set_brake_pedal_input(handle, 0.0f);
        }
        else
        {
            vehicle_ai_set_accel_for_spline_path(vai, info, target_speed, braking_decel);
            if (vai->vai_flags.m_goal_stop_override)
            {
                const float brake = vai->target_decel_to_goal / vai->active_control_info.max_braking_decel;
                if (brake > 0.6f)
                {
                    automobile_set_forward_or_reverse_accelerator(handle, 0.01f);
                    automobile_set_brake_pedal_input(handle, brake);
                }
                else
                {
                    vehicle_ai_set_accel_for_spline_path(vai, info, vai->target_speed_to_goal, 0.0f);
                }
            }
            else if (vai->vai_flags.m_override_stop)
            {
                automobile_set_forward_or_reverse_accelerator(handle, 0.01f);
                automobile_set_brake_pedal_input(handle, 0.7f);
            }
        }
        vai->steer_info.accel_val = automobile_get_accelerator_value(vai->vehicle_handle);
        vai->steer_info.brake_val = automobile_get_brake_value(vai->vehicle_handle);
        vai->steer_info.vel = info.speed;
    }
    else if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        static_cast<flyer*>(vp)->set_request_max_velocity_mag(target_speed);
        vai->steer_info.desired_vel = target_speed;
    }
    const vector goal = vai->steer_info.goal_pos;
    vai->repulsor_info.lateral_info.angle_with_fvec = Collector_cone_half_angle * 0.017453f; // degrees to radians
    vai->repulsor_info.lateral_info.start_width_offset = Collector_start_width_offset;
    vehicle_set_repulsor_lateral_limits(vp);
    unsigned int ignore_handles[2];
    int num_ignore_handles = 0;
    bool ignore_humans = false;
    bool ambient_driving = false;
    human* driver = vehicle_most_recent_driver(vp);
    if (driver != NULL && driver->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        ai_system* ai = static_cast<npc*>(driver)->get_ai();
        const ai_action* action = ai->planner.plan != NULL ? ai->planner.plan->act : NULL;
        if (action == chase_target_in_vehicle_action::get_instance())
        {
            human* target = human_combat_get_target(driver);
            if (target != NULL)
            {
                ignore_handles[0] = driver->combat.target_handle;
                num_ignore_handles = 1;
                if (World.object_from_handle<vehicle>(target->vehicle_handle) != NULL)
                {
                    ignore_handles[1] = target->vehicle_handle;
                    num_ignore_handles = 2;
                }
            }
        }
        else if (action == ambient_veh_drive_action::get_instance())
        {
            ambient_driving = true;
        }
        const object_patrol* patrol = ai->get_patrol_object();
        if (patrol != NULL)
        {
            if (vrail_is_patrol(patrol) && !vrail_in_wait_state())
            {
                ignore_humans = true;
            }
            if (patrol->is_override_patrol())
            {
                ignore_humans = true;
            }
        }
    }
    const bool ignore_buildings = vehicle_is_tank(vp);
    if (!vai->vai_flags.m_ignore_repulsors && !vai->vai_flags.turning_in_place)
    {
        vector drive_to;
        vpf_calculate_turn_to_target(drive_to, vp, goal, ignore_handles, num_ignore_handles, ignore_humans, ignore_buildings, false);
        vai->steer_info.goal_pos = drive_to;
        vehicle_ai_steer_toward_goal(vai, info);
        if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO && vp->is_ambient())
        {
            if (ambient_driving && vai->ambient_wait_timer.valid() && !vai->ambient_wait_timer.elapsed())
            {
                vehicle_set_controls_when_abandoned(vp);
            }
        }
        else if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            static_cast<flyer*>(vp)->set_request_max_velocity_mag(target_speed);
        }
    }
}

// 0x00BC5AD0 (:652)
float get_spline_path_length(navmesh_lane_spline_node* path)
{
    float length = 0.0f;
    navmesh_lane_spline_node* node = path;
    while (node != NULL)
    {
        length += node->lane.arc_length[0];
        node = node->next;
        if (node == path)
        {
            break;
        }
    }
    return length;
}
