// Original: volition/rfg/code/objects/object_area_defense_node.h
// Implementation: volition/rfg/code/objects/object_area_defense_node.cpp
//
// Area defense nodes: the start points of the area defense activity (a flare effect marks an available node), with
// their activity info and completion state.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

struct activity_area_defense_info;
class base_activity_data;
class object_activity_spawn;

// size 0xA8
class object_area_defense_node : public object
{
public:
    object_area_defense_node(); // 0x00A15A30 (:14)
    static const object_type get_type_static(); // 0x00753C90 (.h:25)
    static const object_sub_type get_subtype_static(); // 0x00753CA0 (.h:25)
    static const char* get_classname_static(); // 0x00A10030 (.h:25)
    static int get_bpo_index_static(); // 0x00A10040 (.h:25)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A15A50 (.h:25)
    static unsigned int get_classname_hash(); // 0x00A15A20 (:13)
    static int pool_remaining(); // 0x00A39480 (:13)
    virtual void pool_free(); // vtbl+0x80 0x00A8DB40 (:13)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA4940 (:19)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A15A70 (:47)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A15A90 (:62)
    const activity_area_defense_info* get_area_defense_info() const; // 0x00753CB0 (.h:36)
    const base_activity_data* get_base_data() const; // 0x00A15AA0 (:73)
    bool is_completed() const; // 0x00753CC0 (.h:41)
    void set_completed(); // 0x00753CD0 (.h:42)
    object_activity_spawn* get_start_node() const; // 0x00A60F80 (:82)
    bool flare_started() const; // 0x00A394B0 (:97)
    void start_flare(const object_activity_spawn* start_node); // 0x00A394E0 (:133)
    void stop_flare(); // 0x00A39530 (:142)
    bool can_spawn() const; // 0x00A15AC0 (:104)
    virtual ~object_area_defense_node(); // vtbl+0x00 0x00A15A60

private:
    friend class cartographer; // get_activity_rewards reads ad_info (inferred)
    const activity_area_defense_info* ad_info; // 0x9C
    unsigned int flare_effect_handle; // 0xA0
    bool completed; // 0xA4
};

void types_object_area_defense_node(signed char& type, signed char& subtype); // 0x00A159F0 (:13)
int bpo_index_object_area_defense_node(); // 0x00A15A00 (:13)
const char* classname_object_area_defense_node(); // 0x00A15A10 (:13)
object* alloc_object_area_defense_node(); // 0x00A60F70 (:13)
char* area_defense_get_name(const object_area_defense_node* const orsn); // 0x00A15B00 (:150)

// 0x00753C90 (.h:25)
inline const object_type object_area_defense_node::get_type_static()
{
    return OT_AREA_DEFENSE_NODE;
}

// 0x00753CA0 (.h:25)
inline const object_sub_type object_area_defense_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00753CC0 (.h:41)
inline bool object_area_defense_node::is_completed() const
{
    return completed;
}
