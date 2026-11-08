// Original: volition/rfg/code/objects/object_convoy.cpp
//
// Convoys (NPC.md NP1): a column of squads driving a road route built from a patrol. The lead squad's position is the
// convoy's, the others follow it through one formation. convoys.xtbl names the definitions.

#include "volition/rfg/code/objects/object_convoy.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/road_route.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy.h"
#include "volition/rfg/code/gameplay/activities/activity_convoy_internal.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_comp.h"

#include <new>

extern gfm_orders_manager Orders_manager;

static const int MAX_CONVOY_MEMBERS = 12;
static const int MAX_CONVOYS = 4; // the instantiator's capacity
static const int MAX_DIFFICULTY_LEVEL = 16;
static const unsigned int CONVOY_STRING_POOL_SIZE = 0x200;
static const unsigned int CONVOY_DEFINITION_SIZE = 0x120; // sizeof(convoy_definition), also rfg.exe's allocation size
static const float CONVOY_MIN_SPEED = 1.0f;
static const float CONVOY_MAX_SPEED = 30.0f;
static const float ROAD_SEARCH_RADIUS = 60.0f;
static const float ROUTE_POINT_MERGE_DIST_SQ = 1.0f; // route points within 1 m collapse into one
static const float GHOST_ARRIVAL_DIST_SQ = 9.0f; // 3 m, simulated squads
static const float VEHICLE_ARRIVAL_DIST_SQ = 400.0f; // 20 m, driven vehicles
static const int ARRIVAL_LOOK_AHEAD_POINTS = 3;
static const float SPAWN_SPACING = 20.0f; // metres of road between two vehicles of a new convoy
static const float ATTACKED_MIN_VEHICLE_DAMAGE = 0.01f;

// ---- file-static globals
static const char CONVOY_STRING_POOL_NAME[19] = "Convoy String Pool"; // 0x013115E4
static const char CONVOY_DEFINITION_TABLE_NAME[13] = "convoys.xtbl"; // 0x013115F8
static const char XML_ACTIVITY_BASE_DATA_TAG_OBJECT_CONVOY[19] = "base_activity_data"; // 0x01311610
// Indexed by the radio lines of activity_convoy_internal.h.
static const char* const Radio_line_xml_tags_object_convoy[NUM_CONVOY_RADIO_LINES] = // 0x01311628
{
    "Start_F",
    "Start_M",
    "Captured_Vehicle",
    "Captured_Vehicle_Res_F",
    "Captured_Vehicle_Res_M",
    "Destroyed_Vehicle",
    "Destroyed_Vehicle_Res_F",
    "Destroyed_Vehicle_Res_M",
    "Delivered_Vehicle_F",
    "Delivered_Vehicle_M",
    "Escaping_Vehicle_F",
    "Escaping_Vehicle_M",
    "Fail_F",
    "Fail_M",
    "Distance_25_F",
    "Distance_25_M",
    "Distance_50_F",
    "Distance_50_M",
    "Distance_75_F",
    "Distance_75_M",
};
static farray<convoy_definition const*,20> Convoy_definitions; // 0x016AD158
static char* Convoy_string_pool_data; // 0x0302F12C
static string_pool Convoy_string_pool(CONVOY_STRING_POOL_NAME); // 0x03067ED8
// Key and value pairs ended by a null key, in rfg.exe's order (0x01076A60, 0x01076AA0).
static str_data_hash_table<unsigned int,32,32,unsigned char> Convoy_role_name_table( // 0x031A2B20
    "Target", (unsigned int)CONVOY_SQUAD_ROLE_TARGET,
    "Defender", (unsigned int)CONVOY_SQUAD_ROLE_DEFENDER,
    "On-Foot Escort", (unsigned int)CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT,
    "Break and Attack", (unsigned int)CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK,
    (const char*)nullptr, 0u);
static str_data_hash_table<unsigned int,32,32,unsigned char> Convoy_type_name_table( // 0x033B53B0
    "Capture", (unsigned int)CONVOY_TYPE_CAPTURE,
    "Destroy", (unsigned int)CONVOY_TYPE_DESTROY,
    "Mission", (unsigned int)CONVOY_TYPE_MISSION,
    (const char*)nullptr, 0u);
static object_pool<object_convoy,MAX_CONVOYS,1> object_convoypool; // 0x035389A0

// ---- internal functions

// 0x00A160A0 (:157)
// LTCG: role in EAX.
static bool is_direct_follower_role(convoy_squad_roles role)
{
    return role == CONVOY_SQUAD_ROLE_TARGET || role == CONVOY_SQUAD_ROLE_DEFENDER;
}

// 0x00A617F0 (:162): -1 when the string is not in the table.
static int string_to_enum(const char* str, str_data_hash_table<unsigned int,32,32,unsigned char>& table)
{
    unsigned int value;
    return table.get(str, value) ? (int)value : -1;
}

// 0x00A8DD90 (:183)
// LTCG: patrol in EAX, the list on the stack.
static bool build_node_list(const object_patrol* patrol, farray<convoy_node_info,512>& convoy_node_list)
{
    farray<navpoint*,512> nav_list;
    const unsigned int root_handle = patrol != nullptr ? patrol->get_root_nav_handle() : 0xFFFFFFFF;
    navpoint* root_nav = root_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_handle) : nullptr;
    if (root_nav == nullptr || !build_patrol_point_list(root_nav, nav_list) || nav_list.size() <= 0)
    {
        return false;
    }
    convoy_node_list.clear();
    for (int i = 0; i < nav_list.size(); i++)
    {
        vector closest_point;
        float u_value;
        convoy_node_info node;
        node.node_navp = nav_list[i];
        node.node_road_id.road_spline = object_road_get_closest_road(node.node_navp->pos, false, closest_point, u_value, ROAD_SEARCH_RADIUS, false, nullptr, nullptr, false);
        if (node.node_road_id.road_spline != nullptr)
        {
            // Original quirk kept: capped at num_points, not at the last point's index.
            const unsigned short point = (unsigned short)fl_round(u_value);
            const unsigned short max_point = (unsigned short)node.node_road_id.road_spline->spline_data.num_points;
            node.node_road_id.point = point > max_point ? max_point : point;
            convoy_node_list.add(node);
        }
    }
    return convoy_node_list.size() > 0;
}

// 0x00A160C0 (:217): inlined at its one use in rfg.exe's merge_segment_list.
// LTCG: merged_segments in EAX, end_segment in EDX, start_segment in ESI.
static bool merge_road_segments(road_segment_info& merged_segments, const road_segment_info& start_segment, const road_segment_info& end_segment)
{
    if (start_segment.road_handle != end_segment.road_handle)
    {
        return false;
    }
    merged_segments.road_handle = start_segment.road_handle;
    merged_segments.start_index = start_segment.start_index;
    merged_segments.end_index = end_segment.end_index;
    return true;
}

// 0x00A8DEC0 (:229)
// LTCG: merged_list in EDI, to_be_merged_list on the stack.
static void merge_segment_list(farray<road_segment_info,128>& merged_list, const farray<road_segment_info,128>& to_be_merged_list)
{
    if (merged_list.size() == 0)
    {
        merged_list.copy(to_be_merged_list);
        return;
    }
    for (int i = 0; i < to_be_merged_list.size(); i++)
    {
        road_segment_info& last = merged_list[merged_list.size() - 1];
        if (!merge_road_segments(last, last, to_be_merged_list[i]))
        {
            merged_list.add(to_be_merged_list[i]);
        }
    }
}

// 0x00A8DF70 (:259): appends the segment's points, walking the road backward when the segment runs against it.
// LTCG: segment_info in EDI, convoy_route on the stack.
static void extract_current_segment(const road_segment_info& segment_info, farray<vector,512>& convoy_route)
{
    const object_path_road* road = segment_info.road_handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(segment_info.road_handle) : nullptr;
    if (road == nullptr)
    {
        return;
    }
    const int start = segment_info.start_index;
    const int end = segment_info.end_index;
    if (start <= end)
    {
        for (int i = start; i <= end; i++)
        {
            convoy_route.add(road->get_road_point((unsigned short)i));
        }
    }
    else
    {
        for (int i = start; i >= end; i--)
        {
            convoy_route.add(road->get_road_point((unsigned short)i));
        }
    }
}

// 0x00A61D00 (:1402): walks the route from start_pos and drops a spawn point every min_distance metres.
static bool build_spawn_point_list(farray<convoy_spawn_info,12>& spawn_point_list, int min_points, const vector& start_pos, const farray<vector,512>& waypoint_list, int& destination_index, const float min_distance)
{
    if (min_points < 1 || destination_index >= waypoint_list.size())
    {
        return false;
    }
    const vector waypoint = waypoint_list[destination_index];
    convoy_spawn_info spawn;
    spawn.spawn_direction = waypoint - start_pos;
    const float dist = spawn.spawn_direction.mag_and_normalize_safe();
    if (dist < min_distance)
    {
        destination_index++;
        return build_spawn_point_list(spawn_point_list, min_points, waypoint, waypoint_list, destination_index, min_distance - dist);
    }
    spawn.spawn_pos = start_pos + spawn.spawn_direction * min_distance;
    spawn.spawn_direction.y = 0.0f;
    spawn.spawn_direction.normalize_safe();
    spawn_point_list.add(spawn);
    if (spawn_point_list.size() >= min_points)
    {
        return true;
    }
    return build_spawn_point_list(spawn_point_list, min_points, spawn.spawn_pos, waypoint_list, destination_index, SPAWN_SPACING);
}

// 0x00A162F0
convoy_spawn_info::convoy_spawn_info()
{
}

// ---- convoy_member_info

// 0x00A094A0 (.h:197)
void convoy_member_info::clear()
{
    squad_info = nullptr;
    squad_handle = 0xFFFFFFFF;
    on_foot_follow_index = -1;
    squad_flags.left_convoy_formation = 0;
}

// 0x00754670 (.h:204)
bool convoy_member_info::in_use() const
{
    return squad_handle != 0xFFFFFFFF;
}

// 0x00754680 (.h:206)
convoy_squad_roles convoy_member_info::get_role() const
{
    return squad_info != nullptr ? squad_info->role : CONVOY_SQUAD_ROLE_UNDEFINED;
}

// ---- convoy_definition

// 0x00A61830 (:296)
convoy_definition::convoy_definition()
    : name(nullptr)
    , difficulty_level(-1)
    , normal_speed(0.0f)
    , evade_speed(0.0f)
    , type(CONVOY_TYPE_UNDEFINED)
    , tech_level_reward(0)
    , tod_spawn_start_time(0)
    , tod_spawn_end_time(0)
{
    for (int i = 0; i < NUM_CONVOY_RADIO_LINES; i++)
    {
        radio_lines[i] = INVALID_VOICE_LINE_HANDLE;
    }
}

// 0x00A39B20 (:313)
int convoy_definition::get_num_vehicles() const
{
    int count = 0;
    for (int i = 0; i < squad_definitions.size(); i++)
    {
        if (squad_definitions[i].vehicle_group != nullptr)
        {
            count++;
        }
    }
    return count;
}

// ---- object_convoy: class

// 0x00A160F0 (:330)
void types_object_convoy(signed char& type, signed char& subtype)
{
    type = OT_CONVOY;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A16100 (:330)
int bpo_index_object_convoy()
{
    return 1;
}

// 0x00A16110 (:330)
const char* classname_object_convoy()
{
    return "object_convoy";
}

// 0x00A61910 (:330)
object* alloc_object_convoy()
{
    return object_convoypool.alloc(0);
}

// Magic numbers: rfg.exe's arguments, the pool's bytes and capacity (0x01076BCB, 0x01076BD6).
static obj_class_instantiator object_convoy_instantiator("object_convoy", alloc_object_convoy, types_object_convoy, bpo_index_object_convoy, classname_object_convoy, object_convoy::pool_remaining, 0x66C0, MAX_CONVOYS); // 0x0353F09C

// 0x005DDB90 (.h:217)
const object_sub_type object_convoy::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A094C0 (.h:217)
const char* object_convoy::get_classname_static()
{
    return "object_convoy";
}

// 0x00A094D0 (.h:217)
int object_convoy::get_bpo_index_static()
{
    return 1;
}

// 0x00A61960 (.h:217)
const char* object_convoy::get_classname() const
{
    return "object_convoy";
}

// 0x00A16120 (:330)
unsigned int object_convoy::get_classname_hash()
{
    return object_convoy_instantiator.this_hash;
}

// 0x00A39B50 (:330)
int object_convoy::pool_remaining()
{
    return object_convoypool.get_free();
}

// 0x00A8E030 (:330)
void object_convoy::pool_free()
{
    object_convoypool.free(this);
}

// 0x00A61920 (:331)
object_convoy::object_convoy()
{
}

// 0x00A61970
object_convoy::~object_convoy()
{
}

// 0x00AA5910 (:336)
bool object_convoy::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(CONVOY_PROP_DEFINITION, base_convoy_definition->name);
        pblock.set(CONVOY_PROP_PATROL, original_patrol_handle);
        return true;
    }

    obj_flags.no_save = 1;
    base_convoy_definition = nullptr;
    main_formation_handle = 0xFFFFFFFF;
    next_visited_index = 0;
    convoy_route.clear();
    start_pos.zero();
    end_pos.zero();
    flags.spawned = false;
    flags.attacked = false;
    flags.active_spawn = false;
    char definition_name[256];
    pblock.get(CONVOY_PROP_DEFINITION, definition_name);
    base_convoy_definition = convoy_definition_from_name(definition_name);
    if (base_convoy_definition == nullptr)
    {
        return false;
    }
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        member_info[i].clear();
        if (i < base_convoy_definition->squad_definitions.size())
        {
            member_info[i].squad_info = &base_convoy_definition->squad_definitions[i];
        }
    }
    pblock.get(CONVOY_PROP_PATROL, original_patrol_handle);
    if (get_patrol() == nullptr)
    {
        return false;
    }
    vector closest_point;
    float u_value;
    if (object_road_get_closest_road(pos, false, closest_point, u_value, ROAD_SEARCH_RADIUS, false, nullptr, nullptr, false) == nullptr)
    {
        return false;
    }
    pos = closest_point;
    return build_route();
}

// 0x00A8E040 (:411)
void object_convoy::get_bbox(vector& bmin, vector& bmax) const
{
    const vehicle* vp = get_lead_vehicle();
    if (vp != nullptr)
    {
        vp->get_bbox(bmin, bmax);
        return;
    }
    const vector half(1.0f, 1.0f, 1.0f);
    bmin = pos - half;
    bmax = pos + half;
}

// 0x00A8E140 (:425)
void object_convoy::get_local_bbox(vector& bmin, vector& bmax) const
{
    const vehicle* vp = get_lead_vehicle();
    if (vp != nullptr)
    {
        vp->get_local_bbox(bmin, bmax);
        return;
    }
    bmin.set(-1.0f, -1.0f, -1.0f);
    bmax.set(1.0f, 1.0f, 1.0f);
}

// 0x00A8E1B0 (:438): a squad nobody can see goes with the convoy, a real one stays in the world and stops being persistent.
void object_convoy::free_this()
{
    Cartographer.remove_icon(get_handle());
    Orders_manager.destroy_all_orders_with_target(get_handle());
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        object_squad* squad = get_member_squad(i);
        if (squad == nullptr)
        {
            continue;
        }
        if (squad->simulated())
        {
            World.destroy_object(squad, false);
        }
        else
        {
            squad->set_persistent(false);
        }
    }
    object::free_this();
}

// 0x00A094E0 (.h:232)
const convoy_definition* object_convoy::get_definition() const
{
    return base_convoy_definition;
}

// 0x00602CF0 (.h:233)
int object_convoy::get_base_convoy_size() const
{
    return base_convoy_definition->squad_definitions.size();
}

// 0x00A094F0 (.h:239)
bool object_convoy::spawned() const
{
    return flags.spawned;
}

// 0x005DDBA0 (.h:275)
unsigned int object_convoy::get_main_formation_handle() const
{
    return main_formation_handle;
}

// 0x00754690 (.h:289)
const farray<vector,512>& object_convoy::get_route() const
{
    return convoy_route;
}

// 0x005DDBB0 (.h:298)
bool object_convoy::attacked() const
{
    return flags.attacked;
}

// 0x007546A0 (.h:301)
void object_convoy::set_attacked_flag()
{
    flags.attacked = true;
}

// 0x00B1FC60 (.h:304)
unsigned int object_convoy::get_patrol_handle() const
{
    return original_patrol_handle;
}

// ---- object_convoy: route

// 0x00A8E250 (:470)
bool object_convoy::build_route()
{
    if (convoy_route.size() != 0)
    {
        return true;
    }
    farray<convoy_node_info,512> node_list;
    farray<road_segment_info,128> merged_list;
    farray<road_segment_info,128> segment_list;
    if (!build_node_list(get_patrol(), node_list))
    {
        return false;
    }
    if (node_list.size() <= 1)
    {
        return false;
    }
    for (int i = 1; i < node_list.size(); i++)
    {
        if (road_route_create(node_list[i - 1].node_road_id, node_list[i].node_road_id, segment_list))
        {
            merge_segment_list(merged_list, segment_list);
        }
    }
    if (merged_list.size() <= 0)
    {
        return false;
    }
    convoy_route.clear();
    for (int i = 0; i < merged_list.size(); i++)
    {
        extract_current_segment(merged_list[i], convoy_route);
    }
    if (convoy_route.size() <= 0)
    {
        return false;
    }
    int index = 1;
    while (index < convoy_route.size())
    {
        if (vec_dist_squared(convoy_route[index - 1], convoy_route[index]) <= ROUTE_POINT_MERGE_DIST_SQ)
        {
            convoy_route.remove(index);
        }
        else
        {
            index++;
        }
    }
    start_pos = convoy_route[0];
    end_pos = convoy_route[convoy_route.size() - 1];
    return true;
}

// 0x00A39DD0 (:1048): the next point counts as visited once within range; looks three points ahead.
void object_convoy::update_destination_ghost(bool simulated)
{
    const float arrival_dist_sq = simulated ? GHOST_ARRIVAL_DIST_SQ : VEHICLE_ARRIVAL_DIST_SQ;
    for (int i = 0; i < ARRIVAL_LOOK_AHEAD_POINTS; i++)
    {
        const int index = next_visited_index + i;
        if (index >= convoy_route.size())
        {
            return;
        }
        if (vec_dist_squared(pos, convoy_route[index]) <= arrival_dist_sq)
        {
            next_visited_index = index + 1;
            return;
        }
    }
}

// 0x00A39E60 (:1083)
void object_convoy::update_destination(const vector& new_pos, bool simulated)
{
    pos = new_pos;
    update_destination_ghost(simulated);
}

// 0x00A39E80 (:1093)
void object_convoy::find_next_destination()
{
    next_visited_index = -1;
    if (convoy_route.size() <= 0)
    {
        return;
    }
    int closest = 0;
    float closest_dist_sq = vec_dist_squared(pos, convoy_route[0]);
    for (int i = 1; i < convoy_route.size(); i++)
    {
        const float dist_sq = vec_dist_squared(pos, convoy_route[i]);
        if (dist_sq < closest_dist_sq)
        {
            closest = i;
            closest_dist_sq = dist_sq;
        }
    }
    next_visited_index = closest;
}

// 0x00A39F10 (:1119)
bool object_convoy::get_ghost_destination(const vector& my_pos, vector& dest_pos)
{
    update_destination(my_pos, true);
    if (next_visited_index < 0 || next_visited_index >= convoy_route.size())
    {
        return false;
    }
    dest_pos = convoy_route[next_visited_index];
    return true;
}

// 0x00A39F70 (:1138): past the last point the answer stays on the last point.
bool object_convoy::get_next_destination(const vector& my_pos, vector& dest_pos)
{
    update_destination(my_pos, false);
    if (next_visited_index < 0 || next_visited_index > convoy_route.size())
    {
        return false;
    }
    dest_pos = convoy_route[next_visited_index == convoy_route.size() ? next_visited_index - 1 : next_visited_index];
    return true;
}

// 0x00A16250 (:1157)
int object_convoy::get_next_visited_index() const
{
    return next_visited_index;
}

// 0x00A39FE0 (:1164)
bool object_convoy::is_at_final_destination()
{
    return convoy_route.size() == next_visited_index;
}

// 0x00A3A1B0 (:1869)
const object_patrol* object_convoy::get_patrol() const
{
    return original_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(original_patrol_handle) : nullptr;
}

// ---- object_convoy: members

// 0x00A39B60 (:620)
int object_convoy::get_num_squads() const
{
    int count = 0;
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        const object_squad* squad = get_member_squad(i);
        if (squad != nullptr && !squad->is_killed())
        {
            count++;
        }
    }
    return count;
}

// 0x00A8E4B0 (:636)
int object_convoy::get_num_vehicles() const
{
    int count = 0;
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        if (vehicle_squad_is_valid(get_member_squad(i)))
        {
            count++;
        }
    }
    return count;
}

// 0x00AA5AD0 (:651)
void object_convoy::evaluate_squad_spawn()
{
    if (flags.active_spawn)
    {
        evaluate_squad_spawn_active();
    }
    else
    {
        evaluate_squad_spawn_inactive();
    }
}

// 0x00A39BB0 (:662): stops once every member is simulated again.
void object_convoy::evaluate_squad_spawn_active()
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        const object_squad* squad = get_member_squad(i);
        if (squad != nullptr && !squad->simulated())
        {
            return;
        }
    }
    flags.active_spawn = false;
}

// 0x00A8E500 (:688): starts when every member has its assets and wants to spawn.
void object_convoy::evaluate_squad_spawn_inactive()
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        const object_squad* squad = get_member_squad(i);
        if (squad != nullptr && (!squad->squad_assets_ready() || !squad->should_attempt_spawn()))
        {
            return;
        }
    }
    flags.active_spawn = true;
}

// 0x00A619C0 (:715)
bool object_convoy::convoy_assets_ready() const
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        const object_squad* squad = get_member_squad(i);
        if (squad != nullptr && !squad->squad_assets_ready())
        {
            return false;
        }
    }
    return true;
}

// 0x00A8E560 (:737)
int object_convoy::get_num_target_vehicles() const
{
    int count = 0;
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        if (member_info[i].squad_handle != 0xFFFFFFFF && member_info[i].get_role() == CONVOY_SQUAD_ROLE_TARGET && vehicle_squad_is_valid(get_member_squad(i)))
        {
            count++;
        }
    }
    return count;
}

// 0x00A39C00 (:756)
object_squad* object_convoy::get_primary_target_squad() const
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        if (member_info[i].squad_handle != 0xFFFFFFFF && member_info[i].get_role() == CONVOY_SQUAD_ROLE_TARGET)
        {
            return get_member_squad(i);
        }
    }
    return nullptr;
}

// 0x00A61A10 (:772)
vehicle* object_convoy::get_primary_target_vehicle() const
{
    const object_squad* squad = get_primary_target_squad();
    if (squad == nullptr || squad->squad_vehicle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return World.object_from_handle<vehicle>(squad->squad_vehicle);
}

// 0x00A16130 (:794)
int object_convoy::get_member_index(unsigned int member_squad_handle) const
{
    if (member_squad_handle != 0xFFFFFFFF)
    {
        for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
        {
            if (member_info[i].squad_handle == member_squad_handle)
            {
                return i;
            }
        }
    }
    return -1;
}

// 0x00A39C60 (:811)
object_squad* object_convoy::get_member_squad(int index) const
{
    const unsigned int squad_handle = member_info[index].squad_handle;
    return squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
}

// 0x00A16160 (:820)
float object_convoy::get_move_speed() const
{
    if (flags.attacked || gsm_get_alert_level() > 0)
    {
        return base_convoy_definition->evade_speed;
    }
    return base_convoy_definition->normal_speed;
}

// 0x00A39C90 (:840): the first living member still in formation.
object_squad* object_convoy::get_lead_squad() const
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        if (member_info[i].squad_handle != 0xFFFFFFFF && !member_info[i].squad_flags.left_convoy_formation)
        {
            object_squad* squad = get_member_squad(i);
            if (squad != nullptr && !squad->is_killed())
            {
                return squad;
            }
        }
    }
    return nullptr;
}

// 0x00A39CF0 (:861)
unsigned int object_convoy::get_lead_squad_handle() const
{
    const object_squad* squad = get_lead_squad();
    return squad != nullptr ? squad->get_handle() : 0xFFFFFFFF;
}

// 0x00A161A0 (:868)
bool object_convoy::is_convoy_member(unsigned int squad_handle) const
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        if (member_info[i].squad_handle == squad_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00A61A90 (:882)
vehicle* object_convoy::get_lead_vehicle() const
{
    const object_squad* squad = get_lead_squad();
    if (squad == nullptr || squad->squad_vehicle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return World.object_from_handle<vehicle>(squad->squad_vehicle);
}

// 0x00A39D10 (:897): Original quirk kept: rfg.exe's loop over the members has an empty body, so the answer is always -1.
float object_convoy::get_dist_sq_to_nearest_following_member(const vector& check_pos, unsigned int exclude_squad) const
{
    return -1.0f;
}

// 0x00A39D40 (:942)
unsigned int object_convoy::join_convoy_formation(object_squad* squad)
{
    const int index = get_member_index(squad->get_handle());
    if (index == -1 || member_info[index].get_role() == CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT)
    {
        return 0xFFFFFFFF;
    }
    if (main_formation_handle != 0xFFFFFFFF && ai_formation_is_following(squad, main_formation_handle))
    {
        return main_formation_handle;
    }
    const object_squad* lead = get_lead_squad();
    return lead != nullptr ? ai_formation_follow(squad, lead, AI_FORMATION_FOLLOW_THE_LEADER) : 0xFFFFFFFF;
}

// 0x00A161D0 (:976): moves a member behind the last used slot, closing the gap. Answers its new index.
int object_convoy::rotate_to_back(int index)
{
    if (index < 0 || index >= MAX_CONVOY_MEMBERS)
    {
        return -1;
    }
    const convoy_member_info moved = member_info[index];
    int slot = index;
    while (slot < MAX_CONVOY_MEMBERS - 1 && member_info[slot + 1].squad_handle != 0xFFFFFFFF)
    {
        member_info[slot] = member_info[slot + 1];
        slot++;
    }
    // Original quirk kept: when the next slot is free the member lands in it, leaving the slot it left as it was.
    if (slot < MAX_CONVOY_MEMBERS - 1)
    {
        slot++;
    }
    member_info[slot] = moved;
    return slot;
}

// 0x00A61AC0 (:1012)
// Original quirk kept (asm 0x00A61B1C): a squad that was in the formation returns right after leaving it, so neither the
// formation update nor the activity's notice runs for it.
bool object_convoy::leave_convoy_formation(unsigned int squad_handle)
{
    const int index = get_member_index(squad_handle);
    if (index < 0)
    {
        return false;
    }
    if (!member_info[index].squad_flags.left_convoy_formation)
    {
        member_info[index].squad_flags.left_convoy_formation = 1;
        if (ai_formation_is_member(main_formation_handle, squad_handle))
        {
            return ai_formation_remove_follower(main_formation_handle, squad_handle);
        }
        update_formation();
        activity_convoy_squad_left_convoy(squad_handle);
    }
    return true;
}

// 0x00A16260 (:1170)
bool object_convoy::in_convoy_formation(unsigned int squad_handle) const
{
    const int index = get_member_index(squad_handle);
    return index != -1 && !member_info[index].squad_flags.left_convoy_formation;
}

// 0x00A162A0 (:1190)
convoy_squad_roles object_convoy::get_squad_role(unsigned int squad_handle) const
{
    const int index = get_member_index(squad_handle);
    return index != -1 ? member_info[index].get_role() : CONVOY_SQUAD_ROLE_UNDEFINED;
}

// 0x00A3A000 (:1208)
object_squad* object_convoy::get_on_foot_follow_squad(unsigned int squad_handle) const
{
    const int index = get_member_index(squad_handle);
    if (index == -1 || member_info[index].get_role() != CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT)
    {
        return nullptr;
    }
    const int follow_index = member_info[index].on_foot_follow_index;
    return follow_index != -1 ? get_member_squad(follow_index) : nullptr;
}

// 0x00A16300 (:1442)
const convoy_member_info* object_convoy::get_convoy_member_info(int index) const
{
    return index >= 0 && index < MAX_CONVOY_MEMBERS ? &member_info[index] : nullptr;
}

// ---- object_convoy: per frame

// 0x00AB3C60 (:1231): do_frame without its state result, and a convoy at its last route point or without squads goes away.
void convoy_do_frame()
{
    const int count = World.get_num_objects<object_convoy>();
    for (int i = 0; i < count; i++)
    {
        object_convoy* convoy = World.get_object<object_convoy>(i);
        if ((!convoy->flags.spawned && !convoy->spawn()) || !convoy->remove_dead_squads())
        {
            World.destroy_object(convoy, false);
            continue;
        }
        if (convoy->update_formation() && convoy->get_lead_squad() != nullptr)
        {
            convoy->update_pos_and_orient_from_lead_vehicle();
            convoy->update_attacked_status();
            convoy->update_break_status();
        }
        if (convoy->convoy_route.size() == convoy->next_visited_index)
        {
            activity_convoy_at_destination(convoy);
            World.destroy_object(convoy, false);
        }
    }
}

// 0x00AA5AF0 (:1255): at most one squad per call spawns its members.
void object_convoy::do_squad_member_spawning()
{
    if (!flags.active_spawn)
    {
        return;
    }
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        object_squad* squad = get_member_squad(i);
        if (squad != nullptr && squad->should_attempt_spawn() && squad->spawn_members(false, nullptr))
        {
            return;
        }
    }
}

// 0x00AA5B50 (:1289)
convoy_state object_convoy::do_frame()
{
    if (!flags.spawned && !spawn())
    {
        return CONVOY_STATE_NO_SQUADS;
    }
    if (!remove_dead_squads())
    {
        return CONVOY_STATE_NO_SQUADS;
    }
    if (!update_formation() || !update_pos_and_orient_from_lead_vehicle())
    {
        return CONVOY_STATE_NO_LEADER;
    }
    update_attacked_status();
    update_break_status();
    return CONVOY_STATE_RUNNING;
}

// 0x00A61B50 (:1358)
bool object_convoy::spawn_squad_at_point(int index, const vector& squad_pos, const matrix& squad_orient)
{
    const convoy_squad_definition* squad_info = member_info[index].squad_info;
    if (squad_info == nullptr)
    {
        return false;
    }
    squad_spawn_parameters params;
    params.orders.location_target = pos;
    params.orders.object_target = get_handle();
    params.orders.orders_type = GFM_ORDERS_TYPE_CONVOY;
    params.orders.team = convoy_definition_get_team(base_convoy_definition);
    params.squad.squad_pos = squad_pos;
    params.squad.squad_orient = squad_orient;
    params.squad.squad_def = squad_info->squad_def;
    params.squad.squad_flags.disallow_reassignment = 1;
    params.squad.veh_spawn_group = squad_info->vehicle_group;
    if (base_convoy_definition->type == CONVOY_TYPE_DESTROY)
    {
        params.squad.vflags.disable_for_player = 1;
    }
    object_squad* squad = params.spawn_squad();
    if (squad == nullptr)
    {
        return false;
    }
    member_info[index].squad_handle = squad->get_handle();
    squad->set_persistent(true);
    squad->npc_flags.convoy_squad = 1;
    squad->vflags.convoy_vehicle = 1;
    return true;
}

// 0x00A8E5C0 (:1453)
bool object_convoy::spawn_squads()
{
    if (!flags.spawned)
    {
        flags.spawned = spawn();
    }
    return flags.spawned;
}

// 0x00A61F20 (:1468): vehicles take the spawn points from the front of the list back, then the on-foot escorts do the same.
bool object_convoy::spawn()
{
    int min_points = 0;
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        if (member_info[i].squad_info != nullptr && member_info[i].squad_info->role != CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT)
        {
            min_points++;
        }
    }
    next_visited_index = 1;
    farray<convoy_spawn_info,12> spawn_points;
    if (!build_spawn_point_list(spawn_points, min_points, convoy_route[0], convoy_route, next_visited_index, 0.0f))
    {
        return false;
    }
    for (int pass = 0; pass < 2; pass++)
    {
        const bool on_foot = pass == 1;
        int point = spawn_points.size() - 1;
        for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
        {
            const convoy_squad_definition* squad_info = member_info[i].squad_info;
            if (squad_info == nullptr || (squad_info->role == CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT) != on_foot)
            {
                continue;
            }
            matrix spawn_orient;
            spawn_orient.make_quick(spawn_points[point].spawn_direction);
            if (!spawn_squad_at_point(i, spawn_points[point].spawn_pos, spawn_orient))
            {
                return false;
            }
            point = point > 0 ? point - 1 : 0; // the rest share the last point
        }
    }
    if (get_lead_squad() == nullptr || get_patrol() == nullptr)
    {
        return false;
    }
    flags.spawned = true;
    return true;
}

// 0x00A8E5F0 (:1561)
void object_convoy::remove_squad_from_convoy(int index, object_squad* squad)
{
    convoy_member_info& member = member_info[index];
    const convoy_squad_roles role = member.get_role();
    if (squad == nullptr && member.squad_handle != 0xFFFFFFFF)
    {
        squad = World.object_from_handle<object_squad>(member.squad_handle);
    }
    member.clear();
    if (squad == nullptr)
    {
        return;
    }
    if (main_formation_handle != 0xFFFFFFFF && is_direct_follower_role(role) && ai_formation_get_leader(main_formation_handle) != squad)
    {
        ai_formation_remove_follower(main_formation_handle, squad->get_handle());
    }
    remove_child(squad);
    gfm_orders* orders = Orders_manager.get_orders_from_handle(squad->get_orders_handle());
    if (orders != nullptr)
    {
        Orders_manager.destroy_orders(orders);
    }
}

// 0x00A8E710 (:1592)
bool object_convoy::squad_is_valid(convoy_squad_roles role, const object_squad* const squad) const
{
    return role == CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT ? on_foot_squad_is_valid(squad) : vehicle_squad_is_valid(squad);
}

// 0x00A620F0 (:1609)
bool object_convoy::vehicle_squad_is_valid(const object_squad* squad) const
{
    if (squad == nullptr || squad->is_killed())
    {
        return false;
    }
    if (squad->simulated())
    {
        return true;
    }
    vehicle* vp = squad->squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad->squad_vehicle) : nullptr;
    if (vp == nullptr || vp->is_killed())
    {
        return false;
    }
    human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
    if (driver == nullptr)
    {
        return ai_vehicle_is_usable(vp, nullptr, AI_VEHICLE_SEAT_TYPE_DRIVER);
    }
    const npc* np = World.object_cast<npc>(driver);
    return np != nullptr && np->get_ai()->get_squad_handle() == squad->get_handle();
}

// 0x00A16320 (:1652)
bool object_convoy::on_foot_squad_is_valid(const object_squad* squad) const
{
    return squad != nullptr && !squad->is_killed();
}

// 0x00A8E750 (:1665)
bool object_convoy::remove_dead_squads()
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        const object_squad* squad = get_member_squad(i);
        if (squad == nullptr || squad->is_killed())
        {
            member_info[i].clear();
        }
    }
    return get_num_vehicles() > 0;
}

// 0x00A3A080 (:1690): rebuilds the formation when the lead squad changed.
bool object_convoy::update_formation()
{
    const object_squad* lead = get_lead_squad();
    if (lead == nullptr)
    {
        return false;
    }
    if (main_formation_handle != 0xFFFFFFFF && ai_formation_get_leader_handle(main_formation_handle) == lead->get_handle())
    {
        return true;
    }
    if (main_formation_handle != 0xFFFFFFFF)
    {
        ai_formation_remove_all_followers(main_formation_handle);
        main_formation_handle = 0xFFFFFFFF;
    }
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        if (member_info[i].squad_handle == lead->get_handle() || member_info[i].squad_flags.left_convoy_formation)
        {
            continue;
        }
        object_squad* squad = get_member_squad(i);
        if (squad != nullptr && !squad->is_killed())
        {
            const unsigned int formation = ai_formation_follow(squad, lead, AI_FORMATION_FOLLOW_THE_LEADER);
            if (main_formation_handle == 0xFFFFFFFF)
            {
                main_formation_handle = formation;
            }
        }
    }
    return true;
}

// 0x00A621A0 (:1751): a simulated lead squad gives its own position, a real one its vehicle's. Answers false without a lead.
bool object_convoy::update_pos_and_orient_from_lead_vehicle()
{
    const object_squad* lead = get_lead_squad();
    if (lead == nullptr)
    {
        return false;
    }
    const object* source = lead;
    if (!lead->simulated())
    {
        source = lead->squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(lead->squad_vehicle) : nullptr;
        if (source == nullptr)
        {
            return true;
        }
    }
    update_pos_and_orient(source->pos, source->orient, true);
    return true;
}

// 0x00A3A150 (:1778): once every member is simulated and out of formation, they may all form up again.
void object_convoy::update_break_status()
{
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        const object_squad* squad = get_member_squad(i);
        if (squad != nullptr && (!squad->simulated() || !member_info[i].squad_flags.left_convoy_formation))
        {
            return;
        }
    }
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        member_info[i].squad_flags.left_convoy_formation = 0;
    }
}

// 0x00A8E7C0 (:1801): attacked takes a damaged vehicle and a leader engaging the player, not necessarily of the same squad.
void object_convoy::update_attacked_status()
{
    if (flags.attacked)
    {
        return;
    }
    bool vehicle_damaged = false;
    bool engaging_player = false;
    for (int i = 0; i < MAX_CONVOY_MEMBERS; i++)
    {
        const object_squad* squad = get_member_squad(i);
        if (squad == nullptr)
        {
            continue;
        }
        const vehicle* vp = squad->squad_vehicle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(squad->squad_vehicle) : nullptr;
        if (vp != nullptr && vp->get_vehicle_damage() >= ATTACKED_MIN_VEHICLE_DAMAGE)
        {
            vehicle_damaged = true;
        }
        const npc* leader = squad->get_leader();
        if (leader != nullptr && leader->get_ai() != nullptr && leader->get_ai()->is_engaging_player())
        {
            engaging_player = true;
        }
    }
    flags.attacked = vehicle_damaged && engaging_player;
}

// ---- definitions

// Reads one member of a convoy; false when the entry is skipped (rfg.exe formats a message nothing reads).
static bool parse_convoy_squad(convoy_squad_definition& member, xml_element* element, int& num_targets)
{
    member.squad_def = squad_definition_from_name(xml_table_get_req_string_ref(element, "Squad"));
    if (member.squad_def == nullptr)
    {
        return false;
    }
    member.role = (convoy_squad_roles)string_to_enum(xml_table_get_req_string_ref(element, "Role"), Convoy_role_name_table);
    if (member.role == CONVOY_SQUAD_ROLE_UNDEFINED)
    {
        return false;
    }
    const char* group_name = xml_table_get_opt_string_ref(element, "VehicleGroup");
    if (group_name == nullptr)
    {
        // A squad without a vehicle can only be an on-foot escort.
        member.vehicle_group = nullptr;
        return member.role == CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT;
    }
    if (member.role == CONVOY_SQUAD_ROLE_ON_FOOT_ESCORT)
    {
        return false;
    }
    member.vehicle_group = sog_get_vehicle_group(group_name);
    if (member.vehicle_group == nullptr)
    {
        return false;
    }
    if (member.role == CONVOY_SQUAD_ROLE_TARGET)
    {
        num_targets++;
    }
    return true;
}

// 0x00A8E900 (:1971)
void convoy_definitions_init()
{
    if (Convoy_string_pool_data == nullptr)
    {
        Convoy_string_pool_data = (char*)Permanent_mempool.alloc(CONVOY_STRING_POOL_SIZE, 0);
        Convoy_string_pool.init(Convoy_string_pool_data, CONVOY_STRING_POOL_SIZE, CONVOY_STRING_POOL_NAME, 0, false);
        Convoy_string_pool.clear();
    }
    xml_element* root = xml_table_parse(CONVOY_DEFINITION_TABLE_NAME);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = root->elements; cur != nullptr && Convoy_definitions.size() < Convoy_definitions.capacity(); cur = cur->next)
    {
        const char* name = Convoy_string_pool.add_unique(xml_table_get_req_string_ref(cur, "Name"));
        if (name == nullptr)
        {
            break;
        }
        const char* base_data_name = xml_table_get_opt_string_ref(cur, XML_ACTIVITY_BASE_DATA_TAG_OBJECT_CONVOY);
        const base_activity_data* base_data = base_data_name != nullptr ? activity_data_find_handle(base_data_name) : nullptr;

        farray<convoy_squad_definition,12> squads;
        int num_targets = 0;
        for (xml_element* element = xml_table_find_req(cur, "MemberSquads")->elements; element != nullptr && squads.size() < squads.capacity(); element = element->next)
        {
            convoy_squad_definition member;
            if (parse_convoy_squad(member, element, num_targets))
            {
                squads.add(member);
            }
        }
        // A convoy needs squads and at least one target vehicle.
        if (squads.size() < 1 || num_targets < 1)
        {
            continue;
        }

        int difficulty = 0;
        xml_table_get_req_int(difficulty, cur, "difficulty");
        if (difficulty < 0)
        {
            difficulty = 0;
        }
        else if (difficulty > MAX_DIFFICULTY_LEVEL)
        {
            difficulty = MAX_DIFFICULTY_LEVEL;
        }
        int tech_reward = 0;
        xml_table_get_opt_int(tech_reward, cur, "Tech_Reward");

        voice_line_handle radio_lines[NUM_CONVOY_RADIO_LINES];
        for (int i = 0; i < NUM_CONVOY_RADIO_LINES; i++)
        {
            radio_lines[i] = INVALID_VOICE_LINE_HANDLE;
        }
        xml_element* radio_element = xml_table_find_opt(cur, "Radio_Lines");
        if (radio_element != nullptr)
        {
            for (int i = 0; i < NUM_CONVOY_RADIO_LINES; i++)
            {
                const char* line_name = xml_table_get_opt_string_ref(radio_element, Radio_line_xml_tags_object_convoy[i]);
                if (line_name != nullptr)
                {
                    radio_lines[i] = voice_line_handle_from_name(line_name);
                }
            }
        }

        // Original quirk kept: an unknown type only formats a message; the definition stays with CONVOY_TYPE_UNDEFINED.
        const convoy_type type = (convoy_type)string_to_enum(xml_table_get_req_string_ref(cur, "Type"), Convoy_type_name_table);

        float normal_speed = 0.0f;
        float evade_speed = 0.0f;
        xml_table_get_req_float(normal_speed, cur, "NormalSpeed");
        xml_table_get_req_float(evade_speed, cur, "EvadeSpeed");
        normal_speed = normal_speed < CONVOY_MIN_SPEED ? CONVOY_MIN_SPEED : (normal_speed >= CONVOY_MAX_SPEED ? CONVOY_MAX_SPEED : normal_speed);
        evade_speed = evade_speed < CONVOY_MIN_SPEED ? CONVOY_MIN_SPEED : (evade_speed >= CONVOY_MAX_SPEED ? CONVOY_MAX_SPEED : evade_speed);

        int tod_start = 0;
        int tod_end = 0;
        xml_element* tod_element = xml_find(cur, "tod_info");
        if (tod_element != nullptr)
        {
            xml_table_get_req_int(tod_start, tod_element, "spawn_start_time");
            xml_table_get_req_int(tod_end, tod_element, "spawn_end_time");
        }

        void* memory = Permanent_mempool.alloc(CONVOY_DEFINITION_SIZE, 4);
        if (memory == nullptr)
        {
            // Original: reports the out of memory and leaves the table open.
            return;
        }
        convoy_definition* definition = new (memory) convoy_definition();
        definition->name = name;
        definition->squad_definitions.copy(squads);
        definition->difficulty_level = difficulty;
        definition->normal_speed = normal_speed;
        definition->evade_speed = evade_speed;
        definition->type = type;
        definition->base_data = base_data;
        definition->tech_level_reward = tech_reward;
        definition->tod_spawn_start_time = tod_start;
        definition->tod_spawn_end_time = tod_end;
        load_standard_rewards(definition->standard_rewards, cur);
        for (int i = 0; i < NUM_CONVOY_RADIO_LINES; i++)
        {
            definition->radio_lines[i] = radio_lines[i];
        }
        Convoy_definitions.add(definition);
    }
    xml_table_close(root);
}

// 0x00A3A1E0 (:2204)
const convoy_definition* convoy_definition_from_name(const char* name)
{
    const int index = name != nullptr ? convoy_definition_get_index_from_name(name) : -1;
    return index != -1 ? Convoy_definitions[index] : nullptr;
}

// 0x00A3A230 (:2225)
const convoy_definition* convoy_definition_from_index(int index)
{
    return index >= 0 && index < Convoy_definitions.size() ? Convoy_definitions[index] : nullptr;
}

// 0x00A3A250 (:2238)
int convoy_definition_get_index_from_name(const char* name)
{
    for (int i = 0; i < Convoy_definitions.size(); i++)
    {
        if (v_stricmp(Convoy_definitions[i]->name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A3A290 (:2257)
int convoy_definition_get_index(const convoy_definition* convoy_def)
{
    for (int i = 0; i < Convoy_definitions.size(); i++)
    {
        if (Convoy_definitions[i] == convoy_def)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A3A2C0 (:2275): the first squad's team speaks for the convoy.
human_teams convoy_definition_get_team(const convoy_definition* convoy_def)
{
    const squad_definition* squad_def = convoy_def->squad_definitions[0].squad_def;
    return squad_def != nullptr ? squad_def->squad_team : HUMAN_TEAM_NONE;
}

// 0x00A8F0B0 (:2281)
object_patrol* convoy_find_patrol(const object_convoy_end_point* objp)
{
    farray<object_patrol*,16> patrols;
    world_collect_descendants<object_patrol>(objp, patrols, nullptr, nullptr);
    return patrols.size() > 0 ? patrols[0] : nullptr;
}

// 0x00AA5BB0 (:2296)
object_convoy* convoy_create(const object_convoy_end_point* convoy_objp, const convoy_definition* convoy_def, const char* name)
{
    if (convoy_objp == nullptr)
    {
        return nullptr;
    }
    const object_patrol* patrol = convoy_find_patrol(convoy_objp);
    return patrol != nullptr ? convoy_create(patrol, convoy_def, name) : nullptr;
}

// 0x00A8F100 (:2311)
object_convoy* convoy_create(const object_patrol* patrol, const convoy_definition* convoy_def, const char* name)
{
    if (patrol == nullptr || convoy_def == nullptr)
    {
        return nullptr;
    }
    const unsigned int root_handle = patrol->get_root_nav_handle();
    const navpoint* root = root_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_handle) : nullptr;
    if (root == nullptr)
    {
        return nullptr;
    }
    obj_prop_block block(root->pos, patrol->orient, name);
    block.set(CONVOY_PROP_DEFINITION, convoy_def->name);
    block.set(CONVOY_PROP_PATROL, patrol->get_handle());
    object_convoy* convoy = World.create_object<object_convoy>(block, 0xFFFFFFFF);
    if (convoy != nullptr)
    {
        convoy->set_persistent(true);
    }
    // Original bug kept: a failed creation (a full pool, or no route) reads through null here.
    if (!convoy->spawn_squads())
    {
        World.destroy_object(convoy, false);
        return nullptr;
    }
    return convoy;
}

// 0x00A3A2E0 (:2347)
void console_print_available_convoys()
{
    console_output("Available convoys:", nullptr, false);
    for (int i = 0; i < Convoy_definitions.size(); i++)
    {
        console_output(Convoy_definitions[i]->name, nullptr, false);
    }
}

// 0x00A16340 (:2358)
const farray<convoy_definition const*,20>& convoy_definition_master_list()
{
    return Convoy_definitions;
}

// 0x00A621F0 (:2364)
bool convoy_is_leader(const vehicle* vp)
{
    const int count = World.get_num_objects<object_convoy>();
    for (int i = 0; i < count; i++)
    {
        if (World.get_object<object_convoy>(i)->get_lead_vehicle() == vp)
        {
            return true;
        }
    }
    return false;
}

// 0x00A62290 (:2377)
object_convoy* convoy_find_convoy(unsigned int squad_handle)
{
    if (squad_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const int count = World.get_num_objects<object_convoy>();
    for (int i = 0; i < count; i++)
    {
        object_convoy* convoy = World.get_object<object_convoy>(i);
        if (convoy->is_convoy_member(squad_handle))
        {
            return convoy;
        }
    }
    return nullptr;
}

// 0x00A62310 (:2401)
object_convoy* convoy_find_convoy(const object_squad* squad)
{
    return squad != nullptr ? convoy_find_convoy(squad->get_handle()) : nullptr;
}
