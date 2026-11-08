// Original: volition/rfg/code/gameplay/gameplay.cpp
//
// The single player game's top level (stub_impl.md S9): start-up of the spawn, squad, mission and activity systems and
// their per-level and per-frame calls. Part of rfg_gameplay; the systems still unported are stand-ins that do nothing,
// so a new game runs with an empty world until they land.

#include "volition/rfg/code/gameplay/gameplay.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ambient_action_manager.h"
#include "volition/rfg/code/ai/ambient_edf_action_manager.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_raid_internal.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator_internal.h"
#include "volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.h"
#include "volition/rfg/code/gameplay/activities/morale_effect_manager.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/continuous_support.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_structure_defense_coordinator.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/gameflow/reinforcement_location.h"
#include "volition/rfg/code/gameplay/gameflow/squad_loadout.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression.h"
#include "volition/rfg/code/gameplay/gameflow/squad_progression_internal.h"
#include "volition/rfg/code/gameplay/gameflow/squad_spawn_manager.h"
#include "volition/rfg/code/gameplay/managed_layer_loading.h"
#include "volition/rfg/code/gameplay/marauder_raid.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/restore_list.h"
#include "volition/rfg/code/gameplay/road_distance_info.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/vehicle_drop_off.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/objectives.h"
#include "volition/rfg/code/interface/ui_document_hud_wrecking_crew.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_mission_start_node.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_roadblock_node.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/spawn/ambient_prop_info.h"
#include "volition/rfg/code/spawn/ambient_spawn_behavior.h"
#include "volition/rfg/code/spawn/ambient_spawn_info.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/util/varray.h"

// ---- internal functions of alert_level_manager.cpp and marauder_raid.cpp, which rfg.pdb lists as local
void load_alert_level_ranges(); // 0x00772F90 (alert_level_manager.cpp:669)
void load_alert_values(); // 0x0075D4D0 (alert_level_manager.cpp:758)
void cancel_current_raid(); // 0x0079D630 (marauder_raid.cpp:144)
void do_frame_spawn_new_raid(); // (marauder_raid.cpp)
void do_frame_spawn_selected_raid(); // (marauder_raid.cpp)
void build_road_info(); // 0x00790120 (road_distance_info.cpp:251)


// ---- public globals (their owners read them from these until their files are ported)
extern unsigned int Activity_node_start_on_load; // 0x01651244 (activity.cpp)
extern activity_base* All_activities[17]; // 0x01651248 (activity.cpp)
// Deviation from rfg.exe: the raid handles are external, because gameplay_do_frame inlines the raid test,
// which reads them through its unity build.
extern unsigned int Raid_patrol_handle; // 0x016514F0 (marauder_raid.cpp)
extern unsigned int Raid_squad_handle; // 0x016514F4 (marauder_raid.cpp)
mission_handle Mission_active_index = INVALID_MISSION_HANDLE; // 0x0165150C
farray<human_info const*, 8> Common_guerrilla_list; // 0x01657800
extern farray<road_distance_info, 256> Master_road_info_list; // road_distance_info.cpp
extern farray<unsigned int, 32> All_npcs_converted; // 0x01657318 (morale_effect_manager.cpp)
extern timestamp Spontaneous_conversion_cooldown; // 0x016573A8 (morale_effect_manager.cpp)
extern bool Mission_draft_mode; // 0x022295FF (morale_effect_manager.cpp)
extern gfm_alert_levels Current_alert_level; // 0x022278B4 (alert_level_manager.cpp)
squad_progress_info Guerrilla_progression; // 0x0224CAE0
bool Vehicle_ai_panic_drivers; // 0x01D67DCE
extern bool Dialogue_disable_dialogue_start; // 0x0361141D (dialogue_manager.cpp)
gfm_orders_manager Orders_manager; // 0x0222DE40
gfm_smash_building_coordinator Smash_building_coordinator; // 0x016572EC

// ---- file-static globals
static bool Gameplay_system_init; // 0x02229649
static int* const Gameplay_sfx[NUM_GAMEPLAY_SFX] = { // 0x01651480
    &Ga_cue_ids.wrecking_crew_weapon_fired,
    &Ga_cue_ids.wrecking_crew_red_barrel_destroyed,
    &Ga_cue_ids.wrecking_crew_blue_barrel_destroyed,
    &Ga_cue_ids.wrecking_crew_round_over,
    &Ga_cue_ids.wrecking_crew_round_won_stamp,
    &Ga_cue_ids.wrecking_crew_match_won,
    &Ga_cue_ids.wrecking_crew_countdown_number,
    &Ga_cue_ids.wrecking_crew_countdown_go,
    &Ga_cue_ids.timer_clock_appears,
    &Ga_cue_ids.timer_one_minute_remains,
    &Ga_cue_ids.timer_ten_seconds_remains,
    &Ga_cue_ids.timer_final_seconds_remains,
};

// 0x007B7180 (:119)
void gameplay_system_init()
{
    if (Gameplay_system_init)
    {
        return;
    }
    Gameplay_system_init = true;
    sog_system_init();
    amb_prop_system_init();
    ambient_behavior_system_init();
    amb_spawn_info_system_init(-1);
    srm_system_init();
    squad_loadout_system_init(-1);
    squad_progression_system_init(-1);
    squad_definitions_init(-1);
    action_node_system_init();
    roadblock_system_init();
    load_alert_level_ranges();
    load_alert_values();
    managed_layer_system_init();
    mission_info_system_init();
    voice_line_unlocking_system_init();
    activity_system_init();
    amb_spawn_system_init();
    gfm_tech_level_init();
    rfc_system_init(-1);
    // difficulty_system_init, inlined.
    difficulty_system_init();
    upgrade_system_init(-1);
    hud_ui_wrecking_crew_system_init();
}

// 0x0075E5D0 (:186)
void gameplay_system_shutdown()
{
    if (!Gameplay_system_init)
    {
        return;
    }
    Gameplay_system_init = false;
    for (int i = 0; i < 17; i++)
    {
        if (All_activities[i] != nullptr)
        {
            All_activities[i]->system_shutdown();
        }
    }
}

// 0x0079D540 (:196)
void gameplay_level_shutdown()
{
    activity_system_level_shutdown();
    managed_layer_level_shutdown();
    srm_level_shutdown();
}

// 0x007B4760 (:208)
void gameplay_level_init_pre()
{
    keen::debug::nativePrintArgs("level_init_pre()\n");
    human_teams_reset_all_dispositions();
    amb_spawn_info_build_common_npc_list(Common_guerrilla_list);
    Guerrilla_progression.last_member_used = int_rand_range(0, human_info_get_master_list().size() - 1);
    object_spawn_node_level_init();
    srm_level_init_pre();
    gsm_level_init_pre();
    restore_list_level_init(); // rfg.exe inlines it
    All_manager_layer_objects.clear();
    mission_info_level_init_pre();
    activity_system_level_init_pre();
    safehouse_vip_level_init_pre(); // inlined in rfg.exe
    gfm_level_init_pre();
    roadblock_level_init_pre();
    amb_spawn_level_init_pre();
    All_npcs_converted.clear();
    Mission_draft_mode = false;
    // Magic numbers: a second.
    Spontaneous_conversion_cooldown.set_rand(1000, 1000);
    objectives_level_init();
    restricted_area_level_init();
    hud_ui_handbook_level_init_pre();
}

// 0x007B8A60 (:281)
void gameplay_level_init_post()
{
    convoy_end_find_end_points();
    marauder_raid_level_init_post();
    obj_activity_spawn_level_init_post();
    activity_system_level_init_post();
    object_mission_start_node_level_init();
    Master_road_info_list.clear();
    build_road_info();
    srm_level_init_post();
    object_spawn_node_level_init_post();
    amb_spawn_level_init_post();
    safehouse_vip_level_init_post();
    amb_action_manager_level_init_post();
    amb_edf_action_manager_level_init_post();
    amb_prop_level_init_post();
    mission_evaluate_final_mission_complete();
    Vehicle_ai_panic_drivers = false;
    Dialogue_disable_dialogue_start = false;
}

// 0x0075E600 (:313)
void gameplay_level_init_spawn()
{
    object_spawn_do_spawn();
    amb_spawn_level_init_spawn();
}

// 0x007B97F0 (:323)
void gameplay_do_frame()
{
    object_spawn_node_do_frame();
    srm_do_frame();
    gsm_do_frame();
    // Raids run below alert level 3, outside missions.
    if (((int)Current_alert_level < 0 || (int)Current_alert_level < 3) && Mission_active_index == INVALID_MISSION_HANDLE)
    {
        if (Raid_patrol_handle == 0xFFFFFFFF)
        {
            do_frame_spawn_new_raid();
        }
        else if (Raid_squad_handle == 0xFFFFFFFF)
        {
            do_frame_spawn_selected_raid();
        }
    }
    else
    {
        cancel_current_raid();
    }
    activity_system_do_frame(false);
    if (!Multiplayer && !cutscene_is_playing())
    {
        sdc_do_frame();
        reinforcement_loc_do_frame();
        rfc_do_frame();
        rfc_render_minimap();
        cont_support_do_frame();
        Orders_manager.do_frame();
        vehicle_drop_off_do_frame();
        ssm_do_frame();
        Smash_building_coordinator.do_frame();
    }
    object_mission_start_node_do_frame();
    roadblock_do_frame();
    morale_effect_do_frame();
    amb_prop_do_frame();
    action_node_do_frame();
    amb_spawn_do_frame();
    amb_action_manager_do_frame();
    amb_edf_action_manager_do_frame();
}

// 0x007746E0 (:372)
bool gameplay_should_run_stress()
{
    if (Multiplayer && !multi_game_destruction_should_run_stress())
    {
        return false;
    }
    // rfg.exe reads Wrecking_crew_state (0x01651888) and Wrecking_crew_options.stress_mode (0x016518E8) inline here.
    if (wrecking_crew_get_state() != WCS_NONE && Wrecking_crew_options.stress_mode == WCSS_OFF)
    {
        return false;
    }
    return true;
}

// 0x00774710 (:387)
bool gameplay_can_player_attack(const weapon_info* w_info, damaged_by_types attack_type)
{
    player* pp = player_get_local_player();
    if (attack_type == DBT_MELEE && pp != nullptr && pp->backpack != nullptr && pp->backpack->ramp_up_timer.valid() && pp->backpack->backpack_info->active_abilities.rhino)
    {
        return false;
    }
    if (wrecking_crew_get_state() != WCS_NONE && !wrecking_crew_can_player_attack(w_info, attack_type))
    {
        return false;
    }
    if (pp != nullptr && Multiplayer && !multi_gameplay_mode_player_can_attack(pp, w_info, attack_type))
    {
        return false;
    }
    return true;
}

// 0x007747A0 (:410)
void gameplay_register_player_attack(player* pp, const weapon_info* w_info, damaged_by_types attack_type, bool detonating_charges)
{
    if (wrecking_crew_get_state() != WCS_NONE)
    {
        wrecking_crew_register_attack(w_info, attack_type, detonating_charges);
    }
    if (pp->backpack != nullptr)
    {
        pp->backpack->event_player_attacked(attack_type);
    }
    if (Multiplayer)
    {
        player_set_player_invulnerable(pp, false);
    }
}

// 0x0075E610 (:427)
void gameplay_play_sfx(gameplay_sfx sfx)
{
    if (sfx < NUM_GAMEPLAY_SFX && *Gameplay_sfx[sfx] != -1)
    {
        audiolib_cue_play(*Gameplay_sfx[sfx], nullptr, nullptr);
    }
}
