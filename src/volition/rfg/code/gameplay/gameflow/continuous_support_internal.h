// Original: volition/rfg/code/gameplay/gameflow/continuous_support_internal.h
//
// A continuous support squad: respawning at a picked location and updating its orders.
#pragma once

#include "volition/rfg/code/gameplay/activities/reinforcement_squad.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/time/timestamp.h"

class object_squad;
class object_squad_spawn_node;
class vector;

// size 0x7C
class continuous_support_squad
{
public:
    continuous_support_squad(); // 0x007A4570 (.h:23)
    void reset(); // 0x0079C820 (continuous_support.cpp:162)
    void release_and_reset(); // 0x0079C860 (continuous_support.cpp:174)
    void do_frame(); // 0x007B7090 (continuous_support.cpp:220)
    bool update_squad_info(const object_squad* squad, const object_squad_spawn_node* squad_node, int new_respawn_delay_ms); // 0x00773570 (continuous_support.cpp:181)
private:
    void respawn_squad(); // 0x007B4540 (continuous_support.cpp:244)
    bool pick_spawn_location(vector& spawn_pos, human_teams team, bool vehicle_spawn); // 0x0079C790 (continuous_support.cpp:94)
    bool pick_spawn_location_default(vector& spawn_pos, human_teams team, bool vehicle_spawn); // 0x0078BAC0 (continuous_support.cpp:123)

public:
    unsigned int spawn_node_handle; // 0x00
    gfm_orders_types orders_type; // 0x04
    unsigned int orders_target; // 0x08
    int respawn_delay_ms; // 0x0C
    unsigned int spawn_region_handle; // 0x10
    float min_region_distance_sqrd; // 0x14
    reinforcement_squad base_squad_info; // 0x18
private:
    timestamp dead_time; // 0x78
};
