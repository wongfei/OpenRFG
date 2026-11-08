// Original: volition/rfg/code/spawn/spawn_object_group.cpp
//
// The spawn groups of spawn_group_human.xtbl and spawn_group_vehicle.xtbl (stub_impl.md S9): named lists of human and
// vehicle infos that spawn nodes, squads and ambient spawning pick from, in the permanent pool, a DLC's in the level
// pool. Each team has a default group; a DLC group with "default" in its name overrides it.

#include "volition/rfg/code/spawn/spawn_object_group.h"

#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle/vehicle_table_parser.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.inl"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Magic numbers: the longest spawn group list and name.
static const int MAX_SPAWN_GROUP_LIST = 256;
static const int MAX_SPAWN_GROUP_NAME = 256;

// ---- public globals
object_pool<human_spawn_group_info, 64, 1> Human_spawn_group_pool; // 0x03748390
object_pool<vehicle_spawn_group_info, 136, 1> Vehicle_spawn_group_pool; // 0x03742480
string_pool Spawn_group_string_pool("Spawn_group_string_pool"); // 0x0374ACC8

// ---- file-static globals
// Original: indexed by team (guerrilla, EDF, civilian, marauder): civilians take the guerrilla's group.
static const char* const DEFAULT_SPAWN_GROUP_NAMES[NUM_HUMAN_TEAMS] = {"Default Guerilla", "Default EDF", "Default Guerilla", "Default Marauder"}; // 0x016B6990
static const human_spawn_group_info* Default_human_spawn_groups[NUM_HUMAN_TEAMS]; // 0x0373EF80
static const human_spawn_group_info* DLC_default_human_spawn_groups[NUM_HUMAN_TEAMS]; // 0x0373EFF4
static const vehicle_spawn_group_info* DLC_default_vehicle_spawn_groups[NUM_HUMAN_TEAMS]; // 0x0373F004
static const vehicle_spawn_group_info* Default_vehicle_spawn_groups[NUM_HUMAN_TEAMS]; // 0x03742448
static char* Spawn_group_string_pool_data; // 0x03742470

// 0x00B42DC0 (:103)
// LTCG: input_string in EAX.
static bool copy_to_lower(const char* input_string, char* buffer)
{
    buffer[0] = '\0';
    if (input_string == nullptr)
    {
        return false;
    }
    for (int i = 0; i < MAX_SPAWN_GROUP_NAME; i++)
    {
        if (input_string[i] == '\0')
        {
            buffer[i] = '\0';
            return true;
        }
        buffer[i] = (char)tolower(input_string[i]);
    }
    return false;
}

// 0x00B42E20 (:127)
static void clear_dlc_default_human_spawn_group()
{
    for (int i = 0; i < NUM_HUMAN_TEAMS; i++)
    {
        DLC_default_human_spawn_groups[i] = nullptr;
    }
}

// 0x00B42E40 (:137)
static void clear_dlc_default_vehicle_spawn_group()
{
    for (int i = 0; i < NUM_HUMAN_TEAMS; i++)
    {
        DLC_default_vehicle_spawn_groups[i] = nullptr;
    }
}

// 0x00B55D00 (:147)
static void load_default_spawn_group()
{
    for (int i = 0; i < NUM_HUMAN_TEAMS; i++)
    {
        if (DEFAULT_SPAWN_GROUP_NAMES[i] == nullptr)
        {
            Default_human_spawn_groups[i] = nullptr;
            Default_vehicle_spawn_groups[i] = nullptr;
        }
        else
        {
            Default_human_spawn_groups[i] = sog_get_human_group(DEFAULT_SPAWN_GROUP_NAMES[i]);
            Default_vehicle_spawn_groups[i] = sog_get_vehicle_group(DEFAULT_SPAWN_GROUP_NAMES[i]);
        }
    }
}

// 0x00B4C380 (.h:36)
human_spawn_group_info::human_spawn_group_info()
{
    reset();
}

// 0x00B47E10 (:186)
void human_spawn_group_info::reset()
{
    h_info_list.clear();
    group_name = nullptr;
    name_crc = 0;
    homogeneous_team = true;
}

// 0x00B47E20 (:194)
const human_info* human_spawn_group_info::get_index(int i) const
{
    if (i > -1 && i < h_info_list.size())
    {
        return h_info_list[i];
    }
    return nullptr;
}

int human_spawn_group_info::get_list_size() const
{
    return h_info_list.size();
}

// 0x00B4F7E0 (:202)
// A group of one info, named after it.
bool human_spawn_group_info::load(const human_info* h_info)
{
    group_name = h_info->name;
    name_crc = crc_stri(group_name, 0);
    const human_info** data = (const human_info**)Permanent_mempool.alloc(4, 4);
    if (data == nullptr)
    {
        return false;
    }
    h_info_list.init(data, 1, 0);
    h_info_list.add(h_info);
    homogeneous_team = true;
    return true;
}

// 0x00B4F870 (:221)
bool human_spawn_group_info::load(xml_element* xml_group_root, mempool* pool)
{
    farray<human_info const*, MAX_SPAWN_GROUP_LIST> list;
    xml_element* name_element = xml_find(xml_group_root, "name");
    if (name_element == nullptr)
    {
        return false;
    }
    group_name = Spawn_group_string_pool.add_unique(name_element->text);
    name_crc = crc_stri(group_name, 0);
    xml_element* list_element = xml_find(xml_group_root, "human_list");
    if (list_element == nullptr)
    {
        return false;
    }
    for (xml_element* cur = xml_find(list_element, "human_type"); cur != nullptr; cur = xml_find_next(list_element, cur, "human_type"))
    {
        const human_info* info = human_info_find(cur->text);
        if (info == nullptr)
        {
            continue;
        }
        if (list.size() >= MAX_SPAWN_GROUP_LIST)
        {
            // Original: a full list loads as a success with nothing stored.
            return true;
        }
        list.add_unique(info);
        if (homogeneous_team && list.size() > 1)
        {
            homogeneous_team = list[0]->default_team == info->default_team;
        }
    }
    if (list.size() < 1)
    {
        return false;
    }
    const human_info** data = (const human_info**)pool->alloc(list.size() * 4, 4);
    if (data == nullptr)
    {
        return false;
    }
    h_info_list.init(data, list.size(), 0);
    for (int i = 0; i < list.size(); i++)
    {
        h_info_list.add(list[i]);
    }
    return true;
}

// 0x00B47E40 (:275)
const human_info* human_spawn_group_info::get_random_element() const
{
    const unsigned int count = (unsigned int)h_info_list.size();
    if (count == 0)
    {
        return nullptr;
    }
    if (count == 1)
    {
        return h_info_list[0];
    }
    return h_info_list[(int)(vrand() % count)];
}

// 0x00B47E70 (:293)
human_teams human_spawn_group_info::get_team() const
{
    if (homogeneous_team && h_info_list.size() > 0)
    {
        return h_info_list[0]->default_team;
    }
    return HUMAN_TEAM_NONE;
}

// 0x00B4C3A0 (.h:84)
vehicle_spawn_group_info::vehicle_spawn_group_info()
{
    reset();
}

// 0x00B47E90 (:305)
void vehicle_spawn_group_info::reset()
{
    v_info_list.clear();
    group_name = nullptr;
    name_crc = 0;
}

// 0x00B47EA0 (:313)
const vehicle_info* vehicle_spawn_group_info::get_index(int i) const
{
    if (i > -1 && i < v_info_list.size())
    {
        return v_info_list[i];
    }
    return nullptr;
}

// 0x00769310 (.h:110)
int vehicle_spawn_group_info::get_list_size() const
{
    return v_info_list.size();
}

// 0x00B4F9F0 (:320)
bool vehicle_spawn_group_info::load(const vehicle_info* v_info)
{
    group_name = v_info->name;
    name_crc = crc_stri(group_name, 0);
    const vehicle_info** data = (const vehicle_info**)Permanent_mempool.alloc(4, 4);
    if (data == nullptr)
    {
        return false;
    }
    v_info_list.init(data, 1, 0);
    v_info_list.add(v_info);
    return true;
}

// 0x00B4FA70 (:337)
// Infos without a stream resource are left out.
bool vehicle_spawn_group_info::load(xml_element* xml_group_root, mempool* pool)
{
    farray<vehicle_info const*, MAX_SPAWN_GROUP_LIST> list;
    xml_element* name_element = xml_find(xml_group_root, "name");
    if (name_element == nullptr)
    {
        return false;
    }
    group_name = Spawn_group_string_pool.add_unique(name_element->text);
    name_crc = crc_stri(group_name, 0);
    xml_element* list_element = xml_find(xml_group_root, "vehicle_list");
    if (list_element == nullptr)
    {
        return false;
    }
    for (xml_element* cur = xml_find(list_element, "vehicle_type"); cur != nullptr; cur = xml_find_next(list_element, cur, "vehicle_type"))
    {
        const vehicle_info* info = vehicle_get_info_from_name(cur->text);
        if (info == nullptr || info->srid == 0)
        {
            continue;
        }
        if (list.size() >= MAX_SPAWN_GROUP_LIST)
        {
            // Original: a full list loads as a success with nothing stored.
            return true;
        }
        list.add_unique(info);
    }
    if (list.size() < 1)
    {
        return false;
    }
    const vehicle_info** data = (const vehicle_info**)pool->alloc(list.size() * 4, 4);
    if (data == nullptr)
    {
        return false;
    }
    v_info_list.init(data, list.size(), 0);
    for (int i = 0; i < list.size(); i++)
    {
        v_info_list.add(list[i]);
    }
    return true;
}

// 0x00B47EC0 (:387)
const vehicle_info* vehicle_spawn_group_info::get_random_element() const
{
    const unsigned int count = (unsigned int)v_info_list.size();
    if (count == 0)
    {
        return nullptr;
    }
    if (count == 1)
    {
        return v_info_list[0];
    }
    return v_info_list[(int)(vrand() % count)];
}

// 0x00B47EF0 (:405)
human_teams vehicle_spawn_group_info::get_team() const
{
    if (v_info_list.size() < 1)
    {
        return HUMAN_TEAM_NONE;
    }
    return v_info_list[0]->default_team;
}

// 0x00B47F10 (:412)
bool vehicle_spawn_group_info::has_same_srid() const
{
    const int count = v_info_list.size();
    if (count < 1)
    {
        return false;
    }
    for (int i = 1; i < count; i++)
    {
        if (v_info_list[i]->srid != v_info_list[0]->srid)
        {
            return false;
        }
    }
    return true;
}

// 0x00B47F60 (:432)
bool vehicle_spawn_group_info::has_same_srid_or_family() const
{
    const int count = v_info_list.size();
    if (count < 1)
    {
        return false;
    }
    const vehicle_info* first = v_info_list[0];
    for (int i = 1; i < count; i++)
    {
        const vehicle_info* info = v_info_list[i];
        if (info->srid != first->srid && (first->variant_family == INVALID_VEHICLE_FAMILY_ID || info->variant_family == INVALID_VEHICLE_FAMILY_ID || info->variant_family != first->variant_family))
        {
            return false;
        }
    }
    return true;
}

// 0x00B5FC90 (:471)
void sog_system_init()
{
    if (Spawn_group_string_pool_data == nullptr)
    {
        // Magic number: 3.5 KB of group names.
        Spawn_group_string_pool_data = (char*)Permanent_mempool.alloc(0xE00, 0);
        if (Spawn_group_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the spawn string table");
        }
        Spawn_group_string_pool.init(Spawn_group_string_pool_data, 0xE00, "Spawn Group strings", 0, 0);
    }
    Spawn_group_string_pool.clear();
    for (int i = 0; i < NUM_HUMAN_TEAMS; i++)
    {
        Default_vehicle_spawn_groups[i] = nullptr;
        Default_human_spawn_groups[i] = nullptr;
    }
    load_human_spawn_groups(-1);
    load_vehicle_spawn_groups(-1);
    load_default_spawn_group();
}

// 0x00B4FBD0 (:503)
const vehicle_spawn_group_info* sog_get_vehicle_group(const char* group_name)
{
    const unsigned int crc = crc_stri(group_name, 0);
    Vehicle_spawn_group_pool.thread_access.enter();
    for (vehicle_spawn_group_info* group = Vehicle_spawn_group_pool.get_first(0); group != nullptr; group = Vehicle_spawn_group_pool.get_next(group))
    {
        if (crc == group->name_crc && v_stricmp(group_name, group->group_name) == 0)
        {
            Vehicle_spawn_group_pool.thread_access.leave();
            return group;
        }
    }
    Vehicle_spawn_group_pool.thread_access.leave();
    return nullptr;
}

// 0x00B4FCB0 (:526)
const human_spawn_group_info* sog_get_human_group(const char* group_name)
{
    const unsigned int crc = crc_stri(group_name, 0);
    Human_spawn_group_pool.thread_access.enter();
    for (human_spawn_group_info* group = Human_spawn_group_pool.get_first(0); group != nullptr; group = Human_spawn_group_pool.get_next(group))
    {
        if (crc == group->name_crc && v_stricmp(group_name, group->group_name) == 0)
        {
            Human_spawn_group_pool.thread_access.leave();
            return group;
        }
    }
    Human_spawn_group_pool.thread_access.leave();
    return nullptr;
}

// 0x00B47FD0 (:551)
human_teams sog_get_team(const human_spawn_group_info* hum_group_info)
{
    const int count = hum_group_info->h_info_list.size();
    if (count < 1)
    {
        return HUMAN_TEAM_NONE;
    }
    const human_teams team = hum_group_info->h_info_list[0]->default_team;
    for (int i = 1; i < count; i++)
    {
        if (hum_group_info->h_info_list[i]->default_team != team)
        {
            return HUMAN_TEAM_NONE;
        }
    }
    return team;
}

// 0x00B5A5C0 (:576)
void load_human_spawn_groups(int dlc_id)
{
    const bool dlc = dlc_id != -1;
    mempool* pool;
    xml_element* root;
    if (dlc)
    {
        pool = &Level_mempool;
        char name[256];
        sprintf(name, "dlc%02d_%s", dlc_id, "spawn_group_human.xtbl");
        root = xml_table_parse(name);
    }
    else
    {
        pool = &Permanent_mempool;
        Human_spawn_group_pool.clear();
        root = xml_table_parse("spawn_group_human.xtbl");
    }
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* element = xml_find(root, "spawn_group_human"); element != nullptr; element = xml_find_next(root, element, "spawn_group_human"))
    {
        human_spawn_group_info* group = Human_spawn_group_pool.alloc(0);
        if (group == nullptr)
        {
            break;
        }
        group->reset();
        if (!group->load(element, pool))
        {
            Human_spawn_group_pool.free(group);
        }
        else if (dlc)
        {
            Dlc_human_spawn_group_list.add(group);
        }
    }
    xml_table_close(root);
    if (!dlc)
    {
        clear_dlc_default_human_spawn_group();
    }
}

// 0x00B5A710 (:659)
void load_vehicle_spawn_groups(int dlc_id)
{
    const bool dlc = dlc_id != -1;
    mempool* pool;
    xml_element* root;
    if (dlc)
    {
        pool = &Level_mempool;
        char name[256];
        sprintf(name, "dlc%02d_%s", dlc_id, "spawn_group_vehicle.xtbl");
        root = xml_table_parse(name);
    }
    else
    {
        pool = &Permanent_mempool;
        Vehicle_spawn_group_pool.clear();
        root = xml_table_parse("spawn_group_vehicle.xtbl");
    }
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* element = xml_find(root, "spawn_group_vehicle"); element != nullptr; element = xml_find_next(root, element, "spawn_group_vehicle"))
    {
        vehicle_spawn_group_info* group = Vehicle_spawn_group_pool.alloc(0);
        if (group == nullptr)
        {
            break;
        }
        group->reset();
        if (!group->load(element, pool))
        {
            Vehicle_spawn_group_pool.free(group);
        }
        else if (dlc)
        {
            Dlc_vehicle_spawn_group_list.add(group);
        }
    }
    xml_table_close(root);
    if (!dlc)
    {
        clear_dlc_default_vehicle_spawn_group();
    }
}

// 0x00B48020 (:735)
void sog_load_dlc_default_spawn_group()
{
    clear_dlc_default_human_spawn_group();
    clear_dlc_default_vehicle_spawn_group();
    char lower[MAX_SPAWN_GROUP_NAME];
    for (int i = 0; i < Dlc_human_spawn_group_list.size(); i++)
    {
        const human_spawn_group_info* group = Dlc_human_spawn_group_list[i];
        if (copy_to_lower(group->group_name, lower) && strstr(lower, "default") != nullptr && group->homogeneous_team && group->h_info_list.size() > 0)
        {
            const human_teams team = group->h_info_list[0]->default_team;
            if (team < NUM_HUMAN_TEAMS)
            {
                DLC_default_human_spawn_groups[team] = group;
            }
        }
    }
    for (int i = 0; i < Dlc_vehicle_spawn_group_list.size(); i++)
    {
        const vehicle_spawn_group_info* group = Dlc_vehicle_spawn_group_list[i];
        if (copy_to_lower(group->group_name, lower) && strstr(lower, "default") != nullptr && group->v_info_list.size() > 0)
        {
            const human_teams team = group->v_info_list[0]->default_team;
            if (team < NUM_HUMAN_TEAMS)
            {
                DLC_default_vehicle_spawn_groups[team] = group;
            }
        }
    }
}

// 0x00B42E60 (:791)
void sog_clear_dlc_default_spawn_group()
{
    clear_dlc_default_human_spawn_group();
    clear_dlc_default_vehicle_spawn_group();
}

// 0x00B42E90 (:800)
const human_spawn_group_info* sog_default_human_spawn_group(human_teams team)
{
    if ((unsigned int)team < NUM_HUMAN_TEAMS)
    {
        return DLC_default_human_spawn_groups[team] != nullptr ? DLC_default_human_spawn_groups[team] : Default_human_spawn_groups[team];
    }
    return nullptr;
}

// 0x00B42EC0 (:813)
const vehicle_spawn_group_info* sog_default_vehicle_spawn_group(human_teams team)
{
    if ((unsigned int)team < NUM_HUMAN_TEAMS)
    {
        return DLC_default_vehicle_spawn_groups[team] != nullptr ? DLC_default_vehicle_spawn_groups[team] : Default_vehicle_spawn_groups[team];
    }
    return nullptr;
}
