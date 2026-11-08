// Original: volition/rfg/code/ai/civilian/civilian_common.cpp
//
// Choosing where civilians walk and drive, and the horn.

#include "volition/rfg/code/ai/civilian/civilian_common.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp

static float Horn_honk_prob = 0.5f; // 0x016415B0
static int Horn_honk_period_min_ms = 500; // 0x016415B4
static int Horn_honk_period_max_ms = 2000; // 0x016415B8
static int Horn_cooldown_period_min_ms = 3000; // 0x016415BC
static int Horn_cooldown_period_max_ms = 6000; // 0x016415C0
static tweak_table_item Tweak_table_item_Horn_honk_prob("Horn_honk_prob", &Horn_honk_prob);
static tweak_table_item Tweak_table_item_Horn_honk_period_min_ms("Horn_honk_period_min_ms", &Horn_honk_period_min_ms);
static tweak_table_item Tweak_table_item_Horn_honk_period_max_ms("Horn_honk_period_max_ms", &Horn_honk_period_max_ms);
static tweak_table_item Tweak_table_item_Horn_cooldown_period_min_ms("Horn_cooldown_period_min_ms", &Horn_cooldown_period_min_ms);
static tweak_table_item Tweak_table_item_Horn_cooldown_period_max_ms("Horn_cooldown_period_max_ms", &Horn_cooldown_period_max_ms);

static const float NAVPOINT_DEST_OFFSET = 1.0f; // 0x01647FD4
static const float NEAR_MIDPOINT_DEST_DIST_SQRD = 400.0f; // a local static of select_midpoint_destination
static const signed char NAVPOINT_TYPE_PED = 2; // numeric in rfg.exe, the ped network's nodes

// 0x005E0D00 (:39)
initial_ped_node_candidate::initial_ped_node_candidate()
    : npp(nullptr)
    , direction_dot(-1.0f)
{
}

// 0x005E0D20 (:42)
initial_ped_node_candidate::initial_ped_node_candidate(const navpoint* npp_, const vector& reference_pos, const vector& reference_dir)
    : npp(npp_)
{
    vector dir(npp_->pos.x - reference_pos.x, npp_->pos.y - reference_pos.y, npp_->pos.z - reference_pos.z);
    direction_dot = dir.flatten_mag_and_normalize_safe() < 0.1f ? -1.0f : reference_dir.y * dir.y + reference_dir.x * dir.x + reference_dir.z * dir.z;
}

// 0x005E0E10 (:55)
road_destination_info::road_destination_info()
{
}

// 0x005E0E20 (:129). LTCG: np EAX, pos on the stack.
static bool ambient_behavior_can_go_to_point(const npc* np, const vector& pos)
{
    return TEAM_DISPOSITION_FRIENDLY <= human_teams_get_disposition(np, HUMAN_TEAM_EDF) || !restricted_area_in_any_region(pos, true);
}

// 0x005E0E50 (:138). LTCG: obstruction_hp ESI, obstruction_vp EDI. A player blocking with a slow or no vehicle is yelled at more as morale falls.
static bool should_play_vehicle_stuck_line(const human* obstruction_hp, const vehicle* obstruction_vp)
{
    static const float MORALE_YELL_RANGE = 0.4f;
    if (obstruction_hp == nullptr || obstruction_hp->is_killed())
    {
        return false;
    }
    const player* local = player_get_local_player();
    if (local != obstruction_hp)
    {
        return true;
    }
    if (obstruction_vp != nullptr && obstruction_vp->get_handle() == local->vehicle_handle && 5.0f < obstruction_vp->forward_velocity)
    {
        return false;
    }
    const float morale = gfm_get_effective_morale_percent();
    if (morale < 0.4f)
    {
        return true;
    }
    return morale <= 0.8f && fl_rand_chance(fabsf((morale - 0.4f) / MORALE_YELL_RANGE));
}

// 0x006123D0 (:189). LTCG: dest_list ESI.
static const road_destination_info* pick_road_destination(const farray<road_destination_info, 32>& dest_list)
{
    return dest_list.size() == 1 ? &dest_list[0] : &dest_list[int_rand_range(0, dest_list.size() - 1)];
}

// 0x005E0F40 (:204). LTCG: final_destination_point EDI, current_road_dist XMM1, prp ECX, destination_point ESI, destination_index on the stack.
static bool select_midpoint_destination(const vector& final_destination_point, float current_road_dist, int destination_index, const object_path_road* prp, vector& destination_point)
{
    if (prp == nullptr)
    {
        return false;
    }
    prp->get_point(destination_point, ((float)destination_index + current_road_dist) * 0.5f);
    if (vec_xz_dist_squared(destination_point, final_destination_point) <= NEAR_MIDPOINT_DEST_DIST_SQRD)
    {
        destination_point = final_destination_point;
    }
    return true;
}

// 0x005E0FD0 (:229). LTCG: np ECX, selected_dest EAX.
static bool use_selected_destination(npc* np, const road_destination_info* selected_dest)
{
    if (selected_dest == nullptr)
    {
        return false;
    }
    ai_system* ai = np->get_ai();
    ai->desired_destination = selected_dest->dest_point;
    ai->desired_destination_object = selected_dest->dest_prp->get_handle();
    ai->action_obj_handle = selected_dest->current_prp->get_handle();
    ai->action_substate = selected_dest->dest_index;
    return select_midpoint_destination(ai->desired_destination, selected_dest->current_road_dist, selected_dest->dest_index, selected_dest->current_prp, ai->action_pos);
}

static const object_path_road* get_road_from_handle(unsigned int handle)
{
    const object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_PATH_ROAD ? static_cast<const object_path_road*>(objp) : nullptr;
}

// An ambient road this connection may be taken onto: not against one-way traffic.
static bool connection_is_usable(const road_spline_connection& connection, const object_path_road* other)
{
    return other != nullptr && other->road_flags.ambient && (!other->road_flags.one_way || connection.to_pt != other->spline_data.num_points - 1);
}

// 0x00653D50 (:255). LTCG: prp ESI. Heads for the nearer allowed end of the road.
static bool use_endpoint(npc* np, const object_path_road* prp)
{
    vector closest;
    const float road_dist = prp->find_closest_point_on_road(np->pos, closest, false);
    const road_spline_point* points = prp->spline_data.points.to_pointer();
    const unsigned short last = (unsigned short)(prp->spline_data.num_points - 1);
    const bool first_allowed = ambient_behavior_can_go_to_point(np, points[0].pos);
    const bool last_allowed = ambient_behavior_can_go_to_point(np, points[last].pos);
    const float first_dist_sq = vec_dist_squared(points[0].pos, np->pos);
    const float last_dist_sq = vec_dist_squared(points[last].pos, np->pos);
    unsigned short endpoint = 0;
    if (!first_allowed || prp->road_flags.one_way || (last_allowed && first_dist_sq <= last_dist_sq))
    {
        if (!last_allowed)
        {
            return false;
        }
        endpoint = last;
    }
    const object_path_road* next_road = prp;
    const road_spline_connection* connections = prp->spline_data.connections.to_pointer();
    for (int i = 0; i < prp->spline_data.num_connections; i++)
    {
        const object_path_road* other = connections[i].from_pt == endpoint ? get_road_from_handle(connections[i].handle) : nullptr;
        if (connection_is_usable(connections[i], other))
        {
            next_road = other;
            break;
        }
    }
    ai_system* ai = np->get_ai();
    ai->desired_destination = points[endpoint].pos;
    ai->desired_destination_object = next_road->get_handle();
    ai->action_obj_handle = prp->get_handle();
    ai->action_substate = endpoint;
    return select_midpoint_destination(ai->desired_destination, road_dist, endpoint, prp, ai->action_pos);
}

// 0x005E1040 (:367).
// Original bug kept: best starts at -1 and only a lower dot replaces it, so a link is only taken on rounding error below -1 and this answers null (0x005E119D).
const navpoint* find_last_destination_nav(const npc* np, const navpoint* dest_npp)
{
    vector to_dest;
    if (vec_normalized_dir(to_dest, np->pos, dest_npp->pos) < 0.1f)
    {
        return nullptr;
    }
    const navpoint* best = nullptr;
    float best_dot = -1.0f;
    for (int i = 0; i < dest_npp->num_links; i++)
    {
        if (!Navpoint_treat_all_as_enabled && dest_npp->link_disabled_by[i] != 0)
        {
            continue;
        }
        const navpoint* link = dest_npp->links[i];
        vector to_link(link->pos.x - np->pos.x, link->pos.y - np->pos.y, link->pos.z - np->pos.z);
        const float dot = to_link.flatten_mag_and_normalize_safe() < 0.1f ? -1.0f : to_link.x * to_dest.x + to_dest.y * to_link.y + to_dest.z * to_link.z;
        if (dot < best_dot)
        {
            best = link;
            best_dot = dot;
        }
    }
    return best;
}

// 0x00654010 (:399). LTCG: prp ECX; np and last_road_handle on the stack.
static bool pick_destination_on_road(npc* np, const object_path_road* prp, unsigned int last_road_handle)
{
    farray<road_destination_info, 32> destinations;
    road_destination_info info;
    info.current_prp = prp;
    vector closest;
    info.current_road_dist = prp->find_closest_point_on_road(np->pos, closest, false);
    const road_spline_connection* connections = prp->spline_data.connections.to_pointer();
    const road_spline_point* points = prp->spline_data.points.to_pointer();
    for (int i = 0; i < prp->spline_data.num_connections; i++)
    {
        const road_spline_connection& connection = connections[i];
        if (connection.handle == last_road_handle)
        {
            continue;
        }
        info.dest_prp = get_road_from_handle(connection.handle);
        if (!connection_is_usable(connection, info.dest_prp) || (prp->road_flags.one_way && (float)connection.from_pt < info.current_road_dist))
        {
            continue;
        }
        info.dest_index = connection.from_pt;
        info.dest_point = points[connection.from_pt].pos;
        if (ambient_behavior_can_go_to_point(np, info.dest_point))
        {
            destinations.add(info);
            if (destinations.full())
            {
                break;
            }
        }
    }
    if (destinations.size() < 1)
    {
        return use_endpoint(np, prp);
    }
    return use_selected_destination(np, pick_road_destination(destinations));
}

// 0x005E11D0 (:527)
bool civilian_is_near_ped_node(const npc* np)
{
    bbox box;
    box.min = vector(np->pos.x - 30.0f, np->pos.y - 30.0f, np->pos.z - 30.0f);
    box.max = vector(np->pos.x + 30.0f, np->pos.y + 30.0f, np->pos.z + 30.0f);
    return ped_node_near_box(box);
}

// 0x006542D0 (:537)
const navpoint* civilian_pick_next_ped_node(npc* np)
{
    ai_system* ai = np->get_ai();
    ai_pedestrian_info& info = ai->pedestrian_info;
    if (ai->action_obj_handle != 0xFFFFFFFF)
    {
        const object* last = World.object_from_handle(ai->action_obj_handle);
        if (last != nullptr && last->get_obj_type() == OT_NAVPOINT)
        {
            info.remove_waypoint(last->get_handle());
        }
    }
    const navpoint* node = info.get_next_navpoint();
    if (node == nullptr || node->type != NAVPOINT_TYPE_PED)
    {
        return nullptr;
    }
    ai->action_obj_handle = node->get_handle();
    const vector to_node(node->pos.x - np->pos.x, 0.0f, node->pos.z - np->pos.z);
    matrix orient = IDENTITY_MATRIX;
    if (!to_node.is_null())
    {
        orient.make(to_node);
    }
    ai->desired_destination.x = node->pos.x + orient.fvec.x * NAVPOINT_DEST_OFFSET + orient.rvec.x * NAVPOINT_DEST_OFFSET;
    ai->desired_destination.y = node->pos.y + orient.fvec.y * NAVPOINT_DEST_OFFSET + orient.rvec.y * NAVPOINT_DEST_OFFSET;
    ai->desired_destination.z = node->pos.z + orient.fvec.z * NAVPOINT_DEST_OFFSET + orient.rvec.z * NAVPOINT_DEST_OFFSET;
    if (node->nav_cell_ptr != nullptr && nav_cell_is_point_in_cell_column(node->nav_cell_ptr, node->pos))
    {
        havok_terrain_height(ai->desired_destination.y, ai->desired_destination.x, ai->desired_destination.z, false, nullptr);
    }
    return node;
}

// 0x006545B0 (:592)
const navpoint* civilian_pick_new_ped_node(npc* np, ped_node_select_modes selection_mode)
{
    ai_system* ai = np->get_ai();
    ai->action_obj_handle = 0xFFFFFFFF;
    ai->desired_destination.zero();
    if (!ai->pedestrian_info.near_ped_nodes(np->pos) && !amb_spawn_build_ped_waypoints(np, ai->pedestrian_info, selection_mode))
    {
        return nullptr;
    }
    return civilian_pick_next_ped_node(np);
}

// 0x005E1290 (:617)
bool civilian_is_near_road(const npc* np)
{
    return object_road_get_closest_road(np->pos, np->npc_flags.can_use_special_roads, 100.0f, false, false) != nullptr;
}

// 0x00654610 (:646). With connections both ways the list ahead of the vehicle's facing wins; near an end of the road the only way on does.
bool civilian_pick_destination_on_current_road(npc* np)
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return false;
    }
    vector closest;
    road_destination_info info;
    info.current_prp = object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads, closest, info.current_road_dist, 50.0f, false, nullptr, nullptr, false);
    if (info.current_prp == nullptr)
    {
        return false;
    }
    if (225.0f < vec_dist_squared(vp->pos, closest))
    {
        return pick_destination_on_road(np, info.current_prp, 0xFFFFFFFF);
    }
    const object_path_road* prp = info.current_prp;
    farray<road_destination_info, 32> behind;
    farray<road_destination_info, 32> ahead;
    const road_spline_connection* connections = prp->spline_data.connections.to_pointer();
    const road_spline_point* points = prp->spline_data.points.to_pointer();
    for (int i = 0; i < prp->spline_data.num_connections; i++)
    {
        const road_spline_connection& connection = connections[i];
        info.dest_prp = get_road_from_handle(connection.handle);
        if (!connection_is_usable(connection, info.dest_prp))
        {
            continue;
        }
        info.dest_index = connection.from_pt;
        info.dest_point = points[connection.from_pt].pos;
        if (!ambient_behavior_can_go_to_point(np, info.dest_point))
        {
            continue;
        }
        if ((float)connection.from_pt < info.current_road_dist)
        {
            if (!prp->road_flags.one_way)
            {
                behind.add(info);
                if (behind.full())
                {
                    break;
                }
            }
        }
        else
        {
            ahead.add(info);
            if (ahead.full())
            {
                break;
            }
        }
    }
    const road_destination_info* selected;
    if (behind.size() == 0)
    {
        if (ahead.size() == 0)
        {
            return use_endpoint(np, prp);
        }
        selected = pick_road_destination(ahead);
    }
    else if (ahead.size() == 0)
    {
        selected = pick_road_destination(behind);
    }
    else
    {
        // The 0.05 at 0x012BA518 is widened from float. The road point before the closest one decides which way the vehicle runs (0x006549A8 on).
        const farray<road_destination_info, 32>* list;
        if ((float)(prp->spline_data.num_points - 1) - info.current_road_dist < 0.05f)
        {
            list = &behind;
        }
        else if (info.current_road_dist < 0.05f)
        {
            list = &ahead;
        }
        else
        {
            int index = (int)info.current_road_dist;
            if (0 < index)
            {
                index--;
            }
            vector to_previous(points[index].pos.x - vp->pos.x, points[index].pos.y - vp->pos.y, points[index].pos.z - vp->pos.z);
            if (to_previous.is_null())
            {
                to_previous = FORWARD_VECTOR;
            }
            else
            {
                to_previous.normalize_safe();
            }
            list = 0.0f < vp->orient.fvec.y * to_previous.y + vp->orient.fvec.x * to_previous.x + vp->orient.fvec.z * to_previous.z ? &behind : &ahead;
        }
        selected = pick_road_destination(*list);
    }
    return use_selected_destination(np, selected);
}

// 0x00654AF0 (:763)
bool civilian_ped_enter_common(npc* np, ped_node_select_modes selection_mode)
{
    return civilian_pick_new_ped_node(np, selection_mode) != nullptr;
}

// 0x00690960 (:783)
bool civilian_ped_do_action_common(npc* np, human_movement_states m_state, ped_node_select_modes selection_mode)
{
    ai_system* ai = np->get_ai();
    const vector& dest = ai->desired_destination;
    if (dest.is_null())
    {
        return false;
    }
    const float dist_sq = vec_xz_dist_squared(np->pos, dest);
    const object* objp = ai->action_obj_handle != 0xFFFFFFFF ? World.object_from_handle(ai->action_obj_handle) : nullptr;
    const navpoint* node = objp != nullptr && objp->get_obj_type() == OT_NAVPOINT ? static_cast<const navpoint*>(objp) : nullptr;
    if (dist_sq < 1.5625f || node == nullptr)
    {
        node = civilian_pick_next_ped_node(np);
        if (node == nullptr)
        {
            node = civilian_pick_new_ped_node(np, selection_mode);
            if (node == nullptr)
            {
                return false;
            }
        }
    }
    const uint64_t hint = node->nav_cell_ptr != nullptr ? nav_cell_get_handle(node->nav_cell_ptr) : 0xFFFFFFFFFFFFFFFFull;
    const ai_pathfind_results result = ai->pathfind_to(dest, m_state, HST_STAND, AI_PATHFIND_PRIORITY_LOW, hint, false);
    return result == AI_PF_SUCCESS || result <= AI_PF_RAGDOLLED;
}

// 0x005E12D0 (:829)
void civilian_horn_do_enter(npc* np, bool same_action)
{
    if (!same_action)
    {
        np->get_ai()->guard_node_watch_period.invalidate();
    }
}

// 0x00654B10 (:844). Tells the ambient peds on the road within the cone ahead that they were honked at; the one most ahead answers for them.
static void civilian_honk_at_pedestrians(const npc* driver_np)
{
    static const float SEARCH_DOT = (float)cos((double)(45.0f * 0.017453f)); // 0x012CEB58, widened from float
    const object* vp = driver_np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(driver_np->vehicle_handle) : nullptr;
    if (vp == nullptr || vp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    float range = static_cast<const vehicle*>(vp)->info->spawn_size + 10.0f;
    if (range < 10.0f)
    {
        return;
    }
    range *= range;
    farray<npc*, 100> honked;
    npc* best = nullptr;
    float best_dot = -1.0f;
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* ped = World.get_object<npc>(i);
        if (!ped->npc_flags.ambient_spawn || ped->vehicle_handle != 0xFFFFFFFF || range < vec_dist_squared(vp->pos, ped->pos))
        {
            continue;
        }
        vector dir(ped->pos.x - vp->pos.x, ped->pos.y - vp->pos.y, ped->pos.z - vp->pos.z);
        dir.normalize_safe();
        const float dot = dir * vp->orient.fvec;
        if (SEARCH_DOT <= dot && object_road_point_on_road(ped->pos, false) != nullptr)
        {
            honked.add(ped);
            if (best_dot < dot)
            {
                best = ped;
                best_dot = dot;
            }
        }
    }
    if (best == nullptr || honked.size() < 1)
    {
        return;
    }
    for (int i = 0; i < honked.size(); i++)
    {
        honked[i]->npc_flags.honked_at = 1;
        honked[i]->get_ai()->planner.request_replan();
    }
    if (human_teams_are_friendly(driver_np, best))
    {
        best->npc_flags.honked_at_friendly = 1;
    }
    else if (fl_rand_chance(0.5f))
    {
        best->npc_flags.honked_at_friendly = 1;
        best->npc_flags.honked_at_hostile = 0;
    }
    else
    {
        best->npc_flags.honked_at_friendly = 0;
        best->npc_flags.honked_at_hostile = 1;
    }
}

// 0x00654E90 (:954)
void civilian_horn_do_honk(npc* np, bool should_honk)
{
    if (!should_honk)
    {
        return;
    }
    timestamp& timer = np->get_ai()->guard_node_watch_period;
    if (timer.valid() && !timer.elapsed())
    {
        return;
    }
    if (!timer.valid())
    {
        if (Horn_honk_prob < fl_rand())
        {
            timer.set_rand(Horn_cooldown_period_min_ms, Horn_cooldown_period_max_ms);
            automobile_release_horn(np->vehicle_handle);
            return;
        }
        timer.set_rand(Horn_honk_period_min_ms, Horn_honk_period_max_ms);
        automobile_honk_horn(np->vehicle_handle);
        civilian_honk_at_pedestrians(np);
        return;
    }
    if (vehicle_get_honked_horn(np->vehicle_handle))
    {
        timer.set_rand(Horn_cooldown_period_min_ms, Horn_cooldown_period_max_ms);
        automobile_release_horn(np->vehicle_handle);
        return;
    }
    timer.invalidate();
}

// 0x005E1300 (:1001)
void civilian_horn_do_clean_up(npc* np, bool same_action)
{
    if (!same_action && vehicle_get_honked_horn(np->vehicle_handle))
    {
        automobile_release_horn(np->vehicle_handle);
    }
}

// 0x00690A50 (:1012)
bool civilian_drive_do_action_common(npc* np, vehicle* vp, bool)
{
    ai_system* ai = np->get_ai();
    if (vp->vai.ambient_wait_timer.valid())
    {
        if (!vp->vai.ambient_wait_timer.elapsed())
        {
            if (!ai->flags.played_stuck_line)
            {
                const ai_goal* goal = ai->planner.get_current_goal();
                if (goal == nullptr || !goal->pursues_condition(ai_plan_conditions(4 * 32 + 3)))
                {
                    const unsigned int wait_handle = vp->vai.ambient_wait_object;
                    object* wait_object = wait_handle != 0xFFFFFFFF ? World.object_from_handle(wait_handle) : nullptr;
                    const vehicle* wait_vehicle = wait_object != nullptr && wait_object->get_obj_type() == OT_VEHICLE ? static_cast<const vehicle*>(wait_object) : nullptr;
                    const human* wait_human = wait_object != nullptr && wait_object->get_obj_type() == OT_HUMAN ? static_cast<const human*>(wait_object) : nullptr;
                    if (wait_vehicle == nullptr || !vehicle_is_corpse(wait_vehicle))
                    {
                        const human* obstruction = wait_vehicle != nullptr ? vehicle_passenger_in_seat(wait_vehicle, VSI_DRIVER) : wait_human;
                        if (should_play_vehicle_stuck_line(obstruction, wait_vehicle))
                        {
                            np->play_voice_line(VOICE_LINE_VEHICLE_STUCK, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                        }
                    }
                    ai->flags.played_stuck_line = true;
                }
            }
            vehicle_set_controls_when_abandoned(vp);
            civilian_horn_do_honk(np, true);
            return true;
        }
        ai->flags.played_stuck_line = false;
        vp->vai.ambient_wait_timer.invalidate();
        vp->vai.stuck.stuck_threshold_time.invalidate();
        vp->vai.stuck.extended_stuck_time.invalidate();
    }
    // Pushes before 0x00690BD9: no hint, on roads, 10 m (0x012C8500), smoothing on. obey_checkpoints_and_roadblocks is dropped by LTCG.
    return ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->action_pos, 0xFFFFFFFFFFFFFFFFull, true, 10.0f, false, false, false, true) != AI_RESULT_FAILURE;
}
