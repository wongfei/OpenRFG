// Original: volition/rfg/code/objects/object_activity_spawn.h
// Implementation: volition/rfg/code/objects/object_activity_spawn.cpp
//
// Activity spawns (OT_ACTIVITY_SPAWN): the placed start of an activity, linking the activity data to the activity
// node that runs it (raid, area defense, house arrest, ...) and remembering whether it was completed.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

class base_activity_data;
class object_raid_node;

// size 0xA8
class object_activity_spawn : public object
{
public:
    object_activity_spawn(); // 0x00A15880 (:114)
    static const object_type get_type_static(); // 0x00754DC0 (.h:20)
    static const object_sub_type get_subtype_static(); // 0x00754DD0 (.h:20)
    static const char* get_classname_static(); // 0x00A10010 (.h:20)
    static int get_bpo_index_static(); // 0x00A10020 (.h:20)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A158A0 (.h:20)
    static unsigned int get_classname_hash(); // 0x00A15870 (:113)
    static int pool_remaining(); // 0x00A39300 (:113)
    virtual void pool_free(); // vtbl+0x80 0x00A8DB00 (:113)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA47F0 (:118)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A158C0 (:147)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A8DB10 (:226)
public:
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A158F0 (:233)
    virtual void destroy_notify(); // vtbl+0x2C 0x00A15900 (:238)
    unsigned int get_activity_handle() const; // 0x00754DE0 (.h:41)
    object_type get_activity_type() const; // 0x00754DF0 (.h:44)
    void level_init_post(); // 0x00A60C60 (:205)
    bool get_completed_flag() const; // 0x00A39390 (:198)
    const char* get_activity_name() const; // 0x00A158E0 (:158)
    object* get_activity_node() const; // 0x00A39330 (:164)
    virtual ~object_activity_spawn(); // vtbl+0x00 0x00A158B0

private:
    const base_activity_data* activity_data; // 0x9C
    unsigned int activity_node_handle; // 0xA0
    object_type activity_obj_type; // 0xA4
};

const object_raid_node* find_low_level_raid_object(const object_raid_node* raid_parent_objp); // 0x00A39280 (:42)
const char* classname_object_activity_spawn(); // 0x00A15860 (:113)
object* alloc_object_activity_spawn(); // 0x00A60C50 (:113)
int bpo_index_object_activity_spawn(); // 0x00A15850 (:113)
void types_object_activity_spawn(signed char& type, signed char& subtype); // 0x00A15840 (:113)
void obj_activity_spawn_level_init_post(); // 0x00A60CE0 (:279)
