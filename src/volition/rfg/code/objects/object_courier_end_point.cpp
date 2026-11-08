// Original: volition/rfg/code/objects/object_courier_end_point.cpp
//
// gameflow.md GF4: the courier chase's end points and activity_courier_chase.xtbl. Registers the class, so zones now create
// these objects. serialize fails when the definition is unknown, so they only exist once courier_definitions_init has run.

#include "volition/rfg/code/objects/object_courier_end_point.h"

#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>

static object_pool<object_courier_end_point, 16, 1> object_courier_end_pointpool; // 0x03537E80
static farray<const courier_definition*, 20> Courier_definitions; // 0x016AD1B8
static string_pool Courier_string_pool("Courier String Pool"); // 0x03430960
static char* Courier_string_pool_data; // 0x0302F130

static const float COURIER_MIN_SPEED = 1.0f; // Magic number: the table's speeds are clamped to 1 to 30
static const float COURIER_MAX_SPEED = 30.0f;

// 0x013116A8
static const char* const Radio_line_xml_tags_courier_end[NUM_CTC_RADIO_LINES] = {
    "Courier_Start_Male",
    "Courier_Start_Female",
    "Courier_Spotted",
    "Courier_Spotted_Response_Male",
    "Courier_Spotted_Response_Female",
    "Out_Of_Range",
    "Out_Of_Range_Response_Male",
    "Out_Of_Range_Response_Female",
    "Destroyed_Data",
    "Destroyed_Data_Response_Male",
    "Destroyed_Data_Response_Female",
    "Captured_Data",
    "Captured_Data_Response_Male",
    "Captured_Data_Response_Female",
    "Transmit_Data",
    "Transmit_Data_Response_Male",
    "Transmit_Data_Response_Female",
};

// 0x00A164C0 (:60)
void types_object_courier_end_point(signed char& type, signed char& subtype)
{
    type = OT_COURIER_END;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A164D0 (:60)
int bpo_index_object_courier_end_point()
{
    return 1;
}

// 0x00A164E0 (:60)
const char* classname_object_courier_end_point()
{
    return "object_courier_end_point";
}

// 0x00A623F0 (:60)
object* alloc_object_courier_end_point()
{
    return object_courier_end_pointpool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01076E3B, 0x01076E46), the pool's bytes and capacity.
static obj_class_instantiator object_courier_end_point_instantiator("object_courier_end_point", alloc_object_courier_end_point, types_object_courier_end_point, bpo_index_object_courier_end_point, classname_object_courier_end_point, object_courier_end_point::pool_remaining, 0xB10, 0x10); // 0x030695AC

// 0x00A16500 (:61)
object_courier_end_point::object_courier_end_point()
{
}

// 0x00A16540
object_courier_end_point::~object_courier_end_point()
{
}

// 0x00A16490 (.h:109)
const char* object_courier_end_point::get_classname_static()
{
    return "object_courier_end_point";
}

// 0x00A164A0 (.h:109)
int object_courier_end_point::get_bpo_index_static()
{
    return 1;
}

// 0x00A16530 (.h:109)
const char* object_courier_end_point::get_classname() const
{
    return "object_courier_end_point";
}

// 0x00A164F0 (:60)
unsigned int object_courier_end_point::get_classname_hash()
{
    return object_courier_end_point_instantiator.this_hash;
}

// 0x00A3A3D0 (:60)
int object_courier_end_point::pool_remaining()
{
    return object_courier_end_pointpool.get_free();
}

// 0x00A8F260 (:60)
void object_courier_end_point::pool_free()
{
    object_courier_end_pointpool.free(this);
}

// 0x00A8F270 (:65). rfg.exe inlines the body.
void object_courier_end_point::free_this()
{
    object::free_this();
}

// 0x00AA5E60 (:70)
bool object_courier_end_point::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(COURIER_END_POINT_PROP_TYPE, courier_def != nullptr ? courier_def->name : "");
        const bool end_game_only = courier_node_flags.end_game_only != 0;
        pblock.set(COURIER_END_POINT_END_GAME_FLAG, end_game_only);
        const bool completed = courier_node_flags.completed != 0;
        pblock.set(COURIER_END_POINT_COMPLETED, completed);
        return true;
    }
    *reinterpret_cast<unsigned int*>(&courier_node_flags) = 0;
    char type_name[256];
    pblock.get(COURIER_END_POINT_PROP_TYPE, type_name);
    bool flag;
    if (pblock.get_optional(COURIER_END_POINT_COMPLETED, flag))
    {
        courier_node_flags.completed = flag;
    }
    if (pblock.get_optional(COURIER_END_POINT_END_GAME_FLAG, flag))
    {
        courier_node_flags.end_game_only = flag;
    }
    courier_def = courier_definition_from_name(type_name);
    if (courier_def == nullptr)
    {
        return false;
    }
    last_used_time.set(0);
    return true;
}

// 0x00A16550 (:114)
obj_serialize_mode object_courier_end_point::serialize_mode()
{
    return courier_node_flags.completed ? OSM_FULL : OSM_RESTORE;
}

// 0x00A16570 (:125)
bool object_courier_end_point::get_end_point(vector& courier_end) const
{
    courier_end = pos;
    return true;
}

// 0x00A16590 (:140). Empty in rfg.exe.
bool object_courier_end_point::find_courier_end_point()
{
    return true;
}

// 0x00A165A0 (:183)
const district* object_courier_end_point::get_end_district() const
{
    return district_get_from_pos(pos);
}

// 0x007544D0 (.h:142)
void object_courier_end_point::set_completed_flag(bool new_status)
{
    courier_node_flags.completed = new_status;
}

// 0x00A3A400 (:155). Original: after the last mission every node can spawn, the finished ones too.
bool object_courier_end_point::can_spawn() const
{
    if (mission_final_mission_complete())
    {
        return true;
    }
    if (courier_node_flags.completed)
    {
        return false;
    }
    const district* dp = district_get_from_pos(pos);
    return dp == nullptr || !dp->is_liberated();
}

// 0x007544F0 (.h:159)
bool object_courier_end_point::end_game_only_node() const
{
    return courier_node_flags.end_game_only != 0;
}

// 0x00A8F280 (:203). Original: a squad that does not resolve leaves the vehicle group null, and the entry is kept.
void courier_definitions_init()
{
    if (Courier_string_pool_data == nullptr)
    {
        Courier_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x200, 0)); // Magic number: 512 bytes of names
        if (Courier_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the courier string table");
        }
        Courier_string_pool.init(Courier_string_pool_data, 0x200, "Courier String Pool", 0, 0);
        Courier_string_pool.clear();
    }
    xml_element* root = xml_table_parse("activity_courier_chase.xtbl");
    if (root == nullptr)
    {
        keen::debug::nativePrintArgs("Courier definitions XML table is missing!");
        return;
    }
    for (xml_element* entry = root->elements; entry != nullptr && Courier_definitions.size() < Courier_definitions.capacity(); entry = entry->next)
    {
        const char* name = Courier_string_pool.add_unique(xml_table_get_req_string_ref(entry, "Name"));
        if (name == nullptr)
        {
            break;
        }
        const char* base_name = xml_table_get_opt_string_ref(entry, "base_activity_data");
        const base_activity_data* base_data = base_name != nullptr ? activity_data_find_handle(base_name) : nullptr;
        int reward = 0;
        xml_table_get_req_int(reward, entry, "Reward");
        int start_delay = 0;
        xml_table_get_req_int(start_delay, entry, "Start_Delay");
        float speeds[4]; // normal, evade, catch up, escape
        xml_table_get_req_float(speeds[0], entry, "Normal_Speed");
        xml_table_get_req_float(speeds[1], entry, "Evade_Speed");
        xml_table_get_req_float(speeds[2], entry, "Catch_Up_Speed");
        xml_table_get_req_float(speeds[3], entry, "Escape_Speed");
        for (int i = 0; i < 4; i++)
        {
            if (speeds[i] < COURIER_MIN_SPEED)
            {
                speeds[i] = COURIER_MIN_SPEED;
            }
            if (speeds[i] >= COURIER_MAX_SPEED)
            {
                speeds[i] = COURIER_MAX_SPEED;
            }
        }
        float mass_multiplier = 4.0f; // Magic number: the default, though the tag is required
        xml_table_get_req_float(mass_multiplier, entry, "Vehicle_Mass_Multiplier");
        float health_multiplier = 1.0f;
        xml_table_get_req_float(health_multiplier, entry, "Vehicle_Health_Multiplier");
        const char* squad_name = Courier_string_pool.add_unique(xml_table_get_req_string_ref(entry, "Courier_Squad"));
        if (squad_name == nullptr)
        {
            break;
        }
        const vehicle_spawn_group_info* vehicle_group = nullptr;
        const squad_definition* squad_def = squad_definition_from_name(squad_name);
        if (squad_def != nullptr) // rfg.exe formats a message when it is null and drops it
        {
            const char* group_name = Courier_string_pool.add_unique(xml_table_get_req_string_ref(entry, "Vehicle_Group"));
            if (group_name == nullptr)
            {
                break;
            }
            vehicle_group = sog_get_vehicle_group(group_name);
        }
        voice_line_handle radio_lines[NUM_CTC_RADIO_LINES];
        for (int i = 0; i < NUM_CTC_RADIO_LINES; i++)
        {
            radio_lines[i] = INVALID_VOICE_LINE_HANDLE;
        }
        xml_element* messages = xml_table_find_opt(entry, "Radio_Messages");
        if (messages != nullptr)
        {
            for (int i = 0; i < NUM_CTC_RADIO_LINES; i++)
            {
                const char* line_name = xml_table_get_opt_string_ref(messages, Radio_line_xml_tags_courier_end[i]);
                if (line_name != nullptr)
                {
                    radio_lines[i] = voice_line_handle_from_name(line_name);
                }
            }
        }
        courier_definition* def = static_cast<courier_definition*>(Permanent_mempool.alloc(sizeof(courier_definition), 4));
        if (def == nullptr)
        {
            char text[256];
            sprintf(text, "Out of memory in Permanent_mempool allocating courier definition for courier \"%s\"!", name);
            keen::debug::nativePrintArgs(text);
            // Original: returns with the table still open.
            return;
        }
        def->name = name;
        def->reward = reward;
        def->start_delay = start_delay;
        def->normal_speed = speeds[0];
        def->evade_speed = speeds[1];
        def->catch_up_speed = speeds[2];
        def->escape_speed = speeds[3];
        def->vehicle_mass_multiplier = mass_multiplier;
        def->squad_def = squad_def;
        def->vehicle_health_multiplier = health_multiplier;
        def->base_data = base_data;
        def->vehicle_group = vehicle_group;
        for (int i = 0; i < NUM_CTC_RADIO_LINES; i++)
        {
            def->radio_lines[i] = radio_lines[i];
        }
        Courier_definitions.add(def);
    }
    xml_table_close(root);
}

// 0x00A8F780 (:360)
object_patrol* courier_find_patrol(const object_courier_end_point* objp)
{
    farray<object_patrol*, 16> patrols;
    world_collect_descendants<object_patrol>(objp, patrols, nullptr, nullptr);
    return patrols.size() > 0 ? patrols[0] : nullptr;
}

// 0x00A3A460 (:379)
const courier_definition* courier_definition_from_name(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Courier_definitions.size(); i++)
    {
        if (v_stricmp(Courier_definitions[i]->name, name) == 0)
        {
            return Courier_definitions[i];
        }
    }
    return nullptr;
}

// 0x00A165D0 (:394)
const char* const courier_get_name(const object_courier_end_point* const objp)
{
    if (objp != nullptr && objp->courier_def != nullptr)
    {
        return objp->courier_def->name;
    }
    return nullptr;
}

// 0x00A62400 (:404)
bool courier_end_get_all_points(farray<object_courier_end_point const*, 16>& end_points)
{
    end_points.clear();
    for (int i = 0; i < World.get_num_objects<object_courier_end_point>(); i++)
    {
        end_points.add(World.get_object<object_courier_end_point>(i));
    }
    return end_points.size() > 0;
}

// 0x00A165F0 (:418). Empty in rfg.exe.
void courier_end_find_end_points()
{
}
