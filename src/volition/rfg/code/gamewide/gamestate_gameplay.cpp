// Original: volition/rfg/code/gamewide/gamestate_gameplay.cpp
//
// The gameplay game state (gamestate_gameplay.h), whole (multiplayer.md MP1): the state's callbacks, the level init
// and exit sequences and the simulation frame. Every system they call that is not ported is a stand-in of the test
// programs. rfg_gamewide_unity.obj is a unity build: this file reads game_handshake.cpp's Wait_for_prev_done_frame,
// and game_clock.cpp reads Game_clock and Ingame_time_should_update (rfg_gamewide_unity.cpp includes the files in
// that order).
//
// Build differences:
// - gamestate_gp_handshake runs game_render.cpp's pre-stream render tasks (Task_list_pre_stream_render, a static of the
//   same unity TU in rfg.exe) through game_render_pre_stream_process, which is that call; game_render.cpp is in the
//   render library here.
// - Gameplay_last_havok_simulate (0x02533870) belongs to gameplay.cpp, which is not ported; it is defined here.
// - gamestate_gp_exit cleans the main scene's partition system (rfg.exe: Scene_main_p, game_render.cpp's static of the
//   same unity TU) through game_render_get_main_scene. Deviation: the headless programs have no main scene, so the
//   clean is skipped when there is none (rfg.exe always has one).
//
// Notes (decompiled behaviour, kept as is):
// - gamestate_gp_simulate_frame's death handling reads the first player for the listener height (cur_stance) but the
//   local player for the position.
// - gamestate_gp_level_init_post calls weather_level_shutdown (not a level init).
// - The level init timers (timer_get before and after each system, Load_timer_indent +-2) only feed debug output;
//   the indent is kept, the times are not printed.

#include "volition/rfg/code/gamewide/gamestate_gameplay.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/rl_partition_system.h"
#include "volition/render_lib/code/rl_renderer_core.h"
#include "volition/render_lib/code/rl_scene.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/los_queue.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/dust.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/glare.h"
#include "volition/rfg/code/code_effects/glass.h"
#include "volition/rfg/code/code_effects/los_blocker.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/tracers.h"
#include "volition/rfg/code/code_effects/xray_effect.h"
#include "volition/rfg/code/control/control.h"
#include "volition/rfg/code/cutscene/cutscene.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/destruction/rfg_destruction.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/game_render_terrain.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense.h"
#include "volition/rfg/code/gameplay/activities/activity_delivery.h"
#include "volition/rfg/code/gameplay/activities/activity_demolitions_master_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_house_arrest.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/air_bombing.h"
#include "volition/rfg/code/gameplay/gameplay.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/safehouse_vip.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_handshake.h"
#include "volition/rfg/code/gamewide/game_hudtimer.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/game_time_of_day.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/foliage_mesh.h"
#include "volition/rfg/code/geom/mesh_deform.h"
#include "volition/rfg/code/geom/mesh_morph.h"
#include "volition/rfg/code/geom/static_mesh.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_ui_activity_briefing.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/hud_ui_district_liberation.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/hud_ui_mission_complete.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/interface/ingame/overlay.h"
#include "volition/rfg/code/interface/ingame/ovl_base.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/undergrowth.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_flag_capture_zone.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_flag.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_respawn.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"
#include "volition/rfg/code/objects/custom_object.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_autoaim.h"
#include "volition/rfg/code/objects/humans/player_hold_object.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/mover_fader.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/movers/object_mover_lists.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_light.h"
#include "volition/rfg/code/objects/object_marauder_ambush_region.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_skybox.h"
#include "volition/rfg/code/objects/object_trigger_region.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/salvage.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/action_script.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/objective_highlight.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/scripting/target_outline.h"
#include "volition/rfg/code/traffic/traffic.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/weather/weather.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/vlib/code/cfile/vdir.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/key.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"

#include <math.h>

// ---- file-static globals
// int Current_bundle_id (0x0165CB04) is used here too; placed in downloadable_content.cpp (first in the unity TU).
// Static in rfg.exe's unity TU; external here for gamemusic.cpp (a TU of rfg_audio, SFX.md SX5), as Game_clock below.
int frontend_music_play_id = -1; // 0x0165CB2C
int frontend_mp_music_play_id = -1; // 0x0165CB70
gamemusic_music_types gamemusic_current_music_type = GAMEMUSIC_MUSIC_FRONTEND; // 0x0165CB78, initial data 02000000
static float s_speedScale = 1.0f; // 0x0165CBD4
static bool Ingame_time_should_update = true; // 0x0165CBE8
// bool Achievements_enabled (0x0165CCAB) is used here too; placed in stats.cpp (before this file in the unity TU).
game_clock Game_clock; // 0x025054AC
// unsigned int Dlc_level_mempool_mark (0x025337CC) is used here too; placed in downloadable_content.cpp.
static bool s_pauseGame; // 0x02533875
static bool Render_system_init; // 0x025338BD

int Gameplay_last_havok_simulate; // 0x02533870 (gameplay.cpp, see the file comment)
bool Reload_level_on_death; // 0x02533874

// ---- internal functions
static void gamestate_respawn_player(player* pp); // 0x007BFF70 (:185)
static void gamestate_gp_end_frame(); // 0x007BFFF0 (:228)
static void gamestate_gp_simulate_frame(); // 0x007EDA90 (:245)
// game_clock.cpp's (rfg_gamewide_unity, defined after this file).
static void game_time_triggers_process(); // 0x007C0F60 (game_clock.cpp:348)

// 0x007BFF70 (:185)
// LTCG: pp in ESI (mapped in the Ghidra DB).
static void gamestate_respawn_player(player* pp)
{
    if (Multiplayer)
    {
        if (multi_is_server() && multi_game_respawn_can_player_respawn(pp) != RRDR_NO_REASON)
        {
            return;
        }
        const int player_start_index = multi_game_respawn_get_best_spawn_point(pp, false);
        player_respawn(pp, player_start_index, true);
        return;
    }
    if (Reload_level_on_death)
    {
        vdir_restore(PLATFORM_PC, true);
        World.set_pending_reload();
        return;
    }
    if (World.get_state_data() == nullptr)
    {
        World.save_and_reload();
        return;
    }
    World.set_pending_load_from_memory();
}

// 0x007BFFF0 (:228)
static void gamestate_gp_end_frame()
{
}

// The level init timers of gamestate_gp_level_init_pre: timer_get around each system with the indent raised.
#define GP_TIMED(call)                \
    do                                \
    {                                 \
        timer_get(TIMER_MILLISECONDS); \
        Load_timer_indent += 2;       \
        call;                         \
        Load_timer_indent -= 2;       \
    } while (0)

// 0x007EDA90 (:245)
static void gamestate_gp_simulate_frame()
{
    // Players whose death time is up: single player ends a mission or an activity or offers the death options; in
    // multiplayer the server respawns them when their respawn time is up.
    const int num_players = World.get_num_objects<player>();
    player* first = num_players > 0 ? World.get_object<player>(0) : nullptr;
    for (int i = 0; i < num_players; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->obj_flags.destroyed)
        {
            continue;
        }
        bool respawn_message = false;
        if (pp->dead_timestamp.valid() && pp->dead_timestamp.elapsed())
        {
            if (!pp->hflags.death_func_done)
            {
                pp->hflags.death_func_done = 1;
                if (!Multiplayer && !wrecking_crew_is_active())
                {
                    if (mission_is_active())
                    {
                        mission_terminate(false);
                    }
                    else if (activity_is_active(ACTIVITY_MODE_DEMOLITIONS_MASTER))
                    {
                        pp->dead_timestamp.invalidate();
                        demolitions_master_process_player_death();
                        return;
                    }
                    else if (activity_is_active(ACTIVITY_MODE_RAID_DEFEND) || activity_is_active(ACTIVITY_MODE_RAID_ATTACK) || activity_is_ready(ACTIVITY_MODE_RAID_ATTACK))
                    {
                        pp->dead_timestamp.invalidate();
                        activity_raid_process_player_death();
                    }
                    else if (activity_is_active(ACTIVITY_MODE_HOUSE_ARREST) || activity_is_ready(ACTIVITY_MODE_HOUSE_ARREST))
                    {
                        pp->dead_timestamp.invalidate();
                        activity_house_arrest_process_player_death();
                    }
                    else if (activity_is_active(ACTIVITY_MODE_VEHICLE_RAIL))
                    {
                        pp->dead_timestamp.invalidate();
                        activity_riding_shotgun_process_player_death();
                    }
                    else if (activity_is_active(ACTIVITY_MODE_DELIVERY))
                    {
                        pp->dead_timestamp.invalidate();
                        activity_guerrilla_express_process_player_death();
                    }
                    else if (activity_is_active(ACTIVITY_MODE_AREA_DEFENSE))
                    {
                        pp->dead_timestamp.invalidate();
                        activity_area_defense_process_player_death();
                    }
                    else
                    {
                        World.save_and_continue(nullptr, false, true);
                        State_request_queue.clear();
                        gameseq_push_state(GS_PLAYER_DEATH_OPTIONS, true, false);
                    }
                }
            }
            if (Multiplayer)
            {
                if (multi_is_server() && pp->mp_time_until_player_can_respawn.elapsed())
                {
                    gamestate_respawn_player(pp);
                }
                respawn_message = object_is_local_player(pp) && pp->is_killed() && pp->cur_state != HS_SPECTATOR && Multi_match_info.match_state == MGMS_GAMEPLAY;
            }
        }
        else
        {
            respawn_message = Multiplayer && object_is_local_player(pp) && pp->is_killed() && pp->cur_state != HS_SPECTATOR && Multi_match_info.match_state == MGMS_GAMEPLAY;
        }
        pp->pflags.mp_display_respawn_msg = respawn_message ? 1 : 0;
    }

    if (wrecking_crew_is_active())
    {
        wrecking_crew_process();
    }
    timer_get(TIMER_MICROSECONDS);
    overlay_process();
    interface_process();
    control_process();
    if (!Multiplayer || !multi_is_dedicated_server())
    {
        if (Game_paused_count < 1)
        {
            camera_do_frame_pre();
        }
        hud_process();
        Cartographer.process();
    }
    timer_get(TIMER_MICROSECONDS);
    dialogue_manager_do_frame();
    gamemusic_update(Game_paused_count > 0);
    if (Game_paused_count < 1)
    {
        World.sanity_check_object_lists();
        if (Multiplayer)
        {
            multi_game_mode_system_process();
        }
        multi_game_backpack_process();
        if (!Multiplayer && !wrecking_crew_is_active() && Ingame_time_should_update)
        {
            game_clock_increment_seconds(Game_clock, Game_clock.m_time_scale * Frametime);
            game_time_triggers_process();
        }
        game_hudtimer_update(Frametime);
        game_time_of_day_do_frame();
        human_weapons_process_weapons_pre();
        explosion_do_frame();
        if (!Multiplayer)
        {
            rfg_pf_do_frame();
            ai_system_do_frame();
            convoy_do_frame();
        }
        rfg_do_frame_pre();
        if (cutscene_is_playing())
        {
            cutscene_do_frame();
            camera_do_frame();
        }
        gameplay_do_frame();
        character_instance_animlib_matrices_do_frame();
        if (Multiplayer && multi_game_mode_is_gameplay())
        {
            if (Multiplayer)
            {
                if (multi_is_server())
                {
                    if (Multiplayer && multi_is_server())
                    {
                        multi_game_player_process_weapons();
                    }
                }
                else
                {
                    multi_core_object_move_all();
                }
            }
            multi_object_flag_process();
            multi_object_flag_capture_zone_process();
        }
        turret_do_frame_pre();
        vehicle_interaction_process();
        player_process_all();
        human_process_all();
        human_process_all_animations_start();
        object_item_process();
        if ((!Multiplayer || !multi_is_dedicated_server()) && !multi_game_spectating())
        {
            salvage_do_frame();
        }
        object_debris_process();
        human_voice_do_frame();
        player_process_all_no_animation();
        human_process_all_no_animation();
        vehicle_process_all_presim();
        object_mover_do_frame_pre_sim();
        multi_game_pre_sim();
        human_process_all_animations_end();
        human_process_all_post_transform();
        // Animation-driven movement into each human's velocity.
        for (int i = 0; i < World.get_num_objects<human>(); i++)
        {
            human* hp = World.get_object<human>(i);
            if (hp->char_inst != nullptr && hp->char_inst->vp != nullptr)
            {
                vanim_process_movement_adjustments(hp->char_inst->vp, hp->pos, hp->orient, hp->velocity);
                vanim_process_completed_actions(hp->char_inst->vp);
            }
        }
        World.sanity_check_object_lists();
        const int havok_start = timer_get(TIMER_MICROSECONDS);
        havok_simulate();
        Gameplay_last_havok_simulate = timer_get(TIMER_MICROSECONDS) - havok_start;
        // The velocity each human got from the simulation (its character proxy, or its ragdoll).
        for (int i = 0; i < World.get_num_objects<human>(); i++)
        {
            human* hp = World.get_object<human>(i);
            if (hp->ragdoll_idx < 0 || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
            {
                human_controller_get_vel(hp, hp->actual_velocity);
            }
            else
            {
                havok_ragdoll_get_vel(hp->ragdoll_idx, hp->actual_velocity);
            }
            const vector& v = hp->actual_velocity;
            hp->actual_velocity_mag = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        }
        multi_game_post_sim();
        if (!Multiplayer)
        {
            player_hold_object_process();
        }
        vehicle_process_all_postsim();
        human_process_controllers(Frametime);
        safehouse_do_frame_postsim();
        human_process_all_postsim();
        air_bomb_do_frame();
        projectile_process();
        rfg_destruction_event_do_frame();
        dynamic_hole_requests_do_frame();
        turret_do_frame_post();
        if (!cutscene_is_playing())
        {
            camera_do_frame();
        }
        human_weapons_process_weapons_post();
        human_do_ik_process();
        object_item_process_postsim();
        cloth_sim_batch_process_start();
        custom_object_do_frame();
        trigger_region_do_frame();
        ambush_region_do_frame();
        safehouse_vip_do_frame();
        mission_system_do_frame();
        script_do_frame();
        if (!Multiplayer)
        {
            public_address_kiosk_do_frame();
        }
        rfg_do_frame_post();
        if ((!Multiplayer || !multi_is_dedicated_server()) && !multi_game_spectating())
        {
            weather_update();
        }
        object_item_process_post_transform();
        object_do_frame_postsim();
        objective_highlight_do_frame();
        if (!Multiplayer || !multi_is_dedicated_server())
        {
            object_effect_process_all(Frametime);
        }
        tracers_process();
        traffic_do_frame();
        object_mover_do_frame_post_sim();
        player_autoaim_calculate_assisted_aiming();
        los_queue_do_frame();
        los_blocker_do_frame();
        glass_obj_do_frame();
        video_cutscene_do_frame();
        hud_ui_handbook_do_frame();
        hud_ui_mission_complete_do_frame();
        hud_ui_district_liberation_do_frame();
        hud_ui_activity_complete_do_frame();
        hud_ui_activity_briefing_do_frame();
        if ((!Multiplayer || !multi_is_dedicated_server()) && !multi_game_spectating())
        {
            undergrowth_process();
        }
        if (!Multiplayer || !multi_is_dedicated_server())
        {
            // The audio listener: the spectator camera, or the local player's head height for its stance.
            vector listener_pos;
            vector listener_vel;
            if (multi_game_spectating())
            {
                listener_vel.x = (Rfg_camera.real_pos.x - Rfg_camera.last_pos.x) * Frames_per_second;
                listener_vel.y = (Rfg_camera.real_pos.y - Rfg_camera.last_pos.y) * Frames_per_second;
                listener_vel.z = (Rfg_camera.real_pos.z - Rfg_camera.last_pos.z) * Frames_per_second;
                listener_pos = Rfg_camera.real_pos;
            }
            else
            {
                player* pp = player_get_local_player();
                listener_pos = pp->pos;
                listener_vel.x = (listener_pos.x - pp->last_pos.x) * Frames_per_second;
                listener_vel.y = (listener_pos.y - pp->last_pos.y) * Frames_per_second;
                listener_vel.z = (listener_pos.z - pp->last_pos.z) * Frames_per_second;
                // Magic numbers: the ear height in a vehicle seat and for each stance.
                if ((unsigned int)(pp->cur_state - HS_VEHICLE_SITTING) < 2)
                {
                    listener_pos.y += 0.7f;
                }
                else if (first->cur_stance == HST_STAND)
                {
                    listener_pos.y += 1.5749999f;
                }
                else if (first->cur_stance == HST_PRONE)
                {
                    listener_pos.y += 0.3f;
                }
                else
                {
                    listener_pos.y += 1.0500001f;
                }
            }
            gameaudio_update(listener_pos, Rfg_camera.real_orient, listener_vel);
        }
        gem_do_frame();
        restricted_area_do_frame();
        mover_fader_process();
        cloth_sim_batch_process_end();
    }
    else
    {
        activity_system_do_frame(true);
        custom_object_do_frame();
    }
    havok_bpo_do_frame();
    havok_collision_events_end_sim();
    multi_game_sim_event_process();
    World.sanity_check_object_lists();
    fb_accum_blur_update();
    camera_do_frame_post();
    ai_process_target_building_update();
}

// 0x007F0F00 (:979)
void gamestate_gp_init()
{
    state_info callbacks;
    callbacks.exit_notify = nullptr;
    callbacks.user_flags = 0;
    callbacks.transparent = false;
    callbacks.pause_beneath = false;
    callbacks.enter = gamestate_gp_enter;
    callbacks.exit = gamestate_gp_exit;
    callbacks.process = gamestate_gp_process;
    callbacks.handshake = gamestate_gp_handshake;
    callbacks.render = gamestate_gp_render;
    callbacks.hide_cursor = true;
    gameseq_register_state(GS_GAMEPLAY, callbacks);
}

// 0x007C0000 (:995)
void gamestate_gp_enter(game_state prev_state)
{
    control_set_mode(CM_GAMEPLAY);
    hud_ui_enter();
    if (Multiplayer)
    {
        console_do_command("Rl_drop_shadow_enabled false");
        console_do_command("Rl_shadow_map_stabilize true");
    }
    else
    {
        console_do_command("Rl_drop_shadow_enabled true");
        console_do_command("Rl_shadow_map_stabilize false");
        weather_wind_start();
        dust_enable(true);
    }
    if (Multiplayer)
    {
        return;
    }
    if (interface_get_mode() == INT_MODE_NONE)
    {
        interface_set_mode(INT_MODE_NORMAL);
    }
    if (!Multiplayer && !wrecking_crew_is_active() && mission_final_mission_complete())
    {
        // The end-game unlock tips, each once (1000 ms).
        static const char* const tips[5] = {"Insane_Mode", "Golden_Hammer_Unlocked", "Reset_Destruction_Unlocked", "MP_Upgrades_Unlocked", "Unfinished_Guerrilla_Actions"};
        for (int i = 0; i < 5; i++)
        {
            if (!ui_tool_tips_tip_has_been_played(tips[i], false))
            {
                ui_tool_tips_add_tip(tips[i], 1000, nullptr);
            }
        }
    }
}

// 0x007F0D30 (:1066)
void gamestate_gp_exit(game_state new_state)
{
    ui_kill_subtitle();
    gamemusic_update(false);
    mission_system_level_shutdown();
    havok_add_entity_flush();
    havok_add_phantom_flush();
    Wait_for_prev_done_frame = Framecount_increment_always;
    rl_unlock();
    game_render_dyn_light_clear();
    game_render_terrain_level_shutdown();
    // The partition system is cleaned here and after the level systems (see the file comment).
    if (game_render_get_main_scene() != nullptr)
    {
        game_render_get_main_scene()->get_partition_system()->clean();
    }
    xray_effect_level_shutdown();
    game_time_of_day_level_shutdown();
    gameplay_level_shutdown();
    cutscene_level_shutdown();
    if (Multiplayer)
    {
        multi_game_object_shutdown();
    }
    object_level_shutdown();
    air_bomb_level_shutdown();
    inv_item_level_shutdown();
    effects_level_shutdown();
    mesh_morph_level_shutdown();
    vehicle_level_shutdown();
    salvage_level_shutdown();
    mover_fader_level_shutdown();
    public_address_kiosk_level_shutdown();
    World.level_shutdown();
    obj_zone_level_shutdown();
    rfg_level_shutdown();
    script_system_level_shutdown();
    havok_level_shutdown();
    undergrowth_level_shutdown();
    material_effects_destroy_all();
    static_mesh_level_shutdown();
    foliage_mesh_level_shutdown();
    rfg_pf_level_shutdown();
    Achievements_enabled = true;
    input_set_feedback(0, 0.0f, 0.0f);
    interface_level_shutdown();
    hud_level_shutdown();
    overlay_level_shutdown();
    ui_tool_tips_system_save_to_profile();
    stats_serialize_out_profile_stats();
    ui_tool_tips_level_shutdown();
    audiolib_all_sounds_stop(true, false);
    hud_ui_exit();
    gem_level_shutdown();
    if (!Multiplayer)
    {
        if (!wrecking_crew_is_active())
        {
            gamelog_local_reinit(true, nullptr);
        }
    }
    else
    {
        multi_game_level_shutdown();
        game_save_write_profile_data();
    }
    if (wrecking_crew_is_active())
    {
        wrecking_crew_level_shutdown();
        game_save_write_profile_data();
    }
    while (game_is_paused())
    {
        game_unpause();
    }
    if (game_render_get_main_scene() != nullptr)
    {
        game_render_get_main_scene()->get_partition_system()->clean();
    }
    dlc_level_shutdown();
    hud_ui_district_liberation_level_shutdown();
    hud_ui_handbook_level_shutdown();
    hud_ui_mission_complete_level_shutdown();
    hud_ui_activity_complete_level_shutdown();
    hud_ui_activity_briefing_level_shutdown();
}

// 0x007EE450 (:1251)
void gamestate_gp_process()
{
    if (!s_pauseGame)
    {
        if (s_speedScale != 1.0f)
        {
            Frametime = Frametime * s_speedScale;
        }
        gamestate_gp_simulate_frame();
    }
    Sim_snapshot.in_gameplay = true;
}

// 0x007C0160 (:1281)
void process_save_load()
{
    if (World.sl_flags.pending_save_game)
    {
        World.save_state_data();
        if (World.sl_flags.game_save_to_disk)
        {
            world_state_buf* state = World.get_state_data();
            if (state != nullptr)
            {
                platform_save_to_disk(state->buf, state->cur_size, World.pending_game_save_slot, World.sl_flags.auto_save_game);
            }
            World.sl_flags.auto_save_game = false;
            World.sl_flags.game_save_to_disk = false;
        }
        World.sl_flags.pending_save_game = false;
    }
    if (World.sl_flags.pending_load_game_from_memory)
    {
        World.set_pending_territory_load("", false, false, false, World.dlc_bundle_id, World.sl_flags.reset_destruction);
        World.sl_flags.pending_load_game_from_memory = false;
    }
}

// 0x007D9120 (:1330)
void gamestate_gp_handshake(rl_renderer* pRenderer)
{
    Cartographer.handshake();
    game_render_pre_stream_process(); // see the file comment
    process_save_load();
    World.do_frame(false);
    multi_game_mode_system_handshake();
    mission_setup_do_frame();
    if (wrecking_crew_is_active())
    {
        wrecking_crew_handshake();
    }
    object_item_process_high_res_textures();
    objective_highlight_do_frame_handshake();
    game_render_setup_frame(*pRenderer);
}

// 0x007CD330 (:1360)
void gamestate_gp_render(rl_renderer* pRenderer)
{
    if (wrecking_crew_is_active())
    {
        wrecking_crew_render();
    }
    if (Multiplayer)
    {
        if (multi_is_dedicated_server())
        {
            return;
        }
        if (Multiplayer)
        {
            multi_game_mode_system_render();
        }
    }
    multi_game_backpack_render();
    tracers_render();
    target_outline_render();
    hud_render();
    hud_ui_render();
    interface_render();
    overlay_render();
}

// 0x007C0200 (:1376)
int gamestate_gp_last_havok_sim_time()
{
    return Gameplay_last_havok_simulate;
}

// 0x007EF790 (:1382)
void gamestate_gp_level_init_pre()
{
    gamemusic_update(false);
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        // Single player: the DLC level data, and a fresh level pool unless a DLC bundle is loaded.
        dlc_level_init();
        if (Current_bundle_id == -1 && !Multiplayer && !wrecking_crew_is_active())
        {
            Level_mempool.clear();
        }
    }
    else
    {
        // Multiplayer and Wrecking Crew keep the level pool up to a mark (what the DLC loaded before it).
        if (Dlc_level_mempool_mark != 0)
        {
            Level_mempool.restore_to_mark(Dlc_level_mempool_mark);
        }
        Dlc_level_mempool_mark = Level_mempool.mark();
    }
    GP_TIMED(World.restore_level_init());
    timer_get(TIMER_MILLISECONDS);
    Load_timer_indent += 2;
    if (Render_system_init)
    {
        Render_system_init = false;
        game_render_terrain_level_init();
    }
    Load_timer_indent -= 2;
    GP_TIMED(oml_level_init_pre());
    GP_TIMED(gameplay_level_init_pre());
    GP_TIMED(rfg_prop_level_init_pre());
    GP_TIMED(object_level_init_pre());
    GP_TIMED(dialogue_manager_level_init_pre(); npc_sound_info::level_init_pre(); ai_system_level_init_pre());
    GP_TIMED(public_address_kiosk_level_init_pre());
    GP_TIMED(air_bomb_level_init());
    GP_TIMED(glare_level_init_pre());
    GP_TIMED(los_blocker_level_init_pre());
    GP_TIMED(explosion_level_init_pre());
    GP_TIMED(havok_level_init_pre());
    GP_TIMED(lopm_level_init_pre());
    GP_TIMED(havok_collision_events_level_init_pre());
    GP_TIMED(rfg_pf_level_init_pre());
    GP_TIMED(rfg_level_init_pre());
    GP_TIMED(mission_system_level_init());
    GP_TIMED(script_system_level_init_pre());
    GP_TIMED(mesh_morph_level_init_pre());
    GP_TIMED(gem_level_init_pre());
    GP_TIMED(effects_level_init_pre());
    GP_TIMED(object_effect_level_init_pre());
    GP_TIMED(glass_level_init_pre());
    GP_TIMED(weather_level_init_pre());
    GP_TIMED(undergrowth_level_init());
    GP_TIMED(custom_object_level_init_pre());
    GP_TIMED(object_light_level_init_pre());
    GP_TIMED(Cartographer.level_init_pre());
    GP_TIMED(object_path_road_level_init_pre());
    GP_TIMED(game_time_of_day_level_init_pre());
    GP_TIMED(cutscene_level_init_pre());
    GP_TIMED(salvage_level_init());
    GP_TIMED(tracers_level_init_pre());
    GP_TIMED(target_outline_level_init());
    GP_TIMED(objective_highlight_level_init());
    GP_TIMED(object_skybox_level_init_pre());
    GP_TIMED(ui_level_init_pre());
    GP_TIMED(human_voice_persona_level_init_pre(Multiplayer));
    GP_TIMED(multi_game_camera_level_init_pre());
    if (wrecking_crew_is_active())
    {
        wrecking_crew_level_init_pre();
    }
    if (Multiplayer)
    {
        multi_core_level_init_pre();
        multi_game_level_init_pre();
    }
    gamelog_mark_game_start_time();
}

// 0x007EFCB0 (:1598)
void gamestate_gp_level_init_post()
{
    if (frontend_music_play_id != -1)
    {
        audiolib_play_fade_out(frontend_music_play_id, 1000);
        frontend_music_play_id = -1;
    }
    if (frontend_mp_music_play_id != -1)
    {
        audiolib_play_fade_out(frontend_mp_music_play_id, 1000);
        frontend_mp_music_play_id = -1;
    }
    gamemusic_current_music_type = GAMEMUSIC_MUSIC_NONE;
    if (Multiplayer)
    {
        multi_game_setup_reserved_remote_objects();
    }
    los_queue_level_init_post();
    object_level_init_post();
    gameplay_level_init_post();
    dynamic_hole_requests_level_init_post();
    inv_item_level_init_post();
    traffic_level_init_post();
    camera_level_init_post();
    multi_game_camera_init_post();
    control_level_init_post();
    interface_level_init_post();
    hud_level_init_post();
    ui_level_init_post();
    overlay_level_init_post();
    weapons_level_init_post();
    human_weapons_level_init_post();
    human_voice_level_init_post();
    havok_level_init_post();
    object_debris_level_init_post();
    mesh_deform_level_init_post();
    game_time_of_day_level_init_post();
    weather_level_shutdown();
    object_decal_level_init();
    object_path_road_level_init_post();
    stats_level_init(true);
    if (Multiplayer)
    {
        multi_game_level_init_post();
    }
    while (game_is_paused())
    {
        game_unpause();
    }
    while (get_timestamp_pause() > 0)
    {
        timestamp_unpause();
    }
    key_flush();
    if (!Multiplayer)
    {
        player* pp = player_get_local_player();
        camera_set_transform(pp->pos, pp->orient);
    }
    if (wrecking_crew_is_active())
    {
        wrecking_crew_level_init_post();
    }
    mission_system_level_init_post();
}
