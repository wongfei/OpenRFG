// Original: volition/rfg/code/spawn/ambient_spawn_info.cpp
//
// The sets of ambient_spawn_info.xtbl (stub_impl.md S9): for a zone, the pedestrian and vehicle densities and which spawn
// groups ambient spawning draws from. A DLC's table replaces the main one when it has any.

#include "volition/rfg/code/spawn/ambient_spawn_info.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/ambient_spawn_behavior.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"

#include <stdio.h>
#include <string.h>

// ---- file-static globals
static const char* const SPAWN_DENSITY_LEVEL_NAMES[NUM_AMB_SPAWN_DENSITY] = {"None", "Low", "Medium", "High"}; // 0x016B6970
static char* Ambient_spawn_string_pool_data; // 0x03742458
static farray<ambient_spawn_info, 8> DLC_ambient_spawn_info_pool; // 0x037448B8
static string_pool Ambient_spawn_string_pool("Ambient_spawn_string_pool"); // 0x03748330
static farray<ambient_spawn_info, 64> Ambient_spawn_info_pool; // 0x0374B330

// 0x00B46FB0 (:94)
static base_array<ambient_spawn_info>& get_ambient_spawn_info_pool()
{
    if (World.dlc_bundle_id != -1 && DLC_ambient_spawn_info_pool.size() > 0)
    {
        return DLC_ambient_spawn_info_pool;
    }
    return Ambient_spawn_info_pool;
}

// 0x00B46F20 (.h:52)
ambient_spawn_info::ambient_spawn_info()
{
    reset();
}

// 0x00B40DC0 (.h:83)
bool ambient_spawn_info::ped_spawn_enabled() const
{
    return ambient_flags.ped_spawn_enabled;
}

// 0x00B40DD0 (.h:84)
bool ambient_spawn_info::veh_spawn_enabled() const
{
    return ambient_flags.veh_spawn_enabled;
}

// 0x00B40DE0 (.h:91)
void ambient_spawn_info::reset()
{
    group_name = nullptr;
    h_spawn_group = nullptr;
    v_spawn_group = nullptr;
    ped_density = AMB_SPAWN_DENSITY_NONE;
    night_ped_density = AMB_SPAWN_DENSITY_NONE;
    veh_density = AMB_SPAWN_DENSITY_NONE;
    night_veh_density = AMB_SPAWN_DENSITY_NONE;
    ambient_behavior_id = -1;
    ambient_flags.ped_spawn_enabled = false;
    ambient_flags.veh_spawn_enabled = false;
}

// 0x00B54AA0 (:110)
bool ambient_spawn_info::load(xml_element* xml_root)
{
    reset();
    xml_element* name = xml_find(xml_root, "Name");
    if (name == nullptr)
    {
        return false;
    }
    group_name = Ambient_spawn_string_pool.add_unique(name->text);
    if (group_name == nullptr)
    {
        return false;
    }
    xml_element* ped = xml_find(xml_root, "ped_spawn_density");
    xml_element* night_ped = xml_find(xml_root, "night_ped_spawn_density");
    xml_element* veh = xml_find(xml_root, "veh_spawn_density");
    xml_element* night_veh = xml_find(xml_root, "night_veh_spawn_density");
    if (ped != nullptr)
    {
        ped_density = amb_spawn_density_from_name(ped->text);
    }
    if (night_ped != nullptr)
    {
        night_ped_density = amb_spawn_density_from_name(night_ped->text);
    }
    if (veh != nullptr)
    {
        veh_density = amb_spawn_density_from_name(veh->text);
    }
    if (night_veh != nullptr)
    {
        night_veh_density = amb_spawn_density_from_name(night_veh->text);
    }
    xml_element* npc_group = xml_find(xml_root, "npc_spawn_group");
    xml_element* vehicle_group = xml_find(xml_root, "vehicle_spawn_group");
    if (npc_group != nullptr)
    {
        h_spawn_group = sog_get_human_group(npc_group->text);
    }
    if (vehicle_group != nullptr)
    {
        v_spawn_group = sog_get_vehicle_group(vehicle_group->text);
    }
    ambient_flags.ped_spawn_enabled = h_spawn_group != nullptr && (int)ped_density >= AMB_SPAWN_DENSITY_LOW && (int)ped_density <= AMB_SPAWN_DENSITY_HIGH;
    ambient_flags.veh_spawn_enabled = h_spawn_group != nullptr && v_spawn_group != nullptr && (int)veh_density >= AMB_SPAWN_DENSITY_LOW && (int)veh_density <= AMB_SPAWN_DENSITY_HIGH;
    char* behavior = xml_table_get_opt_string_ref(xml_root, "default_ambient_behavior");
    if (behavior != nullptr)
    {
        ambient_behavior_id = ambient_behavior_get_id(behavior);
    }
    return true;
}

// 0x00B54C50 (:193)
void amb_spawn_info_system_init(int dlc_id)
{
    const bool dlc = dlc_id != -1;
    if (Ambient_spawn_string_pool_data == nullptr)
    {
        // Magic number: 1 KB of names.
        Ambient_spawn_string_pool_data = (char*)Permanent_mempool.alloc(0x400, 0);
        if (Ambient_spawn_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the ambient spawn info string table");
        }
        Ambient_spawn_string_pool.init(Ambient_spawn_string_pool_data, 0x400, "Ambient Spawn Info Pool", 0, 0);
        Ambient_spawn_string_pool.clear();
    }
    Ambient_spawn_string_pool.set_thread_ownership();
    Ambient_spawn_string_pool.mark();
    char file_name[64];
    if (dlc)
    {
        DLC_ambient_spawn_info_pool.clear();
        sprintf(file_name, "dlc%02d_%s", dlc_id, "ambient_spawn_info.xtbl");
        if (!cf_exist(file_name))
        {
            return;
        }
    }
    else
    {
        Ambient_spawn_info_pool.clear();
        strcpy(file_name, "ambient_spawn_info.xtbl");
    }
    xml_element* root = xml_table_parse(file_name);
    if (root == nullptr)
    {
        return;
    }
    ambient_spawn_info info;
    for (xml_element* element = xml_find(root, "ambient_spawn_info"); element != nullptr; element = xml_find_next(root, element, "ambient_spawn_info"))
    {
        if (info.load(element))
        {
            if (dlc)
            {
                DLC_ambient_spawn_info_pool.add(info);
            }
            else
            {
                Ambient_spawn_info_pool.add(info);
            }
        }
    }
    xml_table_close(root);
}

// 0x00B46FD0 (:252)
void amb_spawn_info_dlc_shutdown()
{
    Ambient_spawn_string_pool.set_thread_ownership();
    Ambient_spawn_string_pool.restore_to_mark(0);
    DLC_ambient_spawn_info_pool.clear();
}

// 0x00B40F20 (:261)
const ambient_spawn_info* amb_spawn_info_pos(const vector& pos)
{
    obj_zone* zone = World.get_zone_by_pos(pos);
    return zone != nullptr ? zone->amb_spawn_info : nullptr;
}

// 0x00B40F40 (:276)
const ambient_spawn_info* amb_spawn_info_current()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return nullptr;
    }
    return amb_spawn_info_pos(pp->pos);
}

// 0x00B47000 (:291)
const ambient_spawn_info* amb_spawn_info_from_name(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    base_array<ambient_spawn_info>& pool = get_ambient_spawn_info_pool();
    for (int i = 0; i < pool.size(); i++)
    {
        if (v_stricmp(name, pool[i].group_name) == 0)
        {
            return &pool[i];
        }
    }
    return nullptr;
}

// 0x00B40F70 (:313)
ambient_spawn_density amb_spawn_density_from_name(const char* density_name)
{
    for (int i = 0; i < NUM_AMB_SPAWN_DENSITY; i++)
    {
        if (v_stricmp(density_name, SPAWN_DENSITY_LEVEL_NAMES[i]) == 0)
        {
            return (ambient_spawn_density)i;
        }
    }
    return AMB_SPAWN_DENSITY_NONE;
}

// 0x00B4C440 (:328)
// LTCG: h_group_list in EAX.
static bool human_in_all_spawn_groups(const human_info* h_info, const farray<human_spawn_group_info const*, 64>& h_group_list)
{
    for (int i = 0; i < h_group_list.size(); i++)
    {
        if (h_group_list[i]->h_info_list.find(h_info, false) < 0)
        {
            return false;
        }
    }
    return true;
}

// 0x00B40FB0 (:344)
static bool human_has_common_flag(const human_info* h_info)
{
    return h_info->home_district == 0;
}

// 0x00B54DF0 (:351)
// The humans of no home district that every ambient set's group has.
void amb_spawn_info_build_common_npc_list(farray<human_info const*, 8>& returned_npc_list)
{
    returned_npc_list.clear();
    farray<human_info const*, 128> candidates;
    farray<human_spawn_group_info const*, 64> groups;
    base_array<ambient_spawn_info>& pool = get_ambient_spawn_info_pool();
    for (int i = 0; i < pool.size(); i++)
    {
        if (pool[i].h_spawn_group != nullptr)
        {
            groups.add_unique(pool[i].h_spawn_group);
        }
    }
    if (groups.size() < 1)
    {
        return;
    }
    for (int i = 0; i < groups[0]->h_info_list.size(); i++)
    {
        if (groups[0]->h_info_list[i] != nullptr)
        {
            candidates.add_unique(groups[0]->h_info_list[i]);
        }
    }
    for (int i = 0; i < candidates.size();)
    {
        if (human_in_all_spawn_groups(candidates[i], groups) && human_has_common_flag(candidates[i]))
        {
            i++;
        }
        else
        {
            candidates.remove_unsorted(i);
        }
    }
    for (int i = 0; i < candidates.size(); i++)
    {
        returned_npc_list.add(candidates[i]);
    }
}
