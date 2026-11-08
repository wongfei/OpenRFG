// Original: volition/rfg/code/ai/ambient_edf_action_manager_enum.h
//
// The kinds of ambient EDF actions. Placement inferred.
#pragma once

enum ambient_edf_action_type
{
    INVALID_AMBIENT_EDF_ACTION_TYPE = -1,
    AMBIENT_EDF_ACTION_TYPE_NONE = 0,
    AMBIENT_EDF_ACTION_TYPE_DESPAWN = 1,
    AMBIENT_EDF_ACTION_TYPE_FLYER = 2,
    AMBIENT_EDF_ACTION_TYPE_DRIVE_BY = 3,
    AMBIENT_EDF_ACTION_TYPE_DUMP_CIV = 4,
    AMBIENT_EDF_ACTION_TYPE_HARASS_PLAYER = 5,
    NUM_AMBIENT_EDF_ACTION_TYPES = 6,
};
