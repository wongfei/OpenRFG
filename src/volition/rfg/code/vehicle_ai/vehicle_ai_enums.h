// Original: volition/rfg/code/vehicle_ai/vehicle_ai_enums.h
//
// Vehicle AI enums. Placement inferred.
#pragma once

enum vehicle_ai_state
{
    VS_AI_NONE = -1,
    VS_AI_IDLE = 0,
    VS_AI_PATHFIND = 1,
    NUM_VEHICLE_AI_STATES = 2,
};

enum vehicle_pathfind_results
{
    VPF_RESULT_SUCCESS = 0,
    VPF_RESULT_DELAYED = 1,
    VPF_RESULT_FAILURE_BAD_START = 2,
    VPF_RESULT_FAILURE_BAD_DESTINATION = 3,
    VPF_RESULT_FAILURE_NO_PATH = 4,
    NUM_VEHICLE_PATHFIND_RESULTS = 5,
};

enum path_outcome
{
    PR_UNKNOWN = 0,
    PR_OK = 1,
    PR_OUT_OF_CHUNK = 2,
    PR_ERROR_START_NAVCELL = 3,
    PR_ERROR_END_NAVCELL = 4,
    PR_ERROR_DEGENERATE = 5,
    PR_ERROR_DETOUR_HULL = 6,
    PR_ERROR_WATER = 7,
    PR_ERROR_UNKNOWN = 8,
};

enum clear_debris_states
{
    CDS_DONT = 0,
    CDS_DAMAGE1 = 1,
    CDS_DAMAGE2 = 2,
    CDS_DAMAGE3 = 3,
    CDS_DELETE = 4,
    NUM_CLEAR_DEBRIS_STATES = 5,
};
