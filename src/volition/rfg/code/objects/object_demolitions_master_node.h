// Original: volition/rfg/code/objects/object_demolitions_master_node.h
// Implementation: volition/rfg/code/objects/object_demolitions_master_node.cpp
//
// Demolitions master nodes: the start points of the demolitions master activity. demolitions_master_level_data
// (read from the activity table, also from DLC) holds a level's weapons and ammo, time limits, messages and rewards.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/util/varray.h"

class base_activity_data;
class rfg_mover;
struct weapon_info;
struct xml_element;

// size 0x20
class demolitions_master_weapon_data
{
public:
    demolitions_master_weapon_data(); // 0x00A33BB0 (.h:46)
    void reset(); // 0x00A0FFD0 (.h:52)
    bool load(xml_element* xml_root); // 0x00A17AF0 (:94)

    const weapon_info* w_info; // 0x00
    char icon_name[24]; // 0x04
    int ammo; // 0x1C
};

// size 0x244
class demolitions_master_level_data
{
public:
    void reset(); // 0x00A33BD0 (.h:148)
    bool load(xml_element* xml_root); // 0x00A62E00 (:128)
    demolitions_master_level_data(); // 0x00A7A3D0

    char level_name[24]; // 0x00
    unsigned int name_crc; // 0x18
    char description[128]; // 0x1C
    int level; // 0x9C
    voice_line_handle radio_message_start; // 0xA0
    bool use_vehicle_spawn; // 0xA4
    farray<demolitions_master_weapon_data,4> weapon_list; // 0xA8
    int time_limit; // 0x138
    int par_time; // 0x13C
    bool unlimited_ammo_dm; // 0x140
    bool multiple_target_buildings; // 0x141
    char briefing_destroy_message[48]; // 0x142
    char corner_HUD_destroy_message[48]; // 0x172
    char info_title_1[24]; // 0x1A2
    char info_title_2[24]; // 0x1BA
    char success_string[16]; // 0x1D2
    char failure_string_ammo[16]; // 0x1E2
    char failure_string_time[16]; // 0x1F2
    char failure_string_range[16]; // 0x202
    char failure_string_death[16]; // 0x212
    float active_range; // 0x224
    const base_activity_data* base_data; // 0x228
    activity_standard_rewards standard_rewards; // 0x22C
    struct
    {
        bool completed; // 0x00
        bool discovered; // 0x01
        bool par_beaten; // 0x02
        unsigned int times_completed; // 0x04
        int best_time; // 0x08
    } dm_stats; // 0x238
};

// size 0xA4
class object_demolitions_master_node : public object
{
public:
    object_demolitions_master_node(); // 0x00A17BE0 (:358)
    static const object_type get_type_static(); // 0x00754620 (.h:183)
    static const object_sub_type get_subtype_static(); // 0x00754630 (.h:183)
    static const char* get_classname_static(); // 0x00A0FFF0 (.h:183)
    static int get_bpo_index_static(); // 0x00A10000 (.h:183)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A17C00 (.h:183)
    static unsigned int get_classname_hash(); // 0x00A17BD0 (:357)
    static int pool_remaining(); // 0x00A3AF70 (:357)
    virtual void pool_free(); // vtbl+0x80 0x00A900B0 (:357)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA6AF0 (:387)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A17CE0 (:447)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A17CC0 (:438)
    rfg_mover* find_demolitions_master_target() const; // 0x00A3AFA0 (:457)
    demolitions_master_level_data* get_demolitions_master_data() const; // 0x00754640 (.h:197)
    bool is_completed(); // 0x00754650 (.h:199)
    bool flare_started() const; // 0x00A17C20 (:363)
    void start_flare(); // 0x00A17C30 (:368)
    void stop_flare(); // 0x00A17C90 (:377)
    virtual ~object_demolitions_master_node(); // vtbl+0x00 0x00A17C10

    demolitions_master_level_data* base_demolitions_master_data; // 0x9C
private:
    unsigned int start_effect_handle; // 0xA0
};

object* alloc_object_demolitions_master_node(); // 0x00A633C0 (:357)
const char* classname_object_demolitions_master_node(); // 0x00A17BC0 (:357)
int bpo_index_object_demolitions_master_node(); // 0x00A17BB0 (:357)
void types_object_demolitions_master_node(signed char& type, signed char& subtype); // 0x00A17BA0 (:357)
void demolitions_master_system_init(int dlc_id); // 0x00AA6D30 (:478)
void demolitions_master_unload_dlc(); // 0x00A900C0 (:540)
const char* const demolitions_master_node_get_name(const object_demolitions_master_node* const odmn); // 0x00A17D00 (:552)

// 0x00754620 (.h:183)
inline const object_type object_demolitions_master_node::get_type_static()
{
    return OT_DEMOLITIONS_MASTER_NODE;
}

// 0x00754630 (.h:183)
inline const object_sub_type object_demolitions_master_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00754640 (.h:197)
inline demolitions_master_level_data* object_demolitions_master_node::get_demolitions_master_data() const
{
    return base_demolitions_master_data;
}

// 0x00754650 (.h:199)
inline bool object_demolitions_master_node::is_completed()
{
    if (base_demolitions_master_data == nullptr)
    {
        return false;
    }
    return base_demolitions_master_data->dm_stats.completed;
}
