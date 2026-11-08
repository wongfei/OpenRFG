// Original: volition/rfg/code/objects/object_house_arrest_node.cpp
//
// gameflow.md GF4: the house arrest's start point and its table. Registers the class, so zones now create these objects.

#include "volition/rfg/code/objects/object_house_arrest_node.h"

#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
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

static const char HOUSE_ARREST_FILE_NAME[] = "activity_house_arrest.xtbl"; // 0x013118B4
static const char HOUSE_ARREST_ROOT_TAG[] = "house_arrest"; // 0x013118D0
static const char HOUSE_ARREST_NAME_TAG[] = "Name"; // 0x013118E0

static object_pool<house_arrest_level_data, 32, 1> House_arrest_level_data_pool; // 0x033D4D80
static farray<house_arrest_level_data*, 4> Dlc_house_arrest_list; // 0x016AD270
static string_pool House_arrest_string_pool(nullptr); // 0x0328B6D8
static char* House_arrest_string_pool_data; // 0x0302F140
static object_pool<object_house_arrest_node, 16, 1> object_house_arrest_nodepool; // 0x034253C0

// 0x00A18150 (:210)
void types_object_house_arrest_node(signed char& type, signed char& subtype)
{
    type = OT_HOUSE_ARREST_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A18160 (:210)
int bpo_index_object_house_arrest_node()
{
    return 1;
}

// 0x00A18170 (:210)
const char* classname_object_house_arrest_node()
{
    return "object_house_arrest_node";
}

// 0x00A63BB0 (:210)
object* alloc_object_house_arrest_node()
{
    return object_house_arrest_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01077660), the pool's bytes and capacity.
static obj_class_instantiator object_house_arrest_node_instantiator("object_house_arrest_node", alloc_object_house_arrest_node, types_object_house_arrest_node, bpo_index_object_house_arrest_node, classname_object_house_arrest_node, object_house_arrest_node::pool_remaining, 0xAD0, 0x10);

// 0x00A33C70 (.h:31)
house_arrest_squad_data::house_arrest_squad_data()
{
    reset();
}

// 0x00A10090 (.h:37)
void house_arrest_squad_data::reset()
{
    squad_def = nullptr;
    veh_group = nullptr;
    spawn_delay_ms = 0;
}

// 0x00A90540 (:98)
bool house_arrest_squad_data::load(xml_element* xml_root)
{
    reset();
    xml_element* squad = xml_find(xml_root, "squad_def");
    xml_element* group = xml_find(xml_root, "vehicle_group");
    xml_element* delay = xml_find(xml_root, "spawn_delay");
    if (squad == nullptr || delay == nullptr)
    {
        return false;
    }
    squad_def = squad_definition_from_name(squad->text);
    if (group != nullptr)
    {
        veh_group = sog_get_vehicle_group(group->text);
    }
    const int seconds = xml_get_int(delay, 0);
    spawn_delay_ms = (seconds < 0 ? 0 : seconds) * 1000;
    return true;
}

// 0x00A7A430
house_arrest_level_data::house_arrest_level_data()
{
}

// 0x00A33C80 (.h:76)
void house_arrest_level_data::reset()
{
    level_name = nullptr;
    base_data = nullptr;
    informant_line = INVALID_VOICE_LINE_HANDLE;
    radio_message_start = INVALID_VOICE_LINE_HANDLE;
    informant_anim_action = ANIMATION_ACTION_NONE;
    level = 0;
    reinforcement_squad_list.clear();
    hostage_kill_time = 0;
}

// 0x00A905E0 (:128)
bool house_arrest_level_data::load(xml_element* xml_root)
{
    reset();
    level_name = House_arrest_string_pool.add_unique(xml_table_get_req_string_ref(xml_root, "Name"));
    if (level_name == nullptr)
    {
        return false;
    }
    const char* base_name = xml_table_get_opt_string_ref(xml_root, "base_activity_data");
    if (base_name != nullptr)
    {
        base_data = activity_data_find_handle(base_name);
    }
    xml_table_get_req_int(level, xml_root, "Level");
    if (level < 0)
    {
        level = 0;
    }
    xml_element* intro = xml_find(xml_root, "intro_info");
    if (intro != nullptr)
    {
        xml_element* line = xml_find(intro, "intro_voice_line");
        if (line == nullptr)
        {
            return false;
        }
        informant_line = voice_line_handle_from_name(line->text);
        if (informant_line == INVALID_VOICE_LINE_HANDLE)
        {
            return false;
        }
        xml_element* anim = xml_find(intro, "into_anim_action"); // Original spelling of the tag
        if (anim == nullptr)
        {
            return false;
        }
        informant_anim_action = vanim_lookup_action(anim->text);
        if (informant_anim_action == ANIMATION_ACTION_NONE)
        {
            return false;
        }
    }
    radio_message_start = voice_line_handle_from_name(xml_table_get_req_string_ref(xml_root, "Radio_Message"));
    xml_element* list = xml_find(xml_root, "reinforcement_list");
    if (list == nullptr)
    {
        return false;
    }
    for (xml_element* entry = xml_find(list, "reinforcement_data"); entry != nullptr; entry = xml_find_next(list, entry, "reinforcement_data"))
    {
        house_arrest_squad_data squad;
        if (squad.load(entry))
        {
            reinforcement_squad_list.add(squad);
        }
    }
    int kill_seconds;
    if (xml_table_get_opt_int(kill_seconds, xml_root, "Hostage_Kill_Time"))
    {
        hostage_kill_time = (kill_seconds < 0 ? 0 : kill_seconds) * 1000;
        if (hostage_kill_time < 1)
        {
            hostage_kill_time = 0;
        }
    }
    if (hostage_kill_time <= 0)
    {
        hostage_kill_time = 28800000; // Magic number: eight hours, the default where the table gives no time
    }
    load_standard_rewards(standard_rewards, xml_root);
    return true;
}

// 0x00A63B30 (:158). rfg.exe: LTCG, level_name in EDI.
static house_arrest_level_data* find_house_arrest_data(const char* level_name)
{
    for (house_arrest_level_data* data = House_arrest_level_data_pool.get_first(0); data != nullptr; data = House_arrest_level_data_pool.get_next(data))
    {
        if (v_stricmp(data->level_name, level_name) == 0)
        {
            return data;
        }
    }
    return nullptr;
}

// 0x00A18190 (:211)
object_house_arrest_node::object_house_arrest_node()
{
}

// 0x00A181C0
object_house_arrest_node::~object_house_arrest_node()
{
}

// 0x00A100A0 (.h:93)
const char* object_house_arrest_node::get_classname_static()
{
    return "object_house_arrest_node";
}

// 0x00A100B0 (.h:93)
int object_house_arrest_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A181B0 (.h:93)
const char* object_house_arrest_node::get_classname() const
{
    return "object_house_arrest_node";
}

// 0x00A18180 (:210)
unsigned int object_house_arrest_node::get_classname_hash()
{
    return object_house_arrest_node_instantiator.this_hash;
}

// 0x00A3B2B0 (:210)
int object_house_arrest_node::pool_remaining()
{
    return object_house_arrest_nodepool.get_free();
}

// 0x00A907D0 (:210)
void object_house_arrest_node::pool_free()
{
    object_house_arrest_nodepool.free(this);
}

// 0x00AA72C0 (:234)
bool object_house_arrest_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(HOUSE_ARREST_PROP_TYPE, base_house_arrest_data->level_name);
        if (ha_flags.completed)
        {
            pblock.set(ACTIVITY_COMPLETED_PROP, true);
        }
        if (disabled)
        {
            pblock.set(ACTIVITY_PROP_DISABLED, true);
        }
        return true;
    }
    base_house_arrest_data = nullptr;
    ha_flags.completed = false;
    char type_name[256];
    type_name[0] = '\0';
    pblock.get(HOUSE_ARREST_PROP_TYPE, type_name);
    base_house_arrest_data = find_house_arrest_data(type_name);
    bool flag;
    if (pblock.get_optional(ACTIVITY_COMPLETED_PROP, flag))
    {
        ha_flags.completed = flag;
    }
    if (base_house_arrest_data == nullptr)
    {
        return false;
    }
    if (!pblock.get_optional(ACTIVITY_PROP_DISABLED, disabled))
    {
        disabled = false;
    }
    return true;
}

// 0x00A18200 (:282)
bool object_house_arrest_node::stream(bool load)
{
    if (load)
    {
        activity_house_arrest_report_streaming(this);
    }
    else if (obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A181F0 (:228)
obj_serialize_mode object_house_arrest_node::serialize_mode()
{
    return OSM_FULL;
}

// 0x00754580 (.h:107)
const house_arrest_level_data* object_house_arrest_node::get_house_arrest_data() const
{
    return base_house_arrest_data;
}

// 0x00A181D0 (:217)
const base_activity_data* object_house_arrest_node::get_base_data() const
{
    return base_house_arrest_data != nullptr ? base_house_arrest_data->base_data : nullptr;
}

// 0x00A63BC0 (:297)
void object_house_arrest_node::set_completed_flag()
{
    ha_flags.completed = true;
    if (get_parent() != 0xFFFFFFFF)
    {
        object* parent_objp = World.object_from_handle(get_parent());
        if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_HOUSE_ARREST_NODE)
        {
            static_cast<object_house_arrest_node*>(parent_objp)->set_completed_flag_parent();
        }
    }
    set_completed_flag_child(); // rfg.exe inlines it
}

// 0x00A3B2E0 (:304)
void object_house_arrest_node::set_completed_flag_parent()
{
    object_house_arrest_node* node = this;
    while (true)
    {
        node->ha_flags.completed = true;
        if (node->get_parent() == 0xFFFFFFFF)
        {
            return;
        }
        object* parent_objp = World.object_from_handle(node->get_parent());
        if (parent_objp == nullptr || parent_objp->get_obj_type() != OT_HOUSE_ARREST_NODE)
        {
            return;
        }
        node = static_cast<object_house_arrest_node*>(parent_objp);
    }
}

// 0x00A3B310 (:350)
void object_house_arrest_node::set_completed_flag_child()
{
    ha_flags.completed = true;
    object* child = child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_HOUSE_ARREST_NODE)
        {
            static_cast<object_house_arrest_node*>(child)->set_completed_flag_child();
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            return;
        }
    }
}

// 0x00A18230 (:317)
bool object_house_arrest_node::can_spawn() const
{
    if (mission_final_mission_complete())
    {
        return true;
    }
    if (disabled || ha_flags.completed)
    {
        return false;
    }
    const district* dp = district_get_from_pos(pos);
    return dp == nullptr || !dp->is_liberated();
}

// 0x007545A0 (.h:132)
void object_house_arrest_node::enable()
{
    disabled = false;
}

// 0x007545B0 (.h:134)
bool object_house_arrest_node::is_disabled() const
{
    return disabled;
}

// 0x00AA7420 (:371)
void house_arrest_system_init(int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    xml_element* root;
    if (is_dlc)
    {
        char filename[256];
        House_arrest_string_pool.set_thread_ownership();
        House_arrest_string_pool.mark();
        sprintf(filename, "dlc%02d_%s", dlc_id, HOUSE_ARREST_FILE_NAME);
        root = xml_table_parse(filename);
    }
    else
    {
        if (House_arrest_string_pool_data == nullptr)
        {
            House_arrest_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x200, 0)); // Magic number: 512 bytes of names
            if (House_arrest_string_pool_data == nullptr)
            {
                keen::debug::nativePrintArgs("Insufficient memory to allocate the house arrest string table");
            }
            House_arrest_string_pool.init(House_arrest_string_pool_data, 0x200, nullptr, 0, 0);
            House_arrest_string_pool.clear();
            House_arrest_level_data_pool.clear();
        }
        root = xml_table_parse(HOUSE_ARREST_FILE_NAME);
    }
    for (xml_element* entry = xml_find(root, HOUSE_ARREST_ROOT_TAG); entry != nullptr; entry = xml_find_next(root, entry, HOUSE_ARREST_ROOT_TAG))
    {
        xml_element* name = xml_find(entry, HOUSE_ARREST_NAME_TAG);
        if (name == nullptr)
        {
            continue;
        }
        house_arrest_level_data* data = find_house_arrest_data(name->text);
        if (data == nullptr)
        {
            data = House_arrest_level_data_pool.alloc(0);
            if (data == nullptr)
            {
                continue;
            }
        }
        if (!data->load(entry))
        {
            House_arrest_level_data_pool.free(data);
        }
        else if (is_dlc)
        {
            Dlc_house_arrest_list.add(data);
        }
    }
    xml_table_close(root);
}

// 0x00A907E0 (:434)
void house_arrest_unload_dlc()
{
    for (int i = 0; i < Dlc_house_arrest_list.size(); i++)
    {
        House_arrest_level_data_pool.free(Dlc_house_arrest_list[i]);
    }
    Dlc_house_arrest_list.clear();
    House_arrest_string_pool.set_thread_ownership();
    House_arrest_string_pool.restore_to_mark(0);
}

// 0x00A18270 (:445)
const char* const house_arrest_get_name(const object_house_arrest_node* const ohan)
{
    return ohan != nullptr && ohan->get_house_arrest_data() != nullptr ? ohan->get_house_arrest_data()->level_name : nullptr;
}
