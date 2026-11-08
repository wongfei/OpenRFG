// Original: volition/rfg/code/spawn/spawn_placement.cpp
//
// Where a requested NPC or vehicle goes and creating it, and moving a vehicle that streams in on top of something
// (NPC.md NP5 finished the file). Plain float where rfg.exe uses double chains.

#include "volition/rfg/code/spawn/spawn_placement.h"

#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/gameplay/activities/activity_car_bomb.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_request.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/varray.h"

#include <float.h>
#include <math.h>

// Magic number: an air vehicle spawns at least 35 m over the ground.
static const float AIR_VEHICLE_MIN_HEIGHT = 35.0f;
// Magic numbers: rfg.exe's stack lists.
static const int MAX_OBSTRUCTIONS = 256;
static const int MAX_LOCAL_VEHICLES = 40;

// 0x00B53AA0 <object_mover> (:652)
// 0x00B53BE0 <object_debris> (:652)
// 0x00B53D20 <item> (:652)
// What of type T stands in the vehicle's box at the place; an invalid place or box gives nothing.
template<typename T>
static void collect_vehicle_spawn_obstructions(const vehicle_info* v_info, const vector& center_pos, const matrix& orient, farray<object*, MAX_OBSTRUCTIONS>& obstruction_list)
{
    obstruction_list.clear();
    const bbox& box = v_info->spawn_box;
    if (!center_pos.is_valid() || !orient.is_valid() || !orient.is_orthonormal() || !box.min.is_valid() || !box.max.is_valid() || !(box.min.x < box.max.x && box.min.y < box.max.y && box.min.z < box.max.z))
    {
        return;
    }
    unsigned int handles[MAX_OBSTRUCTIONS];
    const int count = World.collect_objects<T>(box.min, box.max, center_pos, orient, handles, MAX_OBSTRUCTIONS, nullptr, nullptr);
    for (int i = 0; i < count; i++)
    {
        T* objp = World.object_from_handle<T>(handles[i]);
        if (objp != nullptr && !objp->obj_flags.destroyed)
        {
            obstruction_list.add(objp);
        }
    }
}

// 0x00B482B0 (:687)
static void sp_destroy_object(object* objp)
{
    if (objp->get_obj_type() == OT_VEHICLE)
    {
        vehicle_destroy_with_passengers(static_cast<vehicle*>(objp));
    }
    else
    {
        World.destroy_object(objp, false);
    }
}

// 0x00B430A0 (:757)
static bool can_delete_other_vehicle(const vehicle* vp)
{
    return !vp->m_flags.convoy_vehicle;
}

// 0x00B482D0 (:770)
// LTCG: objp in EDX. Loose objects always; movers without a zone; vehicles only for a high priority spawn and if not in a
// convoy; destroyed objects always.
static bool can_delete_object_for_spawn(const object* objp, bool high_priority)
{
    if (objp->obj_flags.destroyed)
    {
        return true;
    }
    switch (objp->get_obj_type())
    {
    case OT_ITEM:
    case OT_DEBRIS:
        return true;
    case OT_MOVER:
        return ((objp->get_handle() >> 16) & 0xFF) == 0xFF;
    case OT_VEHICLE:
        return high_priority && can_delete_other_vehicle((const vehicle*)objp);
    default:
        return false;
    }
}

// 0x00B430B0 (:833): LTCG, vp in EAX.
static float get_vehicle_size(const vehicle* vp)
{
    // Magic number: 5 m for a vehicle with no info.
    return vp != nullptr && vp->info != nullptr ? vp->info->spawn_size : 5.0f;
}

// 0x00B4FEC0 (:843)
static bool collect_local_vehicles(const vector& center_pos, float radius, farray<vehicle*, MAX_LOCAL_VEHICLES>& local_vehicle_list)
{
    local_vehicle_list.clear();
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        const float size = get_vehicle_size(vp);
        if (vec_dist_squared(center_pos, vp->pos) <= (size + radius) * (size + radius))
        {
            local_vehicle_list.add(vp);
        }
    }
    return local_vehicle_list.size() > 0;
}

// 0x00B55D50 (:868)
static bool collect_vehicle_spawn_obstructions(const vehicle_info* v_info, const vector& center_pos, const matrix& orient, farray<object*, MAX_OBSTRUCTIONS>& obstruction_list)
{
    farray<object*, MAX_OBSTRUCTIONS> found;
    obstruction_list.clear();
    collect_vehicle_spawn_obstructions<object_mover>(v_info, center_pos, orient, found);
    for (int i = 0; i < found.size(); i++)
    {
        obstruction_list.add(found[i]);
    }
    collect_vehicle_spawn_obstructions<object_debris>(v_info, center_pos, orient, found);
    for (int i = 0; i < found.size(); i++)
    {
        obstruction_list.add(found[i]);
    }
    farray<vehicle*, MAX_LOCAL_VEHICLES> vehicles;
    if (collect_local_vehicles(center_pos, v_info->spawn_size, vehicles))
    {
        for (int i = 0; i < vehicles.size(); i++)
        {
            obstruction_list.add(vehicles[i]);
        }
    }
    // Items count unless somebody holds them, a player carries them or they are a turret's.
    collect_vehicle_spawn_obstructions<item>(v_info, center_pos, orient, found);
    for (int i = 0; i < found.size(); i++)
    {
        item* ip = (item*)found[i];
        if (ip == nullptr || ip->get_obj_type() != OT_ITEM)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        if (ip->get_owner_handle() != 0xFFFFFFFF)
        {
            object* owner = World.object_from_handle(ip->get_owner_handle());
            if (owner != nullptr && owner->get_obj_type() == OT_HUMAN)
            {
                continue;
            }
        }
        if (ip->get_parent() != 0xFFFFFFFF)
        {
            object* parent = World.object_from_handle(ip->get_parent());
            if (parent != nullptr && parent->get_obj_type() == OT_HUMAN && parent->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                continue;
            }
        }
        if (turret_from_item(ip) == nullptr)
        {
            obstruction_list.add(found[i]);
        }
    }
    return obstruction_list.size() > 0;
}

// 0x00B50180 (:1269)
// LTCG: obstructing_vehicles in EDI.
static bool vehicle_near_spawn_point(const vector& spawn_pos, float radius, farray<vehicle*, MAX_LOCAL_VEHICLES>& obstructing_vehicles)
{
    obstructing_vehicles.clear();
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        const float range = vp->info->spawn_size + radius;
        if (vec_dist_squared(spawn_pos, vp->pos) < range * range)
        {
            obstructing_vehicles.add(vp);
        }
    }
    return obstructing_vehicles.size() > 0;
}

// 0x00B56000 (:1299)
static bool can_drop_vehicle(const vehicle_info* v_info, farray<object*, MAX_OBSTRUCTIONS>& delete_object_list, const vector& position, const matrix& orient, bool high_priority)
{
    farray<object*, MAX_OBSTRUCTIONS> obstructions;
    // Original: the list of near vehicles is not used.
    farray<vehicle*, MAX_LOCAL_VEHICLES> near_vehicles;
    vehicle_near_spawn_point(position, v_info->spawn_size, near_vehicles);
    if (!collect_vehicle_spawn_obstructions(v_info, position, orient, obstructions))
    {
        return true;
    }
    delete_object_list.clear();
    bool blocked = false;
    for (int i = 0; i < obstructions.size(); i++)
    {
        if (can_delete_object_for_spawn(obstructions[i], high_priority))
        {
            delete_object_list.add(obstructions[i]);
        }
        else
        {
            blocked = true;
        }
    }
    return !blocked;
}

// 0x00B431E0 (:2840): LTCG, spawn_param in EAX.
static bool should_snap_vehicle_to_ground(const vehicle_spawn_params& spawn_param)
{
    const auto& flags = spawn_param.veh_spawn_flags;
    if (flags.static_spawn_placement)
    {
        return false;
    }
    return flags.force_to_ground || !flags.air_vehicle;
}

// 0x00B43200 (:2850): LTCG, spawn_param in ESI.
static void set_min_vehicle_height(vehicle_spawn_params& spawn_param)
{
    vector ground = spawn_param.spawn_pos;
    rfg_snap_to_ground(ground);
    if (spawn_param.spawn_pos.y - ground.y < AIR_VEHICLE_MIN_HEIGHT)
    {
        spawn_param.spawn_pos.y = ground.y + AIR_VEHICLE_MIN_HEIGHT;
    }
}

// 0x00B43270 (:2867)
// LTCG: original_pos in EAX, ground_pos in ECX. The ground under the vehicle's centre and front, sloped along them.
static bool find_ground_snap_pos(const vector& original_pos, const matrix& original_orient, const vehicle_info* veh_info, vector& ground_pos, matrix& ground_orient)
{
    ground_pos = original_pos;
    if (!havok_terrain_height(ground_pos.y, ground_pos.x, ground_pos.z, false, nullptr))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
    vector front;
    front.x = ground_pos.x + veh_info->spawn_box.max.z * original_orient.fvec.x;
    front.y = ground_pos.y + original_orient.fvec.y * veh_info->spawn_box.max.z;
    front.z = ground_pos.z + original_orient.fvec.z * veh_info->spawn_box.max.z;
    if (!front.is_valid() || front.is_null() || !havok_terrain_height(front.y, front.x, front.z, false, nullptr))
    {
        return false;
    }
    vector direction;
    direction.x = front.x - ground_pos.x;
    direction.y = front.y - ground_pos.y;
    direction.z = front.z - ground_pos.z;
    if (!direction.is_valid() || direction.is_null())
    {
        return false;
    }
    ground_orient.make(direction);
    if (!ground_orient.is_valid())
    {
        return false;
    }
    ground_pos.y = -veh_info->spawn_box.min.y + ground_pos.y;
    return true;
}

// 0x00B434C0 (:2908)
static bool snap_vehicle_to_ground(vehicle_spawn_params& spawn_param)
{
    vector ground_pos;
    matrix ground_orient;
    if (!find_ground_snap_pos(spawn_param.spawn_pos, spawn_param.spawn_orient, spawn_param.veh_info, ground_pos, ground_orient))
    {
        return false;
    }
    spawn_param.spawn_pos = ground_pos;
    spawn_param.spawn_orient = ground_orient;
    return true;
}

// 0x00B43540 (:2925)
static void create_vehicle_stream_flags(vehicle_spawn_flags& veh_flags, const vehicle_spawn_params& spawn_param)
{
    const auto& flags = spawn_param.veh_spawn_flags;
    veh_flags = vehicle_spawn_flags();
    veh_flags.high_priority = flags.high_priority_clean_up || spawn_param.vflags.convoy_vehicle;
    veh_flags.mobile_spawn = flags.mobile_vehicle_spawn;
    veh_flags.static_placement = flags.interior_spawn || flags.static_spawn_placement;
    veh_flags.snap_to_ground = !flags.static_spawn_placement && (flags.force_to_ground || !flags.air_vehicle);
    veh_flags.road_spawn = flags.road_spawn;
}

// 0x00B48960 (:2939)
static bool create_new_vehicle(vehicle_spawn_params& spawn_param)
{
    const int type = vehicle_info_get_type(spawn_param.veh_info);
    obj_prop_block pblock(spawn_param.spawn_pos, spawn_param.spawn_orient, nullptr);
    pblock.set<int>(VEHICLE_PROP_TYPE, type);
    pblock.set<bool>(VEHICLE_PROP_AT_GROUND, true);
    pblock.set<float>(VEHICLE_INITIAL_SPEED, spawn_param.forward_speed);
    vehicle_spawn_flags stream_flags;
    create_vehicle_stream_flags(stream_flags, spawn_param);
    pblock.set_buf(VEHICLE_MOVE_AT_STREAM_FLAGS, (const unsigned char*)&stream_flags, 4);
    pblock.set_buf(VEHICLE_PROP_FLAGS, (const unsigned char*)&spawn_param.vflags, 8);
    switch (spawn_param.veh_info->vehicle_class)
    {
    case VCL_AUTOMOBILE:
        spawn_param.vp = (vehicle*)World.create_object<automobile>(pblock, spawn_param.parent_handle);
        break;
    case VCL_FLYER:
        spawn_param.vp = (vehicle*)World.create_object<flyer>(pblock, spawn_param.parent_handle);
        break;
    case VCL_WALKER:
        spawn_param.vp = (vehicle*)World.create_object<walker>(pblock, spawn_param.parent_handle);
        break;
    default:
        break;
    }
    if (spawn_param.vp != nullptr)
    {
        spawn_param.vp->obj_flags.no_save = spawn_param.veh_spawn_flags.no_save;
        if (spawn_param.veh_spawn_flags.has_car_bomb)
        {
            vehicle_set_bomb_status(spawn_param.vp, VEHICLE_BOMB_ARMED);
            activity_car_bomb_start(spawn_param.vp);
        }
    }
    return spawn_param.vp != nullptr;
}

// 0x00B48AD0 (:2986)
static bool do_vehicle_spawn_mobile(vehicle_spawn_params& spawn_param)
{
    if (should_snap_vehicle_to_ground(spawn_param) && !snap_vehicle_to_ground(spawn_param))
    {
        return false;
    }
    return create_new_vehicle(spawn_param);
}

// Magic numbers: a road spawn stays 15 to 25 m from where it was asked for along the road.
static const float ROAD_SPAWN_MIN_DIST_SQRD = 225.0f; // 0x012C9648 as a double
static const float MAX_SPAWN_DISTANCE_SQRD = 625.0f; // 0x01336164

// 0x00B50840 (:3001): LTCG, spawn_orient in EDI, prp in ESI, dist_on_road in XMM0.
static bool is_facing_up_road(const matrix& spawn_orient, const object_path_road* prp, const float dist_on_road)
{
    int index = fl_round(dist_on_road);
    const int last = prp->spline_data.num_points - 1;
    if (index < 0)
    {
        index = 0;
    }
    else if (last < index)
    {
        index = last;
    }
    const road_spline_point* points = prp->spline_data.points.to_pointer();
    vector down_road = index < 1 ? points[index].pos - points[1].pos : points[index - 1].pos - points[index].pos;
    down_road.mag_and_normalize_safe();
    return spawn_orient.fvec.y * down_road.y + spawn_orient.fvec.x * down_road.x + spawn_orient.fvec.z * down_road.z < 0.0f;
}

// 0x00B435E0 (:3034): bisects for a point of the road within range of the original, at most 16 steps.
static float find_max_road_spawn_distance(const vector& original_road_pos, const object_path_road* prp, const float min_dist_on_road, const float max_dist_on_road, int count)
{
    if (fabsf(min_dist_on_road - max_dist_on_road) < 0.01f)
    {
        return min_dist_on_road;
    }
    const float middle = (min_dist_on_road + max_dist_on_road) * 0.5f;
    if (count >= 16)
    {
        return middle;
    }
    vector point;
    prp->get_point(point, middle);
    const float dist = vec_dist_squared(original_road_pos, point);
    if (dist > ROAD_SPAWN_MIN_DIST_SQRD && dist < MAX_SPAWN_DISTANCE_SQRD)
    {
        return middle;
    }
    if (dist > MAX_SPAWN_DISTANCE_SQRD)
    {
        return find_max_road_spawn_distance(original_road_pos, prp, min_dist_on_road, middle, count + 1);
    }
    return find_max_road_spawn_distance(original_road_pos, prp, middle, max_dist_on_road, count + 1);
}

// 0x00B50A20 (:3076): LTCG, spawn_orient in EDI, prp in EAX. Looks towards the end of the road the vehicle faces away
// from, as is_facing_up_road means the road's points run against the vehicle. Inlines the first step of the search.
static float pick_road_distance(const matrix& spawn_orient, const vector& original_road_pos, const object_path_road* prp, const float dist_on_road)
{
    const float far_end = is_facing_up_road(spawn_orient, prp, dist_on_road) ? 0.0f : (float)(prp->spline_data.num_points - 1);
    if (fabsf(dist_on_road - far_end) < 0.01f)
    {
        return dist_on_road;
    }
    const float middle = (dist_on_road + far_end) * 0.5f;
    vector point;
    prp->get_point(point, middle);
    const float dist = vec_dist_squared(original_road_pos, point);
    if (dist > ROAD_SPAWN_MIN_DIST_SQRD && dist < MAX_SPAWN_DISTANCE_SQRD)
    {
        return middle;
    }
    if (dist > MAX_SPAWN_DISTANCE_SQRD)
    {
        return find_max_road_spawn_distance(original_road_pos, prp, dist_on_road, middle, 1);
    }
    return find_max_road_spawn_distance(original_road_pos, prp, middle, far_end, 1);
}

// 0x00B50C00 (:3093): LTCG, spawn_orient in EDI, original_road_pos in ECX, prp in EAX. Anywhere between the given place
// and the far one; 0 when the given distance is off the road. Draws from the random stream.
static float randomize_road_spawn_placement(const matrix& spawn_orient, const vector& original_road_pos, const object_path_road* prp, const float dist_on_road)
{
    const float num_points = (float)prp->spline_data.num_points;
    if (dist_on_road < 0.0f || num_points <= dist_on_road)
    {
        return 0.0f;
    }
    const float picked = pick_road_distance(spawn_orient, original_road_pos, prp, dist_on_road);
    if (picked == dist_on_road)
    {
        return dist_on_road;
    }
    const float result = dist_on_road < picked ? fl_rand_range(dist_on_road, picked) : fl_rand_range(picked, dist_on_road);
    return result >= 0.0f && result < num_points ? result : 0.0f;
}

// 0x00B50CD0 (:3131): LTCG, spawn_param in ESI. Half the road's width to the right of the centre line, facing with the
// traffic. With no road within 50 m it is a plain mobile spawn.
static bool do_vehicle_spawn_mobile_road(vehicle_spawn_params& spawn_param)
{
    vector road_pos;
    float dist_on_road;
    object_path_road* road = object_road_get_closest_road_zone(spawn_param.spawn_pos, false, road_pos, dist_on_road, spawn_param.zone_id_xy, false, false);
    if (road == nullptr || vec_dist_squared(spawn_param.spawn_pos, road_pos) > 2500.0f)
    {
        return do_vehicle_spawn_mobile(spawn_param);
    }
    dist_on_road = randomize_road_spawn_placement(spawn_param.spawn_orient, road_pos, road, dist_on_road);
    road->get_point(road_pos, dist_on_road);
    vector tangent;
    road->get_tangent(tangent, dist_on_road);
    tangent.y = 0.0f;
    if (!tangent.is_valid())
    {
        return false;
    }
    tangent.normalize_safe();
    const matrix& spawn_orient = spawn_param.spawn_orient;
    if (road->road_flags.one_way || spawn_orient.fvec.y * tangent.y + spawn_orient.fvec.x * tangent.x + spawn_orient.fvec.z * tangent.z > 0.0f)
    {
        spawn_param.spawn_orient.make_quick(tangent);
    }
    else
    {
        spawn_param.spawn_orient.make_quick(-tangent);
    }
    const float half_width = road->get_width(dist_on_road) * 0.5f;
    const vector asked_pos = spawn_param.spawn_pos;
    spawn_param.spawn_pos = road_pos + spawn_orient.rvec * half_width;
    if (spawn_param.veh_spawn_flags.air_vehicle)
    {
        // Keep the height over the ground it was asked for, at least 15 m, and clear the terrain under the box by 10.
        float ground = 0.0f;
        havok_terrain_height(ground, asked_pos.x, asked_pos.z, false, nullptr);
        spawn_param.spawn_pos.y += asked_pos.y - ground;
        if (spawn_param.spawn_pos.y - ground < 15.0f)
        {
            spawn_param.spawn_pos.y += 15.0f;
        }
        float box_ground = 0.0f;
        const vector box_min(-10.0f, -10.0f, -10.0f);
        const vector box_max(10.0f, 10.0f, 10.0f);
        if (havok_terrain_height(box_ground, box_min, box_max, spawn_param.spawn_pos, spawn_orient, false) && box_ground > spawn_param.spawn_pos.y + 10.0f)
        {
            spawn_param.spawn_pos.y = box_ground + 10.0f;
        }
    }
    return do_vehicle_spawn_mobile(spawn_param);
}

// 0x00B562C0 (:3215)
static bool do_vehicle_spawn_position(vehicle_spawn_params& spawn_param)
{
    const auto& flags = spawn_param.veh_spawn_flags;
    farray<object*, MAX_OBSTRUCTIONS> delete_list;
    const bool in_air = flags.static_spawn_placement ? flags.air_vehicle : (!flags.force_to_ground && flags.air_vehicle);
    if (in_air)
    {
        set_min_vehicle_height(spawn_param);
    }
    else if (!flags.static_spawn_placement && !flags.interior_spawn)
    {
        if (!snap_vehicle_to_ground(spawn_param))
        {
            return false;
        }
    }
    else
    {
        // A placed vehicle stands with its box on the given height.
        spawn_param.spawn_pos.y = -spawn_param.veh_info->spawn_box.min.y + spawn_param.spawn_pos.y;
    }
    if (!flags.static_spawn_placement && !can_drop_vehicle(spawn_param.veh_info, delete_list, spawn_param.spawn_pos, spawn_param.spawn_orient, flags.high_priority_clean_up) && !flags.convoy_vehicle_spawn)
    {
        return false;
    }
    const bool created = create_new_vehicle(spawn_param);
    if (created)
    {
        for (int i = 0; i < delete_list.size(); i++)
        {
            sp_destroy_object(delete_list[i]);
        }
    }
    return created;
}

// 0x00B56420 (:3454)
bool sp_spawn_vehicle(vehicle_spawn_params& spawn_param)
{
    if (spawn_param.vp != nullptr || spawn_param.veh_info == nullptr || (spawn_param.spawn_pos.is_null() && spawn_param.osrp == nullptr))
    {
        return false;
    }
    spawn_param.zone_id_xy = rfg_zonex_get_grid_xy(spawn_param.spawn_pos);
    const auto& flags = spawn_param.veh_spawn_flags;
    if (!flags.convoy_vehicle_spawn && flags.mobile_vehicle_spawn)
    {
        return flags.road_spawn ? do_vehicle_spawn_mobile_road(spawn_param) : do_vehicle_spawn_mobile(spawn_param);
    }
    return do_vehicle_spawn_position(spawn_param);
}

// ---- moving a vehicle that streamed in ----

extern nav_heap Nav_heap; // nav_heap.cpp

static const int MAX_SEARCH_ITERATIONS_PLACEMENT = 8; // 0x016B98B0
static const float BASE_MOVE_DISTANCE = 10.0f; // 0x016B98AC
static const int MAX_ROAD_ATTEMPTS = 4; // 0x016B98E8
static const float NEAR_NAV_CELL_DIST_SQRD = 400.0f; // set on first use in rfg.exe (_S16)

// 0x00B43700 (:3529): LTCG, pos in ESI. The last cell the vehicle visited if it is a vehicle cell within 20 m, or the one
// under the position, or the nearest within 20 m.
static nav_cell* find_near_nav_cell(const vector& pos, uint64_t last_cell_handle)
{
    nav_cell* last_cell = nav_cell_from_handle(last_cell_handle);
    if (last_cell != nullptr)
    {
        if ((last_cell->flags & NAV_CELL_FLAG_VEHICLE) == 0)
        {
            // Original: this one case returns without the search below.
            return bb_pf_get_nav_cell(pos, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        }
        vector centroid;
        nav_cell_get_centroid(&centroid, last_cell);
        if (vec_dist_squared(pos, centroid) <= NEAR_NAV_CELL_DIST_SQRD)
        {
            return last_cell;
        }
    }
    nav_cell* cell = bb_pf_get_nav_cell(pos, true, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell != nullptr)
    {
        return cell;
    }
    vector adjusted_pos;
    nav_cell* found = nullptr;
    return rfg_pf_find_closest_point_on_nav_mesh_using_collect(pos, true, adjusted_pos, found, 20.0f) ? found : nullptr;
}

// 0x00B51090 (:3583)
static bool stream_area_clear_flier(const vector& stream_pos, const vehicle* vp)
{
    const float size = vp->info->spawn_size;
    const int count = World.get_num_objects<flyer>();
    for (int i = 0; i < count; i++)
    {
        const flyer* other = World.get_object<flyer>(i);
        if (static_cast<const vehicle*>(other) == vp)
        {
            continue;
        }
        const float range = other->info->spawn_size + size;
        if (vec_dist_squared(stream_pos, other->pos) < range * range)
        {
            return false;
        }
    }
    return true;
}

// 0x00B48B00 (:3608): LTCG, obstruction_list in EDI.
static void destroy_obstruction_list(farray<object*, MAX_OBSTRUCTIONS>& obstruction_list)
{
    for (int i = 0; i < obstruction_list.size(); i++)
    {
        sp_destroy_object(obstruction_list[i]);
    }
}

// 0x00B56490 (:3620): LTCG, stream_pos in EAX, obstruction_list in EDI. What can go is listed, anything else blocks.
// rfg.exe has can_delete_object_for_spawn inlined here with no high priority. Original: not streamed counts as blocked,
// while the list is left empty.
static bool stream_area_clear(const vector& stream_pos, const vehicle* vp, farray<object*, MAX_OBSTRUCTIONS>& obstruction_list)
{
    obstruction_list.clear();
    if (!World.is_zone_streamed(stream_pos))
    {
        return false;
    }
    farray<object*, MAX_OBSTRUCTIONS> found;
    if (!collect_vehicle_spawn_obstructions(vp->info, stream_pos, vp->orient, found))
    {
        return true;
    }
    bool blocked = false;
    for (int i = 0; i < found.size(); i++)
    {
        object* objp = found[i];
        if (objp == vp)
        {
            continue;
        }
        if (can_delete_object_for_spawn(objp, false))
        {
            obstruction_list.add(objp);
        }
        else
        {
            blocked = true;
        }
    }
    return !blocked;
}

// 0x00B56610 (:3662): the vehicle's own handle is passed to be spared; anything else that may go goes.
static void do_general_clean_up(const vehicle_info* v_info, const vector& center_pos, const matrix& orient, bool high_priority, unsigned int vehicle_handle)
{
    farray<object*, MAX_OBSTRUCTIONS> found;
    if (!collect_vehicle_spawn_obstructions(v_info, center_pos, orient, found))
    {
        return;
    }
    farray<object*, MAX_OBSTRUCTIONS> delete_list;
    for (int i = 0; i < found.size(); i++)
    {
        if (found[i]->get_handle() != vehicle_handle && can_delete_object_for_spawn(found[i], high_priority))
        {
            delete_list.add(found[i]);
        }
    }
    destroy_obstruction_list(delete_list);
}

// 0x00B567A0 (:3695): LTCG, vp in EAX.
static void do_general_clean_up(const vector& center_pos, const vehicle* vp)
{
    do_general_clean_up(vp->info, center_pos, vp->orient, vp->stream_placement_flags.high_priority, vp->get_handle());
}

// 0x00B43810 (:3701): LTCG, new_pos in EAX, new_orient in ECX, vp in ESI.
static void do_vehicle_move(const vector& new_pos, const matrix& new_orient, vehicle* vp, bool snap_to_ground)
{
    vector pos = new_pos;
    matrix orient = new_orient;
    if (snap_to_ground && !find_ground_snap_pos(new_pos, new_orient, vp->info, pos, orient))
    {
        return;
    }
    vp->update_pos_and_orient(pos, orient, true);
}

// 0x00B567D0 (:3725): breadth first over the nav mesh from the vehicle, into a cell only while it is closer than the best
// clear cell so far, starting from 40 m.
static void move_streaming_vehicle_nav_cell(vehicle* vp)
{
    nav_cell* start = find_near_nav_cell(vp->pos, vp->m_navcell_info.last_visited_nav_cell_handle);
    if (start == nullptr)
    {
        return;
    }
    farray<nav_cell*, MAX_OBSTRUCTIONS> cells;
    farray<object*, MAX_OBSTRUCTIONS> obstructions;
    farray<object*, MAX_OBSTRUCTIONS> best_obstructions;
    const nav_cell* best_cell = nullptr;
    float best_dist_sqrd = 1600.0f;
    vector best_pos;
    best_pos.zero();
    const unsigned int session = Nav_heap.start_session();
    start->heap_session_id = session;
    cells.add(start);
    for (int i = 0; i < cells.size(); i++)
    {
        nav_cell* cell = cells[i];
        if ((cell->flags & NAV_CELL_FLAG_DISABLED) != 0)
        {
            continue;
        }
        vector centroid;
        nav_cell_get_centroid(&centroid, cell);
        const float dist_sqrd = vec_dist_squared(centroid, vp->pos);
        if (dist_sqrd >= best_dist_sqrd)
        {
            continue;
        }
        if (stream_area_clear(centroid, vp, obstructions))
        {
            best_cell = cell;
            best_dist_sqrd = dist_sqrd;
            best_pos = centroid;
            best_obstructions.clear();
            for (int j = 0; j < obstructions.size(); j++)
            {
                best_obstructions.add(obstructions[j]);
            }
        }
        for (int j = 0; j < 3 && cells.size() < cells.capacity(); j++)
        {
            nav_cell* adjacent = cell->adjacent_cells[j].to_pointer();
            if (adjacent != nullptr && adjacent->heap_session_id != session)
            {
                adjacent->heap_session_id = session;
                cells.add(adjacent);
            }
        }
    }
    Nav_heap.end_session();
    if (best_cell != nullptr)
    {
        destroy_obstruction_list(best_obstructions);
        do_general_clean_up(best_pos, vp);
        do_vehicle_move(best_pos, vp->orient, vp, true);
    }
}

// 0x00B51160 (:3825): LTCG, vp in ESI. Rings further out in the world's four directions, then 10 m straight up.
static void move_streaming_flier(vehicle* vp)
{
    for (int i = 1; i <= MAX_SEARCH_ITERATIONS_PLACEMENT; i++)
    {
        const float dist = (float)i * BASE_MOVE_DISTANCE + vp->info->spawn_size * 2.0f;
        const vector tries[4] = {
            vp->pos + FORWARD_VECTOR * dist,
            vp->pos + RIGHT_VECTOR * dist,
            vp->pos - FORWARD_VECTOR * dist,
            vp->pos - RIGHT_VECTOR * dist,
        };
        for (int j = 0; j < 4; j++)
        {
            if (stream_area_clear_flier(tries[j], vp))
            {
                const matrix orient = vp->orient;
                vp->update_pos_and_orient(tries[j], orient, true);
                return;
            }
        }
    }
    const vector above = vp->pos + UP_VECTOR * BASE_MOVE_DISTANCE;
    const matrix orient = vp->orient;
    vp->update_pos_and_orient(above, orient, true);
}

// 0x00B438B0 (:3870): LTCG, search_info in ESI. final_orient is worked out here, and read by nobody.
static bool find_stream_position_on_road(road_search_info_placement& search_info)
{
    const object_path_road* road = search_info.prp;
    vector point;
    vector tangent;
    road->get_point(point, search_info.current_road_dist);
    road->get_tangent(tangent, search_info.current_road_dist);
    tangent.y = 0.0f;
    if (tangent.is_null())
    {
        return false;
    }
    const float half_width = road->get_width(search_info.current_road_dist) * 0.5f;
    tangent.normalize();
    const vector& fvec = search_info.original_orient.fvec;
    if (road->road_flags.one_way || fvec.y * tangent.y + fvec.x * tangent.x + fvec.z * tangent.z > 0.0f)
    {
        search_info.final_orient.make_quick(tangent);
    }
    else
    {
        search_info.final_orient.make_quick(-tangent);
    }
    search_info.final_pos = point;
    if (!road->road_flags.one_way)
    {
        search_info.final_pos += search_info.final_orient.rvec * half_width;
    }
    return true;
}

// 0x00B43A80 (:3910): LTCG, search_info in EDI. Bisects along the road, at most 64 steps.
static bool find_new_road_stream_pos(road_search_info_placement& search_info)
{
    if (search_info.max_desired_dist_sqrd <= search_info.min_desired_dist_sqrd)
    {
        return false;
    }
    for (int i = 0; i < 64; i++)
    {
        const float span = search_info.max_road_dist - search_info.min_road_dist;
        if (fabsf(span) < 0.05f)
        {
            return false;
        }
        search_info.current_road_dist = span * 0.5f + search_info.min_road_dist;
        if (!find_stream_position_on_road(search_info))
        {
            return false;
        }
        const float dist = vec_dist_squared(search_info.original_pos, search_info.final_pos);
        if (dist <= search_info.min_desired_dist_sqrd)
        {
            search_info.min_road_dist = search_info.current_road_dist;
        }
        else
        {
            if (dist < search_info.max_desired_dist_sqrd)
            {
                return true;
            }
            search_info.max_road_dist = search_info.current_road_dist;
        }
    }
    return false;
}

// One side of move_streaming_vehicle_road's search. rfg.exe has it twice, the first time with amb_spawn_in_no_spawn
// inlined. Original: the move keeps the vehicle's own orientation (0x00B56D9F reads original_orient).
static bool try_road_stream_pos(road_search_info_placement& search_info, vehicle* vp)
{
    if (!find_new_road_stream_pos(search_info) || amb_spawn_in_no_spawn(search_info.prp->get_handle(), search_info.current_road_dist))
    {
        return false;
    }
    farray<object*, MAX_OBSTRUCTIONS> obstructions;
    if (!stream_area_clear(search_info.final_pos, vp, obstructions))
    {
        return false;
    }
    destroy_obstruction_list(obstructions);
    do_general_clean_up(search_info.final_pos, vp);
    do_vehicle_move(search_info.final_pos, search_info.original_orient, vp, true);
    return true;
}

// 0x00B56B00 (:3960): LTCG, vp in EAX. Along the road within 30 m, up to 32 m each way in widening bands.
static bool move_streaming_vehicle_road(vehicle* vp)
{
    vector road_pos;
    float dist_on_road;
    const object_path_road* road = object_road_get_closest_road_zone(vp->pos, false, road_pos, dist_on_road, rfg_zonex_get_grid_xy(vp->pos), false, false);
    if (road == nullptr || vec_xz_dist_squared(vp->pos, road_pos) >= 900.0f)
    {
        return false;
    }
    road_search_info_placement search_info;
    search_info.original_pos = vp->pos;
    search_info.original_orient = vp->orient;
    search_info.prp = road;
    for (int attempt = 1; attempt <= MAX_ROAD_ATTEMPTS; attempt++)
    {
        const float near_dist = (float)attempt * 4.0f;
        const float far_dist = (float)attempt * 8.0f;
        search_info.min_desired_dist_sqrd = near_dist * near_dist;
        search_info.max_desired_dist_sqrd = far_dist * far_dist;
        search_info.min_road_dist = dist_on_road;
        search_info.max_road_dist = (float)(road->spline_data.num_points - 1);
        if (try_road_stream_pos(search_info, vp))
        {
            return true;
        }
        search_info.min_road_dist = dist_on_road;
        search_info.max_road_dist = 0.0f;
        if (try_road_stream_pos(search_info, vp))
        {
            return true;
        }
    }
    return false;
}

// 0x00B56E10 (:4096)
bool sp_should_move_streamed_vehicle(const vehicle* vp)
{
    const vehicle_spawn_flags& flags = vp->stream_placement_flags;
    if (vp->m_flags.convoy_vehicle || !flags.mobile_spawn || flags.static_placement)
    {
        return false;
    }
    if (vp->get_sub_type() == 5)
    {
        return !stream_area_clear_flier(vp->pos, vp);
    }
    if (!flags.snap_to_ground)
    {
        return false;
    }
    farray<object*, MAX_OBSTRUCTIONS> obstructions;
    return !stream_area_clear(vp->pos, vp, obstructions);
}

// 0x00B56EA0 (:4138)
void sp_move_streamed_vehicle(vehicle* vp)
{
    if (vp->get_sub_type() == 5)
    {
        move_streaming_flier(vp);
    }
    else if (!vp->stream_placement_flags.road_spawn || !move_streaming_vehicle_road(vp))
    {
        move_streaming_vehicle_nav_cell(vp);
    }
}

// 0x00B43B70
road_search_info_placement::road_search_info_placement()
{
}

// 0x00B43B80 (:4163): no caller in the code ported so far.
void sp_place_vehicle_on_ground(vehicle* vp)
{
    vector ground_pos;
    matrix ground_orient;
    if (find_ground_snap_pos(vp->pos, vp->orient, vp->info, ground_pos, ground_orient))
    {
        vp->update_pos_and_orient(ground_pos, ground_orient, true);
    }
}

// 0x00B56EE0 (:4171)
void sp_clean_stream_area(const vehicle* vp)
{
    farray<object*, MAX_OBSTRUCTIONS> obstructions;
    stream_area_clear(vp->pos, vp, obstructions);
    destroy_obstruction_list(obstructions);
    do_general_clean_up(vp->pos, vp);
}

// ---- NPC placement ----

extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp

// 0x00B43100 (:1512): LTCG, point in ESI.
static const nav_cell* get_nearest_nav_cell(const vector& point)
{
    nav_cell* cell = bb_pf_get_nav_cell(point, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (cell == nullptr)
    {
        vector adjusted_pos;
        bb_pf_find_closest_point_on_nav_mesh_slow(point, adjusted_pos, cell);
    }
    return cell;
}

// 0x00B43140 (:1543): skips the random stream for 0 and 1 entries.
static int pick_random_array_index(unsigned int array_size)
{
    return array_size < 2 ? 0 : int_rand_range(0, array_size - 1);
}

// 0x00B487C0 (:1821): LTCG, nav_list in EDX. Original: the pos of the PDB's signature is not read.
static int pick_best_navpoint_index(farray<navpoint_evaluation_set, 512>& nav_list)
{
    int best = 0;
    for (int i = 1; i < nav_list.size(); i++)
    {
        if (nav_list[i].dist_sqrd < nav_list[best].dist_sqrd)
        {
            best = i;
        }
    }
    return best;
}

static const vector NPC_PLACEMENT_BMIN(-0.5f, 0.0f, -0.5f); // 0x016B812C
static const vector NPC_PLACEMENT_BMAX(0.5f, 1.9f, 0.5f); // 0x016B8138
// Magic number: the offsets below are in units of 1.5 m.
static const float NPC_SPAWN_OFFSET_SCALE = 1.5f;
// 0x016B8148: the ring pattern around a cell's centre, nearest first.
static const vector Npc_spawn_npc_offsets[] = {
    vector(0.0f, 0.0f, 0.0f),
    vector(1.0f, 0.0f, 0.0f),
    vector(-1.0f, 0.0f, 0.0f),
    vector(0.0f, 0.0f, 1.0f),
    vector(0.0f, 0.0f, -1.0f),
    vector(1.0f, 0.0f, 1.0f),
    vector(-1.0f, 0.0f, 1.0f),
    vector(-1.0f, 0.0f, -1.0f),
    vector(1.0f, 0.0f, -1.0f),
    vector(2.0f, 0.0f, 0.0f),
    vector(-2.0f, 0.0f, 0.0f),
    vector(0.0f, 0.0f, 2.0f),
    vector(0.0f, 0.0f, -2.0f),
    vector(2.0f, 0.0f, 1.0f),
    vector(2.0f, 0.0f, -1.0f),
    vector(-2.0f, 0.0f, 1.0f),
    vector(-2.0f, 0.0f, -1.0f),
    vector(1.0f, 0.0f, 2.0f),
    vector(-1.0f, 0.0f, 2.0f),
    vector(1.0f, 0.0f, -2.0f),
    vector(-1.0f, 0.0f, -2.0f),
    vector(2.0f, 0.0f, 2.0f),
    vector(2.0f, 0.0f, -2.0f),
    vector(-2.0f, 0.0f, -2.0f),
    vector(-2.0f, 0.0f, 2.0f),
    vector(3.0f, 0.0f, 0.0f),
    vector(0.0f, 0.0f, 3.0f),
    vector(-3.0f, 0.0f, 0.0f),
    vector(0.0f, 0.0f, -3.0f),
    vector(3.0f, 0.0f, 1.0f),
    vector(3.0f, 0.0f, -1.0f),
    vector(1.0f, 0.0f, 3.0f),
    vector(-1.0f, 0.0f, 3.0f),
    vector(-3.0f, 0.0f, 1.0f),
    vector(-3.0f, 0.0f, -1.0f),
    vector(1.0f, 0.0f, -3.0f),
    vector(-1.0f, 0.0f, -3.0f),
    vector(3.0f, 0.0f, 2.0f),
    vector(3.0f, 0.0f, -2.0f),
    vector(2.0f, 0.0f, 3.0f),
    vector(-2.0f, 0.0f, 3.0f),
    vector(-3.0f, 0.0f, 2.0f),
    vector(-3.0f, 0.0f, -2.0f),
    vector(2.0f, 0.0f, -3.0f),
    vector(-2.0f, 0.0f, -3.0f),
    vector(3.0f, 0.0f, 3.0f),
    vector(3.0f, 0.0f, -3.0f),
    vector(-3.0f, 0.0f, 3.0f),
    vector(-3.0f, 0.0f, -3.0f),
};
static const int NUM_NPC_SPAWN_OFFSETS = sizeof(Npc_spawn_npc_offsets) / sizeof(Npc_spawn_npc_offsets[0]);

// 0x00B4FD40 (:281)
building_child_list::building_child_list()
    : spawn_objp(nullptr)
    , list_built(false)
{
}

// 0x00B50600
building_child_list::~building_child_list()
{
}

// 0x00B481C0 (:324)
nav_cell_search_class::nav_cell_search_class(nav_cell_search_node* _node_pool, int _node_pool_size)
    : visited_list_head(nullptr)
    , search_list_head(nullptr)
    , node_pool_size(_node_pool_size)
    , node_pool(_node_pool)
    , next_node(0)
{
}

// 0x00B42F30 (:333)
bool nav_cell_search_class::is_empty() const
{
    return search_list_head == nullptr;
}

// 0x00B42F40 (:369)
navcell_evaluation_set::navcell_evaluation_set(const vector& pos, const nav_cell* new_cell)
    : cell(new_cell)
{
    nav_cell_get_centroid(&center_point, new_cell);
    distance_sqrd = vec_dist_squared(pos, center_point);
}

// 0x00B4FE00 (:377). The action nodes within 35 m.
void building_child_list::generate_list_action_node(const object* new_objp)
{
    action_node_list.clear();
    const int num_nodes = World.get_num_objects<object_action_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        const object_action_node* node = World.get_object<object_action_node>(i);
        if (vec_dist_squared(node->pos, new_objp->pos) < 1225.0f)
        {
            action_node_list.add(node);
            if (action_node_list.capacity() <= action_node_list.size())
            {
                return;
            }
        }
    }
}

// 0x00B5A890 (:398). A bounding box object gathers what lies inside it, anything else its children.
void building_child_list::generate_lists(const object* new_objp)
{
    if (list_built)
    {
        return;
    }
    list_built = true;
    spawn_objp = new_objp;
    action_node_list.clear();
    guard_node_list.clear();
    cover_node_list.clear();
    spawn_node_list.clear();
    nav_node_list.clear();
    if (new_objp->get_obj_type() == OT_BOUNDING_BOX)
    {
        world_collect_descendants<object_guard_node>(new_objp, guard_node_list, nullptr, nullptr);
        world_collect_descendants<object_npc_spawn_node>(spawn_objp, spawn_node_list, nullptr, nullptr);
        generate_list_action_node(spawn_objp);
        vector local_min;
        vector local_max;
        spawn_objp->get_local_bbox(local_min, local_max);
        unsigned int handles[512];
        int count = World.collect_objects<navpoint>(local_min, local_max, spawn_objp->pos, spawn_objp->orient, handles, 512, nullptr, nullptr);
        for (int i = 0; i < count; i++)
        {
            const navpoint* navp = handles[i] != 0xFFFFFFFF ? World.object_from_handle<navpoint>(handles[i]) : nullptr;
            // Magic number: navpoint type 10, the type building interiors use. Its name is not recovered.
            if (navp != nullptr && navp->type == 10)
            {
                navpoint_evaluation_set set;
                set.navp = navp;
                set.dist_sqrd = 0.0f;
                nav_node_list.add(set);
            }
        }
        count = World.collect_objects<cover_node>(local_min, local_max, spawn_objp->pos, spawn_objp->orient, handles, 512, nullptr, nullptr);
        for (int i = 0; i < count; i++)
        {
            const cover_node* node = handles[i] != 0xFFFFFFFF ? World.object_from_handle<cover_node>(handles[i]) : nullptr;
            if (node != nullptr && cover_node_list.size() < cover_node_list.capacity())
            {
                cover_node_list.add(node);
            }
        }
        return;
    }
    generate_list_action_node(new_objp);
    const object* first_child = spawn_objp->child_ptr;
    if (first_child == nullptr)
    {
        return;
    }
    const object* child = first_child;
    do
    {
        const int type = child->get_obj_type();
        if (type == OT_NAVPOINT)
        {
            const navpoint* navp = static_cast<const navpoint*>(child);
            if (nav_node_list.size() < nav_node_list.capacity() && navp->type == 10)
            {
                navpoint_evaluation_set set;
                set.navp = navp;
                set.dist_sqrd = 0.0f;
                nav_node_list.add(set);
            }
        }
        else if (type == OT_GUARD_NODE)
        {
            if (guard_node_list.size() < guard_node_list.capacity())
            {
                guard_node_list.add(static_cast<const object_guard_node*>(child));
            }
        }
        else if (type == OT_COVER_NODE)
        {
            if (cover_node_list.size() < cover_node_list.capacity())
            {
                cover_node_list.add(static_cast<const cover_node*>(child));
            }
        }
        else if (type == OT_NPC_SPAWN_NODE)
        {
            if (spawn_node_list.size() < spawn_node_list.capacity())
            {
                spawn_node_list.add(static_cast<const object_npc_spawn_node*>(child));
            }
        }
        child = child->child_next;
    } while (child != first_child && child != nullptr);
}

// 0x00B481E0 (:505). Original bug kept: action_node_list is not cleared (generate_lists clears it anyway).
void building_child_list::reset()
{
    spawn_objp = nullptr;
    list_built = false;
    guard_node_list.clear();
    cover_node_list.clear();
    nav_node_list.clear();
    spawn_node_list.clear();
}

// 0x00B42F70 (:524). Moves the nearest cell of the search list to the visited list.
const nav_cell_search_node* nav_cell_search_class::get_next_searh_cell()
{
    nav_cell_search_node* head = search_list_head;
    if (head == nullptr)
    {
        return nullptr;
    }
    nav_cell_search_node* best = head;
    nav_cell_search_node* node = head;
    do
    {
        if (node->distance_sqrd < best->distance_sqrd)
        {
            best = node;
        }
        node = node->next;
    } while (node != head && node != nullptr);
    bool unlinked = false;
    if (head == best)
    {
        if (best->next == best)
        {
            best->next = nullptr;
            best->prev = nullptr;
            search_list_head = nullptr;
            unlinked = true;
        }
        else
        {
            search_list_head = best->next;
        }
    }
    if (!unlinked)
    {
        if (best->prev != nullptr)
        {
            best->prev->next = best->next;
        }
        if (best->next != nullptr)
        {
            best->next->prev = best->prev;
        }
        best->next = nullptr;
        best->prev = nullptr;
    }
    if (visited_list_head != nullptr)
    {
        best->prev = visited_list_head->prev;
        best->next = visited_list_head;
        visited_list_head->prev->next = best;
        visited_list_head->prev = best;
        return best;
    }
    best->next = best;
    best->prev = best;
    visited_list_head = best;
    return best;
}

// 0x00B43020 (:553)
int nav_cell_search_class::search_set_size() const
{
    int count = 0;
    const nav_cell_search_node* node = search_list_head;
    while (node != nullptr)
    {
        node = node->next;
        count++;
        if (node == search_list_head)
        {
            break;
        }
    }
    return count;
}

// 0x00B43040 (:566)
void nav_cell_search_class::reset()
{
    search_list_head = nullptr;
    visited_list_head = nullptr;
    next_node = 0;
}

// 0x00B48210 (:575). Up to 6 cells deep, each cell once.
void nav_cell_search_class::add_new_node(const nav_cell* new_nav_cell, const vector& spawn_point, int new_depth)
{
    if (6 <= new_depth)
    {
        return;
    }
    for (int i = 0; i < next_node; i++)
    {
        if (node_pool[i].m_nav_cell == new_nav_cell)
        {
            return;
        }
    }
    nav_cell_search_node* node = get_new_node();
    if (node == nullptr)
    {
        return;
    }
    node->m_nav_cell = new_nav_cell;
    nav_cell_get_centroid(&node->center_point, new_nav_cell);
    node->distance_sqrd = vec_dist_squared(spawn_point, node->center_point);
    node->depth = new_depth;
    if (search_list_head == nullptr)
    {
        node->next = node;
        node->prev = node;
        search_list_head = node;
        return;
    }
    node->prev = search_list_head->prev;
    node->next = search_list_head;
    search_list_head->prev->next = node;
    search_list_head->prev = node;
}

// 0x00B43050 (:618)
nav_cell_search_node* nav_cell_search_class::get_new_node()
{
    return next_node < node_pool_size ? &node_pool[next_node++] : nullptr;
}

// 0x00B43070 (:712)
static const object* find_top_parent_object_placement(const object* objp)
{
    if (objp == nullptr)
    {
        return nullptr;
    }
    while (true)
    {
        const object* parent = World.object_from_handle(objp->get_parent());
        if (parent == nullptr)
        {
            return objp;
        }
        objp = parent;
    }
}

// 0x00B48330 (:950). LTCG: spawn_params EAX. Spawns once: a second call answers what the first made.
static npc* spawn_npc(const npc_spawn_param_list& spawn_param_list, npc_spawn_params& spawn_params)
{
    if (spawn_params.np != nullptr || spawn_params.spawn_flags.attempted_spawned)
    {
        return spawn_params.np;
    }
    spawn_params.spawn_flags.attempted_spawned = true;
    if (spawn_params.hum_info == nullptr || spawn_params.spawn_pos.is_null())
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return nullptr;
    }
    matrix orient;
    if (spawn_params.spawn_orient.is_valid() && spawn_params.spawn_orient.is_orthonormal())
    {
        orient = spawn_params.spawn_orient;
    }
    else if (spawn_param_list.spawn_orient.is_valid() && spawn_param_list.spawn_orient.is_orthonormal())
    {
        orient = spawn_param_list.spawn_orient;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        orient.make_identity();
    }
    obj_human_props props(spawn_params.hum_info, spawn_params.spawn_pos, orient, nullptr);
    props.set_buf(NPC_PROP_NPC_FLAGS, (const unsigned char*)&spawn_param_list.npc_flags, sizeof(spawn_param_list.npc_flags));
    props.set_buf(HUMAN_PROP_HUMAN_FLAGS, (const unsigned char*)&spawn_param_list.hflags, sizeof(spawn_param_list.hflags));
    props.set<int>(HUMAN_PROP_MANAGEMENT_SYSTEM, spawn_param_list.management_system);
    props.set<int>(HUMAN_PROP_LOAD_OUT, spawn_params.member_load);
    if (-1 < spawn_param_list.ambient_behavior_id)
    {
        props.set<int>(HUMAN_PROP_AMBIENT_BEHAVIOR, spawn_param_list.ambient_behavior_id);
    }
    if (spawn_params.spawn_flags.nav_cell_set)
    {
        props.set<uint64_t>(HUMAN_PROP_NAV_CELL_HANDLE, spawn_params.nav_cell_handle);
    }
    // Deviation from rfg.exe: create_object<npc>, where it calls create_object_internal directly.
    object* objp = World.create_object<npc>(props, 0xFFFFFFFF);
    spawn_params.np = static_cast<npc*>(objp);
    if (objp != nullptr)
    {
        objp->obj_flags.no_save = !spawn_param_list.spawn_flags.should_save;
    }
    // Original bug kept: a failed create_object_internal crashes on the next line's handle.
    const unsigned int handle = spawn_params.np->get_handle();
    human* hp = handle != 0xFFFFFFFF ? World.object_from_handle<human>(handle) : nullptr;
    // Magic numbers: the byte at human_info +0x17F is a bool; it sets hflags bit 0x0C:1 once.
    if (hp != nullptr && ((const unsigned char*)hp->h_info)[0x17F] != 0 && !hp->hflags.driverless_exit_only)
    {
        hp->hflags.driverless_exit_only = 1;
    }
    return spawn_params.np;
}

// 0x00B430D0 (:1027). LTCG: test_object EAX, parent_object ESI.
static bool is_parented_to(unsigned int test_object, unsigned int parent_object)
{
    while (test_object != 0xFFFFFFFF && parent_object != 0xFFFFFFFF)
    {
        if (test_object == parent_object)
        {
            return true;
        }
        test_object = World.object_from_handle(test_object)->get_parent();
    }
    return false;
}

// No npc within 1 m. Inlined at the end of both can_drop functions.
static bool npc_clear_of_others(const vector& position)
{
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        if (vec_dist_squared(World.get_object<npc>(i)->pos, position) < 1.0f)
        {
            return false;
        }
    }
    return true;
}

// 0x00B4FFB0 (:1057). LTCG: position stack, ignore_object ECX. Only ignore_object and its descendants may stand there.
static bool can_drop_npc_at_point(const vector& position, unsigned int ignore_object)
{
    const vector box_min(position.x + NPC_PLACEMENT_BMIN.x, position.y + NPC_PLACEMENT_BMIN.y, position.z + NPC_PLACEMENT_BMIN.z);
    const vector box_max(position.x + NPC_PLACEMENT_BMAX.x, position.y + NPC_PLACEMENT_BMAX.y, position.z + NPC_PLACEMENT_BMAX.z);
    unsigned int handles[16];
    const int count = havok_collect_objects_fast(box_min, box_max, handles, 16, COLLIDE_OBJECT_COLLECTOR, nullptr, nullptr, nullptr);
    if (0 < count)
    {
        if (ignore_object == 0xFFFFFFFF)
        {
            return false;
        }
        for (int i = 0; i < count; i++)
        {
            const unsigned int handle = handles[i];
            if (handle == ignore_object)
            {
                continue;
            }
            if (handle == 0xFFFFFFFF)
            {
                return false;
            }
            const unsigned int parent = World.object_from_handle(handle)->get_parent();
            if (parent == 0xFFFFFFFF || (parent != ignore_object && !is_parented_to(parent, ignore_object)))
            {
                return false;
            }
        }
    }
    return npc_clear_of_others(position);
}

// 0x00B5ABE0 (:1185). LTCG: position stack, ignore_object ECX. Standing on a mover is fine if its top is within 0.43 m
// of the position: the npc goes up onto it.
static bool can_drop_npc(vector& position, unsigned int ignore_object)
{
    if (ignore_object == 0xFFFFFFFF)
    {
        return can_drop_npc_at_point(position, ignore_object);
    }
    unsigned int handles[16];
    const int count = havok_collect_objects(NPC_PLACEMENT_BMIN, NPC_PLACEMENT_BMAX, position, IDENTITY_MATRIX, handles, 16, COLLIDE_OBJECT_COLLECTOR, nullptr, nullptr);
    if (1 < count)
    {
        return false;
    }
    if (count == 1 && handles[0] != ignore_object)
    {
        const object* objp = World.object_from_handle(handles[0]);
        if (objp == nullptr || objp->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            return false;
        }
        raycast_input input;
        input.start = vector(position.x, position.y + 2.33f, position.z);
        input.end = position;
        input.radius = 0.0f;
        input.m_filter_info = COLLIDE_RAY_DEFAULT;
        input.filter_params = nullptr;
        input.filter_callback = nullptr;
        input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        input.m_flags = 0;
        raycast_output output;
        if (!havok_raycast(input, output) || 0.43f <= output.m_hits[0].hit_pos.y - position.y)
        {
            return false;
        }
        position.y = output.m_hits[0].hit_pos.y;
    }
    return npc_clear_of_others(position);
}

// 0x00B5ADE0 (:1355). Original bug kept: the first npc not yet tried stays the current one through all the offsets, so
// a call places one npc, and the later offsets only overwrite its spawn_pos; spawn_npc answers without spawning again.
// Where several npcs share a cell the rest wait for the caller's next cell.
static bool spawn_npcs_in_nav_cell(const nav_cell* cell, npc_spawn_param_list& spawn_param_list)
{
    if (cell == nullptr)
    {
        return false;
    }
    vector centroid;
    nav_cell_get_centroid(&centroid, cell);
    npc_spawn_params* params = nullptr;
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        if (!spawn_param_list.npc_spawn_info[i].spawn_flags.attempted_spawned)
        {
            params = &spawn_param_list.npc_spawn_info[i];
            break;
        }
    }
    if (params != nullptr)
    {
        for (int i = 0; i < NUM_NPC_SPAWN_OFFSETS; i++)
        {
            const vector& offset = Npc_spawn_npc_offsets[i];
            vector pos(centroid.x + offset.x * NPC_SPAWN_OFFSET_SCALE, centroid.y + offset.y * NPC_SPAWN_OFFSET_SCALE, centroid.z + offset.z * NPC_SPAWN_OFFSET_SCALE);
            if (!nav_cell_is_point_in_cell_column(cell, pos))
            {
                continue;
            }
            havok_terrain_height(pos.y, pos.x, pos.z, false, nullptr);
            if (can_drop_npc(pos, 0xFFFFFFFF))
            {
                params->spawn_pos = pos;
                params->spawn_orient = spawn_param_list.spawn_orient;
                params->set_nav_cell_handle(nav_cell_get_handle(cell));
                spawn_npc(spawn_param_list, *params);
            }
        }
    }
    return spawn_param_list.all_spawned();
}

// 0x00B485D0 (:1565). The cells within 10 m across and 50 m up or down, or failing that the nearest cell.
static void create_search_data_set(const vector& point, nav_cell_search_class& search_data)
{
    static const vector SEARCH_BBOX_MIN(-10.0f, -50.0f, -10.0f);
    static const vector SEARCH_BBOX_MAX(10.0f, 50.0f, 10.0f);
    nav_cell* cells[128];
    const int count = havok_collect_nav_cells(SEARCH_BBOX_MIN, SEARCH_BBOX_MAX, point, IDENTITY_MATRIX, cells, 128, false, nullptr);
    if (0 < count)
    {
        for (int i = 0; i < count; i++)
        {
            search_data.add_new_node(cells[i], point, 0);
        }
        return;
    }
    const nav_cell* cell = get_nearest_nav_cell(point);
    if (cell != nullptr)
    {
        search_data.add_new_node(cell, point, 0);
    }
}

// 0x00B5AFD0 (:1601). LTCG: spawn_param_list EDI. Here each placed npc moves on to the next one.
static bool do_npc_spawn_location_nav_cell(npc_spawn_param_list& spawn_param_list, const nav_cell* ncp, const vector& cell_center)
{
    for (int i = 0; i < NUM_NPC_SPAWN_OFFSETS; i++)
    {
        const vector& offset = Npc_spawn_npc_offsets[i];
        vector pos(cell_center.x + offset.x * NPC_SPAWN_OFFSET_SCALE, cell_center.y + offset.y * NPC_SPAWN_OFFSET_SCALE, cell_center.z + offset.z * NPC_SPAWN_OFFSET_SCALE);
        if (!nav_cell_is_point_in_cell_column(ncp, pos))
        {
            continue;
        }
        havok_terrain_height(pos.y, pos.x, pos.z, false, nullptr);
        if (!can_drop_npc(pos, 0xFFFFFFFF))
        {
            continue;
        }
        npc_spawn_params* params = spawn_param_list.find_next_npc_spawn();
        if (params == nullptr)
        {
            return true;
        }
        params->spawn_pos = pos;
        params->spawn_orient = spawn_param_list.spawn_orient;
        params->set_nav_cell_handle(nav_cell_get_handle(ncp));
        spawn_npc(spawn_param_list, *params);
        if (spawn_param_list.all_spawned())
        {
            return true;
        }
    }
    return false;
}

// 0x00B5B190 (:1680). The region's cells in random order.
static void do_npc_spawn_region(npc_spawn_param_list& spawn_param_list)
{
    vector local_min;
    vector local_max;
    spawn_param_list.osrp->get_local_bbox(local_min, local_max);
    nav_cell* cells[256];
    int count = havok_collect_nav_cells(local_min, local_max, spawn_param_list.osrp->pos, spawn_param_list.osrp->orient, cells, 256, false, nullptr);
    while (0 < count)
    {
        const int pick = pick_random_array_index(count);
        const nav_cell* cell = cells[pick];
        count--;
        cells[pick] = cells[count];
        if (cell != nullptr && spawn_npcs_in_nav_cell(cell, spawn_param_list))
        {
            return;
        }
    }
}

// 0x00B486F0 (:1717). LTCG: spawn_node_list ESI. The nearest node that takes this human, taken off the list.
static const object_npc_spawn_node* find_best_spawn_node(const vector& spawn_pos, const npc_spawn_params& npc_param, farray<object_npc_spawn_node const*, 64>& spawn_node_list)
{
    int best_index = -1;
    const object_npc_spawn_node* best = nullptr;
    float best_dist_sq = FLT_MAX;
    for (int i = 0; i < spawn_node_list.size(); i++)
    {
        const object_npc_spawn_node* node = spawn_node_list[i];
        if (node->has_human_info(npc_param.hum_info))
        {
            const float dist_sq = vec_dist_squared(spawn_pos, node->pos);
            if (dist_sq < best_dist_sq)
            {
                best_index = i;
                best_dist_sq = dist_sq;
                best = node;
            }
        }
    }
    if (best != nullptr)
    {
        spawn_node_list.remove_unsorted(best_index);
    }
    return best;
}

// 0x00B5B410 (:1750). The free spawn nodes within 10 m on each axis.
static void do_npc_spawn_node(npc_spawn_param_list& spawn_param_list)
{
    const vector& pos = spawn_param_list.spawn_pos;
    const vector box_min(pos.x - 10.0f, pos.y - 10.0f, pos.z - 10.0f);
    const vector box_max(pos.x + 10.0f, pos.y + 10.0f, pos.z + 10.0f);
    unsigned int handles[64];
    const int count = World.collect_objects<object_npc_spawn_node>(box_min, box_max, handles, 64, nullptr, nullptr);
    if (count < 1)
    {
        return;
    }
    farray<object_npc_spawn_node const*, 64> nodes;
    for (int i = 0; i < count; i++)
    {
        const object_npc_spawn_node* node = handles[i] != 0xFFFFFFFF ? World.object_from_handle<object_npc_spawn_node>(handles[i]) : nullptr;
        if (node != nullptr && node->get_parent() == 0xFFFFFFFF)
        {
            nodes.add(node);
        }
    }
    if (nodes.size() < 1)
    {
        return;
    }
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned)
        {
            continue;
        }
        while (0 < nodes.size())
        {
            const object_npc_spawn_node* node = find_best_spawn_node(spawn_param_list.spawn_pos, params, nodes);
            if (node == nullptr)
            {
                break;
            }
            params.spawn_pos = node->pos;
            params.spawn_orient = spawn_param_list.spawn_orient;
            havok_terrain_height(params.spawn_pos.y, params.spawn_pos.x, params.spawn_pos.z, false, nullptr);
            if (can_drop_npc(params.spawn_pos, spawn_param_list.spawn_building_handle))
            {
                spawn_npc(spawn_param_list, params);
                break;
            }
        }
    }
}

// 0x00B5B710 (:1839). LTCG: spawn_param_list EDI. Points every 1.25 m along the link between two navpoints.
static bool do_npc_spawn_building_nav_link(npc_spawn_param_list& spawn_param_list, const navpoint* navp, const navpoint* end_navp)
{
    const vector start_pos = navp->pos;
    const vector end_pos = end_navp->pos;
    vector dir(end_pos.x - start_pos.x, end_pos.y - start_pos.y, end_pos.z - start_pos.z);
    const int steps = (int)(dir.mag_and_normalize_safe() / 1.25);
    for (int i = 0; i <= steps; i++)
    {
        const float along = (float)i * 1.25f;
        vector pos(dir.x * along + start_pos.x, dir.y * along + start_pos.y, dir.z * along + start_pos.z);
        if (!can_drop_npc(pos, spawn_param_list.spawn_building_handle))
        {
            continue;
        }
        npc_spawn_params* params = spawn_param_list.find_next_npc_spawn();
        if (params == nullptr)
        {
            return true;
        }
        params->spawn_pos = pos;
        params->spawn_orient = spawn_param_list.spawn_orient;
        // The cell is the one reached in a straight line from either end of the link.
        vector out_pos;
        const nav_cell* out_cell = nullptr;
        uint64_t handle = 0xFFFFFFFFFFFFFFFFull;
        if (navp->nav_cell_ptr != nullptr && nav_cell_find_longest_line_of_sight_no_detour(out_pos, out_cell, start_pos, navp->nav_cell_ptr, pos, false, nullptr))
        {
            handle = nav_cell_get_handle(out_cell);
        }
        else if (end_navp->nav_cell_ptr != nullptr && nav_cell_find_longest_line_of_sight_no_detour(out_pos, out_cell, end_pos, end_navp->nav_cell_ptr, pos, false, nullptr))
        {
            handle = nav_cell_get_handle(out_cell);
        }
        params->set_nav_cell_handle(handle);
        spawn_npc(spawn_param_list, *params);
    }
    return spawn_param_list.all_spawned();
}

// 0x00B5BA60 (:1910). The enabled links of a navpoint, nearest first.
static bool do_npc_spawn_navpoint_edge(npc_spawn_param_list& spawn_param_list, const navpoint* navp)
{
    farray<navpoint_evaluation_set, 512> links;
    for (int i = 0; i < navp->num_links; i++)
    {
        const navpoint* link = navp->links[i];
        if (Navpoint_treat_all_as_enabled || link->heap_data.disabled_by == 0)
        {
            navpoint_evaluation_set set;
            set.navp = link;
            set.dist_sqrd = vec_dist_squared(spawn_param_list.spawn_pos, link->pos);
            links.add(set);
        }
    }
    while (0 < links.size())
    {
        const int best = pick_best_navpoint_index(links);
        const navpoint* link = links[best].navp;
        links.remove_unsorted(best);
        if (do_npc_spawn_building_nav_link(spawn_param_list, navp, link))
        {
            return true;
        }
    }
    return false;
}


// 0x00B5BBF0 (:1958): LTCG, navp in EAX.
static bool do_npc_spawn_navpoint(npc_spawn_param_list& spawn_param_list, const navpoint* navp)
{
    return (Navpoint_treat_all_as_enabled || navp->heap_data.disabled_by == 0) && do_npc_spawn_navpoint_edge(spawn_param_list, navp);
}
// 0x00B48810 (:1975). LTCG: action_node_list EDI.
static int find_nearest_action_node(const vector& pos, const farray<object_action_node const*, 608>& action_node_list)
{
    int best = -1;
    float best_dist_sq = FLT_MAX;
    for (int i = 0; i < action_node_list.size(); i++)
    {
        const float dist_sq = vec_dist_squared(action_node_list[i]->pos, pos);
        if (dist_sq < best_dist_sq)
        {
            best = i;
            best_dist_sq = dist_sq;
        }
    }
    return best;
}

// 0x00B50280 (:2002). LTCG: current_spawn_param EDI, anp ESI, spawn_param_list EBX.
static bool do_single_npc_spawn_action(npc_spawn_param_list& spawn_param_list, npc_spawn_params& current_spawn_param, const object_action_node* anp)
{
    if (anp->disabled_by != 0 || spawn_param_list.node_type != anp->get_ambient_type())
    {
        return false;
    }
    const object* parent = World.object_from_handle(anp->get_parent());
    const object* top = parent != nullptr ? find_top_parent_object_placement(parent) : anp;
    if (top == nullptr || !can_drop_npc_at_point(anp->pos, top->get_handle()))
    {
        return false;
    }
    if (!anp->get_entry_point(nullptr, current_spawn_param.spawn_pos) || !anp->get_entry_rotation(nullptr, current_spawn_param.spawn_orient))
    {
        return false;
    }
    return spawn_npc(spawn_param_list, current_spawn_param) != nullptr;
}

// 0x00B50310 (:2034). LTCG: action_node_list EAX. A node that fails is off the list for the next npc too.
static bool do_npc_spawn_action_node_list(npc_spawn_param_list& spawn_param_list, farray<object_action_node const*, 608>& action_node_list, bool nearest_node)
{
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned)
        {
            continue;
        }
        while (0 < action_node_list.size())
        {
            int index;
            if (nearest_node)
            {
                index = find_nearest_action_node(spawn_param_list.spawn_pos, action_node_list);
            }
            else
            {
                index = pick_random_array_index(action_node_list.size());
            }
            if (index < 0 || action_node_list.size() <= index)
            {
                return false;
            }
            const object_action_node* node = action_node_list[index];
            action_node_list.remove_unsorted(index);
            if (do_single_npc_spawn_action(spawn_param_list, params, node))
            {
                break;
            }
        }
    }
    return spawn_param_list.all_spawned();
}

// Splits nodes into high priority and normal ones, tries the high priority ones nearest first, then the rest at random.
static bool spawn_in_sorted_action_nodes(npc_spawn_param_list& spawn_param_list, farray<object_action_node const*, 608>& high_priority, farray<object_action_node const*, 608>& normal)
{
    if (0 < high_priority.size() && do_npc_spawn_action_node_list(spawn_param_list, high_priority, true))
    {
        return true;
    }
    return 0 < normal.size() && do_npc_spawn_action_node_list(spawn_param_list, normal, false);
}

// 0x00B50490 (:2076). The free action nodes of the wanted type within 30 m.
static bool do_npc_spawn_action_node(npc_spawn_param_list& spawn_param_list)
{
    farray<object_action_node const*, 608> high_priority;
    farray<object_action_node const*, 608> normal;
    const int num_nodes = World.get_num_objects<object_action_node>();
    if (num_nodes < 1)
    {
        return false;
    }
    for (int i = 0; i < num_nodes; i++)
    {
        const object_action_node* node = World.get_object<object_action_node>(i);
        if (spawn_param_list.node_type == node->get_ambient_type() && vec_dist_squared(spawn_param_list.spawn_pos, node->pos) <= 900.0f && node->disabled_by == 0 && !node->is_occupied())
        {
            (node->node_flags.high_priority ? high_priority : normal).add(node);
        }
    }
    return spawn_in_sorted_action_nodes(spawn_param_list, high_priority, normal);
}

// 0x00B5BC20 (:2136). Spawn nodes, navpoint, action nodes, region, then a search over the nav cells around the position.
static void do_npc_spawn_location(npc_spawn_param_list& spawn_param_list)
{
    if (spawn_param_list.spawn_flags.spawn_on_node)
    {
        do_npc_spawn_node(spawn_param_list);
        if (spawn_param_list.all_spawned())
        {
            return;
        }
    }
    const navpoint* navp = spawn_param_list.navp;
    if (navp != nullptr && (do_npc_spawn_navpoint(spawn_param_list, navp) || navp->get_parent() != 0xFFFFFFFF))
    {
        return;
    }
    if (spawn_param_list.spawn_flags.spawn_in_action_node && do_npc_spawn_action_node(spawn_param_list))
    {
        return;
    }
    if (spawn_param_list.osrp != nullptr)
    {
        do_npc_spawn_region(spawn_param_list);
        if (spawn_param_list.all_spawned())
        {
            return;
        }
    }
    // rfg.exe takes this 0x4000 byte pool from the thread's temporary mempool.
    nav_cell_search_node node_pool[512];
    nav_cell_search_class search(node_pool, 512);
    havok_terrain_height(spawn_param_list.spawn_pos.y, spawn_param_list.spawn_pos.x, spawn_param_list.spawn_pos.z, false, nullptr);
    create_search_data_set(spawn_param_list.spawn_pos, search);
    while (!search.is_empty())
    {
        const nav_cell_search_node* node = search.get_next_searh_cell();
        const nav_cell* cell = node->m_nav_cell;
        // Magic number: nav cell flag 8, detour cells.
        if ((cell->flags & 8) == 0 && do_npc_spawn_location_nav_cell(spawn_param_list, cell, node->center_point))
        {
            return;
        }
        for (int edge = 0; edge < 3; edge++)
        {
            const nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
            if (adjacent != nullptr)
            {
                search.add_new_node(adjacent, spawn_param_list.spawn_pos, node->depth + 1);
            }
        }
    }
}

// 0x00B488B0 (:2222)
bool find_nearest_spawn_node(const vector& pos, farray<object_npc_spawn_node const*, 64>& spawn_node_list, const human_info* h_info, int& best_node_index)
{
    int best = -1;
    float best_dist_sq = FLT_MAX;
    for (int i = 0; i < spawn_node_list.size(); i++)
    {
        if (spawn_node_list[i]->has_human_info(h_info))
        {
            const float dist_sq = vec_dist_squared(spawn_node_list[i]->pos, pos);
            if (dist_sq < best_dist_sq)
            {
                best = i;
                best_dist_sq = dist_sq;
            }
        }
    }
    if (best < 0)
    {
        return false;
    }
    best_node_index = best;
    return true;
}

// 0x00B5BE10 (:2250)
static bool do_npc_spawn_building_spawn_node(npc_spawn_param_list& spawn_param_list, building_child_list& child_lists)
{
    if (!spawn_param_list.spawn_flags.spawn_on_node)
    {
        return false;
    }
    child_lists.generate_lists(spawn_param_list.spawn_parent_obj);
    if (child_lists.spawn_node_list.size() < 1)
    {
        return false;
    }
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned)
        {
            continue;
        }
        // Original quirk kept: the loop counts up while the list shrinks, so it can stop with nodes left, and it goes on
        // after a spawn, when spawn_npc answers without spawning again.
        for (int tries = 0; tries < child_lists.spawn_node_list.size(); tries++)
        {
            int best;
            if (!find_nearest_spawn_node(spawn_param_list.spawn_pos, child_lists.spawn_node_list, params.hum_info, best))
            {
                break;
            }
            const object_npc_spawn_node* node = child_lists.spawn_node_list[best];
            child_lists.spawn_node_list.remove_unsorted(best);
            if (can_drop_npc_at_point(node->pos, spawn_param_list.spawn_building_handle))
            {
                params.spawn_pos = node->pos;
                params.spawn_orient = node->orient;
                params.set_nav_cell_handle(0xFFFFFFFFFFFFFFFFull);
                spawn_npc(spawn_param_list, params);
            }
        }
    }
    return spawn_param_list.all_spawned();
}

// 0x00B5BFC0 (:2307). LTCG: child_lists ESI. Nearest navpoint first.
static bool do_npc_spawn_building_navlist(npc_spawn_param_list& spawn_param_list, building_child_list& child_lists)
{
    const object* parent = spawn_param_list.spawn_parent_obj;
    child_lists.generate_lists(parent);
    farray<navpoint_evaluation_set, 512>& nodes = child_lists.nav_node_list;
    if (nodes.size() < 1)
    {
        return false;
    }
    if (spawn_param_list.spawn_pos.is_null())
    {
        spawn_param_list.spawn_pos = parent->pos;
    }
    for (int i = 0; i < nodes.size(); i++)
    {
        nodes[i].dist_sqrd = vec_dist_squared(spawn_param_list.spawn_pos, nodes[i].navp->pos);
    }
    while (0 < nodes.size())
    {
        const int best = pick_best_navpoint_index(nodes);
        const navpoint* navp = nodes[best].navp;
        nodes.remove_unsorted(best);
        if (do_npc_spawn_navpoint(spawn_param_list, navp))
        {
            return true;
        }
    }
    return false;
}

// 0x00B43160 (:2361)
static bool guard_node_elevation_greater_or_equal(const object_guard_node* const& node1, const object_guard_node* const& node2)
{
    return node2->pos.y <= node1->pos.y;
}

// 0x00B5C110 (:2367)
static bool do_npc_spawn_building_action(npc_spawn_param_list& spawn_param_list, building_child_list& child_lists)
{
    if (!spawn_param_list.spawn_flags.spawn_in_action_node)
    {
        return false;
    }
    child_lists.generate_lists(spawn_param_list.spawn_parent_obj);
    if (child_lists.action_node_list.size() < 1)
    {
        return false;
    }
    farray<object_action_node const*, 608> high_priority;
    farray<object_action_node const*, 608> normal;
    for (int i = 0; i < child_lists.action_node_list.size(); i++)
    {
        const object_action_node* node = child_lists.action_node_list[i];
        if (spawn_param_list.node_type == node->get_ambient_type() && node->disabled_by == 0 && !node->is_occupied())
        {
            (node->node_flags.high_priority ? high_priority : normal).add(node);
        }
    }
    child_lists.action_node_list.clear();
    return spawn_in_sorted_action_nodes(spawn_param_list, high_priority, normal);
}

// The ordered removal the sniper passes use, keeping the list sorted by height.
template<typename T>
static T take_ordered(base_array<T>& list, int index)
{
    T taken = list[index];
    list.remove(index);
    return taken;
}

static bool guard_node_usable(const object_guard_node* node, unsigned int ignore_object)
{
    return !node->is_disabled() && can_drop_npc_at_point(node->pos, ignore_object);
}

// 0x00B5C250 (:2432). LTCG: child_lists ESI. Snipers first, high up: a random node among those within 1.5 m of the top.
static bool do_npc_spawn_building_guard(npc_spawn_param_list& spawn_param_list, building_child_list& child_lists)
{
    if (!spawn_param_list.spawn_flags.spawn_in_guard)
    {
        return false;
    }
    child_lists.generate_lists(spawn_param_list.spawn_parent_obj);
    farray<object_guard_node const*, 512>& nodes = child_lists.guard_node_list;
    if (nodes.size() < 1)
    {
        return false;
    }
    bool sorted = false;
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned || !params.hum_info->flags.sniper)
        {
            continue;
        }
        if (!sorted)
        {
            nodes.sort(guard_node_elevation_greater_or_equal);
            sorted = true;
        }
        bool spawned = false;
        while (0 < nodes.size() && !spawned)
        {
            const float top = nodes[0]->pos.y;
            int group = 0;
            while (group < nodes.size() && top - nodes[group]->pos.y <= 1.5f)
            {
                group++;
            }
            while (0 < group && !spawned)
            {
                const int pick = pick_random_array_index(group);
                const object_guard_node* node = take_ordered<object_guard_node const*>(nodes, pick);
                group--;
                if (guard_node_usable(node, spawn_param_list.spawn_building_handle))
                {
                    params.spawn_pos = node->pos;
                    params.spawn_orient = node->orient;
                    spawn_npc(spawn_param_list, params);
                    spawned = true;
                }
            }
        }
    }
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned)
        {
            continue;
        }
        while (0 < nodes.size())
        {
            const int pick = pick_random_array_index(nodes.size());
            const object_guard_node* node = nodes[pick];
            nodes.remove_unsorted(pick);
            if (guard_node_usable(node, spawn_param_list.spawn_building_handle))
            {
                params.spawn_pos = node->pos;
                params.spawn_orient = node->orient;
                spawn_npc(spawn_param_list, params);
                break;
            }
        }
    }
    return spawn_param_list.all_spawned();
}

// 0x00B43190 (:2551). Popup nodes first, then by height.
static bool cover_node_sniper_sort_func(const cover_node* const& node1, const cover_node* const& node2)
{
    if (node1->flags.fire_popup == node2->flags.fire_popup)
    {
        return node2->pos.y <= node1->pos.y;
    }
    return node1->flags.fire_popup != 0;
}

// Original quirk kept: valid and elapsed, so a cover node whose timer was never set does not count as usable.
static bool cover_node_usable(const cover_node* node, unsigned int ignore_object)
{
    return node->get_disabled_timer().valid() && node->get_disabled_timer().elapsed() && node->is_occupied(nullptr) == nullptr && can_drop_npc_at_point(node->pos, ignore_object);
}

static void spawn_on_cover_node(npc_spawn_param_list& spawn_param_list, npc_spawn_params& params, const cover_node* node)
{
    params.spawn_pos = node->pos;
    params.spawn_orient = node->orient;
    params.set_nav_cell_handle(node->closest_nav_cell_handle);
    spawn_npc(spawn_param_list, params);
}

// 0x00B5C580 (:2562). LTCG: child_lists ESI. Snipers first: a random node among the popups or those within 1.5 m of the top.
static bool do_npc_spawn_building_cover(npc_spawn_param_list& spawn_param_list, building_child_list& child_lists)
{
    if (!spawn_param_list.spawn_flags.spawn_in_cover)
    {
        return false;
    }
    child_lists.generate_lists(spawn_param_list.spawn_parent_obj);
    farray<cover_node const*, 512>& nodes = child_lists.cover_node_list;
    if (nodes.size() < 1)
    {
        return false;
    }
    bool sorted = false;
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned || !params.hum_info->flags.sniper)
        {
            continue;
        }
        if (!sorted)
        {
            nodes.sort(cover_node_sniper_sort_func);
            sorted = true;
        }
        bool spawned = false;
        while (0 < nodes.size() && !spawned)
        {
            const cover_node* top = nodes[0];
            int group = 0;
            while (group < nodes.size() && !(top->flags.fire_popup && !nodes[group]->flags.fire_popup) && top->pos.y - nodes[group]->pos.y <= 1.5f)
            {
                group++;
            }
            while (0 < group && !spawned)
            {
                const int pick = pick_random_array_index(group);
                const cover_node* node = take_ordered<cover_node const*>(nodes, pick);
                group--;
                if (cover_node_usable(node, spawn_param_list.spawn_building_handle))
                {
                    spawn_on_cover_node(spawn_param_list, params, node);
                    spawned = true;
                }
            }
        }
    }
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned)
        {
            continue;
        }
        while (0 < nodes.size())
        {
            const int pick = pick_random_array_index(nodes.size());
            const cover_node* node = nodes[pick];
            nodes.remove_unsorted(pick);
            if (cover_node_usable(node, spawn_param_list.spawn_building_handle))
            {
                spawn_on_cover_node(spawn_param_list, params, node);
                break;
            }
        }
    }
    return spawn_param_list.all_spawned();
}

// 0x00B5C990 (:2701). LTCG: spawn_param_list EDI.
static void do_npc_spawn_building(npc_spawn_param_list& spawn_param_list)
{
    const object* parent = spawn_param_list.spawn_parent_obj;
    if (parent == nullptr)
    {
        return;
    }
    building_child_list child_lists;
    if (!do_npc_spawn_building_spawn_node(spawn_param_list, child_lists) && !do_npc_spawn_building_cover(spawn_param_list, child_lists) && !do_npc_spawn_building_guard(spawn_param_list, child_lists) && !do_npc_spawn_building_action(spawn_param_list, child_lists) && !do_npc_spawn_building_navlist(spawn_param_list, child_lists) && spawn_param_list.spawn_pos.is_null())
    {
        spawn_param_list.spawn_pos = parent->pos;
    }
}

// 0x00B50630 (:2744). Each npc appears at the vehicle and teleports into a seat. From the driver's seat the game picks
// the best seat, from any other the next free one.
static void do_npc_spawn_vehicle(npc_spawn_param_list& spawn_param_list)
{
    vehicle* vp = spawn_param_list.vp;
    if (vp == nullptr)
    {
        return;
    }
    vehicle_seat_index seat = spawn_param_list.first_seat;
    spawn_param_list.spawn_pos = vp->pos;
    const int num_seats = vehicle_get_num_seats(vp);
    const int wanted = num_seats - seat;
    int occupied = 0;
    for (int i = seat; i < vehicle_get_num_seats(vp); i++)
    {
        if (vp->m_seat_info[i].m_current_occupant_handle != 0xFFFFFFFF)
        {
            occupied++;
        }
    }
    if (wanted <= occupied)
    {
        return;
    }
    const bool best_seat = seat < 1;
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (params.spawn_flags.attempted_spawned)
        {
            continue;
        }
        params.spawn_pos = vp->pos;
        params.spawn_orient = vp->orient;
        params.set_nav_cell_handle(0xFFFFFFFFFFFFFFFFull);
        spawn_npc(spawn_param_list, params);
        if (params.np == nullptr)
        {
            continue;
        }
        vi_enter_data data;
        if (best_seat)
        {
            data.seat_selection_method = VI_SEAT_SELECTION_METHOD_BEST;
        }
        else
        {
            while (vehicle_passenger_in_or_entering_seat(vp, seat) != nullptr)
            {
                seat = (vehicle_seat_index)(seat + 1);
                if (num_seats <= seat)
                {
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                    break;
                }
            }
            data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
            data.specified_seat = seat;
            seat = (vehicle_seat_index)(seat + 1);
        }
        data.flags.minimal_processing = 1;
        data.flags.teleport = 1;
        vehicle_interaction_request_enter(params.np, vp, &data);
        occupied++;
        if (wanted <= occupied)
        {
            return;
        }
    }
}

// 0x00B5CA50 (:3380)
bool sp_do_spawn(npc_spawn_param_list& spawn_param_list)
{
    if (!spawn_param_list.placement_ready())
    {
        return false;
    }
    spawn_param_list.zone_id_xy = rfg_zonex_get_grid_xy(spawn_param_list.spawn_pos);
    if (spawn_param_list.spawn_flags.static_spawn_placement)
    {
        npc_spawn_params& first = spawn_param_list.npc_spawn_info[0];
        first.spawn_pos = spawn_param_list.spawn_pos;
        first.spawn_orient = spawn_param_list.spawn_orient;
        spawn_npc(spawn_param_list, first);
        if (spawn_param_list.all_spawned())
        {
            return true;
        }
    }
    if (spawn_param_list.vp != nullptr)
    {
        do_npc_spawn_vehicle(spawn_param_list);
    }
    if (spawn_param_list.spawn_parent_obj != nullptr && !spawn_param_list.all_spawned())
    {
        do_npc_spawn_building(spawn_param_list);
    }
    if (!spawn_param_list.all_spawned())
    {
        do_npc_spawn_location(spawn_param_list);
    }
    for (int i = 0; i < spawn_param_list.npc_spawn_info.size(); i++)
    {
        const npc_spawn_params& params = spawn_param_list.npc_spawn_info[i];
        if (!params.spawn_flags.attempted_spawned || params.spawn_failed())
        {
            return false;
        }
    }
    return true;
}
