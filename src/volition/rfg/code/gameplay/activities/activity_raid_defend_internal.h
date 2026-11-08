// Original: volition/rfg/code/gameplay/activities/activity_raid_defend_internal.h
// Implementation: volition/rfg/code/gameplay/activities/activity_raid_defend.cpp
//
// The defensive raid (the EDF attacks a guerrilla position): the raid data with the defending side swapped.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_raid_internal.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/vlib/code/util/varray.h"

class object;
class vector;

// size 0x1F48
class defensive_raid_data : public activity_raid_data
{
public:
    defensive_raid_data(); // 0x0077E130 (.h:18)
    virtual bool collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const; // vtbl+0xAC 0x00785750 (activity_raid_defend.cpp:111)
    virtual const char* get_title() const; // vtbl+0x80 0x00759D80 (activity_raid_defend.cpp:149)
    virtual const char* get_subtitle() const; // vtbl+0x84 0x00759D90 (activity_raid_defend.cpp:163)
    virtual const char* get_icon_image_name() const; // vtbl+0x88 0x00759DA0 (activity_raid_defend.cpp:168)
    virtual bool is_defending_team(human_teams team) const; // vtbl+0xE0 0x00759D20 (activity_raid_defend.cpp:122)
    virtual bool is_attacking_team(human_teams team) const; // vtbl+0xE4 0x00759D50 (activity_raid_defend.cpp:134)
    virtual ~defensive_raid_data(); // vtbl+0x00 0x0077E150

};

extern defensive_raid_data Defensive_raid; // 0x022330C8
