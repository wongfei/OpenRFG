// Original: volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp
//
// gameflow.md GF4: a mission's vehicle rail, started and cancelled by scripts. The patrol waits until the player is
// aboard, chase vehicles come while it drives, and EVENT_MISSION_VRAIL_COMPLETE fires when the player gets out at the
// drop point. It is a free activity, so it runs alongside the mission and does not end it.

#include "volition/rfg/code/gameplay/activities/activity_mission_vrail.h"
#include "volition/rfg/code/gameplay/activities/activity_mission_vrail_internal.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/vrail_data.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"

extern gfm_orders_flags ACTIVITY_ORDERS_FLAGS; // 0x02231B21 (gfm_smash_building_coordinator.cpp)

activity_mission_vrail_data Mission_vrail_activity_data; // 0x02270D40

// 0x00753B10 (global_event_data.h:230)
mission_vrail_complete_event::mission_vrail_complete_event()
    : event_data(EVENT_MISSION_VRAIL_COMPLETE)
{
}

// 0x007A94A0 (:50). rfg.exe: LTCG, squad in ESI.
static void release_squad_high_prio(object_squad* squad)
{
    const unsigned int orders = squad->get_orders_handle();
    if (orders != 0xFFFFFFFF)
    {
        Orders_manager.destroy_orders(orders); // rfg.exe inlines it
    }
    squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, false);
}

// 0x00759110 (:57). rfg.exe: LTCG, hflags in EAX, squad_flags in ECX; every use of it is inlined.
static void set_vrail_driver_flags(npc_flags_struct& npc_flags, human_flags& hflags, squad_flags_struct& squad_flags)
{
    hflags.invulnerable = 1;
    hflags.disallow_vehicle_exit = 1;
    npc_flags.activity_driver_vr = 1;
    squad_flags.disallow_reassignment = 1;
}

// 0x007A0260 (.h:15). Original: the fields not in the shared data stay unset until level_init calls reset.
activity_mission_vrail_data::activity_mission_vrail_data()
    : activity_base_free(ACTIVITY_MODE_MISSION_VRAIL)
{
}

// 0x0077E490
activity_mission_vrail_data::~activity_mission_vrail_data()
{
}

// 0x00759130 (:76)
void activity_mission_vrail_data::system_init()
{
}

// 0x00783580 (:82). Original: looks the squad up and draws nothing.
void activity_mission_vrail_data::render_icon_player() const
{
}

// 0x007835A0 (:103). Original: nothing sets Render_reinforcement_icon, so this draws nothing.
void activity_mission_vrail_data::render_icon_attackers() const
{
    if (!reinforcement_should_render_icon())
    {
        return;
    }
    for (int i = 0; i < spawn_data.chase_vehicle_array.capacity(); i++)
    {
        vector pos;
        if (spawn_data.chase_vehicle_array[i].get_squad_position(pos))
        {
            Cartographer.add_temporary_icon(pos, MAP_ELEMENT_STAR_RED_ICON, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
        }
    }
}

// 0x00783600 (:122). Original: draws neither the player nor the attackers.
void activity_mission_vrail_data::render_icons() const
{
}

// 0x007B2650 (:136)
void activity_mission_vrail_data::level_init()
{
    activity_base_free::level_init(); // rfg.exe inlines it
    reset();
}

// 0x007A1D60 (:144). Same answer as the base; rfg.exe also calls is_active and was_initialized and ignores them.
bool activity_mission_vrail_data::can_initialize() const
{
    return activity_base::can_initialize();
}

// 0x00759140 (:165)
bool activity_mission_vrail_data::on_alert(const npc* np) const
{
    return is_active();
}

// 0x007ADD60 (:176)
void activity_mission_vrail_data::reset()
{
    patrol_data.release_and_reset();
    spawn_data.release_and_reset();
    current_vrail_def = nullptr;
    vrail_patrol_handle = 0xFFFFFFFF;
    drop_off_pos.zero();
    squad_handle = 0xFFFFFFFF;
}

// 0x007ADDB0 (:190)
bool activity_mission_vrail_data::activity_start()
{
    if (current_vrail_def == nullptr || drop_off_pos.is_null() || !activity_base_free::activity_start())
    {
        return false;
    }
    object_patrol* patrol = vrail_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(vrail_patrol_handle) : nullptr;
    if (patrol == nullptr)
    {
        return false;
    }
    patrol->set_pause_flag(false);
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad == nullptr || !patrol_data.vrail_init(squad, drop_off_pos))
    {
        return false;
    }
    spawn_data.vrail_init(squad, current_vrail_def, patrol, current_vrail_def->chase_vehicle_count);
    vehicle_set_invulnerable(patrol_data.get_patrol_vehicle_handle(), true);
    vehicle_lock_position(patrol_data.get_patrol_vehicle_handle(), true);
    return true;
}

// 0x007ADEA0 (:234). Original: releases the shared data twice, the second time in the inlined reset.
void activity_mission_vrail_data::activity_end()
{
    activity_base_free::activity_end();
    const unsigned int vehicle_handle = patrol_data.get_patrol_vehicle_handle();
    vehicle_set_invulnerable(vehicle_handle, false);
    vehicle_lock_position(vehicle_handle, false);
    vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    if (vp != nullptr)
    {
        vp->m_flags.delete_mission_vehicle = 1; // bit 8 at 0x3F24 in the asm; the decompile names a wrong field
    }
    player_get_local_player()->hflags.disallow_vehicle_exit = 0;
    patrol_data.release_and_reset();
    spawn_data.release_and_reset();
    activity_reinstate_diversions(ADR_ACTIVITY);
    reset();
}

// 0x00798CA0 (:265). Original: active is already false when it asks, so activity_end never runs from here and a killed
// rail keeps current_vrail_def until the next level_init.
void activity_mission_vrail_data::activity_kill()
{
    activity_base_free::activity_kill();
    if (is_active())
    {
        activity_end();
    }
}

// 0x00759150 (:275)
void activity_mission_vrail_data::clear_squad_flags() const
{
}

// 0x007B8160 (:299)
void activity_mission_vrail_data::activity_do_frame_active()
{
    const vrail_patrol_state patrol_state = patrol_data.do_frame();
    if (patrol_state > VRAIL_PATROL_STATE_FAILED)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        activity_end();
        return;
    }
    spawn_data.do_frame();
    switch (patrol_state)
    {
    case VRAIL_PATROL_STATE_PATROL:
    {
        spawn_data.do_frame_spawn();
        player_get_local_player()->hflags.disallow_vehicle_exit = 1;
        const unsigned int vehicle_handle = patrol_data.get_patrol_vehicle_handle();
        const vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
        if (vp == nullptr || !vp->m_flags.m_mission_invulnerable)
        {
            vehicle_set_invulnerable(vehicle_handle, false);
        }
        vehicle_lock_position(vehicle_handle, false);
        activity_suspend_diversions(ADR_ACTIVITY);
        break;
    }
    case VRAIL_PATROL_STATE_GOING_TO_DROP_OFF:
    case VRAIL_PATROL_STATE_COMPLETE_WAIT:
        if (spawn_data.is_in_use())
        {
            spawn_data.release_and_reset();
        }
        break;
    case VRAIL_PATROL_STATE_COMPLETE:
    {
        mission_vrail_complete_event event;
        gem_post_event(event);
        activity_end();
        break;
    }
    default:
        break; // Original: a failed rail keeps running until a script cancels it
    }
}

// 0x007B26B0 (:358)
object_squad* activity_mission_vrail_data::spawn_squad(const object_patrol* patrol)
{
    if (patrol == nullptr || current_vrail_def == nullptr)
    {
        return nullptr;
    }
    squad_spawn_parameters params;
    params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
    params.orders.orders_type = GFM_ORDERS_TYPE_PATROL;
    params.orders.object_target = patrol->get_handle();
    params.orders.location_target = patrol->pos;
    params.squad.squad_pos = patrol->pos;
    params.squad.squad_orient = patrol->orient;
    params.squad.squad_def = current_vrail_def->driver_squad_def;
    params.squad.veh_spawn_group = current_vrail_def->player_veh_spawn_group;
    set_vrail_driver_flags(params.squad.npc_flags, params.squad.hflags, params.squad.squad_flags);
    params.squad.vflags.no_ragdoll = 1;
    return params.spawn_squad();
}

// 0x007B27B0 (:386)
void activity_mission_vrail_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    object_patrol* patrol = current_vrail_def != nullptr && vrail_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(vrail_patrol_handle) : nullptr;
    if (patrol == nullptr)
    {
        activity_kill();
        return;
    }
    patrol->set_pause_flag(true);
    object_squad* squad;
    if (squad_handle == 0xFFFFFFFF)
    {
        squad = spawn_squad(patrol);
        // Deviation: rfg.exe reads the handle through null when the spawn fails.
        if (squad != nullptr)
        {
            squad_handle = squad->get_handle();
        }
    }
    else
    {
        squad = World.object_from_handle<object_squad>(squad_handle);
    }
    if (squad == nullptr)
    {
        activity_kill();
        return;
    }
    if (!squad->simulated() && !activity_start())
    {
        activity_kill();
        release_squad_high_prio(squad);
    }
}

// 0x00759160 (:443)
vrail_patrol_state activity_mission_vrail_data::get_patrol_state() const
{
    return patrol_data.last_patrol_state;
}

// 0x00759170 (:449)
bool activity_mission_vrail_data::get_drop_point(vector& drop_pos) const
{
    if (current_vrail_def == nullptr)
    {
        return false;
    }
    drop_pos = drop_off_pos;
    return true;
}

// 0x007ADFE0 (:461). The squad and vehicle are the script's own, or both null to spawn them in activity_do_frame_ready.
bool activity_mission_vrail_data::init(const vehicle_rail_definition* vrail_def, const object_patrol* parent_patrol, const vector& drop_pos, object_squad* squad, vehicle* vp)
{
    if (vrail_def == nullptr || parent_patrol == nullptr || drop_pos.is_null() || current_vrail_def != nullptr)
    {
        return false;
    }
    if (squad == nullptr)
    {
        if (vp != nullptr)
        {
            return false;
        }
    }
    else if (squad->is_killed() || vp == nullptr || vp->is_killed())
    {
        return false;
    }
    if (!can_initialize() || !activity_initialize(nullptr, 0xFFFFFFFF))
    {
        return false;
    }
    current_vrail_def = vrail_def;
    vrail_patrol_handle = parent_patrol->get_handle();
    drop_off_pos = drop_pos;
    if (squad == nullptr)
    {
        return true;
    }

    squad_handle = squad->get_handle();
    squad->assign_vehicle(vp);
    squad_flags_struct new_squad_flags;
    set_vrail_driver_flags(squad->npc_flags, squad->hflags, new_squad_flags);
    squad->assign_new_squad_flags(new_squad_flags);
    vp->m_flags.disable_accelerator = 0;
    vp->m_flags.no_ragdoll = 1;
    const vehicle_turret_mount* mount = vehicle_get_turret_mount(vp, 0);
    turret* gun = mount != nullptr ? World.object_from_handle<turret>(mount->attached_turret) : nullptr;
    if (gun != nullptr)
    {
        gun->turret_flags.locked_turret = true;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = World.object_from_handle<npc>(squad->member_array[i]);
        if (np != nullptr)
        {
            set_vrail_driver_flags(np->npc_flags, np->hflags, new_squad_flags);
        }
    }
    gfm_orders_creation_params orders;
    orders.location_target = parent_patrol->pos;
    orders.object_target = parent_patrol->get_handle();
    orders.orders_type = GFM_ORDERS_TYPE_PATROL;
    orders.ofp = ACTIVITY_ORDERS_FLAGS;
    orders.team = HUMAN_TEAM_GUERILLA;
    const unsigned int new_orders = Orders_manager.create_new_orders(orders);
    if (new_orders == 0xFFFFFFFF)
    {
        return false;
    }
    squad->assign_base_orders(new_orders);
    return true;
}

// 0x00754270 (.h:60)
bool activity_mission_vrail_data::is_ready() const
{
    return current_vrail_def != nullptr;
}

// 0x00754280 (.h:63)
unsigned int activity_mission_vrail_data::get_patrol() const
{
    return vrail_patrol_handle;
}

// 0x00754290 (.h:66)
unsigned int activity_mission_vrail_data::get_patrol_squad() const
{
    return patrol_data.vrail_patrol_squad;
}

// 0x0077E430 (.h:67)
unsigned int activity_mission_vrail_data::get_patrol_vehicle() const
{
    return patrol_data.get_patrol_vehicle_handle();
}

// 0x0077E460 (.h:76)
void activity_mission_vrail_data::set_chase_vehicle_count(int new_count)
{
    spawn_data.set_chase_vehicle_count(new_count);
}

// 0x00798CD0 (:559)
int activity_mission_vrail_data::get_squads(base_array<object_squad*>& squad_list)
{
    return spawn_data.get_squads(squad_list);
}

// 0x007A0390 (.h:84)
void activity_mission_vrail_data::stop_squad_spawning()
{
    spawn_data.release_and_reset();
}

// 0x007AE1F0 (:566)
bool activity_mission_vrail_start(int index, const object_patrol* patrol, const vector& drop_off_pos, object_squad* squad, vehicle* vp)
{
    const vehicle_rail_definition* def = vrail_data_get_def(index);
    if (def == nullptr)
    {
        return false;
    }
    if (Mission_vrail_activity_data.init(def, patrol, drop_off_pos, squad, vp))
    {
        return true;
    }
    Mission_vrail_activity_data.activity_kill(); // rfg.exe inlines it
    return false;
}

// 0x007AE270 (:584)
bool activity_mission_vrail_start(int index, const object_patrol* patrol, const vector& drop_pos)
{
    return activity_mission_vrail_start(index, patrol, drop_pos, nullptr, nullptr); // rfg.exe inlines it
}

// 0x007AE2F0 (:601)
void activity_mission_vrail_cancel()
{
    Mission_vrail_activity_data.activity_end();
}
