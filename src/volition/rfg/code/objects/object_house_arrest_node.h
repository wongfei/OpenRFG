// Original: volition/rfg/code/objects/object_house_arrest_node.h
// Implementation: volition/rfg/code/objects/object_house_arrest_node.cpp
//
// House arrest nodes: the start points of the house arrest activity. house_arrest_level_data (read from the activity
// table, also from DLC) holds a level's informant line and animation, reinforcement squads, hostage kill time and
// rewards.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/util/varray.h"

class base_activity_data;
struct squad_definition;
class vehicle_spawn_group_info;
struct xml_element;

// size 0xC
class house_arrest_squad_data
{
public:
    house_arrest_squad_data(); // 0x00A33C70 (.h:31)
    void reset(); // 0x00A10090 (.h:37)
    bool load(xml_element* xml_root); // 0x00A90540 (:98)

    const squad_definition* squad_def; // 0x00
    const vehicle_spawn_group_info* veh_group; // 0x04
    int spawn_delay_ms; // 0x08
};

// size 0x98
class house_arrest_level_data
{
public:
    void reset(); // 0x00A33C80 (.h:76)
    bool load(xml_element* xml_root); // 0x00A905E0 (:128)
    house_arrest_level_data(); // 0x00A7A430

    const char* level_name; // 0x00
    const base_activity_data* base_data; // 0x04
    int level; // 0x08
    voice_line_handle informant_line; // 0x0C
    animation_action informant_anim_action; // 0x10
    voice_line_handle radio_message_start; // 0x14
    farray<house_arrest_squad_data,8> reinforcement_squad_list; // 0x18
    int hostage_kill_time; // 0x88
    activity_standard_rewards standard_rewards; // 0x8C
};

// size 0xA4
class object_house_arrest_node : public object
{
public:
    object_house_arrest_node(); // 0x00A18190 (:211)
    static const object_type get_type_static(); // 0x00754560 (.h:93)
    static const object_sub_type get_subtype_static(); // 0x00754570 (.h:93)
    static const char* get_classname_static(); // 0x00A100A0 (.h:93)
    static int get_bpo_index_static(); // 0x00A100B0 (.h:93)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A181B0 (.h:93)
    static unsigned int get_classname_hash(); // 0x00A18180 (:210)
    static int pool_remaining(); // 0x00A3B2B0 (:210)
    virtual void pool_free(); // vtbl+0x80 0x00A907D0 (:210)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA72C0 (:234)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A18200 (:282)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A181F0 (:228)
    const house_arrest_level_data* get_house_arrest_data() const; // 0x00754580 (.h:107)
    const base_activity_data* get_base_data() const; // 0x00A181D0 (:217)
    bool get_completed_flag() const; // 0x00754590 (.h:113)
    void set_completed_flag(); // 0x00A63BC0 (:297)
    void set_completed_flag_parent(); // 0x00A3B2E0 (:304)
    void set_completed_flag_child(); // 0x00A3B310 (:350)
    bool can_spawn() const; // 0x00A18230 (:317)
    void enable(); // 0x007545A0 (.h:132)
    void disable();
    bool is_disabled() const; // 0x007545B0 (.h:134)
    virtual ~object_house_arrest_node(); // vtbl+0x00 0x00A181C0

    const house_arrest_level_data* base_house_arrest_data; // 0x9C
private:
    struct
    {
        bool completed : 1; // 0x00:0
    } ha_flags; // 0xA0
    bool disabled; // 0xA1
};

void types_object_house_arrest_node(signed char& type, signed char& subtype); // 0x00A18150 (:210)
int bpo_index_object_house_arrest_node(); // 0x00A18160 (:210)
const char* classname_object_house_arrest_node(); // 0x00A18170 (:210)
object* alloc_object_house_arrest_node(); // 0x00A63BB0 (:210)
void house_arrest_system_init(int dlc_id); // 0x00AA7420 (:371)
void house_arrest_unload_dlc(); // 0x00A907E0 (:434)
const char* const house_arrest_get_name(const object_house_arrest_node* const ohan); // 0x00A18270 (:445)

// 0x00754560 (.h:93)
inline const object_type object_house_arrest_node::get_type_static()
{
    return OT_HOUSE_ARREST_NODE;
}

// 0x00754570 (.h:93)
inline const object_sub_type object_house_arrest_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00754590 (.h:113)
inline bool object_house_arrest_node::get_completed_flag() const
{
    return ha_flags.completed;
}
