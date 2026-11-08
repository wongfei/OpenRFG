// Original: volition/rfg/code/ai/ambient_action_manager.h
// Implementation: volition/rfg/code/ai/ambient_action_manager.cpp
//
// The ambient action manager: ambient pedestrian actions that involve several NPCs or a vehicle (picking up and
// dropping off passengers, parking), matched and tracked per action set; ai_system::ambient_action is an npc's part in
// one. gameplay_do_frame calls amb_action_manager_do_frame.
#pragma once

#include "volition/rfg/code/ai/ambient_action_enum.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

class automobile;
class navpoint;
class npc;
class object;
class vehicle;

// size 0x48
class ai_ambient_action_info
{
public:
    ai_ambient_action_info(); // 0x0064C980 (.h:22)
    void reset(ambient_ped_action_type new_action_type); // 0x0060CB70 (:1609)
    void reset_action_timeout(); // 0x005DFA60 (:1626)
    bool has_ambient_action() const; // 0x005DFA90 (:1640)
    float get_desired_speed(const vehicle* vp) const; // 0x005DFAB0 (:1649)
    ~ai_ambient_action_info(); // 0x005F9C80

    ambient_ped_action_type action_type; // 0x00
    vector destination_pos; // 0x04
    unsigned int obj_target_handle; // 0x10
    unsigned int vehicle_target_handle; // 0x14
    farray<unsigned int,6> npc_targets; // 0x18
    timestamp action_timeout; // 0x40
    timestamp despawn_timeout; // 0x44
};

// size 0x8
struct ambient_action_set
{
    unsigned int npc_handle; // 0x00
    ambient_ped_action_type action_type; // 0x04
};

// size 0x10
struct ambient_drop_off_info
{
    npc* driver_np; // 0x00
    automobile* ap; // 0x04
    int num_passengers; // 0x08
    float dist_from_player_sqrd; // 0x0C
};

// size 0x10
struct ambient_park_info
{
    npc* driver_np; // 0x00
    automobile* ap; // 0x04
    int num_passengers; // 0x08
    float dist_from_player_sqrd; // 0x0C
};

// size 0x18
struct ambient_pick_up_info
{
    npc* driver_np; // 0x00
    automobile* ap; // 0x04
    int num_passengers; // 0x08
    int max_passengers; // 0x0C
    int availible_seats; // 0x10
    float dist_from_player_sqrd; // 0x14
};

// size 0x4C
class manager_ambient_action_info
{
public:
    manager_ambient_action_info(); // 0x0064F440 (:93)
    void reset(); // 0x0060CA60 (:150)
    bool is_active() const; // 0x0060CA80 (:159)
private:
    bool is_valid_action() const; // 0x005DF980 (:170)
public:
    ~manager_ambient_action_info(); // 0x0064F470

    ambient_ped_action_type action_type; // 0x00
    ambient_ped_action_type sub_action_type; // 0x04
    farray<ambient_action_set,6> npc_list; // 0x08
    struct
    {
        unsigned int sub_action_required : 1; // 0x00:0
    } action_flags; // 0x48
};

const navpoint* find_drop_off_navpoint(const object* objp); // 0x0064FE30 (:778)
void amb_action_manager_level_init_post(); // 0x0060CBC0 (:1675)
void amb_action_manager_do_frame(); // 0x00651060 (:1684)
void amb_action_manager_complete(); // 0x005DFAF0 (:1703)
