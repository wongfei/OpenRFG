// Original: volition/rfg/code/spawn/ambient_spawn_behavior.h
// Implementation: volition/rfg/code/spawn/ambient_spawn_behavior.cpp
//
// Ambient behaviours: groups of animation set variants (with an optional ambient prop, time-of-day window, morale
// range and male/female ratio) that ambient NPCs spawned inside an object_ambient_behavior_region play. The region is
// a placed object with the usual type/class/pool registration.
#pragma once

#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

class ambient_prop_info;

// size 0x20
struct ambient_behavior
{
    anim_set_variants variant_anim_set; // 0x00
    const ambient_prop_info* amb_prop_info; // 0x04
    int occurance_chance; // 0x08
    int day_start; // 0x0C
    int day_end; // 0x10
    float morale_min; // 0x14
    float morale_max; // 0x18
    float male_female_ratio; // 0x1C
};

// size 0x29C
struct ambient_behavior_group
{
    char name[24]; // 0x00
    int num_behaviors; // 0x18
    ambient_behavior behaviors[20]; // 0x1C
};

// size 0xA4
class object_ambient_behavior_region : public object
{
public:
    object_ambient_behavior_region(); // 0x00B40EB0 (:92)
    static const object_type get_type_static(); // 0x00B40D80 (.h:48)
    static const object_sub_type get_subtype_static(); // 0x00B40D90 (.h:48)
    static const char* get_classname_static(); // 0x00B40DA0 (.h:48)
    static int get_bpo_index_static(); // 0x00B40DB0 (.h:48)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00B40ED0 (.h:48)
    static unsigned int get_classname_hash(); // 0x00B40EA0 (:91)
    static int pool_remaining(); // 0x00B46F60 (:91)
    virtual void pool_free(); // vtbl+0x80 0x00B54A90 (:91)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00B4C2D0 (:96)
    virtual bool stream(bool load); // vtbl+0x1C 0x00B40EF0 (:115)
    virtual ~object_ambient_behavior_region(); // vtbl+0x00 0x00B40EE0

    float radius; // 0x9C
    int ambient_group_id; // 0xA0
};

void ambient_behavior_system_init(); // 0x00B4C070 (:32)
int ambient_behavior_get_id(char* group_name); // 0x00B40E10 (:74)
ambient_behavior_group* ambient_behavior_from_id(int id); // 0x00B40E50 (:85)
void types_object_ambient_behavior_region(signed char& type, signed char& subtype); // 0x00B40E70 (:91)
int bpo_index_object_ambient_behavior_region(); // 0x00B40E80 (:91)
const char* classname_object_ambient_behavior_region(); // 0x00B40E90 (:91)
object* alloc_object_ambient_behavior_region(); // 0x00B4C2C0 (:91)
