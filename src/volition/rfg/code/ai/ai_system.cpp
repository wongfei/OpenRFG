// Original: volition/rfg/code/ai/ai_system.cpp
//
// The AI of one npc: frames, stun state, movement requests, guard posts, formations, covering fire, reactions and
// orders (stub_impl.md S9). Linked by OpenRFG only; the test programs keep their stand-ins.

#include "volition/rfg/code/ai/ai_system.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/rfg/code/ai/ai_defense_coordination_internal.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/civilian/ambient_common.h"
#include "volition/rfg/code/ai/civilian/pedestrian_actions.h"
#include "volition/rfg/code/ai/custom_actions.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/player_attack_limiter.h"
#include "volition/rfg/code/ai/search_system.h"
#include "volition/rfg/code/ai/search_system_internal.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
static const float PI = 3.141593f; // 0x012C8170
static const float COS_45_DEGREES = 0.70710677f;
static const float STRICT_MOVING_TO_XZ_TOLERANCE_SQ = 0.01f; // 0x012C83A4
static const float AI_SLOWDOWN_PLAYER_DIST_SQ = 1600.0f; // 0x012C839C, 40 m
static const float AI_SLOWDOWN_VEHICLE_PLAYER_DIST_SQ = 6400.0f; // 0x012C83A0, 80 m
static const float MIN_VEH_ACTIVE_DESPAWN_DIST_SQ = 10000.0f; // 0x012C83C4
static const float MIN_ACTIVE_DESPAWN_DIST_SQ = 2500.0f; // 0x012C9438
static const float AI_LOW_CONTROL_CUTOFF = 0.333333f; // 0x012C83D4
static const float AI_HIGH_CONTROL_CUTOFF = 0.666667f; // 0x012C83D8
static const float AI_LOW_MORALE_CUTOFF = 0.2f; // read from get_district_morale_level
static const float AI_HIGH_MORALE_CUTOFF = 0.5f; // read from get_district_morale_level
static const int AI_ANIM_SET_VARIANT_PRIORITIES[12] = {500, 200, 200, 200, 200, 300, 10, 100, 10, 10, 10, -1}; // 0x012C83DC
static const float NEAR_ROAD_DISTANCE = 60.0f; // 0x012C9054
static const float MIN_TELEPORT_DIST_SQRD = 400.0f; // 0x012C9508

// Magic numbers read from the functions below.
static const int COVERING_FIRE_VALID_MS = 3000;
static const unsigned int PEDESTRIAN_START_PAUSED_PERCENT = 60; // vrand() % 100 <= 60: about 61% start paused
static const float PED_NODE_NEAR_RADIUS = 15.0f;
static const int FETAL_POSITION_MS = 10000;
static const float NEAR_ORDERS_RADIUS = 20.0f;
static const float NEAR_ORDERS_PLAYER_RADIUS = 5.0f;
static const float AT_GUARD_POST_Y_TOLERANCE = 1.5f;
static const signed char NO_GUARD_DIRECTION = -128;
static const int GUARD_FACING_COMPRESSED_COSINE = 15; // index for get_compressed_cosine_int
static const float TELEPORT_MIN_PLAYER_DIST_SQ = 3600.0f;
static const float REINFORCEMENT_REPEAT_DIST_SQ = 1600.0f;
static const float ROAD_ROUTE_VALID_DIST_SQ = 225.0f;
static const char* const CYCLE_FLEE_MISSION_NAME = "sniper hunter"; // the only mission whose npcs cycle fleeing and hiding
static const float UNSAFE_BUILDING_STANDING_DIST = 0.1f;
static const float UNSAFE_BUILDING_DESTROYED_DIST = 15.0f;
static const float PF_MIN_DIST = 0.3f; // 0x01310F28, closer than this needs no path
static const float DEFEND_HUMAN_RADIUS = 7.0f;
static const float DEFEND_OBJECT_RADIUS = 10.0f;
static const unsigned int ON_FIRE_PLAN_CONDITION_BIT = 115; // data[3] & 0x80000 in update_on_fire
static const unsigned int STUN_ANIM_FLAGS = 0x40;
static const unsigned int EXIT_ON_FIRE_ANIM_FLAGS = 0x4040;
// Action aliases and groups passed by number in rfg.exe.
static const int STUN_STOP_ACTION_MASK = 0x3F3; // ANIMATION_ALIAS_ACTIONS group stopped when stunned
static const int STUN_FLINCH_ALIAS = 0x3D9;
static const int KNOCKDOWN_ALIAS = 0x3F4;
static const int PF_BLOCKING_ALIAS_0 = 0x3E0;
static const int PF_BLOCKING_ALIAS_1 = 0x3E1;
static const int PF_SPRINT_STOP_ALIAS = 0x3F5;
static const int STOP_MOVING_ALIAS = 0x407;
static const int SHOVE_ALIAS = 0x298;

const ai_tension_level_info Ai_tension_level_info_table[NUM_AI_TENSION_LEVELS] = // 0x012C8330
{
    { -5000, 200, 1500, { 5, 20, 75 }, { 5, 75, 20 } },
    { 10000, 50, 800, { 15, 30, 55 }, { 10, 75, 15 } },
    { 60000, 50, 300, { 30, 40, 30 }, { 20, 70, 10 } },
};

static const int disallowed_animations[6] = { 572, 985, 992, 994, 995, 1012 }; // 0x012C926C, action aliases

// Tweak table entries, all zero until tweak_table.xtbl loads.
int Ai_fame_num_missions_completed; // 0x01D67740
static int Ai_no_fall_back_chance; // 0x01D6772C
static int Ai_heavy_vehicle_always_cower_chance; // 0x01D67D98
static int Ai_flee_to_defensive_cover_chance; // 0x01D67198
static int Ai_flee_to_safe_building_chance; // 0x01D6595C
static int Ai_remote_charge_panic_chance; // 0x01D65984
int Marauder_follow_player_chance; // 0x01D6594C
static int Civilian_stand_up_to_edf_chance; // 0x01D65964
static int Civilian_freeze_cower_chance; // 0x01D65958
static int Civilian_always_flee_edf_chance; // 0x01D65734
static int Ai_edf_on_fire_panic_chance; // 0x01D65954
static int Ai_gue_on_fire_panic_chance; // 0x01D65200
static int Ai_civ_on_fire_panic_chance; // 0x01D6521C
static int Ai_mar_on_fire_panic_chance;
static int Ai_default_on_fire_panic_chance; // 0x01D6596C
static int Ai_cycle_flee_behavior_flee_time; // 0x01D67DA4
static int Ai_cycle_flee_behavior_hide_time; // 0x01D67754
static float Civilian_max_wounded_hp_pct; // 0x01D65728

static tweak_table_item Tweak_table_item_Ai_fame_num_missions_completed("Ai_fame_num_missions_completed", &Ai_fame_num_missions_completed); // (0x010619F0)
static tweak_table_item Tweak_table_item_Ai_no_fall_back_chance("Ai_no_fall_back_chance", &Ai_no_fall_back_chance); // (0x01061A10)
static tweak_table_item Tweak_table_item_Ai_heavy_vehicle_always_cower_chance("Ai_heavy_vehicle_always_cower_chance", &Ai_heavy_vehicle_always_cower_chance); // (0x01061A30)
static tweak_table_item Tweak_table_item_Ai_flee_to_defensive_cover_chance("Ai_flee_to_defensive_cover_chance", &Ai_flee_to_defensive_cover_chance); // (0x01061A50)
static tweak_table_item Tweak_table_item_Ai_flee_to_safe_building_chance("Ai_flee_to_safe_building_chance", &Ai_flee_to_safe_building_chance); // (0x01061A70)
static tweak_table_item Tweak_table_item_Ai_remote_charge_panic_chance("Ai_remote_charge_panic_chance", &Ai_remote_charge_panic_chance); // (0x01061A90)
static tweak_table_item Tweak_table_item_Marauder_follow_player_chance("Marauder_follow_player_chance", &Marauder_follow_player_chance); // (0x01061AB0)
static tweak_table_item Tweak_table_item_Civilian_stand_up_to_edf_chance("Civilian_stand_up_to_edf_chance", &Civilian_stand_up_to_edf_chance); // (0x01061AD0)
static tweak_table_item Tweak_table_item_Civilian_freeze_cower_chance("Civilian_freeze_cower_chance", &Civilian_freeze_cower_chance); // (0x01061AF0)
static tweak_table_item Tweak_table_item_Civilian_always_flee_edf_chance("Civilian_always_flee_edf_chance", &Civilian_always_flee_edf_chance); // (0x01061B10)
static tweak_table_item Tweak_table_item_Ai_edf_on_fire_panic_chance("Ai_edf_on_fire_panic_chance", &Ai_edf_on_fire_panic_chance); // (0x01061B30)
static tweak_table_item Tweak_table_item_Ai_gue_on_fire_panic_chance("Ai_gue_on_fire_panic_chance", &Ai_gue_on_fire_panic_chance); // (0x01061B50)
static tweak_table_item Tweak_table_item_Ai_civ_on_fire_panic_chance("Ai_civ_on_fire_panic_chance", &Ai_civ_on_fire_panic_chance); // (0x01061B70)
static tweak_table_item Tweak_table_item_Ai_mar_on_fire_panic_chance("Ai_mar_on_fire_panic_chance", &Ai_mar_on_fire_panic_chance); // (0x01061B90)
static tweak_table_item Tweak_table_item_Ai_default_on_fire_panic_chance("Ai_default_on_fire_panic_chance", &Ai_default_on_fire_panic_chance); // (0x01061BB0)
static tweak_table_item Tweak_table_item_Ai_cycle_flee_behavior_flee_time("Ai_cycle_flee_behavior_flee_time", &Ai_cycle_flee_behavior_flee_time); // (0x01061BD0)
static tweak_table_item Tweak_table_item_Ai_cycle_flee_behavior_hide_time("Ai_cycle_flee_behavior_hide_time", &Ai_cycle_flee_behavior_hide_time); // (0x01061BF0)
static tweak_table_item Tweak_table_item_Civilian_max_wounded_hp_pct("Civilian_max_wounded_hp_pct", &Civilian_max_wounded_hp_pct); // (0x01061C10)

static bool Alert_level_set_to_green; // 0x01D67DB5
static object_pool<ai_system,100,1> Ai_system_list; // 0x01D75A40

// ---- internal functions

// A handle's squad, or null. rfg.exe inlines this in almost every function below.
static object_squad* squad_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_SQUAD)
    {
        return nullptr;
    }
    return (object_squad*)objp;
}

// The npc a handle names, or null.
static npc* npc_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return nullptr;
    }
    return (npc*)objp;
}

// The object a handle names if it has this type, or null.
static object* typed_object_from_handle(unsigned int handle, object_type type)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != type)
    {
        return nullptr;
    }
    return objp;
}

// 0x005DE2C0 (:398)
void ai_system::ai_covering_fire_info::clear()
{
    valid_time.set(0);
    ally_handle = 0xFFFFFFFF;
}

// 0x005DE2E0 (:404)
void ai_system::ai_covering_fire_info::set(const npc* ally)
{
    ally_handle = ally->get_handle();
    valid_time.set(COVERING_FIRE_VALID_MS);
}

// 0x0060A530 (:411)
npc* ai_system::ai_covering_fire_info::get() const
{
    if (valid_time.elapsed())
    {
        return nullptr;
    }
    return npc_from_handle(ally_handle);
}

// 0x005F9CA0
ai_system::ai_covering_fire_info::ai_covering_fire_info()
{
}

// 0x005DE300 (:425)
ai_system::can_play_stun_results ai_system::can_play_stun_animation() const
{
    if (np->npc_flags.tied_up != 0 || np->hflags.activity_ha_hostage != 0)
    {
        return CAN_PLAY_STUN_NO;
    }
    const ai_scripting_behaviors behavior = np->get_ai()->custom_behavior;
    if (behavior == AI_SB_TRAPPED_IN_BUILDING || behavior == AI_SB_TRAPPED_IN_JAIL)
    {
        return CAN_PLAY_STUN_NO;
    }
    if (human_in_vehicle(np) != nullptr)
    {
        return CAN_PLAY_STUN_NO;
    }
    if (master_handle != 0xFFFFFFFF || human_in_vehicle_interaction(np, nullptr) || human_is_ragdolled(np))
    {
        return CAN_PLAY_STUN_RAGDOLL_ONLY;
    }
    if ((np->cur_stance != HST_STAND && np->cur_stance != HST_CROUCH) || human_combat_is_manning_turret(np))
    {
        return CAN_PLAY_STUN_RAGDOLL_ONLY;
    }
    for (int i = 0; i < (int)(sizeof(disallowed_animations) / sizeof(disallowed_animations[0])); ++i)
    {
        if (np->char_inst->is_playing_action(disallowed_animations[i]))
        {
            return CAN_PLAY_STUN_RAGDOLL_ONLY;
        }
    }
    return np->cur_state == HS_COVER ? CAN_PLAY_STUN_RAGDOLL_ONLY : CAN_PLAY_STUN_YES;
}

// 0x0064DE90 (:509)
void ai_system::update_stunned_state()
{
    const bool ragdolled = human_is_ragdolled_or_getting_up(np);
    const bool timer_elapsed = stunned_timer.elapsed();
    // enter_stunned_state stores an animation state, stun an action alias, whose indices start above the states.
    const bool stunned_by_state = stunned_animation >= 0 && stunned_animation <= ANIMATION_STATE_OLD_COOT_SIT;
    const bool playing_stun_anim = stunned_animation != -1 && np->char_inst->is_playing_action(stunned_animation);

    bool has_master = master_handle != 0xFFFFFFFF;
    if (has_master)
    {
        object* master = World.object_from_handle(master_handle);
        if (master == nullptr || master->get_obj_type() != OT_HUMAN || ragdolled)
        {
            has_master = false;
            master_handle = 0xFFFFFFFF;
            human_override_default_animation_state(np, false);
        }
    }

    if (!flags.stunned)
    {
        if (ragdolled || has_master)
        {
            np->char_inst->stop_action(STUN_STOP_ACTION_MASK, 3, false);
            flags.stunned = true;
            planner.stun();
            flags.stunned = true;
        }
        if (!flags.stunned)
        {
            return;
        }
    }

    if (human_weapons_is_firearm_equipped(np) && perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr)
    {
        human_set_combat_ready(np, true, 0);
    }

    if (!stunned_by_state)
    {
        const bool playing_flinch = np->char_inst->is_playing_action(STUN_FLINCH_ALIAS);
        if (has_master || ragdolled || playing_stun_anim || !timer_elapsed || playing_flinch)
        {
            return;
        }
        flags.stunned = false;
        stunned_animation = -1;
        return;
    }

    if (!ragdolled && !timer_elapsed)
    {
        return;
    }
    stunned_animation = -1;
    flags.stunned = ragdolled || has_master;
    if (np->hflags.in_fetal_position != 0)
    {
        return;
    }
    human_override_default_animation_state(np, false);
}

// 0x0068EEE0 (:587)
void ai_system::do_death_func()
{
    if (np->hflags.death_func_done != 0)
    {
        return;
    }
    stop_moving();
    np->stop_voice(VOICE_LINE_PRIORITY_HIGH);
    object_squad* squad = squad_from_handle(squad_handle);
    if (squad != nullptr)
    {
        squad->report_member_death(np);
        squad_handle = 0xFFFFFFFF;
    }
    perception.clear();
    planner.clear();
    reset_common();
    if (np->npc_flags.remove_icon_on_death != 0)
    {
        Cartographer.remove_icon(np->get_handle());
        np->npc_flags.remove_icon_on_death = 0;
    }
    np->hflags.death_func_done = 1;
}

// 0x0060A570 (:632)
bool ai_system::can_despawn(bool* offscreen) const
{
    if (!np->should_despawn())
    {
        return false;
    }
    bool local_offscreen;
    if (offscreen == nullptr)
    {
        offscreen = &local_offscreen;
    }

    vehicle* vp = human_in_vehicle(np);
    if (vp == nullptr)
    {
        *offscreen = np->is_offscreen();
    }
    else
    {
        if (!human_is_driving(np))
        {
            return false;
        }
        const int num_seats = vehicle_get_num_seats(vp);
        for (int seat = VSI_PASSENGER_1; seat < num_seats; ++seat)
        {
            npc* passenger = World.object_cast<npc>(vehicle_passenger_in_seat(vp, (vehicle_seat_index)seat));
            if (passenger != nullptr && !passenger->should_despawn() && passenger->npc_flags.ambient_spawn == 0)
            {
                return false;
            }
        }
        *offscreen = vp->is_offscreen();
    }

    if (np->despawn_overdue())
    {
        return true;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return true;
    }
    float min_dist_sq;
    if (vp == nullptr)
    {
        min_dist_sq = *offscreen ? MIN_TELEPORT_DIST_SQRD : MIN_ACTIVE_DESPAWN_DIST_SQ;
    }
    else
    {
        min_dist_sq = *offscreen ? MIN_TELEPORT_DIST_SQRD : MIN_VEH_ACTIVE_DESPAWN_DIST_SQ;
    }
    return vec_dist_squared(pp->pos, np->pos) > min_dist_sq;
}

// 0x0060A6F0 (:714)
void ai_system::despawn_me(bool fade)
{
    // 2000 ms when it is offscreen and can take its time, otherwise 500 ms.
    const int fade_time = fade ? 2000 : 500;
    if (human_is_driving(np))
    {
        vehicle* vp = (vehicle*)typed_object_from_handle(np->vehicle_handle, OT_VEHICLE);
        if (vp != nullptr)
        {
            vehicle_fade_out(vp, fade_time, true, false);
        }
    }
    else
    {
        np->fade_out_and_despawn(fade_time, false);
    }
}

// ---- ai_pedestrian_info

// 0x0064CA70 (.h:118)
ai_pedestrian_info::ai_pedestrian_info()
{
    reset();
}

// 0x005F9C70
ai_pedestrian_info::~ai_pedestrian_info()
{
}

// 0x0060A770 (:738)
void ai_pedestrian_info::reset()
{
    ambient_waypoints.clear();
    old_waypoints.clear();
    animation_cooldown.set(0);
    pause_timer.set(0);
    paused = vrand() % 100 <= PEDESTRIAN_START_PAUSED_PERCENT;
    npc_greeting_timeout.set(0);
    greeting_npc_handle = 0xFFFFFFFF;
}

// 0x0060A7C0 (:752)
bool ai_pedestrian_info::near_ped_nodes(const vector& pos) const
{
    if (ambient_waypoints.size() < 2)
    {
        return false;
    }
    const object* first = typed_object_from_handle(ambient_waypoints[0], OT_NAVPOINT);
    const object* second = typed_object_from_handle(ambient_waypoints[1], OT_NAVPOINT);
    if (first == nullptr || second == nullptr)
    {
        return false;
    }
    vector intp;
    return ix_linesegment_sphere(intp, first->pos, second->pos, pos, PED_NODE_NEAR_RADIUS);
}

// 0x0060A860 (:777)
const navpoint* ai_pedestrian_info::get_next_navpoint() const
{
    if (ambient_waypoints.size() < 1)
    {
        return nullptr;
    }
    return (navpoint*)typed_object_from_handle(ambient_waypoints[0], OT_NAVPOINT);
}

// 0x0060A890 (:788)
const navpoint* ai_pedestrian_info::get_last_navpoint() const
{
    const int num = old_waypoints.size();
    if (num < 1)
    {
        return nullptr;
    }
    return (navpoint*)typed_object_from_handle(old_waypoints[num - 1], OT_NAVPOINT);
}

// 0x0064E070 (:800)
void ai_pedestrian_info::remove_waypoint(unsigned int last_waypoint_handle)
{
    if (ambient_waypoints.size() < 1 || last_waypoint_handle != ambient_waypoints[0])
    {
        return;
    }
    ambient_waypoints.remove(0);
    if (old_waypoints.size() >= old_waypoints.capacity())
    {
        old_waypoints.remove(0);
    }
    old_waypoints.add(last_waypoint_handle);
}

// 0x0064E0F0 (:819)
bool ai_pedestrian_info::is_old_waypoint(unsigned int nav_handle) const
{
    return old_waypoints.find(nav_handle, false) > -1;
}

// 0x00673E40 (:825)
void ai_pedestrian_info::add_new_waypoints(const farray<unsigned int,8>& new_waypoints)
{
    ambient_waypoints.copy(new_waypoints);
}

// ---- ai_reaction_info

// 0x005F9C90
ai_reaction_info::ai_reaction_info()
{
}

// 0x005DE3F0 (:907)
void ai_reaction_info::reset()
{
    time_left.set(0);
    target_handle = 0xFFFFFFFF;
    belief_type = AI_BELIEF_UNDEFINED;
    reason = 0;
}

// ---- the system's frame

// 0x0060A8C0 (:920)
void ai_system_init()
{
    ai_belief_system_init();
    ai_planning_system_init();
}

// 0x006BB710 (:926)
void ai_system_level_init_pre()
{
    ai_rate_limiters_level_init_pre();
    Ai_search_object_manager.level_init();
    Ai_defense_object_manager.level_init();
    ai_formation_level_init_pre();
    player_attack_limiter_level_init_pre();
    navpoint_level_init_pre();
}

// 0x005DE420 (:942)
void ai_system_mission_do_frame()
{
}

// 0x006BB780 (:966)
void ai_system_do_frame()
{
    ai_rate_limiters_do_frame();
    Ai_search_object_manager.do_frame();
    ai_defense_coordination_do_frame();
    ai_formation_do_frame();
    ai_perception::process_movers();
    ai_perception::process_thrown_objects();
}

// 0x00672F70
ai_system::ai_system()
{
}

// 0x00673E50 (:993)
void ai_system::init(npc* np_)
{
    np = np_;
    perception.init(np_);
    perception.clear();
    planner.init(np);
    reset_common();
    create_persona();
    np->pf.extended_stuck_cumulative = 0;
    np->pf.extended_stuck_timer.invalidate();
}

// 0x00673EC0 (:1007)
void ai_system::reset()
{
    perception.clear();
    planner.clear();
    reset_common();
}

// 0x00A05F70 (.h:281)
void ai_system::free()
{
    perception.clear();
    planner.clear();
    reset_common();
}

// 0x0064E120 (:1017)
void ai_system::reset_common()
{
    pathfind_timer.set(0);
    evaluate_cover_timer.set(0);
    in_cover_timer.invalidate();
    out_of_cover_timer.invalidate();
    not_using_cover_timer.invalidate();
    bomb_timer.invalidate();
    stance_change_timer.set(0);
    reaction_info.reset();
    exhaustion_timer.set(0);
    salute_timer.set_rand(3000, 8000); // ms
    pedestrian_info.reset();
    road_route.clear();
    last_road_route.clear();
    amb_action_info.action_type = AMBIENT_PED_ACTION_TYPE_NONE;
    amb_action_info.obj_target_handle = 0xFFFFFFFF;
    amb_action_info.vehicle_target_handle = 0xFFFFFFFF;
    amb_action_info.destination_pos.zero();
    amb_action_info.npc_targets.clear();
    amb_action_info.action_timeout.invalidate();
    amb_action_info.despawn_timeout.invalidate();
    custom_behavior = AI_SB_NONE;
    custom_behavior_value = -1;
    custom_behavior_target = 0xFFFFFFFF;
    stunned_animation = -1;
    master_handle = 0xFFFFFFFF;
    stunned_timer.set(0);
    flee_destination_handle = 0xFFFFFFFF;
    squad_handle = 0xFFFFFFFF;
    formation_handle = 0xFFFFFFFF;
    desired_search_handle = 0xFFFFFFFF;
    guard_node_handle = 0xFFFFFFFF;
    guard_direction_index = -1;
    guard_pos.zero();
    last_reinforcement_pos.zero();
    reserved_corpse_handle = 0xFFFFFFFF;
    desired_defense_assignment_handle = 0xFFFFFFFF;
    covering_fire_provide.clear();
    covering_fire_receive.clear();
    memset(&flags, 0, sizeof(flags));
    time_in_guard_node_group.invalidate();
    time_at_guard_node.invalidate();
    time_at_guard_facing.invalidate();
    projectile_reaction = AI_PROJECTILE_REACTION_NONE;
    tension_timer.set(-5000); // the same as Ai_tension_level_info_table[0].threshold: no tension
    reset_volatile_data();
    desired_burst_size = 0;
}

// 0x005DE430 (:1096)
void ai_system::create_persona()
{
    persona_cycle_flee_behavior_time.invalidate();
    memset(&persona_flags, 0, sizeof(persona_flags));

    if (vrand() % 100 <= (unsigned int)Ai_flee_to_defensive_cover_chance)
    {
        persona_flags.flee_to_defensive_cover = true;
    }
    else if (int_rand_range(1, 100 - Ai_flee_to_defensive_cover_chance) <= Ai_flee_to_safe_building_chance)
    {
        persona_flags.flee_to_safe_building = true;
    }
    else
    {
        const float building = (float)Ai_flee_to_safe_building_chance;
        const float cover = (float)Ai_flee_to_defensive_cover_chance;
        persona_flags.prefer_flee_to_building = fl_rand() <= building / (cover + building);
    }

    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        if (vrand() % 100 <= (unsigned int)Marauder_follow_player_chance)
        {
            persona_flags.follow_player = true;
        }
    }
    else if (vrand() % 100 <= (unsigned int)Ai_remote_charge_panic_chance)
    {
        persona_flags.remote_charge_panic = true;
    }

    // Original bug kept: Ai_mar_on_fire_panic_chance is a tweak nothing reads, marauders get the default chance.
    unsigned int on_fire_chance;
    switch (np->current_team)
    {
    case HUMAN_TEAM_GUERILLA:
        on_fire_chance = Ai_gue_on_fire_panic_chance;
        break;
    case HUMAN_TEAM_EDF:
        on_fire_chance = Ai_edf_on_fire_panic_chance;
        break;
    case HUMAN_TEAM_CIVILIAN:
        on_fire_chance = Ai_civ_on_fire_panic_chance;
        break;
    default:
        on_fire_chance = Ai_default_on_fire_panic_chance;
        break;
    }
    persona_flags.on_fire_panic = vrand() % 100 <= on_fire_chance;

    persona_flags.heavy_vehicle_always_cower = vrand() % 100 <= (unsigned int)Ai_heavy_vehicle_always_cower_chance;
    if (persona_flags.heavy_vehicle_always_cower)
    {
        persona_flags.heavy_vehicle_no_fall_back = true;
    }
    else
    {
        persona_flags.heavy_vehicle_no_fall_back = vrand() % 100 <= (unsigned int)Ai_no_fall_back_chance;
    }

    if (vrand() % 100 <= (unsigned int)Civilian_stand_up_to_edf_chance)
    {
        persona_flags.stand_up_to_edf = true;
        return;
    }
    const int remaining = 100 - Civilian_stand_up_to_edf_chance;
    if (int_rand_range(1, remaining) <= Civilian_freeze_cower_chance)
    {
        persona_flags.cower_from_edf = true;
        return;
    }
    if (int_rand_range(1, remaining - Civilian_freeze_cower_chance) <= Civilian_always_flee_edf_chance)
    {
        persona_flags.always_flee_edf = true;
    }
}

// 0x005DE670 (:1190)
void ai_system::reset_volatile_data()
{
    flags.less_aggressive_melee = false;
    desired_vehicle_speed = -1.0f;
    desired_cover_stance = COVER_STANCE_UNDEFINED;
    desired_cover_firing_position = COVER_FIRE_UNDEFINED;
    desired_vehicle = 0xFFFFFFFF;
    desired_vehicle_seat_type = AI_VEHICLE_SEAT_TYPE_UNDEFINED;
    desired_vehicle_dist_to_goal_pos = 3.0f; // metres
}

// 0x0060A8D0 (:1210)
void ai_system::register_destream()
{
    planner.stun();
}

// 0x0060A8E0 (:1222)
bool ai_system::do_despawn(bool force_despawn)
{
    const vehicle* vp = human_in_vehicle(np);
    if ((vp == nullptr || vp->m_flags.m_despawn_after_fade == 0) && np->npc_flags.despawn_after_fade == 0)
    {
        bool offscreen;
        const bool can = can_despawn(&offscreen);
        if (!force_despawn && !can)
        {
            return false;
        }
        despawn_me(offscreen);
    }
    return true;
}

// 0x005DE6C0 (:1243)
bool ai_system::is_despawning() const
{
    const vehicle* vp = human_in_vehicle(np);
    if (vp != nullptr && vp->m_flags.m_despawn_after_fade != 0)
    {
        return true;
    }
    return np->npc_flags.despawn_after_fade != 0;
}

// 0x006B53A0 (:1258)
void ai_system::do_frame_pre()
{
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    if (np == nullptr || np->is_killed() || flags.yield_next_frame)
    {
        return;
    }
    vehicle* vp = (vehicle*)typed_object_from_handle(np->vehicle_handle, OT_VEHICLE);
    if (vp != nullptr)
    {
        // Wait for the vehicle's resources, and don't plan while its interaction waits on streaming.
        if (!vp->rdd.is_loaded())
        {
            flags.yield_next_frame = true;
            return;
        }
        const int index = vp->vii.find_human(np);
        if (index >= 0 && vp->vii.humans[index].state == VI_STATE_ENTER_STREAMING_STALL)
        {
            flags.yield_next_frame = true;
            return;
        }
    }
    planner.do_frame_pre();
}

// 0x006C3680 (:1298)
void ai_system::do_frame()
{
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    if (np == nullptr || np->is_killed())
    {
        do_death_func();
        return;
    }
    if (flags.yield_next_frame)
    {
        flags.yield_next_frame = false;
        return;
    }
    update_stunned_state();
    if (np == nullptr || np->is_killed())
    {
        do_death_func();
        return;
    }

    update_on_fire();
    flags.reduced_processing = false;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        const float dist_sq = vec_xz_dist_squared(np->pos, pp->pos);
        const float limit = human_in_vehicle(np) != nullptr ? AI_SLOWDOWN_VEHICLE_PLAYER_DIST_SQ : AI_SLOWDOWN_PLAYER_DIST_SQ;
        if (limit < dist_sq)
        {
            flags.reduced_processing = true;
        }
    }
    perception.do_frame();

    const bool falling = np->hflags.jumping_from_building != 0 || np->hflags.is_falling != 0;
    bool busy = np->hflags.is_on_ladder != 0;
    if (!busy && np->npc_flags.reequipping_weapon != 0)
    {
        if (np->get_desired_inv_item() == np->get_equipped_inv_item())
        {
            np->npc_flags.reequipping_weapon = 0;
        }
        else
        {
            busy = true;
        }
    }
    if (flags.stunned || falling || busy)
    {
        human_maybe_play_falling_voice(np);
    }
    else
    {
        planner.do_frame();
    }
    perception.do_attention_frame();
    do_fire_frame();
}

// 0x00A05F80 (.h:303)
void ai_system::register_hit(const human* attacker, float damage_taken, const weapon_info* w_info)
{
    perception.register_hit(attacker, damage_taken, w_info);
}

// 0x0060A950 (:1431)
void ai_system::maybe_switch_to_wounded_animation(flinch_directions flinch_dir, object_hit_location hit_location)
{
    if (!human_team_is_non_combatant(np))
    {
        return;
    }
    if (np->hit_points * 100.0f / (float)np->h_info->get_max_hit_points() >= Civilian_max_wounded_hp_pct)
    {
        return;
    }
    anim_set_variants variant_type;
    switch (hit_location)
    {
    case HIT_TORSO:
    case HIT_PELVIS:
        variant_type = flinch_dir == FLINCH_DIR_BACK ? ANIM_SET_VARIANT_WOUNDED_BACK : (anim_set_variants)(ANIM_SET_VARIANT_WOUNDED_BACK - 1);
        break;
    case HIT_LEFT_LEG:
        variant_type = ANIM_SET_VARIANT_WOUNDED_LEFT_LEG;
        break;
    case HIT_RIGHT_LEG:
        variant_type = ANIM_SET_VARIANT_WOUNDED_RIGHT_LEG;
        break;
    default:
        variant_type = ANIM_SET_VARIANT_WOUNDED;
        break;
    }
    set_base_anim_set(variant_type, true, false);
}

// 0x0060AA80 (:1470)
void ai_system::stun(animation_action anim_action, vanim_aliases anim_alias, float anim_speed, unsigned int anim_flags)
{
    np->char_inst->stop_action(STUN_STOP_ACTION_MASK, 3, false);
    flags.stunned = true;
    planner.stun();
    if (anim_action == ANIMATION_ACTION_NONE)
    {
        return;
    }
    np->char_inst->play_action(anim_action, anim_alias, anim_speed, anim_flags);
    stunned_animation = anim_action;
    const int length_ms = (int)(vanim_get_anim_length(np->char_inst->vp, anim_action) * 1000.0f);
    if (!human_team_is_non_combatant(np))
    {
        human_set_combat_ready(np, true, length_ms);
    }
    if (anim_alias == ANIM_ALIAS_EXPLOSION_KNOCKDOWN && np->cur_stance == HST_CROUCH)
    {
        human_set_stance(np, HST_STAND);
    }
}

// 0x0060AB70 (:1511)
void ai_system::enter_stunned_state(animation_state anim_state, int stun_time, vanim_aliases anim_alias)
{
    if (anim_state == ANIMATION_STATE_NONE)
    {
        return;
    }
    flags.stunned = true;
    np->char_inst->stop_action(STUN_STOP_ACTION_MASK, 3, false);
    human_weapons_cancel_reload(np);
    planner.stun();
    if (anim_state == ANIMATION_STATE_ELECTRIC_SHOCK && np->cur_stance == HST_CROUCH)
    {
        np->char_inst->play_action(ANIMATION_ACTION_STUNGUN_FLINCH, ANIM_ALIAS_NONE, 1.0f, 0);
    }
    else
    {
        human_override_default_animation_state(np, true);
        np->char_inst->set_state(index_alias(anim_state, anim_alias), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
    }
    stunned_animation = anim_state;
    stunned_timer.set(stun_time);
    if (!human_team_is_non_combatant(np))
    {
        human_set_combat_ready(np, stun_time != 0, 0);
    }
}

// 0x005DC960 (.h:310)
bool ai_system::is_stunned() const
{
    return flags.stunned;
}

// 0x005DE700 (:1550)
bool ai_system::is_idle() const
{
    return planner.get_current_goal() == idle_goal::get_instance();
}

// 0x005DE710 (:1557)
bool ai_system::get_path_dest(vector& path_dest) const
{
    if (np->movement_mode != HMM_PATHFINDING)
    {
        return false;
    }
    path_dest = np->pf.eventual_goal_pos;
    return true;
}

// 0x005DE750 (:1569)
bool ai_system::is_pathing_to(const vector& destination, float xz_tolerance_sq, float y_tolerance) const
{
    if (np->movement_mode != HMM_PATHFINDING)
    {
        return false;
    }
    if (vec_xz_dist_squared(np->pf.eventual_goal_pos, destination) > xz_tolerance_sq)
    {
        return false;
    }
    return fabsf(np->pf.eventual_goal_pos.y - destination.y) <= y_tolerance;
}

// 0x005DE7E0 (:1588)
bool ai_system::is_done_pathfinding(float time) const
{
    if (np->movement_mode != HMM_PATHFINDING)
    {
        return true;
    }
    if (time <= 0.0f || fabsf(np->pos.y - np->pf.eventual_goal_pos.y) > 1.0f)
    {
        return false;
    }
    const float dist = vec_xz_dist(np->pos, np->pf.eventual_goal_pos);
    float speed = np->move_speed;
    if (speed < 0.0f)
    {
        speed = human_get_move_speed_info(np, np->cur_movement_state)->average_speed;
    }
    return dist / speed <= time;
}

// 0x005DE8C0 (:1622)
bool ai_system::has_hostile_contact() const
{
    return perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr;
}

// 0x0060AC80 (:1634)
bool ai_system::is_engaging_player() const
{
    // Original bug kept: no check of the local player.
    return perception.get_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, player_get_local_player()->get_handle()) != nullptr;
}

// 0x0064E330 (:1650)
player* ai_system::is_in_player_squad(bool include_defense_orders) const
{
    object_squad* squad = squad_from_handle(squad_handle);
    if (squad == nullptr)
    {
        return nullptr;
    }
    player* pp = squad->get_player();
    if (pp == nullptr && include_defense_orders)
    {
        gfm_orders* orders = squad->get_squad_orders();
        if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND)
        {
            pp = World.object_cast<player>(orders->get_target_object());
        }
    }
    return pp;
}

// 0x005DE8D0 (:1668)
bool ai_system::is_fleeing() const
{
    const ai_goal* goal = planner.get_current_goal();
    const ai_action* action = planner.get_current_action();
    return goal == flee_debris_goal::get_instance()
        || goal == flee_enemies_goal::get_instance()
        || goal == flee_from_vehicle_goal::get_instance()
        || goal == flee_heavy_vehicle_goal::get_instance()
        || goal == flee_restricted_area_goal::get_instance()
        || goal == flee_to_safety_goal::get_instance()
        || goal == flee_unsafe_building_goal::get_instance()
        || goal == pedestrian_panicked_goal::get_instance()
        || action == custom_rescued_action::get_instance();
}

// 0x0060ACB0 (:1684)
static bool all_occupants_are_allies(const npc* np, const vehicle* vp)
{
    if (np == nullptr || vp == nullptr)
    {
        return false;
    }
    int remaining = vp->vii.num_humans;
    for (int i = 0; remaining > 0 && i < 8; ++i)
    {
        const unsigned int handle = vp->vii.humans[i].handle;
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        --remaining;
        human* hp = World.object_from_handle<human>(handle);
        if (!human_teams_are_allies(np, hp))
        {
            npc* other = World.object_cast<npc>(hp);
            if (other == nullptr || other->npc_flags.tied_up == 0)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x0064E390 (:1719)
bool ai_system::usable_squad_vehicle(const vehicle* vp) const
{
    if (vp == nullptr)
    {
        return false;
    }
    if (vp->get_handle() == np->vehicle_handle)
    {
        return true;
    }
    object_squad* squad = squad_from_handle(squad_handle);
    if (squad == nullptr)
    {
        return false;
    }
    player* pp = is_in_player_squad(true);
    if (pp != nullptr)
    {
        return pp->vehicle_handle == vp->get_handle();
    }
    if (vp->get_handle() == squad->squad_vehicle)
    {
        return all_occupants_are_allies(np, vp);
    }

    // Another squad's vehicle is off limits.
    const int num_squads = World.get_num_objects<object_squad>();
    for (int i = 0; i < num_squads; ++i)
    {
        object_squad* other = World.get_object<object_squad>(i);
        if (other != squad && vp->get_handle() == other->squad_vehicle)
        {
            return false;
        }
    }

    if (vehicle_get_num_passengers(vp) == 0
        && (human_teams_are_allies(np, vp->team_of_most_recent_driver) || human_teams_are_allies(np, vp->info->default_team)))
    {
        return true;
    }
    int remaining = vp->vii.num_humans;
    for (int i = 0; remaining > 0 && i < 8; ++i)
    {
        const unsigned int handle = vp->vii.humans[i].handle;
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        --remaining;
        npc* occupant = World.object_from_handle<npc>(handle);
        if (occupant != nullptr && occupant->npc_flags.tied_up == 0)
        {
            if (!human_teams_are_allies(np, occupant))
            {
                return false;
            }
            const unsigned int occupant_squad = occupant->get_ai()->squad_handle;
            if (occupant_squad != 0xFFFFFFFF && occupant_squad != squad->get_handle())
            {
                return false;
            }
        }
    }
    return true;
}

// 0x005DE930 (:1805)
bool ai_system::should_stay_in_fetal_position() const
{
    return np->combat.beaten_up_timer.valid() && np->combat.beaten_up_timer.time_since() <= FETAL_POSITION_MS;
}

// 0x005DE970 (:1817)
bool ai_system::should_go_to_fetal_position() const
{
    if (!human_team_is_non_combatant(np))
    {
        return false;
    }
    if (np->hflags.in_fetal_position != 0)
    {
        return true;
    }
    return np->combat.beaten_up_timer.valid() && np->combat.beaten_up_timer.time_since() <= FETAL_POSITION_MS;
}

// 0x0064E570 (:1832)
vehicle* ai_system::passenger_of_squadmate() const
{
    object_squad* squad = squad_from_handle(squad_handle);
    if (squad == nullptr)
    {
        return nullptr;
    }
    vehicle* vp = (vehicle*)typed_object_from_handle(np->vehicle_handle, OT_VEHICLE);
    if (vp == nullptr)
    {
        return nullptr;
    }
    human* driver = vehicle_passenger_in_entering_or_reserving_seat(vp, VSI_DRIVER);
    if (driver == nullptr || driver == np)
    {
        return nullptr;
    }
    return is_in_my_squad(driver->get_handle()) ? vp : nullptr;
}

// 0x005DE9D0 (:1866)
void ai_system::assign_squad(unsigned int new_squad_handle)
{
    squad_handle = new_squad_handle;
    np->npc_flags.had_squad = 1;
}

// 0x005DE9F0 (:1875)
void ai_system::clear_squad()
{
    squad_handle = 0xFFFFFFFF;
}

// 0x0064E610 (:1883)
bool ai_system::leave_squad()
{
    object_squad* squad = squad_from_handle(squad_handle);
    if (squad == nullptr)
    {
        return false;
    }
    player* pp = squad->get_player();
    if (pp != nullptr)
    {
        if (squad->get_handle() == pp->ally_squad_handle)
        {
            pp->ally_squad_remove(np);
            return true;
        }
        if (squad->get_handle() == pp->escort_squad_handle)
        {
            pp->escort_squad_remove(np, false);
            return true;
        }
    }
    squad->remove_member(np->get_handle());
    squad_handle = 0xFFFFFFFF;
    return true;
}

// 0x0060AD40 (:1928)
void ai_system::register_team_change(human_teams old_team, human_teams new_team)
{
    perception.register_team_change(old_team, new_team);
    planner.request_replan();
}

// 0x0060AD70 (:1937)
void ai_system::assigned_new_orders()
{
    planner.request_replan();
    guard_node_handle = 0xFFFFFFFF;
    guard_pos = ZERO_VECTOR;
}

// 0x0060B3A0 (:2495)
gfm_orders* ai_system::get_orders() const
{
    object_squad* squad = squad_from_handle(squad_handle);
    return squad == nullptr ? nullptr : squad->get_squad_orders();
}

// 0x0064E6C0 (:1949)
gfm_orders_types ai_system::get_orders_type() const
{
    const gfm_orders* orders = get_orders();
    return orders == nullptr ? GFM_ORDERS_TYPE_NONE : orders->get_orders_type();
}

// 0x00673A20 (.h:525)
bool ai_system::has_orders_of_type(gfm_orders_types type) const
{
    const gfm_orders* orders = get_orders();
    if (orders != nullptr)
    {
        return orders->get_orders_type() == type;
    }
    return type == GFM_ORDERS_TYPE_NONE;
}

// 0x0064E700 (:1960)
const object* ai_system::get_orders_object() const
{
    const gfm_orders* orders = get_orders();
    return orders == nullptr ? nullptr : orders->get_target_object();
}

// 0x0064E740 (:1972)
bool ai_system::near_orders_location(float buffer) const
{
    const gfm_orders* orders = get_orders();
    if (orders == nullptr)
    {
        return false;
    }
    const float radius = (is_in_player_squad(true) != nullptr ? NEAR_ORDERS_PLAYER_RADIUS : NEAR_ORDERS_RADIUS) + buffer;
    const float radius_sq = radius * radius;
    const object* target = orders->get_target_object();
    if (target != nullptr)
    {
        vector local_pos = np->pos;
        vec_change_to_local_coords(local_pos, target->pos, target->orient);
        vector bmin, bmax;
        target->get_local_bbox(bmin, bmax);
        if (bbox_find_distance_squared(bmin, bmax, local_pos) <= radius_sq)
        {
            return true;
        }
    }
    return vec_xz_dist_squared(np->pos, orders->get_location()) <= radius_sq;
}

// 0x0060ADB0 (:2007)
object_guard_node* ai_system::get_guard_node() const
{
    object_guard_node* gnp = (object_guard_node*)typed_object_from_handle(guard_node_handle, OT_GUARD_NODE);
    if (gnp == nullptr || !gnp->is_reserved(np->get_handle()))
    {
        return nullptr;
    }
    return gnp;
}

// 0x0060AE00 (:2017)
signed char ai_system::get_guard_direction_index() const
{
    // -128: no direction, the same value reserve_guard_pos stores and reserve_guard_node takes for "any".
    return get_guard_node() != nullptr ? guard_direction_index : (signed char)NO_GUARD_DIRECTION;
}

// 0x00673EF0 (:2028)
bool ai_system::has_valid_guard_post() const
{
    const object_guard_node* gnp = get_guard_node();
    if (gnp != nullptr)
    {
        if (gnp->is_disabled())
        {
            return false;
        }
        const object* defend = get_defend_object();
        if (defend == nullptr || gnp->get_parent() == defend->get_handle())
        {
            return true;
        }
        return !outside_defensive_perimeter(gnp->pos, false, nullptr);
    }
    const gfm_orders* orders = get_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return false;
    }
    return !outside_defensive_perimeter(guard_pos, false, nullptr);
}

// 0x0068EFA0 (:2071)
bool ai_system::is_at_guard_post() const
{
    if (!has_valid_guard_post())
    {
        return false;
    }
    return vec_xz_dist_squared(np->pos, guard_pos) <= 1.0f && fabsf(np->pos.y - guard_pos.y) < AT_GUARD_POST_Y_TOLERANCE;
}

// 0x0060AE50 (:2085)
int ai_system::get_time_at_guard_node() const
{
    if (get_guard_node() == nullptr)
    {
        return -1;
    }
    return time_at_guard_node.valid() ? time_at_guard_node.time_since() : 0;
}

// 0x005DCA20 (.h:587)
void ai_system::reset_time_at_guard_node()
{
    time_at_guard_node.set(0);
}

// 0x0060AEB0 (:2094)
int ai_system::get_time_at_guard_facing() const
{
    if (get_guard_node() == nullptr)
    {
        return -1;
    }
    return time_at_guard_facing.valid() ? time_at_guard_facing.time_since() : 0;
}

// 0x0060AF10 (:2105)
int ai_system::get_time_in_guard_node_group() const
{
    if (get_guard_node() == nullptr)
    {
        return -1;
    }
    return time_in_guard_node_group.valid() ? time_in_guard_node_group.time_since() : 0;
}

// 0x005DCA30 (.h:594)
void ai_system::reset_time_in_guard_node_group()
{
    time_in_guard_node_group.set(0);
}

// 0x0060AF70 (:2122)
bool ai_system::is_in_guard_group(int group_id) const
{
    const object_guard_node* gnp = get_guard_node();
    return gnp != nullptr && group_id == (int)gnp->get_group_id();
}

// 0x0060AFD0 (:2132)
bool ai_system::reserve_guard_node(object_guard_node* gnp, signed char dir_index)
{
    if (gnp == nullptr || !gnp->can_reserve(np->get_handle()))
    {
        return false;
    }
    const object_guard_node* old_node = get_guard_node();
    guard_node_handle = gnp->get_handle();
    guard_pos = gnp->pos;
    gnp->reserve_node(np->get_handle());
    if (gnp != old_node)
    {
        flags.run_to_guard_post = old_node == nullptr && perception.get_first_belief(AI_BELIEF_ENEMY_EXISTS) != nullptr;
        time_at_guard_node.invalidate();
        time_at_guard_facing.invalidate();
    }
    if (old_node == nullptr || old_node->get_group_id() != gnp->get_group_id())
    {
        time_in_guard_node_group.set(0);
    }

    bool at_post;
    if (vec_xz_dist_squared(np->pos, guard_pos) > 1.0f || fabsf(np->pos.y - guard_pos.y) >= AT_GUARD_POST_Y_TOLERANCE)
    {
        at_post = false;
        time_at_guard_node.invalidate();
    }
    else
    {
        at_post = true;
        if (!time_at_guard_node.valid())
        {
            time_at_guard_node.set(0);
        }
    }

    if (dir_index == NO_GUARD_DIRECTION)
    {
        if (old_node == nullptr)
        {
            guard_direction_index = 0;
        }
    }
    else if (guard_direction_index != dir_index)
    {
        time_at_guard_facing.invalidate();
        guard_direction_index = dir_index;
    }
    const vector facing = gnp->get_facing_from_index(guard_direction_index);
    const vector& fvec = np->orient.fvec;
    if (at_post && get_compressed_cosine_int(GUARD_FACING_COMPRESSED_COSINE) <= fvec.y * facing.y + fvec.x * facing.x + fvec.z * facing.z)
    {
        if (!time_at_guard_facing.valid())
        {
            time_at_guard_facing.set(0);
        }
        return true;
    }
    time_at_guard_facing.invalidate();
    return true;
}

// 0x005DEA00 (:2197)
bool ai_system::reserve_guard_pos(const vector& new_guard_pos)
{
    guard_node_handle = 0xFFFFFFFF;
    time_at_guard_node.invalidate();
    time_at_guard_facing.invalidate();
    guard_direction_index = NO_GUARD_DIRECTION;
    guard_pos = new_guard_pos;
    return true;
}

// 0x00674360 (:2209)
bool ai_system::get_guard_pos(vector& gp) const
{
    if (!has_valid_guard_post())
    {
        return false;
    }
    gp = guard_pos;
    return true;
}

// 0x0060B220 (:2219)
void ai_system::release_guard_pos()
{
    object_guard_node* gnp = get_guard_node();
    if (gnp != nullptr)
    {
        gnp->release_node(np->get_handle());
    }
    guard_node_handle = 0xFFFFFFFF;
    guard_pos = ZERO_VECTOR;
}

// 0x0060B2A0 (:2233)
bool ai_system::trigger_custom_behavior(ai_scripting_behaviors behavior, unsigned int target_obj_handle, int special_value, int action_id, bool replan)
{
    custom_behavior = behavior;
    custom_behavior_target = target_obj_handle;
    custom_behavior_value = special_value;
    custom_action_id = action_id;
    if (replan)
    {
        planner.request_replan();
    }
    return true;
}

// 0x005DEA40 (:2247)
void ai_system::end_custom_behavior()
{
    custom_behavior = AI_SB_NONE;
    custom_behavior_target = 0xFFFFFFFF;
    custom_behavior_value = -1;
}

// 0x005DC970 (.h:360)
bool ai_system::has_custom_behavior() const
{
    return custom_behavior != AI_SB_NONE;
}

// 0x0060B2E0 (:2261)
ai_tension_levels ai_system::get_tension_level() const
{
    int time_left = tension_timer.time_until();
    if (np->h_info->flags.elite_dodge && time_left <= 5000)
    {
        time_left = 5000; // ms, elites are always at least this tense
    }
    for (int level = AI_TENSION_LEVEL_NONE; level < NUM_AI_TENSION_LEVELS; ++level)
    {
        if (time_left < Ai_tension_level_info_table[level].threshold)
        {
            return (ai_tension_levels)level;
        }
    }
    return AI_TENSION_LEVEL_HIGH;
}

// 0x0060B330 (:2280)
void ai_system::increase_tension_level(int amount)
{
    int time_left = tension_timer.time_until();
    if (time_left < 0)
    {
        time_left = 0;
    }
    time_left += amount * 1000;
    if (time_left > 60000)
    {
        time_left = 60000; // ms, the last tension threshold
    }
    tension_timer.set(time_left);
}

// 0x005DEA50 (:2296)
ai_projectile_reactions ai_system::pick_projectile_reaction(ai_tension_levels tension_level, bool is_explosive) const
{
    const int* probs = is_explosive ? Ai_tension_level_info_table[tension_level].explosive_reaction_probs : Ai_tension_level_info_table[tension_level].projectile_reaction_probs;
    int roll = int_rand_range(0, 99);
    for (int reaction = AI_PROJECTILE_REACTION_NONE; reaction < NUM_AI_PROJECTILE_REACTIONS; ++reaction)
    {
        roll -= probs[reaction];
        if (roll < 0)
        {
            return (ai_projectile_reactions)reaction;
        }
    }
    return AI_PROJECTILE_REACTION_NONE;
}

// ---- formations (rfg.exe inlines the pool lookups of ai_formation_*)

// 0x006BB7F0 (:2316)
bool ai_system::leave_formation()
{
    if (formation_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const bool removed = ai_formation_remove_follower(formation_handle, np->get_handle());
    if (removed)
    {
        formation_handle = 0xFFFFFFFF;
    }
    return removed;
}

// 0x006BB850 (:2332)
bool ai_system::leave_vehicle_formation()
{
    object_convoy* convoy = get_convoy_object();
    if (convoy != nullptr && convoy->in_convoy_formation(squad_handle))
    {
        return false;
    }
    if (vehicle_formation_handle == 0xFFFFFFFF)
    {
        return false;
    }
    if (human_is_driving(np) && np->vehicle_handle != 0xFFFFFFFF)
    {
        if (ai_formation_remove_follower(vehicle_formation_handle, np->vehicle_handle))
        {
            vehicle_formation_handle = 0xFFFFFFFF;
            return true;
        }
    }
    else
    {
        vehicle_formation_handle = 0xFFFFFFFF;
    }
    return false;
}

// 0x005DCA10 (.h:542)
unsigned int ai_system::get_vehicle_formation_handle() const
{
    return vehicle_formation_handle;
}

// 0x0064E920 (:2355)
bool ai_system::is_following_leader(const npc* leader_np) const
{
    return formation_handle != 0xFFFFFFFF && ai_formation_is_leader(leader_np->get_handle(), formation_handle);
}

// 0x0064E970 (:2365)
bool ai_system::vehicle_is_following_leader(const object* leader) const
{
    return vehicle_formation_handle != 0xFFFFFFFF && ai_formation_is_leader(leader->get_handle(), vehicle_formation_handle);
}

// 0x006BB8D0 (:2375)
unsigned int ai_system::follow_in_formation(const object* target, ai_formation_types type)
{
    if (target == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (formation_handle != 0xFFFFFFFF)
    {
        if (ai_formation_is_leader(target->get_handle(), formation_handle))
        {
            return formation_handle;
        }
        ai_formation_remove_follower(formation_handle, np->get_handle());
        formation_handle = 0xFFFFFFFF;
    }
    formation_handle = ai_formation_follow(np, target, type);
    return formation_handle;
}

// 0x006BB960 (:2393)
unsigned int ai_system::vehicle_follow_in_formation(vehicle* vp, const object* target, ai_formation_types type)
{
    if (vp == nullptr || target == nullptr || vehicle_passenger_in_seat(vp, VSI_DRIVER) != np)
    {
        return 0xFFFFFFFF;
    }
    object_convoy* convoy = get_convoy_object();
    if (convoy != nullptr && convoy->in_convoy_formation(squad_handle))
    {
        return 0xFFFFFFFF;
    }
    if (vehicle_formation_handle != 0xFFFFFFFF)
    {
        if (ai_formation_is_leader(target->get_handle(), vehicle_formation_handle) && ai_formation_is_member(vehicle_formation_handle, vp->get_handle()))
        {
            return vehicle_formation_handle;
        }
        ai_formation_remove_follower(vehicle_formation_handle, vp->get_handle());
        vehicle_formation_handle = 0xFFFFFFFF;
    }
    vehicle_formation_handle = ai_formation_follow(vp, target, type);
    return vehicle_formation_handle;
}

// 0x006BBA20 (:2417)
unsigned int ai_system::vehicle_follow_convoy_start()
{
    object_convoy* convoy = get_convoy_object();
    if (convoy == nullptr)
    {
        return 0xFFFFFFFF;
    }
    object_squad* squad = squad_from_handle(squad_handle);
    if (squad == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (vehicle_formation_handle != 0xFFFFFFFF)
    {
        object_convoy* formation_convoy = get_convoy_object();
        if (formation_convoy == nullptr || !formation_convoy->in_convoy_formation(squad_handle))
        {
            ai_formation_remove_follower(vehicle_formation_handle, np->vehicle_handle);
            vehicle_formation_handle = 0xFFFFFFFF;
        }
        else
        {
            if (convoy->get_main_formation_handle() == vehicle_formation_handle)
            {
                return vehicle_formation_handle;
            }
            const object_squad* lead = convoy->get_lead_squad();
            if (lead != nullptr && (const object*)lead == ai_formation_get_leader(vehicle_formation_handle))
            {
                return vehicle_formation_handle;
            }
            ai_formation_remove_follower(vehicle_formation_handle, squad->get_handle());
            vehicle_formation_handle = 0xFFFFFFFF;
        }
    }
    vehicle_formation_handle = convoy->join_convoy_formation(squad);
    return vehicle_formation_handle;
}

// 0x006BBB20 (:2462)
bool ai_system::vehicle_follow_convoy_stop()
{
    if (vehicle_formation_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object_convoy* convoy = get_convoy_object();
    if (convoy == nullptr)
    {
        ai_formation_remove_follower(vehicle_formation_handle, squad_handle);
        vehicle_formation_handle = 0xFFFFFFFF;
        return true;
    }
    bool result = true;
    if (convoy->get_squad_role(squad_handle) == CONVOY_SQUAD_ROLE_BREAK_AND_ATTACK)
    {
        result = convoy->leave_convoy_formation(squad_handle);
    }
    vehicle_formation_handle = 0xFFFFFFFF;
    return result;
}

// ---- squad

// 0x0060B370 (:2486)
object_squad* ai_system::get_squad() const
{
    return squad_from_handle(squad_handle);
}

// 0x0060B3D0 (:2510)
bool ai_system::is_in_my_squad(unsigned int np_handle) const
{
    const object_squad* squad = squad_from_handle(squad_handle);
    if (squad == nullptr)
    {
        return false;
    }
    return squad->member_array.find(np_handle, false) > -1;
}

// 0x0060B430 (:2523)
bool ai_system::is_in_my_squad(const npc* npp) const
{
    return npp != nullptr && is_in_my_squad(npp->get_handle());
}

// 0x0064E9C0 (:2537)
bool ai_system::collect_squadmates(base_array<npc*>& squadmates, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data) const
{
    if (squadmates.size() >= squadmates.capacity())
    {
        return true;
    }
    const object_squad* squad = squad_from_handle(squad_handle);
    if (squad == nullptr)
    {
        return false;
    }
    for (int i = 0; i < squad->member_array.size(); ++i)
    {
        npc* mate = npc_from_handle(squad->member_array[i]);
        if (mate != np && (cb_func == nullptr || cb_func(mate, 0, cb_data)))
        {
            squadmates.add(mate);
            if (squadmates.size() >= squadmates.capacity())
            {
                return true;
            }
        }
    }
    return false;
}

// 0x0060B450 (:2576)
const char* ai_system::get_action_node_type() const
{
    const gfm_orders* orders = get_orders();
    if (orders == nullptr)
    {
        return nullptr;
    }
    const object* target = orders->get_target_object();
    if (target == nullptr || target->get_obj_type() != OT_ACTION_NODE)
    {
        return nullptr;
    }
    return ((const object_action_node*)target)->get_ambient_type();
}

// 0x006743A0 (:2597)
bool ai_system::allowed_to_use_action_node() const
{
    if (human_in_vehicle(np) != nullptr || np->npc_flags.converted_guerrilla != 0)
    {
        return false;
    }
    if (has_orders_of_type(GFM_ORDERS_TYPE_AMBIENT_ACTION) || has_orders_of_type(GFM_ORDERS_TYPE_AMBIENT_PED) || has_orders_of_type(GFM_ORDERS_TYPE_PATROL))
    {
        return true;
    }
    return np->npc_flags.ambient_spawn != 0 || np->scripted_action_node_handle != 0xFFFFFFFF;
}

// 0x005DEAA0 (:2655)
bool ai_system::can_teleport_into_vehicle(const vehicle* vp, ai_vehicle_seat_types seat_type) const
{
    if (np->npc_flags.chase_teleport_disable != 0 || planner.get_current_goal() == man_flyer_goal::get_instance())
    {
        return false;
    }
    const vector& player_pos = player_get_local_player()->pos;
    if (vec_dist_squared(player_pos, np->pos) < TELEPORT_MIN_PLAYER_DIST_SQ)
    {
        return false;
    }
    const float min_dist_sq = seat_type == AI_VEHICLE_SEAT_TYPE_DRIVER ? TELEPORT_MIN_PLAYER_DIST_SQ : MIN_TELEPORT_DIST_SQRD;
    return vec_dist_squared(player_pos, vp->pos) >= min_dist_sq;
}

// 0x0064EAE0 (:2700)
bool ai_system::is_done_reinforcing() const
{
    const gfm_orders* orders = get_orders();
    if (orders == nullptr)
    {
        return false;
    }
    const vector location = orders->get_location();
    // const_cast: rfg.exe declares this const and still writes the position it reinforced.
    vector& last_pos = const_cast<vector&>(last_reinforcement_pos);
    if (!last_pos.is_null() && vec_dist_squared(location, last_pos) < REINFORCEMENT_REPEAT_DIST_SQ)
    {
        return true;
    }
    if (near_orders_location(10.0f) && perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr)
    {
        last_pos = location;
        return true;
    }
    last_pos.zero();
    return false;
}

// 0x0064EBE0 (:2739)
navpoint* ai_system::get_safe_building_nav(bool friendly_only) const
{
    object* nav = typed_object_from_handle(flee_destination_handle, OT_NAVPOINT);
    if (nav == nullptr || nav->get_parent() == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* parent = World.object_from_handle(nav->get_parent());
    if (parent == nullptr || parent->get_obj_type() != OT_MOVER || parent->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    return perception.building_is_safe((rfg_mover*)parent, friendly_only) ? (navpoint*)nav : nullptr;
}

// 0x005DEB50 (:2768)
bool ai_system::should_cycle_flee_behavior() const
{
    return mission_is_active() && v_stricmp(mission_get_name(), CYCLE_FLEE_MISSION_NAME) == 0;
}

// 0x0060B4A0 (:2789)
bool ai_system::get_current_road_destination(ambient_road_info& destination_info) const
{
    if (road_route.size() < 1)
    {
        return false;
    }
    const road_segment_info& segment = road_route[0];
    destination_info.road = (object_path_road*)typed_object_from_handle(segment.road_handle, OT_PATH_ROAD);
    if (destination_info.road == nullptr)
    {
        return false;
    }
    if (destination_info.road->has_breaks())
    {
        switch (destination_info.road->is_broken_at_index(segment.start_index))
        {
        case ROAD_BROKEN_COMPLETELY:
            return false;
        case ROAD_BROKEN_LEFT:
            if (segment.start_index < segment.end_index)
            {
                return false;
            }
            break;
        case ROAD_BROKEN_RIGHT:
            if (segment.end_index < segment.start_index)
            {
                return false;
            }
            break;
        default:
            break;
        }
    }
    if (destination_info.road->has_road_blocks() && destination_info.road->is_road_blocked_at_index(segment.start_index))
    {
        return false;
    }
    destination_info.destination_index = segment.start_index;
    destination_info.destination_pos = destination_info.road->get_road_point(segment.start_index);
    return true;
}

// 0x0064EC50 (:2831)
void ai_system::remove_road_destination_index(unsigned int last_road_visited, unsigned short last_road_index)
{
    if (last_road_route.size() >= last_road_route.capacity())
    {
        last_road_route.remove(0);
    }
    const int num = last_road_route.size();
    if (num > 0 && last_road_visited == last_road_route[num - 1].road_handle)
    {
        last_road_route[num - 1].end_index = last_road_index;
        return;
    }
    road_segment_info segment;
    segment.road_handle = last_road_visited;
    segment.start_index = last_road_index;
    segment.end_index = last_road_index;
    last_road_route.add(segment);
}

// 0x0064ED50 (:2863)
void ai_system::remove_current_road_destination()
{
    if (road_route.size() < 1)
    {
        return;
    }
    road_segment_info& segment = road_route[0];
    remove_road_destination_index(segment.road_handle, segment.start_index);
    if (segment.start_index == segment.end_index)
    {
        road_route.remove(0);
    }
    else if (segment.end_index < segment.start_index)
    {
        --segment.start_index;
    }
    else
    {
        ++segment.start_index;
    }
}

// 0x0060B570 (:2900)
bool ai_system::road_route_valid() const
{
    if (road_route.size() < 1)
    {
        return false;
    }
    object_path_road* road = (object_path_road*)typed_object_from_handle(road_route[0].road_handle, OT_PATH_ROAD);
    if (road == nullptr)
    {
        return false;
    }
    vector closest;
    road->find_closest_point_on_road(np->pos, closest, false);
    if (vec_dist_squared(np->pos, closest) <= ROAD_ROUTE_VALID_DIST_SQ)
    {
        return true;
    }
    road = object_road_get_closest_road(np->pos, false, NEAR_ROAD_DISTANCE, false, false);
    return road != nullptr && road->road_flags.ambient == 0;
}

// 0x0060B640 (:2938)
void ai_system::maybe_cycle_flee_behavior()
{
    if (!should_cycle_flee_behavior())
    {
        persona_cycle_flee_behavior_time.invalidate();
        return;
    }
    if (!persona_cycle_flee_behavior_time.valid())
    {
        const int longest = Ai_cycle_flee_behavior_flee_time > Ai_cycle_flee_behavior_hide_time ? Ai_cycle_flee_behavior_flee_time : Ai_cycle_flee_behavior_hide_time;
        persona_cycle_flee_behavior_time.set_rand(longest * 1000 / 2, longest * 1000);
        return;
    }
    if (!persona_cycle_flee_behavior_time.elapsed())
    {
        return;
    }
    if (persona_flags.flee_to_defensive_cover)
    {
        persona_flags.flee_to_defensive_cover = false;
        persona_flags.prefer_flee_to_building = false;
    }
    else if (persona_flags.flee_to_safe_building)
    {
        persona_flags.flee_to_safe_building = false;
        persona_flags.prefer_flee_to_building = true;
    }
    else
    {
        if (persona_flags.prefer_flee_to_building)
        {
            persona_flags.flee_to_safe_building = true;
        }
        else
        {
            persona_flags.flee_to_defensive_cover = true;
        }
        persona_flags.prefer_flee_to_building = false;
    }
    const bool hiding = persona_flags.flee_to_safe_building || persona_flags.flee_to_defensive_cover;
    persona_cycle_flee_behavior_time.set((hiding ? Ai_cycle_flee_behavior_hide_time : Ai_cycle_flee_behavior_flee_time) * 1000);
}

// ---- covering fire

// 0x0064CB00 (.h:508)
npc* ai_system::get_ally_covering_me() const
{
    return covering_fire_receive.get();
}

// 0x0064CB40 (.h:509)
npc* ai_system::is_providing_covering_fire() const
{
    return covering_fire_provide.get();
}

// 0x00674420 (:2998)
bool ai_system::request_covering_fire(npc* ally)
{
    if (ally == np || covering_fire_provide.get() != nullptr)
    {
        return false;
    }
    bool already_covered = false;
    npc* covering = covering_fire_receive.get();
    if (covering != nullptr)
    {
        if (ally != nullptr && ally != covering)
        {
            return false;
        }
        already_covered = true;
        ally = covering;
    }
    if (ally == nullptr)
    {
        return false;
    }
    ai_system* ally_ai = ally->get_ai();
    if (!ally_ai->can_provide_covering_fire(np))
    {
        return false;
    }
    covering_fire_receive.set(ally);
    ally_ai->covering_fire_provide.set(np);
    weapon* wp = human_weapons_get_weapon(ally);
    if (!already_covered)
    {
        // Top the magazine up, so the ally does not stop to reload straight away.
        const int magazine_size = wp->w_info->magazine_size;
        wp->set_rounds_in_magazine(wp->get_rounds_in_magazine() > magazine_size ? wp->get_rounds_in_magazine() : magazine_size);
        ally_ai->planner.request_replan();
    }
    return true;
}

// 0x0060B730 (:3073)
void ai_system::end_covering_fire_request()
{
    npc* ally = covering_fire_receive.get();
    if (ally == nullptr)
    {
        return;
    }
    covering_fire_receive.clear();
    ai_system* ally_ai = ally->get_ai();
    if (ally_ai->covering_fire_provide.get() == np)
    {
        ally_ai->covering_fire_provide.clear();
    }
}

// 0x0060B7F0 (:3092)
bool ai_system::can_receive_covering_fire() const
{
    return human_in_vehicle(np) == nullptr && covering_fire_receive.get() == nullptr && covering_fire_provide.get() == nullptr;
}

// 0x0064EDD0 (:3116)
bool ai_system::can_provide_covering_fire(const npc* ally) const
{
    if (ally == nullptr || ally->is_killed() || human_in_vehicle(np) != nullptr || human_teams_get_disposition(np, ally) != TEAM_DISPOSITION_ALLIED)
    {
        return false;
    }
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp == nullptr || !wp->w_info->is_firearm() || !wp->is_loaded() || covering_fire_receive.get() != nullptr)
    {
        return false;
    }
    const npc* covering = covering_fire_provide.get();
    if (covering == ally)
    {
        return true;
    }
    if (covering != nullptr || np->cur_stance == HST_PRONE || human_is_ragdolled(np) || np->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || np->combat.target_handle != ally->combat.target_handle)
    {
        return false;
    }
    return perception.get_belief(AI_BELIEF_FIRST_CONTACT, np->combat.target_handle) != nullptr || human_combat_cover_is_using_node(np);
}

// ---- district

// 0x005DEB80 (:3172)
ai_morale_levels ai_system::get_district_morale_level() const
{
    district* dp = district_get_from_pos(np->pos);
    const float max_value = dp->get_morale_max();
    if (max_value <= 0.0f)
    {
        return AI_MORALE_LEVEL_NONE;
    }
    const float fraction = dp->get_morale() / max_value;
    if (fraction <= AI_LOW_MORALE_CUTOFF)
    {
        return AI_MORALE_LEVEL_LOW;
    }
    return fraction < AI_HIGH_MORALE_CUTOFF ? AI_MORALE_LEVEL_MEDIUM : AI_MORALE_LEVEL_HIGH;
}

// 0x005DEC00 (:3189)
ai_control_levels ai_system::get_district_control_level() const
{
    district* dp = district_get_from_pos(np->pos);
    const float max_value = dp->get_control_max();
    if (max_value <= 0.0f)
    {
        return AI_CONTROL_LEVEL_NONE;
    }
    const float fraction = dp->get_control() / max_value;
    if (fraction <= AI_LOW_CONTROL_CUTOFF)
    {
        return AI_CONTROL_LEVEL_LOW;
    }
    return fraction < AI_HIGH_CONTROL_CUTOFF ? AI_CONTROL_LEVEL_MEDIUM : AI_CONTROL_LEVEL_HIGH;
}

// 0x005DEC80 (:3207)
bool ai_system_consider_player_famous()
{
    return Ai_fame_num_missions_completed <= mission_get_num_completed_missions();
}

// ---- movement

// 0x0064EF00 (:3222)
bool ai_system::point_is_dangerous_to_path_to(const vector& pos) const
{
    const gfm_orders* orders = get_orders();
    if (orders != nullptr && (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND || orders->get_orders_type() == GFM_ORDERS_TYPE_ROADBLOCK))
    {
        const object* target = orders->get_target_object();
        if (target != nullptr && target->get_obj_type() == OT_HUMAN)
        {
            return false;
        }
    }
    const human_teams team = np->current_team;
    if ((int)team < 0 || team == HUMAN_TEAM_CIVILIAN || (int)team > 3)
    {
        if (restricted_area_in_any_region(pos, false) && !restricted_area_in_any_region(np->pos, false))
        {
            return true;
        }
    }
    const rfg_mover* building = perception.get_nearest_unsafe_building();
    if (building != nullptr)
    {
        const float dist = building->flags.game_destroyed == 0 ? UNSAFE_BUILDING_STANDING_DIST : UNSAFE_BUILDING_DESTROYED_DIST;
        const float dist_sq = dist * dist;
        if (ai_building_find_bbox_dist_squared(pos, building) < dist_sq && dist_sq <= ai_building_find_bbox_dist_squared(np->pos, building))
        {
            return true;
        }
    }
    return false;
}

// 0x006B5450 (:3280)
ai_pathfind_results ai_system::pathfind_to(human* target, human_movement_states m_state, human_stances stance, ai_pathfind_priorities priority, const vector* eff_pos)
{
    if (target == nullptr)
    {
        return AI_PF_FAILURE_DEST_INVALID;
    }
    if (eff_pos == nullptr)
    {
        eff_pos = &target->pos;
    }
    uint64_t hint = 0xFFFFFFFFFFFFFFFFull;
    const nav_cell* ncp = nav_cell_from_handle(target->pf.last_visited_nav_cell_handle);
    if (ncp != nullptr && nav_cell_is_point_in_cell_column(ncp, target->pos))
    {
        hint = target->pf.last_visited_nav_cell_handle;
    }
    nav_cell_detour_remove_request(target->nav_cell_detour_request_handle);
    const ai_pathfind_results result = pathfind_to(*eff_pos, m_state, stance, priority, hint, false);
    target->nav_cell_detour_request_handle = nav_cell_detour_add_request(target->get_handle(), -1);
    return result;
}

// 0x0068F040 (:3336)
ai_pathfind_results ai_system::pathfind_to(const vector& pos, human_movement_states m_state, human_stances stance, ai_pathfind_priorities priority, uint64_t end_nav_cell_hint, bool disable_smoothing)
{
    if (human_in_vehicle(np) != nullptr)
    {
        return AI_PF_FAILURE_IN_VEHICLE;
    }
    if (!pathfind_timer.elapsed())
    {
        return AI_PF_NOT_READY;
    }
    if (np->char_inst != nullptr && (np->char_inst->is_playing_action(PF_BLOCKING_ALIAS_0) || np->char_inst->is_playing_action(PF_BLOCKING_ALIAS_1)))
    {
        return AI_PF_NOT_READY;
    }
    if (human_is_ragdolled(np) || flags.stunned)
    {
        return AI_PF_RAGDOLLED;
    }
    if (human_combat_cover_is_using_node(np))
    {
        human_combat_cover_abandon_node(np);
    }
    if (np->hflags.override_default_anim_state != 0)
    {
        human_override_default_animation_state(np, false);
    }
    if (np->cur_stance == HST_PRONE)
    {
        human_override_default_animation_state(np, false);
        human_set_stance(np, HST_CROUCH);
    }
    if (stance == HST_PRONE || (stance == HST_CURRENT && np->cur_stance == HST_PRONE))
    {
        stance = HST_CROUCH;
    }
    if (is_pathing_to(pos, STRICT_MOVING_TO_XZ_TOLERANCE_SQ, 1.0f))
    {
        human_set_state(np, HS_STANDARD, m_state, -1.0f);
        human_set_stance(np, stance);
        return AI_PF_SUCCESS;
    }
    if (point_is_dangerous_to_path_to(pos))
    {
        return AI_PF_FAILURE_DANGER;
    }
    stop_moving();
    human_set_stance(np, stance);
    if (vec_dist_squared(np->pos, pos) < PF_MIN_DIST * PF_MIN_DIST)
    {
        return AI_PF_SUCCESS;
    }
    if (!Ai_pathfind_rate_limiter.request_immediate(np, priority))
    {
        return AI_PF_NOT_READY;
    }
    pathfind_timer.set_rand(900, 1100); // ms
    pf_failure_reason = AI_PF_FAILURE_NO_PATH;
    np->hflags.disable_path_smoothing_for_request = disable_smoothing;
    const bool found = human_pathfind_to(np, pos, HS_STANDARD, m_state, end_nav_cell_hint, false);
    np->hflags.disable_path_smoothing_for_request = 0;
    if (!found)
    {
        return pf_failure_reason;
    }
    if ((int)m_state > HMS_RUN)
    {
        np->char_inst->stop_action(PF_SPRINT_STOP_ALIAS, 3, true);
    }
    return AI_PF_SUCCESS;
}

// 0x005DC980 (.h:395)
bool ai_system::can_pathfind() const
{
    return pathfind_timer.elapsed();
}

// 0x005DC990 (.h:396)
void ai_system::set_pathfind_delay(int msec)
{
    pathfind_timer.set(msec);
}

// 0x005DC9A0 (.h:397)
void ai_system::set_pathfind_delay_rand(int min_msec, int max_msec)
{
    pathfind_timer.set_rand(min_msec, max_msec);
}

// 0x0064F020 (:3459)
ai_pathfind_results ai_system::set_direct_movement(const vector& vec, human_movement_states m_state, human_stances stance, float move_speed)
{
    vector dir;
    dir = vec.copy_flatten_normalize_safe();
    return set_direct_movement_fast(dir, m_state, stance, move_speed);
}

// 0x0060B880 (:3464)
ai_pathfind_results ai_system::set_direct_movement_fast(const vector& dir, human_movement_states m_state, human_stances stance, float move_speed)
{
    if (human_in_vehicle(np) != nullptr)
    {
        return AI_PF_FAILURE_IN_VEHICLE;
    }
    vector steering = dir;
    if (fabsf(steering.y) >= 0.005f)
    {
        steering.flatten_normalize_safe();
    }
    if (human_is_ragdolled(np) || flags.stunned)
    {
        return AI_PF_RAGDOLLED;
    }
    if (stance == HST_PRONE || (stance == HST_CURRENT && np->cur_stance == HST_PRONE))
    {
        stance = HST_CROUCH;
    }
    const weapon* wp = np->get_equipped_weapon();
    if (wp != nullptr)
    {
        move_speed *= wp->w_info->npc_move_speed_multiplier;
    }
    stop_moving();
    human_set_stance(np, stance);
    human_set_state(np, HS_STANDARD, m_state, move_speed);
    np->steering_vector = steering;
    if ((int)m_state > HMS_RUN)
    {
        np->char_inst->stop_action(PF_SPRINT_STOP_ALIAS, 3, true);
    }
    return AI_PF_SUCCESS;
}

// 0x0060BA10 (:3526). Read from the asm: the decompile shifts the stack arguments by one and hides target_navpoint.
ai_pathfind_results ai_system::set_direct_movement_to_pos_internal(const vector& target_pos, uint64_t target_navcell, unsigned int target_navpoint, human_movement_states m_state, human_stances stance, bool disable_smoothing)
{
    if (human_in_vehicle(np) != nullptr)
    {
        return AI_PF_FAILURE_IN_VEHICLE;
    }
    const bool has_navcell = (unsigned int)target_navcell != 0xFFFFFFFF || (unsigned int)(target_navcell >> 32) != 0xFFFFFFFF;
    const bool has_navpoint = target_navpoint != 0xFFFFFFFF;
    if (human_is_ragdolled(np) || flags.stunned)
    {
        return AI_PF_RAGDOLLED;
    }
    if (stance == HST_PRONE || (stance == HST_CURRENT && np->cur_stance == HST_PRONE))
    {
        stance = HST_CROUCH;
    }
    stop_moving();
    human_set_stance(np, stance);
    vector dir;
    if (vec_normalized_dir(dir, target_pos, np->pos) < PF_MIN_DIST)
    {
        return AI_PF_SUCCESS;
    }
    np->pf.action = HPA_STARTUP;
    np->pf.prev_state = HMS_IDLE;
    if (has_navcell)
    {
        np->pf.eventual_goal_pos = nav_cell_force_point_to_cell(target_navcell, target_pos);
    }
    else
    {
        np->pf.eventual_goal_pos = target_pos;
    }
    np->pf.path_repulsors_collection_time.invalidate();
    np->pf.dist_until_recheck = 0.0f;
    np->hflags.recalculate_at_node = 0;
    np->hflags.on_mover = has_navpoint;
    np->pf.dest_check_timer.set(0);
    path_node* node = nullptr;
    if (!np->pf.eventual_goal_pos.is_null())
    {
        node = bb_pf_get_free_path_node(np->pf.eventual_goal_pos, target_navcell, target_navpoint);
    }
    if (node == nullptr)
    {
        return AI_PF_FAILURE_NO_PATH;
    }
    node->normal = dir;
    node->distance_left = 0.0f;
    if (np->pf.path == nullptr)
    {
        node->next = node;
        node->prev = node;
        np->pf.path = node;
    }
    else
    {
        node->prev = np->pf.path->prev;
        node->next = np->pf.path;
        np->pf.path->prev->next = node;
        np->pf.path->prev = node;
    }
    if (!disable_smoothing && np->hflags.disable_all_path_smoothing == 0)
    {
        np->pf.flags.smoothing = rfg_pf_calculate_smoothing_info(np);
    }
    else
    {
        np->pf.flags.smoothing = 0;
    }
    human_set_state(np, HS_STANDARD, m_state, -1.0f);
    human_set_movement_mode(np, HMM_PATHFINDING, HMSM_NONE);
    if ((int)m_state > HMS_RUN)
    {
        np->char_inst->stop_action(PF_SPRINT_STOP_ALIAS, 3, true);
    }
    return AI_PF_SUCCESS;
}

// 0x0060BD30 (:3613)
ai_pathfind_results ai_system::set_direct_movement_to_pos(const vector& target_pos, uint64_t target_navcell, human_movement_states m_state, human_stances stance, bool disable_smoothing)
{
    return set_direct_movement_to_pos_internal(target_pos, target_navcell, 0xFFFFFFFF, m_state, stance, disable_smoothing);
}

// 0x005DECA0 (:3620)
void ai_system::stop_moving()
{
    if (human_in_vehicle_interaction(np, nullptr))
    {
        return;
    }
    np->hflags.recalculate_at_node = 0;
    np->hflags.jumping_from_building = 0;
    if (np->pf.path != nullptr)
    {
        bb_pf_free_path(&np->pf.path);
    }
    const human_move_modes mode = np->movement_mode;
    if (mode == HMM_PATHFINDING || mode == HMM_NONE)
    {
        np->steering_vector = ZERO_VECTOR;
        np->move_speed = -1.0f;
        if (np->cur_state != HS_COVER)
        {
            human_set_movement_mode(np, HMM_NONE, HMSM_NONE);
            if (np != nullptr && !np->is_killed())
            {
                human_set_state(np, HS_STANDARD, HMS_IDLE, -1.0f);
            }
        }
    }
    else if (mode == HMM_KNOCKDOWN)
    {
        np->steering_vector = ZERO_VECTOR;
        np->move_speed = -1.0f;
        if (np != nullptr && !np->is_killed())
        {
            human_set_state(np, HS_STANDARD, HMS_IDLE, -1.0f);
        }
    }
    if (np->char_inst->is_playing_action(STOP_MOVING_ALIAS))
    {
        np->char_inst->stop_action(STOP_MOVING_ALIAS, 3, true);
    }
}

// 0x0060BD60 (:3662)
bool ai_system::register_electrocuted(int elec_ms)
{
    if (np->hflags.disallow_flinches_and_ragdolls != 0)
    {
        return true;
    }
    electrocution_timer.set(elec_ms);
    if (can_play_stun_animation() != CAN_PLAY_STUN_YES)
    {
        return false;
    }
    enter_stunned_state(ANIMATION_STATE_ELECTRIC_SHOCK, elec_ms, ANIM_ALIAS_NONE);
    return true;
}

// ---- animation sets

// 0x005DEE40 (:3711)
anim_set_variants ai_system::get_base_anim_set() const
{
    if (np->char_inst != nullptr && np->char_inst->vp != nullptr)
    {
        return vanim_get_base_animation_set_variant(np->char_inst->vp);
    }
    return ANIM_SET_VARIANT_NONE;
}

// 0x005DEE70 (:3718)
anim_set_variants ai_system::get_temp_anim_set() const
{
    if (np->char_inst != nullptr && np->char_inst->vp != nullptr)
    {
        return vanim_get_temp_animation_set_variant(np->char_inst->vp);
    }
    return ANIM_SET_VARIANT_NONE;
}

// 0x005DEEA0 (:3734)
bool ai_system::set_base_anim_set(anim_set_variants new_variant, bool civilian_only, bool force)
{
    if (AI_ANIM_SET_VARIANT_PRIORITIES[new_variant] <= 0)
    {
        return false;
    }
    if (civilian_only && !human_team_is_non_combatant(np))
    {
        return false;
    }
    if (np->char_inst == nullptr || np->char_inst->vp == nullptr)
    {
        return false;
    }
    v_animate* vp = np->char_inst->vp;
    const anim_set_variants current = vanim_get_base_animation_set_variant(vp);
    if (current == new_variant)
    {
        return true;
    }
    if (force || new_variant == ANIM_SET_VARIANT_NONE || current == ANIM_SET_VARIANT_NONE
        || AI_ANIM_SET_VARIANT_PRIORITIES[current] < AI_ANIM_SET_VARIANT_PRIORITIES[new_variant])
    {
        vanim_set_base_animation_set_variant(vp, new_variant);
        return vanim_get_base_animation_set_variant(vp) == new_variant;
    }
    return false;
}

// 0x005DEF40 (:3777)
bool ai_system::set_temp_anim_set(anim_set_variants new_variant)
{
    if (np->char_inst == nullptr || np->char_inst->vp == nullptr)
    {
        return false;
    }
    vanim_set_temp_animation_set_variant(np->char_inst->vp, new_variant);
    return vanim_get_temp_animation_set_variant(np->char_inst->vp) == new_variant;
}

// 0x0064F060 (:3789)
void ai_system::update_on_fire()
{
    if (np->hflags.on_fire != 0 || np->char_inst == nullptr || np->char_inst->vp == nullptr)
    {
        return;
    }
    if (vanim_get_temp_animation_set_variant(np->char_inst->vp) != ANIM_SET_VARIANT_ON_FIRE)
    {
        return;
    }
    const ai_goal* goal = planner.get_current_goal();
    if (goal != nullptr && goal->pursues_condition(ai_plan_conditions(ON_FIRE_PLAN_CONDITION_BIT)))
    {
        return;
    }
    set_temp_anim_set(ANIM_SET_VARIANT_NONE);
}

// 0x0060BDC0 (:3800)
void ai_system::exit_on_fire()
{
    if (np->cur_stance != HST_STAND)
    {
        stun(ANIMATION_ACTION_BK_GETUP, ANIM_ALIAS_NONE, 1.0f, EXIT_ON_FIRE_ANIM_FLAGS);
        human_set_stance(np, HST_STAND);
    }
    if (np->char_inst != nullptr && np->char_inst->vp != nullptr)
    {
        vanim_set_temp_animation_set_variant(np->char_inst->vp, ANIM_SET_VARIANT_NONE);
        vanim_get_temp_animation_set_variant(np->char_inst->vp);
    }
}

// 0x0060BE40 (:3822)
bool ai_system::play_explosion_flinch(float impulse_mag, const vector& impulse_dir)
{
    if (np->char_inst->is_playing_action(STUN_STOP_ACTION_MASK) || can_play_stun_animation() != CAN_PLAY_STUN_YES)
    {
        return false;
    }
    const animation_action anim_action = get_explosion_flinch_animation(impulse_mag, impulse_dir);
    if (anim_action == ANIMATION_ACTION_NONE)
    {
        return false;
    }
    stun(anim_action, ANIM_ALIAS_EXPLOSION_FLINCH, fl_rand_range(0.8f, 1.2f), STUN_ANIM_FLAGS);
    return true;
}

// 0x0060BEE0 (:3851)
bool ai_system::play_explosion_knockdown(float impulse_mag, const vector& impulse_dir, const vector& angular_impulse, bool force_ragdoll)
{
    const float up_dot = impulse_dir.x * UP_VECTOR.x + UP_VECTOR.y * impulse_dir.y + impulse_dir.z * UP_VECTOR.z;
    const can_play_stun_results can_play = can_play_stun_animation();
    if (can_play == CAN_PLAY_STUN_NO)
    {
        return false;
    }
    animation_action anim_action = ANIMATION_ACTION_NONE;
    if (can_play == CAN_PLAY_STUN_YES && fabsf(up_dot) <= COS_45_DEGREES && !force_ragdoll)
    {
        float rot_angle;
        anim_action = get_explosion_knockdown_animation(impulse_mag, impulse_dir, &rot_angle);
    }
    stun(anim_action, ANIM_ALIAS_EXPLOSION_KNOCKDOWN, fl_rand_range(0.85f, 1.15f), STUN_ANIM_FLAGS);
    if (anim_action == ANIMATION_ACTION_NONE)
    {
        vector impulse;
        impulse.x = impulse_dir.x * impulse_mag;
        impulse.y = impulse_mag * impulse_dir.y;
        impulse.z = impulse_dir.z * impulse_mag;
        human_combat_apply_explosion_ragdoll_impulse(np, impulse, angular_impulse, nullptr);
        return true;
    }
    float ragdoll_time = vanim_get_trigger_time(np->char_inst->vp, Trigger_ragdoll_stop, KNOCKDOWN_ALIAS, 0);
    if (ragdoll_time <= 0.0f)
    {
        ragdoll_time = vanim_get_anim_length(np->char_inst->vp, KNOCKDOWN_ALIAS);
    }
    human_ragdoll_on_impact(np, ragdoll_time, 0.1f, false);
    return true;
}

// 0x0060C0E0 (:3899)
void ai_system::start_stumble(animation_action stumble_anim)
{
    if (stumble_anim == ANIMATION_ACTION_NONE)
    {
        return;
    }
    const ai_system* ai = np->get_ai();
    if (ai->custom_behavior != AI_SB_TRAPPED_IN_JAIL || ai->custom_behavior_value != 0)
    {
        np->char_inst->stop_action(STUN_STOP_ACTION_MASK, 3, false);
        flags.stunned = true;
        planner.stun();
        stunned_animation = stumble_anim;
    }
}

// 0x006BBBC0 (:3925)
void ai_system::register_shove(const human* attacker)
{
    perception.register_hit(attacker, 100.0f, nullptr);
    stun(ANIMATION_ACTION_REACT_SHOVED, ANIM_ALIAS_NONE, 1.0f, STUN_ANIM_FLAGS);
    vector to_attacker;
    to_attacker.x = attacker->pos.x - np->pos.x;
    to_attacker.y = attacker->pos.y - np->pos.y;
    to_attacker.z = attacker->pos.z - np->pos.z;
    vector flat;
    flat = to_attacker.copy_flatten_normalize();
    matrix dest_orient;
    dest_orient.make(flat);
    np->char_inst->set_action_dest_orient(np->orient, dest_orient, SHOVE_ALIAS, 0.25f);
    np->play_voice_line(VOICE_LINE_CIVILIAN_FLEE_EDF, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
}

// 0x005DEF90 (:3941)
void ai_system::register_hit_by_car(const vehicle* vp, const human* driver)
{
}

// 0x0060C140 (:3948)
void ai_system::notify_vehicle_disabled()
{
    flags.stunned = true;
    planner.stun();
    stunned_timer.set(2000); // ms
}

// 0x0060C170 (:3959)
void ai_system::start_slave_state(const human* master)
{
    if (master_handle == 0xFFFFFFFF)
    {
        master_handle = master->get_handle();
        np->char_inst->stop_action(STUN_STOP_ACTION_MASK, 3, false);
        flags.stunned = true;
        planner.stun();
        human_override_default_animation_state(np, true);
    }
}

// 0x0060C1D0 (:3966)
void ai_system::end_slave_state(const human* master)
{
    if (master_handle == master->get_handle())
    {
        end_slave_state_internal();
    }
}

// 0x005DEFA0 (:3971)
void ai_system::end_slave_state_internal()
{
    master_handle = 0xFFFFFFFF;
    human_override_default_animation_state(np, false);
}

// 0x005DC9B0 (.h:447)
bool ai_system::is_slave() const
{
    return master_handle != 0xFFFFFFFF;
}

// 0x00A05FB0 (.h:450)
int ai_system::get_stunned_animation() const
{
    return stunned_animation;
}

// ---- firing

// 0x005DC9C0 (.h:456)
void ai_system::set_desired_burst_size(int num_shots)
{
    if (num_shots > 0)
    {
        desired_burst_size = num_shots;
    }
}

// 0x005DC9E0 (.h:457)
int ai_system::get_desired_burst_size() const
{
    return desired_burst_size;
}

// 0x005DEFC0 (:3988)
void ai_system::fire_weapon()
{
    flags.fire_weapon_this_frame = true;
}

// 0x005DEFD0 (:3993)
void ai_system::fire_secondary_weapon()
{
    flags.alternate_fire_this_frame = true;
}

// 0x005DEFE0 (:3998)
void ai_system::stop_firing()
{
    desired_burst_size = 0;
}

// 0x005DEFF0 (:4009)
void ai_system::do_fire_frame()
{
    const weapon* wp = human_weapons_get_weapon(np);
    if (wp != nullptr)
    {
        const weapon_ammo_type ammo = wp->w_info->ammo_type;
        if (ammo == WEAPON_AMMO_TYPE_BULLET || ammo == WEAPON_AMMO_TYPE_PROJECTILE || ammo == WEAPON_AMMO_TYPE_ELECTRICITY
            || ammo == WEAPON_AMMO_TYPE_REPAIR || ammo == WEAPON_AMMO_TYPE_BLANKS || ammo == WEAPON_AMMO_TYPE_FLAME)
        {
            if (flags.fire_weapon_this_frame)
            {
                human_weapons_firearm_pull_trigger(np);
            }
            else
            {
                human_weapons_firearm_release_trigger(np);
            }
        }
    }
    flags.fire_weapon_this_frame = false;
    turret* tp = human_combat_get_manned_turret(np);
    if (tp != nullptr && tp->get_secondary_weapon_inst() != nullptr)
    {
        if (flags.alternate_fire_this_frame)
        {
            human_weapons_firearm_pull_secondary_weapon_trigger(np);
        }
        else
        {
            human_weapons_firearm_release_secondary_weapon_trigger(np);
        }
        flags.alternate_fire_this_frame = false;
    }
}

// ---- reactions

// 0x005DF0C0 (:4063)
bool ai_system::should_react(unsigned int reason) const
{
    if (reaction_info.time_left.elapsed())
    {
        return false;
    }
    return (reaction_info.reason & reason) != 0;
}

// Sets up a reaction; a negative time picks 2 to 3 seconds.
static void start_reaction(ai_reaction_info& info, unsigned int handle, ai_belief_types belief_type, unsigned int reason, int time)
{
    info.belief_type = belief_type;
    info.target_handle = handle;
    info.reason = reason;
    if (time < 0)
    {
        time = int_rand_range(2000, 3000);
    }
    info.time_left.set(time);
}

// 0x005DF0F0 (:4087)
bool ai_system::request_reaction(const object* obj, unsigned int reason, int time)
{
    if (reason == 0 || obj == nullptr)
    {
        return false;
    }
    start_reaction(reaction_info, obj->get_handle(), AI_BELIEF_UNDEFINED, reason, time);
    return true;
}

// 0x005DF150 (:4101)
bool ai_system::request_reaction(unsigned int handle, unsigned int reason, int time)
{
    if (reason == 0 || handle == 0xFFFFFFFF)
    {
        return false;
    }
    start_reaction(reaction_info, handle, AI_BELIEF_UNDEFINED, reason, time);
    return true;
}

// 0x005DF1B0 (:4115)
bool ai_system::request_reaction(const ai_belief* belief, unsigned int reason, int time)
{
    if (reason == 0 || belief == nullptr)
    {
        return false;
    }
    start_reaction(reaction_info, belief->handle, belief->get_type(), reason, time);
    return true;
}

// 0x005DF210 (:4133)
bool ai_system::request_reaction_bomb(const projectile* proj, unsigned int reason, int time)
{
    if (reason == 0 || proj == nullptr)
    {
        return false;
    }
    start_reaction(reaction_info, proj->get_handle(), AI_BELIEF_PROJECTILE, reason, time);
    return true;
}

// 0x005DF270 (:4152)
void ai_system::end_reaction(unsigned int reason)
{
    if (reason == 0)
    {
        return;
    }
    reaction_info.reason &= ~reason;
    if (reaction_info.reason == 0)
    {
        reaction_info.reset();
    }
}

// ---- orders

// 0x0060C200 (:4172)
human* ai_system::get_human_target() const
{
    object* target = World.object_from_handle(get_target_handle());
    if (target == nullptr || target->get_obj_type() != OT_HUMAN || target->is_killed())
    {
        return nullptr;
    }
    return (human*)target;
}

// 0x0060C240 (:4188)
object* ai_system::get_defend_object() const
{
    const gfm_orders* orders = get_orders();
    if (orders != nullptr && (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND || orders->get_orders_type() == GFM_ORDERS_TYPE_ROADBLOCK))
    {
        return orders->get_target_object();
    }
    return nullptr;
}

// 0x0060C290 (:4217)
bool ai_system::outside_defensive_perimeter(const vector& pos, bool force_eval, const float* buffer_dist) const
{
    if (ZERO_VECTOR.x == pos.x && ZERO_VECTOR.y == pos.y && ZERO_VECTOR.z == pos.z)
    {
        return true;
    }
    if (!force_eval && np->current_team == HUMAN_TEAM_MARAUDER && perception.get_first_belief(AI_BELIEF_ENEMY_POSITION) != nullptr)
    {
        return false;
    }
    const gfm_orders* orders = get_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_DEFEND)
    {
        return false;
    }
    object* target = orders->get_target_object();
    if (target == nullptr)
    {
        return false;
    }
    if (target->get_obj_type() == OT_HUMAN)
    {
        const float radius = buffer_dist != nullptr ? *buffer_dist + DEFEND_HUMAN_RADIUS : DEFEND_HUMAN_RADIUS;
        // Compared in double, as rfg.exe's x87 code does.
        const double radius_sq = (double)radius * (double)radius;
        return (double)vec_dist_squared(target->pos, pos) > radius_sq;
    }
    if (np->npc_flags.activity_related != 0)
    {
        bool outside;
        if (activity_outside_defensive_perimeter(np, pos, outside, buffer_dist))
        {
            return outside;
        }
    }
    float radius = target->get_obj_type() == OT_BOUNDING_BOX ? 0.0f : DEFEND_OBJECT_RADIUS;
    if (buffer_dist != nullptr)
    {
        radius += *buffer_dist;
    }
    float dist_sq;
    if (target->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        // Original bug kept: the subtype is read for any object type, not only for movers.
        dist_sq = ai_building_find_bbox_dist_squared(pos, (rfg_mover*)target);
    }
    else
    {
        vector bmin, bmax;
        target->get_local_bbox(bmin, bmax);
        vector local_pos = pos;
        vec_change_to_local_coords(local_pos, target->pos, target->orient);
        dist_sq = bbox_find_distance_squared(bmin, bmax, local_pos);
    }
    return dist_sq > radius * radius;
}

// 0x00674580 (:4294)
const object_patrol* ai_system::get_patrol_object() const
{
    const gfm_orders* orders = get_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_PATROL)
    {
        return nullptr;
    }
    object* target = orders->get_target_object();
    return (target != nullptr && target->get_obj_type() == OT_PATROL) ? (const object_patrol*)target : nullptr;
}

// 0x00674600 (:4309)
unsigned int ai_system::get_convoy_handle() const
{
    const gfm_orders* orders = get_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_CONVOY)
    {
        return 0xFFFFFFFF;
    }
    return orders->get_target_object_handle();
}

// 0x00674670 (:4326)
object_convoy* ai_system::get_convoy_object() const
{
    const gfm_orders* orders = get_orders();
    if (orders == nullptr || orders->get_orders_type() != GFM_ORDERS_TYPE_CONVOY)
    {
        return nullptr;
    }
    object* target = orders->get_target_object();
    return (target != nullptr && target->get_obj_type() == OT_CONVOY) ? (object_convoy*)target : nullptr;
}

// 0x006746F0 (:4343)
bool ai_system::is_in_convoy_formation() const
{
    const object_convoy* convoy = get_convoy_object();
    return convoy != nullptr && convoy->in_convoy_formation(squad_handle);
}

// 0x0060C4F0 (:4358)
const object_patrol* ai_system::get_cached_patrol_object() const
{
    return (object_patrol*)typed_object_from_handle(desired_destination_object, OT_PATROL);
}

// 0x0064F170 (:4380)
bool ai_system::has_patrol_type_orders() const
{
    const gfm_orders_types type = get_orders_type();
    return type == GFM_ORDERS_TYPE_PATROL || type == GFM_ORDERS_TYPE_CONVOY || type == GFM_ORDERS_TYPE_COURIER;
}

// 0x0064F100 (:4365)
const patrol_node_handle* ai_system::get_patrol_node() const
{
    if (!has_patrol_type_orders())
    {
        return nullptr;
    }
    const object_squad* squad = squad_from_handle(squad_handle);
    return squad == nullptr ? nullptr : &squad->current_node_handle;
}

// 0x0064F1C0 (:4388)
void ai_system::update_patrol_node(const patrol_node_handle* new_node)
{
    if (new_node == nullptr || !has_patrol_type_orders())
    {
        return;
    }
    object_squad* squad = squad_from_handle(squad_handle);
    if (squad != nullptr && np == squad->get_leader())
    {
        squad->current_node_handle = *new_node;
    }
}

// 0x0060C520 (:4407)
void ai_system::update_patrol_speed(vehicle* vp, const patrol_node_handle* new_node, const object_patrol* patrol)
{
    const navpoint* nav = (navpoint*)typed_object_from_handle(new_node->nav_handle, OT_NAVPOINT);
    desired_vehicle_speed = nav != nullptr ? nav->speed_limit : 10.0f; // m/s
    if (vp->vai.vai_flags.m_jumping != 0)
    {
        return;
    }
    if (desired_vehicle_speed > 0.0f)
    {
        vehicle_ai_set_speed_override(&vp->vai, desired_vehicle_speed);
    }
    else
    {
        vehicle_ai_clear_speed_override(&vp->vai);
    }
}

// 0x0060C5A0 (:4420)
void ai_system::update_road_usage(vehicle* vp, const patrol_node_handle* new_node)
{
    const navpoint* nav = (navpoint*)typed_object_from_handle(new_node->nav_handle, OT_NAVPOINT);
    if (nav != nullptr && vp != nullptr && nav->nav_flags.dont_follow_road == 0)
    {
        vehicle_ai_set_ignore_lanes(&vp->vai, nav->nav_flags.ignore_lanes != 0);
    }
}

// 0x005DF2B0 (:4447)
animation_action ai_system::get_explosion_flinch_animation(float impulse_mag, const vector& impulse_dir) const
{
    vector flat_dir;
    flat_dir = impulse_dir.copy_flatten_normalize_safe();
    const vector& fvec = np->orient.fvec;
    const vector& rvec = np->orient.rvec;
    const float forward_dot = fvec.y * flat_dir.y + fvec.x * flat_dir.x + fvec.z * flat_dir.z;
    const float right_dot = rvec.y * flat_dir.y + rvec.x * flat_dir.x + rvec.z * flat_dir.z;
    const timestamp& combat_ready = np->combat.combat_ready_timestamp;
    const bool ready = combat_ready.valid() && !combat_ready.elapsed();
    const bool crouching = np->cur_stance == HST_CROUCH;
    // Each ANIMATION_ACTION_EXPLOSION_FLINCH_* has its large form 4 further on.
    const int large = 0.0f < impulse_mag ? 4 : 0;

    if (forward_dot >= COS_45_DEGREES)
    {
        if (crouching)
        {
            return ANIMATION_ACTION_CROUCH_FLINCH_BACK;
        }
        return ready ? (animation_action)(ANIMATION_ACTION_EXPLOSION_FLINCH_BACK + large) : ANIMATION_ACTION_EXPLOSION_DUCK_BACK;
    }
    if (forward_dot <= -COS_45_DEGREES)
    {
        if (crouching)
        {
            return ANIMATION_ACTION_CROUCH_FLINCH_FRONT;
        }
        return ready ? (animation_action)(ANIMATION_ACTION_EXPLOSION_FLINCH_FRONT + large) : ANIMATION_ACTION_EXPLOSION_DUCK_FRONT;
    }
    if (right_dot <= 0.0f)
    {
        if (crouching)
        {
            return ANIMATION_ACTION_CROUCH_FLINCH_RIGHT;
        }
        return ready ? (animation_action)(ANIMATION_ACTION_EXPLOSION_FLINCH_RIGHT + large) : ANIMATION_ACTION_EXPLOSION_DUCK_RIGHT;
    }
    if (crouching)
    {
        return ANIMATION_ACTION_CROUCH_FLINCH_LEFT;
    }
    return ready ? (animation_action)(ANIMATION_ACTION_EXPLOSION_FLINCH_LEFT + large) : ANIMATION_ACTION_EXPLOSION_DUCK_LEFT;
}

// 0x005DF4A0 (:4506)
animation_action ai_system::get_explosion_knockdown_animation(float impulse_mag, const vector& impulse_dir, float* rot_angle) const
{
    vector flat_dir;
    flat_dir = impulse_dir.copy_flatten_normalize_safe();
    const vector& fvec = np->orient.fvec;
    const vector& rvec = np->orient.rvec;
    const float forward_dot = fvec.y * flat_dir.y + flat_dir.x * fvec.x + fvec.z * flat_dir.z;
    animation_action action;
    float angle;
    if (forward_dot >= COS_45_DEGREES)
    {
        angle = 0.0f;
        action = ANIMATION_ACTION_EXPLOSION_KNOCKDOWN_BACK;
    }
    else if (forward_dot <= -COS_45_DEGREES)
    {
        angle = PI;
        action = ANIMATION_ACTION_EXPLOSION_KNOCKDOWN_FRONT;
    }
    else if (rvec.y * flat_dir.y + rvec.x * flat_dir.x + rvec.z * flat_dir.z >= 0.0f)
    {
        angle = PI / 2.0f;
        action = ANIMATION_ACTION_EXPLOSION_KNOCKDOWN_LEFT;
    }
    else
    {
        angle = -PI / 2.0f;
        action = ANIMATION_ACTION_EXPLOSION_KNOCKDOWN_RIGHT;
    }
    if (rot_angle != nullptr)
    {
        *rot_angle = angle;
    }
    return action;
}

// 0x005DF5D0 (:4563)
void ai_system::yield_next_frame()
{
    // Skips every other frame, staggered by handle.
    if ((np->get_handle() & 1) == (unsigned int)(Framecount % 2))
    {
        flags.yield_next_frame = true;
    }
}

// 0x005DCA40 (.h:648)
bool ai_system::should_yield_frame()
{
    return flags.yield_next_frame;
}

// 0x005DCA50 (.h:649)
void ai_system::clear_yield_next_frame()
{
    flags.yield_next_frame = false;
}

// ---- the pool and team-wide changes

// 0x0064F260 (:4574)
ai_system* ai_system_create()
{
    return Ai_system_list.alloc(0);
}

// 0x00674710 (:4579)
void ai_system_release(ai_system* ai)
{
    Ai_system_list.free(ai);
}

// 0x005DF600 (:4589)
void ai_system_set_alert_level_green()
{
    Alert_level_set_to_green = true;
}

// 0x0064F270 (:4594)
void ai_system_process_alert_level_change()
{
    if (!Alert_level_set_to_green)
    {
        return;
    }
    Alert_level_set_to_green = false;
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; ++i)
    {
        npc* np = World.get_object<npc>(i);
        if (np->is_killed())
        {
            np->hflags.corpse_spotted = 1;
            continue;
        }
        const human* target = np->current_team == HUMAN_TEAM_EDF ? nullptr : human_combat_get_target(np);
        if (np->current_team == HUMAN_TEAM_EDF || (target != nullptr && target->current_team == HUMAN_TEAM_EDF))
        {
            ai_system* ai = np->get_ai();
            ai->perception.deprecate_all_enemy_beliefs();
            if (np->npc_flags.despawn_on_green_alert != 0)
            {
                np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
            }
            ai->planner.request_replan();
        }
    }
}

// 0x0064F350 (:4632)
void ai_system_process_team_disposition_change(human_teams team)
{
    const int num_npcs = World.get_num_objects<npc>();
    for (int i = 0; i < num_npcs; ++i)
    {
        npc* np = World.get_object<npc>(i);
        if (np->current_team == team)
        {
            ai_system* ai = np->get_ai();
            ai->perception.player_friendly_fire.reset(np);
            ai->planner.request_replan();
        }
    }
}
