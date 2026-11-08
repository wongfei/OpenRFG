// Original: volition/rfg/code/scripting/action_script/global_event_manager.cpp
//
// The global event manager: callbacks registered per event type, called by gem_post_event.

#include "volition/rfg/code/scripting/action_script/global_event_manager.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"

// One category per event type.
static object_pool<notify_info,4096,62> Gem_notifications(&notify_info::init); // 0x036BF350
static cs_wrapper Gem_cs(0); // 0x036D0BB0

// 0x00B1FA90 (:31)
void gem_system_init()
{
}

// 0x00B1FAA0 (:37)
void gem_level_init_pre()
{
}

// 0x00B3CC60 (:43)
void gem_level_shutdown()
{
    Gem_cs.enter();
    Gem_notifications.clear();
    Gem_cs.leave();
}

// 0x00B31480 (:53)
notify_info* gem_register_for_event(const notify_info& info)
{
    Gem_cs.enter();
    if (info.callback != nullptr && info.type != EVENT_UNDEFINED && Gem_notifications.get_free() > 0)
    {
        notify_info* notify = Gem_notifications.alloc(info.type);
        if (notify != nullptr)
        {
            notify->type = info.type;
            notify->callback = info.callback;
            notify->data = info.data;
        }
        Gem_cs.leave();
        return notify;
    }
    Gem_cs.leave();
    return nullptr;
}

// 0x00B37B60 (:70)
void gem_unregister_for_event_internal(notify_info* handle)
{
    if (handle != nullptr && Gem_notifications.contains(handle))
    {
        handle->type = EVENT_UNDEFINED;
        handle->callback = nullptr;
        handle->data = nullptr;
        Gem_notifications.free(handle);
    }
}

// 0x00B37BB0 (:81)
void gem_unregister_for_event(notify_info* handle)
{
    Gem_cs.enter();
    gem_unregister_for_event_internal(handle);
    Gem_cs.leave();
}

// 0x00B1FAB0 (:90)
void gem_do_frame()
{
}

// The game trace of an event (game_trace.h; not in rfg.exe). The frequent ones during play or loading are left out.
static void gem_trace_event(const event_data& data, int listeners)
{
    static const char* const names[NUM_EVENT_TYPES] = {
        "BUILDING_DAMAGED",
        "BUILDING_DAMAGED_BY_EXPLOSION",
        "BUILDING_DESTROYED",
        "SALVAGE_CREATED",
        "WEAPON_ACQUIRED",
        "AMMO_ACQUIRED",
        "ITEM_ACQUIRED",
        "RAID_COMPLETE",
        "ACTIVITY_STATE_CHANGE",
        "RAID_STATE_CHANGE",
        "HOUSE_ARREST_STATUS",
        "TURRET_DESTROYED",
        "VEHICLE_DESTROYED",
        "HUMAN_DEAD",
        "DIALOGUE_FINISHED",
        "EDF_CONTROL_LOWERED",
        "CIVILIAN_MORALE_LOWERED",
        "CIVILIAN_MORALE_RAISED",
        "PLAYER_ENTER_TRIGGER_REGION",
        "PLAYER_EXIT_TRIGGER_REGION",
        "OBJECT_ENTER_TRIGGER_REGION",
        "OBJECT_EXIT_TRIGGER_REGION",
        "PLAYER_FAIL_TRIGGER_REGION_RESTRICTION",
        "PLAYER_ENTER_RESTRICTED_AREA",
        "PLAYER_EXIT_RESTRICTED_AREA",
        "MISSION_SUCCESS",
        "MISSION_FAILURE",
        "TIMER_ELAPSED",
        "OBJECTIVE_COMPLETE",
        "MISSION_OBJECTIVE_COMPLETE",
        "MOVER_STREAM_IN",
        "RECRUITMENT",
        "ALERT_LEVEL_CHANGED",
        "SQUAD_KILLED",
        "SQUAD_MEMBER_KILLED",
        "PLAYER_KILLS_VEHICLE",
        "PLAYER_KILLS_HUMAN",
        "ROAD_BECOMES_BLOCKED",
        "BRIDGE_BECOMES_UNUSABLE",
        "PLAYER_KILLS_VEHICLE_FROM_VEHICLE",
        "VEHICLE_DAMAGE",
        "PLAYER_ENTERS_VEHICLE",
        "PLAYER_EXITS_VEHICLE",
        "NPC_ENTERS_VEHICLE",
        "MISSION_VRAIL_COMPLETE",
        "PLAYER_UNDER_ATTACK",
        "VEHICLE_UNDER_ATTACK",
        "PLAYER_DAMAGED_BY_EXPLOSION",
        "CONT_ATTACK_SQUAD_REACHED_END",
        "PATROL_SQUAD_REACHED_NODE",
        "WALKER_MOUNT_VEHICLE",
        "HUMAN_USING_ACTION_NODE",
        "SQUAD_MEMBERS_SPAWNED",
        "PLAYER_GETS_SALVAGE",
        "SATELLITE_REACHES_TARGET",
        "MOVER_HUMAN_SPECIAL_COLLISION",
        "PLAYER_ENTERS_CRITICAL_AREA",
        "SQUAD_MEMBERS_DESPAWNED",
        "NPC_DESPAWNED",
        "PLAYER_FIRES_WEAPON",
        "HUMAN_STOPPED_BEING_RENDERED",
        "USER_DEFINED",
    };
    const event_type type = data.get_type();
    char text[160] = "";
    switch (type)
    {
    case EVENT_BUILDING_DAMAGED:
    case EVENT_BUILDING_DAMAGED_BY_EXPLOSION:
    case EVENT_SALVAGE_CREATED:
    case EVENT_MOVER_STREAM_IN:
    case EVENT_VEHICLE_DAMAGE:
    case EVENT_PLAYER_UNDER_ATTACK:
    case EVENT_VEHICLE_UNDER_ATTACK:
    case EVENT_PLAYER_GETS_SALVAGE:
    case EVENT_PLAYER_FIRES_WEAPON:
    case EVENT_HUMAN_STOPPED_BEING_RENDERED:
    case EVENT_DIALOGUE_FINISHED: // the "dialogue" lines (posted at each line's start and end)
        return;
    case EVENT_OBJECT_ENTER_TRIGGER_REGION:
    case EVENT_OBJECT_EXIT_TRIGGER_REGION:
    {
        const object_enter_trigger_region_event& region = (const object_enter_trigger_region_event&)data;
        snprintf(text, sizeof(text), "object=0x%X(%s) trigger=0x%X(%s)", region.object_handle, game_trace_object_name(region.object_handle), region.trigger_handle,
                 game_trace_object_name(region.trigger_handle));
        break;
    }
    case EVENT_BUILDING_DESTROYED:
    {
        const unsigned int building = ((const building_destroyed_event&)data).building;
        snprintf(text, sizeof(text), "building=0x%X(%s)", building, game_trace_object_name(building));
        break;
    }
    case EVENT_NPC_ENTERS_VEHICLE:
    {
        const npc_enters_vehicle_event& enters = (const npc_enters_vehicle_event&)data;
        snprintf(text, sizeof(text), "npc=0x%X(%s) vehicle=0x%X(%s)", enters.npc_handle, game_trace_object_name(enters.npc_handle), enters.vehicle_handle, game_trace_object_name(enters.vehicle_handle));
        break;
    }
    case EVENT_HUMAN_USING_ACTION_NODE:
    {
        const human_using_action_node_event& using_node = (const human_using_action_node_event&)data;
        snprintf(text, sizeof(text), "human=0x%X(%s) node=0x%X start=%d", using_node.human_handle, game_trace_object_name(using_node.human_handle), using_node.action_node_handle, using_node.start ? 1 : 0);
        break;
    }
    case EVENT_PATROL_SQUAD_REACHED_NODE:
    {
        const unsigned int squad = ((const patrol_squad_reached_node_event&)data).squad_handle;
        snprintf(text, sizeof(text), "squad=0x%X(%s)", squad, game_trace_object_name(squad));
        break;
    }
    case EVENT_SQUAD_KILLED:
    case EVENT_SQUAD_MEMBER_KILLED:
    case EVENT_SQUAD_MEMBERS_DESPANED:
    {
        const unsigned int squad = ((const squad_killed_event&)data).squad_handle;
        snprintf(text, sizeof(text), "squad=0x%X(%s)", squad, game_trace_object_name(squad));
        break;
    }
    case EVENT_SQUAD_MEMBERS_SPAWNED:
    {
        const unsigned int node = ((const squad_members_spawned_event&)data).spawn_node_handle;
        snprintf(text, sizeof(text), "node=0x%X(%s)", node, game_trace_object_name(node));
        break;
    }
    case EVENT_HUMAN_DEAD:
    {
        const unsigned int human = ((const human_dead_event&)data).human_handle;
        snprintf(text, sizeof(text), "human=0x%X(%s)", human, game_trace_object_name(human));
        break;
    }
    case EVENT_NPC_DESPANED:
    {
        const unsigned int human = ((const npc_despawned&)data).npc_handle;
        snprintf(text, sizeof(text), "npc=0x%X(%s)", human, game_trace_object_name(human));
        break;
    }
    case EVENT_USER_DEFINED:
        snprintf(text, sizeof(text), "%s", ((const user_defined_event&)data).desc);
        break;
    case EVENT_MISSION_FAILURE:
    {
        const char* desc = ((const mission_failure_event&)data).desc;
        snprintf(text, sizeof(text), "%s", desc != nullptr ? desc : "");
        break;
    }
    case EVENT_TIMER_ELAPSED:
        snprintf(text, sizeof(text), "timer=0x%X", ((const timer_elapsed_event&)data).handle);
        break;
    case EVENT_OBJECTIVE_COMPLETE:
    case EVENT_MISSION_OBJECTIVE_COMPLETE:
        snprintf(text, sizeof(text), "handle=%d", ((const objective_complete_event&)data).handle);
        break;
    case EVENT_ALERT_LEVEL_CHANGED:
        snprintf(text, sizeof(text), "%d to %d", (int)((const alert_level_change_event&)data).old_level, (int)((const alert_level_change_event&)data).alert_level);
        break;
    case EVENT_PLAYER_ENTERS_VEHICLE:
    case EVENT_PLAYER_EXITS_VEHICLE:
    {
        const unsigned int vehicle = ((const player_enters_vehicle_event&)data).handle;
        snprintf(text, sizeof(text), "vehicle=0x%X(%s)", vehicle, game_trace_object_name(vehicle));
        break;
    }
    default:
        break;
    }
    GAME_TRACE("event", "%s listeners=%d %s", names[type], listeners, text);
}

// 0x00B37C10 (:97)
void gem_post_event(event_data& data)
{
    Gem_cs.enter();
    const event_type type = data.get_type();
    if (type == EVENT_UNDEFINED)
    {
        Gem_cs.leave();
        return;
    }
    const int count = Gem_notifications.get_count(type);
    if (Game_trace_file != nullptr)
    {
        gem_trace_event(data, count);
    }
    // A copy of the list: a callback may register or unregister.
    const unsigned int mark = Common_mempool.mark();
    notify_info** list = (notify_info**)Common_mempool.alloc(count * sizeof(notify_info*), 0);
    int filled = 0;
    for (notify_info* notify = Gem_notifications.get_first(type); notify != nullptr; notify = Gem_notifications.get_next(notify))
    {
        list[filled++] = notify;
    }
    for (int i = 0; i < count; i++)
    {
        notify_info* notify = list[i];
        if (Gem_notifications.get_category(notify) == type)
        {
            // Original bug kept: the early exit on a freed-and-reused entry leaves without restoring the mark.
            if (notify->type == EVENT_UNDEFINED || notify->callback == nullptr)
            {
                Gem_cs.leave();
                return;
            }
            if (notify->callback(data, notify) == EVENT_HANDLED)
            {
                gem_unregister_for_event_internal(notify);
            }
        }
    }
    Common_mempool.restore_to_mark(mark);
    Gem_cs.leave();
}

// The events the script layer posts (their other uses are inlined).
object_enter_trigger_region_event::object_enter_trigger_region_event()
    : event_data(EVENT_OBJECT_ENTER_TRIGGER_REGION)
{
}

object_exit_trigger_region_event::object_exit_trigger_region_event()
    : event_data(EVENT_OBJECT_EXIT_TRIGGER_REGION)
{
}

// 0x00A058C0 (global_event_data.h:133)
player_enter_restricted_area_event::player_enter_restricted_area_event()
    : event_data(EVENT_PLAYER_ENTER_RESTRICTED_AREA)
{
}

// 0x00A058F0 (global_event_data.h:145)
player_exit_restricted_area_event::player_exit_restricted_area_event()
    : event_data(EVENT_PLAYER_EXIT_RESTRICTED_AREA)
{
}

// 0x00A05880 (global_event_data.h:91)
turret_destroyed_event::turret_destroyed_event()
    : event_data(EVENT_TURRET_DESTROYED)
{
}

squad_killed_event::squad_killed_event()
    : event_data(EVENT_SQUAD_KILLED)
{
}

squad_member_killed_event::squad_member_killed_event()
    : event_data(EVENT_SQUAD_MEMBER_KILLED)
{
}

squad_members_despawned::squad_members_despawned()
    : event_data(EVENT_SQUAD_MEMBERS_DESPANED)
{
}

squad_members_spawned_event::squad_members_spawned_event()
    : event_data(EVENT_SQUAD_MEMBERS_SPAWNED)
{
}

mission_failure_event::mission_failure_event()
    : event_data(EVENT_MISSION_FAILURE)
{
}

mission_success_event::mission_success_event()
    : event_data(EVENT_MISSION_SUCCESS)
{
}

player_enters_critical_area_event::player_enters_critical_area_event()
    : event_data(EVENT_PLAYER_ENTERS_CRITICAL_AREA)
{
}

timer_elapsed_event::timer_elapsed_event()
    : event_data(EVENT_TIMER_ELAPSED)
{
}

user_defined_event::user_defined_event()
    : event_data(EVENT_USER_DEFINED)
{
}

mission_objective_complete_event::mission_objective_complete_event()
    : event_data(EVENT_MISSION_OBJECTIVE_COMPLETE)
{
}

objective_complete_event::objective_complete_event()
    : event_data(EVENT_OBJECTIVE_COMPLETE)
{
}
