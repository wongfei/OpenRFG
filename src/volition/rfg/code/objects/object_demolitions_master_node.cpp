// Original: volition/rfg/code/objects/object_demolitions_master_node.cpp
//
// gameflow.md GF4: the start points of Demolitions Master and activity_demolitions_master.xtbl. Registers the class, so
// zones now create these objects. A node with no table entry, or with no rfg_mover above it, fails to load.

#include "volition/rfg/code/objects/object_demolitions_master_node.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>
#include <string.h>

static string_pool Demolitions_master_string_pool(nullptr); // 0x03067F40
static char* Demolitions_master_string_pool_data; // 0x0302F138
static object_pool<demolitions_master_level_data, 32, 1> Demolitions_master_level_data_pool; // 0x0353F0A0
static farray<demolitions_master_level_data*, 6> Dlc_demo_master_list; // 0x016AD248
static object_pool<object_demolitions_master_node, 32, 1> object_demolitions_master_nodepool; // 0x03043AE0

// 0x00A17BA0 (:357)
void types_object_demolitions_master_node(signed char& type, signed char& subtype)
{
    type = OT_DEMOLITIONS_MASTER_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A17BB0 (:357)
int bpo_index_object_demolitions_master_node()
{
    return 1;
}

// 0x00A17BC0 (:357)
const char* classname_object_demolitions_master_node()
{
    return "object_demolitions_master_node";
}

// 0x00A633C0 (:357)
object* alloc_object_demolitions_master_node()
{
    return object_demolitions_master_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01077240), the pool's bytes and capacity.
static obj_class_instantiator object_demolitions_master_node_instantiator("object_demolitions_master_node", alloc_object_demolitions_master_node, types_object_demolitions_master_node, bpo_index_object_demolitions_master_node, classname_object_demolitions_master_node, object_demolitions_master_node::pool_remaining, 0x1560, 0x20);

// 0x00A62D80 (:71). rfg.exe: LTCG, level_name in EDI.
static demolitions_master_level_data* find_demolitions_master_data(const char* level_name)
{
    const unsigned int name_crc = crc_stri(level_name, 0);
    for (demolitions_master_level_data* data = Demolitions_master_level_data_pool.get_first(0); data != nullptr; data = Demolitions_master_level_data_pool.get_next(data))
    {
        if (data->name_crc == name_crc && v_stricmp(data->level_name, level_name) == 0)
        {
            return data;
        }
    }
    return nullptr;
}

// 0x00A33BB0 (.h:46)
demolitions_master_weapon_data::demolitions_master_weapon_data()
{
    reset();
}

// 0x00A0FFD0 (.h:52)
void demolitions_master_weapon_data::reset()
{
    w_info = nullptr;
    icon_name[0] = '\0';
    ammo = -1;
}

// 0x00A17AF0 (:94). A weapon or an icon of its own, and the ammo, are all required.
bool demolitions_master_weapon_data::load(xml_element* xml_root)
{
    reset();
    xml_element* weapon = xml_table_find_opt(xml_root, "Weapon");
    if (weapon != nullptr)
    {
        const unsigned int handle = weapons_get_weapon_info_handle(weapon->text);
        if (handle != 0xFFFFFFFF)
        {
            w_info = weapons_get_weapon_info(handle);
        }
    }
    if (w_info == nullptr && !xml_table_get_opt_string(icon_name, sizeof(icon_name), xml_root, "Non_Weapon_Icon_Name"))
    {
        icon_name[0] = '\0';
        return false;
    }
    xml_element* ammo_element = xml_find(xml_root, "Ammo");
    if (ammo_element == nullptr)
    {
        return false;
    }
    ammo = xml_get_int(ammo_element, -1);
    return true;
}

// 0x00A7A3D0
demolitions_master_level_data::demolitions_master_level_data()
{
}

// 0x00A33BD0 (.h:148). Original: failure_string_time and dm_stats are not cleared.
void demolitions_master_level_data::reset()
{
    level_name[0] = '\0';
    name_crc = 0;
    description[0] = '\0';
    level = 0;
    radio_message_start = INVALID_VOICE_LINE_HANDLE;
    use_vehicle_spawn = false;
    weapon_list.clear();
    time_limit = 0;
    par_time = 0;
    unlimited_ammo_dm = false;
    multiple_target_buildings = false;
    briefing_destroy_message[0] = '\0';
    corner_HUD_destroy_message[0] = '\0';
    info_title_1[0] = '\0';
    info_title_2[0] = '\0';
    success_string[0] = '\0';
    failure_string_ammo[0] = '\0';
    failure_string_range[0] = '\0';
    failure_string_death[0] = '\0';
    active_range = 0.0f;
    base_data = nullptr;
}

// Reads a required tag through the string pool, in upper case, into a fixed buffer. rfg.exe inlines it. Original: the
// copies have no bound, so a table string longer than its field overruns the next one.
static bool load_upper_string(char* out, xml_element* xml_root, const char* tag)
{
    xml_element* element = xml_find(xml_root, tag);
    if (element == nullptr)
    {
        return false;
    }
    str_make_upper(element->text);
    const char* pooled = Demolitions_master_string_pool.add_unique(element->text);
    if (pooled == nullptr)
    {
        return false;
    }
    strcpy(out, pooled);
    return true;
}

// 0x00A62E00 (:128). Every tag is required unless it says otherwise; the first missing one fails the entry.
bool demolitions_master_level_data::load(xml_element* xml_root)
{
    demolitions_master_weapon_data weapon;
    reset();
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr)
    {
        return false;
    }
    const char* pooled = Demolitions_master_string_pool.add_unique(element->text);
    if (pooled == nullptr)
    {
        return false;
    }
    strcpy(level_name, pooled);
    name_crc = crc_stri(pooled, 0);
    const char* base_name = xml_table_get_opt_string_ref(xml_root, "base_activity_data");
    base_data = base_name != nullptr ? activity_data_find_handle(base_name) : nullptr;
    element = xml_find(xml_root, "Demo_Info_Body_Text");
    if (element == nullptr)
    {
        return false;
    }
    pooled = Demolitions_master_string_pool.add_unique(element->text);
    if (pooled == nullptr)
    {
        return false;
    }
    strcpy(description, pooled);
    element = xml_find(xml_root, "Level");
    if (element == nullptr)
    {
        return false;
    }
    level = xml_get_int(element, 0);
    if (level < 0)
    {
        level = 0;
    }
    radio_message_start = voice_line_handle_from_name(xml_table_get_req_string_ref(xml_root, "Radio_Message"));
    load_standard_rewards(standard_rewards, xml_root);
    element = xml_find(xml_root, "Use_Vehicle_Spawn");
    if (element == nullptr)
    {
        return false;
    }
    use_vehicle_spawn = xml_get_bool(element, false);
    xml_element* list = xml_find(xml_root, "Weapon_List");
    if (list == nullptr)
    {
        return false;
    }
    for (xml_element* entry = xml_find(list, "Weapon_Data"); entry != nullptr; entry = xml_find_next(list, entry, "Weapon_Data"))
    {
        if (weapon.load(entry))
        {
            weapon_list.add(weapon);
        }
    }
    if (!xml_table_get_opt_int(time_limit, xml_root, "Time_Limit"))
    {
        time_limit = 0;
    }
    element = xml_find(xml_root, "Par_Time");
    if (element == nullptr)
    {
        return false;
    }
    par_time = xml_get_int(element, 10); // Magic number: rfg.exe's default, in seconds
    if (par_time < 0)
    {
        par_time = 0;
    }
    element = xml_find(xml_root, "Unlimited_Ammo");
    if (element == nullptr)
    {
        return false;
    }
    unlimited_ammo_dm = xml_get_bool(element, false);
    element = xml_find(xml_root, "Multiple_Target_Buildings");
    if (element == nullptr)
    {
        return false;
    }
    multiple_target_buildings = xml_get_bool(element, false);
    if (!load_upper_string(briefing_destroy_message, xml_root, "Briefing_Destroy_Message") || !load_upper_string(corner_HUD_destroy_message, xml_root, "Corner_HUD_Destroy_Message") || !load_upper_string(info_title_1, xml_root, "Info_Title_1"))
    {
        return false;
    }
    char* title_2 = xml_table_get_opt_string_ref(xml_root, "Info_Title_2"); // optional
    if (title_2 != nullptr)
    {
        str_make_upper(title_2);
        pooled = Demolitions_master_string_pool.add_unique(title_2);
        if (pooled == nullptr)
        {
            return false;
        }
        strcpy(info_title_2, pooled);
    }
    if (!load_upper_string(success_string, xml_root, "Success_String") || !load_upper_string(failure_string_ammo, xml_root, "Failure_String_Ammo") || !load_upper_string(failure_string_time, xml_root, "Failure_String_Time") || !load_upper_string(failure_string_range, xml_root, "Failure_String_Range") || !load_upper_string(failure_string_death, xml_root, "Failure_String_Death"))
    {
        return false;
    }
    element = xml_find(xml_root, "Active_Range");
    if (element == nullptr)
    {
        return false;
    }
    active_range = xml_get_float(element, 25.0f); // Magic number: rfg.exe's default, in metres
    if (active_range <= 0.0f)
    {
        active_range = 0.0f;
    }
    dm_stats.completed = false;
    dm_stats.discovered = false;
    dm_stats.par_beaten = false;
    dm_stats.times_completed = 0;
    dm_stats.best_time = 0;
    return true;
}

// 0x00A17BE0 (:358)
object_demolitions_master_node::object_demolitions_master_node()
{
}

// 0x00A17C10
object_demolitions_master_node::~object_demolitions_master_node()
{
}

// 0x00A0FFF0 (.h:183)
const char* object_demolitions_master_node::get_classname_static()
{
    return "object_demolitions_master_node";
}

// 0x00A10000 (.h:183)
int object_demolitions_master_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A17C00 (.h:183)
const char* object_demolitions_master_node::get_classname() const
{
    return "object_demolitions_master_node";
}

// 0x00A17BD0 (:357)
unsigned int object_demolitions_master_node::get_classname_hash()
{
    return object_demolitions_master_node_instantiator.this_hash;
}

// 0x00A3AF70 (:357)
int object_demolitions_master_node::pool_remaining()
{
    return object_demolitions_master_nodepool.get_free();
}

// 0x00A900B0 (:357)
void object_demolitions_master_node::pool_free()
{
    object_demolitions_master_nodepool.free(this);
}

// 0x00A17C20 (:363). Original: only the handle is asked, unlike the other nodes, which look the effect up.
bool object_demolitions_master_node::flare_started() const
{
    return start_effect_handle != 0xFFFFFFFF;
}

// 0x00A17C30 (:368)
void object_demolitions_master_node::start_flare()
{
    static const vector EFFECT_OFFSET(0.0f, 0.0f, 0.5f); // 0x035B599C. Magic number: half a metre forward.
    if (start_effect_handle == 0xFFFFFFFF)
    {
        start_effect_handle = objective_highlight_activity_marker_add(this, EFFECT_OFFSET);
    }
}

// 0x00A17C90 (:377)
void object_demolitions_master_node::stop_flare()
{
    if (start_effect_handle != 0xFFFFFFFF)
    {
        World.destroy_object(start_effect_handle, false);
        start_effect_handle = 0xFFFFFFFF;
    }
}

// 0x00AA6AF0 (:387). The stats live in the table's data, so a node reloaded while a challenge is being accepted, when its
// layer loads, keeps what the table holds.
bool object_demolitions_master_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(DEMOLITIONS_MASTER_PROP_TYPE, base_demolitions_master_data->level_name);
        pblock.set(DEMOLITIONS_MASTER_PROP_BEST_TIME, base_demolitions_master_data->dm_stats.best_time);
        pblock.set(DEMOLITIONS_MASTER_PROP_COMPLETE, base_demolitions_master_data->dm_stats.completed);
        pblock.set(DEMOLITIONS_MASTER_PROP_DISCOVERED, base_demolitions_master_data->dm_stats.discovered);
        pblock.set(DEMOLITIONS_MASTER_PROP_PAR_BEATEN, base_demolitions_master_data->dm_stats.par_beaten);
        pblock.set(DEMOLITIONS_MASTER_PROP_TIMES_COMPLETED, base_demolitions_master_data->dm_stats.times_completed);
        return true;
    }
    char type_name[256];
    type_name[0] = '\0';
    base_demolitions_master_data = nullptr;
    start_effect_handle = 0xFFFFFFFF;
    pblock.get(DEMOLITIONS_MASTER_PROP_TYPE, type_name);
    base_demolitions_master_data = find_demolitions_master_data(type_name);
    if (base_demolitions_master_data == nullptr || find_demolitions_master_target() == nullptr)
    {
        return false;
    }
    if (Demolitions_master_activity.current_state != DM_STATE_PREGAME_ACCEPT)
    {
        base_demolitions_master_data->dm_stats.completed = false;
        base_demolitions_master_data->dm_stats.discovered = false;
        base_demolitions_master_data->dm_stats.par_beaten = false;
        base_demolitions_master_data->dm_stats.times_completed = 0;
        base_demolitions_master_data->dm_stats.best_time = 0;
        pblock.get_optional(DEMOLITIONS_MASTER_PROP_BEST_TIME, base_demolitions_master_data->dm_stats.best_time);
        pblock.get_optional(DEMOLITIONS_MASTER_PROP_COMPLETE, base_demolitions_master_data->dm_stats.completed);
        pblock.get_optional(DEMOLITIONS_MASTER_PROP_DISCOVERED, base_demolitions_master_data->dm_stats.discovered);
        pblock.get_optional(DEMOLITIONS_MASTER_PROP_PAR_BEATEN, base_demolitions_master_data->dm_stats.par_beaten);
        pblock.get_optional(DEMOLITIONS_MASTER_PROP_TIMES_COMPLETED, base_demolitions_master_data->dm_stats.times_completed);
    }
    return true;
}

// 0x00A17CC0 (:438): a finished node restores differently, the enum's next value. Original: no check for null data.
obj_serialize_mode object_demolitions_master_node::serialize_mode()
{
    return (obj_serialize_mode)(OSM_RESTORE + (base_demolitions_master_data->dm_stats.completed ? 1 : 0));
}

// 0x00A17CE0 (:447)
bool object_demolitions_master_node::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A3AFA0 (:457): the first parent that is not a demolitions master node has to be the building.
rfg_mover* object_demolitions_master_node::find_demolitions_master_target() const
{
    const object_demolitions_master_node* node = this;
    unsigned int obj_handle;
    do
    {
        obj_handle = node->get_parent();
        if (obj_handle == 0xFFFFFFFF)
        {
            return nullptr;
        }
        node = World.object_from_handle<object_demolitions_master_node>(obj_handle);
    } while (node != nullptr);
    return World.object_from_handle<rfg_mover>(obj_handle);
}

// 0x00AA6D30 (:478). dlc_id -1 is the base game's table.
void demolitions_master_system_init(int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    xml_element* root;
    if (is_dlc)
    {
        char filename[256];
        Demolitions_master_string_pool.set_thread_ownership();
        Demolitions_master_string_pool.mark();
        sprintf(filename, "dlc%02d_activity_demolitions_master.xtbl", dlc_id);
        root = xml_table_parse(filename);
    }
    else
    {
        if (Demolitions_master_string_pool_data == nullptr)
        {
            Demolitions_master_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x800, 0)); // Magic number: 2 KB of names
            if (Demolitions_master_string_pool_data == nullptr)
            {
                keen::debug::nativePrintArgs("Insufficient memory to allocate the demolitions master string table");
            }
            Demolitions_master_string_pool.init(Demolitions_master_string_pool_data, 0x800, nullptr, 0, 0);
            Demolitions_master_string_pool.clear();
            Demolitions_master_level_data_pool.clear();
        }
        root = xml_table_parse("activity_demolitions_master.xtbl");
    }
    for (xml_element* entry = xml_find(root, "Demolitions_Master"); entry != nullptr; entry = xml_find_next(root, entry, "Demolitions_Master"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            continue;
        }
        demolitions_master_level_data* data = find_demolitions_master_data(name->text);
        if (data == nullptr)
        {
            data = Demolitions_master_level_data_pool.alloc(0);
            if (data == nullptr)
            {
                continue;
            }
        }
        if (!data->load(entry))
        {
            Demolitions_master_level_data_pool.free(data);
        }
        else if (is_dlc)
        {
            Dlc_demo_master_list.add(data); // reports a full list
        }
    }
    xml_table_close(root);
}

// 0x00A900C0 (:540)
void demolitions_master_unload_dlc()
{
    for (int i = 0; i < Dlc_demo_master_list.size(); i++)
    {
        Demolitions_master_level_data_pool.free(Dlc_demo_master_list[i]);
    }
    Dlc_demo_master_list.clear();
    Demolitions_master_string_pool.set_thread_ownership();
    Demolitions_master_string_pool.restore_to_mark(0);
}

// 0x00A17D00 (:552)
const char* const demolitions_master_node_get_name(const object_demolitions_master_node* const odmn)
{
    return odmn != nullptr && odmn->base_demolitions_master_data != nullptr ? odmn->base_demolitions_master_data->level_name : nullptr;
}
