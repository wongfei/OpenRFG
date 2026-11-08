// Original: volition/rfg/code/gameplay/missions.cpp
//
// The active story mission: activation (autosave, layers, voices, reservations, scripts), per frame state and
// termination.

#include "volition/rfg/code/gameplay/missions.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/camera/camera_satellite.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/marauder_raid.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack.h"
#include "volition/rfg/code/gameplay/activities/activity_mission_vrail.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/continuous_support.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/managed_layer_loading.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/restore_list.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gamewide/game_time_of_day.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/hud_ui_district_liberation.h"
#include "volition/rfg/code/hud/hud_ui_mission_complete.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/critical_area.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_mission_start_node.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_upgrade_node.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/scripting/action_script/action_script.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/spawn/ambient_spawn_manager.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"

// gameplay.cpp
extern mission_handle Mission_active_index; // 0x0165150C
extern gfm_alert_levels Current_alert_level; // 0x022278B4
extern bool Mission_draft_mode; // 0x022295FF
extern bool Vehicle_ai_panic_drivers; // 0x01D67DCE
extern int EDF_locked_mission_progression_level; // 0x02229650 (squad_progression.cpp)
extern bool Dialogue_disable_dialogue_start; // 0x0361141D (dialogue_manager.cpp)

bool Unlock_all_missions; // 0x0222966D
bool Mission_parker_liberation_warp;
bool Disable_reinforcement_spawn; // 0x022295CB
static int Mission_saved_checkpoint = -1; // 0x01651500
static unsigned int Mission_warp_obj_after_unlocks = 0xFFFFFFFF; // 0x01651504
static unsigned int Mission_checkpoint_start_handle = 0xFFFFFFFF; // 0x01651508
static mission_handle Mission_checkpoint_restore_index = INVALID_MISSION_HANDLE; // 0x01651510
static mission_data Mission_active_data; // 0x02229578
static bool Allow_mission_at_yellow; // 0x0222964B
static bool Mission_pre_terminated; // 0x02229664
static bool Mission_pending_termination; // 0x02229665
static bool Mission_terminated; // 0x02229666
static bool Mission_terminated_by_script; // 0x02229667
static bool Mission_succeeded; // 0x02229668
static bool Mission_checkpoint_pending; // 0x02229669
static bool Mission_saving_pre; // 0x0222966A
static bool Mission_autosave_after_unlocks; // 0x0222966B
static bool Mission_autostart; // 0x0222966C
static mission_info* Mission_waiting_to_run_complete; // 0x02229670


// Alert_level_ranges[0].min_value (0x01657598, activity.cpp, not ported): green starts at 0.
static const float ALERT_LEVEL_GREEN_MIN_VALUE = 0.0f;

static event_result mission_success_event_happened(const event_data& in_event, notify_info* info);
static event_result mission_failure_event_happened(const event_data& in_event, notify_info* info);

static mission_info* active_mission_info()
{
    return mission_info_get(Mission_active_index);
}

// 0x0075E9A0 (:129)
static void mission_handle_final_mission_complete()
{
    kiosk_handle_game_complete();
}

// 0x0078E030 (:134)
static void mission_clean_invlunerable_flags()
{
    for (int i = 0; i < World.get_num_objects<human>(); i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->hflags.mission_invulnerable)
        {
            hp->hflags.invulnerable = 0;
            hp->hflags.mission_invulnerable = 0;
        }
    }
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        World.get_object<object_squad>(i)->clear_mission_invulnerable();
    }
    for (int i = 0; i < World.get_num_objects<vehicle>(); i++)
    {
        World.get_object<vehicle>(i)->clear_mission_invulnerable();
    }
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->pflags.mission_never_die)
    {
        pp->pflags.never_die = 0;
        pp->pflags.mission_never_die = 0;
    }
}

void mission_reclaim_charges_and_mines_impl(player* pp, weapon* mine_weapon, weapon* charge_weapon, weapon* singularity_weapon)
{
    for (int i = 0; i < World.get_num_objects<projectile>(); i++)
    {
        projectile* proj = World.get_object<projectile>(i);
        const weapon_class_type weapon_class = proj->w_info->weapon_class;
        if (weapon_class == WEAPON_CLASS_THROWN_MINE || weapon_class == WEAPON_CLASS_THROWN_CHARGE)
        {
            if (proj->obj_flags.destroyed || pp->get_handle() != proj->owner_handle)
            {
                continue;
            }
            World.destroy_object(proj, false);
            if (proj->ca_time_offset_stamp.valid())
            {
                continue;
            }
            if (weapon_class == WEAPON_CLASS_THROWN_MINE)
            {
                // Sic: a player already holding mines gets the ammo and, with room, the weapon added as well.
                if (mine_weapon != nullptr)
                {
                    mine_weapon->add_ammo(1);
                }
                if (player_has_room_in_inventory(pp))
                {
                    inventory_item* added = human_add_item_to_inventory(pp, proj->w_info->name, 1, 1, -1, true, false, true);
                    if (added != nullptr)
                    {
                        mine_weapon = World.object_from_handle<weapon>(added->weapon_handle);
                    }
                }
            }
            else if (charge_weapon != nullptr)
            {
                charge_weapon->add_ammo(1);
            }
        }
        else if (weapon_class == WEAPON_CLASS_SINGULARITY_BOMB && !proj->ca_time_offset_stamp.valid() && !proj->obj_flags.destroyed && pp->get_handle() == proj->owner_handle)
        {
            World.destroy_object(proj, false);
            if ((float)proj->age_timer.time_since() <= *(const float*)((const char*)proj->w_info + 0x204))
            {
                if (singularity_weapon != nullptr)
                {
                    singularity_weapon->add_ammo(1);
                }
                else if (player_has_room_in_inventory(pp))
                {
                    inventory_item* added = human_add_item_to_inventory(pp, proj->w_info->name, 1, 1, -1, true, false, true);
                    if (added != nullptr)
                    {
                        singularity_weapon = World.object_from_handle<weapon>(added->weapon_handle);
                    }
                }
            }
        }
    }
}

// 0x0078E170 (:168)
static void mission_reclaim_charges_and_mines()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    weapon* mine_weapon = nullptr;
    weapon* charge_weapon = nullptr;
    weapon* singularity_weapon = nullptr;
    inventory_item* item = pp->inventory;
    while (item != nullptr)
    {
        weapon* wp = World.object_from_handle<weapon>(item->weapon_handle);
        if (wp != nullptr)
        {
            const weapon_class_type weapon_class = wp->w_info->weapon_class;
            if (weapon_class == WEAPON_CLASS_THROWN_MINE)
            {
                mine_weapon = wp;
            }
            else if (weapon_class == WEAPON_CLASS_THROWN_CHARGE)
            {
                charge_weapon = wp;
            }
            else if (weapon_class == WEAPON_CLASS_SINGULARITY_BOMB)
            {
                singularity_weapon = wp;
            }
        }
        item = item->next;
        if (item == pp->inventory)
        {
            break;
        }
    }
    mission_reclaim_charges_and_mines_impl(pp, mine_weapon, charge_weapon, singularity_weapon);
}

// 0x007B4A10 (:263)
static void mission_terminate_internal()
{
    mission_info* mission = active_mission_info();
    if (mission == nullptr)
    {
        return;
    }
    int salvage = 0;
    player_meta_data* meta = player_get_meta_data();
    if (meta != nullptr)
    {
        salvage = meta->get_salvage();
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    GAME_TRACE("mission", "terminate %s: succeeded=%d by_script=%d", mission->name, Mission_succeeded ? 1 : 0, Mission_terminated_by_script ? 1 : 0);
    char exit_status = 1;
    int end_condition_crc = 0;
    if (!Mission_succeeded)
    {
        exit_status = pp->is_killed() ? 2 : 0;
        end_condition_crc = gamelog_get_end_condition_crc();
    }
    const vector player_pos = gamelog_get_human_pos(pp);
    log_event_mission_attempt(gamelog_get_player_checksum(), gamelog_get_logging_time(), gamelog_get_mission_crc(mission), gamelog_get_time_played(), gamelog_get_pct_complete(), exit_status, salvage,
                              gamelog_get_locale(), gamelog_get_difficulty(), gamelog_get_mission_duration(), player_pos, end_condition_crc);
    if (!Mission_succeeded)
    {
        if (!pp->is_killed())
        {
            gamemusic_event_occurred(GAMEMUSIC_MISSION_FAILED_PLAYER_ALIVE);
        }
    }
    else
    {
        if (video_cutscene_pending_or_playing() && !mission->final_mission)
        {
            mission->mission_flags.complete = true;
            mission->mission_flags.unlocked = true;
            mission->mission_flags.viewed = true;
            mission->mission_flags.attempted = true;
            Mission_waiting_to_run_complete = mission;
        }
        else
        {
            mission->mission_completed();
            object_mission_start_node_update_markers();
        }
        gamemusic_event_occurred(GAMEMUSIC_MISSION_SUCCEEDED);
        gamemusic_mission_completed(mission);
        district* dp = nullptr;
        if (mission->district_name != nullptr)
        {
            const unsigned int handle = get_district_by_name(mission->district_name);
            object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
            dp = objp != nullptr && objp->get_obj_type() == OT_DISTRICT ? (district*)objp : nullptr;
        }
        // Magic number: morale as a whole percentage.
        const int reward = fl_round(gfm_get_district_morale_percent(dp) * 100.0f) * mission->morale_multiplier + mission->reward_salvage;
        player_get_meta_data()->add_salvage(reward, false);
        if (mission_evaluate_final_mission_complete())
        {
            mission_handle_final_mission_complete();
        }
        if (dp != nullptr)
        {
            hud_ui_change_control(gfm_get_edf_control_display_value(dp), gfm_get_edf_control_percent(dp), 0, 0.0f, dp->get_handle(), true, 0);
        }
    }
    if (mission->special_score_name != nullptr)
    {
        gamemusic_restore_original_music();
    }
    gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    // The results screen, except for a won tutorial or cutscene mission or one with nothing to tally. The skip path fades
    // out and nothing here fades back in: whatever follows that mission has to (the results screen's exit does it otherwise).
    const bool tutorial = mission->mis_mode == MISSION_MODE_TUTORIAL;
    const bool no_reward = mission->morale_multiplier == 0 && mission->reward_salvage == 0;
    const bool skip_results = Mission_succeeded && (tutorial || mission_get_mode() == MISSION_MODE_CUTSCENE || no_reward);
    if (skip_results)
    {
        GAME_TRACE("mission", "no results screen");
        ui_fade_to_black_fade_out(false);
        mission_terminate_cleanup();
    }
    else
    {
        GAME_TRACE("mission", "results screen");
        hud_ui_mission_complete_start(Mission_succeeded, mission);
    }
    marauder_raid_set_howl_delay(); // inlined in rfg.exe
    Mission_terminated = true;
}

// 0x00753CE0 (.h:24)
void mission_data::reset()
{
    handle1 = 0xFFFFFFFF;
    handle2 = 0xFFFFFFFF;
    handle3 = 0xFFFFFFFF;
    fl_value1 = fl_nan();
    fl_value2 = fl_nan();
    valid = false;
}

// 0x007A58B0 (:369)
void mission_system_level_init()
{
    Mission_pre_terminated = false;
    Mission_pending_termination = false;
    Mission_terminated = false;
    Mission_terminated_by_script = false;
    Mission_succeeded = false;
    Mission_checkpoint_pending = false;
    Mission_autosave_after_unlocks = false;
    Mission_warp_obj_after_unlocks = 0xFFFFFFFF;
    Mission_waiting_to_run_complete = nullptr;
    Mission_active_index = INVALID_MISSION_HANDLE;
    Mission_checkpoint_restore_index = INVALID_MISSION_HANDLE;
    Mission_active_data.reset();
    ai_enable_threaten_behavior();
    notify_info info;
    info.data = nullptr;
    info.callback = mission_success_event_happened;
    info.type = EVENT_MISSION_SUCCESS;
    gem_register_for_event(info);
    info.callback = mission_failure_event_happened;
    info.type = EVENT_MISSION_FAILURE;
    gem_register_for_event(info);
    critical_area_level_init();
}

// 0x0075E9B0 (:411)
void mission_system_level_init_post()
{
}

// 0x0075E9C0 (:415)
void mission_system_init()
{
}

// 0x0078E3D0 (:420)
void mission_system_level_shutdown()
{
    critical_area_level_shutdown();
    mission_info* mission = active_mission_info();
    if (mission != nullptr)
    {
        if (mission->special_score_name != nullptr)
        {
            gamemusic_restore_original_music();
        }
        Mission_active_index = INVALID_MISSION_HANDLE;
    }
    gamemusic_set_minimum_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
    gamemusic_set_limited_music_level(GAMEMUSIC_THRESHOLD_SILENCE);
}

// 0x0078E420 (:438)
bool mission_system_save(cfile* save_file)
{
    const mission_handle handle = World.get_world_state() == WSM_CHECKPOINT && !Mission_succeeded ? Mission_active_index : INVALID_MISSION_HANDLE;
    return cf_write_uint32((unsigned int)handle, save_file) && mission_info_save(save_file);
}

// 0x0079D700 (:455)
bool mission_system_load(cfile* save_file)
{
    Mission_checkpoint_restore_index = (mission_handle)cf_read_uint32(save_file);
    mission_info_load(save_file);
    return true;
}

// 0x0078E460 (:468)
bool mission_is_ready()
{
    mission_info* mission = active_mission_info();
    return mission != nullptr && mission->mission_flags.ready;
}

// 0x0078E490 (:477) LTCG: mission_index in EAX, autostart on the stack
static void mission_activate_pre_save(mission_handle mission_index, bool autostart)
{
    if (Mission_active_index != INVALID_MISSION_HANDLE || mission_index == INVALID_MISSION_HANDLE ||
        (Mission_checkpoint_restore_index != INVALID_MISSION_HANDLE && Mission_checkpoint_restore_index != mission_index))
    {
        GAME_TRACE("mission", "activate #%d refused: active #%d, checkpoint restore #%d", (int)mission_index, (int)Mission_active_index, (int)Mission_checkpoint_restore_index);
        return;
    }
    Mission_pre_terminated = false;
    Mission_pending_termination = false;
    Mission_terminated = false;
    Mission_terminated_by_script = false;
    Mission_succeeded = false;
    Mission_saving_pre = true;
    Mission_autosave_after_unlocks = false;
    Mission_warp_obj_after_unlocks = 0xFFFFFFFF;
    Mission_autostart = autostart;
    Mission_active_index = mission_index;
    mission_info* mission = mission_info_get(mission_index);
    if (mission == nullptr)
    {
        return;
    }
    GAME_TRACE("mission", "activate %s #%d: mode=%d autostart=%d checkpoint_restore=%d", mission->name, (int)mission_index, (int)mission->mis_mode, autostart ? 1 : 0,
               Mission_checkpoint_restore_index != INVALID_MISSION_HANDLE ? 1 : 0);
    gamelog_mark_mission_start_time();
    mission->mission_flags.ready = false;
    if (Mission_checkpoint_restore_index == INVALID_MISSION_HANDLE)
    {
        mission->mission_flags.attempted = true;
        mission_reclaim_charges_and_mines();
        if (mission->mis_mode != MISSION_MODE_CUTSCENE)
        {
            World.save_and_continue(nullptr, false, true);
            if (mission->mis_mode == MISSION_MODE_TUTORIAL || Mission_autostart)
            {
                World.sl_flags.game_save_to_disk = false;
            }
        }
        const int squads = World.get_num_objects<object_squad>();
        for (int i = 0; i < squads; i++)
        {
            object_squad* squad = World.get_object<object_squad>(i);
            const human_info* info = squad->get_squad_def() != nullptr ? squad->get_squad_def()->special_npc_info : nullptr;
            if (info != nullptr && (info->flags.guerrilla_tech || info->flags.guerrilla_rfc))
            {
                squad->despawn();
            }
        }
    }
    dialogue_manager_cancel_all();
}

// 0x0079D720 (:559)
static void mission_activate_post_save()
{
    mission_info* mission = active_mission_info();
    if (mission == nullptr)
    {
        return;
    }
    GAME_TRACE("mission", "pre-mission save done: layers, voices and reservations start");
    if (mission->managed_layers != nullptr)
    {
        mission->managed_layers->start_layer_loading();
    }
    if (Mission_checkpoint_restore_index == INVALID_MISSION_HANDLE)
    {
        restore_list_do_restore(mission->start_node_handle);
    }
    activity_suspend_activities(ADR_MISSION);
    human_voice_load_set(mission->required_voice_personas);
    EDF_locked_mission_progression_level = edf_progression_level_internal();
    safehouse_vip_mission_start(); // inlined in rfg.exe
    squad_pre_mission_clean_up();
    srm_mission_reservation(mission->srm_data);
    object_upgrade_node_disable_markers();
    Mission_checkpoint_restore_index = INVALID_MISSION_HANDLE;
    Cartographer.remove_all_paths();
    if (mission->mis_mode == MISSION_MODE_TUTORIAL)
    {
        activity_disable_mayhem_for_tutorial(); // rfg.exe inlines it
        game_time_of_day_set_tutorial_tod();
    }
    if (mission->special_score_name != nullptr)
    {
        gamemusic_start_special_music(mission->special_score_name);
    }
    local_player_fill_ammo();
}

// The game trace of what mission_setup_do_frame waits for: no script of the mission runs until it is ready, and only a
// change is written.
static void mission_trace_setup_wait(const char* reason)
{
    static const char* last;
    if (reason != last)
    {
        last = reason;
        GAME_TRACE("mission", "setup waits for %s", reason);
    }
}

// 0x0079D840 (:632)
void mission_setup_do_frame()
{
    mission_info* mission = active_mission_info();
    if (mission == nullptr || mission->mission_flags.ready)
    {
        return;
    }
    if (Mission_saving_pre)
    {
        if (World.sl_flags.pending_save_game || World.sl_flags.performing_save_game)
        {
            mission_trace_setup_wait("the pre-mission save");
            return;
        }
        Mission_saving_pre = false;
        mission_activate_post_save();
    }
    if (!restore_list_restore_complete(mission->start_node_handle))
    {
        mission_trace_setup_wait("the restore list");
        return;
    }
    voice_slotting_do_frame();
    if (!human_voice_set_streamed(mission->required_voice_personas))
    {
        mission_trace_setup_wait("the voice personas to stream");
        return;
    }
    if (mission->managed_layers != nullptr && !mission->managed_layers->layer_resources_serialized())
    {
        if (!mission->managed_layers->layer_resources_loaded() || !mission->managed_layers->serialize_layer_objects())
        {
            mission_trace_setup_wait("the managed layers");
            return;
        }
    }
    if (World.get_world_state() == WSM_CHECKPOINT)
    {
        if (Mission_saved_checkpoint == 1)
        {
            if (!mission->mission_flags.checkpoint_script_loaded && mission->checkpoint_managed_script[1] != -1)
            {
                script_activate_managed_group(mission->checkpoint_managed_script[1], true);
                mission->mission_flags.checkpoint_script_loaded = true;
            }
        }
        else if (Mission_saved_checkpoint == 0)
        {
            if (!mission->mission_flags.checkpoint2_script_loaded && mission->checkpoint_managed_script[0] != -1)
            {
                script_activate_managed_group(mission->checkpoint_managed_script[0], true);
                mission->mission_flags.checkpoint2_script_loaded = true;
            }
        }
    }
    else if (!mission->mission_flags.scripts_loaded)
    {
        for (int i = 0; i < mission->managed_script_list.size(); i++)
        {
            script_activate_managed_group(mission->managed_script_list[i], false);
        }
        mission->mission_flags.scripts_loaded = true;
    }
    if (!srm_all_reserved_slots_streamed())
    {
        mission_trace_setup_wait("the reserved spawn slots to stream");
        return;
    }
    mission_trace_setup_wait("nothing");
    GAME_TRACE("mission", "%s ready, its scripts run", mission->name);
    mission->mission_flags.ready = true;
    World.do_frame(true);
    script_do_frame();
    reinforcement_guerrilla_disabled(true); // rfg.exe inlines it
    object_spawn_do_spawn();
}

// 0x0075E9D0 (:758)
void mission_end_warp_to_object(unsigned int warp_to_obj_handle)
{
    GAME_TRACE("mission", "warp to %s after the unlocks", game_trace_object_name(warp_to_obj_handle));
    Mission_warp_obj_after_unlocks = warp_to_obj_handle;
}

// 0x007B4CB0 (:766)
void mission_system_do_frame()
{
    critical_area_do_frame();
    mission_info* active = active_mission_info();
    if (active != nullptr && active->mis_mode == MISSION_MODE_PARTYTIME)
    {
        object* objp = World.object_from_handle(Mission_active_data.handle1);
        if (objp != nullptr)
        {
            // Magic numbers: the party time ring's two reds.
            color inner(0xFF, 0, 0, 100);
            Cartographer.add_decal(objp->pos, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, Mission_active_data.fl_value1, inner, false);
            color outer(0xFF, 0, 0, 60);
            Cartographer.add_decal(objp->pos, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, Mission_active_data.fl_value2, outer, true);
        }
    }
    if (Mission_waiting_to_run_complete != nullptr && !video_cutscene_pending_or_playing())
    {
        Mission_waiting_to_run_complete->mission_completed();
        const bool no_autosave = Mission_waiting_to_run_complete->no_autosave;
        const bool no_reward = Mission_waiting_to_run_complete->morale_multiplier == 0 && Mission_waiting_to_run_complete->reward_salvage == 0;
        Mission_waiting_to_run_complete = nullptr;
        mission_notify_new();
        upgrade_show_new_upgrades(true, 0);
        if (!no_autosave && no_reward)
        {
            Mission_autosave_after_unlocks = true;
        }
        object_mission_start_node_update_markers();
    }
    bool clear_after_unlocks = false;
    if (Mission_warp_obj_after_unlocks != 0xFFFFFFFF && Mission_active_index == INVALID_MISSION_HANDLE && !hud_ui_district_liberation_screen_active())
    {
        object* objp = World.object_from_handle(Mission_warp_obj_after_unlocks);
        if (objp != nullptr)
        {
            World.save_and_warp(objp->pos, objp->orient);
        }
        clear_after_unlocks = true;
    }
    else if (Mission_autosave_after_unlocks && !hud_ui_district_liberation_screen_active())
    {
        World.save_and_continue(nullptr, false, true);
        clear_after_unlocks = true;
    }
    if (clear_after_unlocks)
    {
        Mission_warp_obj_after_unlocks = 0xFFFFFFFF;
        Mission_autosave_after_unlocks = false;
    }
    if (Mission_terminated)
    {
        return;
    }
    if (Mission_checkpoint_pending)
    {
        GAME_TRACE("mission", "checkpoint %d save", Mission_saved_checkpoint + 1);
        Mission_saved_checkpoint++;
        World.save_and_continue(nullptr, true, true);
        Mission_checkpoint_pending = false;
        Mission_checkpoint_start_handle = 0xFFFFFFFF;
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            // The checkpoint's start: the nearest respawn point belonging to this mission.
            float nearest = 0.0f;
            for (int i = 0; i < World.get_num_objects<player_start>(); i++)
            {
                player_start* point = World.get_object<player_start>(i);
                if (point->checkpoint_respawn && point->m_handle == Mission_active_index)
                {
                    const float distance = vec_xz_dist_squared(pp->pos, point->pos);
                    if (Mission_checkpoint_start_handle == 0xFFFFFFFF || distance < nearest)
                    {
                        nearest = distance;
                        Mission_checkpoint_start_handle = point->get_handle();
                    }
                }
            }
        }
    }
    const game_state state = gameseq_get_state();
    if (Mission_pending_termination && state != GS_INGAME_WEAPON_CABINET)
    {
        mission_terminate_internal();
        return;
    }
    if (!Mission_pre_terminated)
    {
        if (Mission_active_index != INVALID_MISSION_HANDLE)
        {
            ai_system_mission_do_frame();
        }
        return;
    }
    if (active != nullptr)
    {
        if (active->mission_flags.scripts_loaded)
        {
            for (int i = 0; i < active->managed_script_list.size(); i++)
            {
                script_terminate_managed_group(active->managed_script_list[i], Mission_succeeded);
            }
            active->mission_flags.scripts_loaded = false;
        }
        if (active->mission_flags.checkpoint_script_loaded)
        {
            script_terminate_managed_group(active->checkpoint_managed_script[Mission_saved_checkpoint], Mission_succeeded);
            active->mission_flags.checkpoint_script_loaded = false;
        }
        if (active->mission_flags.checkpoint2_script_loaded)
        {
            script_terminate_managed_group(active->checkpoint_managed_script[Mission_saved_checkpoint], Mission_succeeded);
            active->mission_flags.checkpoint2_script_loaded = false;
        }
    }
}

// 0x0078E5E0 (:901)
void mission_activate(mission_handle handle, bool autostart)
{
    mission_activate_pre_save(handle, autostart);
}

// 0x0079D9D0 (:907)
void mission_activate(const char* mission_name, bool autostart)
{
    Mission_saved_checkpoint = -1;
    Mission_checkpoint_start_handle = 0xFFFFFFFF;
    const mission_handle handle = mission_handle_find(mission_name);
    if (handle != INVALID_MISSION_HANDLE)
    {
        mission_activate_pre_save(handle, autostart);
    }
}

// 0x0075E9E0 (:933)
mission_handle mission_get_current_mission()
{
    return Mission_active_index;
}

// 0x0075E9F0 (:938)
unsigned int mission_get_checkpoint_start_location_handle()
{
    return Mission_checkpoint_start_handle;
}

// 0x0079DA30 (:951)
const char* mission_get_name()
{
    return mission_get_name_from_handle(Mission_active_index);
}

// 0x0079DA60 (:956)
const char* mission_get_description()
{
    return mission_get_description_from_handle(Mission_active_index);
}

// 0x0078E5F0 (:962)
const char* mission_get_name_from_handle(mission_handle m_handle)
{
    mission_info* mission = mission_info_get(m_handle);
    return mission != nullptr ? mission->name : nullptr;
}

// 0x0078E620 (:975)
const char* mission_get_description_from_handle(mission_handle m_handle)
{
    mission_info* mission = mission_info_get(m_handle);
    return mission != nullptr ? mission->description : nullptr;
}

// 0x0079DA90 (:989)
mission_mode mission_get_mode()
{
    return mission_get_mode_from_handle(Mission_active_index);
}

// 0x0078E650 (:995)
mission_mode mission_get_mode_from_handle(mission_handle m_handle)
{
    mission_info* mission = mission_info_get(m_handle);
    return mission != nullptr ? mission->mis_mode : MISSION_MODE_DEFAULT;
}

// 0x0078E680 (:1008)
const char* mission_crit_area_warning()
{
    mission_info* mission = active_mission_info();
    return mission != nullptr ? mission->crit_area_message : nullptr;
}

// 0x00774970 (:1026)
const mission_data* mission_get_data()
{
    if (Mission_active_index == INVALID_MISSION_HANDLE)
    {
        return nullptr;
    }
    return Mission_active_data.valid ? &Mission_active_data : nullptr;
}

// 0x00774990 (:1043)
bool mission_set_data(unsigned int handle1, unsigned int handle2, unsigned int handle3, float fl_value1, float fl_value2)
{
    if (Mission_active_index == INVALID_MISSION_HANDLE)
    {
        return false;
    }
    Mission_active_data.handle1 = handle1;
    Mission_active_data.fl_value1 = fl_value1;
    Mission_active_data.handle2 = handle2;
    Mission_active_data.handle3 = handle3;
    Mission_active_data.fl_value2 = fl_value2;
    Mission_active_data.valid = true;
    return true;
}

// 0x0079DAC0 (:1064)
void mission_clear_data()
{
    if (Mission_active_index != INVALID_MISSION_HANDLE)
    {
        mission_info* mission = active_mission_info();
        if (mission != nullptr && mission->mis_mode == MISSION_MODE_ANTS_VS_MAG)
        {
            camera_stop_satellite_mode();
        }
        Mission_active_data.reset();
    }
}

// 0x0075EA00 (:1084)
bool mission_is_active()
{
    return Mission_active_index != INVALID_MISSION_HANDLE;
}

// 0x0075EA10 (:1090)
bool mission_is_terminating()
{
    return Mission_active_index != INVALID_MISSION_HANDLE && (Mission_pre_terminated || Mission_pending_termination);
}

// 0x0078E6B0 (:1104)
bool mission_layer_resources_serialized()
{
    mission_info* mission = active_mission_info();
    return mission == nullptr || mission->managed_layers == nullptr || mission->managed_layers->layer_resources_serialized();
}

// 0x0078E6F0 (:1121)
void mission_cancel()
{
    if (Mission_active_index == INVALID_MISSION_HANDLE)
    {
        return;
    }
    GAME_TRACE("mission", "cancel");
    gamemusic_mission_canceled();
    hud_ui_clear_current_radio_message(0);
    if (World.get_world_state() == WSM_CHECKPOINT)
    {
        World.set_world_state(WSM_DEFAULT);
    }
    int salvage = 0;
    player_meta_data* meta = player_get_meta_data();
    if (meta != nullptr)
    {
        salvage = meta->get_salvage();
    }
    mission_info* mission = active_mission_info();
    if (mission != nullptr)
    {
        player* pp = player_get_local_player();
        const vector player_pos = gamelog_get_human_pos(pp);
        // Magic number: exit status 3, cancelled.
        log_event_mission_attempt(gamelog_get_player_checksum(), gamelog_get_logging_time(), gamelog_get_mission_crc(mission), gamelog_get_time_played(), gamelog_get_pct_complete(), 3, salvage,
                                  gamelog_get_locale(), gamelog_get_difficulty(), gamelog_get_mission_duration(), player_pos, 0);
    }
    World.set_pending_territory_load("", false, false, false, World.dlc_bundle_id, false);
}

// 0x0075EA40 (:1173)
void mission_failure()
{
    mission_failure_event data;
    data.desc = nullptr;
    gem_post_event(data);
}

// 0x0075EA60 (:1181)
void mission_player_died()
{
    if (Mission_active_index != INVALID_MISSION_HANDLE)
    {
        GAME_TRACE("mission", "the player died: the mission ends");
        Mission_pre_terminated = true;
    }
}

// 0x0075EA80 (:1194)
void mission_terminate(bool called_from_script)
{
    GAME_TRACE("mission", "termination requested: by_script=%d", called_from_script ? 1 : 0);
    Mission_pending_termination = true;
    Mission_terminated_by_script = called_from_script;
}

// 0x0075EAA0 (:1201)
void mission_checkpoint_save()
{
    GAME_TRACE("mission", "checkpoint requested");
    Mission_checkpoint_pending = true;
}

static void mission_set_ended(bool succeeded)
{
    GAME_TRACE("mission", "ended: succeeded=%d", succeeded ? 1 : 0);
    Mission_pre_terminated = true;
    Mission_succeeded = succeeded;
    player* pp = player_get_local_player();
    if (pp != nullptr && !pp->pflags.never_die)
    {
        pp->pflags.never_die = 1;
        pp->pflags.mission_never_die = 1;
    }
    critical_area_delete();
}

// 0x0079DB30 (:1206)
static event_result mission_success_event_happened(const event_data&, notify_info*)
{
    if (mission_info_get(mission_get_name()) == nullptr)
    {
        return EVENT_IGNORED;
    }
    mission_set_ended(true);
    return EVENT_OK;
}

// 0x0079DBA0 (:1225)
static event_result mission_failure_event_happened(const event_data& in_event, notify_info*)
{
    if (mission_info_get(mission_get_name()) == nullptr)
    {
        return EVENT_IGNORED;
    }
    const char* reason = ((const mission_failure_event&)in_event).desc;
    GAME_TRACE("mission", "failure: %s", reason != nullptr ? reason : "(no reason)");
    hud_ui_mission_complete_set_subtitle(reason);
    gamelog_set_end_condition(reason);
    camera_view_set_default();
    mission_set_ended(false);
    return EVENT_OK;
}

// 0x0075EAB0 (:1257)
bool mission_set_player_never_die(bool never_die)
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    if (never_die)
    {
        if (!pp->pflags.never_die)
        {
            pp->pflags.never_die = 1;
            pp->pflags.mission_never_die = 1;
        }
    }
    else if (pp->pflags.mission_never_die)
    {
        pp->pflags.never_die = 0;
        pp->pflags.mission_never_die = 0;
    }
    return true;
}

// 0x0075EAF0 (:1281)
mission_handle mission_get_checkpoint_handle()
{
    return Mission_checkpoint_restore_index;
}

// 0x007B0420 (:1287)
void mission_terminate_cleanup()
{
    mission_info* mission = active_mission_info();
    player* pp = player_get_local_player();
    if (mission == nullptr || pp == nullptr)
    {
        return;
    }
    GAME_TRACE("mission", "cleanup %s", mission->name);
    if (Mission_succeeded)
    {
        pp->refill_health(1.0f);
    }
    const bool tutorial = mission->mis_mode == MISSION_MODE_TUTORIAL;
    pp->hflags.disallow_vehicle_exit = 0;
    if (mission->mis_mode == MISSION_MODE_WMR)
    {
        vi_exit_data exit_data;
        vehicle_interaction_request_exit(pp, &exit_data);
    }
    for (int i = 0; i < World.get_num_objects<object_effect>(); i++)
    {
        object_effect* effect = World.get_object<object_effect>(i);
        if (effect->obj_flags.created_by_mission_or_activity)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    if (mission->managed_layers != nullptr)
    {
        mission->managed_layers->start_layer_unloading();
    }
    pp->disband_ally_and_escort_squads();
    srm_mission_release();
    human_voice_unload_set(mission->required_voice_personas);
    Dialogue_disable_dialogue_start = false;
    Vehicle_ai_panic_drivers = false;
    activity_mission_vrail_cancel(); // rfg.exe inlines it
    activity_cont_attack_end();
    amb_set_ambient_vehicle_spawn_disabled(false);
    Disable_reinforcement_spawn = false;
    object_spawn_node_enable_mission_spawns();
    amb_spawn_unlock_ambient_levels();
    Mission_draft_mode = false;
    gfm_end_temporary_morale(); // rfg.exe inlines these two
    gfm_set_morale_offset(0.0f);
    public_address_ignore_next_all_clear_message();
    gsm_release_alert_level_cap();
    gsm_set_alert_value(ALERT_LEVEL_GREEN_MIN_VALUE);
    ai_enable_threaten_behavior();
    activity_reinstate_activities(ADR_MISSION);
    object_squad_spawn_node_set_disabled(false);
    vehicle_set_avoid_despawning_vehicles(false);
    kiosk_unlock_from_mission(Mission_active_index);
    if (mission_get_mode() == MISSION_MODE_WMR)
    {
        vector pos = pp->pos;
        float height;
        if (havok_terrain_height(height, pp->pos.x, pos.z, false, nullptr))
        {
            pos.y = height;
        }
        if (human_in_vehicle_interaction(pp, nullptr))
        {
            vi_exit_data exit_data;
            // Magic number: flags 3, as rfg.exe stores them.
            *(unsigned char*)&exit_data.flags = 3;
            vehicle_interaction_request_exit(pp, &exit_data);
        }
        // Magic number: 6 m of clearance for the teleport.
        human_teleport(pp, pos, pp->orient, 6.0f, false);
        if (pp->char_inst->aim_handle != -1)
        {
            animlib_clear_non_looping_animations(pp->char_inst->aim_handle);
        }
        human_set_movement_mode(pp, HMM_NONE, HMSM_NONE);
        human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
        human_controller_reset(pp);
        human_update_collision_filter(pp, true);
        camera_level_init_post();
        vanim_clear_states(pp->char_inst->vp);
    }
    Mission_active_data.reset();
    No_npc_npc_damage = false;
    mission_clean_invlunerable_flags();
    objective_highlight_remove_all();
    hud_ui_missions_remove_objective();
    Cartographer.remove_all_paths();
    Mission_active_index = INVALID_MISSION_HANDLE;
    Mission_checkpoint_restore_index = INVALID_MISSION_HANDLE;
    object_mission_start_node_update_markers();
    object_upgrade_node_enable_markers();
    if (!tutorial)
    {
        squad_mission_clean_up();
    }
    cont_support_remove_all_squads();
    srm_do_frame_special_slot();
    object_spawn_do_spawn();
    reinforcement_guerrilla_disabled(false); // rfg.exe inlines it
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        human* hp = World.get_object<npc>(i);
        if (hp->h_info->flags.guerrilla_tech)
        {
            // Magic number: 10 s before the tech's unlock lines.
            voice_line_unlocking_set_global_cooldown(hp, VLUG_SAMANYA, 10000);
            break;
        }
    }
    public_address_kiosk_restore();
    mission->mission_flags.ready = false;
}

// 0x0075EB00 (:1510)
bool mission_autostarted()
{
    return Mission_autostart;
}

// 0x0075EB10 (:1516)
bool mission_alert_allows_start()
{
    if ((int)Current_alert_level < 0)
    {
        return true;
    }
    if ((int)Current_alert_level < 4)
    {
        if (Current_alert_level == ALERT_LEVEL_GREEN)
        {
            return true;
        }
        if (Current_alert_level == ALERT_LEVEL_YELLOW)
        {
            return Allow_mission_at_yellow;
        }
    }
    return false;
}

// 0x0075EB40 (:1536)
bool mission_allow_at_yellow_alert()
{
    return Allow_mission_at_yellow;
}
