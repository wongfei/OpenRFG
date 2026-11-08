// Original: volition/rfg/code/vehicle_ai/intersection_controller.cpp
//
// Queues ambient vehicles at road junctions (NPC.md NP5). A vehicle nearing a road connection joins the controller of
// the two road points, the queue is kept sorted by distance to the junction, and a vehicle whose path might cross one
// ahead of it in the queue brakes to stop short of the junction. Plain float where rfg.exe uses double chains.

#include "volition/rfg/code/vehicle_ai/intersection_controller.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"
#include "volition/rfg/code/ai/despawn_actions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"

#include <math.h>
#include <stdlib.h>

static const int MAX_INTERSECTION_CONTROLLERS = 20;
static const int MAX_QUEUE = 10;
static const unsigned short NO_CONTROLLER = 0xFFFF;

static intersection_controller Intersection_controllers[MAX_INTERSECTION_CONTROLLERS]; // 0x03870CA8
static unsigned short Active_intersection_controllers = NO_CONTROLLER; // 0x016EB850
static unsigned short Free_intersection_controllers = NO_CONTROLLER; // 0x016EB854
static vector Ixc_center_pos; // 0x038822E0: where ixc_vehicle_sort measures from

static inline unsigned short ixc_index(const intersection_controller* ixcp)
{
    return (unsigned short)(ixcp - Intersection_controllers);
}

// Removes a controller from a circular list; the list's head may move. Inlined at each use in rfg.exe.
static void ixc_list_remove(unsigned short& head, intersection_controller* ixcp)
{
    const unsigned short index = ixc_index(ixcp);
    if (head == index)
    {
        head = ixcp->next;
        if (head == index)
        {
            head = NO_CONTROLLER;
            ixcp->prev = NO_CONTROLLER;
            ixcp->next = NO_CONTROLLER;
            return;
        }
    }
    Intersection_controllers[ixcp->prev].next = ixcp->next;
    Intersection_controllers[ixcp->next].prev = ixcp->prev;
    ixcp->prev = NO_CONTROLLER;
    ixcp->next = NO_CONTROLLER;
}

// Adds a controller at the tail of a circular list.
static void ixc_list_append(unsigned short& head, intersection_controller* ixcp)
{
    const unsigned short index = ixc_index(ixcp);
    if (head == NO_CONTROLLER)
    {
        ixcp->next = index;
        ixcp->prev = index;
        head = index;
    }
    else
    {
        intersection_controller* first = &Intersection_controllers[head];
        ixcp->prev = first->prev;
        ixcp->next = head;
        Intersection_controllers[first->prev].next = index;
        first->prev = index;
    }
}

// The point on a vehicle's centre line at its front bumper.
static vector ixc_front_of_vehicle(const vehicle* vp)
{
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    return vp->pos + vp->orient.fvec * bmax.z;
}

// 0x00BCC0E0 (:129): the road point of the next connection along the vehicle's way, or 0xFF. Original: reads through a
// null road when closest_road_spline is stale (0x00BCC10D).
static unsigned char ixc_get_upcoming_point(const vehicle* vp)
{
    const object_path_road* road = World.object_from_handle<object_path_road>(vp->vai.current_path_info.closest_road_spline);
    const int num_connections = road->spline_data.num_connections;
    if (num_connections == 0)
    {
        return 0xFF;
    }
    const road_spline_connection* connections = road->spline_data.connections.to_pointer();
    const float index = vp->vai.current_path_info.closest_road_index;
    if (vp->vai.current_path_info.flags.next_point_inc)
    {
        const int point = (int)(float)ceil((double)index);
        if (connections[0].from_pt >= point)
        {
            return (unsigned char)connections[0].from_pt;
        }
        for (int i = 1; i < num_connections; i++)
        {
            if (connections[i - 1].from_pt < point && connections[i].from_pt >= point)
            {
                return (unsigned char)connections[i].from_pt;
            }
        }
        return 0xFF;
    }
    // rfg.exe has an inlined floor here (0x00BCC1AD).
    const int point = (int)(float)floor((double)index);
    if (connections[num_connections - 1].from_pt <= point)
    {
        return (unsigned char)connections[num_connections - 1].from_pt;
    }
    for (int i = num_connections - 2; i >= 0; i--)
    {
        if (connections[i + 1].from_pt > point && connections[i].from_pt <= point)
        {
            return (unsigned char)connections[i].from_pt;
        }
    }
    return 0xFF;
}

// 0x00BC8CC0 (:171): LTCG, road1 in EDI. The smaller handle goes first, so a junction has one controller either way.
static intersection_controller* ixc_allocate_controller(unsigned int road1, int point1, unsigned int road2, int point2)
{
    if (Free_intersection_controllers == NO_CONTROLLER)
    {
        return nullptr;
    }
    const object_path_road* road1p = World.object_from_handle<object_path_road>(road1);
    const object_path_road* road2p = World.object_from_handle<object_path_road>(road2);
    if (road1p == nullptr || road2p == nullptr || point1 >= road1p->spline_data.num_points || point2 >= road2p->spline_data.num_points)
    {
        return nullptr;
    }
    intersection_controller* ixcp = &Intersection_controllers[Free_intersection_controllers];
    ixc_list_remove(Free_intersection_controllers, ixcp);
    ixc_list_append(Active_intersection_controllers, ixcp);
    if (road1 < road2)
    {
        ixcp->road1_handle = road1;
        ixcp->road1_point = (unsigned char)point1;
        ixcp->road2_handle = road2;
        ixcp->road2_point = (unsigned char)point2;
    }
    else
    {
        ixcp->road1_handle = road2;
        ixcp->road1_point = (unsigned char)point2;
        ixcp->road2_handle = road1;
        ixcp->road2_point = (unsigned char)point1;
    }
    ixcp->queue_size = 0;
    return ixcp;
}

// 0x00BCC270 (:222): LTCG, road in EAX, vp in ECX. Within 25 m of a connection of that point to a road that is not
// GPS only; two road ends meeting are not a junction.
static bool ixc_check_valid_intersection(const vehicle* vp, unsigned int road, int point, unsigned int& other_road, int& other_point)
{
    const object_path_road* roadp = World.object_from_handle<object_path_road>(road);
    if (roadp == nullptr || roadp->road_flags.gps_only || point >= roadp->spline_data.num_points)
    {
        return false;
    }
    const vector front = ixc_front_of_vehicle(vp);
    if (vec_dist_squared(roadp->spline_data.points.to_pointer()[point].pos, front) > 625.0f)
    {
        return false;
    }
    const road_spline_connection* connections = roadp->spline_data.connections.to_pointer();
    for (int i = 0; i < roadp->spline_data.num_connections; i++)
    {
        if (connections[i].from_pt != (unsigned short)point)
        {
            continue;
        }
        other_road = connections[i].handle;
        other_point = connections[i].to_pt;
        const object_path_road* other = World.object_from_handle<object_path_road>(other_road);
        if (other == nullptr || other->road_flags.gps_only)
        {
            return false;
        }
        if ((point == 0 || point == roadp->spline_data.num_points - 1) && (other_point == 0 || other_point == other->spline_data.num_points - 1))
        {
            return false;
        }
        return true;
    }
    return false;
}

// 0x00BC2EC0 (:275): LTCG, road1 in ECX, road2 in EDX, point1 in ESI, point2 in EDI.
static bool ixc_controller_matches(const intersection_controller* ixcp, unsigned int road1, int point1, unsigned int road2, int point2)
{
    if (road1 < road2)
    {
        return ixcp->road1_handle == road1 && ixcp->road2_handle == road2 && ixcp->road1_point == point1 && ixcp->road2_point == point2;
    }
    return ixcp->road2_handle == road1 && ixcp->road1_handle == road2 && ixcp->road2_point == point1 && ixcp->road1_point == point2;
}

// 0x00BC2F10 (:288): ixcp in ECX.
static void ixc_free_controller(intersection_controller* ixcp)
{
    ixc_list_remove(Active_intersection_controllers, ixcp);
    ixc_list_append(Free_intersection_controllers, ixcp);
}

// 0x00BC8E80 (:310): LTCG, road1 in EAX, road2 in ECX, point2 in ESI.
static intersection_controller* ixc_get_controller(unsigned int road1, int point1, unsigned int road2, int point2)
{
    if (Active_intersection_controllers != NO_CONTROLLER)
    {
        unsigned short index = Active_intersection_controllers;
        do
        {
            intersection_controller* ixcp = &Intersection_controllers[index];
            if (ixc_controller_matches(ixcp, road1, point1, road2, point2))
            {
                return ixcp;
            }
            index = ixcp->next;
        } while (index != Active_intersection_controllers);
    }
    return ixc_allocate_controller(road1, point1, road2, point2);
}

// 0x00BC2FF0 (:328): LTCG, vp in ESI. Only ambient road driving queues. Original: the driver's npc flags and AI are read
// for any human that is not a player (sub type 3), which assumes an NPC.
static bool ixc_vehicle_can_be_queue_controlled(const vehicle* vp)
{
    if (vehicle_is_corpse(vp))
    {
        return false;
    }
    const vehicle_path_info::vehicle_path_flags& flags = vp->vai.current_path_info.flags;
    if (!flags.ixc_can_control || !flags.road_spline_path || flags.off_road)
    {
        return false;
    }
    const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver == nullptr || driver->get_sub_type() == 3)
    {
        return false;
    }
    const npc* np = static_cast<const npc*>(driver);
    if (np->npc_flags.ignore_vehicles)
    {
        return false;
    }
    const ai_action* action = np->get_ai()->planner.get_current_action();
    return action == ambient_veh_drive_action::get_instance() || action == drive_despawn_road_action::get_instance();
}

// 0x00BCC4A0 (:369): LTCG, vp in EAX. The nearest of the vehicle's front, back and centre to road1's point.
static float ixc_smallest_dist_squared_to_vehicle(const intersection_controller* ixcp, const vehicle* vp)
{
    vector bmin;
    vector bmax;
    vp->get_local_bbox(bmin, bmax);
    const vector front = vp->pos + vp->orient.fvec * bmax.z;
    const vector back = vp->pos + vp->orient.fvec * bmin.z;
    const object_path_road* road = World.object_from_handle<object_path_road>(ixcp->road1_handle);
    const vector& point = road->spline_data.points.to_pointer()[ixcp->road1_point].pos;
    const float front_dist = vec_dist_squared(front, point);
    const float back_dist = vec_dist_squared(back, point);
    const float center_dist = vec_dist_squared(vp->pos, point);
    const float smaller = center_dist - front_dist >= 0.0f ? front_dist : center_dist;
    return smaller - back_dist >= 0.0f ? back_dist : smaller;
}

// 0x00BC3080 (:395): LTCG, ixcp in ECX.
static int ixc_get_queue_index(const intersection_controller* ixcp, const vehicle* vp)
{
    for (int i = 0; i < ixcp->queue_size; i++)
    {
        if (ixcp->vehicle_queue[i] == vp->get_handle())
        {
            return i;
        }
    }
    return -1;
}

// 0x00BC8F20 (:413): LTCG, other_veh_handle in EAX, vp in ESI.
static bool ixc_vehicles_might_collide(const vehicle* vp, unsigned int other_veh_handle)
{
    const object* objp = World.object_from_handle(other_veh_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return false;
    }
    const vehicle* other = static_cast<const vehicle*>(objp);
    const vehicle_path_info& path = vp->vai.current_path_info;
    const vehicle_path_info& other_path = other->vai.current_path_info;
    if (path.closest_road_spline != other_path.closest_road_spline)
    {
        // Coming from different roads: two right turns do not cross.
        return !path.flags.ixc_turning_right || !other_path.flags.ixc_turning_right;
    }
    // On the same road only opposite directions meet, and then only with a left turn.
    if (path.flags.next_point_inc == other_path.flags.next_point_inc)
    {
        return false;
    }
    if (path.flags.ixc_turning && !path.flags.ixc_turning_right)
    {
        return true;
    }
    return other_path.flags.ixc_turning && !other_path.flags.ixc_turning_right;
}

// 0x00BCC690 (:452)
static bool ixc_should_still_control_vehicle(const vehicle* vp)
{
    const unsigned int index = vp->vai.active_intersection_controller;
    if (index == NO_CONTROLLER || index >= MAX_INTERSECTION_CONTROLLERS)
    {
        return false;
    }
    const intersection_controller* ixcp = &Intersection_controllers[index];
    const unsigned int road = vp->vai.current_path_info.closest_road_spline;
    const unsigned char point = ixc_get_upcoming_point(vp);
    if (point == 0xFF)
    {
        return false;
    }
    const int queue_index = ixc_get_queue_index(ixcp, vp);
    if (queue_index < 0)
    {
        return false;
    }
    if ((ixcp->road1_handle == road && ixcp->road1_point == point) || (ixcp->road2_handle == road && ixcp->road2_point == point))
    {
        return true;
    }
    // Past the junction: the front of the queue stays until it has cleared by 5 m.
    return queue_index == 0 && ixc_smallest_dist_squared_to_vehicle(ixcp, vp) < 25.0f;
}

// 0x00BC30B0 (:487): for qsort, on the addresses of array entries. Original: the entry at the lower address gets 25
// squared metres off, so the result depends on where qsort has moved them.
static int ixc_vehicle_sort(const void* veha, const void* vehb)
{
    const vehicle* a = *static_cast<vehicle* const*>(veha);
    const vehicle* b = *static_cast<vehicle* const*>(vehb);
    float a_dist = vec_dist_squared(ixc_front_of_vehicle(a), Ixc_center_pos);
    float b_dist = vec_dist_squared(ixc_front_of_vehicle(b), Ixc_center_pos);
    if (veha < vehb)
    {
        a_dist -= 25.0f;
    }
    else
    {
        b_dist -= 25.0f;
    }
    if (a_dist > b_dist)
    {
        return 1;
    }
    return b_dist > a_dist ? -1 : 0;
}

// 0x00BCC750 (:541): LTCG, ixcp in ESI. Drops the handles that are gone, sorts the rest by distance to road1's point.
// Original: the last_vp of the PDB's signature is not read.
static void ixc_resort_queue(intersection_controller* ixcp)
{
    const object_path_road* road = World.object_from_handle<object_path_road>(ixcp->road1_handle);
    Ixc_center_pos = road->spline_data.points.to_pointer()[ixcp->road1_point].pos;
    vehicle* vehicles[MAX_QUEUE];
    int count = 0;
    for (int i = 0; i < ixcp->queue_size; i++)
    {
        object* objp = World.object_from_handle(ixcp->vehicle_queue[i]);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicles[count++] = static_cast<vehicle*>(objp);
        }
    }
    qsort(vehicles, count, sizeof(vehicle*), ixc_vehicle_sort);
    for (int i = 0; i < count; i++)
    {
        ixcp->vehicle_queue[i] = vehicles[i]->get_handle();
    }
    ixcp->queue_size = (unsigned char)count;
}

// 0x00BCC830 (:569): vp in ECX.
static void ixc_remove_vehicle_from_queue(vehicle* vp)
{
    const unsigned int index = vp->vai.active_intersection_controller;
    vp->vai.active_intersection_controller = NO_CONTROLLER;
    if (index == NO_CONTROLLER || index >= MAX_INTERSECTION_CONTROLLERS)
    {
        return;
    }
    intersection_controller* ixcp = &Intersection_controllers[index];
    const int queue_index = ixc_get_queue_index(ixcp, vp);
    if (queue_index < 0)
    {
        return;
    }
    ixcp->queue_size--;
    if (ixcp->queue_size != 0)
    {
        ixcp->vehicle_queue[queue_index] = ixcp->vehicle_queue[ixcp->queue_size];
        ixc_resort_queue(ixcp);
        if (ixcp->queue_size != 0)
        {
            return;
        }
    }
    ixc_free_controller(ixcp);
}

// 0x00BCC8C0 (:606): LTCG, ixcp in EAX.
static bool ixc_add_vehicle_to_queue(intersection_controller* ixcp, const vehicle* vp)
{
    if (ixcp->queue_size >= MAX_QUEUE || ixc_get_queue_index(ixcp, vp) != -1)
    {
        return false;
    }
    ixcp->vehicle_queue[ixcp->queue_size] = vp->get_handle();
    ixcp->queue_size++;
    ixc_resort_queue(ixcp);
    return true;
}

// 0x00BCC920 (:629): LTCG, vp in EDI, point1 in ECX, point2 in EDX.
static void ixc_add_vehicle_to_intersection_controller(vehicle* vp, unsigned int road1, int point1, unsigned int road2, int point2)
{
    if (vp->vai.active_intersection_controller != NO_CONTROLLER)
    {
        return;
    }
    intersection_controller* ixcp = ixc_get_controller(road1, point1, road2, point2);
    if (ixcp != nullptr && ixc_add_vehicle_to_queue(ixcp, vp))
    {
        vp->vai.active_intersection_controller = ixc_index(ixcp);
    }
}

// 0x00BC3360 (:694)
void intersection_controller_system_init()
{
}

// 0x00BC3370 (:708)
void intersection_controller_level_init()
{
    Active_intersection_controllers = NO_CONTROLLER;
    Free_intersection_controllers = NO_CONTROLLER;
    for (int i = 0; i < MAX_INTERSECTION_CONTROLLERS; i++)
    {
        ixc_list_append(Free_intersection_controllers, &Intersection_controllers[i]);
    }
}

// 0x00BCC980 (:722): frees the controllers that emptied. Original: the next controller is taken before a free moves the
// list, and the end of the loop is tested against the head as it is by then.
void intersection_controller_do_frame()
{
    intersection_controller* ixcp = Active_intersection_controllers == NO_CONTROLLER ? nullptr : &Intersection_controllers[Active_intersection_controllers];
    while (ixcp != nullptr)
    {
        intersection_controller* next = ixcp->next == Active_intersection_controllers ? nullptr : &Intersection_controllers[ixcp->next];
        bool free_it = ixcp->queue_size == 0;
        if (!free_it)
        {
            const object* first = World.object_from_handle(ixcp->vehicle_queue[0]);
            if (first == nullptr || first->get_obj_type() != OT_VEHICLE)
            {
                ixc_resort_queue(ixcp);
                free_it = ixcp->queue_size == 0;
            }
        }
        if (free_it)
        {
            ixc_free_controller(ixcp);
        }
        ixcp = next;
    }
}

// 0x00BCCA20 (:746)
void intersection_controller_process_vehicle(vehicle* vp)
{
    const bool can_control = ixc_vehicle_can_be_queue_controlled(vp);
    if (vp->vai.active_intersection_controller != NO_CONTROLLER)
    {
        if (can_control && ixc_should_still_control_vehicle(vp))
        {
            return;
        }
        ixc_remove_vehicle_from_queue(vp);
    }
    if (!can_control)
    {
        return;
    }
    const unsigned int road = vp->vai.current_path_info.closest_road_spline;
    const unsigned char point = ixc_get_upcoming_point(vp);
    if (point == 0xFF)
    {
        return;
    }
    unsigned int other_road;
    int other_point;
    if (ixc_check_valid_intersection(vp, road, point, other_road, other_point))
    {
        ixc_add_vehicle_to_intersection_controller(vp, road, point, other_road, other_point);
    }
}

// 0x00BCCAB0 (:782)
void intersection_controller_handle_vehicle_destroyed(vehicle* vp)
{
    if (vp->vai.active_intersection_controller != NO_CONTROLLER)
    {
        ixc_remove_vehicle_from_queue(vp);
    }
    vp->vai.active_intersection_controller = NO_CONTROLLER;
}

// 0x00BCCAF0 (:798): brakes for the junction when a vehicle ahead in the queue might cross this one's path.
void intersection_controller_update_vehicle_accel_decel(const vehicle* vp, float& overall_speed, float& overall_decel)
{
    const unsigned int index = vp->vai.active_intersection_controller;
    if (index == NO_CONTROLLER || index >= MAX_INTERSECTION_CONTROLLERS)
    {
        return;
    }
    const intersection_controller* ixcp = &Intersection_controllers[index];
    const int queue_index = ixc_get_queue_index(ixcp, vp);
    bool might_collide = false;
    for (int i = 0; i < queue_index && !might_collide; i++)
    {
        might_collide = ixc_vehicles_might_collide(vp, ixcp->vehicle_queue[i]);
    }
    if (!might_collide)
    {
        return;
    }
    // Stop short of the other road's point, by its width and 8 m.
    const bool own_is_road1 = vp->vai.current_path_info.closest_road_spline == ixcp->road1_handle;
    const object_path_road* road = World.object_from_handle<object_path_road>(own_is_road1 ? ixcp->road2_handle : ixcp->road1_handle);
    const road_spline_point& point = road->spline_data.points.to_pointer()[own_is_road1 ? ixcp->road2_point : ixcp->road1_point];
    float dist = vec_dist(ixc_front_of_vehicle(vp), point.pos) - (point.width + 8.0f);
    if (dist <= 0.0f)
    {
        dist = 0.0f;
    }
    float need_decel = vp->vai.active_control_info.max_braking_decel;
    const float comfortable = (need_decel - 10.0f >= 0.0f ? 10.0f : need_decel) * 0.75f;
    const float forward_velocity = vp->forward_velocity;
    float target_speed = 0.0f;
    if (!((dist < 0.5f && forward_velocity < 2.0f) || dist < 0.01f))
    {
        target_speed = sqrtf(comfortable * 2.0f * dist);
        need_decel = forward_velocity * forward_velocity / (dist * 2.0f);
    }
    if (need_decel > comfortable)
    {
        overall_decel = need_decel - overall_decel >= 0.0f ? need_decel : overall_decel;
        overall_speed = target_speed - overall_speed >= 0.0f ? overall_speed : target_speed;
    }
}

// 0x00BC36C0 (:893)
bool intersection_controller_vehicle_at_top_of_queue(const vehicle* vp)
{
    const unsigned int index = vp->vai.active_intersection_controller;
    if (index == NO_CONTROLLER || index >= MAX_INTERSECTION_CONTROLLERS)
    {
        return false;
    }
    return ixc_get_queue_index(&Intersection_controllers[index], vp) == 0;
}
