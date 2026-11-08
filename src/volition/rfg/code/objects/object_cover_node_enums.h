// Original: volition/rfg/code/objects/object_cover_node_enums.h
//
// Cover node firing positions and stances, cover check results and the reasons a cover node is disabled. Placement
// inferred.
#pragma once

enum cover_node_firing_positions
{
    COVER_FIRE_UNDEFINED = -1,
    COVER_FIRE_LEANOUT = 0,
    COVER_FIRE_POPUP = 1,
    NUM_COVER_NODE_FIRING_POSITIONS = 2,
};

enum cover_node_stances
{
    COVER_STANCE_UNDEFINED = -1,
    COVER_STANCE_STAND_RIGHT = 0,
    COVER_STANCE_STAND_LEFT = 1,
    COVER_STANCE_CROUCH_RIGHT = 2,
    COVER_STANCE_CROUCH_LEFT = 3,
    NUM_COVER_NODE_STANCES = 4,
};

enum cover_check_results
{
    CCR_NO_COVER = 0,
    CCR_DEFENSIVE_ONLY = 1,
    CCR_OFFENSIVE = 2,
    NUM_COVER_CHECK_RESULTS = 3,
};

enum cover_node_disabled_reasons
{
    COVER_DISABLED_NONE = 0,
    COVER_DISABLED_TEMPORARY = 1,
    COVER_DISABLED_UNREACHABLE = 2,
    COVER_DISABLED_HITS = 3,
    COVER_DISABLED_DEATHS = 4,
    COVER_DISABLED_NO_STANCES = 5,
    COVER_DISABLED_NO_STEPOUTS = 6,
    COVER_DISABLED_DESTRUCTION = 7,
    NUM_COVER_NODE_DISABLED_REASONS = 8,
};
