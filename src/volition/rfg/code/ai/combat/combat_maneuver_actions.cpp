// Original: volition/rfg/code/ai/combat/combat_maneuver_actions.cpp
//
// Conditions are bit numbers: their names are not in rfg.pdb. The constructors at the end come from
// tools/headless/ai_action_table.txt.

#include "volition/rfg/code/ai/combat/combat_maneuver_actions.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/combat/combat_common.h"
#include "volition/rfg/code/ai/combat/combat_cover.h"
#include "volition/rfg/code/ai/combat/combat_maneuver.h"
#include "volition/rfg/code/ai/combat/defensive_combat_actions.h"
#include "volition/rfg/code/ai/plan_conditions.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"

#include <float.h>
#include <math.h>

// Position 11 of the 33 blocks of AI translation units in rfg.exe's .CRT$XC table (see goals.cpp).
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG11")

static const float MAX_ADVANCE_USING_COVER_COLLECT_FLOOR = 10.0f; // 0x01642648, .data
// Tweak table items, with these .data values until tweak_table.xtbl loads.
static float Marauder_walk_advance_min_dist = 50.0f; // 0x016415C8
static float Marauder_walk_advance_max_dist = 50.0f; // 0x016415CC
static float Marauder_walk_advance_max_chance = 0.75f; // 0x016415C4
static float Marauder_walk_advance_min_chance; // 0x01D67DBC
static float Advance_flanking_min_dist = 15.0f; // 0x016415D0
static tweak_table_item Tweak_table_item_Marauder_walk_advance_min_dist("Marauder_walk_advance_min_dist", &Marauder_walk_advance_min_dist);
static tweak_table_item Tweak_table_item_Marauder_walk_advance_max_dist("Marauder_walk_advance_max_dist", &Marauder_walk_advance_max_dist);
static tweak_table_item Tweak_table_item_Marauder_walk_advance_max_chance("Marauder_walk_advance_max_chance", &Marauder_walk_advance_max_chance);
static tweak_table_item Tweak_table_item_Marauder_walk_advance_min_chance("Marauder_walk_advance_min_chance", &Marauder_walk_advance_min_chance);
static tweak_table_item Tweak_table_item_Advance_flanking_min_dist("Advance_flanking_min_dist", &Advance_flanking_min_dist);

// The box a retreat's navpoints are searched in.
struct retreat_bbox_collect_data
{
    vector bmin;
    vector bmax;
    vector pos;
    matrix orient;
};

// 0x005E5DB0 (:277): cover navpoints (10) inside the box.
static bool bbox_collect_cb(object* hit_objp, unsigned int, const void* user_data)
{
    if (static_cast<const navpoint*>(hit_objp)->type != 10)
    {
        return false;
    }
    const retreat_bbox_collect_data* data = static_cast<const retreat_bbox_collect_data*>(user_data);
    return ix_point_in_box_oriented(hit_objp->pos, data->pos, data->orient, data->bmin, data->bmax);
}

// 0x006593D0 (:289)
static bool pick_retreat_position_building(vector& destination_pos, const rfg_mover* building, const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient)
{
    if (building == nullptr)
    {
        return false;
    }
    retreat_bbox_collect_data data = {bmin, bmax, pos, orient};
    farray<navpoint*, 20> navpoints;
    ai_building_collect_navpoints(navpoints, building, false, bbox_collect_cb, &data);
    if (navpoints.size() <= 0)
    {
        return false;
    }
    destination_pos = navpoints[int_rand_range(0, navpoints.size() - 1)]->pos;
    return true;
}

// 0x005E5E80 (:334), inlined in pick_retreat_position_object. Original bug kept: the second test reads the x extent again, so once x is
// trimmed to the size it fails and z is never trimmed. rfg.exe has get_bbox_x_diff, _z_diff and _area (0x005E5E00, 0x005E5E20, 0x005E5E40) for these.
static void reduce_bbox_size(vector& bmin, vector& bmax, float max_size)
{
    const unsigned int random = vrand();
    if (max_size < fabsf(bmax.x - bmin.x))
    {
        if ((random & 1) == 0)
        {
            bmin.x = bmax.x - max_size;
        }
        else
        {
            bmax.x = bmin.x + max_size;
        }
    }
    if (max_size < fabsf(bmax.x - bmin.x))
    {
        if ((random & 2) != 0)
        {
            bmax.z = bmin.z + max_size;
        }
        else
        {
            bmin.z = bmax.z - max_size;
        }
    }
}

// 0x006594E0 (:366): a building's cover navpoint, else the centroid of a random enabled nav cell in the object's box, grown 40 m in y and cut
// to about 1600 square metres for the search.
static bool pick_retreat_position_object(vector& destination_pos, const object* objp)
{
    static const float MAX_SEARCH_SEARCH_AREA = 1600.0f; // set on the first call in rfg.exe
    if (objp == nullptr)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    objp->get_local_bbox(bmin, bmax);
    bmin.y -= 40.0f;
    bmax.y += 40.0f;
    const vector test_bmin = bmin;
    const vector test_bmax = bmax;
    if (objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        return pick_retreat_position_building(destination_pos, static_cast<const rfg_mover*>(objp), bmin, bmax, objp->pos, objp->orient);
    }
    if (MAX_SEARCH_SEARCH_AREA < fabsf(bmax.x - bmin.x) * fabsf(bmax.z - bmin.z))
    {
        reduce_bbox_size(bmin, bmax, 40.0f);
    }
    nav_cell* cells[512];
    const int num_cells = havok_collect_nav_cells(bmin, bmax, objp->pos, objp->orient, cells, 512, false, nullptr);
    if (num_cells <= 0)
    {
        return false;
    }
    const int first = int_rand_range(0, num_cells - 1);
    for (int i = 0; i < num_cells; i++)
    {
        const int index = (i + first) % num_cells;
        if (-1 < index && index < num_cells && (cells[index]->flags & NAV_CELL_FLAG_DISABLED) == 0)
        {
            nav_cell_get_centroid(&destination_pos, cells[index]);
            vector local_pos = destination_pos;
            vec_change_to_local_coords(local_pos, objp->pos, objp->orient);
            if (ix_point_in_box(local_pos, test_bmin, test_bmax))
            {
                return true;
            }
        }
    }
    return false;
}

// 0x00696D50 (:456): the furthest of up to three steps towards the target, over three quarters of the way, with a clear shot at it.
static bool get_partial_advance_to_pos(vector& pos, const npc* np, const human* target, const nav_cell* ncp)
{
    const ai_system* ai = np->get_ai();
    const float total = vec_xz_dist(np->pos, target->pos) * 0.75f;
    int num_steps = 3;
    float step = total / 3.0f;
    if (step < 5.0f)
    {
        // At least 5 m a step.
        step = 5.0f;
        num_steps = (int)floor((double)(total / 5.0f));
        if (num_steps < 1)
        {
            return false;
        }
    }
    vector current = np->pos;
    const vector to_target = target->pos - np->pos;
    const vector direction = to_target.copy_flatten_normalize();
    vector points[3];
    int best = -1;
    for (int i = 0; i < num_steps; i++)
    {
        const vector next = direction * step + current;
        const nav_cell* next_cell = nullptr;
        if (next == ZERO_VECTOR || ai->outside_defensive_perimeter(next, false, nullptr))
        {
            break;
        }
        const bool clear = nav_cell_find_longest_line_of_sight_no_detour(points[i], next_cell, current, ncp, next, true, nullptr);
        if (!clear && i == 0)
        {
            return false;
        }
        if (ai->perception.firing_los_clear(target, HST_STAND, &points[i]))
        {
            best = i;
        }
        if (!clear)
        {
            break;
        }
        current = points[i];
        ncp = next_cell;
    }
    if (best < 0)
    {
        return false;
    }
    pos = points[best];
    return true;
}

static bool weapon_is_ranged(const weapon* wp)
{
    const weapon_ammo_type type = wp->w_info->ammo_type;
    return type == WEAPON_AMMO_TYPE_BULLET || type == WEAPON_AMMO_TYPE_PROJECTILE || type == WEAPON_AMMO_TYPE_ELECTRICITY || type == WEAPON_AMMO_TYPE_REPAIR || type == WEAPON_AMMO_TYPE_BLANKS ||
           type == WEAPON_AMMO_TYPE_FLAME;
}

// 0x006597A0 (:548): the first offensive cover node in a box ahead, otherwise the last defensive one. Random starting point.
cover_node* get_cover_node_for_advance(bool& defense_only, const npc* np, const weapon* wp, const human* target, const vector& target_dir, float target_dist)
{
    const ai_system* ai = np->get_ai();
    defense_only = false;
    if (wp == nullptr || !weapon_is_ranged(wp))
    {
        return nullptr;
    }
    matrix orient;
    orient.make(target_dir);
    float forward = target_dist - wp->w_info->min_engagement_dist;
    if (0.0f <= forward - 20.0f)
    {
        forward = 20.0f;
    }
    if (forward < MAX_ADVANCE_USING_COVER_COLLECT_FLOOR)
    {
        return nullptr;
    }
    // The box is as wide and high as half the distance, 5 to 10 m each way.
    float half_width = target_dist;
    if (half_width - 10.0f < 0.0f)
    {
        half_width = 10.0f;
    }
    if (0.0f <= half_width - 20.0f)
    {
        half_width = 20.0f;
    }
    half_width *= 0.5f;
    const vector bmax(half_width, half_width, forward);
    const vector bmin(-half_width, -half_width, 5.0f);
    unsigned int nodes[100];
    const int num_nodes = World.collect_objects<cover_node>(bmin, bmax, np->pos, orient, nodes, 100, nullptr, nullptr);
    cover_node* defensive = nullptr;
    if (0 < num_nodes)
    {
        int index = int_rand_range(0, num_nodes - 1);
        for (int i = 0; i < num_nodes; i++, index = (index + 1) % num_nodes)
        {
            cover_node* node = World.object_from_handle<cover_node>(nodes[index]);
            if (node->flags.on_mover != 0 || node->flags.zone == 0 || node->is_disabled() || node->pos == ZERO_VECTOR || ai->outside_defensive_perimeter(node->pos, false, nullptr))
            {
                continue;
            }
            const cover_check_results result = node->check_cover_type(np, static_cast<const object*>(target), true, true, false);
            if (result == CCR_OFFENSIVE)
            {
                return node;
            }
            if (result != CCR_NO_COVER)
            {
                defensive = node;
            }
        }
    }
    defense_only = true;
    return defensive;
}

// 0x00697250 (:655): aims and shoots if in range or forced; otherwise looks at the target.
static bool advance_maybe_fire_at_target(const ai_combat_action_parameters& params, bool force_fire)
{
    ai_system* ai = params.np->get_ai();
    human_set_combat_ready(params.np, true, 0);
    if (!force_fire && !ai_combat_fire_check_range(params))
    {
        if (ai->perception.get_attention_mode() != ATTENTION_LOOK_AT_OBJECT)
        {
            ai->perception.set_attention_mode(ATTENTION_LOOK_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
        }
        return false;
    }
    if (ai->perception.get_attention_mode() != ATTENTION_AIM_AT_OBJECT)
    {
        ai->perception.set_attention_mode(ATTENTION_AIM_AT_OBJECT, ATTENTION_STEERING_TURN_TO_TARGET, ATTENTION_AWARENESS_NORMAL);
    }
    ai_combat_fire_at_target(params, true);
    return true;
}

// 0x00659C10 (:689): 12000 when already on the default retreat, else 10000.
int retreat_defensive_perimeter_building_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    const ai_system* ai = np->get_ai();
    const gfm_orders* orders = ai->get_orders();
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return -1;
    }
    const object* defended = orders->get_target_object();
    if (defended == nullptr)
    {
        return -1;
    }
    vector bmin;
    vector bmax;
    defended->get_local_bbox(bmin, bmax);
    if (defended->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        bmin -= vector(10.0f, 10.0f, 10.0f);
        bmax += vector(10.0f, 10.0f, 10.0f);
    }
    else
    {
        // A bounding box in a building: its own box, in the building's frame.
        if (defended->get_obj_type() != OT_BOUNDING_BOX || defended->get_parent() == 0xFFFFFFFF)
        {
            return -1;
        }
        defended = World.object_from_handle(defended->get_parent());
        if (defended == nullptr || defended->get_obj_type() != OT_MOVER || defended->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            return -1;
        }
    }
    const int cost = ai->planner.get_current_action() == retreat_defensive_perimeter_default_action::get_instance() ? 12000 : 10000;
    if (np->movement_mode == HMM_PATHFINDING && np->pf.eventual_goal_pos != ZERO_VECTOR && !ai->outside_defensive_perimeter(np->pf.eventual_goal_pos, false, nullptr))
    {
        my_data.pos = np->pf.eventual_goal_pos;
        return cost;
    }
    if (ai->desired_destination != ZERO_VECTOR && !ai->outside_defensive_perimeter(ai->desired_destination, false, nullptr))
    {
        my_data.pos = ai->desired_destination;
        return cost;
    }
    return pick_retreat_position_building(my_data.pos, static_cast<const rfg_mover*>(defended), bmin, bmax, defended->pos, defended->orient) ? cost : -1;
}

// 0x005E5F50 (:749)
void retreat_defensive_perimeter_building_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->desired_destination = setup_data.pos;
}

// 0x00697360 (:755)
ai_results retreat_defensive_perimeter_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_combat_fire_at_target_if_available(np, false, false);
    if (ai->is_pathing_to(ai->desired_destination, 0.25f, 1.0f))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const ai_pathfind_results result = ai->pathfind_to(ai->desired_destination, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
    if (result != AI_PF_SUCCESS && 2 < (int)result)
    {
        ai->planner.avoid_action(this, 10000);
        return AI_RESULT_FAILURE;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x006241F0 (:781)
int retreat_defensive_perimeter_default_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const gfm_orders* orders = np->get_ai()->get_orders();
    if (orders != nullptr && (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND || orders->get_orders_type() == GFM_ORDERS_TYPE_ROADBLOCK) && orders->get_target_object() != nullptr)
    {
        return 11000;
    }
    return -1;
}

// 0x00697430 (:795)
ai_results retreat_defensive_perimeter_default_action::enter(npc* np, const ai_action* last_action) const
{
    if (last_action != this)
    {
        np->get_ai()->action_timer.set(0);
    }
    ai_combat_fire_at_target_if_available(np, false, false);
    return AI_RESULT_SUCCESS;
}

// 0x00697470 (:811)
ai_results retreat_defensive_perimeter_default_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    ai_combat_fire_at_target_if_available(np, false, false);
    if (np->movement_mode == HMM_PATHFINDING && np->pf.eventual_goal_pos != ZERO_VECTOR && !ai->outside_defensive_perimeter(np->pf.eventual_goal_pos, false, nullptr))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    if (ai->action_timer.valid() && !ai->action_timer.elapsed())
    {
        return AI_RESULT_IN_PROGRESS;
    }
    const gfm_orders* orders = ai->get_orders();
    if (orders == nullptr || (orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND && orders->get_orders_type() != GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return AI_RESULT_FAILURE;
    }
    const object* defended = orders->get_target_object();
    if (defended == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    vector dest;
    if (ai->action_timer.valid())
    {
        ai->action_timer.invalidate();
        if (!pick_retreat_position_object(dest, defended))
        {
            return AI_RESULT_FAILURE;
        }
        ai->desired_destination = dest;
    }
    else
    {
        dest = ai->desired_destination;
    }
    if (dest != ZERO_VECTOR && !ai->outside_defensive_perimeter(dest, false, nullptr))
    {
        const ai_pathfind_results result = ai->pathfind_to(dest, HMS_RUN, HST_STAND, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false);
        if (result != AI_PF_SUCCESS && (int)result < 3)
        {
            return AI_RESULT_IN_PROGRESS;
        }
    }
    ai->action_timer.set_rand(2000, 4000);
    return AI_RESULT_IN_PROGRESS;
}

// 0x0065A200 (:877): a passenger of a squadmate outside the perimeter.
int follow_to_defensive_perimeter_rule::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    const ai_system* ai = np->get_ai();
    if (ai->passenger_of_squadmate() == nullptr)
    {
        return -1;
    }
    return np->pos == ZERO_VECTOR || ai->outside_defensive_perimeter(np->pos, false, nullptr) ? 1 : -1;
}

// 0x00677B40 (:896): not in a building.
int fall_back_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f) ? -1 : 1000;
}

// 0x00697990 (:909)
ai_results fall_back_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (this != last_action)
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set(0);
        ai->desired_destination = np->pos;
        if (target->vehicle_handle != 0xFFFFFFFF)
        {
            const object* objp = World.object_from_handle(target->vehicle_handle);
            if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
            {
                if (objp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
                {
                    np->play_voice_line(VOICE_LINE_COMBAT_FLEE_ENEMY_WALKER, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                }
                else if (ai_perception::vehicle_is_heavy(static_cast<const vehicle*>(objp)))
                {
                    np->play_voice_line(VOICE_LINE_COMBAT_FLEE_ENEMY_TANK, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                }
            }
        }
        ai->action_substate = 0;
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x00697AA0 (:955): backs off 3 to 8 m from the target. action_substate is the attempt for combat_maneuver_get_retreat_dir_unified.
ai_results fall_back_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr || !weapon_is_ranged(params.weapon_inst))
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    const attention_look_at_modes mode = ai->perception.get_attention_mode();
    if (mode == ATTENTION_AIM_AT_OBJECT)
    {
        ai_combat_fire_at_target(params, false);
    }
    if (!ai->action_timer.elapsed() && np->movement_mode == HMM_PATHFINDING)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    uint64_t end_cell = 0xFFFFFFFFFFFFFFFFull;
    const vector desired_dir = -params.target_dir.copy_flatten_normalize_safe();
    vector dir;
    vector end_pos;
    const ai_results retreat = combat_maneuver_get_retreat_dir_unified(dir, end_pos, np, desired_dir, 3.0f, 8.0f, ai->action_substate, &end_cell);
    if (retreat == AI_RESULT_IN_PROGRESS)
    {
        ai->action_substate++;
        return AI_RESULT_IN_PROGRESS;
    }
    if (retreat == AI_RESULT_FAILURE)
    {
        np->combat.combat_maneuver_timestamp.set_rand(5000, 8000);
        ai->planner.avoid_action(this, 5000);
        return AI_RESULT_FAILURE;
    }
    ai->action_timer.set_rand(800, 1000);
    ai->action_substate = 0;
    if (end_pos == ZERO_VECTOR || ai->outside_defensive_perimeter(end_pos, false, nullptr) || !ai->perception.firing_los_clear(params.target, HST_STAND, &end_pos))
    {
        np->combat.combat_maneuver_timestamp.set_rand(5000, 8000);
        ai->planner.avoid_action(this, 5000);
        return AI_RESULT_FAILURE;
    }
    human_combat_cover_abandon_node(np);
    const weapon* equipped = np->get_equipped_weapon();
    const bool sniper = equipped != nullptr && equipped->w_info != nullptr && equipped->w_info->weapon_anim_group == ANIMATION_GROUP_SNIPER;
    human_movement_states move_state = HMS_RUN;
    const float walk_dist = (int)np->cur_movement_state < 2 ? 15.0f : 20.0f;
    const float run_dist = mode == ATTENTION_AIM_AT_OBJECT ? 10.0f : 7.5f;
    attention_look_at_modes wanted_mode;
    if (!params.weapon_inst->is_loaded())
    {
        // Empty: sprint away.
        move_state = HMS_SPRINT;
        wanted_mode = ATTENTION_LOOK_AT_NONE;
    }
    else if (params.dist_to_target <= run_dist)
    {
        wanted_mode = sniper ? ATTENTION_LOOK_AT_NONE : ATTENTION_AIM_AT_OBJECT;
    }
    else if (walk_dist < params.dist_to_target)
    {
        move_state = HMS_WALK;
        wanted_mode = ATTENTION_AIM_AT_OBJECT;
    }
    else if (ai->planner.get_time_in_current_action() < 2000 || sniper)
    {
        wanted_mode = ATTENTION_LOOK_AT_NONE;
    }
    else
    {
        wanted_mode = ATTENTION_AIM_AT_OBJECT;
    }
    if (mode != wanted_mode)
    {
        ai->perception.set_attention_mode(wanted_mode, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    ai->set_direct_movement_to_pos(end_pos, end_cell, move_state, HST_STAND, (int)np->cur_movement_state < 2);
    ai->desired_destination = end_pos;
    return AI_RESULT_IN_PROGRESS;
}

// 0x00624250 (:1094)
int advance_action::get_cost(const npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return -1;
    }
    const weapon* wp = goal_setup_data.selected_weapon;
    if (wp == nullptr)
    {
        wp = human_weapons_get_weapon(np);
    }
    if (wp == nullptr || !np->combat.no_path_to_target_timestamp.elapsed())
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return -1;
    }
    if (ai->get_defend_object() != nullptr && (human_in_flyer(target) || ai->planner.get_current_action() == defensive_combat_cover_action::get_instance()))
    {
        return -1;
    }
    if (np->current_team != HUMAN_TEAM_MARAUDER && ai_combat_should_charge_target(np) == AI_COMBAT_SHOULD_NOT_CHARGE)
    {
        const weapon_range_categories range = wp->get_range_category(vec_dist(target->pos, np->pos));
        if ((int)range < 2 || ((int)range < 3 && human_combat_cover_is_using_node(np)))
        {
            return -1;
        }
    }
    return 2500;
}

// 0x00698000 (:1156)
ai_results advance_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr || !params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    if (this != last_action)
    {
        if (params.target != nullptr)
        {
            ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
        ai->action_timer.set(0);
        ai->action_substate = 0;
    }
    advance_maybe_fire_at_target(params, false);
    return AI_RESULT_SUCCESS;
}

// 0x005E5F80 (:1185): the path starts by going away from the target, for over a metre.
static bool advance_path_in_wrong_direction(const npc* np, const human* target)
{
    const path_node* path = np->pf.path;
    if (path == nullptr)
    {
        return false;
    }
    vector to_node = path->pos - np->pos;
    const float dist = to_node.flatten_mag_and_normalize_safe();
    vector to_target = target->pos - np->pos;
    to_target.flatten_normalize_safe();
    return 1.0f < dist && to_target * to_node < 0.0f;
}

// 0x00698090 (:1199)
ai_results advance_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr || !params.weapon_inst->is_loaded())
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    if (params.dist_to_target < 2.0f)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    timestamp& timer = ai->action_timer;
    bool walk = false;
    if (timer.elapsed() && np->current_team == HUMAN_TEAM_MARAUDER)
    {
        // Marauders may walk in, firing: the chance goes from max_chance at min_dist to min_chance at max_dist.
        float t = (params.dist_to_target - Marauder_walk_advance_min_dist) / (Marauder_walk_advance_max_dist - Marauder_walk_advance_min_dist);
        if (t - 0.0f < 0.0f)
        {
            t = 0.0f;
        }
        if (0.0f <= t - 1.0f)
        {
            t = 1.0f;
        }
        walk = fl_rand_chance((Marauder_walk_advance_max_chance - Marauder_walk_advance_min_chance) * (1.0f - t) + Marauder_walk_advance_min_chance);
    }
    advance_maybe_fire_at_target(params, walk);
    const weapon_range_categories range = params.weapon_inst->get_range_category(params.dist_to_target);
    vector dest = params.target->pos;
    if (Advance_flanking_min_dist < params.dist_to_target && np->current_team == HUMAN_TEAM_MARAUDER)
    {
        // A marauder aims for the flank, as far as the nav mesh goes from the target.
        dest += (params.target_dir ^ UP_VECTOR) * np->combat.flank_delta;
        const nav_cell* target_cell = nav_cell_from_handle(params.target->pf.last_visited_nav_cell_handle);
        if (target_cell == nullptr)
        {
            dest = params.target->pos;
        }
        else
        {
            vector los_end;
            const nav_cell* los_cell;
            if (!nav_cell_find_longest_line_of_sight_no_detour(los_end, los_cell, params.target->pos, target_cell, dest, false, nullptr))
            {
                dest = los_end;
            }
        }
    }
    if (!timer.elapsed() && np->movement_mode == HMM_PATHFINDING)
    {
        if (2 < (int)range || !advance_path_in_wrong_direction(np, params.target))
        {
            return AI_RESULT_IN_PROGRESS;
        }
        ai->stop_moving();
        np->combat.no_path_to_target_timestamp.set(4000);
        return AI_RESULT_FAILURE;
    }
    human_stances stance = HST_CURRENT;
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        stance = HST_STAND;
        if ((int)range < 1)
        {
            return AI_RESULT_SUCCESS;
        }
    }
    else if (ai_combat_should_charge_target(params) == AI_COMBAT_SHOULD_NOT_CHARGE)
    {
        if ((int)range < 2)
        {
            return AI_RESULT_SUCCESS;
        }
        if (range == WEAPON_RANGE_LONG)
        {
            if (ai->action_substate != 0)
            {
                np->combat.combat_maneuver_timestamp.set_rand(3000, 5000);
                return AI_RESULT_SUCCESS;
            }
        }
        else if (range == WEAPON_OUT_OF_RANGE)
        {
            stance = HST_STAND;
        }
    }
    else
    {
        stance = HST_STAND;
    }
    timer.set_rand(2500, 4000);
    const human_stances old_stance = np->cur_stance;
    const ai_results path_result = ai_pf_to_result(ai->pathfind_to(dest, walk ? HMS_WALK : HMS_RUN, stance, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false));
    if (path_result != AI_RESULT_SUCCESS)
    {
        if (path_result == AI_RESULT_FAILURE)
        {
            np->combat.no_path_to_target_timestamp.set(4000);
            human_set_stance(np, old_stance);
        }
        return path_result;
    }
    if ((int)range < 3)
    {
        ai->action_substate = 1;
        if (advance_path_in_wrong_direction(np, params.target))
        {
            ai->stop_moving();
            np->combat.no_path_to_target_timestamp.set(4000);
            human_set_stance(np, old_stance);
            return AI_RESULT_FAILURE;
        }
    }
    if (np->pf.eventual_goal_pos != ZERO_VECTOR && !ai->outside_defensive_perimeter(np->pf.eventual_goal_pos, false, nullptr))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    // The end of the path is outside the perimeter: cut it at the first node that goes off the mesh away from the target, or out of the perimeter
    // while not closer to what is being defended.
    path_node* node = np->pf.path;
    vector previous = np->pos;
    bool trim = false;
    while (node != nullptr)
    {
        if (node->nav_cell_handle == 0xFFFFFFFFFFFFFFFFull && (node->pos - previous) * (params.target->pos - previous) < 0.0f)
        {
            trim = true;
            break;
        }
        const vector node_pos = node->pos;
        if (node_pos == ZERO_VECTOR || ai->outside_defensive_perimeter(node_pos, false, nullptr))
        {
            const gfm_orders* orders = ai->get_orders();
            const object* defended = orders != nullptr ? orders->get_target_object() : nullptr;
            if (defended == nullptr || vec_dist_squared(defended->pos, np->pos) < vec_dist_squared(defended->pos, node_pos))
            {
                trim = true;
                break;
            }
        }
        path_node* next = node->next;
        if (node->flags.jumping)
        {
            node->flags.jumping = false;
            node = next;
            trim = true;
            break;
        }
        if (next == np->pf.path || next == nullptr)
        {
            break;
        }
        node = next;
        previous = node_pos;
    }
    if (trim && node != nullptr)
    {
        path_node* head = np->pf.path;
        if (node == head)
        {
            ai->stop_moving();
            np->combat.no_path_to_target_timestamp.set(4000);
            human_set_stance(np, old_stance);
            return AI_RESULT_FAILURE;
        }
        path_node* cut = nullptr;
        if (head != nullptr)
        {
            path_node* tail = head->prev;
            node->prev->next = head;
            head->prev = node->prev;
            node->prev = tail;
            tail->next = node;
            cut = node;
        }
        bb_pf_free_path(&cut);
        const path_node* last = np->pf.path == nullptr ? nullptr : np->pf.path->prev;
        // Original quirk: no null check of the last node, as below.
        if (vec_xz_dist_squared(np->pos, last->pos) < 4.0f && fabsf(np->pos.y - last->pos.y) < 1.2f)
        {
            ai->stop_moving();
            np->combat.no_path_to_target_timestamp.set(4000);
            human_set_stance(np, old_stance);
            return AI_RESULT_FAILURE;
        }
    }
    const path_node* last = np->pf.path == nullptr ? nullptr : np->pf.path->prev;
    np->pf.eventual_goal_pos = last->pos;
    return AI_RESULT_IN_PROGRESS;
}

// 0x00698CE0 (:1489): only while advance has no path to the target, from a nav cell, at over short range unless a marauder.
int partial_advance_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions&, const npc* np, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    if (np->combat.no_path_to_target_timestamp.elapsed() || human_combat_cover_is_using_node(np))
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return -1;
    }
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr)
    {
        return -1;
    }
    nav_cell* cell = nullptr;
    if (!ai->perception.is_on_navmesh(np, &cell, 1.25f) || !get_partial_advance_to_pos(my_data.pos, np, target, cell))
    {
        return -1;
    }
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return 3000;
    }
    return 1 < (int)wp->get_range_category(vec_dist(target->pos, np->pos)) ? 3000 : -1;
}

// 0x005E60D0 (:1545)
void partial_advance_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    np->get_ai()->action_pos = setup_data.pos;
}

// 0x00698E00 (:1551)
ai_results partial_advance_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr || !params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    if (this != last_action)
    {
        if (params.target != nullptr)
        {
            ai->perception.pay_attention(params.target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        }
        ai->action_substate = 0;
    }
    advance_maybe_fire_at_target(params, false);
    return AI_RESULT_SUCCESS;
}

// 0x00698E90 (:1578)
ai_results partial_advance_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr || !params.weapon_inst->is_loaded())
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (!params.init_target(true))
    {
        return AI_RESULT_FAILURE;
    }
    if (params.dist_to_target < 2.0f)
    {
        return AI_RESULT_SUCCESS;
    }
    if (ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    advance_maybe_fire_at_target(params, false);
    const weapon_range_categories range = params.weapon_inst->get_range_category(params.dist_to_target);
    if (ai->is_pathing_to(ai->action_pos, 0.25f, 1.0f))
    {
        return AI_RESULT_IN_PROGRESS;
    }
    human_stances stance = HST_CURRENT;
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        stance = HST_STAND;
        if ((int)range < 1)
        {
            return AI_RESULT_SUCCESS;
        }
    }
    else
    {
        if ((int)range < 2)
        {
            return AI_RESULT_SUCCESS;
        }
        if (range == WEAPON_RANGE_LONG)
        {
            if (ai->action_substate != 0)
            {
                np->combat.combat_maneuver_timestamp.set_rand(3000, 5000);
                return AI_RESULT_SUCCESS;
            }
        }
        else if (range == WEAPON_OUT_OF_RANGE)
        {
            stance = HST_STAND;
        }
    }
    const ai_results result = ai_pf_to_result(ai->pathfind_to(ai->action_pos, HMS_RUN, stance, AI_PATHFIND_PRIORITY_NORMAL, 0xFFFFFFFFFFFFFFFFull, false));
    if (result != AI_RESULT_SUCCESS)
    {
        if (result == AI_RESULT_FAILURE)
        {
            np->combat.combat_maneuver_timestamp.set_rand(5000, 8000);
        }
        return result;
    }
    if ((int)range < 3)
    {
        ai->action_substate = 1;
    }
    return AI_RESULT_IN_PROGRESS;
}

// 0x00677B90 (:1690): 20 to 75 m from the target, to a cover node on the way there. Bits 59, 60 and 62 are wanted of a node in use, 62 of one
// that can fire.
int advance_using_cover_action::get_cost(ai_action_setup_data& my_data, ai_plan_conditions& desired_conditions, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || ai_combat_should_charge_target(np) != AI_COMBAT_SHOULD_NOT_CHARGE)
    {
        return -1;
    }
    if (np->hflags.riot_shield != 0 && np->shield_handle != 0xFFFFFFFF)
    {
        return -1;
    }
    nav_cell* cell;
    navpoint* npp;
    vector mesh_pos;
    if (!np->get_ai()->perception.get_nearest_navpoint_or_navcell(cell, npp, &mesh_pos, nullptr) || cell == nullptr)
    {
        return -1;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr)
    {
        return -1;
    }
    vector to_target = target->pos - np->pos;
    const float dist_sqrd = to_target.mag_squared();
    if (dist_sqrd < 400.0f || 5625.0f < dist_sqrd)
    {
        return -1;
    }
    const weapon* wp = goal_setup_data.selected_weapon;
    const float dist = sqrtf(dist_sqrd);
    to_target = to_target * (1.0f / dist);
    if (wp == nullptr)
    {
        wp = human_weapons_get_weapon(np);
    }
    if (wp == nullptr)
    {
        return -1;
    }
    cover_node* current = human_combat_cover_get_node(np);
    cover_node* node = nullptr;
    cover_check_results current_result = CCR_NO_COVER;
    if (current != nullptr)
    {
        if (human_combat_cover_is_using_node(np))
        {
            desired_conditions |= ai_plan_conditions(59) | ai_plan_conditions(60) | ai_plan_conditions(62);
        }
        else if (0.0f < (current->pos.y - np->pos.y) * to_target.y + (current->pos.x - np->pos.x) * to_target.x + (current->pos.z - np->pos.z) * to_target.z)
        {
            const cover_check_results result = current->check_cover_type(np, static_cast<const object*>(target), false, true, false);
            if (result == CCR_OFFENSIVE)
            {
                node = current;
            }
            current_result = result;
        }
    }
    bool defense_only = false;
    if (node == nullptr)
    {
        node = get_cover_node_for_advance(defense_only, np, wp, target, to_target, dist);
        if (defense_only && current_result == CCR_DEFENSIVE_ONLY)
        {
            node = current;
        }
        if (node == nullptr)
        {
            return -1;
        }
    }
    const bool can_fire = !node->is_defense_only();
    if (!defense_only && !can_fire)
    {
        return -1;
    }
    const cover_node_stances cover_stance = cover_pick_stance(node, target->pos);
    if (cover_stance == COVER_STANCE_UNDEFINED)
    {
        node->disable(COVER_DISABLED_NO_STANCES, 12000);
        return -1;
    }
    cover_node_firing_positions firing_position = COVER_FIRE_UNDEFINED;
    if (can_fire)
    {
        firing_position = cover_pick_firing_position(cover_stance, node, target->pos);
        if (firing_position == COVER_FIRE_UNDEFINED)
        {
            node->disable(COVER_DISABLED_NO_STEPOUTS, 12000);
            return -1;
        }
        desired_conditions |= ai_plan_conditions(62);
    }
    my_data.use_target = node;
    my_data.cover_stance = cover_stance;
    my_data.cover_firing_position = firing_position;
    return 1000;
}

// 0x005E6100 (:1815)
void advance_using_cover_action::commit_setup_data(npc* np, const ai_action_setup_data& setup_data) const
{
    ai_system* ai = np->get_ai();
    human_combat_cover_reserve_node(np, static_cast<cover_node*>(setup_data.use_target));
    ai->desired_cover_stance = setup_data.cover_stance;
    ai->desired_cover_firing_position = setup_data.cover_firing_position;
}

// 0x005E6140 (:1823)
ai_results advance_using_cover_action::do_action(npc* np) const
{
    np->combat.combat_maneuver_timestamp.set_rand(3000, 5000);
    return AI_RESULT_SUCCESS;
}

// 0x00677FB0 (:1833): in a building.
int fall_back_in_building_action::get_cost(const npc* np, const ai_goal_setup_data&) const
{
    return np->get_ai()->perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f) ? 1000 : -1;
}

// 0x00699070 (:1848)
ai_results fall_back_in_building_action::enter(npc* np, const ai_action* last_action) const
{
    ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const human* target = human_combat_get_target(np);
    if (target == nullptr || target->is_killed())
    {
        return AI_RESULT_FAILURE;
    }
    if (this != last_action)
    {
        ai->perception.pay_attention(target->get_handle(), AI_BELIEF_UNDEFINED, ATTENTION_USE_CURRENT_MODE, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
        ai->action_timer.set(0);
        ai->desired_destination = np->pos;
    }
    human_set_combat_ready(np, true, 0);
    return AI_RESULT_SUCCESS;
}

// 0x00699110 (:1882)
ai_results fall_back_in_building_action::do_action(npc* np) const
{
    ai_system* ai = np->get_ai();
    if (ai == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    ai_combat_action_parameters params;
    params.init_npc(np);
    if (params.weapon_inst == nullptr || !params.weapon_inst->is_loaded())
    {
        return AI_RESULT_FAILURE;
    }
    human_set_combat_ready(np, true, 0);
    if (!params.init_target(true) || ai->perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) == nullptr)
    {
        return AI_RESULT_FAILURE;
    }
    const attention_look_at_modes mode = ai->perception.get_attention_mode();
    if (mode == ATTENTION_AIM_AT_OBJECT)
    {
        ai_combat_fire_at_target(params, false);
    }
    if (!ai->action_timer.elapsed() && np->movement_mode == HMM_PATHFINDING)
    {
        return AI_RESULT_IN_PROGRESS;
    }
    ai->action_timer.set_rand(800, 1000);
    const vector desired_dir = -params.target_dir.copy_flatten_normalize_safe();
    vector dir;
    vector end_pos;
    unsigned int end_navpoint = 0xFFFFFFFF;
    if (!combat_maneuver_get_indoor_retreat_dir(dir, end_pos, np, desired_dir, &end_navpoint) || !ai->perception.firing_los_clear(params.target, HST_STAND, &end_pos))
    {
        np->combat.combat_maneuver_timestamp.set_rand(5000, 8000);
        ai->planner.avoid_action(this, 8000);
        return AI_RESULT_FAILURE;
    }
    human_combat_cover_abandon_node(np);
    const bool loaded = params.weapon_inst->is_loaded();
    const attention_look_at_modes wanted_mode = loaded ? ATTENTION_AIM_AT_OBJECT : ATTENTION_LOOK_AT_NONE;
    if (mode != wanted_mode)
    {
        ai->perception.set_attention_mode(wanted_mode, ATTENTION_STEERING_USE_DEFAULT, ATTENTION_AWARENESS_NORMAL);
    }
    // Original quirk: the navpoint's handle goes in as the nav cell hint, its upper half zero.
    ai->set_direct_movement_to_pos(end_pos, (uint64_t)end_navpoint, loaded ? HMS_WALK : HMS_SPRINT, HST_STAND, (int)np->cur_movement_state < 2);
    ai->desired_destination = end_pos;
    return AI_RESULT_IN_PROGRESS;
}

// The rest, constructors and macro get_cost overloads, from tools/headless/ai_action_gen.py.

fall_back_action fall_back_action::the_instance; // 0x01DA0674

// 0x00620B20 (.h:12)
fall_back_action::fall_back_action()
    : ai_action("fall back", ai_plan_conditions_from_bits({2, 9, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({84}))
{
    flags.has_enter = 1;
}

ai_action* fall_back_action::get_instance()
{
    return &the_instance;
}

// 0x005E3F40
fall_back_action::~fall_back_action()
{
}

// 0x00620DB0 (.h:12)
int fall_back_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

advance_action advance_action::the_instance; // 0x01D75440

// 0x00620DF0 (.h:13)
advance_action::advance_action()
    : ai_action("advance", ai_plan_conditions_from_bits({2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({83}))
{
    flags.has_enter = 1;
}

ai_action* advance_action::get_instance()
{
    return &the_instance;
}

// 0x005E3F60
advance_action::~advance_action()
{
}

// 0x006210F0 (.h:13)
int advance_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

partial_advance_action partial_advance_action::the_instance; // 0x01D72458

// 0x00621130 (.h:14)
partial_advance_action::partial_advance_action()
    : ai_action("partial advance", ai_plan_conditions_from_bits({2, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({83}))
{
    flags.has_enter = 1;
}

ai_action* partial_advance_action::get_instance()
{
    return &the_instance;
}

// 0x005E3F80
partial_advance_action::~partial_advance_action()
{
}

// 0x00621430 (.h:14)
int partial_advance_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

advance_using_cover_action advance_using_cover_action::the_instance; // 0x01D9F8F8

// 0x00621460 (.h:15)
advance_using_cover_action::advance_using_cover_action()
    : ai_action("advance using cover", ai_plan_conditions_from_bits({2, 9, 47, 59, 60, 136, 137, 145, 153}), ai_plan_conditions_from_bits({83}))
{
}

ai_action* advance_using_cover_action::get_instance()
{
    return &the_instance;
}

// 0x005E3F90
advance_using_cover_action::~advance_using_cover_action()
{
}

// 0x006217A0 (.h:15)
int advance_using_cover_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

fall_back_in_building_action fall_back_in_building_action::the_instance; // 0x01DCA330

// 0x006217D0 (.h:17)
fall_back_in_building_action::fall_back_in_building_action()
    : ai_action("fall back building", ai_plan_conditions_from_bits({9, 47, 136, 137, 145, 153}), ai_plan_conditions_from_bits({84}))
{
    flags.has_enter = 1;
}

ai_action* fall_back_in_building_action::get_instance()
{
    return &the_instance;
}

// 0x005E3FA0
fall_back_in_building_action::~fall_back_in_building_action()
{
}

// 0x00621A00 (.h:17)
int fall_back_in_building_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

retreat_defensive_perimeter_building_action retreat_defensive_perimeter_building_action::the_instance; // 0x01DDB0BC

// 0x00621A40 (.h:19)
retreat_defensive_perimeter_building_action::retreat_defensive_perimeter_building_action()
    : ai_action("retreat def building", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({152}))
{
}

ai_action* retreat_defensive_perimeter_building_action::get_instance()
{
    return &the_instance;
}

// 0x005E3FB0
retreat_defensive_perimeter_building_action::~retreat_defensive_perimeter_building_action()
{
}

// 0x00621D30 (.h:19)
int retreat_defensive_perimeter_building_action::get_cost(const npc*, const ai_goal_setup_data&) const
{
    return -1;
}

retreat_defensive_perimeter_default_action retreat_defensive_perimeter_default_action::the_instance; // 0x01DC9FA0

// 0x00621D60 (.h:20)
retreat_defensive_perimeter_default_action::retreat_defensive_perimeter_default_action()
    : ai_action("retreat def default", ai_plan_conditions_from_bits({4, 9, 47, 58, 136, 137, 145, 153}), ai_plan_conditions_from_bits({152}))
{
    flags.has_enter = 1;
}

ai_action* retreat_defensive_perimeter_default_action::get_instance()
{
    return &the_instance;
}

// 0x005E3FD0
retreat_defensive_perimeter_default_action::~retreat_defensive_perimeter_default_action()
{
}

// 0x00622060 (.h:20)
int retreat_defensive_perimeter_default_action::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc* np, const ai_goal_setup_data& goal_setup_data, const ai_planner_search_node*) const
{
    return get_cost(np, goal_setup_data);
}

follow_to_defensive_perimeter_rule follow_to_defensive_perimeter_rule::the_instance; // 0x01D754A8

// 0x006220A0 (.h:22)
follow_to_defensive_perimeter_rule::follow_to_defensive_perimeter_rule()
    : ai_production_rule("follow to def perimeter", ai_plan_conditions_from_bits({39}), ai_plan_conditions_from_bits({152}))
{
}

ai_action* follow_to_defensive_perimeter_rule::get_instance()
{
    return &the_instance;
}

// 0x005E3FE0
follow_to_defensive_perimeter_rule::~follow_to_defensive_perimeter_rule()
{
}

// 0x00622110 (.h:22)
int follow_to_defensive_perimeter_rule::get_cost(ai_action_setup_data&, ai_plan_conditions&, const npc*, const ai_goal_setup_data&, const ai_planner_search_node*) const
{
    return 0;
}

