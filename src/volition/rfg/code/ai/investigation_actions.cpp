// Original: volition/rfg/code/ai/investigation_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/investigation_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/alerts.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/follow_logic.h"
#include "volition/rfg/code/ai/formation.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

// Position 24 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG24")

extern float Ai_fov_length_low; // percept.cpp, 0x01D6598C
extern float Ai_fov_width; // percept.cpp, 0x01D65724
extern float Ai_fov_height; // percept.cpp, 0x01D67DA0

static const double FOV_DEG_TO_RAD = (double)0.017453f; // float precision, widened (original)
static const unsigned int BF_LOOKED_AT = 0x00000002; // numeric in rfg.exe, name not recovered
static const int RECENT_DISTURBANCE_MS = 60000;
static const float MINE_SAFE_DIST_SQ = 25.0f; // 5 m
static const float MINE_NEAR_DIST_SQ = 16.0f; // 4 m
static const float NEAR_CORPSE_XZ_DIST_SQ = 4.0f; // 2 m, 0x01642EF8
static const float NEAR_CORPSE_Y_DIST = 1.0f; // 0x012C8CB8
static const float GO_TO_CORPSE_RUN_DIST_SQ = 400.0f; // 0x012C8CB0
static const unsigned char NAV_CELL_FLAG_IMPASSABLE = 8;
static const int NUM_RESERVED_CORPSES = 4;

static corpse_reservation_info reserved_corpses[NUM_RESERVED_CORPSES]; // 0x01D7248C

// 0x005EB4B0
mine_nav_cell_info::mine_nav_cell_info()
{
}

// 0x005EB480 (:67)
corpse_reservation_info::corpse_reservation_info()
    : npc_handle(0xFFFFFFFF)
    , corpse_handle(0xFFFFFFFF)
{
    reservation_time.set(0);
}

// 0x00639960 (:186)
static projectile* find_nearest_projectile(const npc* np)
{
    const ai_perception& perception = np->get_ai()->perception;
    for (const ai_belief* belief = perception.get_first_belief(AI_BELIEF_PROJECTILE); belief != nullptr; belief = perception.get_next_belief(belief))
    {
        if ((belief->flags & BF_STATIONARY) == 0 || belief->handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(belief->handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE && !static_cast<projectile*>(objp)->was_investigated())
        {
            return static_cast<projectile*>(objp);
        }
    }
    return nullptr;
}

// 0x006399D0 (:208). LTCG: navp ESI, nav_info_list EDX.
static bool navpoint_in_list(const navpoint* navp, const farray<navpoint_info, 64>& nav_info_list)
{
    for (int i = 0; i < nav_info_list.size(); i++)
    {
        if (nav_info_list[i].navp == navp)
        {
            return true;
        }
    }
    return false;
}

// 0x00639A00 (:222). LTCG: ncp ESI, nav_info_list EDX.
static bool nav_cell_in_list(const nav_cell* ncp, const farray<mine_nav_cell_info, 64>& nav_info_list)
{
    for (int i = 0; i < nav_info_list.size(); i++)
    {
        if (nav_info_list[i].ncp == ncp)
        {
            return true;
        }
    }
    return false;
}

// 0x00639A30 (:237). LTCG: mine_list ESI. Every caller has at least one mine.
static float get_mine_dist_sqrd(const vector& point, const farray<projectile*, 8>& mine_list)
{
    float closest = vec_dist_squared(point, mine_list[0]->pos);
    for (int i = 1; i < mine_list.size(); i++)
    {
        const float dist_sq = vec_dist_squared(point, mine_list[i]->pos);
        if (dist_sq < closest)
        {
            closest = dist_sq;
        }
    }
    return closest;
}

// 0x00661E90 (:258). LTCG: np ECX, cellp EDX. Widens over the nav mesh until a cell is clear of every mine; the closest clear cell seen wins.
static bool find_safe_destination_pos_exterior(const npc* np, vector& safe_dest, const nav_cell* cellp, const farray<projectile*, 8>& mine_list)
{
    farray<mine_nav_cell_info, 64> cells;
    mine_nav_cell_info best;
    best.ncp = nullptr;
    best.center = ZERO_VECTOR;
    best.min_mine_dist_sqrd = 0.0f;
    best.npc_dist_sqrd = FLT_MAX;
    mine_nav_cell_info info;
    info.ncp = cellp;
    nav_cell_get_centroid(&info.center, cellp);
    info.min_mine_dist_sqrd = 0.0f;
    info.npc_dist_sqrd = vec_dist_squared(np->pos, info.center);
    cells.add(info);
    for (int i = 0; i < cells.size(); i++)
    {
        mine_nav_cell_info& cur = cells[i];
        if ((cur.ncp->flags & NAV_CELL_FLAG_IMPASSABLE) == 0 && cur.npc_dist_sqrd < best.npc_dist_sqrd)
        {
            cur.min_mine_dist_sqrd = get_mine_dist_sqrd(cur.center, mine_list);
            if (MINE_SAFE_DIST_SQ <= cur.min_mine_dist_sqrd)
            {
                best = cur;
            }
        }
        if (best.ncp == nullptr && !cells.full())
        {
            for (int edge = 0; edge < 3; edge++)
            {
                const nav_cell* adjacent = cur.ncp->adjacent_cells[edge].to_pointer();
                if (adjacent == nullptr || nav_cell_in_list(adjacent, cells))
                {
                    continue;
                }
                info.ncp = adjacent;
                nav_cell_get_centroid(&info.center, adjacent);
                info.min_mine_dist_sqrd = 0.0f;
                info.npc_dist_sqrd = vec_dist_squared(np->pos, info.center);
                cells.add(info);
                if (cells.full())
                {
                    break;
                }
            }
        }
    }
    if (best.ncp == nullptr)
    {
        return false;
    }
    safe_dest = best.center;
    return true;
}

// 0x00662160 (:329). LTCG: np ECX, mine_list EDX. The same walk over the navpoint links.
static bool find_safe_destination_pos_interior(const npc* np, vector& safe_dest, const navpoint* navp, const farray<projectile*, 8>& mine_list)
{
    farray<navpoint_info, 64> navpoints;
    const navpoint* best = nullptr;
    float best_dist_sq = FLT_MAX;
    navpoint_info info;
    info.navp = navp;
    info.min_mine_dist_sqrd = 0.0f;
    info.npc_dist_sqrd = vec_dist_squared(np->pos, navp->pos);
    navpoints.add(info);
    for (int i = 0; i < navpoints.size(); i++)
    {
        navpoint_info& cur = navpoints[i];
        if (cur.npc_dist_sqrd < best_dist_sq)
        {
            cur.min_mine_dist_sqrd = get_mine_dist_sqrd(cur.navp->pos, mine_list);
            if (MINE_SAFE_DIST_SQ <= cur.min_mine_dist_sqrd)
            {
                best_dist_sq = cur.npc_dist_sqrd;
                best = cur.navp;
            }
        }
        if (best == nullptr && !navpoints.full())
        {
            for (int link = 0; link < cur.navp->num_links; link++)
            {
                const navpoint* linked = cur.navp->links[link];
                if (navpoint_in_list(linked, navpoints))
                {
                    continue;
                }
                info.navp = linked;
                info.min_mine_dist_sqrd = 0.0f;
                info.npc_dist_sqrd = vec_dist_squared(np->pos, linked->pos);
                navpoints.add(info);
                if (navpoints.full())
                {
                    break;
                }
            }
        }
    }
    if (best == nullptr)
    {
        return false;
    }
    safe_dest = best->pos;
    return true;
}

// 0x006623D0 (:392). LTCG: np EAX.
static void build_local_mine_list(const npc* np, farray<projectile*, 8>& mine_list)
{
    mine_list.remove_all();
    const ai_perception& perception = np->get_ai()->perception;
    for (const ai_belief* belief = perception.get_first_belief(AI_BELIEF_PROJECTILE); belief != nullptr; belief = perception.get_next_belief(belief))
    {
        if ((belief->flags & BF_STATIONARY) == 0 || belief->handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(belief->handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE && !static_cast<projectile*>(objp)->was_investigated())
        {
            mine_list.add(static_cast<projectile*>(objp));
        }
    }
}

// 0x0067F8E0 (:417). LTCG: np ESI, safe_dest EDI.
static bool find_safe_destination_pos(const npc* np, vector& safe_dest)
{
    farray<projectile*, 8> mine_list;
    build_local_mine_list(np, mine_list);
    nav_cell* ncp;
    navpoint* navp;
    if (mine_list.size() < 1 || !np->get_ai()->perception.get_nearest_navpoint_or_navcell(ncp, navp, nullptr, nullptr))
    {
        return false;
    }
    if (navp != nullptr && find_safe_destination_pos_interior(np, safe_dest, navp, mine_list))
    {
        return true;
    }
    if (ncp == nullptr)
    {
        ncp = bb_pf_get_nav_cell(np->pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
        if (ncp == nullptr)
        {
            return false;
        }
    }
    return find_safe_destination_pos_exterior(np, safe_dest, ncp, mine_list);
}

// 0x005EB4C0 (:457)
static corpse_reservation_info* get_corpse_info(unsigned int corpse_handle)
{
    for (int i = 0; i < NUM_RESERVED_CORPSES; i++)
    {
        if (reserved_corpses[i].reservation_time.valid() && reserved_corpses[i].corpse_handle == corpse_handle)
        {
            return &reserved_corpses[i];
        }
    }
    return nullptr;
}

// 0x00639AC0 (:475). LTCG: np EAX. A live reservation holds unless this npc is closer to the corpse and more than 3 m from the holder.
static bool can_reserve_corpse(const npc* np, const human* corpse)
{
    const corpse_reservation_info* info = get_corpse_info(corpse->get_handle());
    if (info == nullptr || info->reservation_time.elapsed() || info->npc_handle == np->get_handle() || info->npc_handle == 0xFFFFFFFF)
    {
        return true;
    }
    const object* holder = World.object_from_handle(info->npc_handle);
    if (holder == nullptr || holder->get_obj_type() != OT_HUMAN || holder->get_sub_type() != OT_SUB_HUMAN_NPC || holder->is_killed())
    {
        return true;
    }
    const float holder_dist_sq = vec_dist_squared(np->pos, holder->pos);
    if (holder_dist_sq <= 9.0f)
    {
        return false;
    }
    return vec_dist_squared(np->pos, corpse->pos) < holder_dist_sq;
}

// 0x00639B80 (:517)
static bool reserve_corpse(const npc* np, const human* corpse)
{
    if (!can_reserve_corpse(np, corpse))
    {
        return false;
    }
    corpse_reservation_info* info = get_corpse_info(corpse->get_handle());
    if (info == nullptr)
    {
        for (int i = 0; i < NUM_RESERVED_CORPSES; i++)
        {
            if (reserved_corpses[i].reservation_time.elapsed())
            {
                info = &reserved_corpses[i];
                break;
            }
        }
        if (info == nullptr)
        {
            return false;
        }
    }
    info->corpse_handle = corpse->get_handle();
    info->npc_handle = np->get_handle();
    info->reservation_time.set(3000); // ms
    return true;
}

// 0x0067F9A0 (:549). LTCG: np EAX.
static bool is_near_corpse(const npc* np, const human* corpse)
{
    const bool near_xz = vec_xz_dist_squared(np->pos, corpse->pos) <= NEAR_CORPSE_XZ_DIST_SQ;
    if (near_xz && fabsf(np->pos.y - corpse->pos.y) <= NEAR_CORPSE_Y_DIST)
    {
        return true;
    }
    rfg_mover* building = nullptr;
    if (np->get_ai()->perception.is_in_closest_building(corpse, &building, 0.0f, 1.25f))
    {
        const navpoint* navp = ai_building_find_closest_navpoint(corpse->pos, building, true, nullptr);
        if (navp != nullptr && fabsf(corpse->pos.y - navp->pos.y) <= NEAR_CORPSE_Y_DIST && vec_xz_dist(corpse->pos, navp->pos) - navp->radius <= 2.0f) // m
        {
            return false;
        }
    }
    return near_xz;
}

// 0x006A4280 (:590)
static void play_investigate_voice(npc* np, const ai_belief* belief)
{
    static timestamp ready_to_play_timer;
    if (ready_to_play_timer.valid() && !ready_to_play_timer.elapsed())
    {
        return;
    }
    // The comparison is with 0 in rfg.exe (0x006A42C1), so nearly every roll plays.
    if (fl_rand() != 0.0f)
    {
        npc_sound_info::queue<alert_shout_disturbance>(alert_shout_disturbance(np, belief));
        ready_to_play_timer.set_rand(5000, 10000);
    }
}

// 0x005EB500 (:626). LTCG: target_pos ESI, pos EDI. Chooses between the nav mesh and the closest navpoint.
static bool get_find_disturbance_move_to_point(vector& pos, nav_cell*& ncp, navpoint*& npp, const vector& target_pos, bool in_my_building)
{
    vector ground = target_pos;
    havok_terrain_height(ground.y, ground.x, ground.z, false, nullptr);
    const float target_height = target_pos.y - ground.y;
    npp = nullptr;
    ncp = nullptr;
    float mesh_height = 0.0f;
    float mesh_dist_sq = 0.0f;
    float navpoint_dist_sq = 0.0f;
    float navpoint_height_diff = 0.0f;
    vector mesh_pos;
    ncp = bb_pf_get_nav_cell(ground, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (ncp == nullptr)
    {
        if (bb_pf_find_closest_point_on_nav_mesh(ground, mesh_pos, ncp, false))
        {
            mesh_dist_sq = vec_dist_squared(ground, mesh_pos);
            mesh_height = target_pos.y - mesh_pos.y;
            if (mesh_height < 0.0f)
            {
                mesh_height = 0.0f;
            }
            // Try to stand a little inside the mesh: 2 m along the way there, then 1 m.
            vector dir = mesh_pos - ground;
            dir.normalize_safe();
            vector inside = mesh_pos + dir * 2.0f;
            if (bb_pf_get_nav_cell(inside, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) != nullptr)
            {
                mesh_pos = inside;
            }
            else
            {
                inside = mesh_pos + dir;
                if (bb_pf_get_nav_cell(inside, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr) != nullptr)
                {
                    mesh_pos = inside;
                }
            }
        }
    }
    else
    {
        mesh_pos = ground;
        mesh_height = target_height > 0.0f ? target_height : 0.0f;
    }
    bb_pf_find_closest_navpoint(target_pos, npp, navpoint_dist_sq);
    if (npp != nullptr)
    {
        navpoint_height_diff = fabsf(npp->pos.y - target_pos.y);
    }
    if (ncp == nullptr)
    {
        if (npp == nullptr)
        {
            return false;
        }
        pos = npp->pos;
        return true;
    }
    if (npp == nullptr)
    {
        pos = mesh_pos;
        return true;
    }
    if (mesh_height <= 25.0f) // m
    {
        if (1.5f < navpoint_height_diff)
        {
            npp = nullptr;
            pos = mesh_pos;
            return true;
        }
        // In the npc's own building the mesh has to be clearly closer than the navpoint.
        if (mesh_dist_sq < (in_my_building ? 0.5625f : 1.96f) * navpoint_dist_sq)
        {
            npp = nullptr;
            pos = mesh_pos;
            return true;
        }
    }
    ncp = nullptr;
    pos = npp->pos;
    return true;
}

// 0x005EB950 (:730)
bool is_investigating(const npc* np)
{
    return np->get_ai()->planner.get_current_goal() == investigate_disturbance_goal::get_instance();
}

// 0x00662470 (:748)
static bool is_valid_investigate_follow_target(const npc* np)
{
    const ai_system* ai = np->get_ai();
    return !ai_formation_is_following(np, ai->get_formation_handle()) && ai->planner.get_current_goal() == investigate_disturbance_goal::get_instance();
}

// 0x006624A0 (:774): the current leader if still at it, else the closest investigator, squad members first.
static npc* get_investigate_follow_target(const npc* np, const human*)
{
    const ai_system* ai = np->get_ai();
    if (ai->get_formation_handle() != 0xFFFFFFFF)
    {
        object* leader = ai_formation_get_leader(ai->get_formation_handle());
        if (leader != nullptr && leader->get_obj_type() == OT_HUMAN && leader->get_sub_type() == OT_SUB_HUMAN_NPC && is_investigating(static_cast<npc*>(leader)))
        {
            return static_cast<npc*>(leader);
        }
    }
    object_squad* squad = ai->get_squad_handle() != 0xFFFFFFFF ? World.object_from_handle<object_squad>(ai->get_squad_handle()) : nullptr;
    npc* best = nullptr;
    bool best_in_squad = false;
    float best_dist_sq = -1.0f;
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; i++)
    {
        npc* other = World.get_object<npc>(i);
        if (other == np || other == nullptr || other->is_killed() || human_teams_get_disposition(np, other) != TEAM_DISPOSITION_ALLIED)
        {
            continue;
        }
        if (vec_dist_squared(np->pos, other->pos) > 144.0f || !is_valid_investigate_follow_target(other)) // 12 m
        {
            continue;
        }
        const bool in_squad = squad != nullptr && squad->is_member(other);
        if (!in_squad && best_in_squad)
        {
            continue;
        }
        const float dist_sq = vec_dist_squared(other->pos, np->pos);
        if (best == nullptr || (!best_in_squad && in_squad) || dist_sq < best_dist_sq)
        {
            best_in_squad = best_in_squad || in_squad;
            best = other;
            best_dist_sq = dist_sq;
        }
    }
    return best;
}

// 0x00639C10 (:838)
int investigate_disturbance_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_team_is_non_combatant(np))
    {
        return -1;
    }
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && (vehicle_is_tank(vp) || human_combat_get_manned_turret(np) != nullptr || human_is_driving(np)))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_recent_belief(AI_BELIEF_ENEMY_EXISTS, RECENT_DISTURBANCE_MS, BELIEF_REQUIREMENTS_NOT_INVESTIGATED, OT_UNDEFINED);
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || belief == nullptr)
    {
        return -1;
    }
    return ai->outside_defensive_perimeter(belief->pos, false, nullptr) ? -1 : 1000;
}

// 0x0067FAC0 (:882)
bool investigate_disturbance_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_recent_belief(AI_BELIEF_ENEMY_EXISTS, RECENT_DISTURBANCE_MS, BELIEF_REQUIREMENTS_NOT_INVESTIGATED, OT_UNDEFINED);
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || belief == nullptr)
    {
        return false;
    }
    bool in_my_building = false;
    rfg_mover* closest = nullptr;
    if (ai->perception.is_in_closest_building(&closest, 0.0f, 1.25f))
    {
        in_my_building = true;
        const ai_belief* building_belief = ai->perception.get_first_belief(AI_BELIEF_BUILDING);
        if (building_belief != nullptr && building_belief->handle != 0xFFFFFFFF)
        {
            rfg_mover* building = reinterpret_cast<rfg_mover*>(World.object_from_handle(building_belief->handle));
            if (building != nullptr && building->get_obj_type() == OT_MOVER && building->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                float dist_sq = 0.0f;
                ai_building_find_dist_squared(dist_sq, belief->pos, building, 0.0f);
                in_my_building = !(0.0f < dist_sq);
            }
        }
    }
    vector pos;
    nav_cell* ncp;
    navpoint* npp;
    if (!get_find_disturbance_move_to_point(pos, ncp, npp, belief->pos, in_my_building))
    {
        return false;
    }
    goal_setup_data.position = pos;
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.attention_belief_obj_type = belief->get_obj_type();
    goal_setup_data.selected_target = World.object_from_handle(belief->handle);
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_BELIEF;
    goal_setup_data.attention_awareness_level = ATTENTION_AWARENESS_FOCUSED;
    return true;
}

// 0x006A4310 (:931)
void investigate_disturbance_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->desired_goal_pos = goal_setup_data.position;
    ai_belief* belief;
    goal_setup_data.set_attention(np, &belief);
}

// 0x006626B0 (:947)
int accompany_investigation_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_team_is_non_combatant(np))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_recent_belief(AI_BELIEF_ENEMY_EXISTS, RECENT_DISTURBANCE_MS, BELIEF_REQUIREMENTS_NOT_INVESTIGATED, OT_UNDEFINED);
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || ai_formation_from_leader(np->get_handle()) != 0xFFFFFFFF || ai->outside_defensive_perimeter(belief->pos, false, nullptr))
    {
        return -1;
    }
    return get_investigate_follow_target(np, static_cast<const human*>(objp)) != nullptr ? 1010 : -1;
}

// 0x00662950 (:991)
bool accompany_investigation_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_system* ai = np->get_ai();
    const ai_belief* belief = ai->perception.get_recent_belief(AI_BELIEF_ENEMY_EXISTS, RECENT_DISTURBANCE_MS, BELIEF_REQUIREMENTS_NOT_INVESTIGATED, OT_UNDEFINED);
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || ai_formation_from_leader(np->get_handle()) != 0xFFFFFFFF)
    {
        return false;
    }
    npc* leader = get_investigate_follow_target(np, static_cast<const human*>(objp));
    if (leader == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_leader = leader;
    goal_setup_data.position = leader->get_ai()->desired_goal_pos;
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_belief_obj_type = belief->get_obj_type();
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    goal_setup_data.attention_awareness_level = ATTENTION_AWARENESS_FOCUSED;
    return true;
}

// 0x006A4350 (:1039)
void accompany_investigation_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai->desired_follow_target = goal_setup_data.selected_leader->get_handle();
    ai->desired_goal_pos = goal_setup_data.position;
    goal_setup_data.set_attention(np, nullptr);
}

// 0x00639E70 (:1050)
int investigate_mine_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    // Disposition 3 is the top of the scale: only the EDF's own side looks at mines, and only while calm.
    if ((int)human_teams_get_disposition(np, HUMAN_TEAM_EDF) < 3 || (int)gsm_get_alert_level() > 0 || activity_on_alert(np))
    {
        return -1;
    }
    return find_nearest_projectile(np) != nullptr ? 1170 : -1;
}

// 0x00639EE0 (:1080)
int look_at_disturbance_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_belief* belief = goal_setup_data.get_attention_belief(np);
    if (belief == nullptr)
    {
        return -1;
    }
    return (belief->flags & BF_LOOKED_AT) != 0 ? 0 : 100;
}

// 0x006A43A0 (:1096)
ai_results look_at_disturbance_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = ai->perception.get_attention_belief();
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    const object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return AI_RESULT_FAILURE;
    }
    if ((belief->flags & BF_LOOKED_AT) == 0)
    {
        belief->flags |= BF_LOOKED_AT;
        play_investigate_voice(np, belief);
    }
    if (last_action != this)
    {
        if (human_is_combat_ready(np))
        {
            ai->stop_moving();
            human_set_combat_ready(np, true, 0);
            ai->action_timer.set_rand(1000, 1500);
        }
        else
        {
            ai->action_timer.set_rand(1500, 2500);
        }
        ai->planner.lock_action(true);
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_BELIEF, human_combat_cover_is_using_node(np) ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_USE_CURRENT_MODE);
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EB970 (:1140)
ai_results look_at_disturbance_action::do_action(npc* np) const
{
    if (np->combat.combat_ready_timestamp.valid() && !np->combat.combat_ready_timestamp.elapsed())
    {
        human_set_combat_ready(np, true, 0);
    }
    return np->get_ai()->action_timer.elapsed() ? AI_RESULT_SUCCESS : AI_RESULT_IN_PROGRESS;
}

// The low alert frustum, whatever the alert level: rfg.exe reads the level and does nothing with it (0x006A450A).
static view_frustum_parameters get_disturbance_frustum()
{
    view_frustum_parameters params;
    params.length = Ai_fov_length_low;
    params.h_angle = (float)((double)Ai_fov_width * FOV_DEG_TO_RAD);
    params.v_angle = (float)((double)Ai_fov_height * FOV_DEG_TO_RAD);
    params.initialized = true;
    params.use_fov = true;
    return params;
}

// 0x006A44E0 (:1155)
int find_disturbance_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_belief* belief = goal_setup_data.get_attention_belief(np);
    if (belief == nullptr)
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    return perception.within_view_frustum(belief->pos, get_disturbance_frustum()) && perception.has_los_to_position(belief->pos, false) ? 0 : 1000;
}

// 0x00662A20 (:1172)
ai_results find_disturbance_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        ai->action_timer.set_rand(800, 1000);
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A45A0 (:1185)
ai_results find_disturbance_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = ai->perception.get_attention_belief();
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->action_timer.elapsed())
    {
        if (ai->perception.within_view_frustum(belief->pos, get_disturbance_frustum()) && ai->perception.has_los_to_position(belief->pos, false))
        {
            return AI_RESULT_SUCCESS;
        }
        ai->action_timer.set_rand(800, 1000);
    }
    if (vec_xz_dist_squared(np->pos, belief->pos) < 1.0f) // 1 m
    {
        return AI_RESULT_SUCCESS;
    }
    if (!ai->is_pathing_to(ai->desired_goal_pos, 0.25f, 1.0f) && ai->pathfind_to(ai->desired_goal_pos, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false) > AI_PF_RAGDOLLED)
    {
        belief->flags |= BF_INVESTIGATED;
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x005EB9D0 (:1232)
int examine_disturbance_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1000;
}

// 0x00662AA0 (:1237)
ai_results examine_disturbance_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (ai->perception.get_attention_belief() == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_USE_CURRENT_MODE);
        ai->action_substate = 0;
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A4740 (:1258): walks over when 10 m off or more, runs from 15 m; otherwise plays the look around once.
ai_results examine_disturbance_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = ai->perception.get_attention_belief();
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr || ai->perception.get_first_belief(AI_BELIEF_HAZARD) != nullptr)
    {
        return AI_RESULT_SUCCESS;
    }
    const float dist_sq = vec_dist_squared(np->pos, ai->desired_goal_pos);
    if (100.0f <= dist_sq)
    {
        const human_movement_states m_state = 225.0f <= dist_sq ? HMS_RUN : HMS_WALK;
        if (ai->is_pathing_to(ai->desired_goal_pos, 0.25f, 1.0f))
        {
            if (np->cur_movement_state == HMS_RUN && m_state == HMS_WALK)
            {
                human_set_state(np, HS_CURRENT, HMS_WALK, -1.0f);
            }
            return AI_RESULT_IN_PROGRESS;
        }
        if (ai_pf_to_result(ai->pathfind_to(ai->desired_goal_pos, m_state, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false)) == AI_RESULT_FAILURE)
        {
            belief->flags |= BF_INVESTIGATED;
            return AI_RESULT_FAILURE;
        }
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_substate != 0)
    {
        if (np->char_inst->is_playing_action(ANIMATION_ACTION_LOOK_AROUND))
        {
            return AI_RESULT_IN_PROGRESS;
        }
        // Original quirk kept: the look around ending counts as a failure, which is what ends the goal.
        belief->flags |= BF_INVESTIGATED;
        return AI_RESULT_FAILURE;
    }
    ai->stop_moving();
    ai->planner.lock_action(true);
    const float rate = fl_rand_range(0.8f, 1.25f);
    ai->perception.set_attention_mode(ATTENTION_LOOK_AT_NONE, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    np->char_inst->play_action(ANIMATION_ACTION_LOOK_AROUND, ANIM_ALIAS_NONE, rate, 0);
    ai->action_substate = 1;
    return AI_RESULT_IN_PROGRESS;
}

// 0x00639F10 (:1328)
ai_results examine_disturbance_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this && np->char_inst->rdd.is_loaded())
    {
        np->char_inst->stop_action(ANIMATION_ACTION_LOOK_AROUND, 5, false); // 5: blend frames
    }
    return AI_RESULT_SUCCESS;
}

// 0x005EB9E0 (:1345)
int accompany_investigation_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 1000;
}

// 0x006BD2B0 (:1350)
ai_results accompany_investigation_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (ai->perception.get_attention_belief() == nullptr || ai->desired_follow_target == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    object* leader = World.object_from_handle(ai->desired_follow_target);
    if (leader == nullptr || leader->get_obj_type() != OT_HUMAN || leader->get_sub_type() != OT_SUB_HUMAN_NPC || leader->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (ai_formation_is_follower_of(np, leader))
    {
        if (!ai_do_follow_frame(np, ai->get_formation_handle(), HST_STAND))
        {
            return AI_RESULT_FAILURE;
        }
    }
    else if (ai->follow_in_formation(leader, AI_FORMATION_FLOCK) == 0xFFFFFFFF)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->perception.set_attention_mode(ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_USE_CURRENT_MODE);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A4980 (:1392)
ai_results accompany_investigation_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = ai->perception.get_attention_belief();
    if (belief == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const unsigned int handle = ai->get_formation_handle();
    if (!ai_formation_is_following(np, handle))
    {
        return AI_RESULT_FAILURE;
    }
    const object* leader = ai_formation_get_leader(handle);
    if (leader == nullptr || leader->get_obj_type() != OT_HUMAN || leader->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return AI_RESULT_FAILURE;
    }
    if (!is_investigating(static_cast<const npc*>(leader)))
    {
        belief->flags |= BF_INVESTIGATED;
        return AI_RESULT_FAILURE;
    }
    return ai_do_follow_frame(np, handle, HST_STAND) ? AI_RESULT_IN_PROGRESS : AI_RESULT_FAILURE;
}

// 0x006BD3D0 (:1428)
ai_results accompany_investigation_action::cleanup(npc* np, const ai_action* next_action) const
{
    if (next_action != this)
    {
        np->get_ai()->leave_formation();
    }
    return AI_RESULT_SUCCESS;
}

// 0x0067FC10 (:1443)
int go_to_corpse_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const object* corpse = goal_setup_data.selected_target;
    if (corpse == nullptr || corpse->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    return is_near_corpse(np, static_cast<const human*>(corpse)) ? 0 : 200;
}

// The corpse the npc holds, while it can keep the reservation.
static human* get_reserved_corpse(const npc* np)
{
    const unsigned int handle = np->get_ai()->reserved_corpse_handle;
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || !reserve_corpse(np, static_cast<human*>(objp)))
    {
        return nullptr;
    }
    return static_cast<human*>(objp);
}

// 0x00662B50 (:1461)
ai_results go_to_corpse_action::enter(npc* np, const ai_action* last_action) const
{
    if (get_reserved_corpse(np) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (last_action != this)
    {
        np->get_ai()->perception.set_attention_mode(ATTENTION_SEARCH, ATTENTION_STEERING_PLANT_FEET, ATTENTION_AWARENESS_NORMAL);
    }
    return AI_RESULT_SUCCESS;
}

// 0x006A4A40 (:1479)
ai_results go_to_corpse_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if ((int)gsm_get_alert_level() > 0)
    {
        return AI_RESULT_FAILURE;
    }
    const human* corpse = get_reserved_corpse(np);
    if (corpse == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (is_near_corpse(np, corpse))
    {
        return AI_RESULT_SUCCESS;
    }
    human_movement_states m_state = HMS_WALK;
    if (vec_dist_squared(np->pos, corpse->pos) <= GO_TO_CORPSE_RUN_DIST_SQ)
    {
        if (np->cur_movement_state > HMS_WALK)
        {
            np->cur_movement_state = HMS_WALK;
        }
    }
    else
    {
        m_state = HMS_RUN;
    }
    if (!ai->is_pathing_to(corpse->pos, 0.25f, 1.0f) && ai_pf_to_result(ai->pathfind_to(corpse->pos, m_state, HST_CURRENT, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false)) == AI_RESULT_FAILURE)
    {
        ai->reserved_corpse_handle = 0xFFFFFFFF;
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00639F40 (:1530)
int examine_corpse_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const object* corpse = goal_setup_data.selected_target;
    if (corpse == nullptr || corpse->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    return can_reserve_corpse(np, static_cast<const human*>(corpse)) ? 200 : -1;
}

// 0x006A4B70 (:1548)
ai_results examine_corpse_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    const human* corpse = get_reserved_corpse(np);
    if (corpse == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (last_action != this)
    {
        ai->action_timer.set(4000); // ms
        human_set_stance(np, fabsf(np->pos.y - corpse->pos.y) < 1.0f ? HST_CROUCH : HST_STAND);
        human_set_combat_ready(np, false, 0);
        ai->perception.pay_attention(corpse->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        ai->stop_moving();
    }
    return AI_RESULT_SUCCESS;
}

// 0x00662D00 (:1582)
ai_results examine_corpse_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if ((int)gsm_get_alert_level() > 0)
    {
        return AI_RESULT_FAILURE;
    }
    human* corpse = get_reserved_corpse(np);
    if (corpse == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    if (!ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    // The alert goes up only when the killer was the player or a live member of a player's squad.
    const unsigned int killer_handle = corpse->combat.last_hit_by_handle;
    object* objp = killer_handle != 0xFFFFFFFF ? World.object_from_handle(killer_handle) : nullptr;
    const human* killer = objp != nullptr && objp->get_obj_type() == OT_HUMAN ? static_cast<human*>(objp) : nullptr;
    // Original quirk kept: human_is_dead gets a null killer when the corpse's killer is gone.
    if (object_is_local_player(killer) || (!human_is_dead(killer) && killer->get_sub_type() == OT_SUB_HUMAN_NPC && static_cast<const npc*>(killer)->get_ai()->is_in_player_squad(false) != nullptr))
    {
        gsm_set_alert_level(ALERT_LEVEL_YELLOW);
    }
    ai->reserved_corpse_handle = 0xFFFFFFFF;
    corpse->hflags.death_reported = 1;
    return AI_RESULT_SUCCESS;
}

// 0x00639F80 (:1626)
int move_away_from_mine_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const projectile* mine = find_nearest_projectile(np);
    if (mine == nullptr)
    {
        return -1;
    }
    return vec_dist_squared(np->pos, mine->pos) > MINE_NEAR_DIST_SQ ? 0 : 200;
}

// 0x0067FC50 (:1652)
ai_results move_away_from_mine_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action == this)
    {
        return AI_RESULT_SUCCESS;
    }
    return find_safe_destination_pos(np, np->get_ai()->desired_destination) ? AI_RESULT_SUCCESS : AI_RESULT_FAILURE;
}

// 0x006A4C50 (:1670)
ai_results move_away_from_mine_action::do_action(npc* np) const
{
    farray<projectile*, 8> mine_list;
    build_local_mine_list(np, mine_list);
    if (mine_list.size() < 1 || MINE_NEAR_DIST_SQ < get_mine_dist_sqrd(np->pos, mine_list))
    {
        return AI_RESULT_SUCCESS;
    }
    ai_system* ai = np->get_ai();
    return ai->pathfind_to(ai->desired_destination, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false) > AI_PF_RAGDOLLED ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}

// 0x005EB9F0 (:1705)
int examine_mine_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return 200;
}

// 0x006A4D00 (:1710)
ai_results examine_mine_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (last_action != this)
    {
        const projectile* mine = find_nearest_projectile(np);
        if (mine == nullptr)
        {
            return AI_RESULT_FAILURE;
        }
        ai->perception.pay_attention(mine->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set(4000); // ms
        ai->stop_moving();
        if (ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY) != nullptr)
        {
            np->play_voice_line(VOICE_LINE_REACT_CHARGE_GROUP, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    return AI_RESULT_SUCCESS;
}

// 0x00639FD0 (:1733)
ai_results examine_mine_action::do_action(npc* np) const
{
    if (((int)human_teams_get_disposition(np, HUMAN_TEAM_EDF) > 2 && (int)gsm_get_alert_level() > 0) || activity_on_alert(np))
    {
        return AI_RESULT_FAILURE;
    }
    if (np->get_ai()->action_timer.elapsed())
    {
        gsm_set_alert_level(ALERT_LEVEL_YELLOW);
        return AI_RESULT_SUCCESS;
    }
    return AI_RESULT_IN_PROGRESS;
}

investigate_disturbance_goal investigate_disturbance_goal::the_instance; // 0x01DCA24C

// 0x00634DF0 (.h:14)
investigate_disturbance_goal::investigate_disturbance_goal()
    : ai_goal(ai_plan_conditions(91), "investigate disturbance")
{
}

ai_goal* investigate_disturbance_goal::get_instance()
{
    return &the_instance;
}

// 0x005E9E10
investigate_disturbance_goal::~investigate_disturbance_goal()
{
}

accompany_investigation_goal accompany_investigation_goal::the_instance; // 0x01DDB174

// 0x00634E90 (.h:15)
accompany_investigation_goal::accompany_investigation_goal()
    : ai_goal(ai_plan_conditions(94), "accompany investigation")
{
}

ai_goal* accompany_investigation_goal::get_instance()
{
    return &the_instance;
}

// 0x005E9E20
accompany_investigation_goal::~accompany_investigation_goal()
{
}

investigate_mine_goal investigate_mine_goal::the_instance; // 0x01D6858C

// 0x00634F30 (.h:18)
investigate_mine_goal::investigate_mine_goal()
    : ai_goal(ai_plan_conditions(99), "investigate mine")
{
}

ai_goal* investigate_mine_goal::get_instance()
{
    return &the_instance;
}

// 0x005E9E30
investigate_mine_goal::~investigate_mine_goal()
{
}

examine_disturbance_action examine_disturbance_action::the_instance; // 0x01D757CC

// 0x006355D0 (.h:23)
examine_disturbance_action::examine_disturbance_action()
    : ai_action("examine disturbance loc", ai_plan_conditions_from_bits({4, 9, 18, 19, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({91}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* examine_disturbance_action::get_instance()
{
    return &the_instance;
}

// 0x005E9E60
examine_disturbance_action::~examine_disturbance_action()
{
}

// 0x00635980 (.h:23)
int examine_disturbance_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

find_disturbance_action find_disturbance_action::the_instance; // 0x01D67FF0

// 0x00635240 (.h:22)
find_disturbance_action::find_disturbance_action()
    : ai_action("find disturbance loc", ai_plan_conditions_from_bits({4, 9, 18, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({19}))
{
    flags.has_enter = 1;
}

ai_action* find_disturbance_action::get_instance()
{
    return &the_instance;
}

// 0x005E9E50
find_disturbance_action::~find_disturbance_action()
{
}

// 0x00635590 (.h:22)
int find_disturbance_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

look_at_disturbance_action look_at_disturbance_action::the_instance; // 0x01D67ED4

// 0x00634FD0 (.h:21)
look_at_disturbance_action::look_at_disturbance_action()
    : ai_action("look at disturbance", ai_plan_conditions_from_bits({9, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({18}))
{
    flags.has_enter = 1;
}

ai_action* look_at_disturbance_action::get_instance()
{
    return &the_instance;
}

// 0x005E9E40
look_at_disturbance_action::~look_at_disturbance_action()
{
}

// 0x00635200 (.h:21)
int look_at_disturbance_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

accompany_investigation_action accompany_investigation_action::the_instance; // 0x01D68444

// 0x006359C0 (.h:24)
accompany_investigation_action::accompany_investigation_action()
    : ai_action("accompany investigation", ai_plan_conditions_from_bits({4, 9, 18, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({94}))
{
    flags.has_enter = 1;
    flags.has_cleanup = 1;
}

ai_action* accompany_investigation_action::get_instance()
{
    return &the_instance;
}

// 0x005E9E70
accompany_investigation_action::~accompany_investigation_action()
{
}

// 0x00635D10 (.h:24)
int accompany_investigation_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

go_to_corpse_action go_to_corpse_action::the_instance; // 0x01DA6178

// 0x00635D50 (.h:27)
go_to_corpse_action::go_to_corpse_action()
    : ai_action("go to corpse", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({96}))
{
    flags.has_enter = 1;
}

ai_action* go_to_corpse_action::get_instance()
{
    return &the_instance;
}

// 0x005E9E80
go_to_corpse_action::~go_to_corpse_action()
{
}

// 0x00636050 (.h:27)
int go_to_corpse_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

examine_corpse_action examine_corpse_action::the_instance; // 0x01D685E8

// 0x00636090 (.h:28)
examine_corpse_action::examine_corpse_action()
    : ai_action("examine corpse", ai_plan_conditions_from_bits({9, 47, 58, 96, 136, 137, 145, 153}), ai_plan_conditions_from_bits({97}))
{
    flags.has_enter = 1;
}

ai_action* examine_corpse_action::get_instance()
{
    return &the_instance;
}

// 0x005E9E90
examine_corpse_action::~examine_corpse_action()
{
}

// 0x00636390 (.h:28)
int examine_corpse_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

move_away_from_mine_action move_away_from_mine_action::the_instance; // 0x01DCA120

// 0x006363D0 (.h:31)
move_away_from_mine_action::move_away_from_mine_action()
    : ai_action("move away from mine", ai_plan_conditions_from_bits({9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({98}))
{
    flags.has_enter = 1;
}

ai_action* move_away_from_mine_action::get_instance()
{
    return &the_instance;
}

// 0x005E9EA0
move_away_from_mine_action::~move_away_from_mine_action()
{
}

// 0x00636670 (.h:31)
int move_away_from_mine_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

examine_mine_action examine_mine_action::the_instance; // 0x01D723E4

// 0x006366B0 (.h:32)
examine_mine_action::examine_mine_action()
    : ai_action("examine mine", ai_plan_conditions_from_bits({9, 47, 58, 98, 136, 137, 145, 153}), ai_plan_conditions_from_bits({99}))
{
    flags.has_enter = 1;
}

ai_action* examine_mine_action::get_instance()
{
    return &the_instance;
}

// 0x005E9EB0
examine_mine_action::~examine_mine_action()
{
}

// 0x006369B0 (.h:32)
int examine_mine_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

