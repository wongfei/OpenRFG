// Original: volition/rfg/code/ai/ai_types.h
// Implementation: volition/rfg/code/ai/ai_types.cpp
//
// AI enums and flag sets shared by ai_system and the planner (beliefs, oppression, pathfind results, persona and
// system flags, projectile reactions, scripted behaviours, vehicle seats, action results, control, morale and tension
// levels, flinch directions). Placement inferred.
#pragma once

enum ai_belief_types
{
    AI_BELIEF_UNDEFINED = -1,
    AI_BELIEF_ENEMY_EXISTS = 0,
    AI_BELIEF_ENEMY_POSITION = 1,
    AI_BELIEF_SHOT_RESULT = 2,
    AI_BELIEF_SHOT_AT = 3,
    AI_BELIEF_LAST_CONTACT = 4,
    AI_BELIEF_FIRST_CONTACT = 5,
    AI_BELIEF_CLOSEST_ALLY = 6,
    AI_BELIEF_CLOSEST_FRIENDLY = 7,
    AI_BELIEF_UNFRIENDLY = 8,
    AI_BELIEF_BEATDOWN_TARGET = 9,
    AI_BELIEF_CORPSE = 10,
    AI_BELIEF_FRIENDLY_FIRE = 11,
    AI_BELIEF_BUILDING = 12,
    AI_BELIEF_VEHICLE = 13,
    AI_BELIEF_TURRET = 14,
    AI_BELIEF_ALLY_ON_TURRET = 15,
    AI_BELIEF_PROJECTILE = 16,
    AI_BELIEF_HAZARD = 17,
    AI_BELIEF_MISC_SOUND = 18,
    AI_BELIEF_COMBAT_SOUND = 19,
    AI_BELIEF_BULLET_IMPACT = 20,
    AI_BELIEF_EXPLOSION = 21,
    AI_BELIEF_BUILDING_STRESS = 22,
    AI_BELIEF_LARGE_DESTRUCTION = 23,
    AI_BELIEF_DAMAGE_TAKEN = 24,
    AI_BELIEF_LAST_DAMAGE_TAKEN = 25,
    AI_BELIEF_SHOULD_ENGAGE_ENEMY = 26,
    NUM_AI_BELIEF_TYPES = 27,
};

enum ai_oppression_actions
{
    AI_OPPRESSION_NONE = 0,
    AI_OPPRESSION_EDF_ACTIONS = 1,
    AI_OPPRESSION_BEATDOWN = 1,
    AI_OPPRESSION_EXECUTE = 2,
    AI_OPPRESSION_EDF_REQUIRE_RIFLE = 3,
    AI_OPPRESSION_CHECK_ID = 3,
    AI_OPPRESSION_CIVILIAN_ACTIONS = 4,
    AI_OPPRESSION_GET_ON_KNEES = 4,
    AI_OPPRESSION_SHOW_ID = 5,
    AI_OPPRESSION_SHOOT_THIS_GUY = 6,
    NUM_AI_OPPRESSION_ACTIONS = 7,
};

enum ai_pathfind_results
{
    AI_PF_SUCCESS = 0,
    AI_PF_NOT_READY = 1,
    AI_PF_RAGDOLLED = 2,
    AI_PF_FAILURE_OVERRIDING_ANIMATION = 3,
    AI_PF_FAILURE_IN_VEHICLE = 4,
    AI_PF_FAILURE_DANGER = 5,
    AI_PF_FAILURE_NO_PATH = 6,
    AI_PF_FAILURE_START_INVALID = 7,
    AI_PF_FAILURE_DEST_INVALID = 8,
    NUM_AI_PATHFIND_RESULTS = 9,
};

enum ai_projectile_reactions
{
    AI_PROJECTILE_REACTION_NONE = 0,
    AI_PROJECTILE_REACTION_DODGE = 1,
    AI_PROJECTILE_REACTION_EVADE = 2,
    NUM_AI_PROJECTILE_REACTIONS = 3,
};

enum ai_scripting_behaviors
{
    AI_SB_PLAY_ANIMATION = -3,
    AI_SB_PLAY_ANIMATION_TO_PLAYER = -2,
    AI_SB_UNDEFINED = -1,
    AI_SB_NONE = 0,
    AI_SB_TRAPPED_IN_BUILDING = 1,
    AI_SB_TRAPPED_IN_JAIL = 2,
    AI_SB_MARAUDER_LOOKOUT = 3,
    AI_SB_PLANT_BOMB = 4,
    AI_SB_COOT_RAMBLE = 5,
    AI_SB_TECH_RESPONSE = 6,
    AI_SB_LOOK_AT_OBJECT = 7,
    AI_SB_TURN_ANIMATE = 8,
    AI_SB_FOLLOW_WOUNDED = 9,
    AI_SB_ACTIVITY_INTRO = 10,
    NUM_AI_SCRIPTING_BEHAVIORS = 11,
};

enum ai_vehicle_seat_types
{
    AI_VEHICLE_SEAT_TYPE_UNDEFINED = -1,
    AI_VEHICLE_SEAT_TYPE_DRIVER = 0,
    AI_VEHICLE_SEAT_TYPE_PASSENGER = 1,
    AI_VEHICLE_SEAT_TYPE_GUNNER = 2,
    NUM_AI_VEHICLE_SEAT_TYPES = 3,
};

enum ai_results
{
    AI_RESULT_FAILURE = 0,
    AI_RESULT_SUCCESS = 1,
    AI_RESULT_IN_PROGRESS = 2,
    NUM_AI_RESULTS = 3,
};

enum ai_control_levels
{
    AI_CONTROL_LEVEL_NONE = -1,
    AI_CONTROL_LEVEL_LOW = 0,
    AI_CONTROL_LEVEL_MEDIUM = 1,
    AI_CONTROL_LEVEL_HIGH = 2,
    NUM_AI_CONTROL_LEVELS = 3,
};

enum ai_morale_levels
{
    AI_MORALE_LEVEL_NONE = -1,
    AI_MORALE_LEVEL_LOW = 0,
    AI_MORALE_LEVEL_MEDIUM = 1,
    AI_MORALE_LEVEL_HIGH = 2,
    NUM_AI_MORALE_LEVELS = 3,
};

enum ai_tension_levels
{
    AI_TENSION_LEVEL_NONE = 0,
    AI_TENSION_LEVEL_LOW = 1,
    AI_TENSION_LEVEL_HIGH = 2,
    NUM_AI_TENSION_LEVELS = 3,
};

enum flinch_directions
{
    FLINCH_DIR_FRONT = 0,
    FLINCH_DIR_BACK = 1,
    FLINCH_DIR_RIGHT = 2,
    FLINCH_DIR_LEFT = 3,
    NUM_FLINCH_DIRECTIONS = 4,
};

// size 0x2
struct ai_persona_flags
{
    bool heavy_vehicle_no_fall_back : 1; // 0x00:0
    bool heavy_vehicle_always_cower : 1; // 0x00:1
    bool stand_up_to_edf : 1; // 0x00:2
    bool cower_from_edf : 1; // 0x00:3
    bool always_flee_edf : 1; // 0x00:4
    bool follow_player : 1; // 0x00:5
    bool flee_to_defensive_cover : 1; // 0x00:6
    bool flee_to_safe_building : 1; // 0x00:7
    bool prefer_flee_to_building : 1; // 0x01:0
    bool remote_charge_panic : 1; // 0x01:1
    bool on_fire_panic : 1; // 0x01:2
};

// size 0x2
struct ai_system_flags
{
    bool vehicle_use_road : 1; // 0x00:0
    bool destination_on_road : 1; // 0x00:1
    bool cannot_retreat : 1; // 0x00:2
    bool off_navmesh : 1; // 0x00:3
    bool reduced_processing : 1; // 0x00:4
    bool stunned : 1; // 0x00:5
    bool fire_weapon_this_frame : 1; // 0x00:6
    bool alternate_fire_this_frame : 1; // 0x00:7
    bool less_aggressive_melee : 1; // 0x01:0
    bool yield_next_frame : 1; // 0x01:1
    bool clear_desired_target_after_timeout : 1; // 0x01:2
    bool run_to_guard_post : 1; // 0x01:3
    bool played_stuck_line : 1; // 0x01:4
    bool use_alternate_target : 1; // 0x01:5
};

// 0x005DB4B0 (.h:343): a path started, one that is waiting, or one that cannot be had.
inline ai_results ai_pf_to_result(ai_pathfind_results result)
{
    if (result == AI_PF_SUCCESS)
    {
        return AI_RESULT_SUCCESS;
    }
    return result > AI_PF_RAGDOLLED ? AI_RESULT_FAILURE : AI_RESULT_IN_PROGRESS;
}
