// Original: volition/rfg/code/spawn/spawn_request.cpp
//
// The parameters of an NPC or vehicle spawn (stub_impl.md S9), with what each stage of the spawn needs before it may run
// (the resource manager's check, then placement).

#include "volition/rfg/code/spawn/spawn_request.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/spawn/spawn_resource_manager_internal.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"

#include <string.h>

// 0x00B48B40 (:14)
// The first ancestor that is a building (type 2, subtype 1: an rfg_mover).
const rfg_mover* find_spawn_building_handle(const object* objp)
{
    while (objp != nullptr)
    {
        if (objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            return (const rfg_mover*)objp;
        }
        objp = World.object_from_handle(objp->get_parent());
    }
    return nullptr;
}

// 0x007053C0 (.h:35)
npc_spawn_params::npc_spawn_params()
{
    reset();
}

// 0x007053D0 (.h:36)
npc_spawn_params::npc_spawn_params(const human_info* new_hum_info)
{
    reset();
    hum_info = new_hum_info;
}

// 0x00B40FC0 (.h:80)
bool npc_spawn_params::spawn_failed() const
{
    return spawn_flags.attempted_spawned && np == nullptr;
}

// 0x00B40FE0 (.h:83)
bool npc_spawn_params::spawn_attempted() const
{
    return spawn_flags.attempted_spawned;
}

// 0x00B40FF0 (.h:85)
void npc_spawn_params::set_nav_cell_handle(uint64_t _handle)
{
    spawn_flags.nav_cell_set = true;
    nav_cell_handle = _handle;
}

// 0x00B41010 (.h:87)
bool npc_spawn_params::is_nav_cell_handle_set()
{
    return spawn_flags.nav_cell_set;
}

uint64_t npc_spawn_params::get_nav_cell_handle()
{
    return nav_cell_handle;
}

// 0x00B43BD0 (:45)
void npc_spawn_params::reset()
{
    np = nullptr;
    spawn_pos.zero();
    spawn_orient.make_identity();
    hum_info = nullptr;
    member_load = -1;
    memset(&spawn_flags, 0, sizeof(spawn_flags));
}

// 0x00B43C00 (:57)
bool npc_spawn_params::rm_check_ready() const
{
    return np == nullptr && hum_info != nullptr;
}

// 0x00B43C20 (:65)
bool npc_spawn_params::placement_ready() const
{
    return np == nullptr && hum_info != nullptr && spawn_flags.rm_checked;
}

// 0x00705400 (.h:208)
vehicle_spawn_params::vehicle_spawn_params()
{
    reset();
}

// 0x00B43C50 (:87)
void vehicle_spawn_params::reset()
{
    spawn_pos.zero();
    osrp = nullptr;
    spawn_orient = IDENTITY_MATRIX;
    forward_speed = 0.0f;
    parent_handle = 0xFFFFFFFF;
    spawn_priority = SPAWN_PRIORITY_LOW;
    vp = nullptr;
    veh_info = nullptr;
    memset(&vflags, 0, sizeof(vflags));
    zone_id_xy = 0;
    memset(&veh_spawn_flags, 0, sizeof(veh_spawn_flags));
}

// 0x00B5CB60 (:104)
bool vehicle_spawn_params::pick_vehicle_info(const vehicle_spawn_group_info* v_spawn_group)
{
    veh_info = srm_pick_loaded_vehicle_info(v_spawn_group->v_info_list);
    return veh_info != nullptr;
}

// 0x00B5CB80 (:111)
// In a mission any loaded vehicle of the group; else the first one the special slot holds (reservation flag 15).
bool vehicle_spawn_params::pick_special_vehicle_info(const vehicle_spawn_group_info* v_spawn_group)
{
    if (mission_is_active())
    {
        veh_info = srm_pick_loaded_vehicle_info(v_spawn_group->v_info_list);
        return veh_info != nullptr;
    }
    for (int i = 0; i < v_spawn_group->v_info_list.size(); i++)
    {
        const vehicle_info* info = v_spawn_group->v_info_list[i];
        if (info == nullptr)
        {
            continue;
        }
        resource_slot_record* record = Vehicle_records_list.get_record(info->slot_id);
        if (record != nullptr && record->is_reserved_by(SPAWN_SLOT_VEHICLE_SPECIAL))
        {
            veh_info = info;
            return true;
        }
    }
    return false;
}

// 0x00B43CD0 (:141)
bool vehicle_spawn_params::rm_check_ready() const
{
    return vp == nullptr && veh_info != nullptr && (!spawn_pos.is_null() || osrp != nullptr);
}

// 0x00B43D00 (:157)
bool vehicle_spawn_params::placement_ready() const
{
    return vp == nullptr && veh_info != nullptr && (!spawn_pos.is_null() || osrp != nullptr);
}

// 0x00708680 (.h:105)
npc_spawn_param_flags::npc_spawn_param_flags()
{
    reset();
}

// 0x007053F0 (.h:121)
void npc_spawn_param_flags::reset()
{
    memset(this, 0, sizeof(*this));
}

// 0x0070BE40 (.h:130)
npc_spawn_param_list::npc_spawn_param_list(npc_manager_systems creation_system)
    : management_system(creation_system)
{
    reset();
}

// 0x0070CFC0
npc_spawn_param_list::~npc_spawn_param_list()
{
}

// 0x00B48B70 (:172)
void npc_spawn_param_list::reset()
{
    spawn_pos.zero();
    osrp = nullptr;
    npc_spawn_info.clear();
    memset(&npc_flags, 0, sizeof(npc_flags));
    memset(&hflags, 0, sizeof(hflags));
    vp = nullptr;
    first_seat = VSI_DRIVER;
    navp = nullptr;
    spawn_parent_obj = nullptr;
    zone_id_xy = 0;
    spawn_building_handle = 0xFFFFFFFF;
    node_type = nullptr;
    ambient_behavior_id = -1;
    spawn_flags.reset();
    spawn_orient = IDENTITY_MATRIX;
}

// 0x00B43D30 (:196)
void npc_spawn_param_list::assign_spawn_parent(const object* new_spawn_parent_obj)
{
    spawn_parent_obj = new_spawn_parent_obj;
    if (new_spawn_parent_obj != nullptr)
    {
        spawn_building_handle = new_spawn_parent_obj->get_handle();
    }
}

// 0x00B48C40 (:207)
npc_spawn_params* npc_spawn_param_list::find_next_npc_spawn()
{
    for (int i = 0; i < npc_spawn_info.size(); i++)
    {
        if (!npc_spawn_info[i].spawn_flags.attempted_spawned)
        {
            return &npc_spawn_info[i];
        }
    }
    return nullptr;
}

// 0x00B48C70 (:221)
bool npc_spawn_param_list::all_spawned() const
{
    for (int i = 0; i < npc_spawn_info.size(); i++)
    {
        if (!npc_spawn_info[i].spawn_flags.attempted_spawned && npc_spawn_info[i].np == nullptr)
        {
            return false;
        }
    }
    return true;
}

// 0x00B48CB0 (:238)
bool npc_spawn_param_list::rm_check_ready() const
{
    if (npc_spawn_info.size() < 1 || (spawn_pos.is_null() && osrp == nullptr) || management_system == NPC_MANAGER_INVALID)
    {
        return false;
    }
    for (int i = 0; i < npc_spawn_info.size(); i++)
    {
        if (npc_spawn_info[i].np != nullptr || npc_spawn_info[i].hum_info == nullptr)
        {
            return false;
        }
    }
    return true;
}

// 0x00B48D10 (:265)
bool npc_spawn_param_list::placement_ready() const
{
    if (!rm_check_ready() || !spawn_pos.is_valid() || !spawn_orient.is_valid())
    {
        return false;
    }
    for (int i = 0; i < npc_spawn_info.size(); i++)
    {
        if (!npc_spawn_info[i].placement_ready())
        {
            return false;
        }
    }
    return true;
}
