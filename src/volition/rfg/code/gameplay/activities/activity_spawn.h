// Original: volition/rfg/code/gameplay/activities/activity_spawn.h
// Implementation: volition/rfg/code/gameplay/activities/activity_spawn.cpp
//
// Activity spawn properties: per activity spawn object, the property list (squads, groups, districts) read at
// level_init_post; last_activity_spawned_info records the last activity spawn.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/vlib/code/util/varray.h"

class vector;
struct xml_element;

// size 0xC
class activity_spawn_property
{
public:
    activity_spawn_property(); // 0x00769270 (.h:17)
    bool load(xml_element* xml_entry); // 0x0075A0C0 (:78)
    void reset(); // 0x00754780 (.h:31)

    activity_modes desired_mode; // 0x00
    float control_percentage; // 0x04
    struct
    {
        unsigned int completed : 1; // 0x00:0
    } activity_flags; // 0x08
};

// size 0x194
class activity_spawn_properties
{
public:
    activity_spawn_properties(); // 0x0077EDA0 (.h:43)
    bool load(xml_element* xml_root); // 0x00785AC0 (:105)
    void reset(); // 0x00769290 (.h:55)
    bool in_use() const; // 0x007692A0 (.h:61)
    ~activity_spawn_properties(); // 0x00785A30

    unsigned int district_handle; // 0x00
    farray<activity_spawn_property,32> activation_levels; // 0x04
};

// size 0x8
struct last_activity_spawned_info
{
    unsigned int spawn_node_handle; // 0x00
    activity_modes mode; // 0x04
};

unsigned int get_district_from_name(const char* district_name); // 0x00785A40 (:40)
activity_spawn_properties* get_spawn_props_from_handle(unsigned int district_handle); // 0x00770900 (:54)
activity_spawn_properties* activity_get_spawn_props(const vector& pos); // 0x00799D40 (:149)
activity_spawn_properties* activity_get_spawn_props(unsigned int district_handle); // 0x00770940 (:163)
void activity_spawn_level_init_post(); // 0x00785BD0 (:171)
