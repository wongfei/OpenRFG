// Original: volition/rfg/code/gameplay/activities/activity_demolitions_master.cpp
//
// gameflow.md GF4: destroy one building with the weapons the challenge gives, inside a time limit and a range. The node's
// use prompt starts it: the briefing, the player's inventory swapped for the challenge's, the run, the results briefing,
// then the inventory and position restored under a fade.

#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_stress.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_mayhem_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/custom_dlc_data.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_activity_briefing.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/minimap_icon_info.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_demolitions_master_node.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/spawn/spawn_enums.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/color.h"

#include "volition/rfg/code/gamewide/game_trace.h"

#include <string.h>

extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)

activity_demolitions_master Demolitions_master_activity; // 0x022296E8

static bool Allow_DM_at_yellow = false; // 0x022295C4
static bool Check_demo_master_ammo = true; // 0x016510D9
static custom_dlc_bool Custom_DLC_boolAllow_DM_at_yellow("Allow_DM_at_yellow", Allow_DM_at_yellow); // 0x022296D4
static custom_dlc_bool Custom_DLC_boolCheck_demo_master_ammo("Check_demo_master_ammo", Check_demo_master_ammo); // 0x02231C5C
static int no_ammo_time_ms = 2500; // 0x0165C918, the wait before running out of ammo ends a run
static hud_message_handle five_second_message = INVALID_HUD_MESSAGE_HANDLE; // 0x0165C920
static timestamp Despawn_npcs_timer; // 0x016532A8
static int s_previous_second; // 0x02273DFC

// Magic number: a weapon whose ammo is this has no count; the interface hides its icon and the inventory gets no ammo.
static const int WEAPON_NO_AMMO = -2;

// rfg.exe inlines this. Plays an interface cue if the bank has it.
static void play_timer_cue(int cue_id)
{
    audiolib_cue_info info;
    if (audiolib_cue_get_info(cue_id, &info) == AUDIOLIB_ERR_NONE)
    {
        audiolib_params params;
        params.flags = 0x30; // Magic number: rfg.exe's flags for a 2D interface sound
        audiolib_cue_play(cue_id, &params, nullptr);
    }
}

// rfg.exe inlines this. Original: only clears what mission_never_die says was set for the activity.
static void player_clear_activity_never_die()
{
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->pflags.mission_never_die)
    {
        pp->pflags.never_die = 0;
        pp->pflags.mission_never_die = 0;
    }
}

// 0x00781CE0 (:112): NPCs that are hostile and know of the player, or are in the area, leave. Melee ones get a rifle first.
// Called again every ten seconds of a run.
static void despawn_npcs_for_demo_master()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const vector center = Demolitions_master_activity.region_center;
    const float range = Demolitions_master_activity.base_demolitions_master_data->active_range;
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        const bool enemy = (int)human_teams_get_disposition(pp, np) < TEAM_DISPOSITION_NONE && np->get_ai()->perception.has_player_enemy_belief();
        if (!enemy && vec_xz_dist_squared(np->pos, center) > range * range)
        {
            continue;
        }
        np->start_active_despawn(NPC_DESPAWN_TYPE_LOW, nullptr);
        const weapon* wp = np->get_equipped_weapon();
        if (wp != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            human_delete_all_items_from_inventory(np, false, true);
            inventory_item* iip = human_add_item_to_inventory(np, "m16", 1, 60, -1, true, false, true); // Magic number: rfg.exe's ammo
            if (iip != nullptr)
            {
                human_equip_inventory_item(np, iip);
            }
        }
    }
    Despawn_npcs_timer.set(10000); // Magic number: ten seconds
}

// 0x0077E990 (.h:53)
activity_demolitions_master::activity_demolitions_master()
    : activity_base_modal(ACTIVITY_MODE_DEMOLITIONS_MASTER, false),
      house_handle(0xFFFFFFFF),
      dm_node_handle(0xFFFFFFFF),
      player_last_vehicle_handle(0xFFFFFFFF),
      activity_vehicle_handle(0xFFFFFFFF),
      district_handle(0xFFFFFFFF),
      base_demolitions_master_data(nullptr),
      house_center(ZERO_VECTOR),
      region_center(ZERO_VECTOR),
      player_start_end_position(ZERO_VECTOR),
      player_start_end_orient(IDENTITY_MATRIX),
      loading_done(false),
      loading_screen_shown(false),
      began_startup(false),
      player_info_reloaded(false),
      player_is_ignored(false),
      current_state(DM_STATE_WAIT),
      previous_second(-1),
      pre_activity_unlimited_ammo_state(false),
      saved_player_vehicle(nullptr)
{
    completion_stats.activity_over = false;
    completion_stats.ending_sequence_state = false;
    completion_stats.elapsed_time = -1;
    completion_stats.par_time = -1;
    completion_stats.remaining_time = -1;
    completion_stats.salvage_received = 0;
    completion_stats.status_string1[0] = '\0';
    completion_stats.status_string2[0] = '\0';
}

// 0x007691B0
activity_demolitions_master::~activity_demolitions_master()
{
}

// 0x00781E30 (:168): the briefing's accept, before the run to start it and after it to leave.
void activity_demolitions_master::demolitions_master_accept_internal()
{
    GAME_TRACE("activity", "demolitions master briefing accepted: state=%d", (int)current_state);
    if (current_state == DM_STATE_PREGAME_BRIEFING)
    {
        current_state = DM_STATE_PREGAME_ACCEPT;
        loading_screen_timer.set(1000); // Magic number: one second
        loading_done = false;
        began_startup = false;
        return;
    }
    current_state = DM_STATE_POSTGAME_REJECT;
    loading_screen_timer.set(1000); // Magic number: one second
    loading_done = false;
    player_info_reloaded = false;
    player* pp = player_get_local_player();
    pp->pflags.disable_controls = 1;
    pp->steering_vector.zero();
    flag_demo_master_buildings_for_minimap_coloration(house_handle, false);
}

// 0x0076C9D0 (:194)
void activity_demolitions_master::demolitions_master_reject_internal()
{
    GAME_TRACE("activity", "demolitions master briefing rejected: state=%d", (int)current_state);
    player_get_local_player()->pflags.disable_controls = 0;
    if (current_state == DM_STATE_PREGAME_BRIEFING)
    {
        current_state = DM_STATE_PREGAME_REJECT;
        activity_end();
        reset();
    }
}

// 0x0076CA10 (:217): rfg.exe inlines reset here.
void activity_demolitions_master::system_init()
{
    reset();
}

// 0x0076CA20 (:226)
void activity_demolitions_master::level_init()
{
    activity_base_modal::level_init();
    reset();
}

// 0x00758460 (:237). Original: player_start_end_position, post_activity_timer and out_of_ammo_timer stay as they were.
void activity_demolitions_master::reset()
{
    if (completion_stats.activity_over)
    {
        dm_node_handle = 0xFFFFFFFF;
    }
    house_handle = 0xFFFFFFFF;
    activity_vehicle_handle = 0xFFFFFFFF;
    house_center.zero();
    region_center.zero();
    base_demolitions_master_data = nullptr;
    do_frame_polling_period.set(0);
    spawn_polling_period.set(0);
    time_elapsed.invalidate();
    time_remaining.invalidate();
    out_of_range_timer.invalidate();
    loading_screen_timer.invalidate();
    previous_second = -1;
    completion_stats.elapsed_time = -1;
    completion_stats.par_time = -1;
    completion_stats.remaining_time = -1;
    saved_player_vehicle = nullptr;
    completion_stats.activity_over = false;
    completion_stats.ending_sequence_state = false;
    completion_stats.salvage_received = 0;
    completion_stats.status_string1[0] = '\0';
    completion_stats.status_string2[0] = '\0';
}

// 0x007AD780 (:279)
bool activity_demolitions_master::activity_start()
{
    audiolib_reset_collision_timer();
    if (Multiplayer || !do_startup_initialization() || !activity_base_modal::activity_start())
    {
        return false;
    }
    ui_tool_tips_add_tip("Demo_Master", 2500, nullptr); // Magic number: rfg.exe's delay
    Mayhem_activity.disable(ADR_ACTIVITY); // rfg.exe inlines it
    play_activity_music();
    return true;
}

// 0x007A0FE0 (:308). A dead player only ends a run for the reason process_player_death sets. Starts the ending, and answers
// true when its delay is over; a building down counts as the win, with the longer delay so that it can be watched.
bool activity_demolitions_master::activity_should_end()
{
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->is_killed() && (completion_stats.ending_sequence_state || gamelog_get_activity_failed_reason() != GLCR_PLAYER_DIED))
    {
        return false;
    }
    if (current_state == DM_STATE_POSTGAME_DELAY)
    {
        if (!post_activity_timer.valid() || !post_activity_timer.elapsed())
        {
            return false;
        }
        post_activity_timer.invalidate();
        if (completion_stats.ending_sequence_state && (!base_demolitions_master_data->dm_stats.completed || completion_stats.elapsed_time < base_demolitions_master_data->dm_stats.best_time))
        {
            base_demolitions_master_data->dm_stats.best_time = completion_stats.elapsed_time;
        }
        return true;
    }
    if (is_building_destroyed()) // rfg.exe inlines it
    {
        begin_ending_sequence(true, 7500); // Magic number: seven and a half seconds
        return false;
    }
    if (Check_demo_master_ammo && !human_has_ammo(player_get_local_player(), true, false))
    {
        if (stress_evaluation_is_active(house_handle))
        {
            return false;
        }
        if (!out_of_ammo_timer.valid())
        {
            out_of_ammo_timer.set(no_ammo_time_ms);
            return false;
        }
        if (!out_of_ammo_timer.elapsed())
        {
            return false;
        }
        out_of_ammo_timer.invalidate();
        gamelog_set_activity_failed_reason(GLCR_OUT_OF_AMMO);
        begin_ending_sequence(false, 100); // Magic number: rfg.exe's delay
        return false;
    }
    if (!time_remaining.valid() || !time_remaining.elapsed())
    {
        return false;
    }
    gamelog_set_activity_failed_reason(GLCR_OUT_OF_TIME);
    begin_ending_sequence(false, 100); // Magic number: rfg.exe's delay
    return false;
}

// 0x007979B0 (:372)
void activity_demolitions_master::begin_ending_sequence(bool ending_sequence_state, int delay_time_ms)
{
    if (out_of_range_timer.valid() && !out_of_range_timer.elapsed())
    {
        out_of_range_timer.invalidate();
    }
    const int elapsed = time_elapsed.time_since();
    completion_stats.elapsed_time = elapsed < 0 ? 0 : elapsed;
    completion_stats.par_time = base_demolitions_master_data != nullptr ? base_demolitions_master_data->par_time : 0;
    int remaining = -1;
    if (time_remaining.valid())
    {
        remaining = time_remaining.time_until();
        if (remaining < 0)
        {
            remaining = 0;
        }
    }
    completion_stats.remaining_time = remaining;
    strncpy(completion_stats.status_string1, get_completion_text_part_1(), sizeof(completion_stats.status_string1));
    strncpy(completion_stats.status_string2, get_completion_text_part_2(), sizeof(completion_stats.status_string2));
    completion_stats.status_string1[15] = '\0';
    completion_stats.status_string2[15] = '\0';
    completion_stats.ending_sequence_state = ending_sequence_state;
    {
        const player* tp = player_get_local_player();
        GAME_TRACE("activity", "demolitions master ends: %s %s reason=%d elapsed=%d ms, player at (%.1f %.1f %.1f) xz %.1f of %.1f", base_demolitions_master_data != nullptr ? base_demolitions_master_data->level_name : "(no data)", ending_sequence_state ? "success" : "failure", (int)gamelog_get_activity_failed_reason(), completion_stats.elapsed_time, tp != nullptr ? tp->pos.x : 0.0f, tp != nullptr ? tp->pos.y : 0.0f, tp != nullptr ? tp->pos.z : 0.0f, tp != nullptr ? vec_xz_dist(tp->pos, region_center) : 0.0f, base_demolitions_master_data != nullptr ? base_demolitions_master_data->active_range : 0.0f);
    }
    post_activity_timer.set(delay_time_ms);
    current_state = DM_STATE_POSTGAME_DELAY;
    completion_stats.activity_over = true;
}

// 0x00797AC0 (:399). Original: loading_screen_shown is only cleared; nothing here shows a screen.
void activity_demolitions_master::activity_end()
{
    if (loading_screen_shown)
    {
        loading_screen_shown = false;
    }
    player_get_local_player()->pflags.disable_controls = 0;
    Mayhem_activity.enable(ADR_ACTIVITY); // rfg.exe inlines it
    activity_base_modal::activity_end();
    activity_cooldown_period.set(0);
}

// 0x0076CA70 (:420)
void activity_demolitions_master::do_activity_end()
{
    activity_end();
    player* pp = player_get_local_player();
    if (pp != nullptr && !pp->pflags.never_die)
    {
        pp->pflags.never_die = 1;
        pp->pflags.mission_never_die = 1;
    }
    current_state = DM_STATE_POSTGAME_BRIEFING;
    hud_ui_activity_briefing_start(ABT_DEMOLITIONS_MASTER, INVALID_VOICE_LINE_HANDLE);
}

// 0x00758510 (:432)
void activity_demolitions_master::activity_end_cancel()
{
    activity_kill();
    reset();
    if (player_is_ignored)
    {
        ai_ignore_player(false);
        player_is_ignored = false;
    }
}

// 0x00797B10 (:451)
void activity_demolitions_master::activity_kill()
{
    if (loading_screen_shown)
    {
        loading_screen_shown = false;
    }
    player_get_local_player()->pflags.disable_controls = 0;
    Mayhem_activity.enable(ADR_ACTIVITY); // rfg.exe inlines it
    activity_base_modal::activity_kill();
    stop_all_flares();
    activity_cooldown_period.set(0);
}

// 0x007A1170 (:475)
void activity_demolitions_master::do_activity_kill()
{
    GAME_TRACE("activity", "demolitions master killed: state=%d house=0x%08X loading_done=%d", (int)current_state, house_handle, loading_done ? 1 : 0);
    activity_kill();
    warp_player_on_end();
    if (loading_done)
    {
        reload_inventory();
    }
    reset();
    restore_previous_player_vehicle();
    if (player_is_ignored)
    {
        ai_ignore_player(false);
        player_is_ignored = false;
    }
}

// 0x00797B60 (:495)
void activity_demolitions_master::warp_player_on_end()
{
    player* pp = player_get_local_player();
    if (!player_start_end_position.is_null())
    {
        unsigned int movers[50]; // Magic number: rfg.exe's list size
        const int num_movers = World.collect_objects<general_mover>(player_start_end_position, 2.0f, movers, 50, nullptr, nullptr);
        for (int i = 0; i < num_movers; i++)
        {
            World.destroy_object(movers[i], false);
        }
        player_respawn(pp, player_start_end_position, player_start_end_orient, true);
    }
    player_start_end_position.zero();
    player_start_end_orient.make_identity();
    player_clear_activity_never_die();
}

// 0x00797C40 (:518): fades out, restores the player under the black, and fades back in.
void activity_demolitions_master::do_post_activity_loading_process(bool force_darkness)
{
    static bool input_enabled; // 0x0165C88A
    projectile_invalidate_pending_charges();
    explosion_clear_all_pending();
    human_weapons_cancel_reload(player_get_local_player());
    if (force_darkness)
    {
        loading_done = false;
    }
    if (!loading_screen_shown && loading_screen_timer.time_until() < 751) // Magic number: rfg.exe's 0x2EF ms
    {
        loading_screen_shown = true;
    }
    if (!player_info_reloaded && ui_fade_to_black_is_faded() && !force_darkness)
    {
        warp_player_on_end();
        reload_inventory();
        restore_previous_player_vehicle();
        player_info_reloaded = true;
        ui_fade_to_black_fade_in(true);
        control_enable_player_input(input_enabled);
        loading_done = true;
        return;
    }
    if (ui_fade_to_black_is_fade_in_complete() && gameseq_get_state() == GS_GAMEPLAY && gameseq_get_pending_state() == GS_NONE)
    {
        ui_fade_to_black_fade_out(false);
        input_enabled = control_player_input_enabled();
        control_enable_player_input(false);
    }
}

// 0x007A11C0 (:553): under the fade, clears what a previous run left around the building, and starts the activity up.
void activity_demolitions_master::do_loading_process(bool layer_is_loaded, bool force_darkness)
{
    static bool input_enabled; // 0x0165C91C
    const bool charges_pending = projectile_invalidate_pending_charges();
    const bool explosions_pending = explosion_clear_all_pending();
    for (int i = 0; i < World.get_num_objects<object_effect>(); i++)
    {
        object_effect* effect = World.get_object<object_effect>(i);
        if (vec_xz_dist(effect->pos, player_start_end_position) > 100.0f) // Magic number: 100 m
        {
            continue;
        }
        const visual_effect_info* info = effects_get_visual_effect_info(effect->get_visual_effect_info_handle());
        if (info != nullptr && v_strnicmp(info->name, "dth", 3) == 0) // Magic number: the prefix of death effects
        {
            World.destroy_object(effect, false);
        }
    }
    human_weapons_cancel_reload(player_get_local_player());
    rfg_mover* mover = World.object_from_handle<rfg_mover>(house_handle);
    if (mover != nullptr)
    {
        stress_evaluation_stop(mover);
        object* child = mover->child_ptr;
        while (child != nullptr)
        {
            if (child->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                // Original: a child of another object type with this sub type would pass null, which stops nothing.
                stress_evaluation_stop(child->get_obj_type() == OT_MOVER ? static_cast<rfg_mover*>(child) : nullptr);
            }
            child = child->child_next;
            if (child == mover->child_ptr)
            {
                break;
            }
        }
    }
    if (force_darkness || charges_pending || explosions_pending)
    {
        loading_done = false;
    }
    if (ui_fade_to_black_is_fade_in_complete() && gameseq_get_state() == GS_GAMEPLAY && gameseq_get_pending_state() == GS_NONE)
    {
        ui_fade_to_black_fade_out(false);
        input_enabled = control_player_input_enabled();
        control_enable_player_input(false);
    }
    if (!loading_screen_shown && loading_screen_timer.time_until() < 751) // Magic number: rfg.exe's 0x2EF ms
    {
        loading_screen_shown = true;
    }
    if (charges_pending || explosions_pending)
    {
        return;
    }
    if (layer_is_loaded)
    {
        control_enable_player_input(input_enabled);
        ui_fade_to_black_fade_in(true);
        loading_done = true;
    }
    else if (ui_fade_to_black_is_faded() && !began_startup)
    {
        began_startup = activity_begin_start_up();
        if (!began_startup)
        {
            ui_fade_to_black_instant_fade_in();
            control_enable_player_input(true);
            player_get_local_player()->pflags.disable_controls = 0;
            do_activity_kill(); // rfg.exe inlines it
        }
    }
}

// 0x00758540 (:639). Original: activity_inventory_save_file keeps the closed file's pointer.
void activity_demolitions_master::reload_inventory()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    human_combat_abandon_turret(pp);
    human_delete_all_items_from_inventory(pp, false, false);
    pp->activity_inventory_save_file = cf_open(pp->activity_inventory_buffer, sizeof(pp->activity_inventory_buffer), "rb", PLATFORM_PC);
    pp->load_inventory(pp->activity_inventory_save_file, true);
    cf_close(pp->activity_inventory_save_file);
    pp->pflags.unlimited_ammo = pre_activity_unlimited_ammo_state;
}

// 0x0076CAB0 (:658)
void activity_demolitions_master::restore_previous_player_vehicle()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    if (activity_vehicle_handle != 0xFFFFFFFF)
    {
        vehicle* vp = World.object_from_handle<vehicle>(activity_vehicle_handle);
        if (vp != nullptr)
        {
            vehicle_freeze(activity_vehicle_handle);
            vehicle_reposition_on_ground(vp);
            vehicle_set_player_disabled(vp, true);
            vehicle_set_accelerator_disabled(vp, true);
            vehicle_fade_out(vp, 500, true, false); // Magic number: half a second
        }
        activity_vehicle_handle = 0xFFFFFFFF;
    }
    pp->last_vehicle_driven = 0xFFFFFFFF;
    if (saved_player_vehicle != nullptr)
    {
        srm_swap_player_slot(saved_player_vehicle);
        object_spawn_set_player_vehicle_info(saved_player_vehicle);
        object_spawn_do_spawn();
        pp->last_vehicle_driven = object_spawn_node_get_player_vehicle_handle();
    }
}

// 0x007585C0 (:698)
bool activity_demolitions_master::lock_alert_level() const
{
    return is_active();
}

// 0x007585D0 (:710)
const char* activity_demolitions_master::get_title() const
{
    return "ACT_DEMO_MASTER_NAME";
}

// 0x007585E0 (:725)
const char* activity_demolitions_master::get_subtitle() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->briefing_destroy_message : "ACT_DEMO_MASTER_SUBTITLE";
}

// 0x00758600 (:734)
const char* activity_demolitions_master::get_corner_hud_subtitle() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->corner_HUD_destroy_message : "ACT_DM_DESTROY_BUILDING";
}

// 0x00758620 (:743)
const char* activity_demolitions_master::get_icon_image_name() const
{
    return get_minimap_bitmap_icon_name(MAP_ELEMENT_DEMOLITIONS_MASTER);
}

// 0x00758630 (:749). Magic numbers: 3 a finished run's win, 4 its loss, 0 the briefing; activity.cpp reads the 3.
const int activity_demolitions_master::get_alt_interface_state() const
{
    if (completion_stats.activity_over)
    {
        return completion_stats.ending_sequence_state ? 3 : 4;
    }
    if (current_state == DM_STATE_PREGAME_BRIEFING)
    {
        return 0;
    }
    return get_interface_state();
}

// 0x0076CB70 (:765)
const char* activity_demolitions_master::get_weapon_icon_name(int index)
{
    if (base_demolitions_master_data == nullptr || index >= base_demolitions_master_data->weapon_list.size())
    {
        return nullptr;
    }
    const demolitions_master_weapon_data& data = base_demolitions_master_data->weapon_list[index];
    if (data.w_info == nullptr)
    {
        return data.icon_name;
    }
    return data.ammo != WEAPON_NO_AMMO ? data.w_info->icon_name : nullptr;
}

// 0x0076CBB0 (:790)
const int activity_demolitions_master::get_weapon_ammo(int index)
{
    if (base_demolitions_master_data == nullptr || index >= base_demolitions_master_data->weapon_list.size())
    {
        return -1;
    }
    const demolitions_master_weapon_data& data = base_demolitions_master_data->weapon_list[index];
    if (data.w_info != nullptr && data.w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        return -1;
    }
    return data.ammo != WEAPON_NO_AMMO ? data.ammo : 0;
}

// 0x00758660 (:827)
const char* activity_demolitions_master::get_info_title_1() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->info_title_1 : " ";
}

// 0x00758680 (:841)
const char* activity_demolitions_master::get_info_title_2() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->info_title_2 : " ";
}

// 0x00781EB0 (:858). Original: rfg.exe reads Demolitions_master_activity's house, which is this object's, and with ammo,
// time and range all fine the reason left is death.
const char* activity_demolitions_master::get_completion_text_part_1() const
{
    if (base_demolitions_master_data == nullptr)
    {
        return " ";
    }
    if (is_building_destroyed()) // rfg.exe inlines it
    {
        return base_demolitions_master_data->success_string;
    }
    if (!human_has_ammo(player_get_local_player(), true, false))
    {
        return base_demolitions_master_data->failure_string_ammo;
    }
    if (time_remaining.valid() && time_remaining.elapsed())
    {
        return base_demolitions_master_data->failure_string_time;
    }
    if (out_of_range_timer.valid() && out_of_range_timer.elapsed())
    {
        return base_demolitions_master_data->failure_string_range;
    }
    return base_demolitions_master_data->failure_string_death;
}

// 0x007586A0 (:877). Original: with data it is a blank, and without it the string it was copied into.
const char* activity_demolitions_master::get_completion_text_part_2() const
{
    return base_demolitions_master_data != nullptr ? " " : completion_stats.status_string2;
}

// 0x007586C0 (:887)
const char* activity_demolitions_master::get_description() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->description : " ";
}

// 0x007586E0 (:900)
const int activity_demolitions_master::get_goal_time() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->time_limit : 0;
}

// 0x0076CC00 (:910). Plays the countdown sounds, as the HUD asks for the time every frame. Out of range the time shown is
// the sooner of the two limits.
const int activity_demolitions_master::get_display_time()
{
    if (out_of_range_timer.valid())
    {
        int time = out_of_range_timer.time_until();
        if (time_remaining.valid() && time_remaining.time_until() < time)
        {
            time = time_remaining.time_until();
        }
        return time < 0 ? 0 : time;
    }
    if (base_demolitions_master_data != nullptr)
    {
        if (time_remaining.valid())
        {
            if (current_state == DM_STATE_ACTIVE)
            {
                int time = time_remaining.time_until();
                if (time < 0)
                {
                    time = 0;
                }
                const int second = time / 1000;
                if (second == 60)
                {
                    if (previous_second != 60)
                    {
                        play_timer_cue(Ga_cue_ids.timer_one_minute_remains);
                    }
                }
                else if (second == 10)
                {
                    if (previous_second != 10)
                    {
                        play_timer_cue(Ga_cue_ids.timer_ten_seconds_remains);
                    }
                }
                else if (second < 5 && previous_second != second)
                {
                    play_timer_cue(Ga_cue_ids.timer_final_seconds_remains);
                }
                previous_second = second;
                return time;
            }
            if (current_state == DM_STATE_POSTGAME_DELAY)
            {
                return completion_stats.remaining_time < 0 ? 0 : completion_stats.remaining_time;
            }
        }
        else if (current_state == DM_STATE_ACTIVE)
        {
            const int time = time_elapsed.time_since();
            return time < 0 ? 0 : time;
        }
    }
    return completion_stats.elapsed_time < 0 ? 0 : completion_stats.elapsed_time;
}

// 0x00758700 (:989)
const int activity_demolitions_master::get_par_time() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->par_time : 0;
}

// 0x00758720 (:999). Worked out once, at the first ask of a finished run: the salvage for the first time, and again for
// beating par the first time. It also counts the completion, so a run the screen never asks about does not count.
const int activity_demolitions_master::get_salvage_received()
{
    if (base_demolitions_master_data != nullptr && completion_stats.salvage_received == 0 && completion_stats.activity_over)
    {
        const int salvage = base_demolitions_master_data->standard_rewards.salvage;
        int received = base_demolitions_master_data->dm_stats.times_completed == 0 ? salvage : 0;
        if (completion_stats.ending_sequence_state)
        {
            if (!base_demolitions_master_data->dm_stats.par_beaten && completion_stats.elapsed_time <= completion_stats.par_time * 1000 + 999) // Magic number: par's whole second
            {
                base_demolitions_master_data->dm_stats.par_beaten = true;
                received += salvage;
            }
            base_demolitions_master_data->dm_stats.times_completed++;
        }
        completion_stats.salvage_received = received;
    }
    return completion_stats.salvage_received;
}

// 0x007587B0 (:1021)
const int activity_demolitions_master::get_best_time() const
{
    return base_demolitions_master_data != nullptr ? base_demolitions_master_data->dm_stats.best_time : 0;
}

// 0x00781F90 (:1037). Magic numbers: 2 the building is down, 1 it may still fall with the player out of ammo, 0 neither.
const int activity_demolitions_master::get_stress_active_indicator() const
{
    if (is_building_destroyed()) // rfg.exe inlines it
    {
        return 2;
    }
    return !human_has_ammo(player_get_local_player(), true, false) && stress_evaluation_is_active(house_handle) ? 1 : 0;
}

// 0x007587D0 (:1055)
const char* activity_demolitions_master::get_first_hint() const
{
    return current_state == DM_STATE_PREGAME_BRIEFING ? "MENU_CANCEL" : "";
}

// 0x007587F0 (:1065)
const char* activity_demolitions_master::get_second_hint() const
{
    return current_state == DM_STATE_PREGAME_BRIEFING ? "MENU_ACCEPT" : "MENU_CLOSE";
}

// 0x00797D20 (:1074)
void activity_demolitions_master::process_player_death()
{
    gamelog_set_activity_failed_reason(GLCR_PLAYER_DIED);
    begin_ending_sequence(false, 100); // Magic number: rfg.exe's delay
}

// 0x00758810 (:1091)
bool activity_demolitions_master::activity_clean_up_completed() const
{
    return activity_base_modal::activity_clean_up_completed() && current_state != DM_STATE_POSTGAME_BRIEFING && current_state != DM_STATE_POSTGAME_REJECT;
}

// 0x00797D40 (:1102). Magic number: 1001 is the action of the death animation the fade waits on.
void activity_demolitions_master::activity_do_frame_clean_up()
{
    activity_base::activity_do_frame_clean_up();
    activity_shutdown_period_override(0); // rfg.exe sets shutdown_period directly
    if (current_state == DM_STATE_POSTGAME_REJECT)
    {
        player* pp = player_get_local_player();
        player_exit_cover(pp, false);
        if (!loading_done)
        {
            do_post_activity_loading_process(pp->char_inst->is_playing_action(0x3E9));
            return;
        }
        loading_screen_timer.invalidate();
        loading_screen_shown = false;
        pp->pflags.disable_controls = 0;
        if (completion_stats.ending_sequence_state)
        {
            record_victory();
        }
        else
        {
            record_loss();
        }
        activity_kill();
        reset();
        if (player_is_ignored)
        {
            ai_ignore_player(false);
            player_is_ignored = false;
        }
    }
    if (current_state != DM_STATE_POSTGAME_BRIEFING)
    {
        current_state = DM_STATE_COMPLETE;
    }
}

// 0x0076CEB0 (:1138). Marks the target and its parts for the map. Magic number: mover_flags bit 29 is the mark, bit 21 is
// show on map; an rfg_mover part with inherit_damage_pct clear is left out, but the walk goes on through its children.
void activity_demolitions_master::flag_demo_master_buildings_for_minimap_coloration(unsigned int target_handle, bool add_mark)
{
    if (target_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_mover* mover = World.object_from_handle<object_mover>(target_handle);
    if (mover == nullptr || (mover->mover_flags & 0x200000) == 0)
    {
        return;
    }
    const rfg_mover* rfg = mover->get_sub_type() == OT_SUB_MOVER_RFG ? static_cast<const rfg_mover*>(mover) : nullptr;
    if (add_mark && (target_handle == house_handle || rfg == nullptr || rfg->flags.inherit_damage_pct))
    {
        mover->mover_flags |= 0x20000000;
    }
    else
    {
        mover->mover_flags &= ~0x20000000u;
    }
    object* child = mover->child_ptr;
    while (child != nullptr)
    {
        flag_demo_master_buildings_for_minimap_coloration(child->get_handle(), add_mark);
        child = child->child_next;
        if (child == mover->child_ptr)
        {
            break;
        }
    }
}

// 0x007A1430 (:1163). Original: the 5 second warning is skipped when the time limit has under 5 seconds left, and the
// range is only tested every 250 ms unless the warning runs.
void activity_demolitions_master::activity_do_frame_active()
{
    if (current_state == DM_STATE_LOADING_LAYERS)
    {
        if (!loading_done)
        {
            do_loading_process(true, false);
            return;
        }
        loading_screen_timer.invalidate();
        loading_screen_shown = false;
        player_get_local_player()->pflags.disable_controls = 0;
        if (base_demolitions_master_data->time_limit > 0)
        {
            time_remaining.set(base_demolitions_master_data->time_limit * 1000);
        }
        unsigned int region_dummy = 0xFFFFFFFF; // game trace only
        for (int i = 0; i < layer_objects.size(); i++)
        {
            const object_dummy* dummy = World.object_from_handle<object_dummy>(layer_objects[i]);
            if (dummy != nullptr && dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_DEMO_MASTER)
            {
                region_center = dummy->pos;
                region_dummy = dummy->get_handle();
            }
        }
        flag_demo_master_buildings_for_minimap_coloration(house_handle, true);
        time_elapsed.set(0);
        current_state = DM_STATE_ACTIVE;
        {
            const player* tp = player_get_local_player();
            GAME_TRACE("activity", "demolitions master active: %s layer objects=%d region (%.1f %.1f %.1f) from %s 0x%08X, house (%.1f %.1f %.1f), range %.1f, player at (%.1f %.1f %.1f) xz %.1f", base_demolitions_master_data->level_name, layer_objects.size(), region_center.x, region_center.y, region_center.z, region_dummy != 0xFFFFFFFF ? "dummy" : "house", region_dummy != 0xFFFFFFFF ? region_dummy : house_handle, house_center.x, house_center.y, house_center.z, base_demolitions_master_data->active_range, tp->pos.x, tp->pos.y, tp->pos.z, vec_xz_dist(tp->pos, region_center));
        }
    }
    else if (current_state == DM_STATE_PREGAME_REJECT)
    {
        do_activity_kill();
        return;
    }
    if (time_remaining.valid() && time_remaining.elapsed() && current_state != DM_STATE_POSTGAME_DELAY)
    {
        gamelog_set_activity_failed_reason(GLCR_OUT_OF_TIME);
        begin_ending_sequence(false, 100); // Magic number: rfg.exe's delay
        return;
    }
    if (house_handle == 0xFFFFFFFF)
    {
        do_activity_kill(); // rfg.exe inlines it
        return;
    }
    if (Despawn_npcs_timer.elapsed())
    {
        despawn_npcs_for_demo_master();
    }
    if (!do_frame_polling_period.elapsed() && !out_of_range_timer.valid())
    {
        return;
    }
    do_frame_polling_period.set(250); // Magic number: a quarter of a second
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        do_activity_kill();
        return;
    }
    if (base_demolitions_master_data->use_vehicle_spawn && !post_activity_timer.valid())
    {
        if (activity_vehicle_handle == 0xFFFFFFFF)
        {
            for (int i = 0; i < layer_objects.size(); i++)
            {
                const object_vehicle_spawn_node* node = World.object_from_handle<object_vehicle_spawn_node>(layer_objects[i]);
                if (node != nullptr)
                {
                    activity_vehicle_handle = node->get_child_vehicle_handle();
                    break;
                }
            }
        }
        else if (pp->vehicle_handle != activity_vehicle_handle && pp->vehicle_handle != 0xFFFFFFFF)
        {
            // In another vehicle: the run fails where the player is.
            player_start_end_position.zero();
            gamelog_set_activity_failed_reason(GLCR_FAILED);
            begin_ending_sequence(false, 100); // Magic number: rfg.exe's delay
            return;
        }
    }
    if (activity_should_end())
    {
        do_activity_end();
        return;
    }
    if (current_state != DM_STATE_ACTIVE)
    {
        if (out_of_range_timer.valid() && !out_of_range_timer.elapsed())
        {
            out_of_range_timer.invalidate();
        }
        return;
    }
    if (pp->is_killed())
    {
        out_of_range_timer.invalidate();
        if (five_second_message != INVALID_HUD_MESSAGE_HANDLE)
        {
            hud_message_remove(five_second_message);
        }
        return;
    }
    if (vec_xz_dist(pp->pos, region_center) <= base_demolitions_master_data->active_range)
    {
        if (out_of_range_timer.valid())
        {
            out_of_range_timer.invalidate();
        }
        return;
    }
    // Magic number: 7225 is 85 m squared; beyond it a run fails at once.
    if (vec_xz_dist_squared(pp->pos, region_center) > 7225.0f || (out_of_range_timer.valid() && out_of_range_timer.elapsed()))
    {
        gamelog_set_activity_failed_reason(GLCR_OUT_OF_RANGE);
        begin_ending_sequence(false, 100); // Magic number: rfg.exe's delay
        return;
    }
    if (!out_of_range_timer.valid())
    {
        if (time_remaining.valid() && time_remaining.time_until() < 5001) // Magic number: rfg.exe's 0x1389 ms
        {
            return;
        }
        five_second_message = hud_message_add(L"ACT_DM_5_SECONDS", 4.0f, nullptr, nullptr, nullptr); // Magic number: four seconds shown
        out_of_range_timer.set(5000); // Magic number: five seconds
        return;
    }
    const int second = out_of_range_timer.time_until() / 1000;
    if (second <= 4 && s_previous_second != second)
    {
        play_timer_cue(Ga_cue_ids.timer_final_seconds_remains);
    }
    s_previous_second = second;
}

// 0x00782010 (:1334): rfg.exe jumps straight to the base's.
activity_load_state activity_demolitions_master::activity_do_frame_loading()
{
    return activity_base::activity_do_frame_loading();
}

// 0x00782020 (:1341). Magic number: the slot's owner is this activity's mode.
bool activity_demolitions_master::activity_starting_completed() const
{
    if (!activity_base::activity_starting_completed())
    {
        return false;
    }
    return !base_demolitions_master_data->use_vehicle_spawn || srm_slots_streamed(SPAWN_SLOT_VEHICLE_ACTIVITY, (resource_slot_user_id)mode);
}

// 0x007A1900 (:1350)
void activity_demolitions_master::activity_do_frame_starting()
{
    current_state = DM_STATE_LOADING_LAYERS;
    do_loading_process(false, true);
}

// 0x007A1920 (:1359)
void activity_demolitions_master::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    if (current_state == DM_STATE_LOADING_LAYERS)
    {
        current_state = DM_STATE_PREGAME_BRIEFING;
        hud_ui_activity_briefing_start(ABT_DEMOLITIONS_MASTER, base_demolitions_master_data->radio_message_start);
    }
    else if (current_state == DM_STATE_PREGAME_ACCEPT)
    {
        do_loading_process(false, false);
    }
}

// 0x00782060 (:1388). Original: stop_flare with no flare does nothing, so flares and icons simply follow the alert level.
void activity_demolitions_master::activity_do_frame_inactive()
{
    if (Multiplayer)
    {
        return;
    }
    current_state = DM_STATE_WAIT;
    if (player_is_ignored)
    {
        ai_ignore_player(false);
        player_is_ignored = false;
    }
    const bool allowed = activity_demolitions_master_alert_allows_start() && activity_none() && Mission_active_index == INVALID_MISSION_HANDLE;
    for (int i = 0; i < World.get_num_objects<object_demolitions_master_node>(); i++)
    {
        object_demolitions_master_node* node = World.get_object<object_demolitions_master_node>(i);
        if (!allowed)
        {
            node->stop_flare();
            continue;
        }
        Cartographer.add_temporary_icon(node->pos, MAP_ELEMENT_DEMOLITIONS_MASTER, false, 0xFFFFFFFF, node->is_completed() ? Completed_icon_color : Default_map_item_color, false, node->get_handle());
        node->start_flare();
    }
}

// 0x0076CF60 (:1450): a building that no longer exists counts as destroyed.
bool activity_demolitions_master::is_building_destroyed() const
{
    const rfg_mover* mover = World.object_from_handle<rfg_mover>(house_handle);
    return mover == nullptr || mover->flags.game_destroyed;
}

// 0x007B6050 (:1458): the reward's salvage is what get_salvage_received worked out.
void activity_demolitions_master::record_victory()
{
    if (base_demolitions_master_data == nullptr)
    {
        return;
    }
    base_demolitions_master_data->dm_stats.completed = true;
    const int salvage = base_demolitions_master_data->standard_rewards.salvage;
    base_demolitions_master_data->standard_rewards.salvage = completion_stats.salvage_received;
    apply_standard_rewards(base_demolitions_master_data->standard_rewards, nullptr);
    base_demolitions_master_data->standard_rewards.salvage = salvage;
    stats_update_demolitions_master(base_demolitions_master_data->level_name, (float)completion_stats.elapsed_time);
    activity_base_modal::record_victory();
}

// 0x00758870 (:1480)
void activity_demolitions_master::record_loss()
{
    activity_base::record_loss(); // rfg.exe's copy has the same body, with the mode test that never plays the sound here
}

// 0x0076CFA0 (:1488): anything but a weapon held or racked, and not of the layer, goes. Always answers false, so nothing is
// collected.
bool dm_item_cb(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (Demolitions_master_activity.layer_objects.contains(hit_objp->get_handle()) || hit_objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return false;
    }
    const object* owner = World.object_from_handle(static_cast<item*>(hit_objp)->owner_handle);
    if (owner != nullptr && owner->get_obj_type() == OT_HUMAN)
    {
        return false;
    }
    const object* parent = hit_objp->get_parent() != 0xFFFFFFFF ? World.object_from_handle(hit_objp->get_parent()) : nullptr;
    if (parent != nullptr && parent->get_obj_type() == OT_TURRET)
    {
        return false;
    }
    World.destroy_object(hit_objp, false);
    return false;
}

// 0x0076D030 (:1526): a vehicle not spawned by a node of the layer goes.
bool dm_vehicle_cb(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    const unsigned int node_handle = static_cast<vehicle*>(hit_objp)->spawn_node_handle;
    if (node_handle == 0xFFFFFFFF || !Demolitions_master_activity.layer_objects.contains(node_handle))
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// 0x0076D080 (:1548): movers not of the layer that blew up, or that moved, go if they explode when they die.
bool dm_general_mover_cb(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (Demolitions_master_activity.layer_objects.contains(hit_objp->get_handle()))
    {
        return false;
    }
    const general_mover* mover = static_cast<general_mover*>(hit_objp);
    if ((mover->mover_flags & (0x4 | 0x40000)) != 0 && mover->m_level_obj_properties->m_flags.m_death_explosion)
    {
        World.destroy_object(hit_objp, false);
    }
    return false;
}

// 0x00797E20 (:1578): clears the area for the run. The callbacks do the work and collect nothing.
void activity_demolitions_master::clean_area()
{
    const rfg_mover* mover = World.object_from_handle<rfg_mover>(house_handle);
    const player* pp = player_get_local_player();
    if (mover == nullptr || pp == nullptr)
    {
        return;
    }
    vehicle* last_vehicle = World.object_from_handle<vehicle>(pp->last_vehicle_driven);
    if (last_vehicle != nullptr)
    {
        World.destroy_object(last_vehicle, false);
    }
    const vector center = region_center;
    const float radius = base_demolitions_master_data->active_range + 10.0f; // Magic number: ten metres more
    for (int i = 0; i < World.get_num_objects<projectile>(); i++)
    {
        projectile* shot = World.get_object<projectile>(i);
        if (!layer_objects.contains(shot->get_handle()) && shot->owner_handle == pp->get_handle())
        {
            World.destroy_object(shot, false);
        }
    }
    unsigned int collected;
    World.collect_objects<item>(center, radius, &collected, 1, dm_item_cb, nullptr);
    World.collect_objects<vehicle>(center, radius, &collected, 1, dm_vehicle_cb, nullptr);
    World.collect_objects<general_mover>(center, radius, &collected, 1, dm_general_mover_cb, nullptr);
    if (!player_is_ignored)
    {
        despawn_npcs_for_demo_master();
        ai_ignore_player(true);
        player_is_ignored = true;
    }
}

// 0x007821B0 (:1625)
void activity_demolitions_master::stop_all_flares() const
{
    for (int i = 0; i < World.get_num_objects<object_demolitions_master_node>(); i++)
    {
        World.get_object<object_demolitions_master_node>(i)->stop_flare();
    }
}

// 0x00798000 (:1636)
bool activity_demolitions_master::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (spawn_objp == nullptr || Multiplayer || is_active() || house_handle != 0xFFFFFFFF)
    {
        return false;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr || !activity_base_modal::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    stop_all_flares();
    reset();
    district_handle = new_district_handle;
    if (spawn_objp->get_obj_type() != OT_DEMOLITIONS_MASTER_NODE)
    {
        return false;
    }
    const object_demolitions_master_node* node = static_cast<const object_demolitions_master_node*>(spawn_objp);
    const rfg_mover* target = node->find_demolitions_master_target();
    if (target == nullptr)
    {
        GAME_TRACE("activity", "demolitions master initialize: node=0x%08X parent=0x%08X is no building", node->get_handle(), node->get_parent());
        activity_end();
        reset();
        return false;
    }
    GAME_TRACE("activity", "demolitions master initialize: node=0x%08X %s building=0x%08X range=%.1f time=%d s vehicle=%d", node->get_handle(), node->base_demolitions_master_data != nullptr ? node->base_demolitions_master_data->level_name : "(no data)", target->get_handle(), node->base_demolitions_master_data != nullptr ? node->base_demolitions_master_data->active_range : 0.0f, node->base_demolitions_master_data != nullptr ? node->base_demolitions_master_data->time_limit : 0, node->base_demolitions_master_data != nullptr ? node->base_demolitions_master_data->use_vehicle_spawn : 0);
    const vehicle* last_vehicle = World.object_from_handle<vehicle>(pp->last_vehicle_driven);
    if (last_vehicle != nullptr)
    {
        saved_player_vehicle = last_vehicle->info;
    }
    dm_node_handle = node->get_handle();
    house_handle = target->get_handle();
    base_demolitions_master_data = node->base_demolitions_master_data;
    return base_demolitions_master_data != nullptr;
}

// 0x007A1970 (:1693)
bool activity_demolitions_master::do_startup_initialization()
{
    const rfg_mover* mover = World.object_from_handle<rfg_mover>(house_handle);
    if (mover == nullptr)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    mover->get_bbox(bmin, bmax);
    house_center.x = (bmin.x + bmax.x) * 0.5f;
    house_center.y = (bmin.y + bmax.y) * 0.5f;
    house_center.z = (bmin.z + bmax.z) * 0.5f;
    region_center = house_center;
    return finalize_activity_initialization();
}

// 0x007980F0 (:1712): the player's inventory goes into the buffer, and the challenge's weapons take its place. Slots run
// 3, 0, 1, 2. Magic number: the sledgehammer is what a challenge with no weapons leaves.
bool activity_demolitions_master::finalize_activity_initialization()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return false;
    }
    pp->activity_inventory_save_file = cf_open(pp->activity_inventory_buffer, sizeof(pp->activity_inventory_buffer), "wb", PLATFORM_PC);
    if (pp->activity_inventory_save_file == nullptr)
    {
        return false;
    }
    pp->save_inventory(pp->activity_inventory_save_file);
    cf_close(pp->activity_inventory_save_file);
    pp->activity_inventory_save_file = nullptr;
    human_delete_all_items_from_inventory(pp, false, false);
    int added = 0;
    for (int i = 0; i < base_demolitions_master_data->weapon_list.size(); i++)
    {
        const demolitions_master_weapon_data& data = base_demolitions_master_data->weapon_list[i];
        if (data.w_info == nullptr)
        {
            continue;
        }
        inventory_item* iip = human_add_item_to_inventory(pp, data.w_info->name, 1, data.ammo != WEAPON_NO_AMMO ? data.ammo : 0, (added + 3) % 4, false, false, true);
        if (iip != nullptr)
        {
            if (added == 0)
            {
                human_equip_inventory_item(pp, iip);
            }
            else if (added == 1)
            {
                pp->last_equipped_weapon = iip;
            }
            else if (added == 2)
            {
                pp->second_last_equipped_weapon = iip;
            }
        }
        added++;
    }
    if (added == 0)
    {
        human_equip_inventory_item(pp, human_add_item_to_inventory(pp, "sledgehammer", 0, -1, 3, false, false, true));
    }
    if (World.object_from_handle<rfg_mover>(house_handle) == nullptr)
    {
        return false;
    }
    clean_area();
    pre_activity_unlimited_ammo_state = pp->pflags.unlimited_ammo;
    pp->pflags.unlimited_ammo = base_demolitions_master_data->unlimited_ammo_dm;
    play_timer_cue(Ga_cue_ids.timer_clock_appears);
    return true;
}

// 0x007588B0 (:1788)
void activity_demolitions_master::cancel_activity_initialization()
{
    activity_end();
    reset();
}

// 0x0076D0E0 (:1798): the area on the map, red while the out of range timer runs.
void activity_demolitions_master::render_icons() const
{
    static const color Inside_region_color(50, 200, 50, 80); // 0x02273D74
    static const color Outside_region_color(200, 50, 50, 255); // 0x02273D70
    const activity_modes current = activity_get_mode(); // rfg.exe reads the static Current_activity_mode
    if (house_handle == 0xFFFFFFFF || (current != ACTIVITY_MODE_NONE && current != ACTIVITY_MODE_DEMOLITIONS_MASTER) || current_state == DM_STATE_PREGAME_BRIEFING || current_state == DM_STATE_LOADING_LAYERS || post_activity_timer.valid())
    {
        return;
    }
    const bool out_of_range = out_of_range_timer.valid();
    Cartographer.add_decal(region_center, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, base_demolitions_master_data->active_range, out_of_range ? Outside_region_color : Inside_region_color, !out_of_range);
}

// 0x007588D0 (:1820)
bool activity_demolitions_master::house_ready() const
{
    return house_handle != 0xFFFFFFFF;
}

// 0x007588E0 (:1831). Magic number: the delay's last 100 ms hide it.
bool demolitions_master_has_interface()
{
    const demolitions_master_states state = Demolitions_master_activity.current_state;
    return state == DM_STATE_ACTIVE || state == DM_STATE_POSTGAME_BRIEFING || (state == DM_STATE_POSTGAME_DELAY && Demolitions_master_activity.post_activity_timer.time_until() > 99);
}

// 0x00758910 (:1846)
const char* demolitions_master_get_corner_hud_subtitle()
{
    return Demolitions_master_activity.get_corner_hud_subtitle();
}

// 0x0076D1F0 (:1851)
const int demolitions_master_get_display_time()
{
    return Demolitions_master_activity.get_display_time();
}

// 0x00758930 (:1856)
const int demolitions_master_get_par_time()
{
    return Demolitions_master_activity.get_par_time();
}

// 0x00758950 (:1861)
const int demolitions_master_get_salvage_received()
{
    return Demolitions_master_activity.get_salvage_received();
}

// 0x00782200 (:1866)
const int demolitions_master_get_stress_active_indicator()
{
    return Demolitions_master_activity.get_stress_active_indicator();
}

// 0x00798310 (:1871)
void demolitions_master_process_player_death()
{
    Demolitions_master_activity.process_player_death(); // rfg.exe inlines it
}

// 0x0076D200 (:1877). rfg.exe reads the static Current_alert_level; the getter clamps to green and red, which answers the
// same here.
bool activity_demolitions_master_alert_allows_start()
{
    const gfm_alert_levels level = gsm_get_alert_level();
    return level == ALERT_LEVEL_GREEN || (level == ALERT_LEVEL_YELLOW && Allow_DM_at_yellow);
}

// 0x007A1A80 (:1898)
bool activity_demolitions_master_start(const object_demolitions_master_node* dm_node)
{
    if (Demolitions_master_activity.mode == activity_get_mode() || !Demolitions_master_activity.activity_base_modal::can_initialize())
    {
        GAME_TRACE("activity", "demolitions master start refused: node=0x%08X %s", dm_node->get_handle(), Demolitions_master_activity.mode == activity_get_mode() ? "already active" : "cannot initialize");
        return false;
    }
    if (!Demolitions_master_activity.activity_base::start_activity_spawn(dm_node, district_get_from_pos(dm_node->pos)))
    {
        return false;
    }
    Demolitions_master_activity.current_state = DM_STATE_LOADING_LAYERS;
    player* pp = player_get_local_player();
    GAME_TRACE("activity", "demolitions master start: node=0x%08X %s at (%.1f %.1f %.1f), player at (%.1f %.1f %.1f)", dm_node->get_handle(), dm_node->base_demolitions_master_data != nullptr ? dm_node->base_demolitions_master_data->level_name : "(no data)", dm_node->pos.x, dm_node->pos.y, dm_node->pos.z, pp->pos.x, pp->pos.y, pp->pos.z);
    pp->pflags.disable_controls = 1;
    pp->steering_vector.zero();
    Demolitions_master_activity.player_start_end_position = pp->pos;
    Demolitions_master_activity.player_start_end_orient = pp->orient;
    int cue_id;
    if (audiolib_cue_get_id("SYS_MENU_SELECT_BUTTON_X", &cue_id) == AUDIOLIB_ERR_NONE)
    {
        audiolib_cue_play(cue_id, nullptr, nullptr);
    }
    return true;
}

// 0x00782210 (:1932)
void demolitions_master_accept()
{
    Demolitions_master_activity.demolitions_master_accept_internal();
}

// 0x0076D230 (:1938)
void demolitions_master_reject()
{
    Demolitions_master_activity.demolitions_master_reject_internal(); // rfg.exe inlines it
}
