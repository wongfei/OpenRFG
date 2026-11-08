// Original: volition/rfg/code/gameplay/activities/vrail_data.cpp
//
// gameflow.md GF4: the vehicle rail definitions from vehicle_rail_data.xtbl, plus each DLC bundle's table. The boot reads
// the table in activity_system_init, after the squads and vehicle groups it names.

#include "volition/rfg/code/gameplay/activities/vrail_data.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>

static const char* VRAIL_TABLE_FILE_NAME = "vehicle_rail_data.xtbl"; // 0x012DB88C
static const char* XML_VRAIL_ROOT_DATA_TAG = "VRailData"; // 0x012DB8A4
static const int MAX_DLC_VRAIL_DEFS = 3; // Magic number: 0x016573E0, the capacity rfg.exe's list is built with
static const int MAX_VRAIL_DEFS = 16; // Magic number: the pool's capacity, which vrail_data_get_def also checks
static const unsigned int VRAIL_NAME_POOL_BYTES = 0x100; // Magic number: 256 bytes of names

static object_pool<vehicle_rail_definition, MAX_VRAIL_DEFS, 1> Vrail_def_pool; // 0x02272F80
static char Vrail_name_string_pool_data[VRAIL_NAME_POOL_BYTES];
static string_pool Vrail_data_string_pool(Vrail_name_string_pool_data, VRAIL_NAME_POOL_BYTES, nullptr); // 0x02246A88
static farray<vehicle_rail_definition*, MAX_DLC_VRAIL_DEFS> Dlc_vrail_def_list; // 0x016573D8

// 0x00768E40 (.h:42)
squad_vehicle_pair::squad_vehicle_pair()
{
    reset();
}

// 0x00754030 (.h:48)
void squad_vehicle_pair::reset()
{
    squad_def = nullptr;
    veh_group = nullptr;
    respawn_delay = 0;
}

// 0x0077E210 (.h:61)
vehicle_rail_definition::vehicle_rail_definition()
{
    reset();
}

// 0x007726C0 (:72)
void vehicle_rail_definition::reset()
{
    name = nullptr;
    chase_vehicle_count = 0;
    chase_vehicle_list.clear();
    player_veh_spawn_group = nullptr;
    driver_squad_def = nullptr;
    vrail_flags = {};
}

// 0x0078AA20 (:83)
bool vehicle_rail_definition::load(xml_element* xml_root)
{
    reset(); // rfg.exe inlines it
    if (xml_root == nullptr)
    {
        return false;
    }
    xml_element* element = xml_find(xml_root, "Name");
    if (element == nullptr)
    {
        return false;
    }
    name = Vrail_data_string_pool.add_unique(element->text);
    if (name == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "ChaseCount");
    if (element == nullptr)
    {
        return false;
    }
    chase_vehicle_count = xml_get_int(element, 0);

    xml_element* front = xml_find(xml_root, "spawn_front");
    xml_element* behind = xml_find(xml_root, "spawn_behind");
    if (front == nullptr || behind == nullptr)
    {
        return false;
    }
    const bool spawn_front = xml_get_bool(front, true);
    vrail_flags.spawn_back = xml_get_bool(behind, false);
    if (spawn_front || vrail_flags.spawn_back)
    {
        vrail_flags.spawn_both = spawn_front && vrail_flags.spawn_back;
    }
    else
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__); // a rail that spawns nowhere
    }

    xml_element* list = xml_find(xml_root, "chase_vehicle_list");
    if (list == nullptr)
    {
        return false;
    }
    for (xml_element* entry = xml_find(list, "chase_vehicle_data"); entry != nullptr; entry = xml_find_next(list, entry, "chase_vehicle_data"))
    {
        squad_vehicle_pair pair;
        element = xml_find(entry, "squad_def");
        if (element == nullptr)
        {
            return false;
        }
        pair.squad_def = squad_definition_from_name(element->text);
        if (pair.squad_def == nullptr)
        {
            return false;
        }
        element = xml_find(entry, "vehicle_group");
        // Original bug kept: tests the entry, not the tag, so a missing vehicle_group reads through null.
        if (entry == nullptr)
        {
            return false;
        }
        pair.veh_group = sog_get_vehicle_group(element->text);
        if (pair.veh_group == nullptr)
        {
            return false;
        }
        element = xml_find(entry, "respawn_delay");
        if (element == nullptr)
        {
            return false;
        }
        pair.respawn_delay = xml_get_int(element, 0) * 1000; // seconds in the table
        if (pair.respawn_delay < 0)
        {
            pair.respawn_delay = 0;
        }
        chase_vehicle_list.add(pair);
        if (chase_vehicle_list.size() >= chase_vehicle_list.capacity())
        {
            break;
        }
    }
    if (chase_vehicle_list.size() <= 0)
    {
        return false;
    }
    element = xml_find(xml_root, "player_vehicle");
    if (element == nullptr)
    {
        return false;
    }
    player_veh_spawn_group = sog_get_vehicle_group(element->text);
    if (player_veh_spawn_group == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "driver_spawn_group");
    if (element == nullptr)
    {
        return false;
    }
    driver_squad_def = squad_definition_from_name(element->text);
    return driver_squad_def != nullptr;
}

// 0x007726E0 (:183)
const squad_vehicle_pair& vehicle_rail_definition::get_rand_veh_info() const
{
    const int count = chase_vehicle_list.size();
    return count == 1 ? chase_vehicle_list[0] : chase_vehicle_list[int_rand_range(0, count - 1)];
}

// 0x00772710 (:212)
const vehicle_rail_definition* vrail_data_get_def(int index)
{
    if ((unsigned int)index < (unsigned int)MAX_VRAIL_DEFS && (unsigned int)index < Vrail_def_pool.get_capacity())
    {
        return Vrail_def_pool.get_entry(index);
    }
    return nullptr;
}

// 0x0078ACB0 (:222)
const vehicle_rail_definition* vrail_data_get_def(const char* name)
{
    csspinlock_scope lock(Vrail_def_pool.thread_access);
    for (vehicle_rail_definition* def = Vrail_def_pool.get_first(0); def != nullptr; def = Vrail_def_pool.get_next(def))
    {
        if (v_stricmp(name, def->name) == 0)
        {
            return def;
        }
    }
    return nullptr;
}

// 0x0078AD20 (:239)
int vrail_data_get_type(const char* name)
{
    csspinlock_scope lock(Vrail_def_pool.thread_access);
    for (vehicle_rail_definition* def = Vrail_def_pool.get_first(0); def != nullptr; def = Vrail_def_pool.get_next(def))
    {
        if (v_stricmp(name, def->name) == 0)
        {
            return Vrail_def_pool.get_index(def);
        }
    }
    return -1;
}

// 0x007A31D0 (:263)
void vrail_data_system_init(int dlc_id)
{
    const bool is_dlc = dlc_id != -1;
    xml_element* root;
    if (is_dlc)
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, VRAIL_TABLE_FILE_NAME);
        root = xml_table_parse(filename);
        Vrail_data_string_pool.set_thread_ownership();
        Vrail_data_string_pool.mark();
    }
    else
    {
        {
            csspinlock_scope lock(Vrail_def_pool.thread_access);
            for (vehicle_rail_definition* def = Vrail_def_pool.get_first(0); def != nullptr; def = Vrail_def_pool.free(def))
            {
            }
        }
        Vrail_data_string_pool.clear();
        root = xml_table_parse(VRAIL_TABLE_FILE_NAME);
    }
    for (xml_element* entry = xml_find(root, XML_VRAIL_ROOT_DATA_TAG); entry != nullptr; entry = xml_find_next(root, entry, XML_VRAIL_ROOT_DATA_TAG))
    {
        vehicle_rail_definition* def = Vrail_def_pool.alloc(0);
        if (def == nullptr)
        {
            break;
        }
        if (!def->load(entry))
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            Vrail_def_pool.free(def);
        }
        else if (is_dlc)
        {
            Dlc_vrail_def_list.add(def); // base_array::add reports a full list
        }
    }
    xml_table_close(root);
}

// 0x0079B9A0 (:307)
void vrail_data_unload_dlc()
{
    Vrail_data_string_pool.set_thread_ownership();
    Vrail_data_string_pool.restore_to_mark(0);
    for (int i = 0; i < Dlc_vrail_def_list.size(); i++)
    {
        Vrail_def_pool.free(Dlc_vrail_def_list[i]);
    }
    Dlc_vrail_def_list.clear();
}
