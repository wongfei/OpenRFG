// Original: volition/rfg/code/gameplay/activities/delivery_common.cpp
//
// gameflow.md GF4: the Guerrilla Express definitions, read from activity_timed_delivery.xtbl.

#include "volition/rfg/code/gameplay/activities/delivery_common.h"

#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>

static object_pool<base_delivery_definition_data, 32, 1> Delivery_def_pool(&base_delivery_definition_data::reset); // 0x0226DE70
static farray<base_delivery_definition_data*, 5> Dlc_delivery_data_list; // 0x01656474
static string_pool Delivery_string_pool("Delivery String Pool"); // 0x02272F00
static char* Delivery_string_pool_data; // 0x022295F0
static const char* Delivery_message_radio_line_endings[NUM_DELIVERY_RADIO_LINE_ENDINGS] = {"start", "start_res", "win_res"}; // 0x012DB484

// 0x00786B20 (:83). rfg.exe: LTCG, delivery_name in EDI.
static base_delivery_definition_data* find_delivery_definition_slot(const char* delivery_name)
{
    for (base_delivery_definition_data* data = Delivery_def_pool.get_first(0); data != nullptr; data = Delivery_def_pool.get_next(data))
    {
        if (v_stricmp(delivery_name, data->delivery_name) == 0)
        {
            return data;
        }
    }
    return Delivery_def_pool.alloc(0);
}

// 0x0079A660 (:98). Original: only the first call for the base table parses it. A later one walks a null root.
void load_delivery_data(int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    xml_element* root = nullptr;
    char filename[256];
    if (is_dlc)
    {
        Delivery_string_pool.set_thread_ownership();
        Delivery_string_pool.mark();
        sprintf(filename, "dlc%02d_%s", dlc_id, "activity_timed_delivery.xtbl");
        root = xml_table_parse(filename);
    }
    else if (Delivery_string_pool_data == nullptr)
    {
        Delivery_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x200, 0)); // Magic number: 512 bytes of names
        if (Delivery_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the transporter string table");
        }
        Delivery_string_pool.init(Delivery_string_pool_data, 0x200, "Delivery String Pool", 0, 0);
        Delivery_string_pool.clear();
        root = xml_table_parse("activity_timed_delivery.xtbl");
    }
    for (xml_element* entry = xml_find(root, "Timed_Delivery"); entry != nullptr; entry = xml_find_next(root, entry, "Timed_Delivery"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            continue;
        }
        base_delivery_definition_data* data = find_delivery_definition_slot(name->text);
        if (data == nullptr)
        {
            break;
        }
        data->reset(); // rfg.exe inlines it
        if (!data->load(entry))
        {
            Delivery_def_pool.free(data);
        }
        else if (is_dlc)
        {
            if (Dlc_delivery_data_list.size() < Dlc_delivery_data_list.capacity())
            {
                Dlc_delivery_data_list.add(data);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            }
        }
    }
    xml_table_close(root);
}

// 0x0079A860 (:160)
void delivery_data_unload_dlc()
{
    for (int i = 0; i < Dlc_delivery_data_list.size(); i++)
    {
        Delivery_def_pool.free(Dlc_delivery_data_list[i]);
    }
    Dlc_delivery_data_list.clear();
    Delivery_string_pool.set_thread_ownership();
    Delivery_string_pool.restore_to_mark(0);
}

// 0x0075AF10 (:177). Original: subtitle, base_data and the rewards stay as they were.
void base_delivery_definition_data::reset()
{
    delivery_name = nullptr;
    name_crc = 0;
    time_to_complete = 0;
    time_to_fail = 0;
    par_time = 0;
    v_info = nullptr;
    vehicle_max_hitpoints = 0;
    alert_level = ALERT_LEVEL_GREEN;
    vehicle_speed = 0.0f;
    vehicle_health = 1.0f;
    end_node_radius = 0.0f;
    radio_lines[0] = INVALID_VOICE_LINE_HANDLE;
    radio_lines[1] = INVALID_VOICE_LINE_HANDLE;
    radio_lines[2] = INVALID_VOICE_LINE_HANDLE;
}

// 0x00786BE0 (:198)
bool base_delivery_definition_data::load(xml_element* xml_root)
{
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr)
    {
        return false;
    }
    delivery_name = Delivery_string_pool.add_unique(element->text);
    if (delivery_name == nullptr)
    {
        return false;
    }
    name_crc = crc_stri(delivery_name, 0);
    element = xml_find(xml_root, "Subtitle");
    if (element != nullptr)
    {
        subtitle = Delivery_string_pool.add_unique(element->text);
    }
    const char* base_name = xml_table_get_opt_string_ref(xml_root, "base_activity_data");
    base_data = base_name != nullptr ? activity_data_find_handle(base_name) : nullptr;
    element = xml_find(xml_root, "End_Radius");
    end_node_radius = element != nullptr ? xml_get_float(element, 0.0f) : 5.0f; // Magic number: five metres by default
    // The times are seconds in the table and milliseconds here. rfg.exe multiplies by the double 1000.0.
    element = xml_find(xml_root, "Time_To_Complete");
    if (element != nullptr)
    {
        time_to_complete = (int)((double)xml_get_float(element, 0.0f) * 1000.0);
    }
    element = xml_find(xml_root, "Time_To_Fail");
    if (element != nullptr)
    {
        time_to_fail = (int)((double)xml_get_float(element, 0.0f) * 1000.0);
    }
    element = xml_find(xml_root, "Par_Time");
    if (element != nullptr)
    {
        par_time = xml_get_int(element, 0) * 1000;
    }
    xml_element* overrides = xml_table_find_opt(xml_root, "Vehicle_Overrides");
    if (overrides == nullptr)
    {
        vehicle_speed = -1.0f;
        vehicle_max_hitpoints = -1;
        vehicle_health = 1.0f;
    }
    else
    {
        if (!xml_table_get_opt_float(vehicle_speed, overrides, "Vehicle_Speed"))
        {
            vehicle_speed = -1.0f;
        }
        if (!xml_table_get_opt_int(vehicle_max_hitpoints, overrides, "Vehicle_Hit_Points"))
        {
            vehicle_max_hitpoints = -1;
        }
        if (!xml_table_get_opt_float(vehicle_health, overrides, "Vehicle_Health"))
        {
            vehicle_health = 1.0f;
        }
    }
    element = xml_find(xml_root, "Alert_Level");
    alert_level = element != nullptr ? alm_resolve_alert_level(element->text) : ALERT_LEVEL_INVALID;
    if (alert_level == ALERT_LEVEL_INVALID)
    {
        alert_level = ALERT_LEVEL_GREEN;
    }
    load_standard_rewards(standard_rewards, xml_root);
    // Original: one of eight voice sets, picked at random when the table loads, whatever the delivery.
    char prefix[16];
    sprintf(prefix, "act_td_%02dA_", int_rand_range(1, 8));
    for (int i = 0; i < NUM_DELIVERY_RADIO_LINE_ENDINGS; i++)
    {
        char line_name[64];
        sprintf(line_name, "%s%s", prefix, Delivery_message_radio_line_endings[i]);
        radio_lines[i] = voice_line_handle_from_name(line_name);
    }
    return true;
}

// 0x0079A950 (:315)
void delivery_common_system_init()
{
    load_delivery_data(-1);
}

// 0x00786E90 (:321)
const base_delivery_definition_data* delivery_get_def_by_name(const char* delivery_name)
{
    const unsigned int crc = crc_stri(delivery_name, 0);
    for (base_delivery_definition_data* data = Delivery_def_pool.get_first(0); data != nullptr; data = Delivery_def_pool.get_next(data))
    {
        if (crc == data->name_crc && v_stricmp(delivery_name, data->delivery_name) == 0)
        {
            return data;
        }
    }
    return nullptr;
}

// 0x007690B0 (.h:48)
base_delivery_definition_data::base_delivery_definition_data()
{
    reset();
}
