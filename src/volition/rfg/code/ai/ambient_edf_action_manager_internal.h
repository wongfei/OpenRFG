// Original: volition/rfg/code/ai/ambient_edf_action_manager_internal.h
// Implementation: volition/rfg/code/ai/ambient_edf_action_manager.cpp
//
// The state of one ambient EDF action (type, participants, destination, timers).
#pragma once

#include "volition/rfg/code/ai/ambient_edf_action_manager_enum.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

// size 0x24
class ambient_edf_info
{
public:
    ambient_edf_info(); // 0x0060CC20 (.h:41)
    void amb_do_frame(); // 0x00651350 (ambient_edf_action_manager.cpp:510)
    void amb_do_frame_fly_by(); // 0x006512C0 (ambient_edf_action_manager.cpp:222)
    void amb_do_frame_drive_by(); // 0x00651310 (ambient_edf_action_manager.cpp:419)
    void amb_do_frame_dump_civ(); // 0x0060D000 (ambient_edf_action_manager.cpp:457)
    void amb_do_frame_harass_player(); // 0x0060D060 (ambient_edf_action_manager.cpp:495)
    bool amb_do_frame_drive_by_incomplete(); // 0x0060CFD0 (ambient_edf_action_manager.cpp:407)
    bool amb_do_frame_drive_by_complete(); // 0x00651300 (ambient_edf_action_manager.cpp:344)
    bool amb_do_frame_fly_by_incomplete(); // 0x0060CE30 (ambient_edf_action_manager.cpp:279)
    bool amb_do_frame_fly_by_complete(); // 0x0060CDD0 (ambient_edf_action_manager.cpp:243)
    bool amb_do_frame_dump_civ_incomplete(); // 0x0060CFF0 (ambient_edf_action_manager.cpp:445)
    bool amb_do_frame_dump_civ_complete(); // 0x0060CFE0 (ambient_edf_action_manager.cpp:433)
    bool amb_do_frame_harass_player_incomplete(); // 0x0060D050 (ambient_edf_action_manager.cpp:483)
    bool amb_do_frame_harass_player_complete(); // 0x0060D040 (ambient_edf_action_manager.cpp:471)
    bool do_frame_drive_by_player_complete_common(); // 0x0060CF10 (ambient_edf_action_manager.cpp:357)
    bool do_frame_drive_by_player_incomplete_common(); // 0x0060CEA0 (ambient_edf_action_manager.cpp:314)
    void amb_do_spawn(); // 0x00651E70 (ambient_edf_action_manager.cpp:1074)
    void amb_do_spawn_fly_by(); // 0x0060D0A0 (ambient_edf_action_manager.cpp:637)
    void amb_do_spawn_drive_by(); // 0x006513E0 (ambient_edf_action_manager.cpp:749)
    void amb_do_spawn_dump_civ(); // 0x006516A0 (ambient_edf_action_manager.cpp:836)
    void amb_do_spawn_harass_player(); // 0x00651B70 (ambient_edf_action_manager.cpp:979)
    bool is_active() const; // 0x005DFFB0 (ambient_edf_action_manager.cpp:1141)
    void release_and_reset(); // 0x0060CD20 (ambient_edf_action_manager.cpp:190)
    void reset(); // 0x005DFB70 (ambient_edf_action_manager.cpp:178)

    ambient_edf_action_type action_type; // 0x00
    unsigned int ambient_squad_handle; // 0x04
    unsigned int supporting_squad_handle; // 0x08
    timestamp cooldown_timer; // 0x0C
    unsigned int target_obj_handle; // 0x10
    vector target_pos; // 0x14
    struct
    {
        unsigned int completed : 1; // 0x00:0
        unsigned int slot_reserved : 1; // 0x00:1
    } action_flags; // 0x20
};

extern ambient_edf_info Current_ambient_edf_info; // 0x01642228
