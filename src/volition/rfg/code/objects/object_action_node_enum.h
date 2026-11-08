// Original: volition/rfg/code/objects/object_action_node_enum.h
//
// The action node enums (disable reasons, prop creation and removal times, snap targets) and the action node state of
// a human (human::action_node_data): the action nodes it uses and its state in them. Placement of the enums inferred.
#pragma once

#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

enum action_node_disabled_reason
{
    ANDR_POSITION = 0,
    ANDR_SCRIPTING = 1,
    ANDR_KIOSK = 2,
    ANDR_OBSTRUCTION = 3,
    ANDR_SAFE_HOUSE = 4,
    ANDR_MISSING_ANIMATION = 5,
    ANDR_ONE_TIME_USE_ONLY = 6,
    ANDR_DESTRUCTION = 7,
    ANDR_BUILDING = 8,
    ANDR_MISSION = 9,
    ANDR_ENTRY_POINT = 10,
    ANDR_PATHFINDING = 11,
    NUM_ACTION_NODE_DISABLED_REASONS = 12,
};

enum action_node_prop_creation_time
{
    INVALID_PROP_CREATION_TIME = -1,
    PROP_CREATION_TIME_NONE = 0,
    PROP_CREATION_TIME_PRE_ENTRY_ANIM = 1,
    PROP_CREATION_TIME_ENTRY_ANIM = 2,
    PROP_CREATION_TIME_POST_ENTRY_ANIM = 3,
    PROP_CREATION_TIME_ANIMATION_ACTION = 4,
    NUM_PROP_CREATION_TIMES = 5,
};

enum action_node_prop_removal_time
{
    INVALID_PROP_REMOVAL_TIME = -1,
    PROP_REMOVAL_TIME_NONE = 0,
    PROP_REMOVAL_TIME_NODE_COMPLETE = 1,
    PROP_REMOVAL_TIME_EXIT_ANIMATION = 2,
    PROP_REMOVAL_TIME_ANIMATION_ACTION = 3,
    PROP_REMOVAL_TIME_ENTRY_ANIM = 4,
    NUM_PROP_REMOVAL_TIMES = 5,
};

enum action_node_snap_target
{
    INVALID_ACTION_NODE_SNAP = -1,
    ACTION_NODE_SNAP_NONE = 0,
    ACTION_NODE_SNAP_PARENT = 1,
    ACTION_NODE_SNAP_SPAWN_CHILD = 2,
    NUM_ACTION_NODE_SNAP_TARGETS = 3,
};

// size 0x38
struct human_action_node_data
{
    void reset(); // 0x00A2C5A0 (.h:74)
    human_action_node_data(); // 0x00A5AF10
    ~human_action_node_data(); // 0x00A5AF40

    farray<unsigned int,4> action_node_set; // 0x00
    int action_state; // 0x20
    int action_sub_state; // 0x24
    int existing_action_state; // 0x28
    unsigned int current_action_node; // 0x2C
    timestamp state_duration; // 0x30
    bool exit_quick; // 0x34
};

// 0x00A5AF10: the set and the timestamp construct themselves.
inline human_action_node_data::human_action_node_data()
{
}

// 0x00A5AF40
inline human_action_node_data::~human_action_node_data()
{
}
