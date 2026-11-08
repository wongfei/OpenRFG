// Original: volition/rfg/code/spawn/ambient_spawn_manager.cpp
//
// The ambient spawn manager (NPC.md NP4): pedestrians on the ped node networks built from each zone's navpoints, traffic on
// the road points near the player, the guerrilla and vehicle types in rotation, and despawning what is far away or
// offscreen. Single player only: amb_spawn_do_frame returns in multiplayer and Wrecking Crew, though zones in those modes
// still build their networks and no spawn zones in amb_spawn_zone_load_complete. Plain float where rfg.exe uses double
// chains.

#include "volition/rfg/code/spawn/ambient_spawn_manager.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/civilian/ambient_common.h"
#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/road_distance_info.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/spawn/ambient_spawn_behavior.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_request.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/spawn/spawn_resource_manager_internal.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.inl"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/hash_table.inl"

#include <float.h>
#include <math.h>
#include <string.h>

extern navpoint_heap Navpoint_heap; // nav_heap.cpp

// ---- file-local types
// size 0x1C
struct ambient_no_spawn_zone
{
    unsigned int handle; // 0x00
    vector center_pos; // 0x04
    int segment_start; // 0x10
    int segment_end; // 0x14
    float radius_sqrd; // 0x18
};

// size 0x24
struct road_spawn_selection_info
{
    road_spline_identifier spawn_id; // 0x00
    vector pos; // 0x08
    vector direction; // 0x14
    float dot; // 0x20
};

// size 0x8
struct navpoint_cost_pair
{
    navpoint* navp; // 0x00
    float cost; // 0x04
};

// ---- constants. Distances in metres, periods in milliseconds.
static const float MIN_PED_SPAWN_DISTANCE = 20.0f; // 0x01335D44
static const float MIN_CAMERA_PED_SPAWN_DISTANCE = 60.0f; // 0x01335D4C
static const float MIN_VEH_SPAWN_DISTANCE = 60.0f; // the same constant at 0x01335D4C in rfg.exe's reset
static const float MIN_CAMERA_VEH_SPAWN_DISTANCE = 100.0f;
static const float MAX_PED_SPAWN_DISTANCE = 120.0f; // 0x016B6B1C
static const float MAX_VEH_SPAWN_DISTANCE = 260.0f; // 0x016B6B20
static const float MAX_CAMERA_PED_SPAWN_DISTANCE = 160.0f; // 0x016B6B24
static const float MAX_CAMERA_VEH_SPAWN_DISTANCE = 200.0f; // 0x016B6B28
static const float MIN_PED_SPAWN_DISTANCE_SQRD = 400.0f; // 0x016B6B0C
static const float MIN_VEH_SPAWN_DISTANCE_SQRD = 3600.0f; // 0x016B6B10
static const float MAX_PED_SPAWN_DISTANCE_SQRD = MAX_PED_SPAWN_DISTANCE * MAX_PED_SPAWN_DISTANCE; // 0x0374831C
static const float MAX_VEH_SPAWN_DISTANCE_SQRD = MAX_VEH_SPAWN_DISTANCE * MAX_VEH_SPAWN_DISTANCE; // 0x037457CC
static const float MAX_CAMERA_PED_SPAWN_DISTANCE_SQRD = MAX_CAMERA_PED_SPAWN_DISTANCE * MAX_CAMERA_PED_SPAWN_DISTANCE; // 0x0374A4E0
static const float MAX_CAMERA_VEH_SPAWN_DISTANCE_SQRD = MAX_CAMERA_VEH_SPAWN_DISTANCE * MAX_CAMERA_VEH_SPAWN_DISTANCE; // 0x037449EC
static const float MIN_AMBIENT_VEHICLE_DESPAWN_DISTANCE_SQRD = 1600.0f; // 0x016B6B00
static const float MAX_AMBIENT_VEHICLE_DESPAWN_DISTANCE_SQRD = 25600.0f; // 0x016B6B04
static const float MAX_ARRIVAL_DISTANCE_SQRD = 40000.0f; // 0x016B6B08
static const vector NEAR_PLAYER_COLLECTION_OFFSET(100.0f, 500.0f, 100.0f); // 0x016B6AF4
static const float AMBIENT_VEHICLE_SPEED = 9.0f; // 0x01335CE0
// Magic number: a node further than this along the network is not a spawn point, and is left out of the cost table.
static const float MAX_NODE_TRAVEL_DISTANCE = 200.0f;
// Magic numbers: 45 degrees, 0.5 degrees and 50 degrees, as the doubles rfg.exe loads for its cos calls (0x012CEB58,
// 0x01336108, 0x01317C50).
static const double ANGLE_45_DEGREES = 0.785385012626648;
static const double ANGLE_HALF_DEGREE = 0.00872649997472763;
static const double ANGLE_50_DEGREES = 0.8726500272750854;
static const float FRONT_OF_CAMERA_DOT = (float)cos(ANGLE_45_DEGREES); // 0x0374838C
static const float IN_TRAVEL_DIRECTION_DOT = (float)cos(ANGLE_45_DEGREES); // 0x037449E8
static const float MIN_TURINING_DOT = (float)cos(ANGLE_HALF_DEGREE); // 0x0374BF24
static const float MOVE_DIRECTION_DOT = (float)cos(ANGLE_45_DEGREES); // 0x0374BF20
static const float FRONT_SIDE_DOT = (float)cos(ANGLE_50_DEGREES); // 0x0374BF2C
static const float SPAWN_DIRECTION_DOT = (float)cos(ANGLE_45_DEGREES); // 0x0374BF78
// Original bug: rfg.exe only reads this one (0x00B4ED2A), nothing writes it, so it is 0: 90 degrees, not 45. The MIN_DOT that
// can_spawn_at_navp computes (0x0374BF3C) is a different static that nothing reads.
static const float MIN_DOT = 0.0f; // 0x0374BF80
// By alert level, then by density.
static const int MAX_AMBIENT_NPC_COUNT[NUM_ALERT_LEVELS][NUM_AMB_SPAWN_DENSITY] = { // 0x01335CF8
    {0, 10, 20, 30},
    {0, 8, 10, 20},
    {0, 6, 8, 10},
    {0, 6, 6, 8},
};
static const int MAX_AMBIENT_VEHICLE_COUNT[NUM_ALERT_LEVELS][NUM_AMB_SPAWN_DENSITY] = { // 0x013360C8
    {0, 4, 6, 10},
    {0, 4, 6, 8},
    {0, 2, 3, 4},
    {0, 1, 2, 3},
};

// ---- globals
static player_bounding_region Player_spawn_region; // 0x037482A0
static node_list_cache Cached_node_list; // 0x037443A8
static object_pool<ped_node_network, 128, 1> Ped_node_network_pool; // 0x03748C10
static farray<ambient_no_spawn_zone, 128> Ambient_no_spawn_list; // 0x016B6B70
static farray<human_info const*, 165> Old_guerrilla_list; // 0x016B79D0
static farray<vehicle_info const*, 163> Old_vehicle_list; // 0x016B7E88
static farray<unsigned int, 8> Last_road_spawn_list; // 0x016B6B2C
static const human_spawn_group_info* Last_guerrilla_spawn_group; // 0x03742460
static const vehicle_spawn_group_info* Last_vehicle_spawn_group; // 0x03742464
static int Last_civilian_spawned; // 0x0374245C
static int Next_rotation_group; // 0x03742468
static timestamp Npc_despawn_period; // 0x016B6B5C
static timestamp Npc_spawn_period; // 0x016B6B60
static timestamp Vehicle_spawn_period; // 0x016B6B64
static timestamp Guerrilla_update_period; // 0x016B6B68
static timestamp Character_delete_period; // 0x016B6B6C
static bool Disable_ambient_vehicle_spawn; // 0x0374246C
static bool do_npc_spawning_this_frame = true; // 0x016B98EC
static gfm_alert_levels Amb_ped_alert_level_lock = ALERT_LEVEL_INVALID; // 0x016B6980
static ambient_spawn_density Amb_ped_density_level_lock = AMB_SPAWN_DENSITY_INVALID; // 0x016B6984
static gfm_alert_levels Amb_veh_alert_level_lock = ALERT_LEVEL_INVALID; // 0x016B6988
static ambient_spawn_density Amb_veh_density_level_lock = AMB_SPAWN_DENSITY_INVALID; // 0x016B698C

// ---- constructors
// 0x00B41020 (:402)
ambient_spawn_camera_info::ambient_spawn_camera_info()
{
    camera_pos.zero();
    camera_orient_xz = IDENTITY_MATRIX;
    memset(&camera_movement_props, 0, sizeof(camera_movement_props));
}

// 0x00B4C490 (:435)
ambient_ped_spawn_info::ambient_ped_spawn_info()
{
    pp = nullptr;
    amb_spawn_info = nullptr;
    spawn_direction.zero();
    despawn_direction.zero();
    npcs_in_spawn_direction = 0;
    nearest_navp = nullptr;
}

// 0x00B4E570
ambient_ped_spawn_info::~ambient_ped_spawn_info()
{
}

// 0x00B4C5A0 (:479)
road_spawn_info::road_spawn_info()
{
    reset();
}

// 0x00B470B0 (:484)
void road_spawn_info::reset()
{
    road = nullptr;
    index_list.clear();
}

// 0x00B451E0
road_spawn_info::~road_spawn_info()
{
}

// 0x00B55020 (:509)
ambient_veh_spawn_info::ambient_veh_spawn_info()
{
    reset();
}

// 0x00B470C0 (:513)
void ambient_veh_spawn_info::reset()
{
    camera_pos.zero();
    camera_orient.make_identity();
    pp = nullptr;
    nearest_road = nullptr;
    nearest_pos_on_road.zero();
    move_direction.zero();
    dist_on_road = 0.0f;
    move_speed = 0.0f;
    ambient_vehicle_list.clear();
    vehicle_density = 0;
    vehicles_in_region = 0;
    road_spawn_list.clear();
    quadrant_count[0] = 0;
    quadrant_count[1] = 0;
    quadrant_count[2] = 0;
    quadrant_count[3] = 0;
}

// 0x00B4E530
ambient_veh_spawn_info::~ambient_veh_spawn_info()
{
}

// 0x00B4C5C0 (:594)
node_list_cache::node_list_cache() : node_to_cost_hash(false)
{
    reset();
}

// 0x00B47150
node_list_cache::~node_list_cache()
{
}

// 0x00B47070 (:306)
player_bounding_region::player_bounding_region()
{
    reset();
}

// 0x00B47080 (:369)
ped_node_network::ped_node_network()
{
    reset();
}

// ---- small helpers
// 0x00B47160 (:769)
// Magic number: 90 NPCs is the top of what the despawning lets stand.
static bool near_npc_population_cap()
{
    return World.get_num_objects<npc>() > 90;
}

// 0x00B47170 (:777)
// Magic number: 28 automobiles.
static bool near_auto_population_cap()
{
    return World.get_num_objects<automobile>() > 28;
}

// 0x00B41080 (:1282)
static bool is_npc_despawning(const npc* np)
{
    return np->get_ai()->is_despawning();
}

// 0x00B419A0 (:2549)
static bool is_vehicle_despawning(const vehicle* vp)
{
    return vp->m_flags.m_despawn_after_fade != 0;
}

// 0x00B419B0 (:2556)
// LTCG: vp in ESI.
static void destroy_ambient_vehicle(vehicle* vp)
{
    if (is_vehicle_despawning(vp))
    {
        return;
    }
    if (vp->is_offscreen())
    {
        World.destroy_object(vp, false);
    }
    else
    {
        // Magic number: a 2 second fade.
        vehicle_fade_out(vp, 2000, true, false);
    }
}

// 0x00B419F0 (:2575)
// LTCG: np in ESI.
static void start_npc_despawn(npc* np)
{
    if (!is_npc_despawning(np))
    {
        np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
    }
}

// 0x00B410A0 (:1393)
// LTCG: pos in ESI. Magic number: where there is no terrain it counts as very far above it.
static float distance_above_ground(const vector& pos)
{
    float height;
    if (!havok_terrain_height(height, pos.x, pos.z, false, nullptr))
    {
        return 1e+37f;
    }
    return pos.y - height;
}

// 0x00B41110 (:1620)
// LTCG: amb_spawn_info in EDI.
static int max_vehicle_count(const ambient_spawn_info* amb_spawn_info)
{
    if (mission_is_active() && (unsigned int)Amb_veh_alert_level_lock < NUM_ALERT_LEVELS && (unsigned int)Amb_veh_density_level_lock < NUM_AMB_SPAWN_DENSITY)
    {
        // Original bug: a mission's vehicle lock reads the NPC table (0x00B41131).
        return MAX_AMBIENT_NPC_COUNT[Amb_veh_alert_level_lock][Amb_veh_density_level_lock];
    }
    const gfm_alert_levels alert = gsm_get_alert_level();
    if ((unsigned int)alert >= NUM_ALERT_LEVELS)
    {
        return 0;
    }
    const ambient_spawn_density density = game_clock_get_time_of_day() == GC_NIGHT ? amb_spawn_info->night_veh_density : amb_spawn_info->veh_density;
    return (unsigned int)density < NUM_AMB_SPAWN_DENSITY ? MAX_AMBIENT_VEHICLE_COUNT[alert][density] : 0;
}

// 0x00B41180 (:1671)
// LTCG: amb_spawn_info in EDI.
static int max_ped_count(const ambient_spawn_info* amb_spawn_info)
{
    if (mission_is_active() && (unsigned int)Amb_ped_alert_level_lock < NUM_ALERT_LEVELS && (unsigned int)Amb_ped_density_level_lock < NUM_AMB_SPAWN_DENSITY)
    {
        return MAX_AMBIENT_NPC_COUNT[Amb_ped_alert_level_lock][Amb_ped_density_level_lock];
    }
    const gfm_alert_levels alert = gsm_get_alert_level();
    if ((unsigned int)alert >= NUM_ALERT_LEVELS)
    {
        return 0;
    }
    const ambient_spawn_density density = game_clock_get_time_of_day() == GC_NIGHT ? amb_spawn_info->night_ped_density : amb_spawn_info->ped_density;
    return (unsigned int)density < NUM_AMB_SPAWN_DENSITY ? MAX_AMBIENT_NPC_COUNT[alert][density] : 0;
}

// 0x00B47450 (:1714)
// Magic number: two short of the most, and not below none.
static int near_max_ped_count(const ambient_spawn_info* amb_spawn_info)
{
    const int count = max_ped_count(amb_spawn_info) - 2;
    return count < 0 ? 0 : count;
}

// 0x00B47470 (:1723)
// A quarter of the most, rounded up, and at least one.
static int max_ped_count_quad(const ambient_spawn_info* amb_spawn_info)
{
    const float quarter = (float)max_ped_count(amb_spawn_info) * 0.25f;
    const int truncated = (int)quarter;
    const int count = (float)truncated == quarter ? truncated : truncated + 1;
    return count < 1 ? 1 : count;
}

// 0x00B411F0 (:1744)
// LTCG: camera_info in ESI.
static void build_camera_props(ambient_spawn_camera_info& camera_info)
{
    if (camera_info.camera_movement_props.props_built)
    {
        return;
    }
    camera_info.camera_movement_props.props_built = true;
    camera_info.camera_pos = Rfg_camera.real_pos;
    vector ideal_fvec(Rfg_camera.ideal_orient.fvec.x, 0.0f, Rfg_camera.ideal_orient.fvec.z);
    if (ideal_fvec.is_null())
    {
        camera_info.camera_orient_xz = IDENTITY_MATRIX;
    }
    else
    {
        camera_info.camera_orient_xz.make(ideal_fvec);
    }
    vector last_fvec(Rfg_camera.last_orient.fvec.x, 0.0f, Rfg_camera.last_orient.fvec.z);
    matrix last_orient_xz;
    if (last_fvec.is_null())
    {
        last_orient_xz = IDENTITY_MATRIX;
    }
    else
    {
        last_orient_xz.make(last_fvec);
    }
    const matrix& orient = camera_info.camera_orient_xz;
    camera_info.camera_movement_props.is_turning = orient.fvec * last_orient_xz.fvec < MIN_TURINING_DOT;
    if (camera_info.camera_movement_props.is_turning)
    {
        camera_info.camera_movement_props.turning_right = 0.0f <= orient.fvec * last_orient_xz.rvec;
        camera_info.camera_movement_props.turning_left = !camera_info.camera_movement_props.turning_right;
    }
    vector move_direction = Rfg_camera.real_pos - Rfg_camera.last_pos;
    // Magic number: 2 cm in a frame.
    camera_info.camera_movement_props.is_moving = 0.02f <= move_direction.mag_and_normalize_safe();
    if (!camera_info.camera_movement_props.is_moving)
    {
        return;
    }
    const float forward = orient.fvec * move_direction;
    const float right = orient.rvec * move_direction;
    if (MOVE_DIRECTION_DOT <= forward)
    {
        camera_info.camera_movement_props.moving_forward = true;
    }
    else if (forward <= -MOVE_DIRECTION_DOT)
    {
        camera_info.camera_movement_props.moving_backward = true;
    }
    if (MOVE_DIRECTION_DOT <= right)
    {
        camera_info.camera_movement_props.moving_right = true;
    }
    else if (right <= -MOVE_DIRECTION_DOT)
    {
        camera_info.camera_movement_props.moving_left = true;
    }
}

// 0x00B41600 (:1830)
// LTCG: spawn_direction in EDI, camera_info in ESI.
static void pick_spawn_direction(vector& spawn_direction, const ambient_spawn_camera_info& camera_info)
{
    const matrix& orient = camera_info.camera_orient_xz;
    if (camera_info.camera_movement_props.is_turning)
    {
        spawn_direction = camera_info.camera_movement_props.turning_right ? orient.rvec : -orient.rvec;
    }
    else if (camera_info.camera_movement_props.is_moving)
    {
        if (camera_info.camera_movement_props.moving_forward)
        {
            spawn_direction = orient.fvec;
        }
        else if (camera_info.camera_movement_props.moving_right)
        {
            spawn_direction = orient.rvec;
        }
        else if (camera_info.camera_movement_props.moving_left)
        {
            spawn_direction = -orient.rvec;
        }
        else if (camera_info.camera_movement_props.moving_backward)
        {
            spawn_direction = -orient.fvec;
        }
    }
    if (!spawn_direction.is_null())
    {
        return;
    }
    const unsigned int random = vrand();
    if (random & 1)
    {
        spawn_direction = (random & 2) ? orient.fvec : -orient.fvec;
    }
    else
    {
        spawn_direction = (random & 2) ? orient.rvec : -orient.rvec;
    }
}

// 0x00B417B0 (:1878)
// LTCG: pos in EAX, origin in ECX, direction_vec in ESI. Flat on the ground.
static bool is_in_direction(const vector& origin, const vector& direction_vec, const vector& pos, float angle_dot)
{
    vector to_pos(pos.x - origin.x, 0.0f, pos.z - origin.z);
    to_pos.normalize_safe();
    return angle_dot <= direction_vec * to_pos;
}

// 0x00B418A0 (:2304)
// LTCG: result in ESI, navp in EDI. Towards one of the node's links.
static vector face_navpoint_link(const navpoint* navp)
{
    if (navp->num_links < 1)
    {
        return FORWARD_VECTOR;
    }
    const int link = navp->num_links == 1 ? 0 : int_rand_range(0, navp->num_links - 1);
    vector direction = navp->links[link]->pos - navp->pos;
    direction.flatten_normalize_safe();
    return direction;
}

// 0x00B41A10 (:3295)
// LTCG: direction_vec in ESI, spawn_info in EDI.
static quadrant_locations get_quadrant_id(const vector& pos, const ambient_veh_spawn_info& spawn_info, vector& direction_vec, float& distance)
{
    direction_vec = pos - spawn_info.camera_pos;
    distance = direction_vec.mag_and_normalize_safe();
    if (direction_vec.is_null())
    {
        return QUADRANT_LOC_FRONT;
    }
    const float forward = spawn_info.camera_orient.fvec * direction_vec;
    if (FRONT_SIDE_DOT <= forward)
    {
        return QUADRANT_LOC_FRONT;
    }
    if (forward <= -FRONT_SIDE_DOT)
    {
        return QUADRANT_LOC_REAR;
    }
    return spawn_info.camera_orient.rvec * direction_vec < 0.0f ? QUADRANT_LOC_LEFT_SIDE : QUADRANT_LOC_RIGHT_SIDE;
}

// 0x00B41BB0 (:3621)
// Magic number: from alert level 2 on.
static bool asm_restrict_passenger_spawning()
{
    return (int)gsm_get_alert_level() >= 2 || mission_is_active() || !activity_none();
}

// ---- player_bounding_region
// 0x00B41BF0 (:4318)
// Magic numbers: 20 m from the camera, and 60 m in front of it.
// Original: the last test compares a squared distance with the squared maximum, not the minimum, so behind the camera and
// off the player's way everything out to 120 m counts as inside the minimum.
bool player_bounding_region::inside_min_radius_npc(const vector& point) const
{
    if (!World.is_zone_streamed(point))
    {
        return false;
    }
    const float dist_sqrd = vec_dist_squared(point, pp_pos);
    if (dist_sqrd < MIN_PED_SPAWN_DISTANCE_SQRD)
    {
        return true;
    }
    vector to_camera = point - camera_pos;
    const float camera_dist = to_camera.mag_and_normalize_safe();
    if (camera_dist < MIN_PED_SPAWN_DISTANCE)
    {
        return true;
    }
    const bool in_front = FRONT_OF_CAMERA_DOT < camera_orient.fvec * to_camera;
    if (in_front && min_velocity_ped_spawn_distance < MIN_CAMERA_PED_SPAWN_DISTANCE)
    {
        return camera_dist < MIN_CAMERA_PED_SPAWN_DISTANCE;
    }
    if (!ignore_move_direction)
    {
        vector to_player = point - pp_pos;
        const float player_dist = to_player.mag_and_normalize();
        if (to_player.is_null())
        {
            return false;
        }
        if (IN_TRAVEL_DIRECTION_DOT < to_player * pp_move_direction)
        {
            return player_dist < min_velocity_ped_spawn_distance;
        }
    }
    return in_front ? camera_dist < MIN_CAMERA_PED_SPAWN_DISTANCE : dist_sqrd < MAX_PED_SPAWN_DISTANCE_SQRD;
}

// 0x00B41E70 (:4380)
// Original: as inside_min_radius_npc, the last test against the squared maximum.
bool player_bounding_region::inside_min_radius_vehicle(const vector& point) const
{
    if (!World.is_zone_streamed(point))
    {
        return false;
    }
    const float dist_sqrd = vec_dist_squared(point, pp_pos);
    if (dist_sqrd < MIN_VEH_SPAWN_DISTANCE_SQRD)
    {
        return true;
    }
    vector to_camera = point - camera_pos;
    const float camera_dist = to_camera.mag_and_normalize_safe();
    if (camera_dist < MIN_VEH_SPAWN_DISTANCE)
    {
        return true;
    }
    const bool in_front = FRONT_OF_CAMERA_DOT < camera_orient.fvec * to_camera;
    if (in_front && min_velocity_veh_spawn_distance < MIN_CAMERA_VEH_SPAWN_DISTANCE)
    {
        return camera_dist < MIN_CAMERA_VEH_SPAWN_DISTANCE;
    }
    if (!ignore_move_direction)
    {
        vector to_player = point - pp_pos;
        const float player_dist = to_player.mag_and_normalize();
        if (to_player.is_null())
        {
            return false;
        }
        if (IN_TRAVEL_DIRECTION_DOT < to_player * pp_move_direction)
        {
            return player_dist < min_velocity_veh_spawn_distance;
        }
    }
    return in_front ? camera_dist < MIN_CAMERA_VEH_SPAWN_DISTANCE : dist_sqrd < MAX_VEH_SPAWN_DISTANCE_SQRD;
}

// 0x00B420F0 (:4443)
bool player_bounding_region::outside_max_radius_npc(const vector& point) const
{
    if (!World.is_zone_streamed(point))
    {
        return true;
    }
    const float dist_sqrd = vec_dist_squared(point, pp_pos);
    if (farthest_ped_max_sqrd < dist_sqrd)
    {
        return true;
    }
    vector to_camera = point - camera_pos;
    const float camera_dist = to_camera.mag_and_normalize_safe();
    if (camera_dist < MIN_PED_SPAWN_DISTANCE)
    {
        return false;
    }
    const bool in_front = FRONT_OF_CAMERA_DOT < camera_orient.fvec * to_camera;
    if (in_front && min_velocity_ped_spawn_distance < MIN_CAMERA_PED_SPAWN_DISTANCE)
    {
        return MAX_CAMERA_PED_SPAWN_DISTANCE < camera_dist;
    }
    if (!ignore_move_direction)
    {
        vector to_player = point - pp_pos;
        const float player_dist = to_player.mag_and_normalize();
        if (to_player.is_null())
        {
            return false;
        }
        if (IN_TRAVEL_DIRECTION_DOT < to_player * pp_move_direction)
        {
            return max_velocity_ped_spawn_distance < player_dist;
        }
    }
    return in_front ? MAX_CAMERA_PED_SPAWN_DISTANCE < camera_dist : MAX_PED_SPAWN_DISTANCE_SQRD < dist_sqrd;
}

// 0x00B42380 (:4505)
bool player_bounding_region::outside_max_radius_vehicle(const vector& point) const
{
    if (!World.is_zone_streamed(point))
    {
        return true;
    }
    const float dist_sqrd = vec_dist_squared(point, pp_pos);
    if (farthest_veh_max_sqrd < dist_sqrd)
    {
        return true;
    }
    vector to_camera = point - camera_pos;
    const float camera_dist = to_camera.mag_and_normalize_safe();
    if (camera_dist < MIN_VEH_SPAWN_DISTANCE)
    {
        return false;
    }
    const bool in_front = FRONT_OF_CAMERA_DOT < camera_orient.fvec * to_camera;
    if (in_front && min_velocity_veh_spawn_distance < MIN_CAMERA_VEH_SPAWN_DISTANCE)
    {
        return MAX_CAMERA_VEH_SPAWN_DISTANCE < camera_dist;
    }
    if (!ignore_move_direction)
    {
        vector to_player = point - pp_pos;
        const float player_dist = to_player.mag_and_normalize();
        if (to_player.is_null())
        {
            return false;
        }
        if (IN_TRAVEL_DIRECTION_DOT < to_player * pp_move_direction)
        {
            return max_velocity_veh_spawn_distance < player_dist;
        }
    }
    return in_front ? MAX_CAMERA_VEH_SPAWN_DISTANCE < camera_dist : MAX_VEH_SPAWN_DISTANCE_SQRD < dist_sqrd;
}

// 0x00B42610 (:4568)
bool player_bounding_region::in_spawn_region_npc(const vector& point) const
{
    if (!World.is_zone_streamed(point))
    {
        return false;
    }
    const float dist_sqrd = vec_dist_squared(point, pp_pos);
    if (dist_sqrd < MIN_PED_SPAWN_DISTANCE_SQRD || farthest_ped_max_sqrd < dist_sqrd)
    {
        return false;
    }
    vector to_camera = point - camera_pos;
    const float camera_dist = to_camera.mag_and_normalize_safe();
    if (camera_dist < MIN_PED_SPAWN_DISTANCE)
    {
        return false;
    }
    const bool in_front = FRONT_OF_CAMERA_DOT < camera_orient.fvec * to_camera;
    if (!in_front || MIN_CAMERA_PED_SPAWN_DISTANCE <= min_velocity_ped_spawn_distance)
    {
        if (!ignore_move_direction)
        {
            vector to_player = point - pp_pos;
            const float player_dist = to_player.mag_and_normalize();
            if (to_player.is_null())
            {
                return false;
            }
            if (IN_TRAVEL_DIRECTION_DOT < to_player * pp_move_direction)
            {
                return min_velocity_ped_spawn_distance < player_dist && player_dist < max_velocity_ped_spawn_distance;
            }
        }
        if (!in_front)
        {
            return dist_sqrd < MAX_PED_SPAWN_DISTANCE_SQRD;
        }
    }
    return MIN_CAMERA_PED_SPAWN_DISTANCE < camera_dist && camera_dist < MAX_CAMERA_PED_SPAWN_DISTANCE;
}

// 0x00B428F0 (:4630)
bool player_bounding_region::in_spawn_region_vehicle(const vector& point) const
{
    if (!World.is_zone_streamed(point))
    {
        return false;
    }
    const float dist_sqrd = vec_dist_squared(point, pp_pos);
    if (dist_sqrd < MIN_VEH_SPAWN_DISTANCE_SQRD || farthest_veh_max_sqrd < dist_sqrd)
    {
        return false;
    }
    vector to_camera = point - camera_pos;
    const float camera_dist = to_camera.mag_and_normalize_safe();
    if (camera_dist < MIN_VEH_SPAWN_DISTANCE)
    {
        return false;
    }
    const bool in_front = FRONT_OF_CAMERA_DOT < camera_orient.fvec * to_camera;
    if (!in_front || MIN_CAMERA_VEH_SPAWN_DISTANCE <= min_velocity_veh_spawn_distance)
    {
        if (!ignore_move_direction)
        {
            vector to_player = point - pp_pos;
            const float player_dist = to_player.mag_and_normalize();
            if (to_player.is_null())
            {
                return false;
            }
            if (IN_TRAVEL_DIRECTION_DOT < to_player * pp_move_direction)
            {
                return min_velocity_veh_spawn_distance < player_dist && player_dist < max_velocity_veh_spawn_distance;
            }
        }
        if (!in_front)
        {
            return dist_sqrd < MAX_VEH_SPAWN_DISTANCE_SQRD;
        }
    }
    return MIN_CAMERA_VEH_SPAWN_DISTANCE < camera_dist && camera_dist < MAX_CAMERA_VEH_SPAWN_DISTANCE;
}

// 0x00B42BD0 (:4693)
const vector& player_bounding_region::get_move_direction() const
{
    return pp_move_direction;
}

// 0x00B42BE0 (:4699)
float player_bounding_region::get_move_speed() const
{
    return pp_move_speed;
}

// 0x00B47B60 (:4705)
// Original: reads the local player unchecked.
void player_bounding_region::update_region()
{
    // Magic numbers: moving at 5 m/s or more, the spawn ring moves ahead of the player, 4 and 8 seconds out, 100 m deep.
    static const float MIN_MOVE_SPEED = 5.0f;
    reset();
    last_framecount = Framecount;
    const player* pp = player_get_local_player();
    if (Rfg_camera.mode != CAMERA_SLEW_MODE)
    {
        camera_pos = Rfg_camera.real_pos;
        camera_orient = Rfg_camera.real_orient;
    }
    pp_pos = pp->pos;
    pp_move_direction = pp->velocity;
    pp_move_speed = pp_move_direction.mag_and_normalize_safe();
    if (MIN_MOVE_SPEED <= pp_move_speed && !pp_move_direction.is_null())
    {
        min_velocity_ped_spawn_distance = pp_move_speed * 4.0f;
        max_velocity_ped_spawn_distance = min_velocity_ped_spawn_distance + 100.0f;
        min_velocity_ped_spawn_distance_sqrd = min_velocity_ped_spawn_distance * min_velocity_ped_spawn_distance;
        max_velocity_ped_spawn_distance_sqrd = max_velocity_ped_spawn_distance * max_velocity_ped_spawn_distance;
        min_velocity_veh_spawn_distance = pp_move_speed * 8.0f;
        max_velocity_veh_spawn_distance = min_velocity_veh_spawn_distance + 100.0f;
        min_velocity_veh_spawn_distance_sqrd = min_velocity_veh_spawn_distance * min_velocity_veh_spawn_distance;
        max_velocity_veh_spawn_distance_sqrd = max_velocity_veh_spawn_distance * max_velocity_veh_spawn_distance;
        ignore_move_direction = false;
        farthest_ped_max_sqrd = max_velocity_ped_spawn_distance_sqrd <= MAX_CAMERA_PED_SPAWN_DISTANCE_SQRD ? MAX_CAMERA_PED_SPAWN_DISTANCE_SQRD : max_velocity_ped_spawn_distance_sqrd;
        farthest_veh_max_sqrd = max_velocity_veh_spawn_distance_sqrd <= MAX_CAMERA_VEH_SPAWN_DISTANCE_SQRD ? MAX_CAMERA_VEH_SPAWN_DISTANCE_SQRD : max_velocity_veh_spawn_distance_sqrd;
        return;
    }
    pp_move_direction = camera_orient.fvec;
    farthest_ped_max_sqrd = MAX_CAMERA_PED_SPAWN_DISTANCE_SQRD;
    farthest_veh_max_sqrd = MAX_CAMERA_VEH_SPAWN_DISTANCE_SQRD;
    ignore_move_direction = true;
}

// 0x00B42BF0 (:4775)
void player_bounding_region::reset()
{
    last_framecount = 0;
    pp_pos.zero();
    pp_move_direction = FORWARD_VECTOR;
    pp_move_speed = 0.0f;
    min_velocity_ped_spawn_distance = MIN_PED_SPAWN_DISTANCE;
    max_velocity_ped_spawn_distance = MAX_PED_SPAWN_DISTANCE;
    min_velocity_ped_spawn_distance_sqrd = MIN_PED_SPAWN_DISTANCE_SQRD;
    max_velocity_ped_spawn_distance_sqrd = MAX_PED_SPAWN_DISTANCE_SQRD;
    min_velocity_veh_spawn_distance = MIN_VEH_SPAWN_DISTANCE;
    max_velocity_veh_spawn_distance = MAX_VEH_SPAWN_DISTANCE;
    min_velocity_veh_spawn_distance_sqrd = MIN_VEH_SPAWN_DISTANCE_SQRD;
    max_velocity_veh_spawn_distance_sqrd = MAX_VEH_SPAWN_DISTANCE_SQRD;
    farthest_ped_max_sqrd = FLT_MAX;
    farthest_veh_max_sqrd = FLT_MAX;
    camera_pos.zero();
    camera_orient.make_identity();
    ignore_move_direction = true;
}

// ---- guerrilla and vehicle rotation
// 0x00B47180 (:1024)
// LTCG: selected_id in EDI.
static const human_info* find_district_guerrilla(int selected_id)
{
    for (int i = 0; i < Desired_guerrilla_list.size(); i++)
    {
        if (Desired_guerrilla_list[i]->home_district == selected_id)
        {
            return Desired_guerrilla_list[i];
        }
    }
    return nullptr;
}

// Inlined in build_vehicle_rotation_list and clean_up_old_vehicle_list: a vehicle whose type no spawn user has reserved.
static bool vehicle_slot_is_rotatable(const vehicle* vp)
{
    const resource_slot_record* record = vp->info != nullptr ? Vehicle_records_list.get_record(vp->info->slot_id) : nullptr;
    return record == nullptr || !record->is_reserved();
}

// 0x00B55060 (:786)
// Original: a vehicle with no info adds a null entry.
static void build_vehicle_rotation_list(base_array<vehicle_info const*>& rotation_list)
{
    rotation_list.clear();
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        const vehicle* vp = World.get_object<vehicle>(i);
        if (vehicle_slot_is_rotatable(vp))
        {
            rotation_list.add_unique(vp->info);
        }
    }
}

// 0x00B55100 (:802)
static void select_current_vehicles(const vehicle_spawn_group_info* current_spawn_group)
{
    if (current_spawn_group == nullptr)
    {
        return;
    }
    Last_vehicle_spawn_group = current_spawn_group;
    if (Old_vehicle_list.size() == 0)
    {
        build_vehicle_rotation_list(Old_vehicle_list);
    }
    Desired_vehicle_list.clear();
    farray<vehicle_info const*, 256> candidates;
    candidates.copy(current_spawn_group->v_info_list);
    while (candidates.size() > 0)
    {
        const int pick = candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1);
        const vehicle_info* v_info = candidates[pick];
        candidates.remove_unsorted(pick);
        if (Old_vehicle_list.is_member(v_info))
        {
            continue;
        }
        Desired_vehicle_list.add_unique(v_info);
        if (Desired_vehicle_list.size() >= Desired_vehicle_list.capacity())
        {
            return;
        }
    }
}

// 0x00B4C650 (:847)
// District 0 is the common guerrillas; the rest are by home district. Each district's current types are topped up from the
// group's types not used last time, and the new list takes two common ones and then one from each district in turn.
static void select_current_guerrillas(const human_spawn_group_info* current_spawn_group)
{
    // Magic number: home districts run below 16.
    static const int NUM_DISTRICTS = 16;
    farray<human_info const*, 16> selected;
    farray<human_info const*, 165> candidates;
    farray<human_info const*, 16> new_by_district[NUM_DISTRICTS];
    farray<human_info const*, 16> desired_by_district[NUM_DISTRICTS];
    Last_guerrilla_spawn_group = current_spawn_group;
    for (int i = 0; i < Desired_guerrilla_list.size(); i++)
    {
        const human_info* info = Desired_guerrilla_list[i];
        if (info->home_district > -1 && info->home_district < NUM_DISTRICTS)
        {
            desired_by_district[info->home_district].add(info);
        }
    }
    for (int i = 0; i < current_spawn_group->h_info_list.size(); i++)
    {
        candidates.add(current_spawn_group->h_info_list[i]);
    }
    // Only the last common guerrilla stays on the old list.
    const human_info* last_common = nullptr;
    for (int i = 0; i < Old_guerrilla_list.size(); i++)
    {
        if (Old_guerrilla_list[i]->home_district == 0)
        {
            last_common = Old_guerrilla_list[i];
        }
    }
    if (last_common != nullptr)
    {
        for (int i = 0; i < Old_guerrilla_list.size();)
        {
            if (Old_guerrilla_list[i]->home_district == 0 && Old_guerrilla_list[i] != last_common)
            {
                Old_guerrilla_list.remove_unsorted(i);
            }
            else
            {
                i++;
            }
        }
    }
    while (candidates.size() > 0)
    {
        const int pick = candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1);
        const human_info* info = candidates[pick];
        candidates.remove_unsorted(pick);
        const int district = info->home_district;
        if (district > -1 && district < NUM_DISTRICTS && new_by_district[district].size() < new_by_district[district].capacity() && !Old_guerrilla_list.is_member(info))
        {
            new_by_district[district].add(info);
        }
    }
    for (int district = 1; district < NUM_DISTRICTS; district++)
    {
        if (new_by_district[district].size() < 1 && desired_by_district[district].size() > 0)
        {
            desired_by_district[district].clear();
        }
    }
    for (int district = 0; district < NUM_DISTRICTS; district++)
    {
        for (int i = 0; i < new_by_district[district].size() && desired_by_district[district].size() < desired_by_district[district].capacity(); i++)
        {
            desired_by_district[district].add_unique(new_by_district[district][i]);
        }
    }
    // Original: one common guerrilla alone is not taken, it takes two.
    if (desired_by_district[0].size() > 1)
    {
        selected.add_unique(desired_by_district[0][0]);
        selected.add_unique(desired_by_district[0][1]);
    }
    for (int round = 0; round < selected.capacity() && selected.size() < selected.capacity(); round++)
    {
        for (int district = 1; district < NUM_DISTRICTS && selected.size() < selected.capacity(); district++)
        {
            if (round < desired_by_district[district].size())
            {
                selected.add_unique(desired_by_district[district][round]);
            }
        }
    }
    for (int i = 0; i < Desired_guerrilla_list.size(); i++)
    {
        const human_info* info = Desired_guerrilla_list[i];
        if (!selected.is_member(info) && !Old_guerrilla_list.is_member(info))
        {
            Old_guerrilla_list.add(info);
        }
    }
    Desired_guerrilla_list.clear();
    for (int i = 0; i < selected.size(); i++)
    {
        Desired_guerrilla_list.add(selected[i]);
    }
}

// 0x00B551E0 (:1111)
static void clean_up_old_vehicle_list()
{
    farray<vehicle_info const*, 163> old_list;
    old_list.copy(Old_vehicle_list);
    Old_vehicle_list.clear();
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        const vehicle* vp = World.get_object<vehicle>(i);
        if (vehicle_slot_is_rotatable(vp) && old_list.is_member(vp->info))
        {
            Old_vehicle_list.add_unique(vp->info);
        }
    }
}

// 0x00B4CD00 (:1136)
// Ambient NPCs of a type no longer desired go, unless they are in a squad.
static void clean_up_old_guerrilla_list()
{
    Old_guerrilla_list.clear();
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->h_info->home_district != -1 && np->npc_flags.ambient_spawn && !Desired_guerrilla_list.is_member(np->h_info))
        {
            Old_guerrilla_list.add_unique(np->h_info);
            if (np->get_ai()->get_squad() == nullptr)
            {
                np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
            }
        }
    }
}

// ---- despawning
// 0x00B471B0 (:1183)
// LTCG: pp in EAX, np in ESI. Magic number: offscreen for more than 5 seconds.
static bool can_delete_despawning_npc(const player* pp, const npc* np)
{
    const float dist_sqrd = vec_dist_squared(pp->pos, np->pos);
    if (np->h_info->flags.vip && mission_is_active() && dist_sqrd < MAX_PED_SPAWN_DISTANCE_SQRD)
    {
        return false;
    }
    if (dist_sqrd < MIN_PED_SPAWN_DISTANCE_SQRD)
    {
        return false;
    }
    return np->get_offscreen_period() > 5000 || Player_spawn_region.outside_max_radius_npc(np->pos);
}

// 0x00B47240 (:1215)
// LTCG: pp in EAX, np in ESI. Magic number: a second's fade.
static void do_frame_delete_on_foot(const player* pp, npc* np)
{
    if (!can_delete_despawning_npc(pp, np))
    {
        return;
    }
    if (np->is_offscreen())
    {
        World.destroy_object(np, false);
    }
    else
    {
        np->fade_out_and_despawn(1000, false);
    }
}

// 0x00B47280 (:1227)
// LTCG: pp in EAX, vp in ECX.
static bool can_delete_despawning_vehicle(const player* pp, const npc* np, const vehicle* vp)
{
    const float dist_sqrd = vec_dist_squared(vp->pos, pp->pos);
    if (np->h_info->flags.vip && mission_is_active() && dist_sqrd < MAX_VEH_SPAWN_DISTANCE_SQRD)
    {
        return false;
    }
    if (dist_sqrd < MIN_VEH_SPAWN_DISTANCE_SQRD)
    {
        return false;
    }
    return np->get_offscreen_period() > 5000 || Player_spawn_region.outside_max_radius_vehicle(np->pos);
}

// 0x00B47310 (:1258)
// LTCG: pp in EAX, np in EDI, vp in ESI.
static void do_frame_delete_in_vehicle(const player* pp, npc* np, vehicle* vp)
{
    if (!can_delete_despawning_vehicle(pp, np, vp))
    {
        return;
    }
    const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (vp->is_offscreen())
    {
        if (np == driver || vehicle_get_num_passengers(vp) < 2)
        {
            World.destroy_object(vp, false);
        }
        World.destroy_object(np, false);
        return;
    }
    if (np == driver)
    {
        vehicle_fade_out(vp, 1000, true, false);
    }
    np->fade_out_and_despawn(1000, false);
}

// 0x00B4CDD0 (:1296)
static void do_frame_despawn_delete()
{
    const player* pp = player_get_local_player();
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!np->should_despawn() || is_npc_despawning(np))
        {
            continue;
        }
        vehicle* vp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(np->vehicle_handle) : nullptr;
        if (vp != nullptr)
        {
            do_frame_delete_in_vehicle(pp, np, vp);
        }
        else
        {
            do_frame_delete_on_foot(pp, np);
        }
    }
}

// 0x00B47700 (:2587)
// LTCG: pp in EAX, np in ESI. Magic numbers: alive 10 seconds and offscreen 5 before anything, gone regardless after 20.
static bool should_despawn_npc(const player* pp, const npc* np)
{
    if (vec_dist_squared(np->pos, pp->pos) < MIN_PED_SPAWN_DISTANCE_SQRD)
    {
        return false;
    }
    const int lifetime = np->get_lifetime();
    if (lifetime < 10000 || np->get_offscreen_period() < 5000)
    {
        return false;
    }
    return lifetime >= 20000 || Player_spawn_region.outside_max_radius_npc(np->pos);
}

// 0x00B47770 (:2619)
// LTCG: pp in EAX, np in ESI.
static void do_frame_despawn_npc_ped(const player* pp, npc* np)
{
    if (should_despawn_npc(pp, np))
    {
        start_npc_despawn(np);
    }
}

// 0x00B477A0 (:2627)
// LTCG: pp in EAX, vp in ECX.
static bool should_despawn_vehicle(const player* pp, const npc* np, const vehicle* vp)
{
    if (vec_dist_squared(vp->pos, pp->pos) < MIN_VEH_SPAWN_DISTANCE_SQRD)
    {
        return false;
    }
    const int lifetime = np->get_lifetime();
    if (lifetime < 10000 || np->get_offscreen_period() < 5000)
    {
        return false;
    }
    return lifetime >= 20000 || Player_spawn_region.outside_max_radius_vehicle(vp->pos);
}

// 0x00B47820 (:2659)
static void do_frame_despawn_npc_vehicle(const player* pp, npc* np, const vehicle* vp)
{
    if (should_despawn_vehicle(pp, np, vp))
    {
        start_npc_despawn(np);
    }
}

// 0x00B4D650 (:2668)
static void do_frame_despawn_npcs()
{
    const player* pp = player_get_local_player();
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!np->npc_flags.ambient_spawn || is_npc_despawning(np))
        {
            continue;
        }
        if (np->is_killed())
        {
            np->npc_flags.ambient_spawn = 0;
            continue;
        }
        const vehicle* vp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(np->vehicle_handle) : nullptr;
        if (vp != nullptr)
        {
            do_frame_despawn_npc_vehicle(pp, np, vp);
        }
        else
        {
            do_frame_despawn_npc_ped(pp, np);
        }
    }
}

// 0x00B4D730 (:2708)
// Automobiles left behind by squads, once there are 24 or more.
static void do_frame_despawn_squad_auto()
{
    if (World.get_num_objects<automobile>() < 24)
    {
        return;
    }
    const player* pp = player_get_local_player();
    const int count = World.get_num_objects<automobile>();
    for (int i = 0; i < count; i++)
    {
        automobile* vp = World.get_object<automobile>(i);
        // Original: tests again that the handle names no squad, which the first test has settled (dropped here).
        if (vp->squad_handle != 0xFFFFFFFF || vp->created_from_managed_layer() || is_vehicle_despawning(vp) || pp->last_vehicle_driven == vp->get_handle() || vehicle_get_num_passengers(vp) > 0)
        {
            continue;
        }
        const float dist_sqrd = vec_dist_squared(vp->pos, pp->pos);
        if ((vp->m_flags.despawned_squad_vehicle || MIN_AMBIENT_VEHICLE_DESPAWN_DISTANCE_SQRD < dist_sqrd) && (MAX_AMBIENT_VEHICLE_DESPAWN_DISTANCE_SQRD < dist_sqrd || vp->is_offscreen()))
        {
            destroy_ambient_vehicle(vp);
        }
    }
}

// 0x00B4D880 (:2791)
static void do_frame_despawn_vehicle()
{
    const player* pp = player_get_local_player();
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        if (is_vehicle_despawning(vp) || !vp->m_flags.ambient_spawn || vehicle_should_avoid_despawning(vp) || pp->last_vehicle_driven == vp->get_handle() || vp->is_killed() || vehicle_get_num_passengers(vp) > 0)
        {
            continue;
        }
        if (MAX_AMBIENT_VEHICLE_DESPAWN_DISTANCE_SQRD < vec_dist_squared(vp->pos, pp->pos))
        {
            destroy_ambient_vehicle(vp);
        }
    }
}

// 0x00B4D950 (:2836)
automobile* find_farthest_offscreen_auto()
{
    automobile* farthest = nullptr;
    float farthest_sqrd = 0.0f;
    const player* pp = player_get_local_player();
    const int count = World.get_num_objects<automobile>();
    for (int i = 0; i < count; i++)
    {
        automobile* vp = World.get_object<automobile>(i);
        if (vehicle_get_num_passengers(vp) > 0)
        {
            continue;
        }
        const float dist_sqrd = vec_dist_squared(vp->pos, pp->pos);
        if (farthest_sqrd < dist_sqrd && vp->is_offscreen())
        {
            farthest = vp;
            farthest_sqrd = dist_sqrd;
        }
    }
    return farthest;
}

// 0x00B4DA00 (:2868)
static int delete_vehicle_passengers(bool remove_on_screen)
{
    const player* pp = player_get_local_player();
    int num_deleted = 0;
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        vehicle* vp = World.get_object<vehicle>(i);
        const unsigned int handle = vp->get_handle();
        if (handle == pp->vehicle_handle || handle == pp->last_vehicle_driven || !vp->m_flags.ambient_spawn || vehicle_get_num_passengers(vp) < 2 || (!remove_on_screen && !vp->is_offscreen()))
        {
            continue;
        }
        const bool offscreen = vp->is_offscreen();
        // Magic number: a vehicle's eight interaction slots (rfg.exe steps 0x68 bytes up to 0x340).
        for (int seat = 0; seat < 8; seat++)
        {
            const unsigned int human_handle = vp->vii.humans[seat].handle;
            npc* np = human_handle != 0xFFFFFFFF ? World.object_from_handle<npc>(human_handle) : nullptr;
            if (np == nullptr || !np->npc_flags.ambient_spawn || vehicle_passenger_get_seat(vp, np) == VSI_DRIVER)
            {
                continue;
            }
            if (offscreen)
            {
                World.destroy_object(np, false);
            }
            else
            {
                np->fade_out_and_despawn(1000, false);
            }
            num_deleted++;
        }
    }
    return num_deleted;
}

// 0x00B4DB70 (:2922)
static int delete_dead_npcs(const float MIN_DELETE_DISTANCE_SQRD, const float MIN_FADE_DISTANCE_SQRD)
{
    const player* pp = player_get_local_player();
    int num_deleted = 0;
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (!np->is_killed())
        {
            continue;
        }
        const float dist_sqrd = vec_dist_squared(pp->pos, np->pos);
        if (dist_sqrd < MIN_DELETE_DISTANCE_SQRD)
        {
            continue;
        }
        if (np->is_offscreen())
        {
            World.destroy_object(np, false);
            num_deleted++;
        }
        else if (MIN_FADE_DISTANCE_SQRD < dist_sqrd)
        {
            np->fade_out_and_despawn(1000, false);
            num_deleted++;
        }
    }
    return num_deleted;
}

// 0x00B4DC50 (:2965)
// Magic numbers: 75 and 100 m, squared.
static void do_frame_despawn_npc_high()
{
    if (delete_dead_npcs(5625.0f, 10000.0f) < 1)
    {
        delete_vehicle_passengers(false);
    }
}

// 0x00B4DC90 (:2979)
// Magic numbers: 75 m, squared, and 75 NPCs.
static void do_frame_despawn_npc_critical()
{
    int num_deleted = delete_dead_npcs(0.0f, 5625.0f);
    if (World.get_num_objects<npc>() - num_deleted < 76)
    {
        return;
    }
    num_deleted += delete_vehicle_passengers(true);
    if (World.get_num_objects<npc>() - num_deleted < 76)
    {
        return;
    }
    const player* pp = player_get_local_player();
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        // Original: an NPC on foot has the same handle (none) as a player on foot, so with the player on foot only NPCs in
        // vehicles go here.
        if (np->h_info->flags.vip || pp->vehicle_handle == np->vehicle_handle || pp->last_vehicle_driven == np->vehicle_handle || np->get_ai()->get_squad() != nullptr)
        {
            continue;
        }
        vehicle* vp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(np->vehicle_handle) : nullptr;
        const bool offscreen = vp != nullptr ? vp->is_offscreen() : np->is_offscreen();
        if (!offscreen)
        {
            continue;
        }
        World.destroy_object(np, false);
        // Original: asks for the seat of the NPC just destroyed.
        if (vp != nullptr && vehicle_passenger_get_seat(vp, np) == VSI_DRIVER)
        {
            World.destroy_object(vp, false);
        }
    }
}

// 0x00B4DDE0 (:3047)
void do_frame_despawn_auto_aggressive()
{
    const int count = World.get_num_objects<automobile>();
    for (int i = 0; i < count; i++)
    {
        automobile* vp = World.get_object<automobile>(i);
        if (is_vehicle_despawning(vp) && vp->is_offscreen())
        {
            World.destroy_object(vp, false);
            return;
        }
    }
    automobile* farthest = find_farthest_offscreen_auto();
    if (farthest != nullptr)
    {
        World.destroy_object(farthest, false);
    }
}

// 0x00B4DE70 (:3071)
// Magic numbers: 30 automobiles; 90 NPCs is critical, 75 is high; a quarter to half a second.
static void do_frame_despawn()
{
    if (Npc_despawn_period.elapsed())
    {
        Npc_despawn_period.set_rand(250, 500);
        do_frame_despawn_npcs();
        do_frame_despawn_vehicle();
        do_frame_despawn_squad_auto();
    }
    if (World.get_num_objects<automobile>() >= 30)
    {
        do_frame_despawn_auto_aggressive();
    }
    if (World.get_num_objects<npc>() >= 90)
    {
        do_frame_despawn_delete();
        do_frame_despawn_npc_critical();
    }
    else if (World.get_num_objects<npc>() >= 75)
    {
        do_frame_despawn_delete();
        do_frame_despawn_npc_high();
    }
}

// ---- no spawn zones: road segments off the ground, like bridges
// 0x00B47390 (:1474)
// LTCG: road_handle in ESI.
static bool is_on_no_spawn_list(unsigned int road_handle)
{
    for (int i = 0; i < Ambient_no_spawn_list.size(); i++)
    {
        if (Ambient_no_spawn_list[i].handle == road_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00B473C0 (:1571)
// LTCG: road_handle in ESI, road_dist in XMM0.
static bool inside_no_spawn_zone(unsigned int road_handle, float road_dist)
{
    for (int i = 0; i < Ambient_no_spawn_list.size(); i++)
    {
        const ambient_no_spawn_zone& zone = Ambient_no_spawn_list[i];
        if (zone.handle == road_handle && (float)zone.segment_start <= road_dist && road_dist <= (float)zone.segment_end)
        {
            return true;
        }
    }
    return false;
}

// 0x00B47420 (:1594)
// LTCG: road_handle in EDI, road_index in ESI.
static bool inside_no_spawn_zone(unsigned int road_handle, int road_index)
{
    for (int i = 0; i < Ambient_no_spawn_list.size(); i++)
    {
        const ambient_no_spawn_zone& zone = Ambient_no_spawn_list[i];
        if (zone.handle == road_handle && zone.segment_start <= road_index && road_index <= zone.segment_end)
        {
            return true;
        }
    }
    return false;
}

// 0x00B47D80 (:5857)
bool amb_spawn_in_no_spawn(unsigned int road_handle, float road_dist)
{
    return inside_no_spawn_zone(road_handle, road_dist);
}

// 0x00B4CEB0 (:1406)
// Magic numbers: samples no more than 4 m apart, each tested for being over 1 m above the terrain.
static bool segment_far_above_ground(const object_path_road* road, int segment_start, int segment_end, const int num_search_segments)
{
    const float step = 1.0f / (float)num_search_segments;
    const float start = (float)segment_start;
    vector first_step;
    road->get_point(first_step, start + step);
    if (4.0f < vec_dist(road->spline_data.points[segment_start].pos, first_step))
    {
        return segment_far_above_ground(road, segment_start, segment_end, num_search_segments * 2);
    }
    for (int i = 0; i < num_search_segments; i++)
    {
        vector sample;
        road->get_point(sample, (float)i * step + start);
        if (1.0f < distance_above_ground(sample))
        {
            return true;
        }
    }
    return false;
}

// 0x00B4D040 (:1444)
// LTCG: road in ESI, segment_end in EDI.
static void generate_center_and_radius(vector& center_pos, float& best_radius_sqrd, const object_path_road* road, int segment_start, int segment_end)
{
    road->get_point(center_pos, (float)(segment_start + segment_end) * 0.5f);
    const float start_width = road->get_width((float)segment_start);
    const float end_width = road->get_width((float)segment_end);
    const float start_radius = vec_dist(center_pos, road->spline_data.points[segment_start].pos) + start_width;
    const float end_radius = vec_dist(center_pos, road->spline_data.points[segment_end].pos) + end_width;
    best_radius_sqrd = end_radius < start_radius ? start_radius * start_radius : end_radius * end_radius;
}

// 0x00B4D180 (:1489)
static void update_no_spawn_zones(unsigned short zone_xy)
{
    for (int i = 0; i < Ambient_no_spawn_list.size();)
    {
        if (World.is_zone_streamed((unsigned short)(Ambient_no_spawn_list[i].handle >> 16)))
        {
            i++;
        }
        else
        {
            Ambient_no_spawn_list.remove_unsorted(i);
        }
    }
    const int count = World.get_num_objects<object_path_road>();
    for (int i = 0; i < count; i++)
    {
        object_path_road* road = World.get_object<object_path_road>(i);
        const unsigned int handle = road->get_handle();
        if ((unsigned short)(handle >> 24) != zone_xy || is_on_no_spawn_list(handle))
        {
            continue;
        }
        for (int segment = 0; segment < road->spline_data.num_points - 1; segment++)
        {
            // Magic number: starts with two samples per segment.
            if (!segment_far_above_ground(road, segment, segment + 1, 2))
            {
                continue;
            }
            ambient_no_spawn_zone zone;
            zone.handle = handle;
            zone.segment_start = segment;
            zone.segment_end = segment + 1;
            generate_center_and_radius(zone.center_pos, zone.radius_sqrd, road, segment, segment + 1);
            if (Ambient_no_spawn_list.size() >= Ambient_no_spawn_list.capacity())
            {
                return;
            }
            Ambient_no_spawn_list.add(zone);
        }
    }
}

// ---- ped node networks
// 0x00B42CB0 (:4803)
void ped_node_network::reset()
{
    region.min.zero();
    region.max.zero();
    head_node = 0xFFFFFFFF;
    head_node_pos.zero();
    zone_xy = -1;
}

// 0x00B4E8A0 (:4814)
void ped_node_network::visit_node(unsigned int session_id, navpoint* navp, farray<navpoint*, 512>& node_list, bool skip_parented_nodes)
{
    for (int i = 0; i < navp->num_links; i++)
    {
        navpoint* link = navp->links[i];
        if (link->heap_data.session_id == session_id)
        {
            continue;
        }
        link->heap_data.session_id = session_id;
        if (!skip_parented_nodes || link->get_parent() == 0xFFFFFFFF)
        {
            node_list.add(link);
            if (node_list.size() >= node_list.capacity())
            {
                return;
            }
        }
    }
}

// 0x00B4E950 (:4867)
void ped_node_network::get_node_list(farray<navpoint*, 512>& node_list, unsigned int session_id, bool skip_parented_nodes)
{
    node_list.clear();
    navpoint* head = head_node != 0xFFFFFFFF ? World.object_from_handle<navpoint>(head_node) : nullptr;
    if (head == nullptr)
    {
        return;
    }
    head->heap_data.session_id = session_id;
    node_list.add(head);
    for (int i = 0; i < node_list.size(); i++)
    {
        visit_node(session_id, node_list[i], node_list, skip_parented_nodes);
        if (node_list.size() >= node_list.capacity())
        {
            return;
        }
    }
}

// 0x00B55B70 (:4851)
void ped_node_network::get_node_list(farray<navpoint*, 512>& node_list)
{
    const unsigned int session_id = Navpoint_heap.start_session();
    get_node_list(node_list, session_id, false);
    Navpoint_heap.end_session();
}

// 0x00B4D320 (:2072)
// With the pool full, the network farthest from the player is reused unless it is within 1 km (1e6 squared).
static ped_node_network* get_free_ped_network()
{
    ped_node_network* network = Ped_node_network_pool.alloc(0);
    if (network != nullptr)
    {
        return network;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return nullptr;
    }
    // rfg.exe starts its search at the pool's first slot. With the pool full that slot is on the list too, so only a tie can
    // come out differently from starting at the head of the list.
    network = Ped_node_network_pool.get_first(0);
    float farthest_sqrd = bbox_find_xz_distance_squared(network->region.min, network->region.max, pp->pos);
    for (ped_node_network* other = network; other != nullptr; other = Ped_node_network_pool.get_next(other))
    {
        const float dist_sqrd = bbox_find_xz_distance_squared(other->region.min, other->region.max, pp->pos);
        if (farthest_sqrd < dist_sqrd)
        {
            network = other;
            farthest_sqrd = dist_sqrd;
        }
    }
    return farthest_sqrd <= 1000000.0f ? nullptr : network;
}

// 0x00B4D400 (:2111)
// LTCG: navp in EDI. Magic numbers: a thin box around the head node, 1 m wide and 500 m tall each way.
static ped_node_network* create_ped_network(const navpoint* navp, unsigned int session_id)
{
    static const vector OFFSET(1.0f, 500.0f, 1.0f);
    ped_node_network* network = get_free_ped_network();
    if (network == nullptr)
    {
        return nullptr;
    }
    network->reset();
    network->head_node = navp->get_handle();
    network->head_node_pos = navp->pos;
    network->region.min = navp->pos - OFFSET;
    network->region.max = navp->pos + OFFSET;
    network->zone_xy = navp->get_handle() >> 24;
    return network;
}

// 0x00B556D0 (:2249)
// LTCG: navp in ECX, session_id in EAX. Nodes in a building (with a parent) join no network of their own.
static void amb_spawn_register_new_node(navpoint* navp, unsigned int session_id)
{
    navp->heap_data.session_id = session_id;
    if (navp->get_parent() != 0xFFFFFFFF || navp->num_links < 1)
    {
        return;
    }
    ped_node_network* network = create_ped_network(navp, session_id);
    if (network == nullptr)
    {
        return;
    }
    farray<navpoint*, 512> node_list;
    network->get_node_list(node_list, session_id, true);
    for (int i = 0; i < node_list.size(); i++)
    {
        bbox_expand(network->region.min, network->region.max, node_list[i]->pos);
    }
}

// 0x00B59550 (:2133)
// Original: adds the network once for each of its nodes inside the area, so the list can hold it many times (and print the
// array's Int3 line when it fills), and every user then walks it that many times.
static void nearest_local_ped_node_network(const bbox& test_area, farray<ped_node_network*, 128>& network_list)
{
    network_list.clear();
    const vector center((test_area.max.x + test_area.min.x) * 0.5f, (test_area.max.y + test_area.min.y) * 0.5f, (test_area.max.z + test_area.min.z) * 0.5f);
    ped_node_network* nearest = nullptr;
    float nearest_sqrd = FLT_MAX;
    for (ped_node_network* network = Ped_node_network_pool.get_first(0); network != nullptr; network = Ped_node_network_pool.get_next(network))
    {
        if (network->head_node == 0xFFFFFFFF || World.object_from_handle<navpoint>(network->head_node) == nullptr)
        {
            continue;
        }
        const float dist_sqrd = bbox_find_xz_distance_squared(network->region.min, network->region.max, center);
        if (dist_sqrd < nearest_sqrd)
        {
            nearest_sqrd = dist_sqrd;
            nearest = network;
        }
    }
    if (nearest == nullptr)
    {
        return;
    }
    farray<navpoint*, 512> node_list;
    nearest->get_node_list(node_list);
    for (int i = 0; i < node_list.size(); i++)
    {
        if (ix_point_in_box(node_list[i]->pos, test_area.min, test_area.max))
        {
            network_list.add(nearest);
        }
    }
}

// 0x00B597E0 (:2186)
static void find_local_ped_node_networks(const bbox& test_area, farray<ped_node_network*, 128>& network_list)
{
    network_list.clear();
    for (ped_node_network* network = Ped_node_network_pool.get_first(0); network != nullptr; network = Ped_node_network_pool.get_next(network))
    {
        const bbox& region = network->region;
        if (test_area.min.x < region.max.x && test_area.min.y < region.max.y && test_area.min.z < region.max.z && region.min.x < test_area.max.x && region.min.y < test_area.max.y && region.min.z < test_area.max.z && network->head_node != 0xFFFFFFFF && World.object_from_handle<navpoint>(network->head_node) != nullptr)
        {
            network_list.add(network);
        }
    }
    if (network_list.size() == 0)
    {
        nearest_local_ped_node_network(test_area, network_list);
    }
}

// 0x00B55A80 (:4112)
static const navpoint* find_nearest_navpoint_from_list(const vector& pos, const farray<ped_node_network*, 128>& network_list)
{
    farray<navpoint*, 512> node_list;
    const navpoint* nearest = nullptr;
    float nearest_sqrd = FLT_MAX;
    const unsigned int session_id = Navpoint_heap.start_session();
    for (int i = 0; i < network_list.size(); i++)
    {
        network_list[i]->get_node_list(node_list, session_id, false);
        for (int j = 0; j < node_list.size(); j++)
        {
            const float dist_sqrd = vec_dist_squared(pos, node_list[j]->pos);
            if (dist_sqrd < nearest_sqrd)
            {
                nearest = node_list[j];
                nearest_sqrd = dist_sqrd;
            }
        }
    }
    Navpoint_heap.end_session();
    return nearest;
}

// ---- node_list_cache: travel distances along the network from the node nearest the player
// 0x00B47B10 (:4161)
void node_list_cache::reset()
{
    head_node_handle = 0xFFFFFFFF;
    node_to_cost_hash.clear();
    update_period.set(0);
}

// 0x00B59E00 (:4169)
void node_list_cache::update_node_cost()
{
    if (!update_period.elapsed())
    {
        return;
    }
    // Magic numbers: a quarter to half a second.
    update_period.set_rand(250, 500);
    const player* pp = player_get_local_player();
    bbox area;
    area.min = pp->pos - NEAR_PLAYER_COLLECTION_OFFSET;
    area.max = pp->pos + NEAR_PLAYER_COLLECTION_OFFSET;
    farray<ped_node_network*, 128> network_list;
    find_local_ped_node_networks(area, network_list);
    if (network_list.size() < 1)
    {
        return;
    }
    const navpoint* nearest_navp = find_nearest_navpoint_from_list(pp->pos, network_list);
    if (nearest_navp != nullptr && head_node_handle != nearest_navp->get_handle())
    {
        rebuild_network_cost(nearest_navp);
    }
}

// 0x00B4E5A0 (:4207)
// Breadth first from the node, up to 256 nodes within 200 m of it. Costs can still fall after a node has been expanded, and
// its neighbours do not hear about it.
void node_list_cache::rebuild_network_cost(const navpoint* nearest_navp)
{
    navpoint* root = (navpoint*)nearest_navp;
    head_node_handle = root->get_handle();
    const unsigned int session_id = Navpoint_heap.start_session();
    root->heap_data.flags.is_open = 1;
    root->heap_data.arrival_cost = 0.0f;
    root->heap_data.est_cost_to_goal = 0.0f;
    root->heap_data.session_id = session_id;
    root->prev_path_navpoint = nullptr;
    Navpoint_heap.insert_cell(root);
    // Magic number: rfg.exe's stack list.
    navpoint* nodes[256];
    nodes[0] = root;
    int num_nodes = 1;
    for (int i = 0; i < num_nodes; i++)
    {
        navpoint* node = nodes[i];
        for (int j = 0; j < node->num_links; j++)
        {
            navpoint* link = node->links[j];
            if (link == nullptr)
            {
                continue;
            }
            const float cost = vec_dist(node->pos, link->pos) + node->heap_data.arrival_cost;
            if (link->heap_data.session_id == session_id)
            {
                if (cost < link->heap_data.arrival_cost)
                {
                    link->heap_data.arrival_cost = cost;
                    link->heap_data.est_cost_to_goal = cost;
                    link->prev_path_navpoint = node;
                }
                continue;
            }
            link->heap_data.flags.is_open = 1;
            link->heap_data.arrival_cost = cost;
            link->heap_data.est_cost_to_goal = cost;
            link->heap_data.session_id = session_id;
            link->prev_path_navpoint = node;
            if (vec_dist_squared(root->pos, link->pos) < MAX_ARRIVAL_DISTANCE_SQRD && num_nodes < 256)
            {
                nodes[num_nodes++] = link;
            }
        }
    }
    node_to_cost_hash.clear();
    for (int i = 0; i < num_nodes; i++)
    {
        if (nodes[i]->heap_data.arrival_cost <= MAX_NODE_TRAVEL_DISTANCE)
        {
            node_to_cost_hash.insert_key_data(nodes[i]->get_handle(), nodes[i]->heap_data.arrival_cost);
        }
    }
    Navpoint_heap.end_session();
}

// 0x00B4E850 (:4300)
float node_list_cache::get_travel_distance(unsigned int node_handle)
{
    float cost;
    return node_to_cost_hash.get(node_handle, cost) ? cost : FLT_MAX;
}

// 0x00B4F760 (:5851)
float amb_spawn_get_navpoint_cost(unsigned int navpoint_handle)
{
    return Cached_node_list.get_travel_distance(navpoint_handle);
}

// ---- spawning NPCs
// 0x00B474D0 (:1897)
// LTCG: spawn_info in ESI.
static void count_npcs_in_spawn_direction(ambient_ped_spawn_info& spawn_info)
{
    spawn_info.npcs_in_spawn_direction = 0;
    for (int i = 0; i < spawn_info.npc_list.size(); i++)
    {
        if (is_in_direction(spawn_info.camera_pos, spawn_info.spawn_direction, spawn_info.npc_list[i]->pos, FRONT_OF_CAMERA_DOT))
        {
            spawn_info.npcs_in_spawn_direction++;
        }
    }
}

// 0x00B47610 (:1916)
// LTCG: navp in EAX.
static bool can_spawn_at_navp(const navpoint* navp, ambient_ped_spawn_info& spawn_info)
{
    if (!Navpoint_treat_all_as_enabled && navp->get_disabled())
    {
        return false;
    }
    if (!Player_spawn_region.in_spawn_region_npc(navp->pos) || restricted_area_in_any_region(navp->pos, true))
    {
        return false;
    }
    return is_in_direction(spawn_info.camera_pos, spawn_info.spawn_direction, navp->pos, FRONT_OF_CAMERA_DOT);
}

// 0x00B552F0 (:1942)
// Walks out from the nearest node. Magic number: of the nodes it may spawn at, the four nearest along the network stay.
static void build_spawn_node_list(unsigned int session_id, ambient_ped_spawn_info& spawn_info)
{
    static const int MAX_SPAWN_NODES = 4;
    spawn_info.spawn_node_list.clear();
    farray<navpoint*, 256> open_list;
    farray<navpoint_cost_pair, 512> candidates;
    spawn_info.nearest_navp->heap_data.session_id = session_id;
    open_list.add(spawn_info.nearest_navp);
    for (int i = 0; i < open_list.size(); i++)
    {
        navpoint* navp = open_list[i];
        const float cost = Cached_node_list.get_travel_distance(navp->get_handle());
        if (MAX_NODE_TRAVEL_DISTANCE < cost)
        {
            continue;
        }
        if (can_spawn_at_navp(navp, spawn_info))
        {
            navpoint_cost_pair candidate;
            candidate.navp = navp;
            candidate.cost = cost;
            candidates.add(candidate);
        }
        for (int j = 0; j < navp->num_links; j++)
        {
            navpoint* link = navp->links[j];
            if (link != nullptr && link->heap_data.session_id != session_id)
            {
                link->heap_data.session_id = session_id;
                open_list.add(link);
            }
        }
    }
    while (candidates.size() > MAX_SPAWN_NODES)
    {
        int farthest = 0;
        for (int i = 1; i < candidates.size(); i++)
        {
            if (candidates[farthest].cost < candidates[i].cost)
            {
                farthest = i;
            }
        }
        candidates.remove_unsorted(farthest);
    }
    for (int i = 0; i < candidates.size(); i++)
    {
        spawn_info.spawn_node_list.add(candidates[i].navp);
    }
}

// 0x00B556A0 (:2025)
static void build_spawn_node_list(ambient_ped_spawn_info& spawn_info)
{
    const unsigned int session_id = Navpoint_heap.start_session();
    build_spawn_node_list(session_id, spawn_info);
    Navpoint_heap.end_session();
}

// 0x00B59AA0 (:2404)
// The next desired guerrilla type that is loaded, in the master list's order.
static const human_info* select_ambient_human_info()
{
    const farray<human_info, 165>& list = human_info_get_master_list();
    for (int tries = 0; tries < list.size(); tries++)
    {
        Last_civilian_spawned++;
        if (Last_civilian_spawned >= list.size() || Last_civilian_spawned < 0)
        {
            Last_civilian_spawned = 0;
        }
        const human_info* info = &list[Last_civilian_spawned];
        if (Desired_guerrilla_list.is_member(info) && srm_human_info_loaded(info))
        {
            return info;
        }
    }
    return nullptr;
}

// 0x00B4D590 (:2441)
static bool is_inside_safehouse_area(const vector& pos)
{
    const int count = World.get_num_objects<object_safehouse>();
    for (int i = 0; i < count; i++)
    {
        const object_safehouse* safehouse = World.get_object<object_safehouse>(i);
        if (!safehouse->is_enabled())
        {
            continue;
        }
        vector bmin;
        vector bmax;
        safehouse->get_local_bbox(bmin, bmax);
        vector local_pos = pos;
        vec_change_to_local_coords(local_pos, safehouse->pos, safehouse->orient);
        if (ix_point_in_box(local_pos, bmin, bmax))
        {
            return true;
        }
    }
    return false;
}

// 0x00B5EB20 (:2468)
// Inside a safehouse an NPC keeps its default team and talks like a miner.
static spawn_status_result spawn_npc_navpoint(const navpoint* navp, const ambient_spawn_info* amb_spawn_info)
{
    npc_spawn_param_list spawn_list(NPC_MANAGER_AMBIENT);
    spawn_list.reset();
    const human_info* info = select_ambient_human_info();
    if (info == nullptr)
    {
        return SPAWN_STATUS_FAILURE;
    }
    spawn_list.spawn_pos = navp->pos;
    const bool in_safehouse = is_inside_safehouse_area(navp->pos);
    const vector facing = face_navpoint_link(navp);
    spawn_list.spawn_orient.make_quick(facing);
    spawn_list.navp = navp;
    spawn_list.npc_flags.ambient_spawn = 1;
    spawn_list.npc_flags.use_default_team = in_safehouse;
    spawn_list.hflags.miner_persona_lines = in_safehouse;
    spawn_list.npc_spawn_info.add(npc_spawn_params(info));
    spawn_list.spawn_building_handle = navp->get_parent();
    // Original: looks for the behaviour region around the node, then uses neither the region nor amb_spawn_info's
    // ambient_behavior_id (0x00B5EDBE stores a constant): every ambient NPC gets group 2.
    const int num_regions = World.get_num_objects<object_ambient_behavior_region>();
    for (int i = 0; i < num_regions; i++)
    {
        const object_ambient_behavior_region* region = World.get_object<object_ambient_behavior_region>(i);
        if (vec_dist_squared(region->pos, navp->pos) < region->radius * region->radius)
        {
            break;
        }
    }
    spawn_list.ambient_behavior_id = 2;
    return object_spawn_do_spawn(spawn_list);
}

// 0x00B476B0 (:2512)
// LTCG: spawn_node_list in ESI.
static const navpoint* pick_spawn_navpoint(farray<navpoint*, 512>& spawn_node_list)
{
    if (spawn_node_list.size() < 1)
    {
        return nullptr;
    }
    const int pick = spawn_node_list.size() == 1 ? 0 : int_rand_range(0, spawn_node_list.size() - 1);
    const navpoint* navp = spawn_node_list[pick];
    spawn_node_list.remove_unsorted(pick);
    return navp;
}

// 0x00B5EDF0 (:2534)
static void do_single_npc_spawn(ambient_ped_spawn_info& spawn_info)
{
    const navpoint* navp = pick_spawn_navpoint(spawn_info.spawn_node_list);
    if (navp != nullptr)
    {
        spawn_npc_navpoint(navp, spawn_info.amb_spawn_info);
    }
}

// 0x00B59BA0 (:3104)
// Magic number: a box 50 m each way around the player.
static void collect_near_player_networks(ambient_ped_spawn_info& spawn_info)
{
    const vector& pos = spawn_info.pp->pos;
    bbox area;
    area.min = vector(pos.x - 50.0f, pos.y - 50.0f, pos.z - 50.0f);
    area.max = vector(pos.x + 50.0f, pos.y + 50.0f, pos.z + 50.0f);
    find_local_ped_node_networks(area, spawn_info.network_list);
}

// 0x00B4DF00 (:3116)
// LTCG: spawn_info in EDI.
static void collect_all_ambient_npcs(ambient_ped_spawn_info& spawn_info)
{
    spawn_info.npc_list.clear();
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        const npc* np = World.get_object<npc>(i);
        if (np->npc_flags.ambient_spawn)
        {
            spawn_info.npc_list.add(np);
        }
    }
}

// 0x00B59C70 (:3133)
// LTCG: spawn_info in ESI. Keeps the nodes of the network holding the node nearest the player.
static void collect_local_ped_nodes(ambient_ped_spawn_info& spawn_info)
{
    spawn_info.node_list.clear();
    farray<navpoint*, 512> node_list;
    ped_node_network* best_network = nullptr;
    navpoint* best_navp = nullptr;
    float best_sqrd = FLT_MAX;
    const unsigned int session_id = Navpoint_heap.start_session();
    for (int i = 0; i < spawn_info.network_list.size(); i++)
    {
        ped_node_network* network = spawn_info.network_list[i];
        network->get_node_list(node_list, session_id, false);
        if (node_list.size() < 1)
        {
            continue;
        }
        navpoint* nearest = node_list[0];
        float nearest_sqrd = vec_dist_squared(spawn_info.pp->pos, nearest->pos);
        for (int j = 1; j < node_list.size(); j++)
        {
            const float dist_sqrd = vec_dist_squared(spawn_info.pp->pos, node_list[j]->pos);
            if (dist_sqrd < nearest_sqrd)
            {
                nearest = node_list[j];
                nearest_sqrd = dist_sqrd;
            }
        }
        if (nearest_sqrd < best_sqrd)
        {
            best_sqrd = nearest_sqrd;
            best_network = network;
            best_navp = nearest;
        }
    }
    Navpoint_heap.end_session();
    if (best_network != nullptr && best_navp != nullptr)
    {
        best_network->get_node_list(spawn_info.node_list);
        spawn_info.nearest_navp = best_navp;
    }
}

// 0x00B47850 (:3209)
// The oldest on foot past the distance test, else the farthest.
// Original: the test is 20 against a squared distance (0x00B47903), so 4.5 m, not 20.
static npc* select_despawning_npc(ambient_ped_spawn_info& spawn_info)
{
    npc* oldest = nullptr;
    npc* farthest = nullptr;
    int oldest_lifetime = 0;
    float farthest_sqrd = 0.0f;
    const player* pp = player_get_local_player();
    for (int i = 0; i < spawn_info.npc_list.size(); i++)
    {
        npc* np = (npc*)spawn_info.npc_list[i];
        if (np->vehicle_handle != 0xFFFFFFFF || np->npc_flags.converted_guerrilla)
        {
            continue;
        }
        const float dist_sqrd = vec_dist_squared(pp->pos, np->pos);
        const int lifetime = np->get_lifetime();
        if (farthest_sqrd < dist_sqrd)
        {
            farthest = np;
            farthest_sqrd = dist_sqrd;
        }
        if (20.0f < dist_sqrd && oldest_lifetime < lifetime)
        {
            oldest = np;
            oldest_lifetime = lifetime;
        }
    }
    return oldest != nullptr ? oldest : farthest;
}

// 0x00B47940 (:3266)
static void do_frame_spawn_aggressive_despawn(ambient_ped_spawn_info& spawn_info)
{
    npc* np = select_despawning_npc(spawn_info);
    if (np != nullptr)
    {
        start_npc_despawn(np);
    }
}

// 0x00B5F640 (:3980)
// Magic numbers: from 75 NPCs on half as often, and none from 90 on.
static void do_frame_spawn_npc()
{
    if (!Npc_spawn_period.elapsed())
    {
        return;
    }
    if (World.get_num_objects<npc>() < 75)
    {
        Npc_spawn_period.set_rand(250, 500);
    }
    else
    {
        Npc_spawn_period.set_rand(500, 1000);
    }
    if (World.get_num_objects<npc>() >= 90 || activity_disable_ambient_ped_spawn() || near_npc_population_cap())
    {
        return;
    }
    ambient_ped_spawn_info spawn_info;
    spawn_info.amb_spawn_info = amb_spawn_info_current();
    if (spawn_info.amb_spawn_info == nullptr || !spawn_info.amb_spawn_info->ambient_flags.ped_spawn_enabled)
    {
        return;
    }
    spawn_info.pp = player_get_local_player();
    collect_near_player_networks(spawn_info);
    if (spawn_info.network_list.size() < 1)
    {
        return;
    }
    collect_all_ambient_npcs(spawn_info);
    if ((unsigned int)gsm_get_alert_level() >= NUM_ALERT_LEVELS)
    {
        return;
    }
    if (near_max_ped_count(spawn_info.amb_spawn_info) <= spawn_info.npc_list.size())
    {
        do_frame_spawn_aggressive_despawn(spawn_info);
    }
    if (spawn_info.npc_list.size() >= max_ped_count(spawn_info.amb_spawn_info))
    {
        return;
    }
    collect_local_ped_nodes(spawn_info);
    if (spawn_info.nearest_navp == nullptr)
    {
        return;
    }
    build_camera_props(spawn_info);
    pick_spawn_direction(spawn_info.spawn_direction, spawn_info);
    count_npcs_in_spawn_direction(spawn_info);
    if (spawn_info.npcs_in_spawn_direction >= max_ped_count_quad(spawn_info.amb_spawn_info))
    {
        return;
    }
    build_spawn_node_list(spawn_info);
    // Original: needs two nodes to spawn at one.
    if (spawn_info.spawn_node_list.size() > 1)
    {
        do_single_npc_spawn(spawn_info);
    }
}

// ---- spawning traffic
// 0x00B4DFB0 (:3327)
static void find_all_ambinet_vehicles(ambient_veh_spawn_info& spawn_info)
{
    spawn_info.ambient_vehicle_list.clear();
    spawn_info.vehicles_in_region = 0;
    spawn_info.quadrant_count[0] = 0;
    spawn_info.quadrant_count[1] = 0;
    spawn_info.quadrant_count[2] = 0;
    spawn_info.quadrant_count[3] = 0;
    const int count = World.get_num_objects<vehicle>();
    for (int i = 0; i < count; i++)
    {
        const vehicle* vp = World.get_object<vehicle>(i);
        if (!vp->m_flags.ambient_spawn)
        {
            continue;
        }
        spawn_info.ambient_vehicle_list.add(vp);
        if (spawn_info.ambient_vehicle_list.size() >= spawn_info.ambient_vehicle_list.capacity())
        {
            return;
        }
        spawn_info.vehicles_in_region++;
        vector direction;
        float distance;
        spawn_info.quadrant_count[get_quadrant_id(vp->pos, spawn_info, direction, distance)]++;
    }
}

// 0x00B4E090 (:3390)
// Original: the test is 20 against a squared distance, so 4.5 m.
static bool near_other_ambient_vehicle(const vector& spawn_pos)
{
    const int count = World.get_num_objects<automobile>();
    for (int i = 0; i < count; i++)
    {
        const automobile* vp = World.get_object<automobile>(i);
        if (vp->m_flags.ambient_spawn && vec_dist_squared(spawn_pos, vp->pos) < 20.0f)
        {
            return true;
        }
    }
    return false;
}

// 0x00B4E110 (:3411)
// LTCG: current_road_spline in EAX.
static bool can_spawn_ambient_vehicle(const road_spline_identifier& current_road_spline, const ambient_veh_spawn_info& spawn_info)
{
    const object_path_road* road = current_road_spline.road_spline;
    if (!road->road_flags.ambient)
    {
        return false;
    }
    const vector& pos = road->spline_data.points[current_road_spline.point].pos;
    if (!World.is_zone_streamed(pos) || inside_no_spawn_zone(road->get_handle(), (int)current_road_spline.point))
    {
        return false;
    }
    return Player_spawn_region.in_spawn_region_vehicle(pos) && !near_other_ambient_vehicle(pos);
}

// 0x00B55770 (:3441)
// LTCG: road_spawn_list in EAX.
static void add_road_index_to_list(farray<road_spawn_info, 16>& road_spawn_list, road_spline_identifier& current_road_spline)
{
    for (int i = 0; i < road_spawn_list.size(); i++)
    {
        road_spawn_info& info = road_spawn_list[i];
        if (info.road == current_road_spline.road_spline)
        {
            if (info.index_list.size() < info.index_list.capacity())
            {
                info.index_list.add(current_road_spline.point);
            }
            return;
        }
    }
    road_spawn_info* info = road_spawn_list.add_in_place();
    if (info != nullptr)
    {
        info->road = current_road_spline.road_spline;
        info->index_list.clear();
        info->index_list.add(current_road_spline.point);
    }
}

// 0x00B55870 (:3469)
// Magic number: road points the player could reach within 250 m along the roads.
static void build_vehicle_spawn_locations(ambient_veh_spawn_info& spawn_info)
{
    const farray<road_distance_info, 256>& road_list = road_info_get_list();
    spawn_info.road_spawn_list.clear();
    for (int i = 0; i < road_list.size(); i++)
    {
        const road_distance_info& info = road_list[i];
        if (250.0f < info.travel_distance)
        {
            continue;
        }
        road_spline_identifier spline;
        spline.road_spline = info.road_handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(info.road_handle) : nullptr;
        if (spline.road_spline == nullptr)
        {
            continue;
        }
        spline.point = info.index;
        if (can_spawn_ambient_vehicle(spline, spawn_info))
        {
            add_road_index_to_list(spawn_info.road_spawn_list, spline);
        }
    }
}

// 0x00B4E190 (:3529)
// LTCG: spline_list in ECX. Prefers points within 45 degrees ahead of the player's movement.
static bool pick_vehicle_spawn_loc_list(const farray<road_spawn_info, 16>& spline_list, road_spline_identifier& spawn_spline_id)
{
    farray<road_spawn_selection_info, 128> ahead;
    farray<road_spawn_selection_info, 128> others;
    const player* pp = player_get_local_player();
    const vector move_direction = Player_spawn_region.get_move_direction();
    for (int i = 0; i < spline_list.size(); i++)
    {
        const road_spawn_info& info = spline_list[i];
        for (int j = 0; j < info.index_list.size(); j++)
        {
            road_spawn_selection_info selection;
            selection.spawn_id.road_spline = info.road;
            selection.spawn_id.point = info.index_list[j];
            selection.pos = info.road->get_road_point(selection.spawn_id.point);
            selection.direction = selection.pos - pp->pos;
            selection.direction.normalize_safe();
            selection.dot = move_direction * selection.direction;
            if (selection.dot < SPAWN_DIRECTION_DOT)
            {
                others.add(selection);
            }
            else
            {
                ahead.add(selection);
            }
        }
    }
    const farray<road_spawn_selection_info, 128>& picks = ahead.size() > 0 ? ahead : others;
    if (picks.size() < 1)
    {
        return false;
    }
    spawn_spline_id = picks[picks.size() == 1 ? 0 : int_rand_range(0, picks.size() - 1)].spawn_id;
    return true;
}

// 0x00B55920 (:3591)
// LTCG: spline_list in ESI, spawn_spline_id in ECX. Roads not used by the last spawns come first.
static bool pick_vehicle_spawn_loc(const farray<road_spawn_info, 16>& spline_list, road_spline_identifier& spawn_spline_id)
{
    farray<road_spawn_info, 16> new_roads;
    for (int i = 0; i < spline_list.size(); i++)
    {
        const road_spawn_info& info = spline_list[i];
        if (Last_road_spawn_list.is_member(info.road->get_handle()))
        {
            continue;
        }
        road_spawn_info* copy = new_roads.add_in_place();
        if (copy != nullptr)
        {
            copy->road = info.road;
            copy->index_list.copy(info.index_list);
        }
    }
    if (new_roads.size() > 0 && pick_vehicle_spawn_loc_list(new_roads, spawn_spline_id))
    {
        return true;
    }
    return pick_vehicle_spawn_loc_list(spline_list, spawn_spline_id);
}

// 0x00B59970 (:2367)
static const vehicle_info* pick_loaded_and_streamed_vehicle_info(const base_array<vehicle_info const*>& vehicle_info_list)
{
    farray<vehicle_info const*, 163> loaded;
    for (int i = 0; i < vehicle_info_list.size(); i++)
    {
        const vehicle_info* v_info = vehicle_info_list[i];
        if (stream2_get_status(v_info->srid) == ST_LOADED && srm_vehicle_info_loaded(v_info))
        {
            loaded.add(v_info);
        }
    }
    if (loaded.size() == 0)
    {
        return nullptr;
    }
    return loaded[loaded.size() == 1 ? 0 : int_rand_range(0, loaded.size() - 1)];
}

// 0x00B5EE60 (:3640)
// Magic numbers: half the road's width to the right of the road point; three people at most, two fewer when alert or busy,
// one from 75 NPCs on; the next spawn 2 to 4 seconds later.
static void spawn_single_vehicle(ambient_veh_spawn_info& spawn_info)
{
    farray<road_segment_info, 8> route;
    farray<human_info const*, 16> loaded_humans;
    vehicle_spawn_params veh_params;
    veh_params.reset();
    npc_spawn_param_list npc_params(NPC_MANAGER_AMBIENT);
    npc_params.reset();
    road_spline_identifier spawn_id;
    if (!pick_vehicle_spawn_loc(spawn_info.road_spawn_list, spawn_id))
    {
        return;
    }
    object_path_road* road = spawn_id.road_spline;
    const unsigned int start_index = spawn_id.point;
    const vector road_pos = road->spline_data.points[start_index].pos;
    matrix spawn_orient;
    if (!ambient_build_road_waypoints(route, spawn_orient, road, start_index))
    {
        return;
    }
    const float half_width = road->get_width((float)start_index) * 0.5f;
    const vector spawn_pos(road_pos.x + spawn_orient.rvec.x * half_width, road_pos.y + spawn_orient.rvec.y * half_width, road_pos.z + spawn_orient.rvec.z * half_width);
    if (!World.is_zone_streamed(spawn_pos))
    {
        return;
    }
    for (int i = 0; i < Desired_guerrilla_list.size(); i++)
    {
        const human_info* info = Desired_guerrilla_list[i];
        if (srm_human_info_loaded(info) && stream2_get_status(info->srid) == ST_LOADED)
        {
            loaded_humans.add(info);
        }
    }
    if (loaded_humans.size() < 1)
    {
        return;
    }
    veh_params.veh_info = pick_loaded_and_streamed_vehicle_info(spawn_info.amb_spawn_info->v_spawn_group->v_info_list);
    if (veh_params.veh_info == nullptr)
    {
        return;
    }
    veh_params.veh_spawn_flags.mobile_vehicle_spawn = 1;
    veh_params.veh_spawn_flags.road_spawn = 1;
    veh_params.veh_spawn_flags.one_attempt = 1;
    veh_params.veh_spawn_flags.no_save = 1;
    veh_params.vflags.ambient_spawn = 1;
    veh_params.spawn_pos = spawn_pos;
    veh_params.spawn_orient = spawn_orient;
    veh_params.forward_speed = AMBIENT_VEHICLE_SPEED;
    if (spawn_info.amb_spawn_info->h_spawn_group == nullptr)
    {
        return;
    }
    if (object_spawn_vehicle(veh_params) != SPAWN_STATUS_SUCCESS || veh_params.vp == nullptr)
    {
        return;
    }
    vehicle* vp = veh_params.vp;
    const int num_seats = vehicle_get_num_seats(vp);
    if (num_seats < 1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        World.destroy_object(vp, false);
        return;
    }
    int num_people = 1;
    if (World.get_num_objects<npc>() < 75)
    {
        num_people = int_rand_range(1, num_seats);
        if (num_people > 3)
        {
            num_people = 3;
        }
        if (asm_restrict_passenger_spawning())
        {
            num_people -= 2;
            if (num_people < 1)
            {
                num_people = 1;
            }
        }
    }
    for (int i = 0; i < num_people; i++)
    {
        npc_params.npc_spawn_info.add(npc_spawn_params(loaded_humans[int_rand_range(0, loaded_humans.size() - 1)]));
    }
    npc_params.npc_flags.ambient_vehicle_spawn = 1;
    npc_params.npc_flags.ambient_spawn = 1;
    npc_params.hflags.limited_vehicle_exit = 1;
    npc_params.spawn_pos = spawn_pos;
    npc_params.spawn_orient = spawn_orient;
    npc_params.vp = vp;
    if (object_spawn_do_spawn(npc_params) != SPAWN_STATUS_SUCCESS)
    {
        World.destroy_object(vp, false);
        return;
    }
    bool route_given = false;
    for (int i = 0; i < npc_params.npc_spawn_info.size(); i++)
    {
        npc* np = npc_params.npc_spawn_info[i].np;
        if (np == nullptr)
        {
            continue;
        }
        np->get_ai()->road_route.clear();
        route_given = true;
        for (int j = 0; j < route.size(); j++)
        {
            np->get_ai()->road_route.add(route[j]);
        }
    }
    if (!route_given)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        World.destroy_object(vp, false);
        return;
    }
    if (Last_road_spawn_list.size() >= Last_road_spawn_list.capacity())
    {
        Last_road_spawn_list.remove(0);
    }
    Last_road_spawn_list.add(road->get_handle());
    Vehicle_spawn_period.set_rand(2000, 4000);
}

// 0x00B47970 (:3821)
// LTCG: spawn_info in ESI. The farthest empty vehicle outside the minimum radius.
// Original bugs: the position to test is never set, so vec_change_to_local_coords keeps transforming what the stack held and
// the front test means nothing (zero to start with here). And the behind-the-camera branch compares its best distance with
// itself (0x00B47A75), so only a vehicle taken for in front is ever picked.
static const vehicle* pick_ambient_vehicle_for_delete(ambient_veh_spawn_info& spawn_info)
{
    const vehicle* best_front = nullptr;
    float best_front_sqrd = 0.0f;
    vector local_pos;
    local_pos.zero();
    for (int i = 0; i < spawn_info.ambient_vehicle_list.size(); i++)
    {
        const vehicle* vp = spawn_info.ambient_vehicle_list[i];
        if (vehicle_get_num_passengers(vp) > 0 || Player_spawn_region.inside_min_radius_vehicle(vp->pos))
        {
            continue;
        }
        const float dist_sqrd = vec_dist_squared(spawn_info.pp->pos, vp->pos);
        vec_change_to_local_coords(local_pos, spawn_info.camera_pos, spawn_info.camera_orient);
        if (0.0f < local_pos.z && best_front_sqrd < dist_sqrd)
        {
            best_front = vp;
            best_front_sqrd = dist_sqrd;
        }
    }
    return best_front;
}

// 0x00B47AC0 (:3874)
// LTCG: spawn_info in EAX.
static void agressive_delete_ambient_vehicle(ambient_veh_spawn_info& spawn_info)
{
    vehicle* vp = (vehicle*)pick_ambient_vehicle_for_delete(spawn_info);
    if (vp != nullptr)
    {
        destroy_ambient_vehicle(vp);
    }
}

// 0x00B5F490 (:3883)
// Magic numbers: a road within 60 m of the player; at 10 ambient vehicles one goes instead.
static void do_frame_spawn_vehicle()
{
    if (!Vehicle_spawn_period.elapsed())
    {
        return;
    }
    Vehicle_spawn_period.set_rand(250, 500);
    if (activity_disable_ambient_veh_spawn() || Disable_ambient_vehicle_spawn || near_npc_population_cap() || near_auto_population_cap())
    {
        return;
    }
    ambient_veh_spawn_info spawn_info;
    spawn_info.camera_pos = Rfg_camera.real_pos;
    spawn_info.camera_orient = Rfg_camera.real_orient;
    spawn_info.amb_spawn_info = amb_spawn_info_current();
    if (spawn_info.amb_spawn_info == nullptr || !spawn_info.amb_spawn_info->ambient_flags.veh_spawn_enabled)
    {
        return;
    }
    spawn_info.pp = player_get_local_player();
    spawn_info.nearest_road = object_road_get_closest_road(spawn_info.pp->pos, false, spawn_info.nearest_pos_on_road, spawn_info.dist_on_road, MIN_CAMERA_PED_SPAWN_DISTANCE, false, nullptr, nullptr, false);
    if (spawn_info.nearest_road == nullptr)
    {
        return;
    }
    find_all_ambinet_vehicles(spawn_info);
    spawn_info.minimap_radius = Cartographer.get_zoom_radius();
    if (spawn_info.ambient_vehicle_list.size() >= 10)
    {
        agressive_delete_ambient_vehicle(spawn_info);
        return;
    }
    spawn_info.vehicle_density = max_vehicle_count(spawn_info.amb_spawn_info);
    if (spawn_info.vehicles_in_region >= spawn_info.vehicle_density)
    {
        return;
    }
    build_vehicle_spawn_locations(spawn_info);
    if (spawn_info.road_spawn_list.size() > 0)
    {
        spawn_single_vehicle(spawn_info);
    }
}

// ---- the guerrilla update
// 0x00B5E730 (:1042)
// Rotates one district's type out when nothing of the old types stands and the player is near ped nodes.
// Magic number: a box 80 m each way around the player.
static bool rotate_character_usage()
{
    static const vector NEAR_PLAYER_SIZE(80.0f, 80.0f, 80.0f);
    if (World.get_num_objects<npc>() < 1)
    {
        return false;
    }
    const player* pp = player_get_local_player();
    bbox area;
    area.min = pp->pos - NEAR_PLAYER_SIZE;
    area.max = pp->pos + NEAR_PLAYER_SIZE;
    farray<ped_node_network*, 128> network_list;
    find_local_ped_node_networks(area, network_list);
    if (network_list.size() < 1)
    {
        return false;
    }
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        if (Old_guerrilla_list.is_member(World.get_object<npc>(i)->h_info))
        {
            return false;
        }
    }
    // Magic number: home districts run below 16.
    for (int i = 0; i < 16; i++)
    {
        const human_info* info = find_district_guerrilla((i + Next_rotation_group) % 16);
        if (info == nullptr)
        {
            continue;
        }
        Old_guerrilla_list.add_unique(info);
        Desired_guerrilla_list.remove_matching(info);
        Next_rotation_group = info->home_district + 1;
        return true;
    }
    return false;
}

// 0x00B5EA20 (:1330)
// Original: reads the human group of team 0 and not of the player's team, unlike the vehicle group below it.
static void update_desired_guerrillas()
{
    if (mission_is_active())
    {
        return;
    }
    player* pp = player_get_local_player();
    const ambient_spawn_info* info = nullptr;
    if (pp != nullptr)
    {
        obj_zone* zone = World.get_zone_by_pos(pp->pos);
        if (zone != nullptr)
        {
            info = zone->amb_spawn_info;
        }
    }
    const human_spawn_group_info* human_group = info != nullptr ? info->h_spawn_group : nullptr;
    if (human_group == nullptr)
    {
        human_group = sog_default_human_spawn_group(HUMAN_TEAM_GUERILLA);
    }
    if (Last_guerrilla_spawn_group != human_group || rotate_character_usage())
    {
        select_current_guerrillas(human_group);
    }
    pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const vehicle_spawn_group_info* vehicle_group = info != nullptr ? info->v_spawn_group : nullptr;
    if (vehicle_group == nullptr)
    {
        vehicle_group = sog_default_vehicle_spawn_group(pp->current_team);
    }
    if (Old_vehicle_list.size() == 0 || Last_vehicle_spawn_group != vehicle_group)
    {
        select_current_vehicles(vehicle_group);
    }
    clean_up_old_guerrilla_list();
    clean_up_old_vehicle_list();
}

// ---- the system
// 0x00B42CE0 (:4901)
void amb_spawn_system_init()
{
}

// 0x00B59F80 (:4912)
void amb_spawn_level_init_pre()
{
    Ambient_no_spawn_list.clear();
    Player_spawn_region.reset();
    Npc_despawn_period.set(0);
    Npc_spawn_period.set(0);
    Vehicle_spawn_period.set(0);
    Guerrilla_update_period.set(0);
    Character_delete_period.set(0);
    Cached_node_list.reset();
    Disable_ambient_vehicle_spawn = false;
    for (ped_node_network* network = Ped_node_network_pool.get_first(0); network != nullptr;)
    {
        network = Ped_node_network_pool.free(network);
    }
    amb_spawn_unlock_ambient_levels();
    Last_civilian_spawned = int_rand_range(0, human_info_get_master_list().size() - 1);
}

// 0x00B5F800 (:4938)
// Magic numbers: a quarter to half a second for the deletes, half to one second for the types. NPCs and traffic take turns.
void amb_spawn_do_frame()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    Player_spawn_region.update_region();
    if (Character_delete_period.elapsed())
    {
        do_frame_despawn_delete();
        Character_delete_period.set_rand(250, 500);
    }
    if (Guerrilla_update_period.elapsed())
    {
        update_desired_guerrillas();
        Guerrilla_update_period.set_rand(500, 1000);
    }
    Cached_node_list.update_node_cost();
    if (!Disable_ambient_vehicle_spawn && !Mission_mode)
    {
        srm_allocate_ambient_spawn_slots_human();
        srm_allocate_ambient_spawn_slots_vehicle();
    }
    do_frame_despawn();
    if (do_npc_spawning_this_frame)
    {
        do_frame_spawn_npc();
    }
    else
    {
        do_frame_spawn_vehicle();
    }
    do_npc_spawning_this_frame = !do_npc_spawning_this_frame;
}

// 0x00B55BA0 (:4994)
// Rebuilds the networks of the zone. Magic number: navpoint type 2 is a ped node.
void amb_spawn_zone_load_complete(unsigned short zone_gid)
{
    const unsigned short zone_xy = zone_gid >> 8;
    for (ped_node_network* network = Ped_node_network_pool.get_first(0); network != nullptr;)
    {
        ped_node_network* next = Ped_node_network_pool.get_next(network);
        if ((int)zone_xy == network->zone_xy)
        {
            network->reset();
            Ped_node_network_pool.free(network);
        }
        network = next;
    }
    const unsigned int session_id = Navpoint_heap.start_session();
    const int count = World.get_num_objects<navpoint>();
    for (int i = 0; i < count; i++)
    {
        navpoint* navp = World.get_object<navpoint>(i);
        if (navp->type == 2 && navp->heap_data.session_id != session_id && (navp->get_handle() >> 24) == zone_xy)
        {
            amb_spawn_register_new_node(navp, session_id);
        }
    }
    Navpoint_heap.end_session();
    update_no_spawn_zones(zone_xy);
}

// 0x00B5A060 (:5043)
bool ped_node_near_box(const bbox& area)
{
    farray<ped_node_network*, 128> network_list;
    find_local_ped_node_networks(area, network_list);
    return network_list.size() > 0;
}

// 0x00B5F8D0 (:5053)
// Nodes in a building do not count.
const navpoint* ped_node_find_nearest(const vector& pos, float max_search_size)
{
    bbox area;
    area.min = vector(pos.x - max_search_size, pos.y - max_search_size, pos.z - max_search_size);
    area.max = vector(pos.x + max_search_size, pos.y + max_search_size, pos.z + max_search_size);
    farray<navpoint*, 512> node_list;
    ped_node_get_network_nodes(area, node_list);
    const navpoint* nearest = nullptr;
    float nearest_sqrd = FLT_MAX;
    const float max_sqrd = max_search_size * max_search_size;
    for (int i = 0; i < node_list.size(); i++)
    {
        const navpoint* navp = node_list[i];
        if (navp->get_parent() != 0xFFFFFFFF)
        {
            continue;
        }
        const float dist_sqrd = vec_dist_squared(pos, navp->pos);
        if (dist_sqrd < nearest_sqrd && dist_sqrd < max_sqrd)
        {
            nearest = navp;
            nearest_sqrd = dist_sqrd;
        }
    }
    return nearest;
}

// 0x00B5A0B0 (:5096)
// Original: get_node_list empties the list, and each network has its own session, so only the last network's nodes come back.
void ped_node_get_network_nodes(const bbox& area, farray<navpoint*, 512>& node_list)
{
    farray<ped_node_network*, 128> network_list;
    find_local_ped_node_networks(area, network_list);
    if (network_list.size() < 1)
    {
        return;
    }
    node_list.clear();
    for (int i = 0; i < network_list.size(); i++)
    {
        const unsigned int session_id = Navpoint_heap.start_session();
        network_list[i]->get_node_list(node_list, session_id, false);
        Navpoint_heap.end_session();
    }
}

// 0x00B5FA70 (:5117)
void amb_spawn_level_init_post()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    // Magic number: home districts run below 16.
    Next_rotation_group = int_rand_range(0, 15);
    Last_guerrilla_spawn_group = nullptr;
    Desired_guerrilla_list.clear();
    Old_guerrilla_list.clear();
    Old_vehicle_list.clear();
    update_desired_guerrillas();
    if (!Mission_mode)
    {
        srm_allocate_ambient_spawn_slots_human();
        srm_allocate_ambient_spawn_slots_vehicle();
    }
}

// 0x00B5FAD0 (:5138)
// A first population. Magic numbers: nodes within 150 m of the player, 20 m apart, a quarter of the most NPCs.
void amb_spawn_level_init_spawn()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const obj_zone* zone = World.get_zone_by_pos(pp->pos);
    const ambient_spawn_info* info = zone != nullptr ? zone->amb_spawn_info : nullptr;
    if (info == nullptr || !info->ambient_flags.ped_spawn_enabled)
    {
        return;
    }
    farray<navpoint const*, 512> nodes;
    const int count = World.get_num_objects<navpoint>();
    for (int i = 0; i < count; i++)
    {
        const navpoint* navp = World.get_object<navpoint>(i);
        if (navp->type == 2 && vec_dist_squared(pp->pos, navp->pos) < 22500.0f)
        {
            nodes.add(navp);
        }
    }
    if (nodes.size() == 0 || info->h_spawn_group == nullptr)
    {
        return;
    }
    const int num_to_spawn = max_ped_count_quad(info);
    for (int spawned = 0; spawned < num_to_spawn; spawned++)
    {
        const navpoint* navp;
        if (!nodes.get_random_element(navp))
        {
            return;
        }
        for (int i = 0; i < nodes.size();)
        {
            if (vec_dist_squared(navp->pos, nodes[i]->pos) < 400.0f)
            {
                nodes.remove_unsorted(i);
            }
            else
            {
                i++;
            }
        }
        if (spawn_npc_navpoint(navp, info) != SPAWN_STATUS_SUCCESS)
        {
            return;
        }
    }
}

// 0x00B42CF0 (:5223)
void amb_set_ambient_vehicle_spawn_disabled(bool disabled)
{
    Disable_ambient_vehicle_spawn = disabled;
}

// 0x00B42D00 (:5229)
void amb_spawn_lock_ped_levels(gfm_alert_levels desired_alert_value, ambient_spawn_density desired_spawn_density)
{
    if (mission_is_active() && (unsigned int)desired_alert_value < NUM_ALERT_LEVELS && (unsigned int)desired_spawn_density < NUM_AMB_SPAWN_DENSITY)
    {
        Amb_ped_alert_level_lock = desired_alert_value;
        Amb_ped_density_level_lock = desired_spawn_density;
    }
}

// 0x00B42D30 (:5243)
void amb_spawn_lock_veh_levels(gfm_alert_levels desired_alert_value, ambient_spawn_density desired_spawn_density)
{
    if (mission_is_active() && (unsigned int)desired_alert_value < NUM_ALERT_LEVELS && (unsigned int)desired_spawn_density < NUM_AMB_SPAWN_DENSITY)
    {
        Amb_veh_alert_level_lock = desired_alert_value;
        Amb_veh_density_level_lock = desired_spawn_density;
    }
}

// 0x00B42D60 (:5257)
void amb_spawn_unlock_ambient_levels()
{
    Amb_ped_alert_level_lock = ALERT_LEVEL_INVALID;
    Amb_ped_density_level_lock = AMB_SPAWN_DENSITY_INVALID;
    Amb_veh_alert_level_lock = ALERT_LEVEL_INVALID;
    Amb_veh_density_level_lock = AMB_SPAWN_DENSITY_INVALID;
}

// ---- pedestrian waypoints
// 0x00B5A150 (:5274)
// Magic numbers: a last node within 4 m stands; else searches a box 20 m each way, growing twice by 20.
const navpoint* find_root_ped_node(const npc* np, const ai_pedestrian_info& ped_info)
{
    static const vector NEAR_NPC_BBOX_SIZE(20.0f, 20.0f, 20.0f);
    const navpoint* best = ped_info.get_last_navpoint();
    float best_sqrd = best != nullptr ? vec_dist_squared(np->pos, best->pos) : FLT_MAX;
    if (best_sqrd < 16.0f)
    {
        return best;
    }
    bbox area;
    area.min = np->pos - NEAR_NPC_BBOX_SIZE;
    area.max = np->pos + NEAR_NPC_BBOX_SIZE;
    farray<ped_node_network*, 128> network_list;
    find_local_ped_node_networks(area, network_list);
    for (int tries = 2; network_list.size() == 0; tries--)
    {
        if (tries < 1)
        {
            return nullptr;
        }
        area.min = area.min - NEAR_NPC_BBOX_SIZE;
        area.max = area.max + NEAR_NPC_BBOX_SIZE;
        find_local_ped_node_networks(area, network_list);
    }
    farray<navpoint*, 512> node_list;
    const unsigned int session_id = Navpoint_heap.start_session();
    for (int i = 0; i < network_list.size(); i++)
    {
        network_list[i]->get_node_list(node_list, session_id, false);
        for (int j = 0; j < node_list.size(); j++)
        {
            const float dist_sqrd = vec_dist_squared(np->pos, node_list[j]->pos);
            if (dist_sqrd < best_sqrd)
            {
                best = node_list[j];
                best_sqrd = dist_sqrd;
            }
        }
    }
    Navpoint_heap.end_session();
    return best;
}

// 0x00B4E9F0 (:5348)
// A random link not yet on the list, preferring those in the direction, else any link.
const navpoint* find_linked_navpoint(const navpoint* navp, const vector& desired_direction, const farray<unsigned int, 8>& waypoint_list)
{
    if (navp == nullptr || navp->num_links < 1)
    {
        return nullptr;
    }
    farray<navpoint const*, 10> forward;
    farray<navpoint const*, 10> other;
    for (int i = 0; i < navp->num_links; i++)
    {
        const navpoint* link = navp->links[i];
        vector to_link = link->pos - navp->pos;
        to_link.normalize_safe();
        if (to_link * desired_direction <= 0.0f)
        {
            other.add(link);
        }
        else
        {
            forward.add(link);
        }
    }
    farray<navpoint const*, 10>* lists[2] = {&forward, &other};
    for (int i = 0; i < 2; i++)
    {
        farray<navpoint const*, 10>& list = *lists[i];
        while (list.size() > 0)
        {
            const int pick = list.size() == 1 ? 0 : int_rand_range(0, list.size() - 1);
            const navpoint* link = list[pick];
            list.remove_unsorted(pick);
            if (!waypoint_list.is_member(link->get_handle()))
            {
                return link;
            }
        }
    }
    return navp->links[int_rand_range(0, navp->num_links - 1)];
}

// 0x00B4EC00 (:5414)
// LTCG: current_navp in EDI. A random one of the list within MIN_DOT of the direction, else a random other.
static const navpoint* find_next_navpoint_direction(const navpoint* current_navp, const farray<navpoint const*, 10>& nav_selection_list, const vector& direction)
{
    // Magic number: the direction must be unit length to 0.01.
    if (!direction.is_unit(0.01f) || nav_selection_list.size() < 1)
    {
        return nullptr;
    }
    farray<navpoint const*, 10> forward;
    farray<navpoint const*, 10> other;
    for (int i = 0; i < nav_selection_list.size(); i++)
    {
        const navpoint* navp = nav_selection_list[i];
        vector to_navp = navp->pos - current_navp->pos;
        if (to_navp.is_null())
        {
            to_navp = FORWARD_VECTOR;
        }
        else
        {
            to_navp.normalize_safe();
        }
        if (MIN_DOT < to_navp * direction)
        {
            forward.add(navp);
        }
        else
        {
            other.add(navp);
        }
    }
    if (forward.size() > 0)
    {
        return forward[forward.size() == 1 ? 0 : int_rand_range(0, forward.size() - 1)];
    }
    if (other.size() > 0)
    {
        return other[other.size() == 1 ? 0 : int_rand_range(0, other.size() - 1)];
    }
    return nullptr;
}

// 0x00B4EDE0 (:5470)
// Links neither on the new list nor old; failing that, not on the new list; failing that, any.
static const navpoint* find_next_navpoint_direction(const navpoint* current_navp, const vector& direction, const farray<unsigned int, 8>& new_waypoint_list, const ai_pedestrian_info& ped_info)
{
    if (current_navp->num_links == 0)
    {
        return nullptr;
    }
    farray<navpoint const*, 10> candidates;
    for (int i = 0; i < current_navp->num_links; i++)
    {
        const navpoint* link = current_navp->links[i];
        if (!new_waypoint_list.is_member(link->get_handle()) && !ped_info.is_old_waypoint(link->get_handle()))
        {
            candidates.add(link);
        }
    }
    if (candidates.size() < 1)
    {
        for (int i = 0; i < current_navp->num_links; i++)
        {
            const navpoint* link = current_navp->links[i];
            if (!new_waypoint_list.is_member(link->get_handle()))
            {
                candidates.add(link);
            }
        }
    }
    if (candidates.size() < 1)
    {
        for (int i = 0; i < current_navp->num_links; i++)
        {
            candidates.add(current_navp->links[i]);
        }
    }
    return candidates.size() > 0 ? find_next_navpoint_direction(current_navp, candidates, direction) : nullptr;
}

// 0x00B4F000 (:5535)
// LTCG: navp in ESI. The links nearer the player along the network than the node, or farther.
static void find_navpoint_cost_list(const navpoint* navp, farray<navpoint const*, 10>& nav_selection_list, bool lower_cost_desired)
{
    if (navp == nullptr)
    {
        return;
    }
    nav_selection_list.clear();
    if (navp->num_links < 1)
    {
        return;
    }
    const float cost = Cached_node_list.get_travel_distance(navp->get_handle());
    for (int i = 0; i < navp->num_links; i++)
    {
        const navpoint* link = navp->links[i];
        if (link == nullptr)
        {
            continue;
        }
        const float link_cost = Cached_node_list.get_travel_distance(link->get_handle());
        if (lower_cost_desired ? link_cost < cost : cost < link_cost)
        {
            nav_selection_list.add(link);
        }
    }
}

// 0x00B4F0C0 (:5571)
// LTCG: new_waypoint_list in ESI.
static bool build_new_waypoints_directional(const npc* np, const vector& travel_direction, farray<unsigned int, 8>& new_waypoint_list, const navpoint* root_navp, const ai_pedestrian_info& ped_info)
{
    const navpoint* navp = root_navp;
    while (new_waypoint_list.size() < new_waypoint_list.capacity())
    {
        navp = find_next_navpoint_direction(navp, travel_direction, new_waypoint_list, ped_info);
        if (navp == nullptr)
        {
            break;
        }
        new_waypoint_list.add(navp->get_handle());
    }
    return new_waypoint_list.size() > 0;
}

// Towards a point, or forward when already there.
static vector direction_or_forward(const vector& from, const vector& to)
{
    vector direction = to - from;
    if (direction.is_null())
    {
        return FORWARD_VECTOR;
    }
    direction.normalize_safe();
    return direction;
}

// 0x00B4F130 (:5595)
// LTCG: np in ECX, root_navp in EAX. Walks towards the player along the network. Within 30 m along it or 30 m straight
// (900 squared) it goes on in a plain direction: from the start, from the NPC towards the player; later on, as it was heading.
static bool build_new_waypoints_normal(const npc* np, farray<unsigned int, 8>& new_waypoint_list, const navpoint* root_navp, const ai_pedestrian_info& ped_info)
{
    if (root_navp == nullptr || np == nullptr)
    {
        return false;
    }
    new_waypoint_list.clear();
    if (!ped_info.is_old_waypoint(root_navp->get_handle()))
    {
        new_waypoint_list.add(root_navp->get_handle());
    }
    const vector player_pos = player_get_local_player()->pos;
    float cost = Cached_node_list.get_travel_distance(root_navp->get_handle());
    if (cost < 30.0f || vec_dist_squared(player_pos, np->pos) < 900.0f)
    {
        const vector direction = direction_or_forward(np->pos, player_pos);
        return build_new_waypoints_directional(np, direction, new_waypoint_list, root_navp, ped_info);
    }
    const navpoint* current = root_navp;
    farray<navpoint const*, 10> nav_selection_list;
    while (new_waypoint_list.size() < new_waypoint_list.capacity())
    {
        find_navpoint_cost_list(current, nav_selection_list, true);
        const navpoint* next;
        if (nav_selection_list.size() < 1)
        {
            const vector direction = direction_or_forward(player_pos, np->pos);
            next = find_next_navpoint_direction(current, direction, new_waypoint_list, ped_info);
            if (next == nullptr)
            {
                break;
            }
        }
        else
        {
            next = nav_selection_list[nav_selection_list.size() == 1 ? 0 : int_rand_range(0, nav_selection_list.size() - 1)];
        }
        new_waypoint_list.add(next->get_handle());
        cost = Cached_node_list.get_travel_distance(next->get_handle());
        if (cost < 30.0f || vec_dist_squared(player_pos, next->pos) < 900.0f)
        {
            const vector direction = direction_or_forward(current->pos, next->pos);
            return build_new_waypoints_directional(np, direction, new_waypoint_list, next, ped_info);
        }
        current = next;
    }
    return new_waypoint_list.size() > 0;
}

// 0x00B4F520 (:5694)
bool build_new_waypoints_random(const npc* np, farray<unsigned int, 8>& new_waypoint_list, const navpoint* root_navp, const ai_pedestrian_info& ped_info)
{
    if (root_navp == nullptr)
    {
        return false;
    }
    const navpoint* navp = root_navp;
    while (new_waypoint_list.size() < new_waypoint_list.capacity())
    {
        if (navp->num_links < 1)
        {
            return false;
        }
        navp = navp->links[navp->num_links == 1 ? 0 : int_rand_range(0, navp->num_links - 1)];
        if (navp == nullptr)
        {
            return false;
        }
        new_waypoint_list.add(navp->get_handle());
    }
    return true;
}

// 0x00B4F5A0 (:5726)
// Away from the player along the network; with no such link, in the direction of the player.
// Original: every pass starts from the root again (ESI never moves, 0x00B4F723), so this does not walk: the list fills with
// the root's links, and may repeat them.
bool build_new_waypoints_despawn(const npc* np, farray<unsigned int, 8>& new_waypoint_list, const navpoint* root_navp, const ai_pedestrian_info& ped_info)
{
    if (root_navp == nullptr || np == nullptr)
    {
        return false;
    }
    new_waypoint_list.clear();
    if (!ped_info.is_old_waypoint(root_navp->get_handle()))
    {
        new_waypoint_list.add(root_navp->get_handle());
    }
    const player* pp = player_get_local_player();
    farray<navpoint const*, 10> nav_selection_list;
    while (new_waypoint_list.size() < new_waypoint_list.capacity())
    {
        find_navpoint_cost_list(root_navp, nav_selection_list, false);
        if (nav_selection_list.size() < 1)
        {
            const vector direction = direction_or_forward(np->pos, pp->pos);
            const navpoint* next = find_next_navpoint_direction(root_navp, direction, new_waypoint_list, ped_info);
            if (next == nullptr)
            {
                break;
            }
            new_waypoint_list.add(next->get_handle());
        }
        else
        {
            const navpoint* next = nav_selection_list[nav_selection_list.size() == 1 ? 0 : int_rand_range(0, nav_selection_list.size() - 1)];
            new_waypoint_list.add(next->get_handle());
        }
    }
    return new_waypoint_list.size() > 0;
}

// 0x00B5A4B0 (:5791)
bool amb_spawn_build_ped_waypoints(const npc* np, ai_pedestrian_info& ped_info, ped_node_select_modes desired_mode)
{
    const navpoint* root_navp = find_root_ped_node(np, ped_info);
    if (root_navp == nullptr)
    {
        return false;
    }
    farray<unsigned int, 8> new_waypoints;
    bool built;
    switch (desired_mode)
    {
    case PED_NODE_SELECT_MODE_NORMAL:
        built = build_new_waypoints_normal(np, new_waypoints, root_navp, ped_info);
        break;
    case PED_NODE_SELECT_MODE_DESPAWN:
        built = build_new_waypoints_despawn(np, new_waypoints, root_navp, ped_info);
        break;
    case PED_NODE_SELECT_MODE_RANDOM:
        built = build_new_waypoints_random(np, new_waypoints, root_navp, ped_info);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
    if (!built)
    {
        return false;
    }
    ped_info.add_new_waypoints(new_waypoints);
    return true;
}

// 0x00B42D80 (:5838)
const farray<human_info const*, 16>& amb_spawn_desired_guerrilla_list()
{
    return Desired_guerrilla_list;
}

// 0x00B42D90 (:5844)
const farray<vehicle_info const*, 128>& amb_spawn_desired_vehicle_list()
{
    return Desired_vehicle_list;
}
