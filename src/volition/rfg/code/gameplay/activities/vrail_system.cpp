// Original: volition/rfg/code/gameplay/activities/vrail_system.cpp
//
// gameflow.md GF4: queries over the two rails, the mission rail and riding shotgun. AI, vehicles and scripts ask these.
// rfg.exe reads the activity objects' fields here through inlined getters; this file calls those getters.

#include "volition/rfg/code/gameplay/activities/vrail_system.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_mission_vrail_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail_internal.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_squad.h"

// 0x0075BCE0 (:15)
bool vrail_is_driver(const npc* np)
{
    const unsigned int squad = np->get_ai()->get_squad_handle();
    if (squad == 0xFFFFFFFF)
    {
        return false;
    }
    if (Mission_vrail_activity_data.is_active())
    {
        return Mission_vrail_activity_data.get_patrol_squad() == squad;
    }
    if (!Vehicle_rail_activity_data.was_initialized_or_active())
    {
        return false;
    }
    return Vehicle_rail_activity_data.get_patrol_squad() == squad;
}

// 0x0075BD50 (:35)
bool vrail_is_riding_shotgun_driver(const npc* np)
{
    const unsigned int squad = np->get_ai()->get_squad_handle();
    if (squad == 0xFFFFFFFF || !Vehicle_rail_activity_data.was_initialized_or_active())
    {
        return false;
    }
    return Vehicle_rail_activity_data.get_patrol_squad() == squad;
}

// Inlined in vrail_in_wait_state.
static bool is_wait_state(vrail_patrol_state state)
{
    return state == VRAIL_PATROL_STATE_WAIT || state == VRAIL_PATROL_STATE_COMPLETE_WAIT || state == VRAIL_PATROL_STATE_COMPLETE;
}

// 0x0075BDA0 (:52)
bool vrail_in_wait_state()
{
    if (Mission_vrail_activity_data.is_active() && is_wait_state(Mission_vrail_activity_data.get_patrol_state()))
    {
        return true;
    }
    return Vehicle_rail_activity_data.was_initialized_or_active() && is_wait_state(Vehicle_rail_activity_data.get_patrol_state());
}

// 0x0075BE00 (:84)
vrail_patrol_state vrail_get_patrol_state()
{
    if (Mission_vrail_activity_data.is_active())
    {
        return Mission_vrail_activity_data.get_patrol_state();
    }
    return Vehicle_rail_activity_data.was_initialized_or_active() ? Vehicle_rail_activity_data.get_patrol_state() : INVALID_VRAIL_PATROL_STATE;
}

// 0x0075BE40 (:100)
bool vrail_is_patrol(const object_patrol* patrol)
{
    if (patrol == nullptr)
    {
        return false;
    }
    if (Mission_vrail_activity_data.is_ready())
    {
        return Mission_vrail_activity_data.get_patrol() == patrol->get_handle();
    }
    return Vehicle_rail_activity_data.was_initialized_or_active() && Vehicle_rail_activity_data.get_patrol() == patrol->get_handle();
}

// 0x0078ADB0 (:118). Original: not an else, so both rails change when both are up.
void vrail_change_chase_count(int new_count)
{
    if (Mission_vrail_activity_data.is_active())
    {
        Mission_vrail_activity_data.set_chase_vehicle_count(new_count);
    }
    if (Vehicle_rail_activity_data.was_initialized_or_active())
    {
        Vehicle_rail_activity_data.set_chase_vehicle_count(new_count);
    }
}

// 0x0075BEA0 (:133)
bool vrail_drop_point(vector& drop_point)
{
    if (Mission_vrail_activity_data.is_ready())
    {
        return Mission_vrail_activity_data.get_drop_point(drop_point);
    }
    return Vehicle_rail_activity_data.was_initialized_or_active() && Vehicle_rail_activity_data.get_drop_point(drop_point);
}

// 0x0078AE30 (:147). Original: only asks riding shotgun, never the mission rail.
bool vrail_is_vehicle(const unsigned int vehicle_handle)
{
    return Vehicle_rail_activity_data.was_initialized_or_active() && vehicle_handle == Vehicle_rail_activity_data.get_patrol_vehicle();
}

// 0x0078AEA0 (:159). Original: answers true with an invalid handle when the rail has no squad yet.
bool vrail_get_patrol_vehicle(unsigned int& vehicle_handle)
{
    if (Mission_vrail_activity_data.is_active())
    {
        vehicle_handle = Mission_vrail_activity_data.get_patrol_vehicle();
        return true;
    }
    if (!Vehicle_rail_activity_data.was_initialized_or_active())
    {
        return false;
    }
    vehicle_handle = Vehicle_rail_activity_data.get_patrol_vehicle();
    return true;
}

// 0x0075BF20 (:176)
bool vrail_get_patrol_squad(unsigned int& squad_handle)
{
    if (Mission_vrail_activity_data.is_active())
    {
        squad_handle = Mission_vrail_activity_data.get_patrol_squad();
        return true;
    }
    if (!Vehicle_rail_activity_data.was_initialized_or_active())
    {
        return false;
    }
    squad_handle = Vehicle_rail_activity_data.get_patrol_squad();
    return true;
}

// 0x0075BF80 (:192)
bool vrail_any_ready()
{
    return Vehicle_rail_activity_data.is_ready() || Mission_vrail_activity_data.was_initialized_or_active();
}

// 0x0079BAB0 (:197). rfg.exe reads activity.cpp's Current_activity_mode here and below.
int vrail_get_squads(base_array<object_squad*>& squad_list)
{
    if (!vrail_any_ready())
    {
        return 0;
    }
    if (Mission_vrail_activity_data.is_active())
    {
        return Mission_vrail_activity_data.get_squads(squad_list);
    }
    return Vehicle_rail_activity_data.mode == activity_get_mode() ? Vehicle_rail_activity_data.get_squads(squad_list) : 0;
}

// 0x007A3380 (:216)
void vrail_stop_squad_spawns()
{
    if (Mission_vrail_activity_data.is_active())
    {
        Mission_vrail_activity_data.stop_squad_spawning();
    }
    else if (Vehicle_rail_activity_data.mode == activity_get_mode())
    {
        Vehicle_rail_activity_data.stop_squad_spawning();
    }
}
