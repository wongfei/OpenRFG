// Original: volition/rfg/code/gameplay/activities/activity_tanks_a_lot_internal.h
//
// The tank activity (a modal activity): its levels (destruction targets and time) and sub-modes. Placement of the helper
// types inferred.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/hud/hud_message.h"

enum atal_sub_modes
{
    ATALSM_INACTIVE = 0,
    ATALSM_PRE = 1,
    ATALSM_ACTIVE = 2,
    ATALSM_PAUSED = 3,
    ATALSM_SUCCESS = 4,
    ATALSM_FAILURE = 5,
    NUM_ATAL_SUB_MODES = 6,
};

// size 0x8
struct tanks_a_lot_level
{
    float damage_amount; // 0x00
    int reward; // 0x04
};

// size 0x4C0
class activity_tanks_a_lot : public activity_base_modal
{
public:
    activity_tanks_a_lot(); // 0x0077E4E0 (.h:53)
    virtual void system_init(); // vtbl+0x1C 0x0075A1A0 (activity_tanks_a_lot.cpp:111)
    virtual void level_init(); // vtbl+0x24 0x0075A350 (activity_tanks_a_lot.cpp:198)
    virtual bool activity_start(); // vtbl+0x38 0x007AEAE0 (activity_tanks_a_lot.cpp:209)
    virtual void activity_end(); // vtbl+0x3C 0x00799D60 (activity_tanks_a_lot.cpp:241)
    virtual void activity_kill(); // vtbl+0x40 0x00799D80 (activity_tanks_a_lot.cpp:249)
    virtual void activity_do_frame_inactive(); // vtbl+0x50 0x0075A3B0 (activity_tanks_a_lot.cpp:328)
    virtual void activity_do_frame_ready(); // vtbl+0x58 0x00770A00 (activity_tanks_a_lot.cpp:321)
    virtual void activity_do_frame_active(); // vtbl+0x60 0x007B2EE0 (activity_tanks_a_lot.cpp:256)
    virtual void render_icons() const; // vtbl+0x74 0x0075A3C0 (activity_tanks_a_lot.cpp:334)
    virtual bool render_hud(int x_pos, int y_pos); // vtbl+0x70 0x00770A20 (activity_tanks_a_lot.cpp:357)
    void report_damage(float damage_amount); // 0x0075A3E0 (activity_tanks_a_lot.cpp:422)
    virtual ~activity_tanks_a_lot(); // vtbl+0x00 0x00768F30

    atal_sub_modes sub_mode; // 0x44C
    float current_damage; // 0x450
    int current_level; // 0x454
    int max_level; // 0x458
    tanks_a_lot_level level[10]; // 0x45C
    int non_priority_value; // 0x4AC
    int low_priority_value; // 0x4B0
    int medium_priority_value; // 0x4B4
    int high_priority_value; // 0x4B8
    hud_message_handle hud_message; // 0x4BC
};

extern activity_tanks_a_lot Tanks_a_lot_activity; // 0x01655FB0
