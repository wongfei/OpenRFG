// Original: volition/rfg/code/objects/object_raid_node.h
// Implementation: volition/rfg/code/objects/object_raid_node.cpp
//
// Raid nodes: the start points of the raid activities (attack and defend), with their raid definitions.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

class base_activity_data;
class base_raid_definition_data;

// size 0xA4
class object_raid_node : public object
{
public:
    object_raid_node(); // 0x00A1AFB0 (:31)
    static const object_type get_type_static(); // 0x00753EF0 (.h:37)
    static const object_sub_type get_subtype_static(); // 0x00753F00 (.h:37)
    static const char* get_classname_static(); // 0x00A15810 (.h:37)
    static int get_bpo_index_static(); // 0x00A15820 (.h:37)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1AFD0 (.h:37)
    static unsigned int get_classname_hash(); // 0x00A1AFA0 (:30)
    static int pool_remaining(); // 0x00A3D600 (:30)
    virtual void pool_free(); // vtbl+0x80 0x00A92A50 (:30)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAAD80 (:56)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A1B030 (:97)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A1AFF0 (:35)
    const base_raid_definition_data* get_raid_def() const; // 0x00753F10 (.h:48)
    const base_activity_data* get_base_data() const; // 0x00A1B010 (:45)
    bool get_completed_flag() const; // 0x00753F20 (.h:54)
    void set_completed_flag(); // 0x00A6A040 (:111)
    void set_completed_flag_parent(); // 0x00A3D630 (:118)
    void set_completed_flag_child(); // 0x00A3D660 (:178)
    bool is_defensive_raid() const; // 0x00A1B050 (:131)
    bool can_spawn() const; // 0x00A1B070 (:139)
    virtual ~object_raid_node(); // vtbl+0x00 0x00A1AFE0

    const base_raid_definition_data* base_raid_def; // 0x9C
private:
    struct
    {
        bool completed : 1; // 0x00:0
    } raid_flags; // 0xA0
};

void types_object_raid_node(signed char& type, signed char& subtype); // 0x00A1AF70 (:30)
int bpo_index_object_raid_node(); // 0x00A1AF80 (:30)
const char* classname_object_raid_node(); // 0x00A1AF90 (:30)
object* alloc_object_raid_node(); // 0x00A6A030 (:30)
const char* const raid_node_get_name(const object_raid_node* const orn); // 0x00A1B0B0 (:198)

// 0x00753EF0 (.h:37)
inline const object_type object_raid_node::get_type_static()
{
    return OT_RAID_NODE;
}

// 0x00753F00 (.h:37)
inline const object_sub_type object_raid_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00753F20 (.h:54)
inline bool object_raid_node::get_completed_flag() const
{
    return raid_flags.completed;
}
