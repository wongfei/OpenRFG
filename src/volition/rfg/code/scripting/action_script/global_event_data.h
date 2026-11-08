// Original: volition/rfg/code/scripting/action_script/global_event_data.h
//
// The payloads of the global events, one per event type (get_type_static), posted by the game and evaluated by script
// events.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_enum.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"

class item;
class patrol_node_handle;
struct weapon_info;

// size 0xC
struct building_damaged_event : public event_data
{
    building_damaged_event(); // 0x00A05840 (.h:34)
    static event_type get_type_static(); // 0x00B1F620 (.h:34)

    unsigned int building; // 0x04
    float percent; // 0x08
};

// size 0x10
struct building_damaged_by_explosion_event : public event_data
{
    building_damaged_by_explosion_event(); // 0x00710EA0 (.h:39)
    static event_type get_type_static(); // 0x00B1F630 (.h:39)

    unsigned int building; // 0x04
    unsigned int info_handle; // 0x08
    unsigned int attacker; // 0x0C
};

// size 0x8
struct building_destroyed_event : public event_data
{
    building_destroyed_event(); // 0x00A05850 (.h:45)
    static event_type get_type_static(); // 0x00B1F640 (.h:45)

    unsigned int building; // 0x04
};

// size 0x8
struct mover_stream_in_event : public event_data
{
    mover_stream_in_event(); // 0x00710EB0 (.h:168)
    static event_type get_type_static();

    unsigned int handle; // 0x04
};

// size 0x4
struct salvage_created_event : public event_data
{
    salvage_created_event(); // 0x00A05860 (.h:49)
    static event_type get_type_static(); // 0x00B1F650 (.h:49)

};

// size 0x8
struct user_defined_event : public event_data
{
    user_defined_event(); // 0x00B1F660 (.h:52)
    static event_type get_type_static(); // 0x00B1F670 (.h:52)

    const char* desc; // 0x04
};

// size 0x8
struct weapon_acquired_event : public event_data
{
    weapon_acquired_event(); // 0x00943040 (.h:56)
    static event_type get_type_static(); // 0x00B1F680 (.h:56)

    unsigned int w_handle; // 0x04
};

// size 0x8
struct ammo_acquired_event : public event_data
{
    ammo_acquired_event(); // 0x00A05870 (.h:60)
    static event_type get_type_static(); // 0x00B1F690 (.h:60)

    const weapon_info* w_info; // 0x04
};

// size 0x8
struct item_acquired_event : public event_data
{
    item_acquired_event();
    static event_type get_type_static(); // 0x00B1F6A0 (.h:64)

    const item* itemp; // 0x04
};

// size 0x10
struct raid_state_change_event : public event_data
{
    raid_state_change_event(); // 0x00753A80 (.h:68)
    static event_type get_type_static(); // 0x00B1F6B0 (.h:68)

    unsigned int raid_handle; // 0x04
    human_teams team; // 0x08
    raid_team_states new_state; // 0x0C
};

// size 0xC
struct ha_state_change_event : public event_data
{
    ha_state_change_event(); // 0x00753A90 (.h:74)
    static event_type get_type_static(); // 0x00B1F6C0 (.h:74)

    unsigned int ha_handle; // 0x04
    int defender_count; // 0x08
};

// size 0x10
struct activity_status_event : public event_data
{
    activity_status_event(); // 0x00753AA0 (.h:79)
    static event_type get_type_static(); // 0x00B1F6D0 (.h:79)

    activity_modes mode; // 0x04
    unsigned int node_handle; // 0x08
    base_activity_states activity_state; // 0x0C
};

// size 0xC
struct raid_complete_event : public event_data
{
    raid_complete_event(); // 0x00753AB0 (.h:86)
    static event_type get_type_static(); // 0x00B1F6E0 (.h:86)

    unsigned int raid_handle; // 0x04
    human_teams winning_team; // 0x08
};

// size 0x8
struct turret_destroyed_event : public event_data
{
    turret_destroyed_event(); // 0x00A05880 (.h:91)
    static event_type get_type_static(); // 0x00B1F6F0 (.h:91)

    unsigned int turret_handle; // 0x04
};

// size 0x8
struct vehicle_destroyed_event : public event_data
{
    vehicle_destroyed_event(); // 0x00B62E50 (.h:95)
    static event_type get_type_static(); // 0x00B1F700 (.h:95)

    unsigned int vehicle_handle; // 0x04
};

// size 0x8
struct human_dead_event : public event_data
{
    human_dead_event(); // 0x00A05890 (.h:99)
    static event_type get_type_static(); // 0x00B1F710 (.h:99)

    unsigned int human_handle; // 0x04
};

// size 0xC
struct dialogue_event : public event_data
{
    dialogue_event(); // 0x00B1F720 (.h:103)
    static event_type get_type_static(); // 0x00B1F730 (.h:103)

    bool is_start; // 0x04
    voice_line_handle voice_line; // 0x08
};

// size 0xC
struct edf_control_lowered_event : public event_data
{
    edf_control_lowered_event(); // 0x00753AC0 (.h:108)
    static event_type get_type_static(); // 0x00B1F740 (.h:108)

    float prev_percent; // 0x04
    float percent; // 0x08
};

// size 0xC
struct civilian_morale_lowered_event : public event_data
{
    civilian_morale_lowered_event(); // 0x00753AD0 (.h:113)
    static event_type get_type_static(); // 0x00B1F750 (.h:113)

    float prev_percent; // 0x04
    float percent; // 0x08
};

// size 0xC
struct civilian_morale_raised_event : public event_data
{
    civilian_morale_raised_event(); // 0x00753AE0 (.h:118)
    static event_type get_type_static(); // 0x00B1F760 (.h:118)

    float prev_percent; // 0x04
    float percent; // 0x08
};

// size 0xC
struct object_enter_trigger_region_event : public event_data
{
    object_enter_trigger_region_event(); // 0x00A058A0 (.h:123)
    static event_type get_type_static(); // 0x00B1F770 (.h:123)

    unsigned int object_handle; // 0x04
    unsigned int trigger_handle; // 0x08
};

// size 0xC
struct object_exit_trigger_region_event : public event_data
{
    object_exit_trigger_region_event(); // 0x00A058B0 (.h:128)
    static event_type get_type_static(); // 0x00B1F780 (.h:128)

    unsigned int object_handle; // 0x04
    unsigned int trigger_handle; // 0x08
};

// size 0x8
struct player_enter_restricted_area_event : public event_data
{
    player_enter_restricted_area_event(); // 0x00A058C0 (.h:133)
    static event_type get_type_static(); // 0x00B1F790 (.h:133)

    unsigned int area_handle; // 0x04
};

// size 0x8
struct squad_killed_event : public event_data
{
    squad_killed_event(); // 0x00A058D0 (.h:137)
    static event_type get_type_static(); // 0x00B1F7A0 (.h:137)

    unsigned int squad_handle; // 0x04
};

// size 0x8
struct squad_member_killed_event : public event_data
{
    squad_member_killed_event(); // 0x00A058E0 (.h:141)
    static event_type get_type_static(); // 0x00B1F7B0 (.h:141)

    unsigned int squad_handle; // 0x04
};

// size 0x8
struct player_exit_restricted_area_event : public event_data
{
    player_exit_restricted_area_event(); // 0x00A058F0 (.h:145)
    static event_type get_type_static(); // 0x00B1F7C0 (.h:145)

    unsigned int area_handle; // 0x04
};

// size 0x4
struct mission_success_event : public event_data
{
    mission_success_event(); // 0x00B1F7D0 (.h:149)
    static event_type get_type_static(); // 0x00B1F7E0 (.h:149)

};

// size 0x8
struct mission_failure_event : public event_data
{
    mission_failure_event(); // 0x00753AF0 (.h:152)
    static event_type get_type_static(); // 0x00B1F7F0 (.h:152)

    const char* desc; // 0x04
};

// size 0x8
struct timer_elapsed_event : public event_data
{
    timer_elapsed_event(); // 0x00B1F800 (.h:156)
    static event_type get_type_static(); // 0x00B1F810 (.h:156)

    unsigned int handle; // 0x04
};

// size 0x8
struct objective_complete_event : public event_data
{
    objective_complete_event(); // 0x00B1F820 (.h:160)
    static event_type get_type_static(); // 0x00B1F830 (.h:160)

    int handle; // 0x04
};

// size 0x8
struct mission_objective_complete_event : public event_data
{
    mission_objective_complete_event(); // 0x00B1F840 (.h:164)
    static event_type get_type_static(); // 0x00B1F850 (.h:164)

    int handle; // 0x04
};

// size 0x8
struct recruitment_event : public event_data
{
    recruitment_event(); // 0x00A05900 (.h:172)
    static event_type get_type_static();

    unsigned int handle; // 0x04
};

// size 0xC
struct alert_level_change_event : public event_data
{
    alert_level_change_event(); // 0x00753B00 (.h:176)
    static event_type get_type_static(); // 0x00B1F860 (.h:176)

    gfm_alert_levels alert_level; // 0x04
    gfm_alert_levels old_level; // 0x08
};

// size 0xC
struct player_kills_vehicle_event : public event_data
{
    player_kills_vehicle_event(); // 0x00B62E60 (.h:182)
    static event_type get_type_static(); // 0x00B1F870 (.h:182)

    unsigned int handle; // 0x04
    human_teams team; // 0x08
};

// size 0x8
struct player_kills_human_event : public event_data
{
    player_kills_human_event(); // 0x00A05910 (.h:188)
    static event_type get_type_static(); // 0x00B1F880 (.h:188)

    unsigned int handle; // 0x04
};

// size 0x8
struct road_becomes_blocked_event : public event_data
{
    road_becomes_blocked_event();
    static event_type get_type_static(); // 0x00B1F890 (.h:192)

    unsigned int road_handle; // 0x04
};

// size 0x8
struct bridge_becomes_unusable_event : public event_data
{
    bridge_becomes_unusable_event(); // 0x00AE9F20 (.h:196)
    static event_type get_type_static(); // 0x00B1F8A0 (.h:196)

    unsigned int bridge_handle; // 0x04
};

// size 0xC
struct player_kills_vehicle_from_vehicle_event : public event_data
{
    player_kills_vehicle_from_vehicle_event(); // 0x00B62E70 (.h:201)
    static event_type get_type_static(); // 0x00B1F8B0 (.h:201)

    unsigned int handle; // 0x04
    human_teams team; // 0x08
};

// size 0x10
struct vehicle_takes_damage_event : public event_data
{
    vehicle_takes_damage_event(); // 0x00B62E80 (.h:207)
    static event_type get_type_static(); // 0x00B1F8C0 (.h:207)

    unsigned int handle; // 0x04
    unsigned int attacker; // 0x08
    float health_level; // 0x0C
};

// size 0x8
struct player_enters_vehicle_event : public event_data
{
    player_enters_vehicle_event(); // 0x00B62E90 (.h:214)
    static event_type get_type_static(); // 0x00B1F8D0 (.h:214)

    unsigned int handle; // 0x04
};

// size 0xC
struct npc_enters_vehicle_event : public event_data
{
    npc_enters_vehicle_event(); // 0x00B62EA0 (.h:219)
    static event_type get_type_static(); // 0x00B1F8E0 (.h:219)

    unsigned int npc_handle; // 0x04
    unsigned int vehicle_handle; // 0x08
};

// size 0x8
struct player_exits_vehicle_event : public event_data
{
    player_exits_vehicle_event(); // 0x00B62EB0 (.h:225)
    static event_type get_type_static(); // 0x00B1F8F0 (.h:225)

    unsigned int handle; // 0x04
};

// size 0x4
struct mission_vrail_complete_event : public event_data
{
    mission_vrail_complete_event(); // 0x00753B10 (.h:230)
    static event_type get_type_static(); // 0x00B1F900 (.h:230)

};

// size 0x8
struct player_under_attack_event : public event_data
{
    player_under_attack_event(); // 0x005DBE30 (.h:233)
    static event_type get_type_static(); // 0x00B1F910 (.h:233)

    unsigned int handle; // 0x04
};

// size 0xC
struct vehicle_under_attack_event : public event_data
{
    vehicle_under_attack_event(); // 0x005DBE40 (.h:237)
    static event_type get_type_static(); // 0x00B1F920 (.h:237)

    unsigned int attacker_handle; // 0x04
    unsigned int vehicle_handle; // 0x08
};

// size 0x8
struct player_damaged_by_explosion_event : public event_data
{
    player_damaged_by_explosion_event(); // 0x00A05920 (.h:242)
    static event_type get_type_static(); // 0x00B1F930 (.h:242)

    unsigned int info_handle; // 0x04
};

// size 0x8
struct cont_attack_squad_reached_end_event : public event_data
{
    cont_attack_squad_reached_end_event(); // 0x00753B20 (.h:246)
    static event_type get_type_static(); // 0x00B1F940 (.h:246)

    int count; // 0x04
};

// size 0xC
struct patrol_squad_reached_node_event : public event_data
{
    patrol_squad_reached_node_event(); // 0x005DBE50 (.h:250)
    static event_type get_type_static(); // 0x00B1F950 (.h:250)

    unsigned int squad_handle; // 0x04
    const patrol_node_handle* patrol_node; // 0x08
};

// size 0x8
struct squad_members_spawned_event : public event_data
{
    squad_members_spawned_event(); // 0x00A05930 (.h:255)
    static event_type get_type_static(); // 0x00B1F960 (.h:255)

    unsigned int spawn_node_handle; // 0x04
};

// size 0x8
struct walker_mount_vehicle_event : public event_data
{
    walker_mount_vehicle_event(); // 0x00B62EC0 (.h:259)
    static event_type get_type_static(); // 0x00B1F970 (.h:259)

    unsigned int walker_handle; // 0x04
};

// size 0x10
struct human_using_action_node_event : public event_data
{
    human_using_action_node_event(); // 0x00A05940 (.h:263)
    static event_type get_type_static(); // 0x00B1F980 (.h:263)

    unsigned int action_node_handle; // 0x04
    unsigned int human_handle; // 0x08
    bool start; // 0x0C
};

// size 0x8
struct player_gets_salvage_event : public event_data
{
    player_gets_salvage_event(); // 0x00A05950 (.h:269)
    static event_type get_type_static(); // 0x00B1F990 (.h:269)

    int salvage_val; // 0x04
};

// size 0x8
struct satellite_reaches_target_event : public event_data
{
    satellite_reaches_target_event(); // 0x006C4200 (.h:273)
    static event_type get_type_static(); // 0x00B1F9A0 (.h:273)

    unsigned int target_handle; // 0x04
};

// size 0xC
struct mover_human_special_collision_event : public event_data
{
    mover_human_special_collision_event(); // 0x0080D2F0 (.h:277)
    static event_type get_type_static();

    unsigned int mover_handle; // 0x04
    unsigned int human_handle; // 0x08
};

// size 0x4
struct player_enters_critical_area_event : public event_data
{
    player_enters_critical_area_event(); // 0x00A05960 (.h:282)
    static event_type get_type_static(); // 0x00B1F9B0 (.h:282)

};

// size 0x8
struct squad_members_despawned : public event_data
{
    squad_members_despawned(); // 0x00A05970 (.h:286)
    static event_type get_type_static(); // 0x00B1F9C0 (.h:286)

    unsigned int squad_handle; // 0x04
};

// size 0x8
struct npc_despawned : public event_data
{
    npc_despawned(); // 0x00A05980 (.h:291)
    static event_type get_type_static(); // 0x00B1F9D0 (.h:291)

    unsigned int npc_handle; // 0x04
};

// size 0x4
struct player_fires_weapon_or_detonates_event : public event_data
{
    player_fires_weapon_or_detonates_event(); // 0x00A05990 (.h:295)
    static event_type get_type_static(); // 0x00B1F9E0 (.h:295)

};

// size 0x8
struct human_stopped_being_rendered_event : public event_data
{
    human_stopped_being_rendered_event(); // 0x00A059A0 (.h:298)
    static event_type get_type_static(); // 0x00B1F9F0 (.h:298)

    unsigned int target_handle; // 0x04
};

// 0x00753A80 (.h:68)
inline raid_state_change_event::raid_state_change_event()
    : event_data(EVENT_RAID_STATE_CHANGE)
{
}

// 0x00753A90 (.h:74)
inline ha_state_change_event::ha_state_change_event()
    : event_data(EVENT_HOUSE_ARREST_STATUS)
{
}

// 0x00753AA0 (.h:79)
inline activity_status_event::activity_status_event()
    : event_data(EVENT_ACTIVITY_STATE_CHANGE)
{
}

// 0x00753AB0 (.h:86)
inline raid_complete_event::raid_complete_event()
    : event_data(EVENT_RAID_COMPLETE)
{
}
