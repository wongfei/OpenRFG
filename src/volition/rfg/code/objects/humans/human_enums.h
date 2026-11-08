// Original: volition/rfg/code/objects/humans/human_enums.h
//
// Human state enums: the state machine (human_states, human_movement_states), stance and movement modes, rotation,
// ragdoll state, blocked movement, foot plant and path actions. Placement inferred (the PDB records no inline code for
// human_enums.h).
#pragma once

enum human_states
{
    HS_CURRENT = -1,
    HS_STANDARD = 0,
    HS_JUMP = 1,
    HS_FALL = 2,
    HS_TURRET = 3,
    HS_DEAD = 4,
    HS_LADDER = 5,
    HS_PLAYER_PATHING = 6,
    HS_VEHICLE_ENTERING = 7,
    HS_VEHICLE_SITTING = 8,
    HS_VEHICLE_DRIVING = 9,
    HS_VEHICLE_EXITING = 10,
    HS_SPECTATOR = 11,
    HS_COVER = 12,
    HS_AMMO_BOX = 13,
    HS_MP_DEFUSE = 14,
    HS_ACTION_NODE = 15,
    HS_SATELLITE = 16,
    NUM_HUMAN_STATES = 17,
};

enum human_movement_states
{
    HMS_CURRENT = -1,
    HMS_IDLE = 0,
    HMS_WALK = 1,
    HMS_RUN = 2,
    HMS_SPRINT = 3,
    NUM_HUMAN_MOVEMENT_STATES = 4,
};

enum human_stances
{
    HST_CURRENT = -1,
    HST_STAND = 0,
    HST_CROUCH = 1,
    HST_PRONE = 2,
    NUM_HUMAN_STANCES = 3,
};

enum human_move_modes
{
    HMM_NONE = 0,
    HMM_DIRECT = 1,
    HMM_PATHFINDING = 2,
    HMM_PATHFINDING_TO_VEHICLE = 3,
    HMM_JUMP = 4,
    HMM_FALL = 5,
    HMM_FP_MOVE = 6,
    HMM_KNOCKDOWN = 7,
    HMM_SATELLITE = 8,
    NUM_HUMAN_MOVE_MODES = 9,
};

enum human_move_submodes
{
    HMSM_NONE = 0,
    HMSM_JUMP_START = 1,
    HMSM_JUMP_PRE = 2,
    HMSM_JUMP_MID = 3,
    HMSM_FALL_START = 4,
    HMSM_FALL_NORMAL = 5,
    HMSM_FALL_WORRY = 6,
    HMSM_FALL_PANIC = 7,
    HMSM_FALL_FREE = 8,
    HMSM_LAND = 9,
    HMSM_KNOCKDOWN_RAGDOLL = 10,
    HMSM_KNOCKDOWN_GETUP = 11,
    HMSM_KNOCKDOWN_TELEPORT = 12,
    HMSM_PTV_PATHFINDING = 13,
    HMSM_PTV_PAUSED = 14,
    HMSM_PTV_FINAL_ADJUSTMENT = 15,
    HMSM_COVER_RIGHT = 16,
    HMSM_COVER_RIGHT_EDGE = 17,
    HMSM_COVER_LEFT = 18,
    HMSM_COVER_LEFT_EDGE = 19,
    HMSM_COVER_RIGHT_LEAN = 20,
    HMSM_COVER_LEFT_LEAN = 21,
    HMSM_COVER_RIGHT_LEANUP = 22,
    HMSM_COVER_LEFT_LEANUP = 23,
    HMSM_COVER_RIGHT_BLINDFIRE = 24,
    HMSM_COVER_LEFT_BLINDFIRE = 25,
    HMSM_COVER_RIGHT_BLINDFIRE2 = 26,
    HMSM_COVER_LEFT_BLINDFIRE2 = 27,
    NUM_HUMAN_MOVE_SUBMODES = 28,
};

enum human_rotate_dir
{
    HUMAN_ROTATE_SHORTEST = 0,
    HUMAN_ROTATE_CLOCKWISE = 1,
    HUMAN_ROTATE_COUNTERCLOCKWISE = 2,
    HUMAN_ROTATE_USE_LIMITS = 3,
};

enum human_ragdoll_state
{
    HUMAN_RAGDOLL_STATE_NONE = 0,
    HUMAN_RAGDOLL_STATE_ACTIVE = 1,
    HUMAN_RAGDOLL_STATE_BLEND_OUT = 2,
    HUMAN_RAGDOLL_STATE_FLINCH = 3,
    HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT = 4,
    HUMAN_RAGDOLL_STATE_DEACTIVATED = 5,
    HUMAN_RAGDOLL_STATE_POSE_SAVED = 6,
    NUM_HUMAN_RAGDOLL_STATES = 7,
};

enum block_movement_types
{
    BLOCK_MOVEMENT_POS = 0,
    BLOCK_MOVEMENT_ORIENT = 1,
    BLOCK_MOVEMENT_POS_AND_ORIENT = 2,
};

enum foot_plant
{
    FP_LEFT_FOOT = 0,
    FP_RIGHT_FOOT = 1,
    FP_NEITHER_FOOT = 2,
    FP_BOTH_FEET = 3,
};

enum human_path_actions
{
    HPA_NORMAL = 0,
    HPA_STARTUP = 1,
    HPA_WAIT = 2,
    HPA_STEER = 3,
};
