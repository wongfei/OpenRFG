// Original: volition/rfg/code/scripting/action_script/script_definitions.cpp
//
// The callbacks behind script actions, functions and events, and their dictionary registration. The registration table
// of all of them is generated from rfg.exe by tools/headless/scriptdefgen.py (script_definition_table.inl).

#include "volition/rfg/code/scripting/action_script/script_definitions.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/air_bombing.h"

#include "volition/rfg/code/ai/ai_scripting.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/code_effects/bullet_sparks.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/code_effects/tracers.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase.h"
#include "volition/rfg/code/gameplay/activities/activity_courier_chase_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_mission_vrail.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/restore_list.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/hud_ui_weapon_cabinet.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/objects/critical_area.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_courier_end_point.h"
#include "volition/rfg/code/objects/object_delivery_node.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_area_defense_node.h"
#include "volition/rfg/code/objects/object_riding_shotgun_node.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/objects/object_house_arrest_node.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_raid_node.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_roadblock_node.h"
#include "volition/rfg/code/objects/object_spawn_region.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/scripting/action_script/script_timer.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_audio.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/camera_satellite.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/interface/ui_document_hud_satellite.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/gameflow/continuous_support.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.h"
#include "volition/rfg/code/gameplay/activities/morale_effect_manager.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_time_of_day.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/hud/objectives.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_trigger_region.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/string_comp.h"

#include <string.h>

extern bool Dialogue_disable_dialogue_start; // 0x0361141D (dialogue_manager.cpp)
extern activity_courier_chase_data Courier_chase_data; // 0x02242BA8 (activity_courier_chase.cpp)
extern int Sniper_attack_player_chance; // 0x01D65950 (custom_actions.cpp)
extern gfm_orders_manager Orders_manager; // 0x0222DE40 (gameplay.cpp)
extern gfm_smash_building_coordinator Smash_building_coordinator; // 0x016572EC (gameplay.cpp)
static const gfm_orders_flags DEFAULT_ORDERS_FLAGS(true, false); // 0x02270D39

// The object of a handle when it has this type (and subtype, for npcs), else null: rfg.exe inlines this everywhere.
template<typename T>
static T* object_of_type(unsigned int handle, int obj_type)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == obj_type ? (T*)objp : nullptr;
}

// Magic numbers: the object types the callbacks test (object_type values).
static const int OBJ_TYPE_HUMAN = 0x00;
static const int OBJ_TYPE_VEHICLE = 0x03;
static const int OBJ_TYPE_NAVPOINT = 0x0A;
static const int OBJ_TYPE_SQUAD = 0x0B;
static const int OBJ_TYPE_PATROL = 0x0E;
static const int OBJ_TYPE_TRIGGER_REGION = 0x14;
static const int OBJ_TYPE_VEHICLE_SPAWN_NODE = 0x19;
static const int OBJ_TYPE_NPC_SPAWN_NODE = 0x1A;
static const int OBJ_TYPE_ACTION_NODE = 0x1C;
static const int OBJ_TYPE_SQUAD_SPAWN_NODE = 0x1D;
static const int OBJ_TYPE_SAFEHOUSE = 0x30;
static const int HUMAN_SUBTYPE_NPC = 2;
static const int HUMAN_SUBTYPE_PLAYER = 3;

static npc* npc_from_handle(unsigned int handle)
{
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OBJ_TYPE_HUMAN && objp->get_sub_type() == HUMAN_SUBTYPE_NPC ? (npc*)objp : nullptr;
}

// Magic number: a script's priority number maps onto dialogue_priorities, clamped.
static dialogue_priorities dialogue_priority_from_number(float number)
{
    int priority = (int)fl_round(number);
    if (priority < 0)
    {
        priority = DIALOGUE_PRIORITY_AMBIENT;
    }
    else if (priority > 3)
    {
        priority = DIALOGUE_PRIORITY_HIGH;
    }
    return (dialogue_priorities)priority;
}

// Magic number: 7, the size of Vip_character_names (and the capacity of the lookup table).
static const char* Vip_character_names[7] = {"Samanya", "RFCommander", "OldCoot", "Dan", "Carmen", "Kyle", "EDF_Colonel_Baker"}; // 0x016B0C98
static farray<vip_npc_name_lookup,7> Vip_npc_lookup_table; // 0x016B2610

// 0x00B31540 (:919): which living npc each named character is, refreshed every frame.
void script_cache_vip_npcs()
{
    if (Vip_npc_lookup_table.size() != 7)
    {
        for (int i = 0; i < 7; i++)
        {
            vip_npc_name_lookup entry;
            entry.npc_name_crc = crc_stri(Vip_character_names[i], 0);
            entry.npc_handle = 0xFFFFFFFF;
            Vip_npc_lookup_table.add(entry);
        }
    }
    for (int i = 0; i < 7; i++)
    {
        if (Vip_npc_lookup_table[i].npc_handle != 0xFFFFFFFF && npc_from_handle(Vip_npc_lookup_table[i].npc_handle) == nullptr)
        {
            Vip_npc_lookup_table[i].npc_handle = 0xFFFFFFFF;
        }
    }
    for (int n = 0; n < World.get_num_objects<npc>(); n++)
    {
        npc* np = World.get_object<npc>(n);
        const unsigned int crc = crc_stri(np->h_info->name, 0);
        for (int i = 0; i < Vip_npc_lookup_table.size(); i++)
        {
            if (Vip_npc_lookup_table[i].npc_handle == 0xFFFFFFFF && crc == Vip_npc_lookup_table[i].npc_name_crc)
            {
                Vip_npc_lookup_table[i].npc_handle = np->get_handle();
                break;
            }
        }
    }
}

// ---- functions

static const float SCRIPT_TRUE = 1.0f;

// 0x00B1FDF0 (:244)
void Function_callback_and(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    for (int i = 0; args[i] != nullptr; i++)
    {
        if (!(bool)*args[i])
        {
            return;
        }
    }
    dest_result.number = SCRIPT_TRUE;
}

// 0x00B1FE40
void Function_callback_or(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    for (int i = 0; args[i] != nullptr; i++)
    {
        if ((bool)*args[i])
        {
            dest_result.number = SCRIPT_TRUE;
            return;
        }
    }
}

// 0x00B1FE90
void Function_callback_not(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    if (!(bool)*args[0])
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B1FED0
void Function_callback_equal(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    for (int i = 1; args[i] != nullptr; i++)
    {
        if (first != (float)*args[i])
        {
            return;
        }
    }
    dest_result.number = SCRIPT_TRUE;
}

// 0x00B1FF50
void Function_callback_not_equal(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    if (first != (float)*args[1])
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B20130: true once the first argument was seen true and then the second is true.
void Function_callback_then(script_variant& dest_result, script_variant& custom_value, script_node** args)
{
    dest_result.number = 0.0f;
    const float state = custom_value.number;
    if (state == 0.0f && (bool)*args[0])
    {
        custom_value.number = 1.0f;
        args[1]->reset();
    }
    else if (state != 1.0f)
    {
        return;
    }
    if ((bool)*args[1])
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B20310
void Function_callback_get_alert_value(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = 0.0f;
    dest_result.number = gsm_get_alert_value();
}

// 0x00B202F0
void Function_callback_get_alert_level(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = 0.0f;
    dest_result.number = (float)(int)gsm_get_alert_level();
}

// 0x00B20420
void Function_callback_player_in_radius(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    object* objp = World.object_from_handle((unsigned int)*args[0]);
    if (objp != nullptr)
    {
        const float radius = (float)*args[1];
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            const float dx = pp->pos.x - objp->pos.x;
            const float dz = pp->pos.z - objp->pos.z;
            if (dz * dz + dx * dx < radius * radius)
            {
                dest_result.number = SCRIPT_TRUE;
            }
        }
    }
}

// 0x00B20600 (:872)
void Function_callback_mission_is_active(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = mission_is_active() ? SCRIPT_TRUE : 0.0f;
}

// 0x00B20660
void Function_callback_get_player_handle(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.obj = 0xFFFFFFFF;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        dest_result.obj = pp->get_handle();
    }
}

// 0x00B269F0
void Function_callback_player_in_trigger_region(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const unsigned int handle = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        trigger_region* trigger = object_of_type<trigger_region>(handle, OBJ_TYPE_TRIGGER_REGION);
        if (trigger != nullptr && trigger->is_enabled() && trigger_region_point_inside(trigger, pp->pos))
        {
            dest_result.number = SCRIPT_TRUE;
        }
    }
}

// 0x00B26A60
void Function_callback_object_in_trigger_region(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int trigger_handle = (unsigned int)*args[1];
    object* objp = World.object_from_handle(handle);
    trigger_region* trigger = object_of_type<trigger_region>(trigger_handle, OBJ_TYPE_TRIGGER_REGION);
    if (objp != nullptr && trigger != nullptr && trigger->is_enabled() && trigger_region_point_inside(trigger, objp->pos))
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B26CE0
void Function_callback_get_npc_from_spawn_object(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    const unsigned int handle = (unsigned int)*args[0];
    object_npc_spawn_node* npc_node = object_of_type<object_npc_spawn_node>(handle, OBJ_TYPE_NPC_SPAWN_NODE);
    if (npc_node != nullptr)
    {
        dest_result.obj = npc_node->get_child_npc_handle();
        return;
    }
    object_squad_spawn_node* squad_node = object_of_type<object_squad_spawn_node>(handle, OBJ_TYPE_SQUAD_SPAWN_NODE);
    if (squad_node != nullptr)
    {
        object_squad* squad = object_of_type<object_squad>(squad_node->get_child_squad_handle(), OBJ_TYPE_SQUAD);
        if (squad != nullptr && squad->member_array.size() > 0)
        {
            dest_result.obj = squad->member_array[0];
        }
    }
}

// 0x00B26D70
void Function_callback_get_squad_from_spawn_object(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>((unsigned int)*args[0], OBJ_TYPE_SQUAD_SPAWN_NODE);
    if (node != nullptr)
    {
        dest_result.obj = node->get_child_squad_handle();
    }
}

// 0x00B26E00
void Function_callback_get_vehicle_from_spawn_object(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    object* node = World.object_from_handle((unsigned int)*args[0]);
    dest_result.obj = 0xFFFFFFFF;
    if (node == nullptr)
    {
        return;
    }
    if (node->get_obj_type() == OBJ_TYPE_VEHICLE_SPAWN_NODE)
    {
        dest_result.obj = ((object_vehicle_spawn_node*)node)->get_child_vehicle_handle();
    }
    else if (node->get_obj_type() == OBJ_TYPE_SQUAD_SPAWN_NODE)
    {
        object* vehicle = object_of_type<object>(((object_squad_spawn_node*)node)->get_child_vehicle_handle(), OBJ_TYPE_VEHICLE);
        if (vehicle != nullptr)
        {
            dest_result.obj = vehicle->get_handle();
        }
    }
}

// 0x00B26EC0
void Function_callback_npc_is_alive(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    npc* np = npc_from_handle((unsigned int)*args[0]);
    if (np != nullptr && !np->is_killed())
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B27310
void Function_callback_player_ammo_type_is_full(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const weapon_info* info = weapons_get_weapon_info((unsigned int)(game_handle)*args[0]);
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->inventory == nullptr)
    {
        return;
    }
    inventory_item* item = pp->inventory;
    weapon* wp;
    while (true)
    {
        wp = item->weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(item->weapon_handle) : nullptr;
        if (wp != nullptr && wp->w_info == info)
        {
            break;
        }
        item = item->next;
        if (item == pp->inventory || item == nullptr)
        {
            return;
        }
    }
    // Charges already thrown count against the reserve.
    const int thrown = wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE ? count_charges(pp->get_handle()) : 0;
    const int room = wp->get_max_rounds() - wp->get_rounds_in_magazine() - wp->get_rounds_in_reserve();
    if (room - thrown <= 0)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// ---- events

// 0x00B207B0
event_result Event_callback_building_damaged(const building_damaged_event* data, script_node** args)
{
    if (activity_is_active(ACTIVITY_MODE_VEHICLE_RAIL))
    {
        return EVENT_IGNORED;
    }
    return data->building == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20860
event_result Event_callback_building_damaged_by_explosion_type(const building_damaged_by_explosion_event* data, script_node** args)
{
    const unsigned int building = (unsigned int)*args[0];
    const game_handle info = (game_handle)*args[1];
    return data->building == building && (game_handle)data->info_handle == info ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B208C0
event_result Event_callback_building_destroyed(const building_destroyed_event* data, script_node** args)
{
    if (activity_is_active(ACTIVITY_MODE_VEHICLE_RAIL))
    {
        return EVENT_IGNORED;
    }
    return data->building == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B208F0: counts as the listed buildings fall. The destroyed one is swapped out of the list; the event stays
// registered until the last one (EVENT_OK = 2 keeps it, EVENT_HANDLED = 1 unregisters it).
event_result Event_callback_num_buildings_destroyed(const building_destroyed_event* data, script_node** args)
{
    if (activity_is_active(ACTIVITY_MODE_VEHICLE_RAIL))
    {
        return EVENT_IGNORED;
    }
    unsigned int* list = (unsigned int*)*args[0];
    int count = 0;
    while (list[count] != 0xFFFFFFFF)
    {
        count++;
    }
    if (count == 0)
    {
        return EVENT_IGNORED;
    }
    const int last = count - 1;
    for (int i = 0; list[i] != 0xFFFFFFFF; i++)
    {
        if (data->building == list[i])
        {
            list[i] = list[last];
            list[last] = 0xFFFFFFFF;
            return last != 0 ? EVENT_OK : EVENT_HANDLED;
        }
    }
    return EVENT_IGNORED;
}

// 0x00B20980 (:1732)
event_result Event_callback_user_event(const user_defined_event* data, script_node** args)
{
    return v_stricmp(data->desc, (const char*)*args[0]) == 0 ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20A20
event_result Event_callback_ammo_acquired(const ammo_acquired_event* data, script_node** args)
{
    const weapon_info* info = weapons_get_weapon_info((unsigned int)(game_handle)*args[0]);
    return strcmp(data->w_info->name, info->name) == 0 ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20BE0
event_result Event_callback_squad_killed(const squad_killed_event* data, script_node** args)
{
    return data->squad_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20C60
event_result Event_callback_human_dead(const human_dead_event* data, script_node** args)
{
    return data->human_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20C80
event_result Event_callback_dialogue_line_started(const dialogue_event* data, script_node** args)
{
    if (!data->is_start)
    {
        return EVENT_IGNORED;
    }
    return (game_handle)data->voice_line == (game_handle)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20CB0
event_result Event_callback_dialogue_line_finished(const dialogue_event* data, script_node** args)
{
    if (data->is_start)
    {
        return EVENT_IGNORED;
    }
    return (game_handle)data->voice_line == (game_handle)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20DD0
event_result Event_callback_alert_level_raised(const alert_level_change_event* data, script_node** args)
{
    return (int)*args[0] <= (int)data->alert_level ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20D80
event_result Event_callback_civilian_morale_raised(const civilian_morale_raised_event* data, script_node** args)
{
    const float percent = (float)*args[0];
    return data->prev_percent < percent && percent <= data->percent ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20D30
event_result Event_callback_civilian_morale_lowered(const civilian_morale_lowered_event* data, script_node** args)
{
    const float percent = (float)*args[0];
    return percent < data->prev_percent && data->percent <= percent ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20CE0
event_result Event_callback_edf_control_lowered(const edf_control_lowered_event* data, script_node** args)
{
    const float percent = (float)*args[0];
    return percent < data->prev_percent && data->percent <= percent ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20DF0
event_result Event_callback_player_enter_trigger_region(const object_enter_trigger_region_event* data, script_node** args)
{
    const unsigned int trigger = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    return pp != nullptr && data->trigger_handle == trigger && data->object_handle == pp->get_handle() ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20F40
event_result Event_callback_player_kills_npc(const player_kills_human_event* data, script_node** args)
{
    return (unsigned int)*args[0] == data->handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21160
event_result Event_callback_mission_specific_success(const mission_success_event*, script_node** args)
{
    return (game_handle)*args[0] == (game_handle)mission_get_current_mission() ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21190
event_result Event_callback_mission_specific_failure(const mission_failure_event*, script_node** args)
{
    return (game_handle)*args[0] == (game_handle)mission_get_current_mission() ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21320 (:2684)
event_result Event_callback_satellite_reaches_target(const satellite_reaches_target_event* data, script_node** args)
{
    return data->target_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B211B0
event_result Event_callback_timer_elapsed(const timer_elapsed_event* data, script_node** args)
{
    return data->handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B279F0
event_result Event_callback_patrol_squad_reached_node(const patrol_squad_reached_node_event* data, script_node** args)
{
    const unsigned int node = (unsigned int)*args[0];
    const unsigned int squad = (unsigned int)*args[1];
    if (object_of_type<object>(node, OBJ_TYPE_NAVPOINT) != nullptr && object_of_type<object>(squad, OBJ_TYPE_SQUAD) != nullptr && node == data->patrol_node->nav_handle &&
        squad == data->squad_handle)
    {
        return EVENT_HANDLED;
    }
    return EVENT_IGNORED;
}

// ---- actions

// 0x00B21420
void Action_callback_tool_tip_add(script_node** args)
{
    const char* name = (const char*)*args[0];
    // Magic number: seconds to milliseconds.
    const double delay = args[1] == nullptr ? 0.0 : (double)((float)*args[1] * 1000.0f);
    ui_tool_tips_add_tip(name, (int)delay, nullptr);
}

// 0x00B21520 (:2894)
void Action_callback_add_mission_objective(script_node** args)
{
    GAME_TRACE("objective", "add handle %d", (int)(game_handle)*args[0]);
    add_objective((int)(game_handle)*args[0]);
}

// 0x00B21560
void Action_callback_add_mission_objective_icon(script_node** args)
{
    const int handle = (int)(game_handle)*args[0];
    const minimap_elements icon = (minimap_elements)(int)*args[1];
    for (int i = 2; args[i] != nullptr; i++)
    {
        const unsigned int object_handle = (unsigned int)*args[i];
        object* objp = World.object_from_handle(object_handle);
        if (objp != nullptr)
        {
            if (objp->get_obj_type() == OBJ_TYPE_SQUAD)
            {
                ((object_squad*)objp)->assign_objective_icons_to_members(handle, icon);
            }
            else
            {
                add_objective_icon(handle, object_handle, icon, false);
            }
        }
    }
}

// 0x00B21720
void Action_callback_mission_draft_mode(script_node**)
{
    morale_start_draft_mode();
}

// 0x00B21A10
void Action_callback_add_icon(script_node** args)
{
    // Magic numbers: the script's icon number picks one of these.
    const minimap_elements icons[5] = {MAP_ELEMENT_STAR_RED_ICON, (minimap_elements)1, (minimap_elements)2, (minimap_elements)3, (minimap_elements)0x28};
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int index = (unsigned int)(int)*args[1];
    if (handle == 0xFFFFFFFF || index >= 5)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp != nullptr)
    {
        if (objp->get_obj_type() == OBJ_TYPE_SQUAD)
        {
            ((object_squad*)objp)->assign_icons_to_members(icons[index], MAP_ICON_TYPE_STICKY);
            return;
        }
        Cartographer.add_icon(handle, icons[index], true, false, false);
    }
}

// 0x00B21D30
void Action_callback_lower_alert_value(script_node** args)
{
    const float value = (float)*args[0];
    gsm_set_alert_value(gsm_get_alert_value() - value);
}

// 0x00B21CF0
void Action_callback_raise_alert_value(script_node** args)
{
    const float value = (float)*args[0];
    gsm_set_alert_value(gsm_get_alert_value() + value);
}

// 0x00B21CC0
void Action_callback_lower_alert_level(script_node** args)
{
    const gfm_alert_levels level = (gfm_alert_levels)(int)*args[0];
    if ((unsigned int)level < NUM_ALERT_LEVELS && (int)level < (int)gsm_get_alert_level())
    {
        gsm_set_alert_level(level);
    }
}

// 0x00B21C90
void Action_callback_raise_alert_level(script_node** args)
{
    const gfm_alert_levels level = (gfm_alert_levels)(int)*args[0];
    if ((unsigned int)level < NUM_ALERT_LEVELS && (int)gsm_get_alert_level() < (int)level)
    {
        gsm_set_alert_level(level);
    }
}

// 0x00B21C50
void Action_callback_cap_alert_level(script_node** args)
{
    const gfm_alert_levels min = (gfm_alert_levels)(int)*args[0];
    const gfm_alert_levels max = (gfm_alert_levels)(int)*args[1];
    if ((unsigned int)min < NUM_ALERT_LEVELS && (unsigned int)max < NUM_ALERT_LEVELS)
    {
        gsm_set_alert_level_cap(min, max);
    }
}

// 0x00B21DA0
void Action_callback_building_destroyed_message(script_node** args)
{
    hud_ui_mission_set_building_destroyed((const char*)*args[0], OBJECT_IMPORTANCE_LEVEL_MEDIUM, true);
}

// 0x00B21EC0
void Action_callback_give_player_weapon(script_node** args)
{
    const weapon_info* info = weapons_get_weapon_info((unsigned int)(game_handle)*args[0]);
    const int ammo = (int)*args[1];
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        human_add_item_to_inventory(pp, info->name, 1, ammo, -1, true, false, true);
    }
}

// 0x00B22260
void Action_callback_morale_offset_decrease(script_node** args)
{
    gfm_change_morale_offset(-(float)*args[0]);
}

// 0x00B22230
void Action_callback_morale_offset_increase(script_node** args)
{
    gfm_change_morale_offset((float)*args[0]);
}

// 0x00B22200
void Action_callback_morale_offset_set(script_node** args)
{
    gfm_set_morale_offset((float)*args[0]);
}

// 0x00B221D0
void Action_callback_temporary_morale_decrease(script_node** args)
{
    gfm_change_temporary_morale(-(float)*args[0]);
}

// 0x00B221A0
void Action_callback_temporary_morale_increase(script_node** args)
{
    gfm_change_temporary_morale((float)*args[0]);
}

// 0x00B22170
void Action_callback_temporary_morale_set(script_node** args)
{
    gfm_set_temporary_morale((float)*args[0]);
}

// 0x00B22160
void Action_callback_temporary_morale_end(script_node**)
{
    gfm_end_temporary_morale();
}

// 0x00B22150
void Action_callback_temporary_morale_start(script_node**)
{
    gfm_start_temporary_morale();
}

// 0x00B22120
void Action_callback_raise_civilian_morale(script_node** args)
{
    gfm_change_district_morale((float)*args[0], nullptr, nullptr, nullptr);
}

// 0x00B220F0
void Action_callback_lower_civilian_morale(script_node** args)
{
    gfm_change_district_morale(-(float)*args[0], nullptr, nullptr, nullptr);
}

// 0x00B22040
void Action_callback_set_civilian_morale(script_node** args)
{
    float value = (float)*args[0];
    if (value - 0.0f < 0.0f)
    {
        value = 0.0f;
    }
    const float max_value = gfm_get_district_morale_max(nullptr);
    gfm_set_district_morale(value - max_value < 0.0f ? value : max_value, nullptr);
}

// 0x00B21FE0
void Action_callback_lower_edf_control_at_object(script_node** args)
{
    const float value = (float)*args[0];
    const object* objp = World.object_from_handle((unsigned int)*args[1]);
    if (objp != nullptr)
    {
        gfm_change_edf_control(-(value + value), district_get_from_pos(objp->pos), nullptr, nullptr);
    }
}

// 0x00B21FB0
void Action_callback_lower_edf_control(script_node** args)
{
    // Magic: rfg.exe doubles the script's value (FADD ST0,ST0).
    const float value = (float)*args[0];
    gfm_change_edf_control(-(value + value), nullptr, nullptr, nullptr);
}

// 0x00B222F0
void Action_callback_mission_terminate(script_node**)
{
    if (mission_is_active())
    {
        mission_terminate(true);
    }
}

// 0x00B22550
void Action_callback_continuous_support_end(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != 0xFFFFFFFF; i++)
    {
        cont_support_remove_squad(list[i]);
    }
}

// 0x00B22640
void Action_callback_enable_dialogue(script_node** args)
{
    Dialogue_disable_dialogue_start = !(bool)*args[0];
}

// 0x00B22320: empty in rfg.exe.
void Action_callback_satellite_camera_position(script_node** args)
{
}

// 0x00B22330
void Action_callback_satellite_camera_start(script_node** args)
{
    camera_start_satellite_mode();
}

// 0x00B22340
void Action_callback_satellite_camera_show_disconnect_screen(script_node** args)
{
    camera_satellite_shutdown();
}

// 0x00B22350
void Action_callback_satellite_camera_stop(script_node** args)
{
    camera_stop_satellite_mode();
}

// 0x00B22360
void Action_callback_satellite_camera_set_target(script_node** args)
{
    const unsigned int target_handle = (unsigned int)*args[0];
    if (target_handle != 0xFFFFFFFF)
    {
        camera_satellite_set_target(target_handle);
    }
}

// 0x00B22380
void Action_callback_satellite_camera_max_range(script_node** args)
{
    camera_satellite_set_max_range((float)*args[0]);
}

// 0x00B223B0
void Action_callback_satellite_set_weapon_source(script_node** args)
{
    const object* const objp = World.object_from_handle((unsigned int)*args[0]);
    if (objp != nullptr)
    {
        camera_satellite_set_weapon_source_pos(objp->pos);
    }
}

// 0x00B223E0
void Action_callback_satellite_temp_message(script_node** args)
{
    const char* const msg = (const char*)*args[0];
    hud_ui_satellite_set_temp_message(msg, (int)*args[1]);
}

// 0x00B22950
void Action_callback_video_cutscene_play_with_credits(script_node** args)
{
    const char* const filename = (const char*)*args[0];
    video_cutscene_play_with_credits(filename, (unsigned int)*args[1]);
}

// 0x00B22910
void Action_callback_video_cutscene_play(script_node** args)
{
    const char* filename = (const char*)*args[0];
    const unsigned int warp_to_object = (unsigned int)*args[1];
    video_cutscene_play(filename, warp_to_object, nullptr, false, false, nullptr, nullptr, nullptr);
}

// 0x00B22A50
void Action_callback_music_set_limited_level(script_node** args)
{
    gamemusic_set_limited_music_level((GAMEMUSIC_THRESHOLDS)(game_handle)*args[0]);
}

// 0x00B22A70
void Action_callback_npc_set_ambient_vehicles_disabled(script_node** args)
{
    amb_set_ambient_vehicle_spawn_disabled((bool)*args[0]);
}

// 0x00B22A90
void Action_callback_player_set_mission_never_die(script_node** args)
{
    if (mission_is_active())
    {
        mission_set_player_never_die((bool)*args[0]);
    }
}

// 0x00B228B0
void Action_callback_disable_guerrilla_reinforcement(script_node** args)
{
    reinforcement_guerrilla_disabled((bool)*args[0]);
}

// 0x00B22880
void Action_callback_change_tech_level(script_node** args)
{
    gfm_change_tech_level((float)*args[0]);
}

// 0x00B22840
void Action_callback_set_tech_level(script_node** args)
{
    const float level = (float)*args[0];
    if (gfm_get_tech_level() < level)
    {
        gfm_set_tech_level(level);
    }
}

// 0x00B22810
void Action_callback_set_marauder_progression(script_node** args)
{
    const int level = (int)*args[0];
    if (marauder_progression_level() < level)
    {
        marauder_progression_set_level(level);
    }
}

// 0x00B227D0
void Action_callback_set_edf_progression_level(script_node** args)
{
    const int level = (int)*args[0];
    if (edf_progression_level() < level)
    {
        edf_progression_set_level(level);
    }
}

// 0x00B228D0
void Action_callback_reinforcements_enable(script_node**)
{
    reinforcement_set_disable(false);
}

// 0x00B228E0
void Action_callback_reinforcements_disable(script_node**)
{
    reinforcement_set_disable(true);
}

// 0x00B22C30
void Action_callback_set_time_of_day(script_node** args)
{
    const unsigned int hours = (unsigned int)(int)*args[0];
    const unsigned int minutes = (unsigned int)(int)*args[1];
    if (hours < 24 && minutes < 60)
    {
        game_clock_set_time((int)hours, (int)minutes, 0);
        game_time_of_day_do_frame();
    }
}

// 0x00B22C70
void Action_callback_pause_time_of_day(script_node** args)
{
    game_clock_freeze_ingame_time((bool)*args[0]);
}

// 0x00B22CF0
void Action_callback_hide_hud(script_node** args)
{
    hud_hide((bool)*args[0]);
}

// 0x00B22D10
void Action_callback_player_disable_input(script_node**)
{
    if (player_get_local_player() != nullptr)
    {
        control_enable_player_input(false);
    }
}

// 0x00B22D30
void Action_callback_player_enable_input(script_node**)
{
    if (player_get_local_player() != nullptr)
    {
        control_enable_player_input(true);
    }
}

// 0x00B281E0
void Action_callback_trigger_region_enable_list(script_node** args)
{
    unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != 0xFFFFFFFF; i++)
    {
        trigger_region* trigger = object_of_type<trigger_region>(list[i], OBJ_TYPE_TRIGGER_REGION);
        if (trigger != nullptr)
        {
            trigger->enable();
        }
    }
}

// 0x00B28440
void Action_callback_safehouse_set_visible(script_node** args)
{
    const bool visible = (bool)*args[0];
    unsigned int* list = (unsigned int*)*args[1];
    for (int i = 0; list[i] != 0xFFFFFFFF; i++)
    {
        object_safehouse* safehouse = object_of_type<object_safehouse>(list[i], OBJ_TYPE_SAFEHOUSE);
        if (safehouse != nullptr)
        {
            safehouse->set_visible(visible);
        }
    }
}

// 0x00B291B0
void Action_callback_spawn_squad(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>((unsigned int)*args[0], OBJ_TYPE_SQUAD_SPAWN_NODE);
    if (node != nullptr)
    {
        const unsigned int handle = node->spawn_child_object(MAP_ELEMENT_INVALID, MAP_ICON_TYPE_NORMAL);
        if (handle != 0xFFFFFFFF)
        {
            Action_return_value.set(handle);
        }
    }
}

// 0x00B295A0
void Action_callback_squad_despawn_immediate(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OBJ_TYPE_SQUAD);
    if (squad != nullptr)
    {
        Orders_manager.destroy_orders(squad->get_orders_handle());
        squad->despawn();
    }
}

// Shared by orders_defend and orders_patrol: the squad of a squad or a squad node. Returns false when it is done.
static bool orders_find_squad(unsigned int handle, object* target, gfm_orders_types orders_type, object_squad_spawn_node*& squad_node, object_squad*& squad)
{
    squad_node = object_of_type<object_squad_spawn_node>(handle, OBJ_TYPE_SQUAD_SPAWN_NODE);
    if (squad_node == nullptr)
    {
        squad = object_of_type<object_squad>(handle, OBJ_TYPE_SQUAD);
        return squad != nullptr;
    }
    squad = object_of_type<object_squad>(squad_node->get_child_squad_handle(), OBJ_TYPE_SQUAD);
    if (squad == nullptr)
    {
        // Nobody spawned yet: the node keeps the orders for the squad it will spawn.
        cont_support_update_orders(squad_node, orders_type, target->get_handle(), 0xFFFFFFFF);
        return false;
    }
    return true;
}

// 0x00B29A40
void Action_callback_orders_attack(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int target_handle = (unsigned int)*args[1];
    object* target = target_handle != 0xFFFFFFFF ? World.object_from_handle(target_handle) : nullptr;
    if (target == nullptr)
    {
        return;
    }
    object_squad_spawn_node* squad_node;
    object_squad* squad;
    if (!orders_find_squad(handle, target, GFM_ORDERS_TYPE_ATTACK, squad_node, squad))
    {
        return;
    }
    gfm_orders_creation_params params;
    params.ofp.reset();
    params.reset();
    if (!reinforcement_find_destination_location(squad->pos, target->pos, true, params.location_target))
    {
        params.location_target = target->pos;
    }
    params.object_target = target->get_handle();
    params.orders_type = GFM_ORDERS_TYPE_ATTACK;
    params.team = squad->get_squad_team();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    const unsigned int orders = Orders_manager.create_new_orders(params);
    if (orders != 0xFFFFFFFF)
    {
        squad->reassign_orders(orders, nullptr, false);
        if (squad_node != nullptr)
        {
            cont_support_update_orders(squad_node, GFM_ORDERS_TYPE_ATTACK, target->get_handle(), orders);
        }
    }
}

// 0x00B29D00
void Action_callback_orders_defend(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int target_handle = (unsigned int)*args[1];
    object* target = target_handle != 0xFFFFFFFF ? World.object_from_handle(target_handle) : nullptr;
    if (target == nullptr)
    {
        return;
    }
    object_squad_spawn_node* squad_node;
    object_squad* squad;
    if (!orders_find_squad(handle, target, GFM_ORDERS_TYPE_DEFEND, squad_node, squad))
    {
        return;
    }
    gfm_orders_creation_params params;
    params.ofp.reset();
    params.reset();
    if (!reinforcement_find_destination_location(squad->pos, target->pos, true, params.location_target))
    {
        params.location_target = target->pos;
    }
    params.object_target = target->get_handle();
    params.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.team = squad->get_squad_team();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    const unsigned int orders = Orders_manager.create_new_orders(params);
    if (orders != 0xFFFFFFFF)
    {
        squad->reassign_orders(orders, nullptr, false);
        if (squad_node != nullptr)
        {
            cont_support_update_orders(squad_node, GFM_ORDERS_TYPE_DEFEND, target->get_handle(), orders);
        }
    }
}

// 0x00B29E60
void Action_callback_orders_patrol(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int target_handle = (unsigned int)*args[1];
    object* target = target_handle != 0xFFFFFFFF ? World.object_from_handle(target_handle) : nullptr;
    if (target == nullptr)
    {
        return;
    }
    object_squad_spawn_node* squad_node;
    object_squad* squad;
    if (!orders_find_squad(handle, target, GFM_ORDERS_TYPE_PATROL, squad_node, squad))
    {
        return;
    }
    gfm_orders_creation_params params;
    params.ofp.reset();
    params.reset();
    params.location_target = target->pos;
    params.object_target = target->get_handle();
    params.orders_type = GFM_ORDERS_TYPE_PATROL;
    params.team = squad->get_squad_team();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    const unsigned int orders = Orders_manager.create_new_orders(params);
    if (orders != 0xFFFFFFFF)
    {
        squad->reassign_orders(orders, nullptr, false);
        if (squad_node != nullptr)
        {
            cont_support_update_orders(squad_node, GFM_ORDERS_TYPE_PATROL, target->get_handle(), orders);
        }
    }
}

// 0x00B2A020
void Action_callback_continuous_support_update(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    const unsigned int region = (unsigned int)*args[1];
    const float min_distance = (float)*args[2];
    for (int i = 0; list[i] != 0xFFFFFFFF; i++)
    {
        const object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(list[i]);
        if (node == nullptr)
        {
            return;
        }
        cont_support_register_region(node, region, min_distance);
    }
}

// 0x00B29FB0
void Action_callback_continuous_support_start(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    // Magic number: the script gives seconds.
    const int delay_ms = (int)*args[1] * 1000;
    for (int i = 0; list[i] != 0xFFFFFFFF; i++)
    {
        const object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(list[i]);
        if (node == nullptr)
        {
            return;
        }
        cont_support_add_squad(node, delay_ms < 0 ? 0 : delay_ms);
    }
}

// 0x00B2A370
void Action_callback_play_animation(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const animation_action animation = (animation_action)(game_handle)*args[1];
    const bool to_player = (bool)*args[2];
    if (!object_is_player(handle))
    {
        ai_scripting_play_animation_on_character(handle, animation, to_player);
        return;
    }
    // Sic: on the player rfg.exe plays this one fixed action whatever the script names (the argument is dropped), and blocks
    // movement on it. Magic numbers: action 0x369, play flag 0x40.
    object* objp = World.object_from_handle(handle);
    player* pp = objp != nullptr && objp->get_obj_type() == OBJ_TYPE_HUMAN && objp->get_sub_type() == HUMAN_SUBTYPE_PLAYER ? (player*)objp : nullptr;
    vanim_play_action(pp->char_inst->vp, index_alias(0x369, (vanim_aliases)-1), 0x40, 1.0f, 1.0f);
    pp->block_steering_vector = pp->orient.fvec;
    pp->block_movement_anim = 0x369;
    pp->block_movement_type = (block_movement_types)2; // the value rfg.exe stores
    pp->block_steering_heading_offset = 0.0f;
    pp->released_movement_anim = 0;
}

// 0x00B2A460
void Action_callback_turn_and_play_animation(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const int special_value = (int)(game_handle)*args[1];
    object* target = World.object_from_handle((unsigned int)*args[2]);
    if (target == nullptr || handle == 0xFFFFFFFF)
    {
        return;
    }
    npc* np = npc_from_handle(handle);
    if (np != nullptr)
    {
        np->npc_flags.scripted_move = 0;
        ai_scripting_trigger_behavior_on_character(np, AI_SB_TURN_ANIMATE, target->get_handle(), special_value, -1);
        np->get_ai()->planner.request_replan();
        return;
    }
    object_squad* squad = object_of_type<object_squad>(handle, OBJ_TYPE_SQUAD);
    if (squad != nullptr)
    {
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            np = npc_from_handle(squad->member_array[i]);
            if (np != nullptr)
            {
                np->npc_flags.scripted_move = 0;
                ai_scripting_trigger_behavior_on_character(np, AI_SB_TURN_ANIMATE, target->get_handle(), special_value, -1);
                np->get_ai()->planner.request_replan();
            }
        }
    }
}

// 0x00B2AB10
void Action_callback_patrol_pause(script_node** args)
{
    object_patrol* patrol = object_of_type<object_patrol>((unsigned int)*args[0], OBJ_TYPE_PATROL);
    if (patrol != nullptr)
    {
        patrol->set_pause_flag(true);
    }
}

// 0x00B2AB40
void Action_callback_patrol_unpause(script_node** args)
{
    object_patrol* patrol = object_of_type<object_patrol>((unsigned int)*args[0], OBJ_TYPE_PATROL);
    if (patrol != nullptr)
    {
        patrol->set_pause_flag(false);
    }
}

// 0x00B2AB70
void Action_callback_patrol_set_override(script_node** args)
{
    object_patrol* patrol = object_of_type<object_patrol>((unsigned int)*args[0], OBJ_TYPE_PATROL);
    if (patrol != nullptr)
    {
        patrol->set_override_flag(true);
    }
}

// 0x00B2ABA0
void Action_callback_patrol_clear_override(script_node** args)
{
    object_patrol* patrol = object_of_type<object_patrol>((unsigned int)*args[0], OBJ_TYPE_PATROL);
    if (patrol != nullptr)
    {
        patrol->set_override_flag(false);
    }
}

// 0x00B2BA70
void Action_callback_action_node_set_disabled(script_node** args)
{
    const bool disabled = (bool)*args[0];
    unsigned int* list = (unsigned int*)*args[1];
    for (int i = 0; list[i] != 0xFFFFFFFF; i++)
    {
        object_action_node* node = object_of_type<object_action_node>(list[i], OBJ_TYPE_ACTION_NODE);
        if (node != nullptr)
        {
            if (disabled)
            {
                node->set_disabled_by(ANDR_SCRIPTING);
            }
            else
            {
                node->clear_disabled_by(ANDR_SCRIPTING);
            }
        }
    }
}

// 0x00B2B140
void Action_callback_human_look_at_start(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int target_handle = (unsigned int)*args[1];
    const bool value = (bool)*args[2];
    object* target = World.object_from_handle(target_handle);
    if (target == nullptr || handle == 0xFFFFFFFF)
    {
        return;
    }
    npc* np = npc_from_handle(handle);
    if (np != nullptr)
    {
        np->npc_flags.scripted_move = 0;
        ai_scripting_trigger_behavior_on_character(np, AI_SB_LOOK_AT_OBJECT, target->get_handle(), value ? 1 : 0, -1);
        np->get_ai()->planner.request_replan();
        return;
    }
    object_squad* squad = object_of_type<object_squad>(handle, OBJ_TYPE_SQUAD);
    if (squad != nullptr)
    {
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            np = npc_from_handle(squad->member_array[i]);
            if (np != nullptr)
            {
                np->npc_flags.scripted_move = 0;
                ai_scripting_trigger_behavior_on_character(np, AI_SB_LOOK_AT_OBJECT, target->get_handle(), value ? 1 : 0, -1);
                np->get_ai()->planner.request_replan();
            }
        }
    }
}

// 0x00B2B270
void Action_callback_human_look_at_stop(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    npc* np = npc_from_handle(handle);
    if (np != nullptr)
    {
        if (np->get_ai()->custom_behavior == AI_SB_LOOK_AT_OBJECT)
        {
            np->get_ai()->end_custom_behavior();
            np->get_ai()->planner.request_replan();
        }
        return;
    }
    object_squad* squad = object_of_type<object_squad>(handle, OBJ_TYPE_SQUAD);
    if (squad != nullptr)
    {
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            np = npc_from_handle(squad->member_array[i]);
            if (np != nullptr && np->get_ai()->custom_behavior == AI_SB_LOOK_AT_OBJECT)
            {
                np->get_ai()->end_custom_behavior();
                np->get_ai()->planner.request_replan();
            }
        }
    }
}

static void npc_ignore_player(npc* np, bool ignore, unsigned int player_handle)
{
    if (ignore && np->get_ai() != nullptr)
    {
        // Magic number: the belief types up to and including the enemy ones (AI_BELIEF_ENEMY_EXISTS to 26).
        for (int type = AI_BELIEF_ENEMY_EXISTS; type < 27; type++)
        {
            np->get_ai()->perception.deprecate_belief((ai_belief_types)type, player_handle);
        }
    }
    np->npc_flags.ignore_player = ignore;
}

// 0x00B2BD70
void Action_callback_ai_ignore_player(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool ignore = (bool)*args[1];
    const bool leader_only = (bool)*args[2];
    object* objp = World.object_from_handle(handle);
    player* pp = player_get_local_player();
    if (objp == nullptr || pp == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OBJ_TYPE_HUMAN)
    {
        npc* np = npc_from_handle(handle);
        if (np != nullptr && np->get_ai() != nullptr)
        {
            npc_ignore_player(np, ignore, pp->get_handle());
        }
    }
    else if (objp->get_obj_type() == OBJ_TYPE_SQUAD)
    {
        object_squad* squad = (object_squad*)objp;
        if (leader_only)
        {
            squad->npc_flags.squad_leader_ignore_player = ignore;
            npc* leader = squad->get_leader();
            if (leader != nullptr)
            {
                npc_ignore_player(leader, ignore, pp->get_handle());
            }
        }
        else
        {
            squad->npc_flags.ignore_player = ignore;
            for (int i = 0; i < squad->member_array.size(); i++)
            {
                npc* np = World.object_from_handle<npc>(squad->member_array[i]);
                // Sic: a member without an AI keeps its flag when ignoring is switched on.
                if (np != nullptr && (!ignore || np->get_ai() != nullptr))
                {
                    npc_ignore_player(np, ignore, pp->get_handle());
                }
            }
        }
    }
}

// 0x00B2BF70
void Action_callback_ai_ignore_npc(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool ignore = (bool)*args[1];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    npc* np = npc_from_handle(handle);
    if (np != nullptr)
    {
        np->hflags.ai_ignore = ignore;
        return;
    }
    object_squad* squad = object_of_type<object_squad>(handle, OBJ_TYPE_SQUAD);
    if (squad != nullptr)
    {
        // The flags the squad gives its members (human_flags at 0xE4).
        squad->hflags.ai_ignore = ignore;
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            np = npc_from_handle(squad->member_array[i]);
            if (np != nullptr)
            {
                np->hflags.ai_ignore = ignore;
            }
        }
    }
}

// 0x00B31FF0
void Action_callback_attack_buildings(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[1];
    const int num_orders = (int)*args[0];
    if (num_orders <= 0 || list[0] == 0xFFFFFFFF)
    {
        return;
    }
    farray<object const*, 16> buildings;
    // Magic number: 16 buildings at most.
    for (int i = 0; i <= 0xF && list[i] != 0xFFFFFFFF; i++)
    {
        const object* objp = World.object_from_handle(list[i]);
        if (objp != nullptr)
        {
            buildings.add(objp);
        }
    }
    if (buildings.size() > 0)
    {
        for (int i = 0; i < num_orders; i++)
        {
            Smash_building_coordinator.add_new_orders(i, buildings);
        }
    }
}

// 0x00B325E0
void Action_callback_force_npc_use_action_node(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int node_handle = (unsigned int)*args[1];
    npc* np = npc_from_handle(handle);
    object_action_node* node = object_of_type<object_action_node>(node_handle, OBJ_TYPE_ACTION_NODE);
    if (np == nullptr || node == nullptr || np->is_killed() || node->disabled_by != 0)
    {
        return;
    }
    node->cooldown_period.set(0);
    farray<unsigned int,4> node_set;
    ai_system* ai = np->get_ai();
    ai->planner.request_replan();
    if (action_node_build_set(np->get_handle(), node->get_handle(), node_set) && action_node_reserve_node(np->get_handle(), node_set))
    {
        node->get_entry_rotation(np, ai->desired_orient);
        np->scripted_action_node_handle = node_handle;
    }
}

// 0x00B383F0
void Action_callback_complete_mission_objective(script_node** args)
{
    const int handle = (int)(game_handle)*args[0];
    GAME_TRACE("objective", "complete handle %d", handle);
    complete_objective(handle, true);
    mission_objective_complete_event data;
    data.handle = handle;
    gem_post_event(data);
}

// 0x00B38470
void Action_callback_post_event(script_node** args)
{
    user_defined_event data;
    data.desc = (const char*)*args[0];
    gem_post_event(data);
}

// 0x00B22AF0 (:9009)
void Action_callback_mission_save_checkpoint(script_node**)
{
    if (mission_is_active())
    {
        mission_checkpoint_save();
    }
}

// 0x00B384A0
void Action_callback_mission_success(script_node**)
{
    if (mission_is_active())
    {
        player* pp = player_get_local_player();
        if (pp != nullptr && !pp->is_killed())
        {
            mission_success_event data;
            gem_post_event(data);
        }
    }
}

// 0x00B38510
void Action_callback_mission_set_failed(script_node** args)
{
    if (mission_is_active())
    {
        mission_failure_event data;
        data.desc = (const char*)*args[0];
        gem_post_event(data);
    }
}

// 0x00B38550
void Action_callback_spawn_timer(script_node** args)
{
    // Magic number: seconds to milliseconds.
    Action_return_value.set(script_timer_create((int)((float)*args[0] * 1000.0f)));
}

// 0x00B38600
void Action_callback_add_dialogue_line_on_human(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    human* hp = object_of_type<human>((unsigned int)*args[1], OBJ_TYPE_HUMAN);
    const voice_line_handle parent_line = (voice_line_handle)(game_handle)*args[2];
    // Magic number: seconds to milliseconds.
    const int time_offset = (int)((float)*args[3] * 1000.0f);
    if (hp != nullptr && !hp->is_killed())
    {
        dialogue_manager_add_line(line, hp, parent_line, time_offset);
    }
}

// 0x00B38690
void Action_callback_add_dialogue_line_on_player(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    player* pp = player_get_local_player();
    const voice_line_handle parent_line = (voice_line_handle)(game_handle)*args[1];
    // Magic number: seconds to milliseconds.
    const int time_offset = (int)((float)*args[2] * 1000.0f);
    if (pp != nullptr && !pp->is_killed())
    {
        dialogue_manager_add_line(line, pp, parent_line, time_offset);
    }
}

// 0x00B3CEE0 (:3097)
// Args: a list of handles ending in 0xFFFFFFFF, and the purpose.
void Action_callback_objective_highlight_add_list(script_node** args)
{
    const highlight_purpose purpose = (highlight_purpose)(int)*args[1];
    for (const unsigned int* handles = (unsigned int*)*args[0]; *handles != 0xFFFFFFFF; handles++)
    {
        if (World.object_from_handle(*handles) != nullptr)
        {
            objective_highlight_add(*handles, purpose, HIGHLIGHT_STYLE_DEFAULT);
        }
    }
}

// 0x00B3CFE0 (:3112)
void Action_callback_objective_highlight_remove_list(script_node** args)
{
    for (const unsigned int* handles = (unsigned int*)*args[0]; *handles != 0xFFFFFFFF; handles++)
    {
        objective_highlight_remove(*handles);
    }
}

// 0x00B3D060 (:3120)
// Args: the purpose, then any number of objects.
void Action_callback_objective_highlight_add_func(script_node** args)
{
    const highlight_purpose purpose = (highlight_purpose)(int)*args[0];
    for (int i = 1; args[i] != nullptr; i++)
    {
        const unsigned int handle = (unsigned int)*args[i];
        if (World.object_from_handle(handle) != nullptr)
        {
            objective_highlight_add(handle, purpose, HIGHLIGHT_STYLE_DEFAULT);
        }
    }
}

// 0x00B3D170 (:3139)
// Handle 0, which the list form cannot hold, is skipped.
void Action_callback_objective_highlight_remove_func(script_node** args)
{
    for (int i = 0; args[i] != nullptr; i++)
    {
        const unsigned int handle = (unsigned int)*args[i];
        if (handle != 0)
        {
            objective_highlight_remove(handle);
        }
    }
}

// 0x00B3D200
void Action_callback_remove_timer(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle != 0xFFFFFFFF)
    {
        if (args[0]->type == SN_VARIABLE_REF)
        {
            script_variable_clear(args[0]);
        }
        script_timer_delete(handle);
    }
}

// 0x00B3D270
void Action_callback_set_timer(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle != 0xFFFFFFFF)
    {
        // Magic number: seconds to milliseconds.
        script_timer_set(handle, (int)((float)*args[1] * 1000.0f));
    }
}

// 0x00B3D310
void Action_callback_pause_timer(script_node** args)
{
    script_timer_pause((unsigned int)*args[0]);
}

// 0x00B3D360
void Action_callback_unpause_timer(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle != 0xFFFFFFFF)
    {
        script_timer_unpause(handle);
    }
}

// 0x00B3D380
void Action_callback_play_voice_line_on_human(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    human* hp = object_of_type<human>((unsigned int)*args[1], OBJ_TYPE_HUMAN);
    const dialogue_priorities priority = dialogue_priority_from_number((float)*args[2]);
    if (hp != nullptr && !hp->is_killed())
    {
        dialogue_manager_start_dialogue(line, hp, priority);
        // Magic number: 45 s without another target to focus on.
        hp->spinebend_data.focus_target_ts.set(45000);
    }
}

// 0x00B3D420
void Action_callback_play_voice_line_on_player(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    player* pp = player_get_local_player();
    const dialogue_priorities priority = dialogue_priority_from_number((float)*args[1]);
    if (pp != nullptr && !pp->is_killed())
    {
        dialogue_manager_start_dialogue(line, pp, priority);
        // Magic number: 45 s without another target to focus on.
        pp->spinebend_data.focus_target_ts.set(45000);
    }
}

// 0x00B3D4A0
void Action_callback_play_voice_line_on_persona(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    human_voice_persona* persona = human_voice_persona_from_handle((human_voice_persona_handle)(game_handle)*args[1]);
    dialogue_manager_start_dialogue(line, persona, dialogue_priority_from_number((float)*args[2]), INVALID_PA_HANDLE, nullptr);
}

// 0x00B3D5D0
void Action_callback_stop_all_dialogue(script_node**)
{
    dialogue_manager_cancel_all();
    ui_kill_subtitle();
}

// ---- gameflow.md GF3: districts, safehouses, progression

// 0x00B204E0
void Function_callback_player_has_radiation_upgrade(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = upgrade_has_radiation_shield() ? 1.0f : 0.0f;
}

// 0x00B20590
void Function_callback_district_control(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = 0.0f;
    const district* dp = district_local_player();
    if (dp != nullptr)
    {
        dest_result.number = dp->get_control();
    }
}

// 0x00B26BF0
void Function_callback_district_specific_control(script_variant& dest_result, script_variant&, script_node** args)
{
    const char* district_name = (const char*)*args[0];
    dest_result.number = 0.0f;
    const unsigned int handle = get_district_by_name(district_name);
    if (handle != 0xFFFFFFFF)
    {
        const district* dp = World.object_from_handle<district>(handle);
        if (dp != nullptr)
        {
            dest_result.number = dp->get_control();
        }
    }
}

// 0x00B205B0
void Function_callback_air_bomb_running(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    if (args[0] != nullptr)
    {
        dest_result.number = air_bomb_is_running_by_index((int)(game_handle)*args[0]) ? 1.0f : 0.0f;
    }
}

// 0x00B20970
event_result Event_callback_salvage_created(const salvage_created_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B21300
event_result Event_callback_player_gets_num_salvage(const player_gets_salvage_event* data, script_node** args)
{
    return (event_result)((int)*args[0] <= data->salvage_val);
}

// 0x00B381B0
// rfg.exe inlines public_address_add_speaker. The list of handles ends with -1.
void Action_callback_public_address_add_speakers(script_node** args)
{
    const public_address_group_handle group = (public_address_group_handle)(game_handle)*args[0];
    for (const unsigned int* handles = (unsigned int*)*args[1]; *handles != 0xFFFFFFFF; handles++)
    {
        public_address_add_speaker(group, *handles);
    }
}

// 0x00B38250
// rfg.exe inlines public_address_remove_speaker.
void Action_callback_public_address_remove_speakers(script_node** args)
{
    for (const unsigned int* handles = (unsigned int*)*args[0]; *handles != 0xFFFFFFFF; handles++)
    {
        public_address_remove_speaker(*handles);
    }
}

// 0x00B404E0
// rfg.exe inlines public_address_override_ambient. Magic numbers: the delays are in seconds in the script.
void Action_callback_public_address_override_ambient(script_node** args)
{
    const public_address_group_handle group = (public_address_group_handle)(game_handle)*args[0];
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[1];
    const human_voice_persona_handle persona = (human_voice_persona_handle)(game_handle)*args[2];
    const int min_delay_ms = (int)((float)*args[3] * 1000.0f);
    const int max_delay_ms = (int)((float)*args[4] * 1000.0f);
    public_address_override_ambient(group, line, persona, min_delay_ms, max_delay_ms);
}

// 0x00B27A60
void Action_callback_public_address_stop_ambient(script_node** args)
{
    public_address_stop_ambient((public_address_group_handle)(game_handle)*args[0]);
}

// 0x00B27A90
void Action_callback_public_address_restore_ambient(script_node** args)
{
    public_address_restore_ambient((public_address_group_handle)(game_handle)*args[0]);
}

// 0x00B38350
void Action_callback_kiosk_unlock_line(script_node** args)
{
    kiosk_unlock_from_voice_line_handle((voice_line_handle)(game_handle)*args[0]);
}

// 0x00B27EA0
// Argument order from the asm: the origin is read first, then the target, then the definition.
void Action_callback_air_bomb_start_stationary(script_node** args)
{
    const unsigned int origin_handle = (unsigned int)*args[2];
    const unsigned int target_handle = (unsigned int)*args[1];
    const air_bomb_info* info = air_bomb_definition_from_index((int)(game_handle)*args[0]);
    Action_return_value.set((unsigned int)air_bomb_start(info, target_handle, origin_handle, false));
}

// 0x00B27EF0
void Action_callback_air_bomb_start_following(script_node** args)
{
    const unsigned int origin_handle = (unsigned int)*args[2];
    const unsigned int target_handle = (unsigned int)*args[1];
    const air_bomb_info* info = air_bomb_definition_from_index((int)(game_handle)*args[0]);
    Action_return_value.set((unsigned int)air_bomb_start(info, target_handle, origin_handle, true));
}

// 0x00B21AF0
void Action_callback_air_bomb_stop(script_node** args)
{
    air_bomb_stop((air_bomb_handle)(unsigned int)*args[0]);
}

// 0x00B21B10
void Action_callback_air_bomb_stop_type(script_node** args)
{
    air_bomb_stop_type(air_bomb_definition_from_index((int)(game_handle)*args[0]));
}

// 0x00B21B30
void Action_callback_air_bomb_stop_all(script_node**)
{
    air_bomb_stop_all();
}

// 0x00B21B40
void Action_callback_air_bomb_avoid_list_add(script_node** args)
{
    const int time = (int)*args[1];
    air_bomb_avoid_list_add((unsigned int)*args[0], time);
}

// 0x00B21B70
void Action_callback_air_bomb_avoid_list_remove(script_node** args)
{
    air_bomb_avoid_list_remove((unsigned int)*args[0]);
}

// 0x00B21B90
void Action_callback_air_bomb_avoid_list_clear(script_node**)
{
    air_bomb_avoid_list_clear();
}

// 0x00B21BA0
void Action_callback_air_bomb_attack_list_add(script_node** args)
{
    const int time = (int)*args[1];
    air_bomb_attack_list_add((unsigned int)*args[0], time);
}

// 0x00B21BD0
void Action_callback_air_bomb_attack_list_remove(script_node** args)
{
    air_bomb_attack_list_remove((unsigned int)*args[0]);
}

// 0x00B21BF0
void Action_callback_air_bomb_attack_list_clear(script_node**)
{
    air_bomb_attack_list_clear();
}

// 0x00B21C00
void Action_callback_air_bomb_destroy_target(script_node** args)
{
    air_bomb_destroy_target((unsigned int)*args[0], 0);
}

// 0x00B21C20
void Action_callback_air_bomb_kill_player(script_node**)
{
    air_bomb_kill_player();
}

// 0x00B21C30
void Action_callback_air_bomb_hit_target(script_node** args)
{
    air_bomb_hit_target((unsigned int)*args[0]);
}

// 0x00B21E30
void Action_callback_upgrade_unlock_level(script_node** args)
{
    const unsigned int upgrade_id = (unsigned int)(game_handle)*args[0];
    const unsigned int level = (unsigned int)(int)*args[1];
    player_meta_data* meta = player_get_meta_data();
    if (meta != nullptr)
    {
        meta->unlock_upgrade(upgrade_id, level, false);
    }
}

// The safehouse actions take a list of handles ending with -1.
template<typename F>
static void for_each_safehouse(script_node** args, F function)
{
    const unsigned int* handles = (unsigned int*)*args[0];
    for (; *handles != 0xFFFFFFFF; handles++)
    {
        object_safehouse* osp = World.object_from_handle<object_safehouse>(*handles);
        if (osp != nullptr)
        {
            function(osp);
        }
    }
}

// 0x00B282B0
void Action_callback_safehouse_enable(script_node** args)
{
    for_each_safehouse(args, [](object_safehouse* osp) { osp->clear_disabled(SDR_NORMAL); });
}

// 0x00B28300
void Action_callback_safehouse_disable(script_node** args)
{
    for_each_safehouse(args, [](object_safehouse* osp) { osp->set_disabled(SDR_NORMAL); });
}

// 0x00B28350
void Action_callback_safehouse_activity_enable(script_node** args)
{
    for_each_safehouse(args, [](object_safehouse* osp) { osp->clear_disabled(SDR_ACTIVITY); });
}

// 0x00B283A0
void Action_callback_safehouse_activity_disable(script_node** args)
{
    for_each_safehouse(args, [](object_safehouse* osp) { osp->set_disabled(SDR_ACTIVITY); });
}

// 0x00B283F0
void Action_callback_safehouse_disable_until_stream(script_node** args)
{
    for_each_safehouse(args, [](object_safehouse* osp) { osp->set_disabled(SDR_TEMP_DISABLE_UNTIL_STREAM); });
}

// 0x00B284B0
void Action_callback_safehouse_despawn_squad(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const char* name = (const char*)*args[1];
    if (handle != 0xFFFFFFFF)
    {
        object_safehouse* osp = World.object_from_handle<object_safehouse>(handle);
        if (osp != nullptr)
        {
            osp->despawn_squad(name);
        }
    }
}

// 0x00B22470
void Action_callback_disable_safehouse_vip(script_node** args)
{
    const game_handle id = (game_handle)*args[0];
    const bool disable_vip = (bool)*args[1];
    const human_info* hinfo = human_info_find(id);
    if (hinfo != nullptr)
    {
        safehouse_vip_disable(hinfo, disable_vip);
    }
}

// 0x00B28E30
// The spawned lookout stays where it is put, is not saved and goes when its zone streams out.
void Action_callback_spawn_marauder_lookout(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    const unsigned int node_handle = (unsigned int)*args[0];
    const unsigned int navpoint_handle = (unsigned int)*args[1];
    const int special_value = (int)*args[2];
    if (navpoint_handle == 0xFFFFFFFF)
    {
        return;
    }
    const navpoint* nav = World.object_from_handle<navpoint>(navpoint_handle);
    if (nav == nullptr || special_value <= 0 || node_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_npc_spawn_node* node = World.object_from_handle<object_npc_spawn_node>(node_handle);
    if (node == nullptr)
    {
        return;
    }
    const unsigned int spawned = node->spawn_child_object();
    if (spawned == 0xFFFFFFFF)
    {
        return;
    }
    npc* np = World.object_from_handle<npc>(spawned);
    if (np == nullptr)
    {
        return;
    }
    ai_scripting_trigger_behavior_on_character(np, AI_SB_MARAUDER_LOOKOUT, nav->get_handle(), special_value, -1);
    np->hflags.invulnerable = 1;
    np->hflags.disable_all_path_smoothing = 1;
    np->npc_flags.high_priority_despawn = 1;
    np->obj_flags.no_save = 1;
    np->obj_flags.destroy_on_stream = 1;
    Action_return_value.set(spawned);
}

// 0x00B32200
void Action_callback_squad_marauder_despawn_all(script_node**)
{
    const int count = World.get_num_objects<object_squad>();
    for (int i = 0; i < count; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->get_squad_team() == HUMAN_TEAM_MARAUDER)
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, true);
        }
    }
}

// 0x00B22800: empty in rfg.exe.
void Action_callback_set_marauder_weapon_prob(script_node**)
{
}

// ---- gameflow.md GF6: the rest of the callbacks
//
// Registration order does not follow this file: these sit together, ordered by address, so that the GF6 port can be read
// beside rfg.exe. Calls where rfg.exe inlines a function are commented once, at the helper.

static const gfm_orders_flags FODDER_ORDERS_FLAGS(false, true); // 0x02231B23, the constant object_spawn_node.cpp also holds

// Calls fn for each member of the squad that is an npc. rfg.exe inlines this walk in every squad callback.
template<typename F>
static void for_each_squad_npc(object_squad* squad, F fn)
{
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = npc_from_handle(squad->member_array[i]);
        if (np != nullptr)
        {
            fn(np);
        }
    }
}

// The end of the handle lists scripts pass.
static const unsigned int HANDLE_LIST_END = 0xFFFFFFFF;

// ---- GF6 functions

// 0x00B1FFB0
void Function_callback_less(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    dest_result.number = first < (float)*args[1] ? SCRIPT_TRUE : 0.0f;
}

// 0x00B20010
void Function_callback_less_or_equal(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    dest_result.number = first <= (float)*args[1] ? SCRIPT_TRUE : 0.0f;
}

// 0x00B20070
void Function_callback_greater(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    dest_result.number = first > (float)*args[1] ? SCRIPT_TRUE : 0.0f;
}

// 0x00B200D0
void Function_callback_greater_or_equal(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    dest_result.number = first >= (float)*args[1] ? SCRIPT_TRUE : 0.0f;
}

// 0x00B201B0
void Function_callback_add(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    for (int i = 0; args[i] != nullptr; i++)
    {
        dest_result.number += (float)*args[i];
    }
}

// 0x00B201F0
void Function_callback_subtract(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    dest_result.number = first - (float)*args[1];
}

// 0x00B20230
void Function_callback_multiply(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 1.0f;
    for (int i = 0; args[i] != nullptr; i++)
    {
        dest_result.number *= (float)*args[i];
    }
}

// 0x00B20270: no check for a zero divisor.
void Function_callback_divide(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const float first = (float)*args[0];
    dest_result.number = first / (float)*args[1];
}

// 0x00B202B0
void Function_callback_random(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const int minimum = (int)*args[0];
    dest_result.number = (float)int_rand_range(minimum, (int)*args[1]);
}

// 0x00B20330
void Function_callback_player_in_turret(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const unsigned int handle = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        const turret* tp = human_combat_get_manned_turret(pp);
        if (tp != nullptr && tp->get_handle() == handle)
        {
            dest_result.number = SCRIPT_TRUE;
        }
    }
}

// 0x00B20380
void Function_callback_player_in_any_turret(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = 0.0f;
    player* pp = player_get_local_player();
    if (pp != nullptr && human_combat_get_manned_turret(pp) != nullptr)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B203B0: a zone's handle holds its grid position, x in the top 4 bits and z in the 4 below.
void Function_callback_player_in_zone(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const obj_zone* zone = World.get_zone_by_pos(pp->pos);
    if (zone != nullptr)
    {
        const unsigned int grid_x = (unsigned int)(int)*args[0];
        const unsigned int grid_z = (unsigned int)(int)*args[1];
        const unsigned int position = zone->get_handle() >> 16;
        if (position >> 12 == grid_x && ((position >> 8) & 0xF) == grid_z)
        {
            dest_result.number = SCRIPT_TRUE;
        }
    }
}

// 0x00B20510
void Function_callback_player_has_purchased_nano_forge(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = upgrade_has_nano_rifle() ? SCRIPT_TRUE : 0.0f;
}

// 0x00B20540
void Function_callback_player_completed_GoT(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = upgrade_completed_guns_of_tharsis() ? SCRIPT_TRUE : 0.0f;
}

// 0x00B20570
void Function_callback_num_missions_complete(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = (float)mission_get_num_completed_missions();
}

// 0x00B20630
void Function_callback_activity_is_active(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = !activity_none() ? SCRIPT_TRUE : 0.0f;
}

// 0x00B20680
void Function_callback_get_player_vehicle_handle(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.obj = 0xFFFFFFFF;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        dest_result.obj = pp->vehicle_handle;
    }
}

// 0x00B206A0
void Function_callback_get_active_courier_squad_handle(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.obj = activity_cc_get_courier_squad_handle();
}

// 0x00B206C0
void Function_callback_get_vehicle_from_vrail(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.obj = 0xFFFFFFFF;
    vrail_get_patrol_vehicle(dest_result.obj);
}

// 0x00B206E0
void Function_callback_vrail_get_patrol_squad(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.obj = 0xFFFFFFFF;
    vrail_get_patrol_squad(dest_result.obj);
}

// 0x00B20700: also true for a dead player.
void Function_callback_player_is_damaged(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = 0.0f;
    player* pp = player_get_local_player();
    if (pp != nullptr && (pp->is_killed() || pp->hit_points / (float)pp->max_hit_points != 1.0f))
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B20760: leaves the result as it was when a team is out of range or the two are the same.
void Function_callback_get_team_dispositions(script_variant& dest_result, script_variant&, script_node** args)
{
    const int team1 = (int)*args[0];
    if (team1 >= NUM_HUMAN_TEAMS)
    {
        return;
    }
    const int team2 = (int)*args[1];
    if (team2 < NUM_HUMAN_TEAMS && team1 != team2)
    {
        dest_result.number = (float)(int)human_teams_get_disposition((human_teams)team1, (human_teams)team2);
    }
}

// 0x00B267D0
void Function_callback_get_vehicle_hitpoints(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const vehicle* vp = object_of_type<vehicle>((unsigned int)*args[0], OT_VEHICLE);
    if (vp != nullptr)
    {
        dest_result.number = (float)vehicle_get_hit_points(vp);
    }
}

// The vehicle the local player is in, or null. rfg.exe inlines it in the three player_in_*car callbacks.
static vehicle* script_player_vehicle(player* pp)
{
    vehicle* vp = object_of_type<vehicle>(pp->vehicle_handle, OT_VEHICLE);
    return vp != nullptr && human_in_vehicle(pp, vp) ? vp : nullptr;
}

// 0x00B26830
void Function_callback_player_in_car(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const int type_index = (int)(game_handle)*args[0];
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const vehicle* vp = script_player_vehicle(pp);
    if (vp != nullptr && strcmp(vp->info->name, vehicle_get_name(type_index)) == 0)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B268E0
void Function_callback_player_in_specific_car(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const unsigned int handle = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const vehicle* vp = script_player_vehicle(pp);
    if (vp != nullptr && vp->get_handle() == handle)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B26980
void Function_callback_player_in_any_car(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = 0.0f;
    player* pp = player_get_local_player();
    if (pp != nullptr && script_player_vehicle(pp) != nullptr)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B26AF0
void Function_callback_player_in_restricted_area(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const unsigned int handle = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const object_restricted_area* area = object_of_type<object_restricted_area>(handle, OT_RESTRICTED_AREA);
    if (area != nullptr && !area->is_disabled() && area->is_inside(pp->pos))
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B26B60
void Function_callback_player_in_npc_los(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    npc* np = npc_from_handle((unsigned int)*args[0]);
    if (np == nullptr || np->get_ai() == nullptr)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        vector head_pos;
        human_get_head_pos(pp, head_pos);
        if (np->get_ai()->perception.has_los_to_position(head_pos, false))
        {
            dest_result.number = SCRIPT_TRUE;
        }
    }
}

// 0x00B26C40
void Function_callback_player_squad_size(script_variant& dest_result, script_variant&, script_node**)
{
    dest_result.number = 0.0f;
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        const object_squad* squad = object_of_type<object_squad>(pp->ally_squad_handle, OT_SQUAD);
        if (squad != nullptr)
        {
            dest_result.number = (float)squad->member_array.size();
        }
    }
}

// 0x00B26C90: only the named characters of Vip_character_names, filled by script_cache_vip_npcs.
void Function_callback_get_npc_by_name(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    const unsigned int crc = crc_stri((const char*)*args[0], 0);
    for (int i = 0; i < Vip_npc_lookup_table.size(); i++)
    {
        if (Vip_npc_lookup_table[i].npc_name_crc == crc)
        {
            dest_result.obj = Vip_npc_lookup_table[i].npc_handle;
            return;
        }
    }
}

// 0x00B26DB0
void Function_callback_get_vehicle_from_squad(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    const object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad != nullptr)
    {
        const vehicle* vp = squad->get_vehicle();
        if (vp != nullptr)
        {
            dest_result.obj = vp->get_handle();
        }
    }
}

// 0x00B26E80
void Function_callback_get_turret_from_spawn_object(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    const object_turret_spawn_node* node = object_of_type<object_turret_spawn_node>((unsigned int)*args[0], OT_TURRET_SPAWN_NODE);
    if (node != nullptr)
    {
        dest_result.obj = node->get_child_turret_handle();
    }
}

// 0x00B26F20
void Function_callback_npc_is_in_specific_vehicle(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    npc* np = npc_from_handle((unsigned int)*args[0]);
    const unsigned int vehicle_handle = (unsigned int)*args[1];
    if (np != nullptr && !np->is_killed() && np->vehicle_handle == vehicle_handle)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B26F90
void Function_callback_vehicle_is_usable(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const vehicle* vp = object_of_type<vehicle>((unsigned int)*args[0], OT_VEHICLE);
    if (vp != nullptr && !vehicle_is_flipped(vp) && ai_vehicle_is_usable(vp, nullptr, AI_VEHICLE_SEAT_TYPE_DRIVER))
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// Upright, not burning and alive. rfg.exe inlines it in the two functions below.
static bool script_vehicle_is_functional(vehicle* vp)
{
    return !vehicle_is_flipped(vp) && vp->m_flags.m_on_fire == 0 && !vp->is_killed();
}

// 0x00B27000
void Function_callback_vehicle_is_functional(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    vehicle* vp = object_of_type<vehicle>((unsigned int)*args[0], OT_VEHICLE);
    if (vp != nullptr && script_vehicle_is_functional(vp))
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B27070: a squad with no vehicle of its own counts as functional while it simulates.
void Function_callback_squad_vehicle_is_functional(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>((unsigned int)*args[0], OT_SQUAD_SPAWN_NODE);
    if (node == nullptr)
    {
        return;
    }
    vehicle* vp = object_of_type<vehicle>(node->get_child_vehicle_handle(), OT_VEHICLE);
    if (vp != nullptr)
    {
        if (script_vehicle_is_functional(vp))
        {
            dest_result.number = SCRIPT_TRUE;
        }
        return;
    }
    const object_squad* squad = object_of_type<object_squad>(node->get_child_squad_handle(), OT_SQUAD);
    if (squad != nullptr && squad->simulated())
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B27150
void Function_callback_get_driver_from_vehicle(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    const vehicle* vp = object_of_type<vehicle>((unsigned int)*args[0], OT_VEHICLE);
    if (vp != nullptr)
    {
        const human* driver = vehicle_passenger_in_seat(vp, VSI_DRIVER);
        if (driver != nullptr)
        {
            dest_result.obj = driver->get_handle();
        }
    }
}

// 0x00B271A0
void Function_callback_squad_exists(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD) != nullptr ? SCRIPT_TRUE : 0.0f;
}

// 0x00B271F0: how many of the squads, or of the squads of spawn nodes, are alive.
// Original quirk: a squad with spawn status OK asks simulated() and drops the answer, so it counts either way (the asm's
// counter is already raised on that path). The call, const and with no effect, is left out here.
void Function_callback_squads_are_alive(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    int alive = 0;
    for (int i = 0; args[i] != nullptr; i++)
    {
        unsigned int handle = (unsigned int)*args[i];
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        const object* objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_SQUAD_SPAWN_NODE)
        {
            handle = ((const object_squad_spawn_node*)objp)->get_child_squad_handle();
        }
        object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
        if (squad != nullptr && !squad->is_killed())
        {
            alive++;
        }
    }
    dest_result.number = (float)alive;
}

// 0x00B272A0
void Function_callback_human_is_assigned_custom_behavior(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    npc* np = npc_from_handle((unsigned int)*args[0]);
    // Original quirk kept: the ai pointer is not checked here, unlike the callbacks around it.
    if (np != nullptr && !np->is_killed() && np->get_ai()->custom_behavior != 0)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B273F0: true for any turret that exists.
void Function_callback_turret_is_functional(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    if (object_of_type<turret>((unsigned int)*args[0], OT_TURRET) != nullptr)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x01332740: min_y_val_tipped, the up vector's height below which a mover counts as tipped.
static const float MIN_Y_VAL_TIPPED = 0.71f;

// 0x00B27440: the threshold on speed is 1 m/s, compared squared.
void Function_callback_mover_is_moving_or_tipped(script_variant& dest_result, script_variant&, script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    dest_result.number = 0.0f;
    const object* mover = object_of_type<object>(handle, OT_MOVER);
    if (mover == nullptr)
    {
        return;
    }
    if (mover->orient.uvec.y < MIN_Y_VAL_TIPPED)
    {
        dest_result.number = SCRIPT_TRUE;
        return;
    }
    if (mover->havok_handle != 0xFFFFFFFF)
    {
        vector velocity;
        havok_body_get_linear_velocity(mover->havok_handle, velocity);
        // Summed at double precision and narrowed once (0x00B274BE).
        if ((float)((double)velocity.x * (double)velocity.x + (double)velocity.y * (double)velocity.y + (double)velocity.z * (double)velocity.z) > 1.0f)
        {
            dest_result.number = SCRIPT_TRUE;
        }
    }
}

// 0x00B27500
void Function_callback_human_is_being_rendered(script_variant& dest_result, script_variant&, script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    dest_result.number = 0.0f;
    const human* hp = object_of_type<human>(handle, OT_HUMAN);
    if (hp != nullptr && hp->hflags.was_rendered)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B31690
void Function_callback_get_vehicle_from_convoy(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    const object* patrol = object_of_type<object>((unsigned int)*args[0], OT_PATROL);
    const unsigned int index = (unsigned int)(int)*args[1];
    // Magic number: 12, the size of object_convoy::member_info.
    if (patrol == nullptr || index >= 12)
    {
        return;
    }
    const int count = World.get_num_objects<object_convoy>();
    for (int i = 0; i < count; i++)
    {
        const object_convoy* convoy = World.get_object<object_convoy>(i);
        if (patrol->get_handle() == convoy->get_patrol_handle())
        {
            const object_squad* squad = convoy->get_member_squad((int)index);
            if (squad != nullptr)
            {
                dest_result.obj = squad->squad_vehicle;
                return;
            }
        }
    }
}

// 0x00B31760
void Function_callback_get_nearest_npc_on_team(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.obj = 0xFFFFFFFF;
    const human_teams team = (human_teams)(game_handle)*args[0];
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    npc* nearest = nullptr;
    float nearest_dist = -1.0f;
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->current_team == team && !np->is_killed() && !human_is_ragdolled(np))
        {
            const float dist = vec_dist_squared(np->pos, pp->pos);
            if (nearest == nullptr || dist < nearest_dist)
            {
                nearest = np;
                nearest_dist = dist;
            }
        }
    }
    if (nearest != nullptr)
    {
        dest_result.obj = nearest->get_handle();
    }
}

// 0x00B380B0
void Function_callback_human_is_playing_voice_line(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    human* hp = object_of_type<human>((unsigned int)*args[0], OT_HUMAN);
    if (hp != nullptr && !hp->is_killed()
        && (hp->get_current_voice_line_handle() != INVALID_VOICE_LINE_HANDLE || dialogue_manager_is_any_line_queued(hp, true)))
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B38120: the second argument is in seconds. A timer that is gone counts as zero time left.
void Function_callback_timer_has_less_time_remaining(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const unsigned int handle = (unsigned int)*args[0];
    const float seconds = (float)*args[1];
    if (handle != 0xFFFFFFFF && (float)((double)(float)script_timer_get_time(handle) * 0.001) <= seconds)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// 0x00B3CD90: true when the first thing straight below the player, within 100 metres, is that mover.
void Function_callback_player_in_building(script_variant& dest_result, script_variant&, script_node** args)
{
    dest_result.number = 0.0f;
    const unsigned int handle = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    raycast_input in;
    raycast_output out;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.start = pp->pos;
    in.end = pp->pos;
    in.end.y -= 100.0f;
    in.m_filter_info = COLLIDE_MOVER;
    if (havok_raycast(in, out) && out.m_hits[0].hit_object_handle == handle)
    {
        dest_result.number = SCRIPT_TRUE;
    }
}

// ---- GF6 events

// 0x00B207E0: as Event_callback_num_buildings_destroyed, for damage.
event_result Event_callback_num_buildings_damaged(const building_damaged_event* data, script_node** args)
{
    if (activity_is_active(ACTIVITY_MODE_VEHICLE_RAIL))
    {
        return EVENT_IGNORED;
    }
    unsigned int* list = (unsigned int*)*args[0];
    int count = 0;
    while (list[count] != HANDLE_LIST_END)
    {
        count++;
    }
    if (count == 0)
    {
        return EVENT_IGNORED;
    }
    const int last = count - 1;
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        if (data->building == list[i])
        {
            list[i] = list[last];
            list[last] = HANDLE_LIST_END;
            return last != 0 ? EVENT_OK : EVENT_HANDLED;
        }
    }
    return EVENT_IGNORED;
}

// 0x00B208A0
event_result Event_callback_player_damaged_by_explosion_type(const player_damaged_by_explosion_event* data, script_node** args)
{
    return (game_handle)data->info_handle == (game_handle)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B209B0
event_result Event_callback_weapon_acquired(const weapon_acquired_event* data, script_node** args)
{
    const weapon_info* wanted = weapons_get_weapon_info((unsigned int)(game_handle)*args[0]);
    const weapon_info* acquired = weapons_get_weapon_info((unsigned int)data->w_handle);
    return strcmp(acquired->name, wanted->name) == 0 ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20A80
event_result Event_callback_item_acquired(const item_acquired_event* data, script_node** args)
{
    return data->itemp->get_handle() == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20AA0
event_result Event_callback_raid_status_change(const raid_state_change_event* data, script_node** args)
{
    const unsigned int raid_handle = (unsigned int)*args[0];
    const game_handle team = (game_handle)*args[1];
    const game_handle state = (game_handle)*args[2];
    return (game_handle)data->new_state == state && (game_handle)data->team == team
        && activity_raid_nodes_are_relatives(raid_handle, data->raid_handle) ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20B00
event_result Event_callback_ha_defender_change(const ha_state_change_event* data, script_node** args)
{
    const unsigned int node_handle = (unsigned int)*args[0];
    const int count = (int)(float)*args[1];
    return activity_house_arrest_nodes_are_relatives(data->ha_handle, node_handle) && data->defender_count < count ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20B50
event_result Event_callback_activity_status_change(const activity_status_event* data, script_node** args)
{
    const unsigned int node_handle = (unsigned int)*args[0];
    const game_handle state = (game_handle)*args[1];
    return node_handle == data->node_handle && state == (game_handle)data->activity_state ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20B90
event_result Event_callback_raid_complete(const raid_complete_event* data, script_node** args)
{
    const unsigned int raid_handle = (unsigned int)*args[0];
    const game_handle team = (game_handle)*args[1];
    return raid_handle == data->raid_handle && team == (game_handle)data->winning_team ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20BD0
event_result Event_callback_mission_vrail_completed(const mission_vrail_complete_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B20C00
event_result Event_callback_squad_member_killed(const squad_member_killed_event* data, script_node** args)
{
    return data->squad_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20C20
event_result Event_callback_turret_destroyed(const turret_destroyed_event* data, script_node** args)
{
    return data->turret_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20C40
event_result Event_callback_vehicle_destroyed(const vehicle_destroyed_event* data, script_node** args)
{
    return data->vehicle_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20E30
event_result Event_callback_object_enter_trigger_region(const object_enter_trigger_region_event* data, script_node** args)
{
    const unsigned int object_handle = (unsigned int)*args[0];
    const unsigned int trigger_handle = (unsigned int)*args[1];
    return data->trigger_handle == trigger_handle && data->object_handle == object_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20E70
event_result Event_callback_continuous_attack_num_squads_reached_end(const cont_attack_squad_reached_end_event* data, script_node** args)
{
    return (int)*args[0] == data->count ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20E90
event_result Event_callback_player_exit_trigger_region(const object_exit_trigger_region_event* data, script_node** args)
{
    const unsigned int trigger_handle = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    return pp != nullptr && data->trigger_handle == trigger_handle && data->object_handle == pp->get_handle() ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20ED0
event_result Event_callback_object_exit_trigger_region(const object_exit_trigger_region_event* data, script_node** args)
{
    const unsigned int object_handle = (unsigned int)*args[0];
    const unsigned int trigger_handle = (unsigned int)*args[1];
    return data->trigger_handle == trigger_handle && data->object_handle == object_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20F10
event_result Event_callback_player_kills_any_vehicle(const player_kills_vehicle_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B20F20
event_result Event_callback_player_kills_vehicle(const player_kills_vehicle_event* data, script_node** args)
{
    return data->handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20F60
event_result Event_callback_player_kills_vehicle_team(const player_kills_vehicle_event* data, script_node** args)
{
    return (game_handle)data->team == (game_handle)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20F80
event_result Event_callback_bridge_becomes_unusable(const bridge_becomes_unusable_event* data, script_node** args)
{
    return (unsigned int)*args[0] == data->bridge_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20FA0
event_result Event_callback_player_kills_vehicle_team_from_vehicle(const player_kills_vehicle_from_vehicle_event* data, script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const game_handle team = (game_handle)*args[1];
    return (game_handle)data->team == team && data->handle == handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B20FE0
event_result Event_callback_squad_members_despawned(const squad_members_despawned* data, script_node** args)
{
    return (unsigned int)*args[0] == data->squad_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21000
event_result Event_callback_npc_despawned(const npc_despawned* data, script_node** args)
{
    return (unsigned int)*args[0] == data->npc_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21020
event_result Event_callback_vehicle_damage_threshold(const vehicle_takes_damage_event* data, script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const float threshold = (float)*args[1];
    return data->handle == handle && data->health_level <= threshold ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21070
event_result Event_callback_player_enters_any_vehicle(const player_enters_vehicle_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B21080
event_result Event_callback_player_exits_any_vehicle(const player_exits_vehicle_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B21090
event_result Event_callback_player_enters_vehicle(const player_enters_vehicle_event* data, script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    return data != nullptr && data->handle == handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B210C0
event_result Event_callback_player_exits_vehicle(const player_exits_vehicle_event* data, script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    return data != nullptr && data->handle == handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B210F0
event_result Event_callback_npc_enters_player_vehicle(const npc_enters_vehicle_event* data, script_node** args)
{
    if (data->npc_handle != (unsigned int)*args[0])
    {
        return EVENT_IGNORED;
    }
    player* pp = player_get_local_player();
    return pp != nullptr && pp->vehicle_handle == data->vehicle_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21130
event_result Event_callback_npc_anyone_enters_vehicle(const npc_enters_vehicle_event* data, script_node** args)
{
    return (unsigned int)*args[0] == data->vehicle_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21150
event_result Event_callback_mission_success(const mission_success_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B21180
event_result Event_callback_mission_failure(const mission_failure_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B211D0
event_result Event_callback_objective_complete(const objective_complete_event* data, script_node** args)
{
    return data->handle == (int)(game_handle)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B211F0
event_result Event_callback_mission_objective_complete(const mission_objective_complete_event* data, script_node** args)
{
    return data->handle == (int)(game_handle)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21210
event_result Event_callback_vehicle_under_attack_any_npc(const vehicle_under_attack_event* data, script_node** args)
{
    return (unsigned int)*args[0] == data->vehicle_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21230
event_result Event_callback_player_fires_weapon_or_detonates(const player_fires_weapon_or_detonates_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B21240
event_result Event_callback_squad_members_spawned(const squad_members_spawned_event* data, script_node** args)
{
    return data->spawn_node_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21260
event_result Event_callback_walker_mount_vehicle(const walker_mount_vehicle_event* data, script_node** args)
{
    return data->walker_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21280: original quirk kept: the local player is not checked for null, here or in the next one.
event_result Event_callback_player_used_action_node(const human_using_action_node_event* data, script_node** args)
{
    if (data->action_node_handle != (unsigned int)*args[0])
    {
        return EVENT_IGNORED;
    }
    return data->human_handle == player_get_local_player()->get_handle() && !data->start ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B212C0
event_result Event_callback_player_started_action_node(const human_using_action_node_event* data, script_node** args)
{
    if (data->action_node_handle != (unsigned int)*args[0])
    {
        return EVENT_IGNORED;
    }
    return data->human_handle == player_get_local_player()->get_handle() && data->start ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B21340
event_result Event_callback_player_enters_critical_area(const player_enters_critical_area_event*, script_node**)
{
    return EVENT_HANDLED;
}

// 0x00B21350
event_result Event_callback_human_stopped_being_rendered(const human_stopped_being_rendered_event* data, script_node** args)
{
    return data->target_handle == (unsigned int)*args[0] ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B27550
event_result Event_callback_human_type_dead(const human_dead_event* data, script_node** args)
{
    const human_info* info = human_info_find((int)(game_handle)*args[0]);
    const human* hp = object_of_type<human>(data->human_handle, OT_HUMAN);
    return hp != nullptr && hp->h_info == info ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B275A0
event_result Event_callback_sub_piece_damaged(const building_damaged_event* data, script_node** args)
{
    const script_point point = (script_point)*args[0];
    const float threshold = (float)*args[1];
    if (data->building != point.ref_handle || point.ref_handle == 0xFFFFFFFF)
    {
        return EVENT_IGNORED;
    }
    object* objp = World.object_from_handle(point.ref_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return EVENT_IGNORED;
    }
    float percent_destroyed = 0.0f;
    return ((rfg_mover*)objp)->get_sub_piece_percent_destroyed(percent_destroyed, point.index) && threshold <= percent_destroyed ? EVENT_HANDLED : EVENT_IGNORED;
}

// The area of an enter or leave event, when it is the one asked for and not disabled. rfg.exe inlines it below.
static bool script_event_is_for_area(unsigned int area_handle, unsigned int handle)
{
    if (area_handle != handle || handle == 0xFFFFFFFF)
    {
        return false;
    }
    const object_restricted_area* area = object_of_type<object_restricted_area>(handle, OT_RESTRICTED_AREA);
    return area != nullptr && !area->is_disabled();
}

// 0x00B27640
event_result Event_callback_player_enter_restricted_area(const player_enter_restricted_area_event* data, script_node** args)
{
    return script_event_is_for_area(data->area_handle, (unsigned int)*args[0]) ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B27690
event_result Event_callback_player_exit_restricted_area(const player_exit_restricted_area_event* data, script_node** args)
{
    return script_event_is_for_area(data->area_handle, (unsigned int)*args[0]) ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B276E0: true when the object is within the road's width plus 5 metres of the blocked road.
event_result Event_callback_nearest_road_becomes_blocked(const road_becomes_blocked_event* data, script_node** args)
{
    object* objp = World.object_from_handle((unsigned int)*args[0]);
    const object_path_road* road = object_of_type<object_path_road>(data->road_handle, OT_PATH_ROAD);
    if (objp == nullptr || road == nullptr)
    {
        return EVENT_IGNORED;
    }
    vector closest;
    const float u = road->find_closest_point_on_road(objp->pos, closest, false);
    vec_change_to_local_coords(closest, objp->pos, objp->orient);
    vector bmin;
    vector bmax;
    objp->get_local_bbox(bmin, bmax);
    const float dist_squared = bbox_find_distance_squared(bmin, bmax, closest);
    const float reach = road->get_width(u) + 5.0f;
    return dist_squared <= reach * reach ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B277F0: the attacker is the player, or a vehicle the player drives.
event_result Event_callback_player_damages_vehicle(const vehicle_takes_damage_event* data, script_node** args)
{
    if ((unsigned int)*args[0] != data->handle)
    {
        return EVENT_IGNORED;
    }
    const vehicle* attacker = object_of_type<vehicle>(data->attacker, OT_VEHICLE);
    const human* driver = attacker != nullptr ? vehicle_passenger_in_seat(attacker, VSI_DRIVER) : nullptr;
    return object_is_local_player(data->attacker) || object_is_local_player(driver) ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B27860
event_result Event_callback_squad_any_member_enters_vehicle(const npc_enters_vehicle_event* data, script_node** args)
{
    const unsigned int squad_handle = (unsigned int)*args[0];
    const unsigned int vehicle_handle = (unsigned int)*args[1];
    if (vehicle_handle != data->vehicle_handle)
    {
        return EVENT_IGNORED;
    }
    npc* np = npc_from_handle(data->npc_handle);
    return np != nullptr && np->get_ai() != nullptr && squad_handle == np->get_ai()->get_squad_handle() ? EVENT_HANDLED : EVENT_IGNORED;
}

// True when handle is the squad, or a squad that holds it. rfg.exe inlines it in the two events below.
static bool script_handle_in_squad(const object_squad* squad, unsigned int handle)
{
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        if (squad->member_array[i] == handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00B278D0
event_result Event_callback_player_under_attack(const player_under_attack_event* data, script_node** args)
{
    const object* objp = World.object_from_handle((unsigned int)*args[0]);
    if (objp == nullptr)
    {
        return EVENT_IGNORED;
    }
    if (objp->get_handle() == data->handle)
    {
        return EVENT_HANDLED;
    }
    return objp->get_obj_type() == OT_SQUAD && script_handle_in_squad((const object_squad*)objp, data->handle) ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B27930
event_result Event_callback_vehicle_under_attack(const vehicle_under_attack_event* data, script_node** args)
{
    const unsigned int vehicle_handle = (unsigned int)*args[0];
    const unsigned int attacker_handle = (unsigned int)*args[1];
    if (vehicle_handle != data->vehicle_handle)
    {
        return EVENT_IGNORED;
    }
    if (attacker_handle == data->attacker_handle)
    {
        return EVENT_HANDLED;
    }
    const object_squad* squad = object_of_type<object_squad>(attacker_handle, OT_SQUAD);
    return squad != nullptr && script_handle_in_squad(squad, data->attacker_handle) ? EVENT_HANDLED : EVENT_IGNORED;
}

// 0x00B279B0
event_result Event_callback_patrol_any_squad_reached_node(const patrol_squad_reached_node_event* data, script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (object_of_type<object>(handle, OT_NAVPOINT) == nullptr)
    {
        return EVENT_IGNORED;
    }
    return handle == data->patrol_node->nav_handle ? EVENT_HANDLED : EVENT_IGNORED;
}

// ---- GF6 actions

// 0x00B21370
void Action_callback_debug_console_command(script_node** args)
{
    console_do_command((const char*)*args[0]);
}

// 0x00B21390: empty in rfg.exe, and so are the next four.
void Action_callback_post_radio_message(script_node**)
{
}

// 0x00B213A0
void Action_callback_play_radio_message_on_human(script_node**)
{
}

// 0x00B213B0
void Action_callback_play_radio_message_on_team(script_node**)
{
}

// 0x00B213C0
void Action_callback_play_radio_message_on_persona(script_node**)
{
}

// 0x00B213D0
void Action_callback_post_message(script_node**)
{
}

// 0x00B213E0: Magic number: 4 seconds on screen.
void Action_callback_post_local_message(script_node** args)
{
    hud_message_add(localization_get_display_string_req((const char*)*args[0], nullptr), 4.0f, nullptr, nullptr, nullptr);
}

// 0x00B21470
void Action_callback_add_objective(script_node** args)
{
    add_objective((int)(game_handle)*args[0]);
}

// 0x00B21490
void Action_callback_remove_objective(script_node** args)
{
    remove_objective((int)(game_handle)*args[0]);
}

// 0x00B214B0
void Action_callback_add_objective_icon(script_node** args)
{
    const int objective = (int)(game_handle)*args[0];
    const minimap_elements icon = (minimap_elements)(int)*args[1];
    for (const unsigned int* list = (unsigned int*)*args[2]; list != nullptr && *list != HANDLE_LIST_END; list++)
    {
        object* objp = World.object_from_handle(*list);
        if (objp == nullptr)
        {
            continue;
        }
        if (objp->get_obj_type() == OT_SQUAD)
        {
            ((object_squad*)objp)->assign_objective_icons_to_members(objective, icon);
        }
        else
        {
            add_objective_icon(objective, *list, icon, false);
        }
    }
}

// 0x00B21540
void Action_callback_remove_mission_objective(script_node** args)
{
    remove_objective((int)(game_handle)*args[0]);
}

// 0x00B215E0
void Action_callback_add_player_marker(script_node** args)
{
    const object* objp = World.object_from_handle((unsigned int)*args[0]);
    if (objp != nullptr)
    {
        Cartographer.add_player_marker(objp->pos, true, 0xFFFFFFFF, false, false);
    }
}

// 0x00B21620
void Action_callback_remove_player_marker(script_node**)
{
    Cartographer.remove_player_marker(true);
}

// 0x00B21630: the handles to mark are the arguments from the third on.
void Action_callback_add_mission_objective_icon_closest(script_node** args)
{
    const int objective = (int)(game_handle)*args[0];
    const minimap_elements icon = (minimap_elements)(int)*args[1];
    for (int i = 2; args[i] != nullptr; i++)
    {
        if (World.object_from_handle((unsigned int)*args[i]) != nullptr)
        {
            add_objective_icon(objective, (unsigned int)*args[i], icon, true);
        }
    }
}

// 0x00B216B0
void Action_callback_remove_objective_icon(script_node** args)
{
    const int objective = (int)(game_handle)*args[0];
    for (int i = 1; args[i] != nullptr; i++)
    {
        const unsigned int handle = (unsigned int)*args[i];
        if (Cartographer.does_icon_exist(handle) || Cartographer.does_sticky_icon_exist(handle))
        {
            remove_objective_icon(objective, handle);
        }
    }
}

// 0x00B21730
void Action_callback_mission_lock_ped_level(script_node** args)
{
    const gfm_alert_levels alert = (gfm_alert_levels)(game_handle)*args[0];
    amb_spawn_lock_ped_levels(alert, (ambient_spawn_density)(game_handle)*args[1]);
}

// 0x00B21760
void Action_callback_mission_lock_veh_level(script_node** args)
{
    const gfm_alert_levels alert = (gfm_alert_levels)(game_handle)*args[0];
    amb_spawn_lock_veh_levels(alert, (ambient_spawn_density)(game_handle)*args[1]);
}

// 0x00B21790
void Action_callback_mover_set_visible_over_fow(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool visible = (bool)*args[1];
    if (handle != 0xFFFFFFFF)
    {
        Cartographer.object_set_visible_over_fow(handle, visible);
    }
}

// 0x00B21830
void Action_callback_activity_disable_diversions(script_node** args)
{
    if ((bool)*args[0])
    {
        activity_suspend_diversions(ADR_SCRIPT);
    }
    else
    {
        activity_reinstate_diversions(ADR_SCRIPT);
    }
}

// 0x00B21860
void Action_callback_activity_set_spawn(script_node** args)
{
    if ((bool)*args[0])
    {
        activity_reinstate_activities(ADR_SCRIPT);
    }
    else
    {
        activity_suspend_activities_special(ADR_SCRIPT);
    }
}

// 0x00B21890: original quirk kept: the count is tested after each registration, so up to 0x81 entries go in.
void Action_callback_raid_register_object(script_node** args)
{
    const unsigned int raid_node = (unsigned int)*args[0];
    const unsigned int* list = (unsigned int*)*args[1];
    if (raid_node == 0xFFFFFFFF)
    {
        return;
    }
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        activity_raid_register_target(raid_node, list[i]);
        if (i >= 0x80)
        {
            break;
        }
    }
}

// 0x00B218F0: rfg.exe only reports a critical failure here.
void Action_callback_start_vrail(script_node**)
{
    VLIB_CRITICAL_FAILURE();
}

// 0x00B21910
void Action_callback_vrail_play_next_story_section(script_node**)
{
    activity_riding_shotgun_play_next_story_section(); // rfg.exe inlines it
}

// 0x00B21920
void Action_callback_vrail_chase_vehicle_count(script_node** args)
{
    vrail_change_chase_count((int)*args[0]);
}

// 0x00B21940
void Action_callback_spawn_roadblock(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (World.object_from_handle(handle) != nullptr)
    {
        roadblock_spawn(handle);
    }
}

// 0x00B21970
void Action_callback_set_raid_regions(script_node** args)
{
    const unsigned int raid_node = (unsigned int)*args[0];
    const bool attacking_team = (bool)*args[1];
    const unsigned int withdraw_handle = (unsigned int)*args[2];
    const unsigned int standard_handle = (unsigned int)*args[3];
    activity_raid_set_regions(raid_node, attacking_team, withdraw_handle, standard_handle, (unsigned int)*args[4]);
}

// 0x00B219D0
void Action_callback_release_squad_to_manager(script_node** args)
{
    activity_release_squad((unsigned int)*args[0]);
}

// 0x00B219F0
void Action_callback_courier_escape(script_node**)
{
    Courier_chase_data.scripted_activity_courier_escape(); // rfg.exe inlines it
}

// 0x00B21A00: empty in rfg.exe.
void Action_callback_asd_setup_bomber(script_node**)
{
}

// 0x00B21AD0
void Action_callback_remove_icon(script_node** args)
{
    Cartographer.remove_icon((unsigned int)*args[0]);
}

// 0x00B21D70
void Action_callback_clear_player_weapons(script_node**)
{
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        human_disarm(pp, false);
    }
}

// 0x00B21D90: empty in rfg.exe.
void Action_callback_propaganda_destroyed(script_node**)
{
}

// 0x00B21DC0
void Action_callback_new_target_available(script_node**)
{
    wchar_t text[0x80];
    _snprintf_safe(text, 0x80, L"[image:ui_map_icon_phigh] %ls", localization_get_display_string_req("HM_NEW_TARGET_AVAILABLE", nullptr));
    hud_message_add_minor(text, -1.0f, false);
}

// 0x00B21E70
void Action_callback_player_enter_radiation(script_node** args)
{
    const float damage_per_second = (float)*args[0];
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        player_radiation_start(pp, damage_per_second);
    }
}

// 0x00B21EA0
void Action_callback_player_exit_radiation(script_node**)
{
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        player_radiation_stop(pp);
    }
}

// 0x00B21F10
void Action_callback_add_weapon_to_arsenal(script_node** args)
{
    const weapon_info* w_info = weapons_get_weapon_info((unsigned int)(game_handle)*args[0]);
    if (!hud_ui_weapon_cabinet_unlock_weapon(w_info, true))
    {
        hud_ui_weapon_cabinet_add_weapon(w_info, true);
    }
}

// 0x00B21F50
void Action_callback_explosion_spawn(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const explosion_info* info = explosion_lookup((unsigned int)(game_handle)*args[1]);
    if (info != nullptr)
    {
        object* owner = World.object_from_handle(handle);
        if (owner != nullptr)
        {
            explosion_create(info, nullptr, owner, owner->pos, owner->orient, nullptr, nullptr, false);
        }
    }
}

// 0x00B220D0
void Action_callback_disable_threaten_actions(script_node** args)
{
    if ((bool)*args[0])
    {
        ai_disable_threaten_behavior();
    }
    else
    {
        ai_enable_threaten_behavior();
    }
}

// 0x00B22290
void Action_callback_walker_spawn_enable(script_node** args)
{
    object_spawn_set_walker_spawn_status((bool)*args[0]);
}

// 0x00B222B0
void Action_callback_restore_object(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        World.restore_object(list[i]);
    }
}

// 0x00B22310
void Action_callback_mission_clear_unique_data(script_node**)
{
    if (mission_is_active())
    {
        mission_clear_data();
    }
}

// 0x00B22410: empty in rfg.exe.
void Action_callback_log_objective_complete(script_node**)
{
}

// 0x00B22440
void Action_callback_set_vehicle_hitpoints(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const int new_max = (int)*args[1];
    if (handle != 0xFFFFFFFF)
    {
        vehicle_set_max_hp(new_max, handle);
    }
}

// 0x00B224B0
void Action_callback_disable_team_squad_spawns(script_node** args)
{
    object_squad_spawn_disable_for_team((human_teams)(game_handle)*args[0]);
}

// 0x00B224D0
void Action_callback_enable_team_squad_spawns(script_node** args)
{
    object_squad_spawn_enable_for_team((human_teams)(game_handle)*args[0]);
}

// 0x00B224F0
void Action_callback_squad_node_disable_spawning(script_node** args)
{
    object_squad_spawn_node_set_disabled((bool)*args[0]);
}

// 0x00B22510
void Action_callback_continuous_attack_start(script_node** args)
{
    const int index = (int)(game_handle)*args[1];
    const object* target = World.object_from_handle((unsigned int)*args[0]);
    if (target != nullptr)
    {
        activity_cont_attack_start(target, index);
    }
}

// 0x00B22590
void Action_callback_continuous_attack_set_attacker_count(script_node** args)
{
    activity_cont_attack_set_attacker_count((int)(float)*args[0]);
}

// 0x00B225B0: with no attack running the change itself becomes the count.
void Action_callback_continuous_attack_change_attacker_count(script_node** args)
{
    const int change = (int)(float)*args[0];
    int count = 0;
    const int new_count = activity_cont_attack_get_attacker_count(count) ? count + change : change;
    activity_cont_attack_set_attacker_count(new_count < 0 ? 0 : new_count);
}

// 0x00B225F0
void Action_callback_continuous_attack_end(script_node**)
{
    activity_cont_attack_end(); // rfg.exe inlines the same body
}

// 0x00B22600
void Action_callback_continuous_attack_set_spawn_group(script_node** args)
{
    const int group = (int)*args[0];
    if (group > 0)
    {
        activity_cont_attack_set_spawn_group((unsigned int)group);
    }
}

// 0x00B22620: empty in rfg.exe, and so is the next one.
void Action_callback_play_voice_line(script_node**)
{
}

// 0x00B22630
void Action_callback_player_play_voice_line(script_node**)
{
}

// 0x00B22660
void Action_callback_old_coot_tell_story(script_node** args)
{
    ai_scripting_trigger_behavior_on_character((unsigned int)*args[0], AI_SB_COOT_RAMBLE, 0xFFFFFFFF, -1, -1);
}

// 0x00B22680
void Action_callback_npc_combat_damage_enable(script_node** args)
{
    No_npc_npc_damage = !(bool)*args[0];
}

// 0x00B226A0
void Action_callback_set_priority_target(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    ai_scripting_set_priority_target(handle, (bool)*args[1]);
}

// 0x00B226D0
void Action_callback_set_vehicle_enabled_for_npc_drivers(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    ai_scripting_set_vehicle_enabled_for_npc_drivers(handle, (bool)*args[1]);
}

// 0x00B22700
void Action_callback_set_vehicle_turrets_enabled_for_npcs(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    ai_scripting_set_vehicle_turrets_enabled_for_npcs(handle, (bool)*args[1]);
}

// 0x00B22730
void Action_callback_set_global_vehicle_driver_panic(script_node** args)
{
    Vehicle_ai_panic_drivers = (bool)*args[0];
}

// 0x00B22750
void Action_callback_player_set_vehicle_disallow_exit(script_node** args)
{
    const bool disallow = (bool)*args[0];
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        pp->hflags.disallow_vehicle_exit = disallow;
    }
}

// 0x00B22790: always true, whatever the script passes.
void Action_callback_vehicle_avoid_despawning_player_vehicles(script_node**)
{
    vehicle_set_avoid_despawning_vehicles(true);
}

// 0x00B227A0
void Action_callback_set_npc_only_ride_with_player(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    ai_scripting_set_only_ride_with_player(handle, (bool)*args[1]);
}

// 0x00B228F0
void Action_callback_mission_end_warp(script_node** args)
{
    mission_end_warp_to_object((unsigned int)*args[0]);
}

// 0x00B22980
void Action_callback_audio_play_automobile_megaphone(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    human_voice_persona* persona = human_voice_persona_from_handle((human_voice_persona_handle)(game_handle)*args[1]);
    automobile_play_megaphone(line, persona, (unsigned int)*args[2]);
}

// 0x00B229C0
void Action_callback_object_set_position(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int target_handle = (unsigned int)*args[1];
    object* objp = World.object_from_handle(handle);
    if (objp != nullptr)
    {
        const object* target = World.object_from_handle(target_handle);
        if (target != nullptr)
        {
            objp->update_pos_and_orient(target->pos, target->orient, true);
        }
    }
}

// 0x00B22A20: empty in rfg.exe.
void Action_callback_object_remove(script_node**)
{
}

// 0x00B22A30
void Action_callback_music_set_minimum_level(script_node** args)
{
    gamemusic_set_minimum_music_level((GAMEMUSIC_THRESHOLDS)(game_handle)*args[0]);
}

// 0x00B22AC0
void Action_callback_player_ragdoll_on_next_explosion(script_node** args)
{
    const int get_up_time = (int)*args[0];
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        pp->pflags.ragdoll_on_next_explosion = 1;
        pp->ragdoll_override_get_up_time = get_up_time;
    }
}

// 0x00B22B00
void Action_callback_enable_mission_critical_area(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    critical_area_create(handle, (float)*args[1]);
}

// 0x00B22B40
void Action_callback_disable_mission_critical_area(script_node**)
{
    critical_area_delete(); // rfg.exe inlines it
}

// 0x00B22B50
void Action_callback_enable_minimap_static_zone(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    Cartographer.enable_mini_static(handle, (float)*args[1]);
}

// 0x00B22B90
void Action_callback_disable_minimap_static_zone(script_node**)
{
    Cartographer.enable_mini_static(0xFFFFFFFF, 0.0f);
}

// 0x00B22BB0
void Action_callback_show_important_buildings(script_node** args)
{
    Cartographer.show_important_bldgs((bool)*args[0]);
}

// 0x00B22BE0
void Action_callback_set_team_dispositions(script_node** args)
{
    const int team1 = (int)*args[0];
    if (team1 >= NUM_HUMAN_TEAMS)
    {
        return;
    }
    const int team2 = (int)*args[1];
    if (team2 >= NUM_HUMAN_TEAMS || team1 == team2)
    {
        return;
    }
    const int disposition = (int)*args[2];
    if (disposition < NUM_HUMAN_TEAM_DISPOSITIONS)
    {
        human_teams_set_disposition((human_teams)team1, (human_teams)team2, (human_team_dispositions)disposition);
    }
}

// 0x00B22C90
void Action_callback_handbook_unlock_entry(script_node** args)
{
    hud_ui_handbook_unlock_entry((const char*)*args[0]);
}

// 0x00B22CB0
void Action_callback_handbook_show(script_node**)
{
    if (Tool_tips_enabled)
    {
        hud_ui_handbook_show();
    }
}

// 0x00B22CC0
void Action_callback_fullscreen_map_show(script_node**)
{
    if (Tool_tips_enabled)
    {
        minimap_start_fullscreen_map(checksum_stri("GPS_Path", 0), false, true);
    }
}

// 0x00B22D50
void Action_callback_mission_hostage_icons_remove(script_node**)
{
    hud_ui_missions_set_hostages(0, nullptr);
}

// 0x00B22D60
void Action_callback_dlc_found_fuel_cell(script_node**)
{
    stats_increment_by_one(STATS_FUEL_CELLS_FOUND);
    const tracked_stat* stat = nullptr;
    stats_get_stat(stat, STATS_FUEL_CELLS_FOUND);
    wchar_t count[10];
    sprintf(count, L"%d", stats_get_stat_value(stat));
    wchar_t text[64];
    sprintf(text, localization_get_display_string_req("HM_FUEL_CELLS_FOUND", nullptr), count);
    hud_message_add_minor(text, -1.0f, false);
}

// 0x00B27AD0
void Action_callback_mission_add_ground_marker_vehicle_entry(script_node** args)
{
    vehicle* vp = object_of_type<vehicle>((unsigned int)*args[0], OT_VEHICLE);
    if (vp != nullptr)
    {
        vehicle_interaction_highlight_entry_points(vp, (float)*args[1]);
    }
}

// 0x00B27B30
void Action_callback_spawn_courier(script_node** args)
{
    const object_courier_end_point* end_point = object_of_type<object_courier_end_point>((unsigned int)*args[0], OT_COURIER_END);
    if (end_point != nullptr)
    {
        activity_cc_spawn_courier(end_point);
    }
}

// 0x00B27B70
void Action_callback_spawn_convoy(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const int index = (int)(game_handle)*args[1];
    const object_patrol* patrol = object_of_type<object_patrol>(handle, OT_PATROL);
    const convoy_definition* definition = convoy_definition_from_index(index);
    if (definition != nullptr)
    {
        convoy_create(patrol, definition, nullptr);
    }
}

// 0x00B27BD0: original quirk kept: activities come back for any object, not only for a raid node.
void Action_callback_activity_spawn(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return;
    }
    activity_reinstate_activities(ADR_SCRIPT);
    if (objp->get_obj_type() == OT_RAID_NODE)
    {
        activity_raid_start_raid((const object_raid_node*)objp);
    }
}

// 0x00B27C20
void Action_callback_vrail_set_spawn_region(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object_spawn_region* region = object_of_type<object_spawn_region>(list[i], OT_SPAWN_REGION);
        if (region != nullptr)
        {
            region->set_vrail_spawn_flag(true);
        }
    }
}

// 0x00B27C70
void Action_callback_vrail_clear_spawn_region(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object_spawn_region* region = object_of_type<object_spawn_region>(list[i], OT_SPAWN_REGION);
        if (region != nullptr)
        {
            region->set_vrail_spawn_flag(false);
        }
    }
}

// 0x00B27CC0
void Action_callback_start_mission_vrail(script_node** args)
{
    const int index = (int)(game_handle)*args[0];
    const object_patrol* patrol = object_of_type<object_patrol>((unsigned int)*args[1], OT_PATROL);
    const object* drop_off = World.object_from_handle((unsigned int)*args[2]);
    if (index != -1 && patrol != nullptr && drop_off != nullptr)
    {
        activity_mission_vrail_start(index, patrol, drop_off->pos);
    }
}

// 0x00B27D40: drops the rider off at the patrol's last node.
void Action_callback_start_mission_vrail_manual(script_node** args)
{
    const int index = (int)(game_handle)*args[0];
    const object_patrol* patrol = object_of_type<object_patrol>((unsigned int)*args[1], OT_PATROL);
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[2], OT_SQUAD);
    vehicle* vp = object_of_type<vehicle>((unsigned int)*args[3], OT_VEHICLE);
    vector drop_off;
    if (index != -1 && patrol != nullptr && squad != nullptr && !squad->is_killed() && vp != nullptr && patrol->get_last_patrol_node_pos(drop_off))
    {
        activity_mission_vrail_start(index, patrol, drop_off, squad, vp);
    }
}

// 0x00B27E20
void Action_callback_riding_shotgun_start(script_node** args)
{
    const object_riding_shotgun_node* node = object_of_type<object_riding_shotgun_node>((unsigned int)*args[0], OT_RIDING_SHOTGUN_NODE);
    if (node != nullptr)
    {
        activity_riding_shotgun_start(node);
    }
}

// 0x00B27E60
void Action_callback_start_raid(script_node** args)
{
    const object_raid_node* node = object_of_type<object_raid_node>((unsigned int)*args[0], OT_RAID_NODE);
    if (node != nullptr)
    {
        activity_raid_start_raid(node);
    }
}

// 0x00B27F40: original quirk kept: the npc's ai is not checked for null.
void Action_callback_npc_give_weapon_and_equip(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int weapon = (unsigned int)(game_handle)*args[1];
    const int ammo_count = (int)*args[2];
    const weapon_info* w_info = weapons_get_weapon_info(weapon);
    npc* np = npc_from_handle(handle);
    if (np != nullptr && !np->is_killed())
    {
        np->get_ai()->planner.clear();
        np->get_ai()->planner.request_replan();
        human_delete_all_items_from_inventory(np, false, false);
        human_add_item_to_inventory(np, w_info->name, 1, ammo_count, -1, true, false, true);
    }
}

// 0x00B27FF0
void Action_callback_set_building_owner(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const human_teams team = (human_teams)(game_handle)*args[1];
    object_mover* mover = object_of_type<object_mover>(handle, OT_MOVER);
    if (mover != nullptr)
    {
        mover->team = team;
    }
}

// Magic number: the bit of object_mover::mover_flags that shows a building's damage on the minimap.
static const unsigned int MOVER_FLAG_SHOW_DAMAGE_ON_MINIMAP = 0x20000;

// 0x00B28030
void Action_callback_building_show_damage_on_minimap(script_node** args)
{
    object_mover* mover = object_of_type<object_mover>((unsigned int)*args[0], OT_MOVER);
    if (mover != nullptr)
    {
        mover->mover_flags |= MOVER_FLAG_SHOW_DAMAGE_ON_MINIMAP;
    }
}

// 0x00B28070
void Action_callback_effect_spawn(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int effect = (unsigned int)(game_handle)*args[1];
    const bool looping = (bool)*args[2];
    const object* objp = World.object_from_handle(handle);
    if (objp != nullptr)
    {
        Action_return_value.set(effects_play(effect, objp->pos, objp->orient, looping, handle, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE));
    }
}

// 0x00B28100
void Action_callback_effect_spawn_at_script_point(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    const script_point point = (script_point)*args[0];
    const unsigned int effect = (unsigned int)(game_handle)*args[1];
    const bool looping = (bool)*args[2];
    Action_return_value.set(effects_play_at_script_point(effect, point.ref_handle, point.index, looping, nullptr, nullptr));
}

// 0x00B28170
void Action_callback_remove_effect(script_node** args)
{
    object_effect* effect = object_of_type<object_effect>((unsigned int)*args[0], OT_EFFECT);
    if (effect != nullptr)
    {
        effect->stop(object_effect::SS_STOP_EMITTING);
    }
}

// 0x00B281B0
void Action_callback_trigger_region_enable(script_node** args)
{
    trigger_region* trigger = object_of_type<trigger_region>((unsigned int)*args[0], OT_TRIGGER_REGION);
    if (trigger != nullptr)
    {
        trigger->enable();
    }
}

// 0x00B28230
void Action_callback_trigger_region_disable(script_node** args)
{
    trigger_region* trigger = object_of_type<trigger_region>((unsigned int)*args[0], OT_TRIGGER_REGION);
    if (trigger != nullptr)
    {
        trigger->disable();
    }
}

// 0x00B28260
void Action_callback_trigger_region_disable_list(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        trigger_region* trigger = object_of_type<trigger_region>(list[i], OT_TRIGGER_REGION);
        if (trigger != nullptr)
        {
            trigger->disable();
        }
    }
}

// 0x00B28500
void Action_callback_enable_restricted_area(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object_restricted_area* area = object_of_type<object_restricted_area>(list[i], OT_RESTRICTED_AREA);
        if (area != nullptr)
        {
            area->enable(); // rfg.exe inlines it
        }
    }
}

// 0x00B28550
void Action_callback_disable_restricted_area(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object_restricted_area* area = object_of_type<object_restricted_area>(list[i], OT_RESTRICTED_AREA);
        if (area != nullptr)
        {
            area->disable();
        }
    }
}

// 0x00B285A0
void Action_callback_delete_chunk(script_node** args)
{
    object* mover = object_of_type<object>((unsigned int)*args[0], OT_MOVER);
    if (mover != nullptr)
    {
        World.destroy_object(mover, false);
    }
}

// 0x00B285E0
void Action_callback_display_timer(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle != 0xFFFFFFFF)
    {
        script_timer_display(handle, TP_DEFAULT);
    }
}

// 0x00B28600
void Action_callback_hide_timer(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle != 0xFFFFFFFF)
    {
        script_timer_hide(handle);
    }
}

// 0x00B28620
void Action_callback_vehicle_bomb_arm(script_node** args)
{
    vehicle* vp = object_of_type<vehicle>((unsigned int)*args[0], OT_VEHICLE);
    if (vp != nullptr)
    {
        vehicle_set_scripted_car_bomb(vp);
    }
}

// 0x00B22420 (LTCG: new_max in EAX, np in ECX): a change of maximum also fills the npc up to it.
static void set_npc_max_hit_points(npc* np, int new_max)
{
    if (new_max != np->max_hit_points)
    {
        np->max_hit_points = new_max;
        np->hit_points = (float)new_max;
    }
}

// 0x00B28660 (LTCG: the squad in EDI)
static void set_squad_max_hit_points(object_squad* squad, int new_max)
{
    for_each_squad_npc(squad, [new_max](npc* np) { set_npc_max_hit_points(np, new_max); });
}

// 0x00B286D0: original quirk kept: a handle with no object ends the walk of the list, it does not skip the entry.
void Action_callback_set_npc_hitpoints(script_node** args)
{
    const int new_max = (int)*args[0];
    const unsigned int* list = (unsigned int*)*args[1];
    if (new_max <= 0)
    {
        return;
    }
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object* objp = World.object_from_handle(list[i]);
        if (objp == nullptr)
        {
            return;
        }
        switch (objp->get_obj_type())
        {
        case OT_HUMAN:
            if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
            {
                set_npc_max_hit_points((npc*)objp, new_max);
            }
            break;
        case OT_SQUAD:
            set_squad_max_hit_points((object_squad*)objp, new_max);
            break;
        case OT_NPC_SPAWN_NODE:
        {
            npc* np = ((object_npc_spawn_node*)objp)->get_child_npc();
            if (np != nullptr)
            {
                set_npc_max_hit_points(np, new_max);
            }
            break;
        }
        case OT_SQUAD_SPAWN_NODE:
        {
            object_squad* squad = object_of_type<object_squad>(((object_squad_spawn_node*)objp)->get_child_squad_handle(), OT_SQUAD);
            if (squad != nullptr)
            {
                set_squad_max_hit_points(squad, new_max);
            }
            break;
        }
        default:
            break;
        }
    }
}

// 0x00B28800: a sphere around the object, optionally attached to it. Magic number: the block's bbox is the sphere's cube.
void Action_callback_trigger_region_create(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    const unsigned int handle = (unsigned int)*args[0];
    const float radius = (float)*args[1];
    const bool attach = (bool)*args[2];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return;
    }
    obj_prop_block pblock(objp->pos, objp->orient, nullptr);
    bbox box;
    box.min.set(-radius, -radius, -radius);
    box.max.set(radius, radius, radius);
    pblock.set<bbox>(TR_PROP_BOX, box);
    pblock.set<bool>(TR_PROP_ENABLED, true);
    pblock.set<float>(TR_PROP_RADIUS, radius);
    pblock.set(TR_PROP_SHAPE, "sphere");
    // Deviation from rfg.exe: through the generic create_object<trigger_region>, with the null zone of its
    // direct call. Same result.
    trigger_region* child = World.create_object<trigger_region>(pblock, 0xFFFFFFFF);
    if (child != nullptr)
    {
        child->obj_flags.no_save = 1;
        if (attach && child->get_parent() == 0xFFFFFFFF)
        {
            objp->set_child(child, -1, -1, false, true);
        }
        Action_return_value.set(child->get_handle());
    }
}

// 0x00B28990
void Action_callback_trigger_region_remove(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (object_of_type<object>(handle, OT_TRIGGER_REGION) != nullptr)
    {
        World.destroy_object(handle, false);
    }
}

// 0x00B289D0: the hostage's squad stops being tied up and defends the player.
void Action_callback_release_mission_hostage(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>(handle, OT_SQUAD_SPAWN_NODE);
    if (node == nullptr)
    {
        return;
    }
    object_squad* squad = object_of_type<object_squad>(node->get_child_squad_handle(), OT_SQUAD);
    if (squad == nullptr)
    {
        return;
    }
    npc* leader = squad->get_leader();
    if (leader == nullptr)
    {
        return;
    }
    leader->npc_flags.freed_from_bonds = 1;
    squad->npc_flags.tied_up = 0;
    const unsigned int old_orders = squad->get_orders_handle();
    gfm_orders_creation_params params;
    params.ofp.reset();
    params.reset();
    params.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.team = pp->current_team;
    params.location_target = pp->pos;
    params.object_target = pp->get_handle();
    const unsigned int new_orders = Orders_manager.create_new_orders(params);
    if (new_orders != 0xFFFFFFFF)
    {
        squad->assign_base_orders(new_orders);
    }
    Orders_manager.destroy_orders(old_orders);
}

// 0x00B28AE0
void Action_callback_spawn_vehicle_hostage(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    const unsigned int handle = (unsigned int)*args[0];
    const int id = (int)(game_handle)*args[1];
    const vehicle_seat_index seat = (vehicle_seat_index)(int)*args[2];
    vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
    if (vp == nullptr || (int)seat >= vehicle_get_num_seats(vp) || vehicle_passenger_in_seat(vp, seat) != nullptr)
    {
        return;
    }
    const human_info* h_info = human_info_find(id);
    if (h_info == nullptr)
    {
        return;
    }
    npc* np = srm_spawn_single_npc_vehicle(h_info, vp, seat);
    if (np != nullptr)
    {
        np->npc_flags.tied_up = 1;
        np->hflags.disallow_vehicle_exit = 1;
        np->hflags.invulnerable = 1;
        np->hflags.ai_ignore = 1;
        np->hflags.mission_invulnerable = 1;
        Action_return_value.set(np->get_handle());
    }
}

// 0x00B28BA0: only clears the return value in rfg.exe.
void Action_callback_spawn_trapped_npc(script_node**)
{
    Action_return_value.set(0xFFFFFFFF);
}

// Requests the squad's despawn and gives each npc the behavior. rfg.exe inlines it in the two callbacks below.
// Magic number: 1, the behavior's special value here; the jailed spawn passes 0.
static void script_free_captive_squad(unsigned int handle, ai_scripting_behaviors behavior)
{
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
        for_each_squad_npc(squad, [behavior](npc* np) { ai_scripting_trigger_behavior_on_character(np, behavior, 0xFFFFFFFF, 1, -1); });
    }
}

// 0x00B28BC0
void Action_callback_player_free_trapped_npc(script_node** args)
{
    script_free_captive_squad((unsigned int)*args[0], AI_SB_TRAPPED_IN_BUILDING);
}

// 0x00B28C50: original quirk kept: a spawn node with no live squad dereferences null.
void Action_callback_custom_action_wounded_follow(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool follow = (bool)*args[1];
    const object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>(handle, OT_SQUAD_SPAWN_NODE);
    if (node == nullptr)
    {
        return;
    }
    object_squad* squad = object_of_type<object_squad>(node->get_child_squad_handle(), OT_SQUAD);
    const ai_scripting_behaviors behavior = follow ? AI_SB_FOLLOW_WOUNDED : AI_SB_NONE;
    for_each_squad_npc(squad, [behavior](npc* np) { ai_scripting_trigger_behavior_on_character(np, behavior, 0xFFFFFFFF, -1, -1); });
}

// 0x00B28D30: original quirk kept: the return value is the child's handle even when no child spawned.
void Action_callback_spawn_jailed_npc(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    object_npc_spawn_node* node = object_of_type<object_npc_spawn_node>((unsigned int)*args[0], OT_NPC_SPAWN_NODE);
    if (node != nullptr)
    {
        const unsigned int child = node->spawn_child_object();
        ai_scripting_trigger_behavior_on_character(child, AI_SB_TRAPPED_IN_JAIL, 0xFFFFFFFF, 0, -1);
        Action_return_value.set(child);
    }
}

// 0x00B28DA0
void Action_callback_player_free_jailed_npc(script_node** args)
{
    script_free_captive_squad((unsigned int)*args[0], AI_SB_TRAPPED_IN_JAIL);
}

// 0x00B28F30: an npc told to plant a singularity bomb at the navpoint, with a mission icon.
void Action_callback_spawn_saboteur(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    static inv_item_info* bomb_inv_info = inv_item_get_info("singularity_EBS"); // 0x0373EF50
    if (bomb_inv_info == nullptr)
    {
        return;
    }
    const unsigned int node_handle = (unsigned int)*args[0];
    const object* target = object_of_type<object>((unsigned int)*args[1], OT_NAVPOINT);
    if (target == nullptr)
    {
        return;
    }
    object_npc_spawn_node* node = object_of_type<object_npc_spawn_node>(node_handle, OT_NPC_SPAWN_NODE);
    if (node == nullptr)
    {
        return;
    }
    const unsigned int child = node->spawn_child_object();
    npc* np = npc_from_handle(child);
    if (np == nullptr)
    {
        return;
    }
    np->obj_flags.no_save = 1;
    np->obj_flags.destroy_on_stream = 1;
    ai_scripting_trigger_behavior_on_character(np, AI_SB_PLANT_BOMB, target->get_handle(), -1, -1);
    if (human_has_item(np, bomb_inv_info) != nullptr || human_add_item_to_inventory(np, bomb_inv_info, 0, -1, -1, true, false, true) != nullptr)
    {
        Cartographer.add_icon(np->get_handle(), MAP_ELEMENT_MISSION_OBJECTIVE, true, false, false);
        np->npc_flags.remove_icon_on_death = 1;
        Action_return_value.set(child);
    }
}

// The two setters of a spawn node. rfg.exe inlines them per class.
template<typename T>
static void script_disable_node(T* node, bool mission_only)
{
    if (mission_only)
    {
        node->disable_mission_spawn();
    }
    else
    {
        node->disable_spawn();
    }
}

// 0x00B29080: the second argument picks the node's mission flag, and only during a mission, over its plain one.
void Action_callback_disable_spawn_node(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool mission_only = (bool)*args[1];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || (mission_only && !mission_is_active()))
    {
        return;
    }
    switch (objp->get_obj_type())
    {
    case OT_VEHICLE_SPAWN_NODE:
        script_disable_node((object_vehicle_spawn_node*)objp, mission_only);
        break;
    case OT_NPC_SPAWN_NODE:
        script_disable_node((object_npc_spawn_node*)objp, mission_only);
        break;
    case OT_TURRET_SPAWN_NODE:
        script_disable_node((object_turret_spawn_node*)objp, mission_only);
        break;
    case OT_SQUAD_SPAWN_NODE:
        script_disable_node((object_squad_spawn_node*)objp, mission_only);
        break;
    default:
        break;
    }
}

// 0x00B29150
void Action_callback_spawn_npc(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    object_npc_spawn_node* node = object_of_type<object_npc_spawn_node>((unsigned int)*args[0], OT_NPC_SPAWN_NODE);
    if (node != nullptr)
    {
        const unsigned int child = node->spawn_child_object();
        if (child != 0xFFFFFFFF)
        {
            Action_return_value.set(child);
        }
    }
}

// The two tables the icon callbacks index, as they sit on rfg.exe's stack: 3 icon types, then 5 minimap elements.
// Magic numbers: the raw values rfg.exe writes, in its enums' value order.
// Original quirk kept: the type index is checked against 4, so an index of 3 reads the first element of the next table.
static const int SCRIPT_ICON_TABLES[8] = {MAP_ICON_TYPE_NORMAL, 1, 2, MAP_ELEMENT_STAR_RED_ICON, 1, 2, 3, 0x28};
static const unsigned int SCRIPT_ICON_NUM_TYPES_CHECKED = 4;
static const unsigned int SCRIPT_ICON_NUM_ELEMENTS = 5;
static const int SCRIPT_ICON_ELEMENTS_START = 3;

// 0x00B29210
void Action_callback_spawn_squad_with_icons(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int element = (unsigned int)(int)*args[1];
    const unsigned int type = (unsigned int)(int)*args[2];
    if (type >= SCRIPT_ICON_NUM_TYPES_CHECKED)
    {
        return;
    }
    object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>(handle, OT_SQUAD_SPAWN_NODE);
    if (node != nullptr && element < SCRIPT_ICON_NUM_ELEMENTS)
    {
        Action_return_value.set(node->spawn_child_object((minimap_elements)SCRIPT_ICON_TABLES[SCRIPT_ICON_ELEMENTS_START + element], (map_icon_types)SCRIPT_ICON_TABLES[type]));
    }
}

// 0x00B292F0
void Action_callback_set_squad_icons(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad == nullptr)
    {
        return;
    }
    const unsigned int element = (unsigned int)(int)*args[1];
    const unsigned int type = (unsigned int)(int)*args[2];
    if (type < SCRIPT_ICON_NUM_TYPES_CHECKED && element < SCRIPT_ICON_NUM_ELEMENTS)
    {
        squad->assign_icons_to_members((minimap_elements)SCRIPT_ICON_TABLES[SCRIPT_ICON_ELEMENTS_START + element], (map_icon_types)SCRIPT_ICON_TABLES[type]);
    }
}

// 0x00B293B0
void Action_callback_spawn_near_squad(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>((unsigned int)*args[0], OT_SQUAD_SPAWN_NODE);
    if (node != nullptr)
    {
        node->respawn_child();
        const unsigned int child = node->spawn_child_object(MAP_ELEMENT_INVALID, MAP_ICON_TYPE_NORMAL);
        if (child != 0xFFFFFFFF)
        {
            Action_return_value.set(child);
        }
    }
}

// 0x00B29410
void Action_callback_guerrilla_squad_despawn(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool immediate = (bool)*args[1];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr && squad->get_squad_team() == HUMAN_TEAM_GUERILLA)
    {
        Orders_manager.destroy_orders(squad->get_orders_handle());
        if (immediate)
        {
            squad->despawn();
        }
        else
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, true);
        }
    }
}

// 0x00B29490
void Action_callback_squad_despawn(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, true);
    }
}

// 0x00B294D0
void Action_callback_squad_despawn_priority(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const npc_despawn_type type = (npc_despawn_type)(game_handle)*args[1];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(type, nullptr, true);
    }
}

// 0x00B29520: original quirk kept: the orders are destroyed twice.
void Action_callback_squad_despawn_object(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int despawn_handle = (unsigned int)*args[1];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        const object* despawn_objp = World.object_from_handle(despawn_handle);
        Orders_manager.destroy_orders(squad->get_orders_handle());
        Orders_manager.destroy_orders(squad->get_orders_handle());
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, despawn_objp, false);
    }
}

// 0x00B295F0: Magic number: 4 teams may be assigned.
void Action_callback_squad_assign_team(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const human_teams team = (human_teams)(game_handle)*args[1];
    if ((int)team < 4)
    {
        object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
        if (squad != nullptr)
        {
            squad->set_team_override(team);
        }
    }
}

// 0x00B29640
void Action_callback_squad_force_abandon_vehicle(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad != nullptr)
    {
        squad->remove_vehicle(true, true);
    }
}

// 0x00B29680
void Action_callback_squad_assign_new_vehicle(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    vehicle* vp = object_of_type<vehicle>((unsigned int)*args[1], OT_VEHICLE);
    if (squad == nullptr)
    {
        return;
    }
    squad->remove_vehicle(true, false);
    if (vp != nullptr)
    {
        squad->assign_vehicle(vp);
        squad->npc_flags.chase_teleport_disable = 1;
        for_each_squad_npc(squad, [](npc* np) { np->npc_flags.chase_teleport_disable = 1; });
    }
}

// 0x00B29750
void Action_callback_spawn_region_disable(script_node** args)
{
    object_spawn_region* region = object_of_type<object_spawn_region>((unsigned int)*args[0], OT_SPAWN_REGION);
    if (region != nullptr)
    {
        region->disable();
    }
}

// 0x00B29780: the time is in seconds, and a negative one is taken as zero.
void Action_callback_disable_turret_for_npcs(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const int seconds = (int)*args[1];
    const object_turret_spawn_node* node = object_of_type<object_turret_spawn_node>(handle, OT_TURRET_SPAWN_NODE);
    if (node != nullptr)
    {
        turret* tp = object_of_type<turret>(node->get_child_turret_handle(), OT_TURRET);
        if (tp != nullptr)
        {
            const int ms = seconds * 1000;
            tp->disable_for_npcs_time.set(ms < 0 ? 0 : ms);
        }
    }
}

// 0x00B29800
void Action_callback_spawn_turret(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    object_turret_spawn_node* node = object_of_type<object_turret_spawn_node>((unsigned int)*args[0], OT_TURRET_SPAWN_NODE);
    if (node != nullptr)
    {
        const unsigned int child = node->spawn_child_object(0xFFFFFFFF);
        if (child != 0xFFFFFFFF)
        {
            Action_return_value.set(child);
        }
    }
}

// 0x00B29860
void Action_callback_spawn_vehicle(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    object_vehicle_spawn_node* node = object_of_type<object_vehicle_spawn_node>((unsigned int)*args[0], OT_VEHICLE_SPAWN_NODE);
    if (node != nullptr && node->obj_flags.streamed)
    {
        const unsigned int child = node->spawn_child_object(0xFFFFFFFF);
        if (child != 0xFFFFFFFF)
        {
            Action_return_value.set(child);
        }
    }
}

// 0x00B298C0: only for a squad of one.
void Action_callback_orders_terrorize(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad == nullptr || squad->get_max_member_count() != 1)
    {
        return;
    }
    const object* target = World.object_from_handle((unsigned int)*args[1]);
    if (target == nullptr)
    {
        return;
    }
    gfm_orders_creation_params params;
    params.ofp.reset();
    params.reset();
    params.orders_type = GFM_ORDERS_TYPE_TERRORIZE;
    params.object_target = target->get_handle();
    params.location_target = target->pos;
    params.team = squad->get_squad_team();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    const unsigned int orders = Orders_manager.create_new_orders(params);
    if (orders != 0xFFFFFFFF)
    {
        squad->reassign_orders(orders, nullptr, false);
    }
}

// 0x00B299A0
void Action_callback_orders_terrorize_spawn(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>((unsigned int)*args[0], OT_SQUAD_SPAWN_NODE);
    if (node != nullptr && node->get_child_squad_handle() == 0xFFFFFFFF)
    {
        node->assign_default_orders(GFM_ORDERS_TYPE_TERRORIZE);
        Action_return_value.set(node->spawn_child_object(MAP_ELEMENT_INVALID, MAP_ICON_TYPE_NORMAL));
    }
}

// 0x00B29A10: a percentage, clamped.
void Action_callback_orders_terrorize_attack_chance(script_node** args)
{
    Sniper_attack_player_chance = (int)*args[0];
    if (Sniper_attack_player_chance > 99)
    {
        Sniper_attack_player_chance = 100;
    }
    else if (Sniper_attack_player_chance < 1)
    {
        Sniper_attack_player_chance = 0;
    }
}

// 0x00B29BA0: the courier's leader leaves the vehicle, and the squad defends the object.
void Action_callback_courier_exit_and_defend(script_node** args)
{
    const unsigned int squad_handle = activity_cc_get_courier_squad_handle();
    if (squad_handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int target_handle = (unsigned int)*args[0];
    if (target_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_squad* squad = object_of_type<object_squad>(squad_handle, OT_SQUAD);
    if (squad == nullptr)
    {
        return;
    }
    const object* target = World.object_from_handle(target_handle);
    if (target == nullptr)
    {
        return;
    }
    npc* leader = squad->get_leader();
    if (leader == nullptr)
    {
        return;
    }
    leader->npc_flags.force_out_of_vehicle = 1;
    leader->hflags.disallow_vehicle_exit = 0;
    leader->hflags.limited_vehicle_exit = 0;
    const vector backwards(-leader->orient.fvec.x, -leader->orient.fvec.y, -leader->orient.fvec.z);
    leader->request_avoidance(nullptr, backwards, ZERO_VECTOR);
    gfm_orders_creation_params params;
    params.ofp.reset();
    params.reset();
    if (!reinforcement_find_destination_location(squad->pos, target->pos, true, params.location_target))
    {
        params.location_target = target->pos;
    }
    params.object_target = target->get_handle();
    params.orders_type = GFM_ORDERS_TYPE_DEFEND;
    params.team = squad->get_squad_team();
    params.ofp = DEFAULT_ORDERS_FLAGS;
    const unsigned int orders = Orders_manager.create_new_orders(params);
    if (orders != 0xFFFFFFFF)
    {
        squad->reassign_orders(orders, nullptr, false);
    }
}

// 0x00B2A0A0
void Action_callback_continuous_attack_add_priority_spawn(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object_spawn_region* region = object_of_type<object_spawn_region>(list[i], OT_SPAWN_REGION);
        if (region != nullptr)
        {
            region->set_continuous_attack_region(true);
        }
    }
}

// 0x00B2A0F0
void Action_callback_continuous_attack_remove_priority_spawn(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object_spawn_region* region = object_of_type<object_spawn_region>(list[i], OT_SPAWN_REGION);
        if (region != nullptr)
        {
            region->set_continuous_attack_region(false);
        }
    }
}

// The turret of the first mount, when the vehicle has one. rfg.exe inlines it in the two callbacks below.
static turret* script_vehicle_first_turret(const vehicle* vp)
{
    return vp->m_num_turret_mounts > 0 ? object_of_type<turret>(vp->m_turret_mounts[0].attached_turret, OT_TURRET) : nullptr;
}

// 0x00B2A140: the delay is in seconds, multiplied as a double as rfg.exe does.
void Action_callback_turret_start_autofire(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const int delay_ms = (int)((double)(float)*args[1] * 1000.0);
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    turret* tp = object_of_type<turret>(handle, OT_TURRET);
    if (tp == nullptr)
    {
        vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
        if (vp == nullptr)
        {
            return;
        }
        vp->m_turret_autofire_ms = delay_ms;
        tp = script_vehicle_first_turret(vp);
        if (tp == nullptr)
        {
            return;
        }
    }
    tp->automatic_firing_pattern_start(delay_ms);
}

// 0x00B2A1E0
void Action_callback_turret_stop_autofire(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    turret* tp = object_of_type<turret>(handle, OT_TURRET);
    if (tp == nullptr)
    {
        vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
        if (vp == nullptr)
        {
            return;
        }
        vp->m_turret_autofire_ms = -1;
        tp = script_vehicle_first_turret(vp);
        if (tp == nullptr)
        {
            return;
        }
    }
    tp->automatic_firing_pattern_end();
}

// 0x00B2A260
void Action_callback_play_unlocking_voice_line_on_human(script_node** args)
{
    const voice_line_unlocking_handle unlocking = (voice_line_unlocking_handle)(game_handle)*args[0];
    human* hp = object_of_type<human>((unsigned int)*args[1], OT_HUMAN);
    if (hp != nullptr && !hp->is_killed())
    {
        const voice_line_handle line = voice_line_unlocking_choose(unlocking);
        if (line != INVALID_VOICE_LINE_HANDLE)
        {
            hp->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, false, false, true, true);
        }
    }
}

// 0x00B2A2D0
void Action_callback_disable_non_pain_voice_lines(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad != nullptr)
    {
        const bool pain_only = (bool)*args[1];
        for_each_squad_npc(squad, [pain_only](npc* np) { np->hflags.voice_line_pain_only = pain_only; });
    }
}

// 0x00B2A590: the player and the human play the action facing each other. Give and take use the nano forge prop, and give
// also holsters the player's weapon and goes back one in the equip history.
// Original quirk kept: the local player is not checked for null.
void Action_callback_play_two_person_animation(script_node** args)
{
    human* hp = object_of_type<human>((unsigned int)*args[0], OT_HUMAN);
    const int anim_action = (int)(game_handle)*args[1];
    if (hp == nullptr || hp->is_killed())
    {
        return;
    }
    player* pp = player_get_local_player();
    vanim_play_action(pp->char_inst->vp, index_alias(anim_action, ANIM_ALIAS_THROW), 0x10, 1.0f, 1.0f);
    if (anim_action == ANIMATION_ACTION_TAKE || anim_action == ANIMATION_ACTION_GIVE)
    {
        item_info* prop = object_item_get_info("nano_forge");
        pp->anim_prop_info = prop;
        hp->anim_prop_info = prop;
        if (anim_action == ANIMATION_ACTION_GIVE)
        {
            human_weapons_immediately_holster_weapon(pp);
            pp->desired_equipped_inv_item = pp->last_equipped_weapon;
            pp->last_equipped_weapon = pp->second_last_equipped_weapon;
            pp->second_last_equipped_weapon = nullptr;
        }
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        ai_system* ai = ((npc*)hp)->get_ai();
        if (ai != nullptr)
        {
            ai->stun((animation_action)anim_action, ANIM_ALIAS_THROW, 1.0f, 0x10);
            human_set_combat_ready(hp, false, 0);
        }
    }
    vector to_human(hp->pos.x - pp->pos.x, hp->pos.y - pp->pos.y, hp->pos.z - pp->pos.z);
    to_human.flatten_normalize();
    matrix facing;
    facing.make_quick(to_human);
    vanim_set_action_starting_orient(pp->char_inst->vp, pp->pos, pp->orient, pp->pos, facing, ANIM_ALIAS_THROW, 0.2f);
    const vector to_player(-to_human.x, -to_human.y, -to_human.z);
    facing.make_quick(to_player);
    vanim_set_action_starting_orient(hp->char_inst->vp, hp->pos, hp->orient, hp->pos, facing, ANIM_ALIAS_THROW, 0.2f);
    pp->block_movement_anim = ANIM_ALIAS_THROW;
    pp->block_steering_vector.zero();
    pp->block_steering_heading_offset = 0.0f;
    pp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    hp->block_movement_anim = ANIM_ALIAS_THROW;
    hp->block_steering_vector.zero();
    hp->block_steering_heading_offset = 0.0f;
    hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    // Magic numbers: the offset between the two characters and the vertical blend time of this action.
    const vector offset(-0.015f, 0.0f, 1.07f);
    vanim_attach_actions(pp, hp, ANIM_ALIAS_THROW, ANIM_ALIAS_THROW, offset, 0.3f);
}

// 0x00B2A870: only the npcs carry the flag, not the squad.
void Action_callback_set_reduced_npc_damage(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool reduced = (bool)*args[1];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    npc* np = npc_from_handle(handle);
    if (np != nullptr)
    {
        np->npc_flags.reduced_npc_damage = reduced;
        return;
    }
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        for_each_squad_npc(squad, [reduced](npc* member) { member->npc_flags.reduced_npc_damage = reduced; });
    }
}

// 0x00B2A950
void Action_callback_remove_vehicle_turret_fov_constraints(script_node** args)
{
    vehicle* vp = object_of_type<vehicle>((unsigned int)*args[0], OT_VEHICLE);
    if (vp != nullptr)
    {
        vp->disable_turret_fov_limits();
    }
}

// 0x00B2A980
void Action_callback_vehicle_disable_accelerator(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool disabled = (bool)*args[1];
    vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
    if (vp != nullptr)
    {
        vehicle_set_accelerator_disabled(vp, disabled);
    }
}

// 0x00B2A9D0
void Action_callback_npc_exit_vehicle(script_node** args)
{
    npc* np = npc_from_handle((unsigned int)*args[0]);
    if (np == nullptr || np->vehicle_handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int vehicle_handle = np->vehicle_handle;
    np->npc_flags.force_out_of_vehicle = 1;
    np->hflags.disallow_vehicle_exit = 0;
    np->hflags.limited_vehicle_exit = 0;
    if (np->get_ai() != nullptr)
    {
        np->request_avoidance(vehicle_handle);
    }
}

// 0x00B2AA40
void Action_callback_player_exit_vehicle(script_node** args)
{
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->vehicle_handle != 0xFFFFFFFF)
    {
        vi_exit_data exit_data;
        exit_data.flags.teleport = (bool)*args[0];
        vehicle_interaction_request_exit(pp, &exit_data);
    }
}

// 0x00B2AA90: only automobiles. A vehicle rail's patrol vehicle ends its rail first.
void Action_callback_bail_vehicle(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_AUTO)
    {
        return;
    }
    unsigned int patrol_vehicle;
    if (vrail_get_patrol_vehicle(patrol_vehicle) && patrol_vehicle == handle)
    {
        activity_mission_vrail_cancel();
    }
    vp->m_flags.disable_for_npc_drivers = 1;
    vp->m_flags.no_ragdoll = 0;
    vp->m_flags.m_scripted_bail = 1;
    vehicle_interaction_bail_all(vp);
}

// 0x00B2ABD0
void Action_callback_audio_play_foley_2D(script_node** args)
{
    Action_return_value.set(0xFFFFFFFF);
    Action_return_value.set((unsigned int)audiolib_cue_play((int)(game_handle)*args[0], nullptr, nullptr));
}

// 0x00B2AC10
void Action_callback_party_time_rock_remove(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const object* mover = object_of_type<object>(handle, OT_MOVER);
    if (mover != nullptr && mover->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        World.destroy_object(handle, false);
    }
}

// 0x00B2AC50
void Action_callback_remove_item_spawn(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    item* itemp = object_of_type<item>(handle, OT_ITEM);
    if (itemp != nullptr)
    {
        itemp->item_flags.respawn = 0;
        World.destroy_object(handle, false);
    }
}

// Scripted movement of one npc. rfg.exe inlines it in the callback below.
static void script_npc_move(npc* np, unsigned int location, bool run, bool fire_at_target, bool use_vehicle)
{
    np->npc_flags.scripted_move = 1;
    np->scripting_info.location_object = location;
    np->scripting_info.flags.run = run;
    np->scripting_info.flags.fire_at_target = fire_at_target;
    np->scripting_info.flags.use_vehicle = use_vehicle;
    if (np->get_ai() != nullptr)
    {
        np->get_ai()->planner.request_replan();
    }
}

// 0x00B2AC90: original quirk kept: the first test reads the subtype without the type, which holds for npcs only.
void Action_callback_move_to_location(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int location = (unsigned int)*args[1];
    const bool run = (bool)*args[2];
    const bool fire_at_target = (bool)*args[3];
    const bool use_vehicle = (bool)*args[4];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || object_of_type<object>(location, OT_NAVPOINT) == nullptr)
    {
        return;
    }
    if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        script_npc_move((npc*)objp, location, run, fire_at_target, use_vehicle);
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        object_squad* squad = (object_squad*)objp;
        squad->npc_flags.scripted_move = 1;
        squad->scripting_info.location_object = location;
        squad->scripting_info.flags.run = run;
        squad->scripting_info.flags.fire_at_target = fire_at_target;
        squad->scripting_info.flags.use_vehicle = use_vehicle;
        for_each_squad_npc(squad, [location, run, fire_at_target, use_vehicle](npc* np) { script_npc_move(np, location, run, fire_at_target, use_vehicle); });
    }
}

// Scripted drive of one npc. It answers whether the npc has an ai to replan.
static bool script_npc_drive(npc* np, unsigned int location, bool drive_directly)
{
    np->npc_flags.scripted_drive = 1;
    np->hflags.disallow_vehicle_exit = 1;
    np->scripting_info.location_object = location;
    np->scripting_info.flags.drive_directly = drive_directly;
    if (np->get_ai() == nullptr)
    {
        return false;
    }
    np->get_ai()->planner.request_replan();
    return true;
}

// Scripted drive of a squad and its npcs. With stop_without_ai, a member with no ai ends the walk and it answers false:
// the _ca and _vrail callbacks return there, while the plain one goes on.
static bool script_squad_drive(object_squad* squad, unsigned int location, bool drive_directly, bool stop_without_ai)
{
    squad->npc_flags.scripted_drive = 1;
    squad->hflags.disallow_vehicle_exit = 1;
    squad->scripting_info.location_object = location;
    squad->scripting_info.flags.drive_directly = drive_directly;
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        npc* np = npc_from_handle(squad->member_array[i]);
        if (np != nullptr && !script_npc_drive(np, location, drive_directly) && stop_without_ai)
        {
            return false;
        }
    }
    return true;
}

// 0x00B2AE50
void Action_callback_drive_to_location(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int location = (unsigned int)*args[1];
    const bool drive_directly = (bool)*args[2];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || World.object_from_handle(location) == nullptr)
    {
        return;
    }
    if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        script_npc_drive((npc*)objp, location, drive_directly);
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        script_squad_drive((object_squad*)objp, location, drive_directly, false);
    }
}

// 0x00B2AFC0: Magic number: 16, the size of the squad list on the stack.
void Action_callback_drive_to_location_ca(script_node** args)
{
    const unsigned int location = (unsigned int)*args[0];
    const bool drive_directly = (bool)*args[1];
    if (World.object_from_handle(location) == nullptr)
    {
        return;
    }
    unsigned int squads[16];
    const int count = activity_cont_attack_get_squads(16, squads);
    for (int i = 0; i < count; i++)
    {
        object_squad* squad = object_of_type<object_squad>(squads[i], OT_SQUAD);
        if (squad == nullptr || !script_squad_drive(squad, location, drive_directly, true))
        {
            return;
        }
    }
}

// 0x00B2B350: the local player is never moved. A squad is walked from its last member down.
void Action_callback_human_teleport(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    object* target = World.object_from_handle((unsigned int)*args[1]);
    if (target == nullptr || handle == 0xFFFFFFFF)
    {
        return;
    }
    // Magic number: 6 metres, the placement range.
    const float placement_range = 6.0f;
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad == nullptr)
    {
        human* hp = object_of_type<human>(handle, OT_HUMAN);
        if (hp != nullptr && hp != player_get_local_player())
        {
            human_teleport(hp, target->pos, target->orient, placement_range, false);
        }
        return;
    }
    for (int i = squad->member_array.size() - 1; i >= 0; i--)
    {
        human* hp = object_of_type<human>(squad->member_array[i], OT_HUMAN);
        if (hp != nullptr && hp != player_get_local_player())
        {
            human_teleport(hp, target->pos, target->orient, placement_range, false);
        }
    }
}

// 0x00B2B440: an out of range seat leaves the choice to the interaction code.
// Original quirk kept: the seat count is asked with no check of the vehicle for null, and the first answer is dropped.
void Action_callback_human_teleport_into_vehicle(script_node** args)
{
    human* hp = object_of_type<human>((unsigned int)*args[0], OT_HUMAN);
    if (hp == nullptr)
    {
        return;
    }
    vehicle* vp = object_of_type<vehicle>((unsigned int)*args[1], OT_VEHICLE);
    const int seat = (int)*args[2];
    vi_enter_data enter_data;
    enter_data.specified_seat = VSI_INVALID;
    enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_NONE;
    enter_data.destination_seat = VSI_INVALID;
    enter_data.flags.teleport = 1;
    vehicle_get_num_seats(vp);
    if (seat > -1 && seat < vehicle_get_num_seats(vp))
    {
        enter_data.seat_selection_method = VI_SEAT_SELECTION_METHOD_SPECIFIED;
        enter_data.specified_seat = (vehicle_seat_index)seat;
    }
    if (human_is_ragdolled_or_getting_up(hp))
    {
        human_ragdoll_stop(hp, false);
    }
    vehicle_interaction_request_enter(hp, vp, &enter_data);
}

// 0x00B2B530
void Action_callback_player_squad_add_npc(script_node** args)
{
    npc* np = npc_from_handle((unsigned int)*args[0]);
    player* pp = player_get_local_player();
    if (pp != nullptr && np != nullptr)
    {
        np->set_team(pp->current_team);
        pp->ally_squad_add(np);
    }
}

// 0x00B2B5A0
void Action_callback_player_squad_add_squad_members(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    player* pp = player_get_local_player();
    if (pp == nullptr || squad == nullptr)
    {
        return;
    }
    while (squad->member_array.size() > 0 && pp->ally_squad_can_add())
    {
        const unsigned int member = squad->member_array[0];
        squad->remove_member(member);
        npc* np = World.object_from_handle<npc>(member);
        if (np != nullptr)
        {
            pp->ally_squad_add(np);
        }
    }
}

// 0x00B2B640: original quirk kept: a player with no ally squad dereferences null.
void Action_callback_player_squad_remove_all(script_node**)
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    object_squad* squad = object_of_type<object_squad>(pp->ally_squad_handle, OT_SQUAD);
    while (squad->member_array.size() > 0)
    {
        npc* np = World.object_from_handle<npc>(squad->member_array[0]);
        if (np != nullptr)
        {
            pp->ally_squad_remove(np);
        }
    }
}

// 0x00B2B6B0
void Action_callback_npc_set_team(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const human_teams team = (human_teams)(game_handle)*args[1];
    human* hp = object_of_type<human>(handle, OT_HUMAN);
    if (hp != nullptr)
    {
        hp->set_team(team);
    }
}

// 0x00B2B700
void Action_callback_npc_set_debris_pusher(script_node** args)
{
    human* hp = object_of_type<human>((unsigned int)*args[0], OT_HUMAN);
    if (hp != nullptr)
    {
        hp->hflags.pushes_debris_scripted = 1;
    }
}

// 0x00B2B740 (LTCG: the squad in EDI)
static void set_complain_flag_squad(object_squad* squad)
{
    for_each_squad_npc(squad, [](npc* np) { np->hflags.complain_when_shot = 1; });
}

// 0x00B2B790
void Action_callback_set_complaint_flag(script_node** args)
{
    object* objp = World.object_from_handle((unsigned int)*args[0]);
    if (objp == nullptr)
    {
        return;
    }
    switch (objp->get_obj_type())
    {
    case OT_HUMAN:
        ((human*)objp)->hflags.complain_when_shot = 1;
        break;
    case OT_SQUAD:
        set_complain_flag_squad((object_squad*)objp);
        break;
    case OT_SQUAD_SPAWN_NODE:
    {
        object_squad* squad = World.object_from_handle<object_squad>(((object_squad_spawn_node*)objp)->get_child_squad_handle());
        if (squad != nullptr)
        {
            set_complain_flag_squad(squad);
        }
        break;
    }
    default:
        break;
    }
}

// 0x00B2B800
void Action_callback_human_set_mission_invulnerable(script_node** args)
{
    if (!mission_is_active())
    {
        return;
    }
    const unsigned int handle = (unsigned int)*args[0];
    const bool invulnerable = (bool)*args[1];
    human* hp = object_of_type<human>(handle, OT_HUMAN);
    if (hp != nullptr)
    {
        hp->hflags.invulnerable = invulnerable;
        hp->hflags.mission_invulnerable = invulnerable;
    }
}

// 0x00B2B870: any human member, and the squad's flags for those who join later.
void Action_callback_squad_set_mission_invulnerable_on_humans(script_node** args)
{
    if (!mission_is_active())
    {
        return;
    }
    const unsigned int handle = (unsigned int)*args[0];
    const bool invulnerable = (bool)*args[1];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad == nullptr)
    {
        return;
    }
    squad->hflags.invulnerable = invulnerable;
    squad->hflags.mission_invulnerable = invulnerable;
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        human* hp = object_of_type<human>(squad->member_array[i], OT_HUMAN);
        if (hp != nullptr)
        {
            hp->hflags.invulnerable = invulnerable;
            hp->hflags.mission_invulnerable = invulnerable;
        }
    }
}

// 0x00B2B950
void Action_callback_vehicle_set_mission_invulnerable(script_node** args)
{
    if (!mission_is_active())
    {
        return;
    }
    const unsigned int handle = (unsigned int)*args[0];
    const bool invulnerable = (bool)*args[1];
    vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
    if (vp != nullptr)
    {
        vp->m_flags.m_invulnerable = invulnerable;
        vp->m_flags.m_mission_invulnerable = invulnerable;
    }
}

// 0x00B2B9D0
void Action_callback_squad_set_mission_invulnerable_on_vehicle(script_node** args)
{
    if (!mission_is_active())
    {
        return;
    }
    const unsigned int handle = (unsigned int)*args[0];
    const bool invulnerable = (bool)*args[1];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad == nullptr)
    {
        return;
    }
    squad->vflags.m_invulnerable = invulnerable;
    squad->vflags.m_mission_invulnerable = invulnerable;
    vehicle* vp = squad->get_vehicle();
    if (vp != nullptr)
    {
        vp->m_flags.m_invulnerable = invulnerable;
        vp->m_flags.m_mission_invulnerable = invulnerable;
    }
}

// Sets an npc's desired target. rfg.exe inlines it in the two callbacks below.
static void script_npc_set_desired_target(npc* np, unsigned int target)
{
    ai_system* ai = np->get_ai();
    if (ai != nullptr)
    {
        ai->flags.clear_desired_target_after_timeout = false;
        ai->desired_target = target;
    }
}

// 0x00B2BAE0
void Action_callback_set_desired_target(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int target = (unsigned int)*args[1];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || World.object_from_handle(target) == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            script_npc_set_desired_target((npc*)objp, target);
        }
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        for_each_squad_npc((object_squad*)objp, [target](npc* np) { script_npc_set_desired_target(np, target); });
    }
}

// 0x00B2BBB0
void Action_callback_clear_desired_target(script_node** args)
{
    object* objp = World.object_from_handle((unsigned int)*args[0]);
    if (objp == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            script_npc_set_desired_target((npc*)objp, 0xFFFFFFFF);
        }
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        for_each_squad_npc((object_squad*)objp, [](npc* np) { script_npc_set_desired_target(np, 0xFFFFFFFF); });
    }
}

// 0x00B2BC60: only flyers.
void Action_callback_man_flyer(script_node** args)
{
    object* objp = World.object_from_handle((unsigned int)*args[0]);
    const unsigned int vehicle_handle = (unsigned int)*args[1];
    if (objp == nullptr)
    {
        return;
    }
    vehicle* vp = object_of_type<vehicle>(vehicle_handle, OT_VEHICLE);
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER)
    {
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ((npc*)objp)->npc_flags.man_flyer = 1;
            vehicle_reserve_vehicle(vp, (npc*)objp);
        }
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        object_squad* squad = (object_squad*)objp;
        squad->npc_flags.man_flyer = 1;
        squad->squad_vehicle = vp->get_handle();
        vp->m_flags.do_not_destroy_with_squad = 1;
        for_each_squad_npc(squad, [vp](npc* np)
        {
            np->npc_flags.man_flyer = 1;
            vehicle_reserve_vehicle(vp, np);
        });
    }
}

// 0x00B2C080
void Action_callback_fire_upon_vehicles_in_way(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool fire = (bool)*args[1];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ((npc*)objp)->npc_flags.fire_on_vehicles = fire;
        }
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        ((object_squad*)objp)->npc_flags.fire_on_vehicles = fire;
        for_each_squad_npc((object_squad*)objp, [fire](npc* np) { np->npc_flags.fire_on_vehicles = fire; });
    }
}

// 0x00B2C170
void Action_callback_vehicle_ignore_vehicles_in_path(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool ignore = (bool)*args[1];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ((npc*)objp)->npc_flags.ignore_vehicles = ignore;
        }
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        ((object_squad*)objp)->npc_flags.ignore_vehicles = ignore;
        for_each_squad_npc((object_squad*)objp, [ignore](npc* np) { np->npc_flags.ignore_vehicles = ignore; });
    }
}

// 0x00B2C260: original quirk kept: any value of at most 1 becomes 1, so the mass only ever goes above 1.
void Action_callback_set_extra_mass(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool extra = (bool)*args[1];
    float value = (float)*args[2];
    vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
    if (vp != nullptr)
    {
        vp->set_extra_mass(extra);
        if (extra)
        {
            if ((float)(1.0 - (double)value) >= 0.0f)
            {
                value = 1.0f;
            }
            vp->set_extra_mass_value(value);
        }
    }
}

// 0x00B2C2F0: true marks the patrol as an EDF one.
void Action_callback_patrol_path_add_to_map(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (object_of_type<object>(handle, OT_PATROL) != nullptr)
    {
        Cartographer.add_patrol_path(handle, true);
    }
}

// 0x00B2C330
void Action_callback_patrol_path_add_to_map_guerrilla(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (object_of_type<object>(handle, OT_PATROL) != nullptr)
    {
        Cartographer.add_patrol_path(handle, false);
    }
}

// 0x00B2C370
void Action_callback_patrol_path_remove_from_map(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (object_of_type<object>(handle, OT_PATROL) != nullptr)
    {
        Cartographer.remove_patrol_path(handle);
    }
}

// 0x00B2C3B0
void Action_callback_orders_fodder(script_node** args)
{
    gfm_orders_creation_params params;
    params.ofp.reset();
    params.reset();
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad == nullptr)
    {
        return;
    }
    const unsigned int target = (unsigned int)*args[1];
    if (target == 0xFFFFFFFF || World.object_from_handle(target) == nullptr)
    {
        return;
    }
    params.location_target = squad->pos;
    params.orders_type = GFM_ORDERS_TYPE_FODDER;
    params.team = squad->get_squad_team();
    params.ofp = FODDER_ORDERS_FLAGS;
    params.object_target = target;
    const unsigned int orders = Orders_manager.create_new_orders(params);
    if (orders != 0xFFFFFFFF)
    {
        unsigned int old_orders;
        squad->reassign_orders(orders, &old_orders, true);
        Orders_manager.destroy_orders(old_orders);
    }
}

// 0x00B2C490
void Action_callback_orders_active(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool active = (bool)*args[1];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        squad->npc_flags.orders_active = active;
        for_each_squad_npc(squad, [active](npc* np) { np->npc_flags.orders_active = active; });
    }
}

// 0x00B2C550
void Action_callback_fodder_active_on_object(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    if (squad != nullptr)
    {
        squad->npc_flags.fodder_on_object = 1;
        for_each_squad_npc(squad, [](npc* np) { np->npc_flags.fodder_on_object = 1; });
    }
}

// 0x00B2C5D0: the road nearest a trigger region's position.
void Action_callback_break_or_rejoin_road(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool do_break = (bool)*args[1];
    const object* trigger = object_of_type<object>(handle, OT_TRIGGER_REGION);
    if (trigger == nullptr)
    {
        return;
    }
    if (do_break)
    {
        object_road_break_closest_road_at_point(trigger->pos);
    }
    else
    {
        object_road_rejoin_closest_road_at_point(trigger->pos);
    }
}

// 0x00B2C630: Magic number: 60 metres, the search radius.
void Action_callback_change_ambient_flag_on_road(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool ambient = (bool)*args[1];
    const object* trigger = object_of_type<object>(handle, OT_TRIGGER_REGION);
    if (trigger != nullptr)
    {
        object_path_road* road = object_road_get_closest_road(trigger->pos, true, 60.0f, false, false);
        if (road != nullptr)
        {
            road->road_flags.ambient = ambient;
        }
    }
}

// 0x00B2C6B0
void Action_callback_allow_use_of_special_roads(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool allow = (bool)*args[1];
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OT_HUMAN)
    {
        if (objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            ((npc*)objp)->npc_flags.can_use_special_roads = allow;
        }
    }
    else if (objp->get_obj_type() == OT_SQUAD)
    {
        ((object_squad*)objp)->npc_flags.can_use_special_roads = allow;
        for_each_squad_npc((object_squad*)objp, [allow](npc* np) { np->npc_flags.can_use_special_roads = allow; });
    }
}

// 0x00B2C790: opens the bomber's doors (over 1 second) and arms its leader, or closes and disarms.
void Action_callback_bombing_run(script_node** args)
{
    object_squad* squad = object_of_type<object_squad>((unsigned int)*args[0], OT_SQUAD);
    const bool bomb = (bool)*args[1];
    if (squad == nullptr)
    {
        return;
    }
    vehicle* vp = squad->get_vehicle();
    if (vp == nullptr || vp->get_sub_type() != OT_SUB_VEHICLE_FLYER || !vehicle_is_bomber(vp) || !vp->rdd.is_loaded())
    {
        return;
    }
    for (unsigned int i = 0; i < vp->rdd.get_data()->m_num_components; i++)
    {
        vehicle_component* component = &vp->rdd.get_data()->m_components[i];
        if (component->get_type() == VCT_DOOR)
        {
            if (bomb)
            {
                vehicle_component_open_door(component, vp, 1.0f);
            }
            else
            {
                vehicle_component_close_door(component, vp, 1.0f);
            }
        }
    }
    npc* leader = squad->get_leader();
    if (leader == nullptr)
    {
        return;
    }
    leader->npc_flags.bomb = bomb;
    ai_system* ai = leader->get_ai();
    if (ai != nullptr)
    {
        if (bomb)
        {
            leader->npc_flags.going_left = 1;
            ai->bomb_timer.set(1000); // Magic number: 1 second
        }
        else
        {
            ai->bomb_timer.invalidate();
        }
    }
}

// 0x00B2C8D0: above ground when the third argument says so, else an absolute height.
// Original quirk kept: with no vehicle, this moves the squad object itself.
void Action_callback_change_flyer_patrol_height(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const float height = (float)*args[1];
    const bool above_ground = (bool)*args[2];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad == nullptr)
    {
        return;
    }
    vehicle* vp = squad->get_vehicle();
    if (vp == nullptr)
    {
        float ground = 0.0f;
        havok_terrain_height(ground, squad->pos.x, squad->pos.z, true, nullptr);
        squad->pos.y = above_ground ? ground + height : height;
        return;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER)
    {
        float ground = 0.0f;
        havok_terrain_height(ground, vp->pos.x, vp->pos.z, true, nullptr);
        vp->vai.original_offset = above_ground ? height : height - ground;
        vp->vai.desired_flying_offset = height;
    }
}

// 0x00B2CA30: unlike the callbacks around it, this one takes only a squad, not a single npc.
void Action_callback_drive_disabled(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool disabled = (bool)*args[1];
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        squad->npc_flags.drive_disabled = disabled;
        for_each_squad_npc(squad, [disabled](npc* np) { np->npc_flags.drive_disabled = disabled; });
    }
}

// 0x00B2CAF0: links keep the lowest set id of the two and spread it. Unlinking gives the second point a new set, unless a
// point has no links left.
void Action_callback_link_nav_points(script_node** args)
{
    const unsigned int handle1 = (unsigned int)*args[0];
    const unsigned int handle2 = (unsigned int)*args[1];
    const bool link = (bool)*args[2];
    navpoint* navpoint1 = object_of_type<navpoint>(handle1, OT_NAVPOINT);
    navpoint* navpoint2 = object_of_type<navpoint>(handle2, OT_NAVPOINT);
    if (navpoint1 == nullptr || navpoint2 == nullptr)
    {
        return;
    }
    if (link)
    {
        for (int i = 0; i < navpoint1->num_links; i++)
        {
            if (navpoint1->links[i] == navpoint2)
            {
                return;
            }
        }
        link_navpoints(navpoint1, navpoint2, true);
        navpoint* source = nullptr;
        unsigned short set = 0;
        if (navpoint1->set != 0)
        {
            source = navpoint1;
            set = navpoint1->set;
        }
        if (navpoint2->set != 0 && (set == 0 || navpoint2->set < set))
        {
            source = navpoint2;
            set = navpoint2->set;
        }
        if (set == 0)
        {
            navpoint1->set = get_next_navpoint_set_id();
            source = navpoint1;
        }
        source->propogate_set_info();
        return;
    }
    remove_navpoint_link(navpoint1, navpoint2);
    if (navpoint1->num_links == 0)
    {
        navpoint1->set = 0;
    }
    if (navpoint2->num_links == 0)
    {
        navpoint2->set = 0;
    }
    if (navpoint1->set != 0 && navpoint2->set != 0)
    {
        navpoint2->set = get_next_navpoint_set_id();
        navpoint2->propogate_set_info();
    }
}

// 0x00B2CC60
void Action_callback_enable_nav_cells(script_node** args)
{
    object* box = object_of_type<object>((unsigned int)*args[0], OT_BOUNDING_BOX);
    if (box != nullptr)
    {
        vector bmin;
        vector bmax;
        box->get_local_bbox(bmin, bmax);
        bb_pf_enable_nav_cells(bmin, bmax, box->pos, box->orient);
    }
}

// Magic number: -1, no alias on the action rfg.exe plays on the victim.
static const vanim_aliases SCRIPT_ANIM_ALIAS_NONE = (vanim_aliases)-1;

// 0x00B2CCC0: kills the npc as if a gauss rifle shot came from the object: sound, sparks, tracer, decals and ragdoll.
void Action_callback_npc_kill_with_phantom_bullet(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int shooter_handle = (unsigned int)*args[1];
    human* hp = npc_from_handle(handle);
    const object* shooter = World.object_from_handle(shooter_handle);
    if (hp == nullptr)
    {
        return;
    }
    vector start = ZERO_VECTOR;
    if (shooter != nullptr)
    {
        start = shooter->pos;
    }
    vector head;
    human_get_head_pos(hp, head);
    vector direction(head.x - start.x, head.y - start.y, head.z - start.z);
    direction.normalize_safe();
    vector hit_pos = head;
    human_get_hit_location(hp, start, direction, hit_pos, 0.0f, nullptr, nullptr);
    hp->hflags.invulnerable = 0;
    human_damage(hp, nullptr, -1.0f, DBT_NONE, nullptr, nullptr, true, false);
    const weapon_info* w_info = weapons_get_weapon_info("gauss_rifle");
    if (w_info == nullptr)
    {
        return;
    }
    audiolib_cue_play_3d(w_info->npc_fire_sounds[0], &start, nullptr, nullptr, nullptr);
    const vector backwards(-direction.x, -direction.y, -direction.z);
    bullet_spark_create(hit_pos, backwards, w_info, hp->get_effect_material(0xFFFFFFFF, nullptr, nullptr), MATERIAL_EVENT_SHAPE_SOLID, hp, 0xFFFFFFFF, -1.0f);
    tracers_create(hp->get_handle(), start, hit_pos, w_info->tracer_effect, WEAPON_TRACER_DEFAULT);
    object_decal_create_bullet_impact_decal(hp, player_get_local_player(), w_info, hit_pos, backwards, 0.0f, nullptr);
    object_decal_create_blood_splatter_decal(hp, player_get_local_player(), hit_pos, direction, true, DBT_NONE);
    vanim_play_action(hp->char_inst->vp, index_alias(ANIMATION_ACTION_DANS_DEATH, SCRIPT_ANIM_ALIAS_NONE), 0x40, 1.0f, 1.0f);
    human_ragdoll_on_impact(hp, 10.0f, 0.1f, false);
}

// 0x00B2CFD0
void Action_callback_activity_enable_specific(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    for (int i = 0; list[i] != HANDLE_LIST_END; i++)
    {
        object_house_arrest_node* node = object_of_type<object_house_arrest_node>(list[i], OT_HOUSE_ARREST_NODE);
        if (node != nullptr)
        {
            node->enable();
        }
    }
}

// 0x00B2D020: Magic number: rfg.exe's stack holds 3 handles.
// Original quirks kept: the count takes in entries that fail, leaving those slots unset, and the list is not bounded.
void Action_callback_mission_hostage_icons_show(script_node** args)
{
    const unsigned int* list = (unsigned int*)*args[0];
    unsigned int hostages[3];
    int count = 0;
    for (; list[count] != HANDLE_LIST_END; count++)
    {
        const object_squad_spawn_node* node = object_of_type<object_squad_spawn_node>(list[count], OT_SQUAD_SPAWN_NODE);
        if (node != nullptr)
        {
            const object_squad* squad = object_of_type<object_squad>(node->get_child_squad_handle(), OT_SQUAD);
            if (squad != nullptr && squad->member_array.size() > 0)
            {
                hostages[count] = squad->member_array[0];
            }
        }
    }
    hud_ui_missions_set_hostages(count, hostages);
}

// 0x00B31880
void Action_callback_mission_add_ground_marker_offset(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const float diameter = (float)*args[1];
    vector offset;
    offset.x = (float)*args[2];
    offset.y = 0.0f;
    offset.z = (float)*args[3];
    if (World.object_from_handle(handle) != nullptr)
    {
        objective_highlight_ground_marker_add(handle, diameter, offset, false, true);
    }
}

// 0x00B31900
void Action_callback_mission_add_ground_marker_action_node(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const float diameter = (float)*args[1];
    if (World.object_from_handle(handle) != nullptr)
    {
        objective_highlight_ground_marker_add(handle, diameter, ZERO_VECTOR, true, false);
    }
}

// 0x00B31950
void Action_callback_mission_remove_ground_marker(script_node** args)
{
    objective_highlight_ground_marker_remove((unsigned int)*args[0]); // rfg.exe inlines it
}

// 0x00B31970
void Action_callback_disable_convoys_except(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const int count = World.get_num_objects<object_convoy_end_point>();
    for (int i = 0; i < count; i++)
    {
        object_convoy_end_point* end_point = World.get_object<object_convoy_end_point>(i);
        if (end_point->get_handle() == handle)
        {
            end_point->enable_convoy();
        }
        else
        {
            end_point->disable_convoy();
        }
    }
}

// 0x00B319E0
void Action_callback_disable_convoy(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool suspend = (bool)*args[1];
    const int count = World.get_num_objects<object_convoy_end_point>();
    for (int i = 0; i < count; i++)
    {
        object_convoy_end_point* end_point = World.get_object<object_convoy_end_point>(i);
        if (end_point->get_handle() == handle)
        {
            if (suspend)
            {
                end_point->suspend_convoy();
            }
            else
            {
                end_point->release_convoy();
            }
        }
    }
}

// 0x00B31A60
void Action_callback_disable_all_convoys(script_node** args)
{
    const bool disabled = (bool)*args[0];
    const int count = World.get_num_objects<object_convoy_end_point>();
    for (int i = 0; i < count; i++)
    {
        object_convoy_end_point* end_point = World.get_object<object_convoy_end_point>(i);
        if (disabled)
        {
            end_point->disable_convoy();
        }
        else
        {
            end_point->enable_convoy();
        }
    }
}

// 0x00B217D0: the object and the children of the same type each get the list. Recursive.
static void register_restorable_object_list(unsigned int handle, const farray<unsigned int,800>& list)
{
    restore_list_create(handle, list);
    const object* objp = World.object_from_handle(handle);
    if (objp == nullptr)
    {
        return;
    }
    for (const object* child = objp->child_ptr; child != nullptr; )
    {
        if (child->get_obj_type() == objp->get_obj_type())
        {
            register_restorable_object_list(child->get_handle(), list);
        }
        child = child->child_next;
        if (child == objp->child_ptr)
        {
            break;
        }
    }
}

// 0x00B31B00 (LTCG: the handles in ECX): rfg.exe's overload builds the list without duplicates and inlines the one above.
static void register_restorable_object_list(unsigned int handle, const unsigned int* handles)
{
    farray<unsigned int,800> list;
    for (; *handles != HANDLE_LIST_END; handles++)
    {
        if (list.size() >= list.capacity())
        {
            break;
        }
        bool found = false;
        for (int i = 0; i < list.size() && !found; i++)
        {
            found = list[i] == *handles;
        }
        if (!found)
        {
            list.add(*handles);
        }
    }
    register_restorable_object_list(handle, list);
}

// 0x00B31C10
void Action_callback_mission_restore_object(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int* handles = (unsigned int*)*args[1];
    if (handle != 0xFFFFFFFF)
    {
        register_restorable_object_list(handle, handles);
    }
}

// 0x00B31C40
void Action_callback_activity_restore_object(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const unsigned int* handles = (unsigned int*)*args[1];
    if (handle != 0xFFFFFFFF)
    {
        register_restorable_object_list(handle, handles);
    }
}

// 0x00B31C70: at the script point of a building chunk that has any, else at the object.
void Action_callback_explosion_spawn_at_script_point(script_node** args)
{
    const script_point point = (script_point)*args[0];
    const explosion_info* info = explosion_lookup((unsigned int)(game_handle)*args[1]);
    if (info == nullptr)
    {
        return;
    }
    vector pos;
    matrix orient;
    object_mover* mover = object_of_type<object_mover>(point.ref_handle, OT_MOVER);
    object* owner = mover;
    if (mover == nullptr)
    {
        owner = World.object_from_handle(point.ref_handle);
        if (owner == nullptr)
        {
            return;
        }
        pos = owner->pos;
        orient = owner->orient;
    }
    else
    {
        const c_chunk_base* chunk = chunk_get_base_by_uid(mover->chunk_uid);
        if (chunk == nullptr || point.index < 0 || (point.index >= chunk->num_script_points && chunk->num_script_points != 0))
        {
            return;
        }
        if (chunk->num_script_points == 0)
        {
            pos = owner->pos;
            orient = owner->orient;
        }
        else
        {
            vector local_pos;
            matrix local_orient;
            chunk->script_points[point.index].transform.get_translation(local_pos);
            chunk->script_points[point.index].transform.get_orientation(local_orient);
            pos.x = owner->pos.x + owner->orient.rvec.x * local_pos.x + owner->orient.uvec.x * local_pos.y + owner->orient.fvec.x * local_pos.z;
            pos.y = owner->pos.y + owner->orient.rvec.y * local_pos.x + owner->orient.uvec.y * local_pos.y + owner->orient.fvec.y * local_pos.z;
            pos.z = owner->pos.z + owner->orient.rvec.z * local_pos.x + owner->orient.uvec.z * local_pos.y + owner->orient.fvec.z * local_pos.z;
            orient = local_orient * owner->orient;
        }
    }
    explosion_create(info, nullptr, owner, pos, orient, nullptr, nullptr, false);
}

// 0x00B320C0: in MISSION_MODE_WMR, a vehicle handle in the second or third argument becomes the handle of its squad.
void Action_callback_mission_set_unique_data(script_node** args)
{
    if (!mission_is_active())
    {
        return;
    }
    const unsigned int handle1 = (unsigned int)*args[0];
    unsigned int handle2 = (unsigned int)*args[1];
    unsigned int handle3 = (unsigned int)*args[2];
    const float value1 = (float)*args[3];
    const float value2 = (float)*args[4];
    if (mission_get_mode() == MISSION_MODE_WMR)
    {
        const int count = World.get_num_objects<object_squad>();
        for (int i = 0; i < count; i++)
        {
            const object_squad* squad = World.get_object<object_squad>(i);
            if (squad->squad_vehicle == handle2)
            {
                handle2 = squad->get_handle();
                break;
            }
        }
        for (int i = 0; i < count; i++)
        {
            const object_squad* squad = World.get_object<object_squad>(i);
            if (squad->squad_vehicle == handle3)
            {
                handle3 = squad->get_handle();
                break;
            }
        }
    }
    mission_set_data(handle1, handle2, handle3, value1, value2);
}

// 0x00B321A0
void Action_callback_squad_edf_despawn_all(script_node**)
{
    const int count = World.get_num_objects<object_squad>();
    for (int i = 0; i < count; i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->get_squad_team() == HUMAN_TEAM_EDF)
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_HIGH, nullptr, true);
        }
    }
}

// 0x00B32260: a squad, or the vehicle of a squad. The flag on the squad and the vehicle's own stay in step.
void Action_callback_player_set_vehicle_disabled(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool disabled = (bool)*args[1];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        squad->vflags.disable_for_player = disabled;
        vehicle* vp = object_of_type<vehicle>(squad->squad_vehicle, OT_VEHICLE);
        if (vp != nullptr)
        {
            vehicle_set_player_disabled(vp, disabled);
        }
        return;
    }
    vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
    if (vp == nullptr)
    {
        return;
    }
    vehicle_set_player_disabled(vp, disabled);
    const int count = World.get_num_objects<object_squad>();
    for (int i = 0; i < count; i++)
    {
        object_squad* holder = World.get_object<object_squad>(i);
        if (holder->squad_vehicle == handle)
        {
            holder->vflags.disable_for_player = disabled;
            return;
        }
    }
}

// 0x00B32370: Magic number: 100, the size of the squad list on the stack.
void Action_callback_drive_to_location_vrail(script_node** args)
{
    const unsigned int location = (unsigned int)*args[0];
    const bool drive_directly = (bool)*args[1];
    if (World.object_from_handle(location) == nullptr)
    {
        return;
    }
    farray<object_squad*,100> squads;
    vrail_get_squads(squads);
    for (int i = 0; i < squads.size(); i++)
    {
        if (squads[i] == nullptr || !script_squad_drive(squads[i], location, drive_directly, true))
        {
            return;
        }
    }
    vrail_stop_squad_spawns();
}

// 0x00B324E0: same shape as Action_callback_player_set_vehicle_disabled.
void Action_callback_set_infinite_mass(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    const bool set = (bool)*args[1];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object_squad* squad = object_of_type<object_squad>(handle, OT_SQUAD);
    if (squad != nullptr)
    {
        squad->set_infinite_mass(set);
        vehicle* vp = object_of_type<vehicle>(squad->squad_vehicle, OT_VEHICLE);
        if (vp != nullptr)
        {
            vp->set_infinite_mass(set);
        }
        return;
    }
    vehicle* vp = object_of_type<vehicle>(handle, OT_VEHICLE);
    if (vp == nullptr)
    {
        return;
    }
    vp->set_infinite_mass(set);
    const int count = World.get_num_objects<object_squad>();
    for (int i = 0; i < count; i++)
    {
        object_squad* holder = World.get_object<object_squad>(i);
        if (holder->squad_vehicle == handle)
        {
            holder->set_infinite_mass(set);
            return;
        }
    }
}

// Shows every object of the class that is in the district. rfg.exe inlines this walk.
template<typename T>
static void script_show_objects_over_fow(const district* dp)
{
    const int count = World.get_num_objects<T>();
    for (int i = 0; i < count; i++)
    {
        const T* objp = World.get_object<T>(i);
        if (district_get_from_pos(objp->pos) == dp)
        {
            Cartographer.object_set_visible_over_fow(objp->get_handle(), true);
        }
    }
}

// 0x00B32710: the district's important movers, activities and EDF raids.
void Action_callback_fog_of_war_show_important_buildings(script_node** args)
{
    const unsigned int handle = get_district_by_name((const char*)*args[0]);
    const district* dp = object_of_type<district>(handle, OT_DISTRICT);
    if (dp == nullptr)
    {
        return;
    }
    // Magic numbers: importance 2, the raw value rfg.exe compares; 2070, its limit on z, keeps out the movers beyond it
    // whatever the district.
    const int num_movers = World.get_num_objects<object_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        const object_mover* mover = World.get_object<object_mover>(i);
        if ((int)mover->gameplay_props->importance == 2 && district_get_from_pos(mover->pos) == dp && mover->pos.z <= 2070.0f)
        {
            Cartographer.object_set_visible_over_fow(mover->get_handle(), true);
        }
    }
    script_show_objects_over_fow<object_riding_shotgun_node>(dp);
    script_show_objects_over_fow<object_demolitions_master_node>(dp);
    script_show_objects_over_fow<object_delivery_node>(dp);
    const int num_spawns = World.get_num_objects<object_activity_spawn>();
    for (int i = 0; i < num_spawns; i++)
    {
        const object_activity_spawn* spawn = World.get_object<object_activity_spawn>(i);
        if ((spawn->get_activity_type() == OT_RAID_NODE || spawn->get_activity_type() == OT_HOUSE_ARREST_NODE) && district_get_from_pos(spawn->pos) == dp)
        {
            Cartographer.object_set_visible_over_fow(spawn->get_handle(), true);
        }
    }
    const int num_raids = World.get_num_objects<object_raid_node>();
    for (int i = 0; i < num_raids; i++)
    {
        const object_raid_node* raid = World.get_object<object_raid_node>(i);
        if (district_get_from_pos(raid->pos) == dp && raid_get_defending_team(raid->base_raid_def) == HUMAN_TEAM_EDF)
        {
            Cartographer.object_set_visible_over_fow(raid->get_handle(), true);
        }
    }
    script_show_objects_over_fow<object_area_defense_node>(dp);
}

// 0x00B38370
void Action_callback_complete_objective(script_node** args)
{
    const int handle = (int)(game_handle)*args[0];
    complete_objective(handle, true);
    objective_complete_event data;
    data.handle = handle;
    gem_post_event(data);
}

// 0x00B383B0
void Action_callback_complete_objective_silent(script_node** args)
{
    const int handle = (int)(game_handle)*args[0];
    complete_objective(handle, false);
    objective_complete_event data;
    data.handle = handle;
    gem_post_event(data);
}

// 0x00B38430
void Action_callback_complete_mission_objective_silent(script_node** args)
{
    const int handle = (int)(game_handle)*args[0];
    complete_objective(handle, false);
    mission_objective_complete_event data;
    data.handle = handle;
    gem_post_event(data);
}

// 0x00B384E0
void Action_callback_mission_failure(script_node**)
{
    if (mission_is_active())
    {
        mission_failure_event data;
        data.desc = nullptr;
        gem_post_event(data);
    }
}

// 0x00B386F0: the time offset is in seconds, multiplied as a double as rfg.exe does.
void Action_callback_add_dialogue_line_on_persona(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    const human_voice_persona* persona = human_voice_persona_from_handle((human_voice_persona_handle)(game_handle)*args[1]);
    const voice_line_handle parent = (voice_line_handle)(game_handle)*args[2];
    const int time_offset = (int)((double)(float)*args[3] * 1000.0);
    dialogue_manager_add_line(line, persona, parent, time_offset, INVALID_PA_HANDLE, nullptr);
}

// 0x00B38750
void Action_callback_add_dialogue_line_on_pa(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    const human_voice_persona* persona = human_voice_persona_from_handle((human_voice_persona_handle)(game_handle)*args[1]);
    const public_address_group_handle group = (public_address_group_handle)(game_handle)*args[2];
    const voice_line_handle parent = (voice_line_handle)(game_handle)*args[3];
    const int time_offset = (int)((double)(float)*args[4] * 1000.0);
    dialogue_manager_add_line(line, persona, parent, time_offset, group, nullptr);
}

// 0x00B3D2B0: the time is in seconds.
void Action_callback_add_time_to_timer(script_node** args)
{
    const unsigned int handle = (unsigned int)*args[0];
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    const int ms = (int)((double)(float)*args[1] * 1000.0);
    if (handle < Script_timers.get_capacity())
    {
        script_timer_set(handle, Script_timers.get_entry((int)handle)->get_time() + ms);
    }
}

// 0x00B3D520
void Action_callback_play_voice_line_on_pa(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    human_voice_persona* persona = human_voice_persona_from_handle((human_voice_persona_handle)(game_handle)*args[1]);
    public_address_start_dialogue(line, persona, (public_address_group_handle)(game_handle)*args[2]);
}

// 0x00B3D560
void Action_callback_briefing_start_voice(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    const human_voice_persona* persona = human_voice_persona_from_handle((human_voice_persona_handle)(game_handle)*args[1]);
    dialogue_manager_start_dialogue(line, persona, DIALOGUE_PRIORITY_BRIEFING, INVALID_PA_HANDLE, nullptr);
}

// 0x00B3D5A0
void Action_callback_stop_dialogue(script_node** args)
{
    const voice_line_handle line = (voice_line_handle)(game_handle)*args[0];
    dialogue_manager_cancel(line, (bool)*args[1]);
}

// ---- registration

#include "volition/rfg/code/scripting/action_script/script_definition_table.inl"
