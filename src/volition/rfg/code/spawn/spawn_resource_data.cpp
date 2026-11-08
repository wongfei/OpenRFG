// Original: volition/rfg/code/spawn/spawn_resource_data.cpp
//
// The sets of spawn_resource.xtbl (stub_impl.md S9): for a zone, how many squads of each team and how many human and
// vehicle slots of which spawn groups are wanted. Looked up by name through a hash table of pool indices; "Default" is
// made up here when the table has no such entry.

#include "volition/rfg/code/spawn/spawn_resource_data.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/memory/object_pool.inl"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/hash_table.inl"
#include "volition/vlib/code/util/string_pool.h"

#include <stdio.h>
#include <string.h>

// Magic numbers: rfg.exe's limits on the sum of a set's reservations (squads plus human slots, vehicle slots).
static const int MAX_RESERVED_HUMAN_SLOTS = 16;
static const int MAX_RESERVED_VEHICLE_SLOTS = 7;

// ---- file-static globals
static object_pool<spawn_resource_data, 44, 1> Spawn_resource_data_pool; // 0x037457D0
static str_data_hash_table_cached<int, 44, unsigned char> Spawn_res_data_group_hash; // 0x016B8398
static string_pool Spawn_res_data_name_string_pool("Spawn_res_data_name_string_pool");
static char* Spawn_res_data_name_string_pool_data;
static const spawn_resource_data* Default_spawn_redource_data; // [sic]

// 0x00B4C3C0 (.h:74)
spawn_resource_data::spawn_resource_data()
{
    reset();
}

// The set at a pool index, null out of range.
static spawn_resource_data* resource_data_at(int index)
{
    // Magic number: the pool holds 44.
    if (index < 0 || index >= 44)
    {
        return nullptr;
    }
    return Spawn_resource_data_pool.get_entry(index);
}

// 0x00B516D0 (:66)
// LTCG: name in ESI.
static spawn_resource_data* find_resource_data_internal(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    int index;
    if (!Spawn_res_data_group_hash.get(name, index))
    {
        return nullptr;
    }
    return resource_data_at(index);
}

// 0x00B56F60 (:82)
void spawn_res_dlc_unload()
{
    Spawn_resource_data_pool.thread_access.enter();
    spawn_resource_data* data = Spawn_resource_data_pool.get_first(0);
    while (data != nullptr)
    {
        spawn_resource_data* next = Spawn_resource_data_pool.get_next(data);
        if (data->dlc_resource)
        {
            Spawn_res_data_group_hash.remove(data->name);
            Spawn_resource_data_pool.free(data);
        }
        data = next;
    }
    Spawn_resource_data_pool.thread_access.leave();
    Spawn_res_data_name_string_pool.set_thread_ownership();
    Spawn_res_data_name_string_pool.restore_to_mark(0);
}

// 0x00B57050 (:101)
void spawn_load_resource_data(int dlc_id)
{
    const bool dlc = dlc_id != -1;
    xml_element* root;
    if (dlc)
    {
        Spawn_res_data_name_string_pool.set_thread_ownership();
        Spawn_res_data_name_string_pool.restore_to_mark(0);
        char name[256];
        sprintf(name, "dlc%02d_%s", dlc_id, "spawn_resource.xtbl");
        root = xml_table_parse(name);
    }
    else
    {
        // Magic string: the default set, empty until the table has an entry of that name.
        spawn_resource_data* def = find_resource_data_internal("Default");
        if (def == nullptr)
        {
            def = Spawn_resource_data_pool.alloc(0);
            if (def == nullptr)
            {
                return;
            }
        }
        def->reset();
        def->name = "Default";
        Default_spawn_redource_data = def;
        Spawn_res_data_group_hash.insert_key_data(def->name, Spawn_resource_data_pool.get_index(def));
        root = xml_table_parse("spawn_resource.xtbl");
    }
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* element = xml_find(root, "spawn_resource_entry"); element != nullptr; element = xml_find_next(root, element, "spawn_resource_entry"))
    {
        xml_element* name_element = xml_find(element, "Name");
        if (name_element == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        spawn_resource_data* data = find_resource_data_internal(name_element->text);
        if (data != nullptr)
        {
            data->load(element, dlc);
            continue;
        }
        data = Spawn_resource_data_pool.alloc(0);
        if (data == nullptr)
        {
            continue;
        }
        data->reset();
        if (data->load(element, dlc))
        {
            Spawn_res_data_group_hash.insert_key_data(data->name, Spawn_resource_data_pool.get_index(data));
        }
        else
        {
            Spawn_resource_data_pool.free(data);
        }
    }
    xml_table_close(root);
    if (!dlc)
    {
        Spawn_res_data_name_string_pool.mark();
    }
}

// 0x00B46F90 (.h:29)
human_reservation_info::human_reservation_info()
{
    reset();
}

// 0x00B43D60 (:187)
void human_reservation_info::reset()
{
    spawn_group = nullptr;
    count = 0;
}

// 0x00B51720 (:193)
bool human_reservation_info::load(xml_element* xml_root)
{
    reset();
    xml_element* group = xml_find(xml_root, "npc_group");
    xml_element* reservation = xml_find(xml_root, "reservation_count");
    if (group == nullptr || reservation == nullptr)
    {
        return false;
    }
    spawn_group = sog_get_human_group(group->text);
    if (spawn_group == nullptr)
    {
        return false;
    }
    count = xml_get_int(reservation, 0);
    return count > 0;
}

// 0x00B46FA0 (.h:47)
vehicle_reservation_info::vehicle_reservation_info()
{
    reset();
}

// 0x00B43D70 (:219)
void vehicle_reservation_info::reset()
{
    spawn_group = nullptr;
    count = 0;
}

// 0x00B51790 (:226)
bool vehicle_reservation_info::load(xml_element* xml_root)
{
    reset();
    xml_element* group = xml_find(xml_root, "vehicle_group");
    xml_element* reservation = xml_find(xml_root, "reservation_count");
    if (group == nullptr || reservation == nullptr)
    {
        return false;
    }
    spawn_group = sog_get_vehicle_group(group->text);
    if (spawn_group == nullptr)
    {
        return false;
    }
    count = xml_get_int(reservation, 0);
    return count > 0;
}

// 0x00B48D90 (:251)
void spawn_resource_data::reset()
{
    name = nullptr;
    dlc_resource = false;
    for (int i = 0; i < 4; i++)
    {
        team_squad_counts[i] = 0;
    }
    required_human_slots.clear();
    required_vehicle_slots.clear();
}

// 0x00B51800 (:267)
bool spawn_resource_data::load(xml_element* xml_root, bool dlc_only)
{
    reset();
    dlc_resource = dlc_only;
    xml_element* name_element = xml_find(xml_root, "Name");
    if (name_element == nullptr)
    {
        return false;
    }
    name = Spawn_res_data_name_string_pool.add_unique(name_element->text);
    if (name == nullptr)
    {
        return false;
    }
    xml_element* edf = xml_find(xml_root, "edf_population");
    xml_element* guerrilla = xml_find(xml_root, "gue_population");
    xml_element* marauder = xml_find(xml_root, "mar_population");
    if (edf == nullptr || guerrilla == nullptr || marauder == nullptr)
    {
        return false;
    }
    team_squad_counts[HUMAN_TEAM_EDF] = xml_get_int(edf, 0);
    team_squad_counts[HUMAN_TEAM_GUERILLA] = xml_get_int(guerrilla, 0);
    team_squad_counts[HUMAN_TEAM_MARAUDER] = xml_get_int(marauder, 0);
    for (int i = 0; i < 4; i++)
    {
        if (team_squad_counts[i] < 0)
        {
            team_squad_counts[i] = 0;
        }
    }
    required_vehicle_slots.clear();
    xml_element* vehicles = xml_find(xml_root, "vehicle_list");
    if (vehicles != nullptr)
    {
        vehicle_reservation_info info;
        for (xml_element* element = xml_find(vehicles, "vehicle_info"); element != nullptr; element = xml_find_next(vehicles, element, "vehicle_info"))
        {
            if (info.load(element))
            {
                required_vehicle_slots.add(info);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
    required_human_slots.clear();
    xml_element* humans = xml_find(xml_root, "npc_list");
    if (humans != nullptr)
    {
        human_reservation_info info;
        for (xml_element* element = xml_find(humans, "npc_info"); element != nullptr; element = xml_find_next(humans, element, "npc_info"))
        {
            if (info.load(element))
            {
                required_human_slots.add(info);
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
    }
    return check_for_allocation_errors();
}

// 0x00B48DC0 (:341)
bool spawn_resource_data::check_for_allocation_errors() const
{
    int total = 0;
    for (int i = 0; i < 4; i++)
    {
        if (team_squad_counts[i] < 0)
        {
            return false;
        }
        total += team_squad_counts[i];
    }
    if (total > MAX_RESERVED_HUMAN_SLOTS)
    {
        return false;
    }
    for (int i = 0; i < required_human_slots.size(); i++)
    {
        total += required_human_slots[i].count;
    }
    if (total > MAX_RESERVED_HUMAN_SLOTS)
    {
        return false;
    }
    int vehicles = 0;
    for (int i = 0; i < required_vehicle_slots.size(); i++)
    {
        vehicles += required_vehicle_slots[i].count;
    }
    return vehicles <= MAX_RESERVED_VEHICLE_SLOTS;
}

// 0x00B5CC10 (:379)
void spawn_res_data_system_init()
{
    if (Spawn_res_data_name_string_pool_data == nullptr)
    {
        // Magic number: 512 bytes of names.
        Spawn_res_data_name_string_pool_data = (char*)Permanent_mempool.alloc(0x200, 4);
        if (Spawn_res_data_name_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the spawn resource data name string pool");
        }
        Spawn_res_data_name_string_pool.init(Spawn_res_data_name_string_pool_data, 0x200, "Spawn Resource Data", 0, 0);
    }
    Spawn_resource_data_pool.clear();
    Spawn_res_data_name_string_pool.clear();
    Spawn_res_data_group_hash.clear();
    Default_spawn_redource_data = nullptr;
    spawn_load_resource_data(-1);
}

// 0x00B43D80 (:399)
void spawn_res_data_level_init_pre()
{
}

// 0x00B51A10 (:405)
const spawn_resource_data* spawn_res_data_find(const char* name)
{
    const spawn_resource_data* data = find_resource_data_internal(name);
    return data != nullptr ? data : Default_spawn_redource_data;
}

// 0x00B43D90 (:418)
const spawn_resource_data* spawn_res_data_default()
{
    return Default_spawn_redource_data;
}

// 0x00B43DA0 (:424)
const spawn_resource_data* spawn_res_data_current()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return nullptr;
    }
    obj_zone* zone = World.get_zone_by_pos(pp->pos);
    if (zone == nullptr || zone->spawn_res_data == nullptr)
    {
        return Default_spawn_redource_data;
    }
    return zone->spawn_res_data;
}
