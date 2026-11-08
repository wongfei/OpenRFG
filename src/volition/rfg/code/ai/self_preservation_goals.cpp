// Original: volition/rfg/code/ai/self_preservation_goals.cpp

#include "volition/rfg/code/ai/self_preservation_goals.h"

#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_planning_data.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_enums.h"
#include "volition/rfg/code/ai/hazard_detection.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_utilities.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <math.h>

// goal_pool fills in construction order and rank_goals breaks ties by it: this file's singletons follow goals.cpp's (their
// entries in rfg.exe's .CRT$XC table run 0x012417xx-0x01241920 right after goals.cpp's). A numbered section keeps that
// across translation units, which link order would not.
#pragma warning(disable: 4075)
#pragma init_seg(".CRT$XCUAG31")

// 0x005F2F90 (:121)
bool should_evade_sledgehammer_charge(const npc* np)
{
    const player* local = player_get_local_player();
    if (local == nullptr || !local->pflags.do_sprint || human_combat_get_target(np) != local || vec_dist_squared(np->pos, local->pos) >= 225.0f)
    {
        return false;
    }
    const vector to_npc(np->pos.x - local->pos.x, np->pos.y - local->pos.y, np->pos.z - local->pos.z);
    const vector direction = to_npc.copy_flatten_normalize();
    // Within 30 degrees of the player's facing.
    return 0.8660254f < local->orient.fvec.y * direction.y + direction.x * local->orient.fvec.x + local->orient.fvec.z * direction.z;
}

// 0x006879A0 (:153). LTCG: start EAX, dir ECX, distance XMM0, obstruction_pos ESI. True when nothing is hit.
bool do_movement_raycast(const vector& start, const vector& dir, float distance, vector* obstruction_pos)
{
    if (distance - 1.0f < 0.0f)
    {
        distance = 1.0f;
    }
    raycast_input input;
    input.radius = 0.0f;
    input.start.x = start.x + dir.x * 0.55f;
    input.start.y = start.y + dir.y * 0.55f + 0.4f;
    input.start.z = start.z + dir.z * 0.55f;
    input.filter_params = nullptr;
    input.filter_callback = nullptr;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    input.end.x = start.x + dir.x * distance;
    input.end.y = start.y + dir.y * distance + 0.4f;
    input.end.z = start.z + dir.z * distance;
    input.m_filter_info = COLLIDE_PHYSICAL_OBJECT;

    raycast_output output;
    const bool hit = havok_raycast(input, output);
    if (obstruction_pos != nullptr)
    {
        *obstruction_pos = hit ? output.m_hits[0].hit_pos : ZERO_VECTOR;
    }
    return !hit;
}

// 0x00642940 (:188). LTCG: candidates EDX, dir ESI: a bubble sort, most aligned with dir first.
void sort_evade_directions(farray<vector, 4>& candidates, const vector& dir)
{
    const int last = candidates.size() - 1;
    for (int i = 0; i < last; i++)
    {
        for (int j = last; j > i; j--)
        {
            const vector& low = candidates[j - 1];
            const vector& high = candidates[j];
            if (low.y * dir.y + dir.x * low.x + low.z * dir.z < high.y * dir.y + dir.x * high.x + high.z * dir.z)
            {
                const vector swap = candidates[j];
                candidates[j] = candidates[j - 1];
                candidates[j - 1] = swap;
            }
        }
    }
}

// 0x00687BF0 (:213): takes the first direction the npc can move in, otherwise the first one, trapped.
void evaluate_evade_directions(ai_goal_setup_data& setup_data, const npc* np, const farray<vector, 4>& candidate_directions, float total_distance)
{
    const ai_perception& perception = np->get_ai()->perception;
    nav_cell* cell = nullptr;
    navpoint* start_navpoint = nullptr;
    vector navmesh_pos;
    perception.get_nearest_navpoint_or_navcell(cell, start_navpoint, &navmesh_pos, nullptr);
    vector center;
    np->get_center_pos(center);

    const int count = candidate_directions.size();
    for (int i = 0; i < count; i++)
    {
        const vector& dir = candidate_directions[i];
        if (!do_movement_raycast(np->pos, dir, total_distance, nullptr))
        {
            continue;
        }
        const vector dest(dir.x * total_distance + center.x, dir.y * total_distance + center.y, center.z + dir.z * total_distance);
        bool clear = false;
        if (cell == nullptr)
        {
            if (start_navpoint == nullptr || vec_xz_dist(np->pos, start_navpoint->pos) - start_navpoint->radius > 0.5f)
            {
                clear = true;
            }
            else
            {
                navpoint* end_navpoint;
                float closest_dist_squared;
                clear = bb_pf_find_closest_navpoint(dest, end_navpoint, closest_dist_squared) && closest_dist_squared <= 0.25f
                    && perception.has_clear_line_of_travel(dest, start_navpoint, end_navpoint);
            }
        }
        else
        {
            vector reached;
            const nav_cell* reached_cell;
            clear = nav_cell_find_longest_line_of_sight_no_detour(reached, reached_cell, navmesh_pos, cell, dest, false, nullptr) || vec_dist_squared(dest, reached) <= 0.25f;
        }
        if (clear)
        {
            setup_data.direction = dir;
            setup_data.flags.trapped = false;
            return;
        }
    }
    setup_data.direction = candidate_directions[0];
    setup_data.flags.trapped = true;
}

// 0x00687E50 (:304): straight away from pos, or 45 degrees to either side. LTCG: np EDI.
void setup_evade_data(ai_goal_setup_data& setup_data, const npc* np, const vector& pos)
{
    setup_data.haz_info.time_to_pass = -1.0f;
    setup_data.haz_info.speed = 0.0f;
    setup_data.haz_info.will_impact = false;

    vector away(np->pos.x - pos.x, 0.0f, np->pos.z - pos.z);
    if (away.x == ZERO_VECTOR.x && ZERO_VECTOR.y == 0.0f && away.z == ZERO_VECTOR.z)
    {
        away = FORWARD_VECTOR;
    }
    away.normalize();

    farray<vector, 4> candidates;
    candidates.add(away);
    const vector side(-candidates[0].z, 0.0f, candidates[0].x);
    candidates.add(vector((candidates[0].x + side.x) * 0.70710677f, (candidates[0].y + 0.0f) * 0.70710677f, (candidates[0].z + side.z) * 0.70710677f));
    candidates.add(vector((candidates[0].x - side.x) * 0.70710677f, (candidates[0].y - 0.0f) * 0.70710677f, (candidates[0].z - side.z) * 0.70710677f));

    sort_evade_directions(candidates, np->orient.fvec);
    evaluate_evade_directions(setup_data, np, candidates, 9.5f);
}

// 0x006881E0 (:351): off the object's path. LTCG: np ECX.
void setup_evade_data(ai_goal_setup_data& setup_data, const npc* np, const object* obj)
{
    get_hazard_physics_info(setup_data.haz_info, np, obj);
    if (!setup_data.haz_info.will_impact)
    {
        setup_evade_data(setup_data, np, obj->pos);
        return;
    }

    float evade_dist = setup_data.haz_info.time_to_pass * 5.0f;
    if (7.0f - evade_dist < 0.0f)
    {
        evade_dist = 7.0f;
    }
    const float total_distance = evade_dist + 2.5f;

    vector away(np->pos.x - setup_data.haz_info.pass_point.x, 0.0f, np->pos.z - setup_data.haz_info.pass_point.z);
    if (away.is_null())
    {
        const vector& to_npc = setup_data.haz_info.obj_to_npc;
        away = vector(to_npc.y * UP_VECTOR.z - to_npc.z * UP_VECTOR.y, to_npc.z * UP_VECTOR.x - to_npc.x * UP_VECTOR.z, to_npc.x * UP_VECTOR.y - to_npc.y * UP_VECTOR.x);
    }
    if (away.is_null())
    {
        away = FORWARD_VECTOR;
    }
    away.normalize();

    farray<vector, 4> candidates;
    candidates.add(away);

    float side_x = -candidates[0].z;
    float side_y = 0.0f;
    float side_z = candidates[0].x;
    const vector& velocity = setup_data.haz_info.velocity;
    if (velocity.x * side_x + velocity.y * 0.0f + velocity.z * side_z < 0.0f)
    {
        side_x = -side_x;
        side_y = -0.0f;
        side_z = -side_z;
    }
    side_x *= 0.5f;
    side_y *= 0.5f;
    side_z *= 0.5f;

    // cos 30 and sin 30.
    candidates.add(vector(candidates[0].x * 0.8660254f + side_x, candidates[0].y * 0.8660254f + side_y, candidates[0].z * 0.8660254f + side_z));
    if (setup_data.haz_info.distance_at_pass * 2.0f < evade_dist)
    {
        candidates.add(vector(candidates[0].x * 0.8660254f - side_x, candidates[0].y * 0.8660254f - side_y, candidates[0].z * 0.8660254f - side_z));
    }
    if (setup_data.haz_info.distance_at_pass * 4.0f < evade_dist)
    {
        candidates.add(vector(candidates[0].x * -0.8660254f + side_x, candidates[0].y * -0.8660254f + side_y, candidates[0].z * -0.8660254f + side_z));
    }

    sort_evade_directions(candidates, np->orient.fvec);
    evaluate_evade_directions(setup_data, np, candidates, total_distance);
}

// 0x00688800 (:420)
const ai_belief* ai_get_enemy_flee_belief(const npc* np)
{
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return nullptr;
    }
    const ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if (squad != nullptr && squad->get_player() != nullptr)
    {
        return nullptr;
    }
    if (human_team_is_non_combatant(np))
    {
        return ai->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION);
    }
    if (np->npc_flags.bureaucrat)
    {
        // Original quirk kept: the local player is used with no null check.
        const ai_belief_specification specifications[3] = {
            ai_belief_specification(AI_BELIEF_FIRST_CONTACT, player_get_local_player()->get_handle(), -1, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_ENEMY_EXISTS, player_get_local_player()->get_handle(), 20000, BELIEF_REQUIREMENTS_FLEEING_FROM),
            ai_belief_specification(AI_BELIEF_ENEMY_EXISTS, 0xFFFFFFFF, 20000, BELIEF_REQUIREMENTS_FLEEING_FROM),
        };
        ai_belief* belief = nullptr;
        if (ai->perception.collect_all_matching_beliefs(&belief, 1, specifications, 3, 0xFFFFFFFF) >= 1 && belief != nullptr)
        {
            return belief;
        }
        const ai_belief_specification attacks[4] = {
            ai_belief_specification(AI_BELIEF_EXPLOSION, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NEARBY),
            ai_belief_specification(AI_BELIEF_DAMAGE_TAKEN, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_BULLET_IMPACT, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NEARBY),
            ai_belief_specification(AI_BELIEF_SHOT_AT, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
        };
        const ai_belief* attack = ai->perception.find_matching_belief(attacks, 4, 0xFFFFFFFF);
        return attack != nullptr ? ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS, attack->handle) : nullptr;
    }

    // Alone and on foot: flees from three or more marauders.
    if (ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY) == nullptr && human_in_vehicle(np) == nullptr)
    {
        int marauders = 0;
        const ai_belief* first = nullptr;
        belief_obj_iterator<human> enemies;
        enemies.init(ai, AI_BELIEF_ENEMY_POSITION);
        for (; enemies; ++enemies)
        {
            if (enemies->current_team == HUMAN_TEAM_MARAUDER)
            {
                marauders++;
                if (first == nullptr)
                {
                    first = enemies.get_belief();
                }
            }
        }
        if (marauders > 2)
        {
            return first;
        }
    }
    return nullptr;
}

// 0x0066DC10 (:518)
const ai_belief* ai_get_flee_to_safety_belief(const npc* np)
{
    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        return nullptr;
    }
    const ai_system* ai = np->get_ai();
    const object_squad* squad = ai->get_squad();
    if ((squad != nullptr && squad->get_player() != nullptr) || np->npc_flags.bureaucrat)
    {
        return nullptr;
    }

    const bool non_combatant = human_team_is_non_combatant(np);
    const ai_belief_specification projectile(AI_BELIEF_PROJECTILE, 0xFFFFFFFF, -1, non_combatant ? BELIEF_REQUIREMENTS_NEARBY_OR_FLEEING_FROM : BELIEF_REQUIREMENTS_NEARBY);
    ai_belief* belief = nullptr;
    if (ai->perception.collect_all_matching_beliefs(&belief, 1, &projectile, 1, 0xFFFFFFFF) >= 1 && belief != nullptr)
    {
        return belief;
    }
    if (!non_combatant && inv_item_get_best_weapon(np, false) != nullptr && Rfg_camera.mode != CAMERA_SATELLITE_MODE)
    {
        return nullptr;
    }

    const int time_since = (non_combatant && gsm_get_alert_level() > 0) ? 30000 : 10000;
    const ai_belief_specification specifications[5] = {
        ai_belief_specification(AI_BELIEF_EXPLOSION, 0xFFFFFFFF, time_since, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_DAMAGE_TAKEN, 0xFFFFFFFF, time_since, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_VEHICLE, 0xFFFFFFFF, time_since, BELIEF_REQUIREMENTS_HEAVY_VEHICLE_THREAT),
        ai_belief_specification(AI_BELIEF_BULLET_IMPACT, 0xFFFFFFFF, time_since, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_COMBAT_SOUND, 0xFFFFFFFF, time_since, BELIEF_REQUIREMENTS_NONE),
    };
    belief = nullptr;
    return ai->perception.collect_all_matching_beliefs(&belief, 1, specifications, 5, 0xFFFFFFFF) >= 1 ? belief : nullptr;
}

// 0x00642E00 (:589)
int flee_unsafe_building_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const rfg_mover* building = ai->perception.get_nearest_unsafe_building();
    if (building == nullptr)
    {
        return -1;
    }
    const object* defended = ai->get_defend_object();
    if (defended != nullptr && defended->get_obj_type() == OT_HUMAN)
    {
        return -1;
    }
    const float safe_dist = building->flags.game_destroyed ? 15.0f : 0.1f;
    return ai_building_find_bbox_dist_squared(np->pos, building) < safe_dist * safe_dist ? 7000 : -1;
}

// 0x00642EC0 (:646)
bool flee_unsafe_building_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    rfg_mover* building = np->get_ai()->perception.get_nearest_unsafe_building();
    if (building == nullptr)
    {
        return false;
    }
    goal_setup_data.selected_target = building;
    goal_setup_data.attention_steering_mode = ATTENTION_STEERING_PLANT_FEET;
    goal_setup_data.attention_awareness_level = ATTENTION_AWARENESS_OBLIVIOUS;
    goal_setup_data.attention_belief_type = AI_BELIEF_BUILDING;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    return true;
}

// 0x006AB5E0 (:663): flag 1 of the belief keeps the voice line to once per building.
void flee_unsafe_building_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_system* ai = np->get_ai();
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
    if (belief != nullptr && (belief->flags & 1) == 0)
    {
        if (ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY) != nullptr)
        {
            np->play_voice_line(VOICE_LINE_COMBAT_FLEE_COLLAPSING_BUILDING, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        belief->flags |= 1;
    }
}

// 0x005F30C0 (:684)
int leave_demolition_area_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return activity_raid_is_in_demolition_area(np->pos) ? 7100 : -1;
}

// 0x00642F00 (:694)
bool leave_demolition_area_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    if (!activity_raid_is_in_demolition_area(np->pos))
    {
        return false;
    }
    object* target = activity_raid_get_target();
    if (target == nullptr || target->get_obj_type() != OT_MOVER || target->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return false;
    }
    goal_setup_data.selected_target = target;
    goal_setup_data.attention_belief_type = AI_BELIEF_BUILDING;
    goal_setup_data.attention_mode = ATTENTION_IDLE;
    return true;
}

// 0x006AB630 (:720)
void leave_demolition_area_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    goal_setup_data.set_attention(np, nullptr);
}

static const ai_belief* find_attached_projectile_belief(const npc* np)
{
    const ai_belief* head = np->get_ai()->perception.get_first_belief(AI_BELIEF_PROJECTILE);
    for (const ai_belief* belief = head; belief != nullptr;)
    {
        if (BELIEF_REQUIREMENTS_ATTACHED_TO_ME.test(belief->flags))
        {
            return belief;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return nullptr;
}

static const projectile* projectile_from_handle(unsigned int handle)
{
    const object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE) ? static_cast<const projectile*>(objp) : nullptr;
}

static const ai_belief* find_hazard_belief(const npc* np, object_type type)
{
    return np->get_ai()->perception.get_belief(AI_BELIEF_HAZARD, BELIEF_REQUIREMENTS_NONE, type);
}

// 0x00642F50 (:728)
int react_attached_projectile_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_belief* belief = find_attached_projectile_belief(np);
    return (belief != nullptr && projectile_from_handle(belief->handle) != nullptr) ? 51000 : -1;
}

// 0x00642FE0 (:764)
bool react_attached_projectile_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = find_attached_projectile_belief(np);
    const projectile* pp = belief != nullptr ? projectile_from_handle(belief->handle) : nullptr;
    if (pp == nullptr || pp->owner_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* owner = World.object_from_handle(pp->owner_handle);
    if (owner == nullptr || owner->get_obj_type() != OT_HUMAN)
    {
        return false;
    }
    goal_setup_data.selected_target = owner;
    goal_setup_data.attention_awareness_level = ATTENTION_AWARENESS_OBLIVIOUS;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    return true;
}

// 0x006AB650 (:794)
void react_attached_projectile_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
    goal_setup_data.set_attention(np, nullptr);
}

// 0x005F30E0 (:804)
int react_on_fire_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return np->hflags.on_fire ? 52000 : -1;
}

// 0x005F3100 (:813)
bool react_on_fire_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    if (!np->hflags.on_fire)
    {
        return false;
    }
    goal_setup_data.attention_awareness_level = ATTENTION_AWARENESS_OBLIVIOUS;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    return true;
}

// 0x006AB6D0 (:826)
void react_on_fire_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || np->current_team == HUMAN_TEAM_EDF)
    {
        np->char_inst->vp->animation_set = anim_set_find("Civilian_Male");
    }
    if (np->get_equipped_inv_item() != nullptr)
    {
        human_weapons_drop_weapon(np, np->get_equipped_inv_item());
    }
    np->get_ai()->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
    if (np->char_inst != nullptr && np->char_inst->vp != nullptr)
    {
        vanim_set_temp_animation_set_variant(np->char_inst->vp, ANIM_SET_VARIANT_ON_FIRE);
    }
    goal_setup_data.set_attention(np, nullptr);
}

// 0x006430A0 (:848)
int evade_projectile_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || (np->hflags.riot_shield && np->shield_handle != 0xFFFFFFFF))
    {
        return -1;
    }
    int fitness;
    switch (np->get_ai()->projectile_reaction)
    {
    case AI_PROJECTILE_REACTION_NONE:
        return -1;
    case AI_PROJECTILE_REACTION_DODGE:
        fitness = 6700;
        break;
    case AI_PROJECTILE_REACTION_EVADE:
        fitness = (human_combat_cover_is_using_node(np) && int_rand_chance(50)) ? 6700 : 6900;
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        return -1;
    }
    if (human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    return (belief != nullptr && World.object_from_handle<projectile>(belief->handle) != nullptr) ? fitness : -1;
}

// 0x00688A90 (:908)
bool evade_projectile_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_PROJECTILE)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    setup_evade_data(goal_setup_data, np, objp);
    return true;
}

// 0x006AB7D0 (:934): flag 1 of the belief keeps the voice line to once per projectile.
void evade_projectile_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
    const projectile* pp = belief != nullptr ? projectile_from_handle(belief->handle) : nullptr;
    if (pp == nullptr)
    {
        return;
    }
    ai->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
    ai->desired_evade_dir = goal_setup_data.direction;
    if ((belief->flags & 1) == 0 && ai->perception.get_first_belief(AI_BELIEF_CLOSEST_ALLY) != nullptr)
    {
        const voice_lines line = pp->is_thrown_weapon() ? VOICE_LINE_COMBAT_SPOT_GRENADE_GROUP : VOICE_LINE_COMBAT_SPOT_PROJECTILE_GROUP;
        np->play_voice_line(line, match_goal_callback, const_cast<evade_projectile_goal*>(this), AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        belief->flags |= 1;
    }
}

// 0x00643170 (:965)
int dodge_projectile_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (should_evade_sledgehammer_charge(np))
    {
        return 6800;
    }
    if (np->get_ai()->projectile_reaction == AI_PROJECTILE_REACTION_NONE || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    const projectile* pp = belief != nullptr ? projectile_from_handle(belief->handle) : nullptr;
    if (pp == nullptr)
    {
        return -1;
    }
    const human* owner = World.object_from_handle<human>(pp->owner_handle);
    return (owner != nullptr && human_teams_are_unfriendly(np, owner)) ? 6800 : -1;
}

// The object's pass point when it will hit, otherwise a second along its path.
static vector get_dodge_position(const hazard_physics_info& info, const object* objp)
{
    if (info.will_impact)
    {
        return info.pass_point;
    }
    return vector(objp->pos.x + info.velocity.x, objp->pos.y + info.velocity.y, objp->pos.z + info.velocity.z);
}

// 0x0066DE50 (:1007)
bool dodge_projectile_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    player* local = player_get_local_player();
    if (local != nullptr && should_evade_sledgehammer_charge(np))
    {
        goal_setup_data.selected_target = local;
        goal_setup_data.attention_belief_type = AI_BELIEF_UNDEFINED;
        goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
        goal_setup_data.position = local->pos;
        return true;
    }
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_PROJECTILE)
    {
        return false;
    }
    const unsigned int owner_handle = static_cast<const projectile*>(objp)->owner_handle;
    object* owner = owner_handle != 0xFFFFFFFF ? World.object_from_handle(owner_handle) : nullptr;
    if (owner == nullptr || owner->get_obj_type() != OT_HUMAN || human_teams_get_disposition(np, static_cast<const human*>(owner)) >= TEAM_DISPOSITION_NONE)
    {
        return false;
    }
    hazard_physics_info info;
    get_hazard_physics_info(info, np, objp);
    goal_setup_data.attention_belief_type = AI_BELIEF_UNDEFINED;
    goal_setup_data.selected_target = owner;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    goal_setup_data.use_target = objp;
    goal_setup_data.position = get_dodge_position(info, objp);
    return true;
}

// 0x005F3130 (:1054)
void dodge_projectile_goal::commit_setup_data(npc* np, const ai_goal_setup_data&) const
{
    np->get_ai()->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
}

// 0x00643210 (:1063)
int evade_thrown_weapon_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return -1;
    }
    return vrand() % 100 > 50 ? 6900 : 6700;
}

// 0x00688B40 (:1096)
bool evade_thrown_weapon_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    setup_evade_data(goal_setup_data, np, objp);
    return true;
}

// 0x006AB910 (:1122)
void evade_thrown_weapon_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
    if (belief != nullptr)
    {
        ai->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
        ai->desired_evade_dir = goal_setup_data.direction;
    }
}

// A thrown weapon out of the inventory, thrown within 3 s (0xBB8) by someone unfriendly.
static const human* get_thrown_weapon_threat(const npc* np, const weapon* wp)
{
    if (!wp->w_info->flags.non_inventory || wp->time_since_fired() > 3000)
    {
        return nullptr;
    }
    const human* owner = World.object_from_handle<human>(wp->owner_handle);
    return (owner != nullptr && human_teams_are_unfriendly(np, owner)) ? owner : nullptr;
}

// 0x006432A0 (:1153)
int dodge_thrown_weapon_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return -1;
    }
    return get_thrown_weapon_threat(np, static_cast<const weapon*>(objp)) != nullptr ? 6800 : -1;
}

// 0x0066E020 (:1190)
bool dodge_thrown_weapon_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = find_hazard_belief(np, OT_ITEM);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return false;
    }
    const human* owner = get_thrown_weapon_threat(np, static_cast<const weapon*>(objp));
    if (owner == nullptr)
    {
        return false;
    }
    hazard_physics_info info;
    get_hazard_physics_info(info, np, objp);
    goal_setup_data.attention_belief_type = AI_BELIEF_UNDEFINED;
    goal_setup_data.selected_target = const_cast<human*>(owner);
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    goal_setup_data.position = get_dodge_position(info, objp);
    return true;
}

// 0x005F31A0 (:1230)
void dodge_thrown_weapon_goal::commit_setup_data(npc* np, const ai_goal_setup_data&) const
{
    np->get_ai()->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
}

// 0x00643350 (:1239)
int evade_vehicle_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_combat_get_manned_turret(np) != nullptr || human_in_vehicle_interaction(np, nullptr))
    {
        return -1;
    }
    const ai_belief* belief = find_hazard_belief(np, OT_VEHICLE);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* objp = World.object_from_handle(belief->handle);
    return (objp != nullptr && objp->get_obj_type() == OT_VEHICLE) ? 5700 : -1;
}

static const ai_belief* find_hazard_of_type(const npc* np, object_type type)
{
    const ai_belief* head = np->get_ai()->perception.get_first_belief(AI_BELIEF_HAZARD);
    for (const ai_belief* belief = head; belief != nullptr;)
    {
        if (belief->get_obj_type() == type)
        {
            return belief;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00688BF0 (:1261)
bool evade_vehicle_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = find_hazard_of_type(np, OT_VEHICLE);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    setup_evade_data(goal_setup_data, np, objp);
    return true;
}

static const float SIGHTED_ENEMY_RELEVANCE = 1.0f; // 0x01642F60

// 0x006BF340 (:1286)
void evade_vehicle_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        return;
    }
    const vehicle* vp = static_cast<const vehicle*>(objp);
    if ((belief->flags & 1) == 0)
    {
        const object* last_driver = vp->most_recent_driver != 0xFFFFFFFF ? World.object_from_handle(vp->most_recent_driver) : nullptr;
        if (last_driver != nullptr && last_driver->get_obj_type() == OT_HUMAN && human_teams_get_disposition(np, static_cast<const human*>(last_driver)) == TEAM_DISPOSITION_HOSTILE)
        {
            ai->perception.note_possible_enemy_location(static_cast<const human*>(last_driver), SIGHTED_ENEMY_RELEVANCE, true);
        }
        // The line after the friendly one is for a hostile or unfriendly driver.
        const int unfriendly = human_teams_get_disposition(np, vp->team_of_most_recent_driver) < TEAM_DISPOSITION_NONE ? 1 : 0;
        np->play_voice_line(static_cast<voice_lines>(VOICE_LINE_VEHICLE_DODGE_FRIENDLY + unfriendly), match_goal_callback, const_cast<evade_vehicle_goal*>(this), AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        belief->flags |= 1;
    }
    ai->desired_evade_dir = goal_setup_data.direction;
    ai->desired_vehicle = vp->get_handle();
    if (np->current_team == HUMAN_TEAM_EDF)
    {
        const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (driver != nullptr && human_teams_get_disposition(np, driver) < TEAM_DISPOSITION_NONE)
        {
            ai->perception.note_possible_enemy_location(driver, SIGHTED_ENEMY_RELEVANCE, true);
        }
    }
}

// 0x0066E1C0 (:1333)
int flee_debris_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || human_in_vehicle_interaction(np, nullptr))
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    const ai_belief_requirements requirements(0, 0, 0);
    if (perception.get_belief(AI_BELIEF_HAZARD, requirements, OT_MOVER) == nullptr)
    {
        return -1;
    }
    return perception.is_in_closest_building(np, nullptr, 0.0f, 1.25f) ? -1 : 6300;
}

// 0x005F3210 (:1361)
bool flee_debris_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = find_hazard_of_type(np, OT_MOVER);
    if (belief == nullptr)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.attention_belief_obj_type = belief->get_obj_type();
    goal_setup_data.selected_target = nullptr;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_BELIEF;
    goal_setup_data.attention_awareness_level = ATTENTION_AWARENESS_OBLIVIOUS;
    return true;
}

// 0x006AB9B0 (:1381)
void flee_debris_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
    if (belief != nullptr)
    {
        belief->flags |= 1;
    }
}

// 0x00688C70 (:1402)
vehicle* get_nearest_unsafe_vehicle(const npc* np)
{
    belief_obj_iterator<vehicle> vehicles;
    vehicles.init(np->get_ai(), AI_BELIEF_VEHICLE);
    for (; vehicles; ++vehicles)
    {
        vehicle* vp = vehicles;
        if (vehicle_is_on_fire(vp))
        {
            return vp;
        }
    }
    return nullptr;
}

// 0x00688D10 (:1418)
int flee_from_vehicle_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const object* objp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->vehicle_handle) : nullptr;
    if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
    {
        if (activity_cc_is_active_courier(np))
        {
            return -1;
        }
        return vehicle_is_on_fire(static_cast<const vehicle*>(objp)) ? 6500 : -1;
    }
    const vehicle* unsafe = get_nearest_unsafe_vehicle(np);
    return (unsafe != nullptr && vec_dist_squared(unsafe->pos, np->pos) <= 144.0f) ? 6500 : -1;
}

// 0x00688D90 (:1450)
bool flee_from_vehicle_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    object* objp = np->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle(np->vehicle_handle) : nullptr;
    if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
    {
        objp = get_nearest_unsafe_vehicle(np);
        if (objp == nullptr)
        {
            return false;
        }
    }
    goal_setup_data.attention_belief_type = AI_BELIEF_VEHICLE;
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_belief_obj_type = objp->get_obj_type();
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    goal_setup_data.attention_steering_mode = ATTENTION_STEERING_USE_CURRENT_MODE;
    return true;
}

// 0x006AB9E0 (:1473)
void flee_from_vehicle_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    np->get_ai()->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
    np->npc_flags.ambient_flee = 1;
    goal_setup_data.set_attention(np, nullptr);
    np->play_voice_line(VOICE_LINE_COMBAT_VEHICLE_ABOUT_TO_BLOW, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
}

// 0x0066E260 (:1489)
int flee_to_safety_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_CIVILIAN && Vehicle_ai_panic_drivers && human_in_vehicle_interaction(np, nullptr))
    {
        return 5900;
    }
    // rfg.exe also computes a closest point and reads the alert level here, without using either.
    const ai_belief* belief = ai_get_flee_to_safety_belief(np);
    return (belief != nullptr && belief->get_type() != AI_BELIEF_VEHICLE) ? 5900 : -1;
}

// 0x0066E320 (:1528)
bool flee_to_safety_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = ai_get_flee_to_safety_belief(np);
    if (belief == nullptr)
    {
        return Vehicle_ai_panic_drivers;
    }
    const_cast<npc*>(np)->get_ai()->maybe_cycle_flee_behavior();
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.attention_belief_obj_type = belief->get_obj_type();
    goal_setup_data.selected_target = World.object_from_handle(belief->handle);
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    goal_setup_data.attention_steering_mode = ATTENTION_STEERING_USE_CURRENT_MODE;
    return true;
}

// 0x005F3280 (:1557)
bool collect_nearby_EDF(object* obj, unsigned int, const void*)
{
    return static_cast<human*>(obj)->current_team == HUMAN_TEAM_EDF;
}

// 0x006ABA80 (:1565)
void flee_to_safety_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const ai_system* ai = np->get_ai();
    np->get_ai()->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
    goal_setup_data.set_attention(np, nullptr);
    np->npc_flags.ambient_flee = 1;
    if (!np->npc_flags.ambient_spawn || ai->planner.get_current_goal() == this || vec_dist_squared(np->pos, player_get_local_player()->pos) >= 400.0f)
    {
        return;
    }
    const ai_belief* belief = ai_get_flee_to_safety_belief(np);
    if (belief == nullptr || belief->get_type() != AI_BELIEF_COMBAT_SOUND)
    {
        return;
    }
    if (World.object_from_handle<player>(belief->handle) != nullptr)
    {
        // An EDF soldier in a box in front of the camera: the player shooting near them.
        unsigned int nearby;
        const vector bmin(-10.0f, -10.0f, 0.0f);
        const vector bmax(10.0f, 10.0f, 60.0f);
        if (World.collect_objects<npc>(bmin, bmax, Rfg_camera.real_pos, Rfg_camera.real_orient, &nearby, 1, collect_nearby_EDF, nullptr) > 0)
        {
            np->play_voice_line(VOICE_LINE_COMBAT_ALLY_FLEE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            return;
        }
    }
    else
    {
        const npc* shooter = World.object_from_handle<npc>(belief->handle);
        if (shooter == nullptr)
        {
            return;
        }
        if (shooter->current_team == HUMAN_TEAM_EDF)
        {
            np->play_voice_line(VOICE_LINE_COMBAT_ALLY_FLEE, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            return;
        }
        if (shooter->get_handle() == player_get_local_player()->get_handle())
        {
            return;
        }
    }
    np->play_voice_line(VOICE_LINE_PANIC, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
}

// 0x00688DF0 (:1626)
int flee_enemies_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const ai_perception& perception = np->get_ai()->perception;
    const ai_belief* belief = ai_get_enemy_flee_belief(np);
    if (belief == nullptr || belief->handle == 0xFFFFFFFF)
    {
        return -1;
    }
    const object* objp = World.object_from_handle(belief->handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return -1;
    }
    if (!np->npc_flags.bureaucrat)
    {
        const unsigned int handle = belief->handle;
        const ai_belief_specification specifications[4] = {
            ai_belief_specification(AI_BELIEF_EXPLOSION, handle, 5000, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_DAMAGE_TAKEN, handle, 5000, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_BULLET_IMPACT, handle, 5000, BELIEF_REQUIREMENTS_NONE),
            ai_belief_specification(AI_BELIEF_COMBAT_SOUND, handle, 5000, BELIEF_REQUIREMENTS_NONE),
        };
        ai_belief* attack = nullptr;
        if (perception.collect_all_matching_beliefs(&attack, 1, specifications, 4, 0xFFFFFFFF) < 1 || attack == nullptr)
        {
            // Nothing recent from them: only runs if they are close.
            float dist_squared = vec_dist_squared(np->pos, belief->pos);
            const float actual_dist_squared = vec_dist_squared(np->pos, objp->pos);
            const float flee_dist = perception.get_min_flee_dist(static_cast<const human*>(objp));
            if (0.0f <= dist_squared - actual_dist_squared)
            {
                dist_squared = actual_dist_squared;
            }
            if (flee_dist * flee_dist < dist_squared)
            {
                return -1;
            }
        }
    }
    return 5930;
}

// 0x00688FE0 (:1673)
bool flee_enemies_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* belief = ai_get_enemy_flee_belief(np);
    if (belief == nullptr)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = belief->get_type();
    goal_setup_data.attention_belief_obj_type = belief->get_obj_type();
    goal_setup_data.selected_target = World.object_from_handle(belief->handle);
    goal_setup_data.attention_mode = ATTENTION_USE_CURRENT_MODE;
    goal_setup_data.attention_steering_mode = ATTENTION_STEERING_USE_CURRENT_MODE;
    return true;
}

// 0x006ABC60 (:1690): flag 8 of the enemy-exists belief marks an enemy being fled from (BELIEF_REQUIREMENTS_FLEEING_FROM).
void flee_enemies_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_system* ai = np->get_ai();
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
    if (belief == nullptr)
    {
        return;
    }
    ai->set_base_anim_set(ANIM_SET_VARIANT_AFRAID, true, false);
    np->npc_flags.ambient_flee = 1;
    if (belief->get_type() != AI_BELIEF_ENEMY_EXISTS)
    {
        belief = ai->perception.get_belief(AI_BELIEF_ENEMY_EXISTS, belief->handle);
    }
    if (belief != nullptr)
    {
        belief->flags |= 8;
    }
}

// 0x0066E380 (:1714)
int cower_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team == HUMAN_TEAM_MARAUDER || np->hflags.activity_ha_hostage || human_in_vehicle(np) != nullptr)
    {
        return -1;
    }
    const ai_system* ai = np->get_ai();
    const ai_belief* hazard = ai->perception.get_first_belief(AI_BELIEF_HAZARD);
    if (hazard == nullptr || 10.0f < hazard->get_relevance())
    {
        return -1;
    }
    object* objp = World.object_from_handle(hazard->handle);
    if (objp == nullptr)
    {
        return -1;
    }

    const projectile* pp = World.object_cast<projectile>(objp);
    const player* squad_player = ai->is_in_player_squad(true);
    if (squad_player != nullptr && pp != nullptr && pp->owner_handle == squad_player->get_handle())
    {
        return -1;
    }
    // A projectile only counts here if it explodes.
    if (pp != nullptr && (!pp->w_info->is_explosive() || ai->projectile_reaction == AI_PROJECTILE_REACTION_NONE))
    {
        return -1;
    }

    const vehicle* vp = objp->get_obj_type() == OT_VEHICLE ? static_cast<const vehicle*>(objp) : nullptr;
    const player* local = player_get_local_player();
    if (vp != nullptr && local != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == local && local_player_is_friendly(np))
    {
        return -1;
    }
    if (pp != nullptr || vp != nullptr)
    {
        float radius = 0.0f;
        if (vp != nullptr)
        {
            vector bmin, bmax;
            vp->get_local_bbox(bmin, bmax);
            radius = (bmax.x - bmin.x) * 0.5f;
        }
        else if (pp->w_info->is_explosive())
        {
            radius = pp->w_info->m_explosion_info->m_secondary_radius;
        }
        hazard_physics_info info;
        get_hazard_physics_info(info, np, objp);
        if (!info.will_impact)
        {
            return -1;
        }
        if (radius + 0.5f < info.distance_at_pass && (vp == nullptr || !ai_perception::vehicle_is_heavy(vp) || !ai->persona_flags.heavy_vehicle_always_cower))
        {
            return -1;
        }
    }
    return 4000;
}

// 0x005F32A0 (:1801)
bool cower_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* hazard = np->get_ai()->perception.get_first_belief(AI_BELIEF_HAZARD);
    if (hazard == nullptr || 10.0f < hazard->get_relevance())
    {
        return false;
    }
    object* objp = World.object_from_handle(hazard->handle);
    if (objp == nullptr)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = AI_BELIEF_HAZARD;
    goal_setup_data.selected_target = objp;
    goal_setup_data.attention_belief_obj_type = hazard->get_obj_type();
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_NONE;
    goal_setup_data.attention_steering_mode = ATTENTION_STEERING_PLANT_FEET;
    return true;
}

// 0x006ABD30 (:1825)
void cower_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
    human_stop_playing_turn(np, 0, false);
}

// 0x005F3310 (:1840): never chosen; rfg.exe's body is only the failure.
int surrender_goal::get_fitness(const npc*, ai_plan_conditions) const
{
    return -1;
}

// 0x006433E0 (:1873)
bool surrender_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    const ai_belief* enemy = np->get_ai()->perception.get_first_belief(AI_BELIEF_ENEMY_POSITION);
    if (enemy == nullptr)
    {
        return false;
    }
    goal_setup_data.attention_belief_type = enemy->get_type();
    goal_setup_data.attention_belief_obj_type = enemy->get_obj_type();
    object* objp = enemy->handle != 0xFFFFFFFF ? World.object_from_handle(enemy->handle) : nullptr;
    goal_setup_data.selected_target = (objp != nullptr && objp->get_obj_type() == OT_HUMAN) ? objp : nullptr;
    goal_setup_data.attention_mode = ATTENTION_LOOK_AT_OBJECT;
    return true;
}

// 0x006ABD60 (:1891)
void surrender_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    ai_belief* belief = nullptr;
    goal_setup_data.set_attention(np, &belief);
}

// 0x005F3320 (:1903)
int go_into_safehouse_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr)
    {
        float speed;
        vehicle_get_speed(vp, speed);
        if (0.1f < speed)
        {
            return -1;
        }
    }
    if (np->npc_flags.run_to_safehouse)
    {
        return 30000;
    }
    if (np->hflags.activity_ha_hostage && np->npc_flags.activity_ha)
    {
        return safehouse_contains_player() ? 30000 : -1;
    }
    return -1;
}

// 0x005F33A0 (:1934)
bool go_into_safehouse_goal::store_setup_data(ai_goal_setup_data&, const npc*) const
{
    return true;
}

// 0x00643440 (:1940)
void go_into_safehouse_goal::commit_setup_data(npc* np, const ai_goal_setup_data&) const
{
    player* local = player_get_local_player();
    object_squad* escort = local->escort_squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(local->escort_squad_handle) : nullptr;
    if (escort != nullptr && escort->is_member(np))
    {
        for (int i = 0; i < escort->member_array.size(); i++)
        {
            const unsigned int handle = escort->member_array[i];
            npc* member = handle != 0xFFFFFFFF ? World.object_from_handle<npc>(handle) : nullptr;
            if (member != nullptr && member != np)
            {
                member->get_ai()->planner.request_replan();
            }
        }
        local->escort_squad_remove(np, false);
    }
    np->npc_flags.run_to_safehouse = 1;
}

// 0x005F33B0 (:1965)
int fetal_position_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    return np->hflags.in_fetal_position ? 110000 : -1;
}

// Flags 0x20000 and 0x40000 of a projectile belief.
static const unsigned int PROJECTILE_BELIEF_THREAT_FLAGS = 0x60000;

// 0x00643500 (:1977)
int fall_back_to_player_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (human_teams_get_disposition(np, HUMAN_TEAM_GUERILLA) != TEAM_DISPOSITION_ALLIED || human_in_vehicle(np) != nullptr || human_in_vehicle_transition(np, nullptr, VI_INTERACTION_ENTER))
    {
        return -1;
    }
    const ai_perception& perception = np->get_ai()->perception;
    if (human_weapons_get_weapon(np) == nullptr || perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) == nullptr)
    {
        return -1;
    }
    vector center;
    np->get_center_pos(center);
    const ai_belief* head = perception.get_first_belief(AI_BELIEF_PROJECTILE);
    for (const ai_belief* belief = head; belief != nullptr;)
    {
        if ((belief->flags & PROJECTILE_BELIEF_THREAT_FLAGS) == PROJECTILE_BELIEF_THREAT_FLAGS)
        {
            const projectile* pp = World.object_from_handle<projectile>(belief->handle);
            if (pp != nullptr)
            {
                const float radius = pp->w_info->m_explosion_info->m_knockdown_radius;
                if (vec_dist_squared(center, pp->pos) <= radius * radius && World.object_from_handle<player>(pp->owner_handle) != nullptr)
                {
                    return 4450;
                }
            }
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return -1;
}

// 0x00643630 (:2016)
bool fall_back_to_player_goal::store_setup_data(ai_goal_setup_data& goal_setup_data, const npc* np) const
{
    vector center;
    np->get_center_pos(center);
    const ai_belief* head = np->get_ai()->perception.get_first_belief(AI_BELIEF_PROJECTILE);
    for (const ai_belief* belief = head; belief != nullptr;)
    {
        if ((belief->flags & PROJECTILE_BELIEF_THREAT_FLAGS) == PROJECTILE_BELIEF_THREAT_FLAGS && (belief->flags & 0x40) == 0)
        {
            const projectile* pp = projectile_from_handle(belief->handle);
            if (pp != nullptr)
            {
                const float radius = pp->w_info->m_explosion_info->m_knockdown_radius;
                if (vec_dist_squared(center, pp->pos) <= radius * radius && pp->owner_handle != 0xFFFFFFFF)
                {
                    object* owner = World.object_from_handle(pp->owner_handle);
                    if (owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
                    {
                        goal_setup_data.selected_leader = owner;
                        return true;
                    }
                }
            }
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return false;
}

// 0x00643740 (:2047)
void fall_back_to_player_goal::commit_setup_data(npc* np, const ai_goal_setup_data& goal_setup_data) const
{
    const object* leader = goal_setup_data.selected_leader;
    if (leader != nullptr && leader->get_obj_type() == OT_HUMAN && leader->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        np->get_ai()->desired_follow_target = leader->get_handle();
    }
}

// 0x005F33D0 (:2058)
int high_alert_curfew_goal::get_fitness(const npc* np, ai_plan_conditions) const
{
    if (np->current_team != HUMAN_TEAM_CIVILIAN)
    {
        return -1;
    }
    return gsm_get_alert_level() > 1 ? 5000 : -1;
}

// The singletons, in rfg.exe's construction order: it fills goal_pool, right after goals.cpp's.
react_attached_projectile_goal react_attached_projectile_goal::the_instance; // 0x01DC9DFC

// 0x00609940
react_attached_projectile_goal::react_attached_projectile_goal()
    : ai_goal(ai_plan_conditions(114), "attached projectile")
{
}

ai_goal* react_attached_projectile_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE100
react_attached_projectile_goal::~react_attached_projectile_goal()
{
}

react_on_fire_goal react_on_fire_goal::the_instance; // 0x01D75558

// 0x006099E0
react_on_fire_goal::react_on_fire_goal()
    : ai_goal(ai_plan_conditions(115), "on fire")
{
}

ai_goal* react_on_fire_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE110
react_on_fire_goal::~react_on_fire_goal()
{
}

evade_projectile_goal evade_projectile_goal::the_instance; // 0x01D684E0

// 0x00609A80
evade_projectile_goal::evade_projectile_goal()
    : ai_goal(ai_plan_conditions(116), "evade projectile")
{
}

ai_goal* evade_projectile_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE120
evade_projectile_goal::~evade_projectile_goal()
{
}

dodge_projectile_goal dodge_projectile_goal::the_instance; // 0x01D68024

// 0x00609B20
dodge_projectile_goal::dodge_projectile_goal()
    : ai_goal(ai_plan_conditions(117), "dodge projectile")
{
}

ai_goal* dodge_projectile_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE140
dodge_projectile_goal::~dodge_projectile_goal()
{
}

evade_thrown_weapon_goal evade_thrown_weapon_goal::the_instance; // 0x01D71F1C

// 0x00609BC0
evade_thrown_weapon_goal::evade_thrown_weapon_goal()
    : ai_goal(ai_plan_conditions(116), "evade thrown weapon")
{
}

ai_goal* evade_thrown_weapon_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE150
evade_thrown_weapon_goal::~evade_thrown_weapon_goal()
{
}

dodge_thrown_weapon_goal dodge_thrown_weapon_goal::the_instance; // 0x01D75268

// 0x00609C60
dodge_thrown_weapon_goal::dodge_thrown_weapon_goal()
    : ai_goal(ai_plan_conditions(117), "dodge thrown weapon")
{
}

ai_goal* dodge_thrown_weapon_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE160
dodge_thrown_weapon_goal::~dodge_thrown_weapon_goal()
{
}

evade_vehicle_goal evade_vehicle_goal::the_instance; // 0x01DC9A54

// 0x00609D00
evade_vehicle_goal::evade_vehicle_goal()
    : ai_goal(ai_plan_conditions(118), "evade vehicle")
{
}

ai_goal* evade_vehicle_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE170
evade_vehicle_goal::~evade_vehicle_goal()
{
}

cower_goal cower_goal::the_instance; // 0x01DC9664

// 0x00609DA0
cower_goal::cower_goal()
    : ai_goal(ai_plan_conditions(124), "cower")
{
}

ai_goal* cower_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE180
cower_goal::~cower_goal()
{
}

flee_debris_goal flee_debris_goal::the_instance; // 0x01D71F88

// 0x00609E40
flee_debris_goal::flee_debris_goal()
    : ai_goal(ai_plan_conditions(119), "flee debris")
{
}

// 0x005DE1A0
flee_debris_goal::~flee_debris_goal()
{
}

surrender_goal surrender_goal::the_instance; // 0x01DDB5C4

// 0x00609EE0
surrender_goal::surrender_goal()
    : ai_goal(ai_plan_conditions(125), "surrender")
{
}

ai_goal* surrender_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE1B0
surrender_goal::~surrender_goal()
{
}

flee_unsafe_building_goal flee_unsafe_building_goal::the_instance; // 0x01DC9E4C

// 0x00609F80
flee_unsafe_building_goal::flee_unsafe_building_goal()
    : ai_goal(ai_plan_conditions(122), "flee unsafe building")
{
}

// 0x005DE1D0
flee_unsafe_building_goal::~flee_unsafe_building_goal()
{
}

leave_demolition_area_goal leave_demolition_area_goal::the_instance; // 0x01DDB294

// 0x0060A020
leave_demolition_area_goal::leave_demolition_area_goal()
    : ai_goal(ai_plan_conditions(126), "leave demolition area")
{
}

ai_goal* leave_demolition_area_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE1E0
leave_demolition_area_goal::~leave_demolition_area_goal()
{
}

go_into_safehouse_goal go_into_safehouse_goal::the_instance; // 0x01642F94

// 0x0060A0C0
go_into_safehouse_goal::go_into_safehouse_goal()
    : ai_goal(ai_plan_conditions(151), "go into safehouse")
{
}

ai_goal* go_into_safehouse_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE200
go_into_safehouse_goal::~go_into_safehouse_goal()
{
}

fetal_position_goal fetal_position_goal::the_instance; // 0x01D68320

// 0x0060A160
fetal_position_goal::fetal_position_goal()
    : ai_goal(ai_plan_conditions(10), "fetal position")
{
}

ai_goal* fetal_position_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE210
fetal_position_goal::~fetal_position_goal()
{
}

fall_back_to_player_goal fall_back_to_player_goal::the_instance; // 0x01D9F750

// 0x0060A2A0
fall_back_to_player_goal::fall_back_to_player_goal()
    : ai_goal(ai_plan_conditions(34), "fall back to player")
{
}

ai_goal* fall_back_to_player_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE240
fall_back_to_player_goal::~fall_back_to_player_goal()
{
}

flee_enemies_goal flee_enemies_goal::the_instance; // 0x01DA61C0

// 0x0060A340
flee_enemies_goal::flee_enemies_goal()
    : ai_goal(ai_plan_conditions(120), "flee enemies")
{
}

// 0x005DE260
flee_enemies_goal::~flee_enemies_goal()
{
}

flee_to_safety_goal flee_to_safety_goal::the_instance; // 0x01D71F6C

// 0x0060A3E0
flee_to_safety_goal::flee_to_safety_goal()
    : ai_goal(ai_plan_conditions(120), "flee to safety")
{
}

// 0x005DE280
flee_to_safety_goal::~flee_to_safety_goal()
{
}

flee_from_vehicle_goal flee_from_vehicle_goal::the_instance; // 0x01DCA7F8

// 0x0060A480
flee_from_vehicle_goal::flee_from_vehicle_goal()
    : ai_goal(ai_plan_conditions(120), "flee from vehicle")
{
}

// 0x005DE2A0
flee_from_vehicle_goal::~flee_from_vehicle_goal()
{
}

high_alert_curfew_goal high_alert_curfew_goal::the_instance; // 0x01DC96E8

// 0x0060A200
high_alert_curfew_goal::high_alert_curfew_goal()
    : ai_goal(ai_plan_conditions(109), "high-alert curfew")
{
}

ai_goal* high_alert_curfew_goal::get_instance()
{
    return &the_instance;
}

// 0x005DE230
high_alert_curfew_goal::~high_alert_curfew_goal()
{
}

