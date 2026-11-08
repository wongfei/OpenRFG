// Original: volition/rfg/code/ai/self_preservation_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/self_preservation_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/combat/cover_selection.h"
#include "volition/rfg/code/ai/hazard_detection.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/sorted_list.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/varray.h"

#include <float.h>
#include <math.h>

// Position 30 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG30")

extern navpoint_heap Navpoint_heap; // nav_heap.cpp
extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp

static const signed char NAVPOINT_TYPE_SEARCH = 10; // numeric in rfg.exe, name not recovered
static const signed char NAVPOINT_TYPE_TRANSITION = 11;
static const unsigned char NAV_CELL_FLAG_IMPASSABLE = 8;
static const double PI_D = 3.1415927410125732; // float precision, widened (original)
static const double HALF_PI_D = 1.5707963705062866;
static const double TWO_PI_D = 6.2831854820251465;
static const float COS_15_DEG = 0.9659258f;
static const float COS_30_DEG = 0.8660254f;
static const int AVOID_FLEE_MS = 30000;
static const unsigned int REACTION_FLEE = 0x400; // reaction reason bit, numeric in rfg.exe, name not recovered

static bool Allow_flee_to_safe_building = true; // 0x016415DD
int Ai_removes_attached_projectile_time; // 0x01D65980
static tweak_table_item Tweak_table_item_Ai_removes_attached_projectile_time("Ai_removes_attached_projectile_time", &Ai_removes_attached_projectile_time); // 0x01DA0608
static const float MIN_FLEE_DOT = (float)cos((double)(60.0f * 0.017453f)); // 0x01DC9FE4, 60 degrees
static const animation_action Dive_anim_actions[2] = { ANIMATION_ACTION_DIVE, ANIMATION_ACTION_DIVE_A }; // 0x012C9820

// 0x005F1D70
cell_search::cell_search()
{
}

// 0x005F1D60 (:139)
bool cell_search::operator==(const cell_search& a)
{
    return a.cell == cell;
}

// 0x0066C4C0 (:335): the farthest from flee_pos of the 256 nearest cells, less what the walk there went back toward it.
static bool pick_vehicle_flee_destination_off_road(const vehicle* vp, vector& destination_point, vector flee_pos)
{
    farray<cell_search, 256> cells;
    cell_search cur;
    cur.cell = nav_cell_from_handle(vp->m_navcell_info.last_visited_nav_cell_handle);
    if (cur.cell == nullptr || (cur.cell->flags & NAV_CELL_FLAG_IMPASSABLE) != 0)
    {
        return false;
    }
    cur.closest_dist = 0.0f;
    cells.add(cur);
    const nav_cell* best = nullptr;
    vector best_center = ZERO_VECTOR;
    float best_score = 0.0f;
    float first_dist = 0.0f;
    for (int i = 0; i < cells.size() && i < 256; i++)
    {
        cur = cells[i];
        nav_cell_get_centroid(&cur.center, cur.cell);
        const float dist_sq = vec_dist_squared(flee_pos, cur.center);
        const float dist = sqrtf(dist_sq);
        if (dist - cur.closest_dist < 0.0f)
        {
            cur.closest_dist = dist;
        }
        if (i == 0)
        {
            first_dist = dist;
            cur.closest_dist = dist;
        }
        const float score = dist - (first_dist - cur.closest_dist);
        if (best_score < score)
        {
            best = cur.cell;
            best_center = cur.center;
            best_score = score;
        }
        if (cells.full())
        {
            continue;
        }
        for (int edge = 0; edge < 3; edge++)
        {
            const nav_cell* adjacent = cur.cell->adjacent_cells[edge].to_pointer();
            if (adjacent == nullptr || (adjacent->flags & NAV_CELL_FLAG_IMPASSABLE) != 0)
            {
                continue;
            }
            bool found = false;
            for (int j = 0; j < cells.size(); j++)
            {
                if (cells[j].cell == adjacent)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                cell_search next;
                next.cell = adjacent;
                next.closest_dist = cur.closest_dist;
                cells.add(next);
            }
            if (cells.full())
            {
                break;
            }
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    destination_point = best_center;
    return true;
}

// 0x006866B0 (:418). LTCG: vp ESI, prp EAX, use_road EDI.
static bool pick_vehicle_flee_destination(const vehicle* vp, object_path_road* prp, vector& destination_point, int& use_road, vector flee_pos, bool flee_from_heavy_vehicle)
{
    if (vp == nullptr)
    {
        return false;
    }
    if (prp != nullptr && !flee_from_heavy_vehicle)
    {
        use_road = ai_vehicle_pick_flee_destination_road(vp, prp, destination_point, flee_pos) ? 1 : 0;
        if (use_road != 0)
        {
            return true;
        }
    }
    use_road = 0;
    return pick_vehicle_flee_destination_off_road(vp, destination_point, flee_pos);
}

// 0x005F1D80 (:440)
static bool transition_cb_func(object* objp, unsigned int, const void* cb_data)
{
    const navpoint* ref = static_cast<const navpoint*>(cb_data);
    const navpoint* npp = static_cast<const navpoint*>(objp);
    return ref != npp && ref->set == npp->set && npp->type == NAVPOINT_TYPE_TRANSITION;
}

// 0x005F1DB0 (:448). LTCG: npp EAX.
static float hide_in_building_navpoint_heuristic(const navpoint* npp)
{
    return (float)((double)npp->radius * 3.0 + (double)npp->heap_data.arrival_cost);
}

// 0x0066C750 (:457): walks the building's navpoints out from its doors; of the ten best search points, nearer ones are likelier.
static navpoint* select_safe_path_node_internal(navpoint* ref_point, unsigned int session_id)
{
    const unsigned int parent = ref_point->get_parent();
    object* parent_obj = parent != 0xFFFFFFFF ? World.object_from_handle(parent) : nullptr;
    if (parent_obj == nullptr || parent_obj->get_obj_type() != OT_MOVER || parent_obj->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    const rfg_mover* building = static_cast<rfg_mover*>(parent_obj);
    if (building->flags.is_ai_subpiece != 0)
    {
        building = get_root_building_internal(building);
    }
    farray<navpoint*, 50> transitions;
    if (ref_point->type == NAVPOINT_TYPE_TRANSITION)
    {
        transitions.add(ref_point);
    }
    ai_building_collect_navpoints(transitions, building, true, transition_cb_func, ref_point);
    for (int i = 0; i < transitions.size(); i++)
    {
        navpoint* npp = transitions[i];
        npp->heap_data.flags.is_open = 1;
        npp->heap_data.session_id = session_id;
        npp->prev_path_navpoint = nullptr;
        npp->next_path_navpoint = npp;
        npp->heap_data.arrival_cost = 0.0f;
        npp->heap_data.est_cost_to_goal = 0.0f;
        Navpoint_heap.insert_cell(npp);
    }
    sorted_list<navpoint*, 10, float> candidates(false);
    while (Navpoint_heap.num_navpoints > 0)
    {
        navpoint* cur = Navpoint_heap.remove_best_cell();
        cur->heap_data.flags.is_open = 0;
        if (cur->type == NAVPOINT_TYPE_SEARCH && vec_dist_squared(cur->pos, ref_point->pos) <= 400.0f && (fabsf(cur->pos.y - ref_point->pos.y) <= 1.5f || fabsf(cur->pos.y - cur->next_path_navpoint->pos.y) <= 1.5f)) // 20 m, 1.5 m
        {
            candidates.add(cur, hide_in_building_navpoint_heuristic(cur));
        }
        for (int i = 0; i < cur->num_links; i++)
        {
            navpoint* link = cur->links[i];
            if (!Navpoint_treat_all_as_enabled && (cur->link_disabled_by[i] != 0 || link->heap_data.disabled_by != 0))
            {
                continue;
            }
            bool in_building = false;
            for (const object* objp = link; objp != nullptr; objp = World.object_from_handle(objp->get_parent()))
            {
                if (objp == building)
                {
                    in_building = true;
                    break;
                }
            }
            if (!in_building)
            {
                continue;
            }
            const bool is_new = link->heap_data.session_id != session_id;
            if (is_new)
            {
                link->heap_data.arrival_cost = cur->radius + 20.0f; // the most a first arrival may cost
            }
            else if (link->heap_data.flags.is_open == 0)
            {
                continue;
            }
            const float cost = vec_dist(cur->pos, link->pos) + cur->heap_data.arrival_cost;
            if (cost < link->heap_data.arrival_cost)
            {
                link->prev_path_navpoint = cur;
                link->next_path_navpoint = cur->next_path_navpoint;
                link->heap_data.arrival_cost = cost;
                link->heap_data.est_cost_to_goal = 0.0f;
                if (is_new)
                {
                    link->heap_data.flags.is_open = 1;
                    link->heap_data.session_id = session_id;
                    Navpoint_heap.insert_cell(link);
                }
                else
                {
                    Navpoint_heap.adjust_cell(link);
                }
            }
        }
    }
    int remaining = candidates.size();
    if (remaining <= 0)
    {
        return nullptr;
    }
    // Triangular weights: the first gets n shares, the last one.
    int roll = int_rand_range(1, (remaining + 1) * remaining / 2);
    for (navpoint* const* entry = candidates.get_first(); entry != nullptr; entry = candidates.get_next(entry))
    {
        roll -= remaining;
        remaining--;
        if (roll < 1)
        {
            return *entry;
        }
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
    return nullptr;
}

// 0x0066CB60 (:630)
static navpoint* select_safe_path_node(navpoint* ref_point)
{
    const unsigned int session_id = Navpoint_heap.start_session();
    navpoint* npp = select_safe_path_node_internal(ref_point, session_id);
    Navpoint_heap.end_session();
    return npp;
}

static bool any_belief_within(const ai_perception& perception, ai_belief_types type, const vector& pos, float dist_sq)
{
    for (const ai_belief* belief = perception.get_first_belief(type); belief != nullptr; belief = perception.get_next_belief(belief))
    {
        if (vec_dist_squared(pos, belief->pos) < dist_sq)
        {
            return true;
        }
    }
    return false;
}

static bool heavy_vehicle_within(const ai_perception& perception, const vector& pos, float dist_sq)
{
    for (const ai_belief* belief = perception.get_first_belief(AI_BELIEF_VEHICLE); belief != nullptr; belief = perception.get_next_belief(belief))
    {
        if (BELIEF_REQUIREMENTS_HEAVY_VEHICLE_THREAT.test(belief->flags) && vec_dist_squared(pos, belief->pos) < dist_sq)
        {
            return true;
        }
    }
    return false;
}

// 0x005F1DE0 (:646)
bool flee_navpoint_is_safe(const navpoint* new_navp, const npc* np, const object* flee_objp)
{
    const ai_perception& perception = np->get_ai()->perception;
    if (flee_objp != nullptr && vec_dist_squared(flee_objp->pos, new_navp->pos) < 100.0f) // 10 m
    {
        return false;
    }
    const vector& pos = new_navp->pos;
    return !any_belief_within(perception, AI_BELIEF_EXPLOSION, pos, 400.0f) // 20 m
        && !any_belief_within(perception, AI_BELIEF_LAST_DAMAGE_TAKEN, pos, 25.0f) // 5 m
        && !any_belief_within(perception, AI_BELIEF_BULLET_IMPACT, pos, 25.0f)
        && !any_belief_within(perception, AI_BELIEF_COMBAT_SOUND, pos, 25.0f)
        && !any_belief_within(perception, AI_BELIEF_UNFRIENDLY, pos, 25.0f)
        && !heavy_vehicle_within(perception, pos, 900.0f); // 30 m
}

static bool cover_exposed_to_belief(const ai_perception& perception, ai_belief_types type, const cover_node* new_cnp, const npc* np, float dist_sq)
{
    for (const ai_belief* belief = perception.get_first_belief(type); belief != nullptr; belief = perception.get_next_belief(belief))
    {
        if (vec_dist_squared(new_cnp->pos, belief->pos) < dist_sq && !new_cnp->will_provide_defensive_cover(np, belief->pos, false))
        {
            return true;
        }
    }
    return false;
}

// 0x005F1FE0 (:706)
bool flee_cover_node_is_safe(const cover_node* new_cnp, const npc* np, const object* flee_objp)
{
    const ai_perception& perception = np->get_ai()->perception;
    if (flee_objp != nullptr && vec_dist_squared(flee_objp->pos, new_cnp->pos) < 56.25f && !new_cnp->will_provide_defensive_cover(np, flee_objp->pos, false)) // 7.5 m
    {
        return false;
    }
    const vector& pos = new_cnp->pos;
    return !any_belief_within(perception, AI_BELIEF_EXPLOSION, pos, 100.0f) // 10 m
        && !any_belief_within(perception, AI_BELIEF_LAST_DAMAGE_TAKEN, pos, 6.25f) // 2.5 m
        && !cover_exposed_to_belief(perception, AI_BELIEF_COMBAT_SOUND, new_cnp, np, 12.25f) // 3.5 m
        && !cover_exposed_to_belief(perception, AI_BELIEF_UNFRIENDLY, new_cnp, np, 12.25f)
        && !heavy_vehicle_within(perception, pos, 100.0f);
}

// 0x00641A90 (:771)
void stop_fleeing_to_safe_building(const npc* np)
{
    const_cast<npc*>(np)->get_ai()->planner.avoid_action(flee_to_safe_building_action::get_instance(), AVOID_FLEE_MS);
}

// 0x00641AB0 (:779)
void stop_hiding_in_defensive_cover(const npc* np)
{
    const_cast<npc*>(np)->get_ai()->planner.avoid_action(take_defensive_cover_action::get_instance(), AVOID_FLEE_MS);
}

// 0x00686720 (:796): the first safe friendly building not toward what the npc flees.
navpoint* find_nearest_safe_trans_point(const npc* np, const object* flee_objp)
{
    const ai_system* ai = np->get_ai();
    vector away = ZERO_VECTOR;
    if (flee_objp != nullptr)
    {
        away = np->pos - flee_objp->pos;
        away.flatten_normalize_safe();
    }
    belief_obj_iterator<rfg_mover> it;
    for (it.init(ai, AI_BELIEF_BUILDING); it; ++it)
    {
        const rfg_mover* structure = it.operator->();
        if (structure->flags.is_ai_building == 0 || structure->is_killed())
        {
            continue;
        }
        const ai_belief* belief = ai->perception.get_belief(AI_BELIEF_BUILDING, structure->get_handle());
        if (belief == nullptr || !BELIEF_REQUIREMENTS_SAFE_FRIENDLY.test(belief->flags) || (structure->mover_type_bit_flags & 0x10) != 0)
        {
            continue;
        }
        vector to_structure = structure->pos - np->pos;
        to_structure.flatten_normalize();
        if (flee_objp != nullptr && to_structure * away < MIN_FLEE_DOT)
        {
            continue;
        }
        navpoint* npp = ai_building_find_closest_transition_from_outside(np->pos, structure, nullptr);
        if (npp != nullptr)
        {
            return npp;
        }
    }
    return nullptr;
}

// 0x005F21F0 (:832). LTCG: np ECX, cover_pos EAX.
static bool in_defensive_cover(const npc* np, const vector& cover_pos)
{
    return vec_dist_squared(np->pos, cover_pos) <= 0.0625f; // 0.25 m
}

// 0x0066CB90 (:853). LTCG: next_search_node EDX. A door the rest of the plan already uses, if any.
static navpoint* get_leave_building_transition_navpoint(const npc* np, const ai_planner_search_node* next_search_node, const rfg_mover* building)
{
    for (; next_search_node != nullptr; next_search_node = next_search_node->next_node)
    {
        object* objp = next_search_node->action_data.use_target;
        if (objp == nullptr || objp->get_obj_type() != OT_NAVPOINT)
        {
            continue;
        }
        navpoint* npp = static_cast<navpoint*>(objp);
        if (!Navpoint_treat_all_as_enabled && npp->heap_data.disabled_by != 0)
        {
            continue;
        }
        const signed char type = npp->type;
        // Numeric in rfg.exe, names not recovered: transitions, jumps and ladders.
        if (type == NAVPOINT_TYPE_TRANSITION || type == 12 || type == 21 || type == 15 || type == 16)
        {
            return npp;
        }
    }
    return ai_building_find_closest_transition_from_inside(np->pos, building, true);
}

// 0x0066CC00 (:876): steers square to the hazard's path, on the npc's side of it.
static ai_results do_evade_action_enter(npc* np, bool force_sprint)
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = ai->perception.get_attention_belief();
    if (belief == nullptr || belief->get_type() != AI_BELIEF_HAZARD)
    {
        return AI_RESULT_FAILURE;
    }
    object* obj = World.object_from_handle(belief->handle);
    if (obj == nullptr)
    {
        ai->perception.deprecate_belief(belief);
        return AI_RESULT_FAILURE;
    }
    const vehicle* vp = obj->get_obj_type() == OT_VEHICLE ? static_cast<vehicle*>(obj) : nullptr;
    human_movement_states m_state = HMS_SPRINT;
    if (!force_sprint && vp != nullptr && ai_perception::vehicle_is_heavy(vp))
    {
        m_state = HMS_RUN;
    }
    hazard_physics_info info;
    get_hazard_physics_info(info, np, obj);
    vector velocity = info.velocity;
    velocity.y = 0.0f;
    if (!(0.0f < velocity.mag_and_normalize_safe()))
    {
        return AI_RESULT_FAILURE;
    }
    vector from_hazard = np->pos - obj->pos;
    if (!(0.0f < from_hazard.mag_and_normalize_safe()))
    {
        return AI_RESULT_FAILURE;
    }
    vector evade_dir;
    flyer* fp = vp != nullptr && vp->get_sub_type() == OT_SUB_VEHICLE_FLYER ? World.object_cast<flyer>(obj) : nullptr;
    if (fp != nullptr && fp->wing_is_detached)
    {
        evade_dir = from_hazard;
        evade_dir.flatten_normalize();
    }
    else
    {
        evade_dir.x = UP_VECTOR.z * velocity.y - velocity.z * UP_VECTOR.y;
        evade_dir.y = velocity.z * UP_VECTOR.x - velocity.x * UP_VECTOR.z;
        evade_dir.z = velocity.x * UP_VECTOR.y - UP_VECTOR.x * velocity.y;
        if (from_hazard * evade_dir < 0.0f)
        {
            evade_dir = -evade_dir;
        }
    }
    ai->desired_evade_dir = evade_dir;
    np->orient.make(ai->desired_evade_dir);
    ai->set_direct_movement_fast(ai->desired_evade_dir, m_state, HST_STAND, -1.0f);
    return AI_RESULT_SUCCESS;
}

// size 0x34
struct defensive_cover_data
{
    const npc* np; // 0x00
    const ai_belief* belief; // 0x04
    const object* obj; // 0x08
    vector bmin; // 0x0C
    vector bmax; // 0x18
    vector flee_point; // 0x24
    float min_dist_sq; // 0x30
};

// 0x006869E0 (:985)
static bool defensive_cover_cb(object* hit_objp, unsigned int, const void* data_ptr)
{
    const defensive_cover_data* data = static_cast<const defensive_cover_data*>(data_ptr);
    if (hit_objp == nullptr || hit_objp->get_obj_type() != OT_COVER_NODE)
    {
        return false;
    }
    const cover_node* node = static_cast<const cover_node*>(hit_objp);
    if (node->flags.on_mover != 0 || node->flags.crouch != 0)
    {
        return false;
    }
    if (human_team_is_non_combatant(data->np) && hit_objp->get_parent() != 0xFFFFFFFF)
    {
        object* parent = World.object_from_handle(hit_objp->get_parent());
        if (parent != nullptr && parent->get_obj_type() == OT_MOVER && parent->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            const rfg_mover* root = ai_building_get_root_building(static_cast<rfg_mover*>(parent));
            if (human_teams_are_unfriendly(data->np, root->team))
            {
                return false;
            }
        }
    }
    if (!node->will_provide_defensive_cover(data->np, data->flee_point, true))
    {
        return false;
    }
    if (data->belief != nullptr && vec_dist_squared(hit_objp->pos, data->belief->pos) < data->min_dist_sq)
    {
        return false;
    }
    if (data->obj != nullptr)
    {
        float dist_sq;
        if (data->obj->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            vector local = hit_objp->pos;
            vec_change_to_local_coords(local, data->obj->pos, data->obj->orient);
            dist_sq = bbox_find_xz_distance_squared(data->bmin, data->bmax, local);
        }
        else
        {
            dist_sq = vec_dist_squared(hit_objp->pos, data->obj->pos);
        }
        if (dist_sq < data->min_dist_sq)
        {
            return false;
        }
    }
    return !cover_selection_node_is_obstructed_by_geometry(node);
}

// 0x00686B40 (:1062). LTCG: np ECX, belief EDX.
static cover_node* get_defensive_cover_node(const npc* np, const ai_belief* belief, const object* obj, float min_dist, float max_dist)
{
    if (belief == nullptr && obj == nullptr)
    {
        return nullptr;
    }
    defensive_cover_data data;
    data.np = np;
    data.belief = belief;
    data.obj = obj;
    if (obj != nullptr)
    {
        obj->get_local_bbox(data.bmin, data.bmax);
    }
    data.min_dist_sq = (float)((double)min_dist * (double)min_dist);
    if (belief == nullptr)
    {
        data.flee_point = obj->pos;
    }
    else if (obj == nullptr)
    {
        data.flee_point = belief->pos;
    }
    else
    {
        ix_closest_point_on_line_segment(data.flee_point, np->pos, obj->pos, belief->pos);
    }
    cover_node* current = human_combat_cover_get_node(np);
    if (current != nullptr && defensive_cover_cb(current, 0xFFFFFFFF, &data))
    {
        return current;
    }
    unsigned int handles[20];
    const int num = World.collect_objects<cover_node>(np->pos, max_dist, handles, 20, defensive_cover_cb, &data);
    vector to_threat = (belief != nullptr ? belief->pos : obj->pos) - np->pos;
    to_threat.normalize_safe();
    cover_node* best = nullptr;
    float best_dist_sq = FLT_MAX;
    for (int i = 0; i < num; i++)
    {
        cover_node* node = World.object_from_handle<cover_node>(handles[i]);
        if (node == nullptr)
        {
            continue;
        }
        vector to_node = node->pos - np->pos;
        to_node.normalize_safe();
        if (to_node * to_threat <= MIN_FLEE_DOT)
        {
            const float dist_sq = vec_dist_squared(np->pos, node->pos);
            if (dist_sq < best_dist_sq)
            {
                best = node;
                best_dist_sq = dist_sq;
            }
        }
    }
    return best;
}

// 0x0066CF40 (:1156). IN_PROGRESS with where to run from; SUCCESS when safe already.
static ai_results get_flee_status(vector& flee_pos, vector& desired_flee_dir, const object*& flee_obj, npc* np)
{
    ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_attention_belief();
    const unsigned int target = ai->perception.get_attention_handle();
    object* objp = World.object_from_handle(target);
    flee_obj = objp;
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (objp == nullptr)
    {
        if (target != 0xFFFFFFFF)
        {
            for (int type = AI_BELIEF_ENEMY_EXISTS; type < NUM_AI_BELIEF_TYPES; type++)
            {
                ai->perception.deprecate_belief((ai_belief_types)type, target);
            }
        }
        return AI_RESULT_FAILURE;
    }
    if (objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG && static_cast<rfg_mover*>(objp)->flags.is_ai_structure != 0)
    {
        const rfg_mover* building = static_cast<rfg_mover*>(objp);
        const float unsafe_dist = ai->perception.get_unsafe_building_dist(building) + 2.5f; // m
        if (unsafe_dist * unsafe_dist < ai_building_find_bbox_dist_squared(np->pos, building))
        {
            return AI_RESULT_SUCCESS;
        }
        flee_pos = building->pos;
    }
    else
    {
        if (!human_team_is_non_combatant(np) && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE && (belief->flags & BF_NEARBY) == 0)
        {
            return AI_RESULT_SUCCESS;
        }
        ix_closest_point_on_line_segment(flee_pos, np->pos, belief->pos, objp->pos);
    }
    desired_flee_dir = np->pos - flee_pos;
    desired_flee_dir.flatten_normalize_safe();
    return AI_RESULT_IN_PROGRESS;
}

// 0x00641AD0 (:1251)
void ai_maybe_play_cower_to_stand_animation(npc* np, const ai_action* next_action)
{
    static const ai_plan_conditions ACS_NO_COWER_TO_STAND = ai_plan_conditions_from_bits({107, 119, 120, 121, 122, 123});
    if (np->get_ai()->flags.stunned)
    {
        return;
    }
    if (next_action != nullptr && (next_action->get_post_conditions() & ACS_NO_COWER_TO_STAND))
    {
        return;
    }
    np->char_inst->play_action(ANIMATION_ACTION_COWER_TO_STAND, ANIM_ALIAS_PANIC_COOLDOWN, 1.0f, 0);
    np->block_movement_anim = ANIMATION_ACTION_COWER_TO_STAND;
    np->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    np->block_steering_vector.zero();
}

// 0x005F2230 (:1274)
static animation_action pick_random_dive_anim()
{
    return Dive_anim_actions[vrand() & 1];
}

// How far off a hazard counts as gone: 1 m, or the blast of an explosive projectile when wider.
static float get_hazard_stop_dist(const object* obj)
{
    float limit = 1.0f;
    if (obj->get_obj_type() == OT_ITEM && obj->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        const weapon_info* w_info = static_cast<const projectile*>(obj)->w_info;
        if (w_info->is_explosive() && 1.0f < w_info->m_explosion_info->m_knockdown_radius)
        {
            limit = w_info->m_explosion_info->m_knockdown_radius;
        }
    }
    return limit;
}

// 0x0066D1A0 (:1286)
int dive_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_system* ai = np->get_ai();
    if (human_in_vehicle(np) != nullptr || (human_combat_cover_get_node(np) != nullptr && human_combat_cover_is_stepped_in(np)))
    {
        return -1;
    }
    const ai_belief* belief = goal_setup_data.get_attention_belief(np);
    if (belief == nullptr || goal_setup_data.flags.trapped)
    {
        return -1;
    }
    object* objp = belief->handle != 0xFFFFFFFF ? World.object_from_handle(belief->handle) : nullptr;
    vehicle* vp = objp != nullptr && objp->get_obj_type() == OT_VEHICLE ? static_cast<vehicle*>(objp) : nullptr;
    const bool heavy = vp != nullptr && ai_perception::vehicle_is_heavy(vp);
    if (heavy && ai->persona_flags.heavy_vehicle_always_cower)
    {
        return -1;
    }
    if (vp != nullptr)
    {
        // The player's friends trust slow driving.
        const player* local = player_get_local_player();
        if (local != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == local && local_player_is_friendly(np) && vp->forward_velocity < 10.0f && ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) == nullptr)
        {
            return -1;
        }
        if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            const flyer* fp = World.object_cast<flyer>(vp);
            if (fp == nullptr || !fp->wing_is_detached || 2.0f < fabsf(fp->pos.y / fp->last_velocity.y)) // s until it lands
            {
                return -1;
            }
        }
    }
    const projectile* proj = World.object_from_handle<projectile>(belief->handle);
    const player* squad_player = ai->is_in_player_squad(true);
    if (proj != nullptr && squad_player != nullptr && proj->owner_handle == squad_player->get_handle())
    {
        return -1;
    }
    int cost = 2000;
    const hazard_physics_info& haz = goal_setup_data.haz_info;
    if (!haz.will_impact || (human_team_is_non_combatant(np) && 1.0f < haz.time_to_pass) || (heavy && 1.0f < haz.time_to_pass))
    {
        cost = 10000;
    }
    vector center;
    np->get_center_pos(center);
    if (goal_setup_data.direction * (center - haz.pass_point) < 0.5f)
    {
        return -1;
    }
    return cost;
}

// 0x0066D460 (:1385)
ai_results dive_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set(200); // ms
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    return do_evade_action_enter(np, true);
}

// 0x005F2240 (:1407)
ai_results dive_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (human_weapons_is_firearm_equipped(np) && ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr)
    {
        human_set_combat_ready(np, true, 0);
    }
    if (ai->action_timer.valid())
    {
        if (!ai->action_timer.elapsed() || np->orient.fvec * np->steering_vector < COS_15_DEG)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        ai->stop_moving();
        const float rate = fl_rand_range(0.85f, 1.15f);
        np->char_inst->play_action(pick_random_dive_anim(), ANIM_ALIAS_DIVE, rate, 0x4040);
        human_set_stance(np, HST_PRONE);
        ai->planner.lock_action(true);
        ai->action_timer.invalidate();
    }
    return np->char_inst->is_playing_action(ANIM_ALIAS_DIVE) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00641DB0 (:1446)
ai_results dive_action::cleanup(npc* np, const ai_action*) const
{
    if (np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIM_ALIAS_DIVE, 5, false); // 5: blend frames
    }
    return AI_RESULT_SUCCESS;
}

// 0x00641DE0 (:1458)
int evade_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (human_in_vehicle(np) != nullptr || goal_setup_data.get_attention_belief(np) == nullptr || goal_setup_data.flags.trapped)
    {
        return -1;
    }
    const ai_belief* belief = goal_setup_data.get_attention_belief(np);
    object* objp = belief->handle != 0xFFFFFFFF ? World.object_from_handle(belief->handle) : nullptr;
    const bool heavy = objp != nullptr && objp->get_obj_type() == OT_VEHICLE && ai_perception::vehicle_is_heavy(static_cast<vehicle*>(objp));
    const bool will_impact = goal_setup_data.haz_info.will_impact;
    if (will_impact && heavy && np->get_ai()->persona_flags.heavy_vehicle_always_cower)
    {
        return -1;
    }
    return will_impact ? 2500 : 8000;
}

// 0x0066D4D0 (:1491)
ai_results evade_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    ai->action_timer.set_rand(750, 1000);
    return do_evade_action_enter(np, true);
}

// 0x0066D560 (:1505)
ai_results evade_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (do_evade_action_enter(np, true) == AI_RESULT_FAILURE || !ai->action_timer.valid())
    {
        return AI_RESULT_FAILURE;
    }
    return ai->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// 0x005F23B0 (:1520)
ai_results evade_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != &the_instance && next_action != dive_action::get_instance())
    {
        np->get_ai()->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00641E80 (:1534)
int stay_down_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (human_in_vehicle(np) != nullptr || human_combat_cover_is_using_node(np) || (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF))
    {
        return -1;
    }
    if (!goal_setup_data.haz_info.will_impact)
    {
        return -1;
    }
    object* objp = goal_setup_data.selected_target;
    if (objp != nullptr && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE && World.object_cast<projectile>(objp)->is_thrown_weapon())
    {
        return -1;
    }
    const vector& pass_point = goal_setup_data.haz_info.pass_point;
    // It passes overhead: 1.5 m above, within 1.5 m.
    return 1.5f <= pass_point.y - np->pos.y && vec_xz_dist_squared(pass_point, np->pos) <= 2.25f ? 1300 : -1;
}

// 0x00641F60 (:1571)
ai_results stay_down_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->action_timer.set_rand(200, 300);
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x0066D5B0 (:1582)
ai_results stay_down_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const object* obj = World.object_from_handle(ai->perception.get_attention_handle());
    if (obj == nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->action_timer.elapsed())
    {
        ai->action_timer.set_rand(200, 300);
        hazard_physics_info info;
        get_hazard_physics_info(info, np, obj);
        if (info.speed < 0.1f || (!info.will_impact && get_hazard_stop_dist(obj) < info.distance))
        {
            ai->perception.deprecate_belief(AI_BELIEF_HAZARD, obj);
            return AI_RESULT_SUCCESS;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x0066D6E0 (:1638)
int duck_into_building_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (goal_setup_data.get_attention_belief(np) == nullptr)
    {
        return -1;
    }
    rfg_mover* building = nullptr;
    if (np->get_ai()->perception.is_in_closest_building(&building, 0.0f, 1.25f) || building == nullptr)
    {
        return -1;
    }
    my_data.building_target = building;
    navpoint* next;
    navpoint* npp = ai_building_find_closest_transition_from_outside(np->pos, building, &next);
    if (npp == nullptr || vec_dist_squared(np->pos, npp->pos) > 25.0f) // 5 m
    {
        return -1;
    }
    my_data.use_target = npp;
    my_data.pos = npp->pos;
    return goal_setup_data.haz_info.will_impact && 2.0f <= goal_setup_data.haz_info.time_to_pass ? 1200 : 5000;
}

// 0x005F23E0 (:1679)
void duck_into_building_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_destination = setup_data.pos;
}

// 0x00641FC0 (:1685)
ai_results duck_into_building_action::enter(npc* np, const ai_action*) const
{
    np->get_ai()->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    return AI_RESULT_SUCCESS;
}

// 0x006A9DE0 (:1695)
ai_results duck_into_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (vec_dist_squared(np->pos, ai->desired_destination) < 0.15f)
    {
        return AI_RESULT_SUCCESS;
    }
    if (!ai->is_pathing_to(ai->desired_destination, 0.25f, 1.0f) && ai->pathfind_to(ai->desired_destination, HMS_SPRINT, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false) > AI_PF_RAGDOLLED)
    {
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F2410 (:1718)
int fall_back_to_human_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 500;
}

// 0x006A9E70 (:1723)
ai_results fall_back_to_human_action::enter(npc* np, const ai_action* last_action) const
{
    ai_combat_fire_at_target_if_available(np, false, false);
    if (last_action != this)
    {
        np->hflags.disable_all_path_smoothing = 1;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006BB0C0 (:1734)
ai_results fall_back_to_human_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->desired_follow_target == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* objp = World.object_from_handle(ai->desired_follow_target);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return AI_RESULT_FAILURE;
    }
    human* target = static_cast<human*>(objp);
    ai_combat_fire_at_target_if_available(np, false, false);
    const float dist_sq = vec_dist_squared(target->pos, np->pos);
    if (6.25f <= dist_sq) // 2.5 m
    {
        if (!ai->is_pathing_to(target->pos, 3.0625f, 1.0f) && ai->can_pathfind() && ai->pathfind_to(target, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_HIGH, nullptr) > AI_PF_RAGDOLLED)
        {
            ai->planner.avoid_action(this, 5000);
            return AI_RESULT_FAILURE;
        }
    }
    else if (dist_sq < 3.0625f) // 1.75 m
    {
        ai->stop_moving();
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F2420 (:1765)
ai_results fall_back_to_human_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->hflags.disable_all_path_smoothing = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2440 (:1777)
int fled_building_rule::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 5000;
}

// 0x0066D7D0 (:1782)
int flee_building_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node* next_action) const
{
    rfg_mover* building = nullptr;
    if (!np->get_ai()->perception.is_in_closest_building(&building, 0.0f, 1.25f))
    {
        return -1;
    }
    my_data.building_target = building;
    navpoint* npp = get_leave_building_transition_navpoint(np, next_action, building);
    if (npp == nullptr)
    {
        return -1;
    }
    my_data.use_target = npp;
    my_data.pos = npp->pos;
    if (next_action == nullptr)
    {
        const nav_cell* ncp = bb_pf_get_nav_cell(npp->pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        if (ncp != nullptr)
        {
            nav_cell_get_random_point(my_data.pos, ncp);
        }
    }
    return 2600;
}

// 0x005F2450 (:1814)
void flee_building_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_destination = setup_data.pos;
    ai->desired_leave_building_transition = setup_data.use_target->get_handle();
}

// 0x00642030 (:1821)
ai_results flee_building_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        const object* transition = ai->desired_leave_building_transition != 0xFFFFFFFF ? World.object_from_handle(ai->desired_leave_building_transition) : nullptr;
        if (transition == nullptr || transition->get_obj_type() != OT_NAVPOINT || ai->perception.get_attention_handle() != transition->get_parent())
        {
            ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        }
        else
        {
            ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_USE_CURRENT_MODE, ATTENTION_AWARENESS_USE_CURRENT_MODE);
        }
        if (!human_team_is_non_combatant(np) && !ai->should_react(REACTION_FLEE))
        {
            np->play_voice_line(VOICE_LINE_PANIC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    return AI_RESULT_SUCCESS;
}

// Tired after 2 s of it, or while tired already: slows the npc down for a while.
static void update_exhaustion(ai_system* ai)
{
    if (!ai->exhaustion_timer.elapsed() || (ai->planner.get_time_in_current_action() >= 2000))
    {
        ai->exhaustion_timer.set_rand(1500, 3000);
    }
}

// 0x006A9EA0 (:1843)
ai_results flee_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!ai->exhaustion_timer.elapsed() || ai->planner.get_time_in_current_action() >= 2000)
    {
        if (ai->custom_behavior != AI_SB_TRAPPED_IN_BUILDING && ai->custom_behavior != AI_SB_TRAPPED_IN_JAIL)
        {
            ai->exhaustion_timer.set_rand(1500, 3000);
        }
    }
    if (!ai->is_pathing_to(ai->desired_destination, 0.25f, 1.0f) && ai->pathfind_to(ai->desired_destination, HMS_SPRINT, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, ai->desired_goal_nav_cell_hint_handle, false) > AI_PF_RAGDOLLED)
    {
        return AI_RESULT_FAILURE;
    }
    return ai->perception.is_in_closest_building(nullptr, 0.0f, 1.25f) ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00686EC0 (:1870)
int flee_to_defensive_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const ai_belief* belief = goal_setup_data.get_attention_belief(np);
    const object* flee_obj = goal_setup_data.selected_target;
    if (belief == nullptr && flee_obj == nullptr)
    {
        return -1;
    }
    const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
    if (ncp == nullptr || (ncp->flags & NAV_CELL_FLAG_IMPASSABLE) != 0 || !nav_cell_is_point_in_cell_column(ncp, np->pos))
    {
        return -1;
    }
    float ground;
    if (!havok_terrain_height(ground, np->pos.x, np->pos.z, false, nullptr) || ground + 1.25f < np->pos.y)
    {
        return -1;
    }
    cover_node* node;
    if (ai->planner.get_current_action() == take_defensive_cover_action::get_instance() && (node = human_combat_cover_get_node(np)) != nullptr)
    {
        if (flee_cover_node_is_safe(node, np, flee_obj))
        {
            return 0;
        }
    }
    else
    {
        node = get_defensive_cover_node(np, belief, flee_obj, 15.0f, 25.0f);
        if (node == nullptr)
        {
            return -1;
        }
        if (flee_cover_node_is_safe(node, np, flee_obj))
        {
            my_data.use_target = node;
            const float dist = vec_dist(node->pos, node->closest_point);
            if (0.45f <= dist)
            {
                my_data.pos = node->closest_point + (node->pos - node->closest_point) / dist * 0.45f;
            }
            else
            {
                my_data.pos = node->pos;
            }
            if (in_defensive_cover(np, my_data.pos) && human_combat_cover_get_node(np) == node)
            {
                return 0;
            }
            return ai->persona_flags.flee_to_defensive_cover ? 1001 : 21001;
        }
    }
    stop_hiding_in_defensive_cover(np);
    return -1;
}

// 0x005F2490 (:1938)
void flee_to_defensive_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_goal_pos = setup_data.pos;
    human_combat_cover_reserve_node(np, static_cast<cover_node*>(setup_data.use_target));
}

static void play_panic_and_rearm(npc* np)
{
    np->play_voice_line(VOICE_LINE_PANIC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    if (np->current_team == HUMAN_TEAM_EDF && np->get_equipped_weapon() == nullptr)
    {
        human_equip_inventory_item(np, np->get_last_equipped_weapon());
    }
}

// 0x00642130 (:1945)
ai_results flee_to_defensive_cover_action::enter(npc* np, const ai_action* last_action) const
{
    np->get_ai()->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_USE_CURRENT_MODE);
    if (last_action != this)
    {
        play_panic_and_rearm(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A9FD0 (:1966)
ai_results flee_to_defensive_cover_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    update_exhaustion(ai);
    if (vec_dist_squared(np->pos, ai->desired_goal_pos) <= 0.0625f) // 0.25 m
    {
        ai->stop_moving();
        return AI_RESULT_SUCCESS;
    }
    if (vec_dist_squared(np->pos, node->pos) < 1.0f) // 1 m: the last step is direct
    {
        vector dir = node->pos - np->pos;
        ai->set_direct_movement_fast(dir.copy_flatten_normalize_safe(), HMS_SPRINT, HST_STAND, -1.0f);
        return AI_RESULT_IN_PROGRESS;
    }
    if (!ai->is_pathing_to(ai->desired_goal_pos, 0.25f, 1.0f))
    {
        uint64_t hint = 0xFFFFFFFFFFFFFFFFull;
        bool disable_smoothing = false;
        if (node->flags.on_mover == 0)
        {
            hint = node->closest_nav_cell_handle;
            if (hint == 0)
            {
                ai->planner.avoid_action(this, 5000);
                return AI_RESULT_FAILURE;
            }
            disable_smoothing = true;
        }
        if (ai_pf_to_result(ai->pathfind_to(ai->desired_goal_pos, HMS_SPRINT, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, hint, disable_smoothing)) == AI_RESULT_FAILURE)
        {
            human_combat_cover_abandon_node(np);
            node->disable(COVER_DISABLED_UNREACHABLE, 10000);
            return AI_RESULT_FAILURE;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// In the "sniper hunter" mission only the npcs the mission set up flee.
static bool sniper_hunter_keeps_npc_out()
{
    return mission_is_active() && v_stricmp(mission_get_name(), "sniper hunter") == 0;
}

// 0x005F24D0 (:2037)
int take_defensive_cover_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return sniper_hunter_keeps_npc_out() && !np->get_ai()->persona_flags.flee_to_defensive_cover ? -1 : 200;
}

// 0x005F2520 (:2051)
ai_results take_defensive_cover_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        ai_system* ai = np->get_ai();
        ai->stop_moving();
        const cover_node* node = human_combat_cover_get_node(np);
        np->play_random_cower_animation(node != nullptr && node->flags.crouch != 0);
        ai->exhaustion_timer.set(0);
        human_override_default_animation_state(np, true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2580 (:2082)
ai_results take_defensive_cover_action::do_action(npc* np) const
{
    const cover_node* node = human_combat_cover_get_node(np);
    if (node == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    np->steering_vector = -node->orient.fvec;
    return AI_RESULT_IN_PROGRESS;
}

// 0x006421E0 (:2099)
ai_results take_defensive_cover_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_maybe_play_cower_to_stand_animation(np, next_action);
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2600 (:2114)
int leave_demolition_area_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return human_in_vehicle(np) != nullptr ? -1 : 500;
}

// 0x006871B0 (:2127)
ai_results leave_demolition_area_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const object* raid_object = activity_raid_get_raid_object();
    if (raid_object == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_system* ai = np->get_ai();
    ai->perception.pay_attention(raid_object->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    return activity_raid_get_exit_point(np->pos, ai->desired_destination) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006AA1F0 (:2157)
ai_results leave_demolition_area_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai->pathfind_to(ai->desired_destination, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false) > AI_PF_RAGDOLLED)
    {
        return AI_RESULT_FAILURE;
    }
    const vector target = activity_raid_get_target_pos();
    const float radius = activity_raid_get_demolition_radius();
    return vec_xz_dist_squared(np->pos, target) <= radius * radius ? AI_RESULT_IN_PROGRESS : AI_RESULT_SUCCESS;
}

// 0x00687220 (:2190)
int leave_demolition_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr)
    {
        const ai_vehicle_seat_types seat_type = ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED);
        if (seat_type != AI_VEHICLE_SEAT_TYPE_UNDEFINED && ai_vehicle_is_usable(vp, np, seat_type))
        {
            return 500;
        }
    }
    return -1;
}

// 0x005F2620 (:2209)
void leave_demolition_vehicle_action::commit_setup_data(npc* np, const ai_action_setup_data&) const
{
    np->get_ai()->desired_vehicle_speed = 10.0f; // m/s
}

// 0x00687270 (:2215)
ai_results leave_demolition_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_vehicle_seat_types seat_type = ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED);
    if (seat_type == AI_VEHICLE_SEAT_TYPE_UNDEFINED || !ai_vehicle_is_usable(vp, np, seat_type))
    {
        return AI_RESULT_FAILURE;
    }
    if (seat_type == AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
        ai_vehicle_override_speed(vp, ai->desired_vehicle_speed);
        if (last_action != this)
        {
            ai->action_timer.set(0);
            ai->desired_destination = vp->pos;
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x006AA280 (:2252)
ai_results leave_demolition_vehicle_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_vehicle_seat_types seat_type = ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED);
    if (seat_type == AI_VEHICLE_SEAT_TYPE_UNDEFINED || !ai_vehicle_is_usable(vp, np, seat_type))
    {
        return AI_RESULT_FAILURE;
    }
    const object* building = World.object_from_handle(ai->perception.get_attention_handle());
    if (building == nullptr || building->get_obj_type() != OT_MOVER || building->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return AI_RESULT_FAILURE;
    }
    if (seat_type == AI_VEHICLE_SEAT_TYPE_DRIVER)
    {
        if (ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, false, 3.0f, true, false, false, true) != AI_RESULT_SUCCESS && !ai->action_timer.elapsed())
        {
            return AI_RESULT_IN_PROGRESS;
        }
        ai->action_timer.set_rand(2000, 2500);
        vector away = np->pos - building->pos;
        away.flatten_mag_and_normalize_safe();
        vector dir, end_pos;
        ai->desired_destination = combat_maneuver_get_retreat_dir(dir, end_pos, vp, away, 10.0f, 30.0f) == AI_RESULT_SUCCESS ? end_pos : vp->pos;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00687330 (:2325)
int flee_to_safe_building_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    const bool curfew = goal_setup_data.goal == high_alert_curfew_goal::get_instance();
    if (!Allow_flee_to_safe_building && !curfew)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const object* flee_objp = nullptr;
    if (!curfew)
    {
        const ai_belief* belief = goal_setup_data.get_attention_belief(np);
        if (belief == nullptr)
        {
            return -1;
        }
        flee_objp = World.object_from_handle(belief->handle);
        if (flee_objp == nullptr)
        {
            return -1;
        }
    }
    rfg_mover* structure = nullptr;
    navpoint* ref_point;
    if (ai->perception.is_in_closest_building(np, &structure, 0.0f, 1.25f))
    {
        if (!ai->perception.building_is_safe(structure, true) || (structure->mover_type_bit_flags & 0x10) != 0)
        {
            return -1;
        }
        ref_point = ai_building_find_closest_navpoint(np->pos, structure, true, nullptr);
    }
    else
    {
        ref_point = find_nearest_safe_trans_point(np, flee_objp);
    }
    if (ref_point == nullptr)
    {
        return -1;
    }
    navpoint* new_navp = ai->get_safe_building_nav(true);
    bool recalculate;
    if (new_navp == nullptr || new_navp->set != ref_point->set)
    {
        new_navp = select_safe_path_node(ref_point);
        if (new_navp == nullptr)
        {
            return -1;
        }
        recalculate = true;
    }
    else
    {
        recalculate = false;
    }
    if (!flee_navpoint_is_safe(new_navp, np, flee_objp))
    {
        stop_fleeing_to_safe_building(np);
        return -1;
    }
    if (!recalculate)
    {
        const vector dest(new_navp->pos.x + ai->flee_destination_offset.x * new_navp->radius, new_navp->pos.y + new_navp->radius * 0.0f, new_navp->pos.z + ai->flee_destination_offset.y * new_navp->radius);
        if (vec_dist_squared(np->pos, dest) < 0.122499995f) // 0.35 m
        {
            return 0;
        }
    }
    my_data.flags.recalculate = recalculate;
    my_data.use_target = new_navp;
    return ai->persona_flags.flee_to_safe_building ? 1000 : 21000;
}

// 0x005F2640 (:2422): a uniform point of the navpoint's disc.
void flee_to_safe_building_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->flee_destination_handle = setup_data.use_target->get_handle();
    if (setup_data.flags.recalculate)
    {
        const float radius = (float)sqrt((double)fl_rand());
        const float angle = (float)((double)fl_rand() * TWO_PI_D);
        ai->flee_destination_offset.x = (float)cos((double)angle) * radius;
        ai->flee_destination_offset.y = (float)sin((double)angle) * radius;
    }
}

// 0x0066D890 (:2435)
ai_results flee_to_safe_building_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (ai->get_safe_building_nav(true) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const ai_goal* goal = ai->planner.get_current_goal();
    if (goal != nullptr && goal->pursues_condition(ai_plan_conditions(109))) // the curfew goals: no one to look at
    {
        ai->perception.stop_paying_attention(true);
    }
    else
    {
        if (ai->perception.get_attention_belief() == nullptr || ai->perception.get_attention_target() == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    play_panic_and_rearm(np);
    return AI_RESULT_SUCCESS;
}

// 0x006AA460 (:2490)
ai_results flee_to_safe_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const navpoint* npp = ai->get_safe_building_nav(true);
    if (npp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vector dest(npp->pos.x + ai->flee_destination_offset.x * npp->radius, npp->pos.y + npp->radius * 0.0f, npp->pos.z + ai->flee_destination_offset.y * npp->radius);
    if (vec_dist_squared(np->pos, dest) < 0.122499995f) // 0.35 m
    {
        return AI_RESULT_SUCCESS;
    }
    // Original bug kept: the check is against the navpoint, the path goes to the offset point in its disc.
    if (!ai->is_pathing_to(npp->pos, 0.25f, 1.0f) && ai->pathfind_to(dest, HMS_SPRINT, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false) > AI_PF_RAGDOLLED)
    {
        ai->planner.avoid_action(this, 10000);
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F26D0 (:2523)
ai_results flee_to_safe_building_action::cleanup(npc*, const ai_action*) const
{
    return AI_RESULT_SUCCESS;
}

// 0x005F26E0 (:2538)
int hide_in_safe_building_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->npc_flags.bureaucrat != 0)
    {
        return -1;
    }
    return sniper_hunter_keeps_npc_out() && !np->get_ai()->persona_flags.flee_to_safe_building ? -1 : 200;
}

// 0x0066DA20 (:2561)
ai_results hide_in_safe_building_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    if (ai->get_safe_building_nav(true) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    np->play_random_cower_animation(false);
    ai->exhaustion_timer.set(0);
    human_override_default_animation_state(np, true);
    return AI_RESULT_SUCCESS;
}

// 0x0066DA80 (:2590)
ai_results hide_in_safe_building_action::do_action(npc* np) const
{
    return np->get_ai()->get_safe_building_nav(true) != nullptr ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x00642210 (:2606)
ai_results hide_in_safe_building_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_maybe_play_cower_to_stand_animation(np, next_action);
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2740 (:2620)
int cower_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100000;
}

// 0x005F2750 (:2625)
ai_results cower_action::enter(npc* np, const ai_action* last_action) const
{
    human_override_default_animation_state(np, true);
    if (last_action != this)
    {
        np->play_random_cower_animation(false);
    }
    np->get_ai()->exhaustion_timer.set(0);
    return AI_RESULT_SUCCESS;
}

// 0x005F27A0 (:2642)
ai_results cower_action::do_action(npc*) const
{
    return AI_RESULT_IN_PROGRESS;
}

// 0x00642240 (:2647)
ai_results cower_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        ai_maybe_play_cower_to_stand_animation(np, next_action);
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F27B0 (:2659)
int cower_incoming_action::get_cost(const npc*, const ai_goal_setup_data& goal_setup_data) const
{
    const object* objp = goal_setup_data.selected_target;
    return objp != nullptr && (objp->get_obj_type() == OT_VEHICLE || objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE) ? 99000 : -1;
}

// 0x005F27E0 (:2675)
ai_results cower_incoming_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action == this)
    {
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(14, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false); // state 14: cowering
        return AI_RESULT_SUCCESS;
    }
    const object* obj = World.object_from_handle(ai->perception.get_attention_handle());
    if (obj == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vector to_obj;
    vec_normalized_dir(to_obj, obj->pos, np->pos);
    const float up = to_obj.y;
    to_obj.flatten_normalize_safe();
    const float forward = np->orient.fvec * to_obj;
    const float right = np->orient.rvec * to_obj;
    // The transition faces the danger; the matrix rfg.exe makes from to_obj here (0x005F2915) goes unused.
    int state = 14; // cowering
    int action = -1;
    if (up < 0.5f)
    {
        if (forward < 0.70710677f)
        {
            action = right < 0.0f ? 353 : 354; // numeric in rfg.exe: from the left, from the right
        }
        else
        {
            action = 352;
        }
    }
    else
    {
        state = 10; // from overhead
    }
    ai->exhaustion_timer.set(0);
    human_override_default_animation_state(np, true);
    human_set_stance(np, HST_STAND);
    np->char_inst->set_state(index_alias(state, ANIM_ALIAS_NONE), index_alias(action, action == -1 ? ANIM_ALIAS_NONE : ANIM_ALIAS_TRANSITION_TO_COWERING), 0.3f, false);
    ai->planner.lock_action(true);
    ai->action_timer.set(0);
    ai->action_substate = 0;
    return AI_RESULT_SUCCESS;
}

// 0x0066DAA0 (:2753)
ai_results cower_incoming_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (!np->char_inst->is_playing_action(ANIM_ALIAS_TRANSITION_TO_COWERING))
    {
        ai->planner.lock_action(false);
    }
    const object* obj = World.object_from_handle(ai->perception.get_attention_handle());
    if (obj == nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    if (0 < ai->action_substate)
    {
        return ai->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_timer.elapsed())
    {
        ai->action_timer.set_rand(200, 300);
        hazard_physics_info info;
        get_hazard_physics_info(info, np, obj);
        if (info.speed < 0.1f || (!info.will_impact && get_hazard_stop_dist(obj) < info.distance))
        {
            ai->perception.deprecate_belief(AI_BELIEF_HAZARD, obj);
            ai->action_substate = 1;
        }
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00642270 (:2813)
ai_results cower_incoming_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->char_inst->rdd.is_loaded())
    {
        human_override_default_animation_state(np, false);
        np->char_inst->stop_action(ANIM_ALIAS_TRANSITION_TO_COWERING, 10, true); // 10: blend frames
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2A60 (:2828)
int surrender_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100000;
}

// 0x006422C0 (:2833)
ai_results surrender_action::enter(npc* np, const ai_action*) const
{
    ai_system* ai = np->get_ai();
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    ai->stop_moving();
    human_set_stance(np, HST_STAND);
    return AI_RESULT_SUCCESS;
}

// 0x005F2A70 (:2849)
ai_results surrender_action::do_action(npc* np) const
{
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F2A90 (:2858)
ai_results surrender_action::cleanup(npc*, const ai_action*) const
{
    return AI_RESULT_SUCCESS;
}

// 0x005F2AA0 (:2869)
int fetal_position_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 100000;
}

// 0x005F2AB0 (:2874)
ai_results fetal_position_action::do_action(npc* np) const
{
    if (np->hit_points / (float)np->max_hit_points < 0.5f)
    {
        np->play_voice_line(VOICE_LINE_PATROL_BEAT_CIVILIAN_SUBMISSIVE_RESPONSE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    if (np->combat.beaten_up_timer.valid() && np->combat.beaten_up_timer.time_since() <= 10000) // ms
    {
        return AI_RESULT_IN_PROGRESS;
    }
    np->hflags.in_fetal_position = 0;
    return AI_RESULT_SUCCESS;
}

// 0x005F2B50 (:2894)
int run_around_panicked_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10000;
}

// 0x005F2B60 (:2899)
ai_results run_around_panicked_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        human_set_combat_ready(np, false, 0);
        ai->action_timer.set(0);
        ai->action_substate = 0;
    }
    np->play_voice_line(VOICE_LINE_BOMB, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    return AI_RESULT_SUCCESS;
}

// Runs in a new direction: any, or when blocked, within the half circle turned from the way it was going (0x00642502).
static void set_panic_direction(npc* np, bool blocked)
{
    double angle;
    if (blocked)
    {
        const double spread = (double)fl_rand() * PI_D;
        angle = (double)(float)atan2((double)np->steering_vector.z, (double)np->steering_vector.x) + HALF_PI_D + spread;
    }
    else
    {
        angle = (double)fl_rand() * 2.0 * PI_D;
    }
    const float final_angle = (float)angle;
    const vector dir((float)cos((double)final_angle), 0.0f, (float)sin((double)final_angle));
    np->get_ai()->set_direct_movement_fast(dir, HMS_SPRINT, HST_STAND, -1.0f);
}

// Whether the npc is really going where it steers: at 1 m/s or more, within 30 degrees.
static bool is_moving_as_steered(const npc* np)
{
    return 1.0f <= np->actual_velocity_mag && COS_30_DEG <= np->steering_vector * np->actual_velocity.copy_flatten_normalize_safe();
}

// 0x00642350 (:2918)
ai_results run_around_panicked_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const int time_in_action = ai->planner.get_time_in_current_action();
    if (Ai_removes_attached_projectile_time * 1000 < time_in_action)
    {
        human_weapons_unattach_projectiles(np);
        for (ai_belief* belief = ai->perception.get_first_belief(AI_BELIEF_PROJECTILE); belief != nullptr; belief = ai->perception.get_next_belief(belief))
        {
            belief->flags &= ~BF_ATTACHED_TO_ME;
        }
        return AI_RESULT_SUCCESS;
    }
    if (is_moving_as_steered(np))
    {
        ai->action_substate = 0;
    }
    else
    {
        if (ai->action_timer.time_until() > 250) // ms
        {
            ai->action_timer.set(250);
        }
        ai->action_substate = 1;
    }
    if (ai->action_timer.elapsed())
    {
        ai->action_timer.set_rand(2000, 3000);
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(16, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false); // state 16: panicked run
        set_panic_direction(np, ai->action_substate >= 1);
        ai->action_substate = 0;
    }
    return AI_RESULT_IN_PROGRESS;
}

static void clear_temp_anim_variant(const npc* np)
{
    const character_instance* char_inst = np->char_inst;
    if (char_inst != nullptr && char_inst->vp != nullptr)
    {
        vanim_set_temp_animation_set_variant(char_inst->vp, ANIM_SET_VARIANT_NONE);
    }
}

// 0x006425C0 (:2976)
ai_results run_around_panicked_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        if (np->hflags.override_default_anim_state != 0)
        {
            ai_maybe_play_cower_to_stand_animation(np, next_action);
            human_override_default_animation_state(np, false);
        }
        clear_temp_anim_variant(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2BC0 (:2997)
int on_fire_roll_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    if (np->cur_stance == HST_PRONE)
    {
        return 100;
    }
    return np->get_ai()->persona_flags.on_fire_panic ? 1000 : 500;
}

// 0x005F2C00 (:3011)
ai_results on_fire_roll_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        human_set_combat_ready(np, false, 0);
        if (np->cur_stance == HST_CROUCH)
        {
            human_set_stance(np, HST_STAND);
        }
        ai->action_timer.set_rand(3000, 5000);
        ai->action_substate = 0;
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// True when the fire has been out long enough to stop: burns on 3 to 5 s after going out.
static bool update_fire_out(npc* np)
{
    ai_system* ai = np->get_ai();
    if (np->hflags.on_fire != 0)
    {
        return false;
    }
    if (ai->action_substate < 1)
    {
        ai->action_substate = 1;
        ai->action_timer.set_rand(3000, 5000);
        return false;
    }
    return ai->action_timer.elapsed();
}

// 0x00642640 (:3042)
ai_results on_fire_roll_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (update_fire_out(np))
    {
        ai->exit_on_fire();
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_timer.elapsed() && np->cur_stance != HST_PRONE)
    {
        ai->stop_moving();
        human_set_stance(np, HST_PRONE);
        np->char_inst->play_action(ANIMATION_ACTION_STAND_GO_PRONE, ANIM_ALIAS_NONE, 1.0f, 0x4040);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F2C80 (:3077)
ai_results on_fire_roll_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        clear_temp_anim_variant(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2CE0 (:3095)
int on_fire_panic_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->persona_flags.on_fire_panic ? 500 : 1000;
}

// 0x005F2D10 (:3106)
ai_results on_fire_panic_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        human_set_combat_ready(np, false, 0);
        ai->action_timer.set(0);
        ai->action_substate = 0;
        ai->planner.lock_action(true);
    }
    return AI_RESULT_SUCCESS;
}

// 0x00642700 (:3131)
ai_results on_fire_panic_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (update_fire_out(np))
    {
        ai->exit_on_fire();
        return AI_RESULT_IN_PROGRESS;
    }
    const bool blocked = !is_moving_as_steered(np);
    if (blocked && ai->action_timer.time_until() > 250) // ms
    {
        ai->action_timer.set(250);
    }
    if (ai->action_timer.elapsed())
    {
        ai->action_timer.set_rand(2000, 3000);
        set_panic_direction(np, blocked);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005F2D70 (:3193)
ai_results on_fire_panic_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        clear_temp_anim_variant(np);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2DD0 (:3210)
int flee_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 10000;
}

// 0x005F2DE0 (:3215)
ai_results flee_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        human_set_combat_ready(np, false, 0);
        ai->action_timer.set(0);
        ai->action_substate = 0;
        ai->desired_destination = np->pos;
        ai->flags.cannot_retreat = false;
        ai->flags.off_navmesh = false;
        if (!human_team_is_non_combatant(np) && !ai->should_react(REACTION_FLEE))
        {
            np->play_voice_line(VOICE_LINE_PANIC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        if (np->current_team == HUMAN_TEAM_EDF && np->get_equipped_weapon() == nullptr)
        {
            human_equip_inventory_item(np, np->get_last_equipped_weapon());
        }
    }
    return AI_RESULT_SUCCESS;
}

// Whether the npc is on the nav mesh; steps its last visited cell along when it crossed into a neighbor.
static bool update_flee_nav_cell(npc* np)
{
    const nav_cell* cell = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
    if (cell == nullptr)
    {
        return false;
    }
    if (nav_cell_is_point_in_cell_column(cell, np->pos) || nav_cell_get_closest_xz_dist_squared_to_point(cell, np->pos) <= 0.25f)
    {
        return true;
    }
    for (int edge = 0; edge < 3; edge++)
    {
        const nav_cell* adjacent = cell->adjacent_cells[edge].to_pointer();
        if (adjacent != nullptr && nav_cell_is_point_in_cell_column(adjacent, np->pos))
        {
            np->pf.last_visited_nav_cell_handle = nav_cell_get_handle(adjacent);
            return true;
        }
    }
    return false;
}

// 0x006AA5D0 (:3252)
ai_results flee_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    const ai_goal* goal = ai->planner.get_current_goal();
    if (goal == flee_debris_goal::get_instance())
    {
        for (ai_belief* belief = ai->perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS); belief != nullptr; belief = ai->perception.get_next_belief(belief))
        {
            belief->flags |= BF_FROM_ORDERS;
        }
    }
    vector flee_pos, flee_dir;
    const object* flee_obj;
    const ai_results status = get_flee_status(flee_pos, flee_dir, flee_obj, np);
    if (status != AI_RESULT_IN_PROGRESS)
    {
        return status;
    }
    bool pick_now = ai->action_timer.elapsed();
    if (!pick_now)
    {
        if (flee_obj != nullptr)
        {
            vector bmin, bmax;
            flee_obj->get_local_bbox(bmin, bmax);
            // Original quirk kept: the obj's xz size is added to a squared distance limit.
            if (vec_xz_dist(bmin, bmax) + 100.0f <= vec_dist_squared(flee_pos, np->pos))
            {
                ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_USE_CURRENT_MODE);
            }
            else
            {
                ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_USE_CURRENT_MODE);
            }
        }
        if (ai->flags.cannot_retreat)
        {
            return AI_RESULT_IN_PROGRESS;
        }
        const bool on_mesh = update_flee_nav_cell(np);
        if (ai->flags.off_navmesh)
        {
            if (on_mesh)
            {
                ai->flags.off_navmesh = false;
            }
        }
        else if (!on_mesh)
        {
            ai->flags.off_navmesh = true;
            ai->action_timer.set(0);
            pick_now = true;
        }
        if (!pick_now && 0.0f < np->steering_vector * (ai->desired_destination - np->pos))
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    if (flee_obj != nullptr && flee_obj->get_obj_type() == OT_VEHICLE)
    {
        nav_cell_detour_remove_convex_hull_for_request(static_cast<const vehicle*>(flee_obj)->m_nav_cell_detour_request_handle);
    }
    vector dir, end_pos;
    uint64_t hint = 0xFFFFFFFFFFFFFFFFull;
    const ai_results retreat = combat_maneuver_get_retreat_dir_unified(dir, end_pos, np, flee_dir, 2.0f, 20.0f, ai->action_substate, &hint);
    if (retreat == AI_RESULT_IN_PROGRESS || (retreat == AI_RESULT_SUCCESS && np->cur_movement_state > HMS_WALK && np->steering_vector * dir < 0.0f))
    {
        ai->action_substate++;
        return AI_RESULT_IN_PROGRESS;
    }
    if (retreat == AI_RESULT_SUCCESS)
    {
        human_override_default_animation_state(np, false);
        ai->flags.cannot_retreat = false;
        ai->desired_destination = end_pos;
        ai->set_direct_movement_to_pos(end_pos, hint, HMS_SPRINT, HST_STAND, np->cur_movement_state < HMS_RUN);
    }
    else if (!ai->flags.cannot_retreat)
    {
        ai->stop_moving();
        ai->flags.cannot_retreat = true;
        const int turn_alias = 0.0f < np->orient.rvec * flee_dir ? 1003 : 1002; // numeric in rfg.exe: turn right, turn left
        matrix face_threat;
        face_threat.make(-flee_dir);
        if (np->get_equipped_weapon() != nullptr)
        {
            ai->planner.avoid_action(this, 5000);
            return AI_RESULT_FAILURE;
        }
        human_override_default_animation_state(np, true);
        np->play_random_cower_animation(false);
        np->char_inst->set_action_dest_orient(np->orient, face_threat, turn_alias, -1.0f);
    }
    ai->action_substate = 0;
    ai->action_timer.set_rand(2000, ai->flags.cannot_retreat ? 4000 : 2500);
    if (goal == flee_debris_goal::get_instance() || ai->flags.cannot_retreat)
    {
        ai->exhaustion_timer.set(0);
    }
    else
    {
        update_exhaustion(ai);
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00642900 (:3431)
ai_results flee_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->hflags.override_default_anim_state != 0)
    {
        ai_maybe_play_cower_to_stand_animation(np, next_action);
        human_override_default_animation_state(np, false);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006AAC40 (:3450)
int flee_in_vehicle_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) == AI_VEHICLE_SEAT_TYPE_DRIVER && ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER))
    {
        return np->npc_flags.ambient_spawn != 0 ? 500 : -1;
    }
    return -1;
}

// The point to flee from: the believed threat, or behind the given direction; false when there is no reason to go on.
static bool get_vehicle_flee_pos(vector& flee_pos, bool& flee_from_heavy_vehicle, const npc* np, const vector& no_threat_dir)
{
    const ai_belief* belief = ai_get_flee_to_safety_belief(np);
    if (belief == nullptr && !Vehicle_ai_panic_drivers)
    {
        return false;
    }
    flee_from_heavy_vehicle = false;
    if (belief == nullptr)
    {
        flee_pos = np->pos - no_threat_dir * 10.0f; // m
        return true;
    }
    flee_pos = belief->pos;
    const object* objp = World.object_from_handle(belief->handle);
    if (objp != nullptr)
    {
        flee_pos = objp->pos;
    }
    flee_from_heavy_vehicle = belief->get_type() == AI_BELIEF_VEHICLE;
    return true;
}

// 0x006875A0 (:3467)
ai_results flee_in_vehicle_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    vehicle_ai_clear_speed_override(&vp->vai);
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    vector flee_pos;
    bool flee_from_heavy_vehicle;
    if (!get_vehicle_flee_pos(flee_pos, flee_from_heavy_vehicle, np, vp->orient.fvec))
    {
        return AI_RESULT_FAILURE;
    }
    object_path_road* road = object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads != 0, 60.0f, false, false); // 60 m
    return pick_vehicle_flee_destination(vp, road, ai->desired_destination, ai->action_substate, flee_pos, flee_from_heavy_vehicle) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006AACB0 (:3515)
ai_results flee_in_vehicle_action::do_action(npc* np) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (vp->vai.vai_flags.obstructed_by_player == 0)
    {
        vehicle_ai_clear_speed_override(&vp->vai);
    }
    else if (vp->vai.ambient_wait_timer.valid())
    {
        if (vp->vai.ambient_wait_timer.elapsed())
        {
            vp->vai.ambient_wait_timer.invalidate();
            vp->vai.stuck.stuck_threshold_time.invalidate();
            vp->vai.stuck.extended_stuck_time.invalidate();
        }
        else
        {
            vehicle_ai_set_speed_override(&vp->vai, 0.0f);
        }
    }
    ai_system* ai = np->get_ai();
    if (Vehicle_ai_panic_drivers)
    {
        ai->action_substate = 0;
    }
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, ai->action_substate != 0, 10.0f, true, false, false, true);
    if (result != AI_RESULT_SUCCESS)
    {
        return result;
    }
    object_path_road* road = ai->action_substate != 0 ? object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads != 0, 60.0f, false, false) : nullptr;
    vector flee_pos;
    bool flee_from_heavy_vehicle;
    // Original quirk kept: with no threat this is behind the npc's own velocity, not the vehicle's facing as in enter.
    if (!get_vehicle_flee_pos(flee_pos, flee_from_heavy_vehicle, np, np->velocity))
    {
        return AI_RESULT_FAILURE;
    }
    return pick_vehicle_flee_destination(vp, road, ai->desired_destination, ai->action_substate, flee_pos, flee_from_heavy_vehicle) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x005F2EB0 (:3599)
ai_results flee_in_vehicle_action::cleanup(npc* np, const ai_action*) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr)
    {
        vehicle_ai_clear_speed_override(&vp->vai);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006AAF40 (:3617)
int flee_in_vehicle_xtreme_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const vehicle* vp = human_in_vehicle(np);
    const object* target = goal_setup_data.selected_target;
    if (vp != nullptr && ai_vehicle_get_seat_type(np, vp, AI_VEHICLE_SEAT_TYPE_UNDEFINED) == AI_VEHICLE_SEAT_TYPE_DRIVER && ai_vehicle_is_usable(vp, np, AI_VEHICLE_SEAT_TYPE_DRIVER) && ai_vehicle_is_seat_available(vp, np, VSI_DRIVER) && np->npc_flags.ambient_spawn != 0 && target != nullptr && target->get_obj_type() == OT_VEHICLE)
    {
        return 500;
    }
    return -1;
}

// The vehicle the belief is about, or the one its player drives.
static vehicle* get_belief_vehicle(const ai_belief* belief)
{
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr)
    {
        return nullptr;
    }
    if (objp->get_obj_type() == OT_VEHICLE)
    {
        return static_cast<vehicle*>(objp);
    }
    return object_is_player(objp) ? World.object_from_handle<vehicle>(static_cast<human*>(objp)->vehicle_handle) : nullptr;
}

// 0x00687780 (:3639)
ai_results flee_in_vehicle_xtreme_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vehicle_ai_set_state(&vp->vai, VS_AI_PATHFIND);
    vehicle_ai_clear_speed_override(&vp->vai);
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    const ai_belief* belief = ai_get_flee_to_safety_belief(np);
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vehicle* threat = get_belief_vehicle(belief);
    if (threat == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const vector flee_pos = threat->pos;
    ai->action_timer.invalidate();
    ai->action_substate = 1;
    if (0.0f < vp->orient.fvec * (flee_pos - vp->pos))
    {
        ai->action_substate = 0;
        ai->action_timer.set(10000); // ms
    }
    object_path_road* road = object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads != 0, 60.0f, false, false); // 60 m
    return pick_vehicle_flee_destination(vp, road, ai->desired_destination, ai->action_substate, flee_pos, true) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006AAFB0 (:3700)
ai_results flee_in_vehicle_xtreme_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const ai_belief* belief = ai_get_flee_to_safety_belief(np);
    vector flee_pos = ZERO_VECTOR;
    if (belief != nullptr)
    {
        const vehicle* threat = get_belief_vehicle(belief);
        if (threat == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        flee_pos = threat->pos;
        if (0.0f < vp->orient.fvec * (flee_pos - vp->pos))
        {
            ai->action_substate = 0;
            ai->action_timer.set(10000); // ms
        }
        if (!ai->action_timer.valid() || ai->action_timer.elapsed())
        {
            ai->action_timer.invalidate();
            ai->action_substate = 1;
        }
    }
    if (vp->vai.vai_flags.obstructed_by_player != 0 && human_in_vehicle(player_get_local_player()) == nullptr)
    {
        if (!vp->vai.ambient_wait_timer.valid())
        {
            vehicle_ai_clear_speed_override(&vp->vai);
        }
        else if (vp->vai.ambient_wait_timer.elapsed())
        {
            vp->vai.ambient_wait_timer.invalidate();
            vp->vai.stuck.stuck_threshold_time.invalidate();
            vp->vai.stuck.extended_stuck_time.invalidate();
        }
        else
        {
            vehicle_ai_set_speed_override(&vp->vai, 0.0f);
        }
    }
    const ai_results result = ai_vehicle_travel_to_a_point_do_frame(np, vp, ai->desired_destination, 0xFFFFFFFFFFFFFFFFull, ai->action_substate != 0, 10.0f, true, false, false, true);
    if (result != AI_RESULT_SUCCESS)
    {
        return result;
    }
    if (belief == nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    object_path_road* road = ai->action_substate != 0 ? object_road_get_closest_road(vp->pos, np->npc_flags.can_use_special_roads != 0, 60.0f, false, false) : nullptr;
    return pick_vehicle_flee_destination(vp, road, ai->desired_destination, ai->action_substate, flee_pos, true) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x005F2EE0 (:3797)
ai_results flee_in_vehicle_xtreme_action::cleanup(npc* np, const ai_action*) const
{
    vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr)
    {
        vehicle_ai_clear_speed_override(&vp->vai);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005F2F10 (:3809)
int go_into_safehouse_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    object_safehouse* safehouse = safehouse_get_safehouse_containing_player();
    if (safehouse == nullptr)
    {
        if (np->npc_flags.run_to_safehouse == 0)
        {
            return -1;
        }
        safehouse = safehouse_find_nearest_safehouse(np->pos, false, false);
        if (safehouse == nullptr)
        {
            return -1;
        }
    }
    my_data.use_target = safehouse;
    return 200;
}

// 0x005F2F50 (:3833)
void go_into_safehouse_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    if (setup_data.use_target != nullptr)
    {
        ai_system* ai = np->get_ai();
        ai->desired_destination_object = setup_data.use_target->get_handle();
        ai->desired_destination = setup_data.use_target->pos;
    }
}

// 0x006AB2D0 (:3841)
ai_results go_into_safehouse_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    object* objp = ai->desired_destination_object != 0xFFFFFFFF ? World.object_from_handle(ai->desired_destination_object) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_SAFEHOUSE || !static_cast<object_safehouse*>(objp)->is_enabled() || np->npc_flags.run_to_safehouse == 0)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_stance(np, HST_STAND);
    ai->perception.set_attention_mode(ATTENTION_IDLE, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    if (np->npc_flags.activity_ha != 0)
    {
        activity_house_arrest_npc_at_safehouse(np);
    }
    if (np->current_team != HUMAN_TEAM_MARAUDER && np->npc_flags.thanked_player == 0)
    {
        dialogue_manager_start_dialogue(voice_line_handle_from_line(VOICE_LINE_HA_HOSTAGE_THANK), np, DIALOGUE_PRIORITY_NORMAL);
        np->npc_flags.thanked_player = 1;
        const human_voice_persona* voice = np->voice;
        if (voice != nullptr)
        {
            // Keeps it from chatting over the thanks.
            static const voice_lines QUIET_LINES[] = {
                VOICE_LINE_AMBIENT_PLAYER_DONT_FOLLOW_ME,
                VOICE_LINE_AMBIENT_TALK_TO_SELF_LOW_MORALE,
                VOICE_LINE_AMBIENT_TALK_TO_SELF_MEDIUM_MORALE,
                VOICE_LINE_AMBIENT_TALK_TO_SELF_HIGH_MORALE,
                VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE,
                VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE,
                VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE,
                VOICE_LINE_AMBIENT_PLAYER_GREET_LOW_MORALE_FAMOUS,
                VOICE_LINE_AMBIENT_PLAYER_GREET_MEDIUM_MORALE_FAMOUS,
                VOICE_LINE_AMBIENT_PLAYER_GREET_HIGH_MORALE_FAMOUS,
            };
            for (voice_lines line : QUIET_LINES)
            {
                voice->set_line_cooldown(line, 10000); // ms
            }
        }
    }
    if (!ai->is_pathing_to(ai->desired_destination, 0.25f, 1.0f) && ai->pathfind_to(ai->desired_destination, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_HIGH, 0xFFFFFFFFFFFFFFFFull, false) > AI_PF_RAGDOLLED)
    {
        return AI_RESULT_FAILURE;
    }
    if (vec_xz_dist_squared(np->pos, ai->desired_destination) <= 9.0f) // 3 m
    {
        np->npc_flags.run_to_safehouse = 0;
        np->npc_flags.ambient_spawn = 1;
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

dive_action dive_action::the_instance; // 0x01D72048

// 0x006059D0 (.h:15)
dive_action::dive_action()
    : ai_action("dive", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({116, 118}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* dive_action::get_instance()
{
    return &the_instance;
}

// 0x005DDF10
dive_action::~dive_action()
{
}

// 0x00605CB0 (.h:15)
int dive_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

evade_action evade_action::the_instance; // 0x01DA05A0

// 0x00605CF0 (.h:16)
evade_action::evade_action()
    : ai_action("evade", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({116, 118}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* evade_action::get_instance()
{
    return &the_instance;
}

// 0x005DDF30
evade_action::~evade_action()
{
}

// 0x00605FD0 (.h:16)
int evade_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

stay_down_action stay_down_action::the_instance; // 0x01D7568C

// 0x00606010 (.h:17)
stay_down_action::stay_down_action()
    : ai_action("stay down", ai_plan_conditions_from_bits({8, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({116}))
{
    flags.has_enter = 1;
}

ai_action* stay_down_action::get_instance()
{
    return &the_instance;
}

// 0x005DDF40
stay_down_action::~stay_down_action()
{
}

// 0x006062B0 (.h:17)
int stay_down_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

duck_into_building_action duck_into_building_action::the_instance; // 0x01DDAD78

// 0x006062F0 (.h:18)
duck_into_building_action::duck_into_building_action()
    : ai_action("duck into building", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({118}))
{
    flags.has_enter = 1;
}

ai_action* duck_into_building_action::get_instance()
{
    return &the_instance;
}

// 0x005DDF50
duck_into_building_action::~duck_into_building_action()
{
}

// 0x00606590 (.h:18)
int duck_into_building_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

fall_back_to_human_action fall_back_to_human_action::the_instance; // 0x01D74F3C

// 0x006089E0 (.h:38)
fall_back_to_human_action::fall_back_to_human_action()
    : ai_action("fall back to human", ai_plan_conditions_from_bits({9, 35, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({34}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* fall_back_to_human_action::get_instance()
{
    return &the_instance;
}

// 0x005DE080
fall_back_to_human_action::~fall_back_to_human_action()
{
}

// 0x00608CE0 (.h:38)
int fall_back_to_human_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_to_defensive_cover_rule flee_to_defensive_cover_rule::the_instance; // 0x01DC9ED0

// 0x006065C0 (.h:20)
flee_to_defensive_cover_rule::flee_to_defensive_cover_rule()
    : ai_production_rule("defensive cover rule", ai_plan_conditions_from_bits({16}), ai_plan_conditions_from_bits({120}))
{
}

ai_action* flee_to_defensive_cover_rule::get_instance()
{
    return &the_instance;
}

// 0x005DDF60
flee_to_defensive_cover_rule::~flee_to_defensive_cover_rule()
{
}

run_around_panicked_action run_around_panicked_action::the_instance; // 0x01DC9720

// 0x00606660 (.h:22)
run_around_panicked_action::run_around_panicked_action()
    : ai_action("shake off projectile", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({114}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* run_around_panicked_action::get_instance()
{
    return &the_instance;
}

// 0x005DDF80
run_around_panicked_action::~run_around_panicked_action()
{
}

// 0x00606900 (.h:22)
int run_around_panicked_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

on_fire_roll_action on_fire_roll_action::the_instance; // 0x01DDB620

// 0x00606940 (.h:23)
on_fire_roll_action::on_fire_roll_action()
    : ai_action("on fire roll", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({115}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* on_fire_roll_action::get_instance()
{
    return &the_instance;
}

// 0x005DDF90
on_fire_roll_action::~on_fire_roll_action()
{
}

// 0x00606B70 (.h:23)
int on_fire_roll_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

on_fire_panic_action on_fire_panic_action::the_instance; // 0x01D72508

// 0x00606BB0 (.h:24)
on_fire_panic_action::on_fire_panic_action()
    : ai_action("on fire panic", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({115}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* on_fire_panic_action::get_instance()
{
    return &the_instance;
}

// 0x005DDFA0
on_fire_panic_action::~on_fire_panic_action()
{
}

// 0x00606E50 (.h:24)
int on_fire_panic_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_action flee_action::the_instance; // 0x01D72204

// 0x00606E90 (.h:25)
flee_action::flee_action()
    : ai_action("flee!", ai_plan_conditions_from_bits({9, 14, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({108, 119, 120, 122, 123}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* flee_action::get_instance()
{
    return &the_instance;
}

// 0x005DDFB0
flee_action::~flee_action()
{
}

// 0x006072D0 (.h:25)
int flee_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_building_action flee_building_action::the_instance; // 0x01DC97CC

// 0x00607310 (.h:26)
flee_building_action::flee_building_action()
    : ai_action("flee building", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({14, 121}))
{
    flags.has_enter = 1;
}

ai_action* flee_building_action::get_instance()
{
    return &the_instance;
}

// 0x005DDFC0
flee_building_action::~flee_building_action()
{
}

// 0x006075F0 (.h:26)
int flee_building_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

flee_to_defensive_cover_action flee_to_defensive_cover_action::the_instance; // 0x01D9F7D4

// 0x00607620 (.h:27)
flee_to_defensive_cover_action::flee_to_defensive_cover_action()
    : ai_action("flee to defensive cover", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({15}))
{
    flags.has_enter = 1;
}

ai_action* flee_to_defensive_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005DDFE0
flee_to_defensive_cover_action::~flee_to_defensive_cover_action()
{
}

// 0x006078C0 (.h:27)
int flee_to_defensive_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

take_defensive_cover_action take_defensive_cover_action::the_instance; // 0x01D7591C

// 0x006078F0 (.h:28)
take_defensive_cover_action::take_defensive_cover_action()
    : ai_action("take defensive cover", ai_plan_conditions_from_bits({9, 15, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({16, 119, 122}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* take_defensive_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005DE000
take_defensive_cover_action::~take_defensive_cover_action()
{
}

// 0x00607C10 (.h:28)
int take_defensive_cover_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

leave_demolition_area_action leave_demolition_area_action::the_instance; // 0x01D67EA0

// 0x00607C50 (.h:30)
leave_demolition_area_action::leave_demolition_area_action()
    : ai_action("leave demolition area", ai_plan_conditions_from_bits({9, 14, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({126}))
{
    flags.has_enter = 1;
}

ai_action* leave_demolition_area_action::get_instance()
{
    return &the_instance;
}

// 0x005DE010
leave_demolition_area_action::~leave_demolition_area_action()
{
}

// 0x00607EF0 (.h:30)
int leave_demolition_area_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

leave_demolition_vehicle_action leave_demolition_vehicle_action::the_instance; // 0x01DCA4CC

// 0x00607F30 (.h:31)
leave_demolition_vehicle_action::leave_demolition_vehicle_action()
    : ai_action("lv. demo vehicle", ai_plan_conditions(), ai_plan_conditions_from_bits({126}))
{
    flags.has_enter = 1;
}

ai_action* leave_demolition_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005DE020
leave_demolition_vehicle_action::~leave_demolition_vehicle_action()
{
}

// 0x00607F80 (.h:31)
int leave_demolition_vehicle_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

flee_to_safe_building_action flee_to_safe_building_action::the_instance; // 0x01DCA75C

// 0x00607FB0 (.h:33)
flee_to_safe_building_action::flee_to_safe_building_action()
    : ai_action("flee to safe building", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({13, 17}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* flee_to_safe_building_action::get_instance()
{
    return &the_instance;
}

// 0x005DE040
flee_to_safe_building_action::~flee_to_safe_building_action()
{
}

// 0x00608290 (.h:33)
int flee_to_safe_building_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

hide_in_safe_building_action hide_in_safe_building_action::the_instance; // 0x01D72010

// 0x006082C0 (.h:34)
hide_in_safe_building_action::hide_in_safe_building_action()
    : ai_action("hide in safe building", ai_plan_conditions_from_bits({17, 47, 136, 145}), ai_plan_conditions_from_bits({109, 120}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* hide_in_safe_building_action::get_instance()
{
    return &the_instance;
}

// 0x005DE050
hide_in_safe_building_action::~hide_in_safe_building_action()
{
}

// 0x00608460 (.h:34)
int hide_in_safe_building_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_in_vehicle_action flee_in_vehicle_action::the_instance; // 0x01DA056C

// 0x006084A0 (.h:36)
flee_in_vehicle_action::flee_in_vehicle_action()
    : ai_action("flee in vehicle", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({120, 123}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* flee_in_vehicle_action::get_instance()
{
    return &the_instance;
}

// 0x005DE060
flee_in_vehicle_action::~flee_in_vehicle_action()
{
}

// 0x00608720 (.h:36)
int flee_in_vehicle_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

flee_in_vehicle_xtreme_action flee_in_vehicle_xtreme_action::the_instance; // 0x01D7536C

// 0x00608760 (.h:37)
flee_in_vehicle_xtreme_action::flee_in_vehicle_xtreme_action()
    : ai_action("flee in vehicle xtreme", ai_plan_conditions_from_bits({9, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({108}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* flee_in_vehicle_xtreme_action::get_instance()
{
    return &the_instance;
}

// 0x005DE070
flee_in_vehicle_xtreme_action::~flee_in_vehicle_xtreme_action()
{
}

// 0x006089A0 (.h:37)
int flee_in_vehicle_xtreme_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

cower_action cower_action::the_instance; // 0x01DCA0B8

// 0x00608D20 (.h:40)
cower_action::cower_action()
    : ai_action("cower", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({124}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* cower_action::get_instance()
{
    return &the_instance;
}

// 0x005DE090
cower_action::~cower_action()
{
}

// 0x00608F50 (.h:40)
int cower_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

cower_incoming_action cower_incoming_action::the_instance; // 0x01DDB4B4

// 0x00608F90 (.h:41)
cower_incoming_action::cower_incoming_action()
    : ai_action("cower incoming", ai_plan_conditions_from_bits({47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({124}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* cower_incoming_action::get_instance()
{
    return &the_instance;
}

// 0x005DE0A0
cower_incoming_action::~cower_incoming_action()
{
}

// 0x006091C0 (.h:41)
int cower_incoming_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

surrender_action surrender_action::the_instance; // 0x01D71E80

// 0x00609200 (.h:42)
surrender_action::surrender_action()
    : ai_action("surrender", ai_plan_conditions_from_bits({6, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({125}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* surrender_action::get_instance()
{
    return &the_instance;
}

// 0x005DE0B0
surrender_action::~surrender_action()
{
}

// 0x00609490 (.h:42)
int surrender_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

go_into_safehouse_action go_into_safehouse_action::the_instance; // 0x01DDB974

// 0x00609560 (.h:46)
go_into_safehouse_action::go_into_safehouse_action()
    : ai_action("go into safehouse", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({151}))
{
}

ai_action* go_into_safehouse_action::get_instance()
{
    return &the_instance;
}

// 0x005DE0D0
go_into_safehouse_action::~go_into_safehouse_action()
{
}

// 0x006097F0 (.h:46)
int go_into_safehouse_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

fetal_position_action fetal_position_action::the_instance; // 0x01DC9E68

// 0x006094D0 (.h:44)
fetal_position_action::fetal_position_action()
    : ai_action("fetal position", ai_plan_conditions(), ai_plan_conditions_from_bits({10}))
{
}

ai_action* fetal_position_action::get_instance()
{
    return &the_instance;
}

// 0x005DE0C0
fetal_position_action::~fetal_position_action()
{
}

// 0x00609520 (.h:44)
int fetal_position_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

fled_building_rule fled_building_rule::the_instance; // 0x01D6861C

// 0x00609820 (.h:53)
fled_building_rule::fled_building_rule()
    : ai_production_rule("fled building = safety", ai_plan_conditions_from_bits({121}), ai_plan_conditions_from_bits({120, 122}))
{
}

ai_action* fled_building_rule::get_instance()
{
    return &the_instance;
}

// 0x005DE0E0
fled_building_rule::~fled_building_rule()
{
}

// 0x00609910 (.h:53)
int fled_building_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

