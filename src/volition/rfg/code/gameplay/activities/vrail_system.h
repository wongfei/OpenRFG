// Original: volition/rfg/code/gameplay/activities/vrail_system.h
// Implementation: volition/rfg/code/gameplay/activities/vrail_system.cpp
//
// The vehicle rail system: queries over the running rails (drivers, patrol state, wait states, drop points, squads).
#pragma once

#include "volition/vlib/code/util/varray.h"

class npc;
class object_patrol;
class object_squad;
class vector;

enum vrail_patrol_state
{
    INVALID_VRAIL_PATROL_STATE = -1,
    VRAIL_PATROL_STATE_WAIT = 0,
    VRAIL_PATROL_STATE_PATROL = 1,
    VRAIL_PATROL_STATE_GOING_TO_DROP_OFF = 2,
    VRAIL_PATROL_STATE_COMPLETE_WAIT = 3,
    VRAIL_PATROL_STATE_COMPLETE = 4,
    VRAIL_PATROL_STATE_FAILED = 5,
    NUM_VRAIL_PATROL_STATES = 6,
};

bool vrail_is_driver(const npc* np); // 0x0075BCE0 (:15)
bool vrail_is_riding_shotgun_driver(const npc* np); // 0x0075BD50 (:35)
bool vrail_in_wait_state(); // 0x0075BDA0 (:52)
vrail_patrol_state vrail_get_patrol_state(); // 0x0075BE00 (:84)
bool vrail_is_patrol(const object_patrol* patrol); // 0x0075BE40 (:100)
void vrail_change_chase_count(int new_count); // 0x0078ADB0 (:118)
bool vrail_drop_point(vector& drop_point); // 0x0075BEA0 (:133)
bool vrail_is_vehicle(const unsigned int vehicle_handle); // 0x0078AE30 (:147)
bool vrail_get_patrol_vehicle(unsigned int& vehicle_handle); // 0x0078AEA0 (:159)
bool vrail_get_patrol_squad(unsigned int& squad_handle); // 0x0075BF20 (:176)
bool vrail_any_ready(); // 0x0075BF80 (:192)
int vrail_get_squads(base_array<object_squad*>& squad_list); // 0x0079BAB0 (:197)
void vrail_stop_squad_spawns(); // 0x007A3380 (:216)
