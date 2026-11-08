// Original: volition/rfg/code/cutscene/cutscene.cpp
//
// Real-time cutscenes: a load state and job that read the cutscene and take the player out of the game, then per frame
// the timer walks the shots, which place the actors, play their animations and effects, and the cutscene camera mode
// plays the camera script. Bink movies can play on a mover's material.
//
// Dead in rfg.exe: nothing references cutscene_system_init, nor unused_dcf_cutscene_play, the only caller of
// cutscene_play. So GS_LOAD_CUTSCENE is never registered and Cutscene_is_playing stays false. The boot does not call it
// here either.

#include "volition/rfg/code/cutscene/cutscene.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_material.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_material_library.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/cutscene/cutscene_audio.h"
#include "volition/rfg/code/cutscene/cutscene_internal.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gamewide/game_render_thread.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_spinebend.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_placement.h"
#include "volition/rfg/code/spawn/spawn_request.h"
#include "volition/rfg/code/spawn/spawn_resource_data.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/video_player/video_player.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_allocator.h"
#include "volition/vlib/code/cfile/packfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

extern bool Passive_ai; // 0x01D67DB4 (misc_actions.cpp)

static const unsigned int INVALID_ANIM_INSTANCE_HANDLE = 0xFFFFFFFF; // 0x01343B48

// ---- globals
// Deviation from rfg.exe: external, where it is a file static, as the cutscene files are separate
// translation units here and one there.
cutscene_current_info* Current_cutscene; // 0x01E2B444
cutscene_play_params Cutscene_play_parameters; // 0x01E2B9B0
cutscene_timer Cutscene_timer; // 0x01E2B4AC
bool Cutscene_skip_all; // 0x01E2B44E
char Bink_filename[33]; // 0x01E2B3D0
rfg_mover* Rfg_mover_playing_bink; // 0x01E2B46C
rl_material* Cloned_mat; // 0x01E2B460
rl_material_map* Cloned_mat_map; // 0x01E2B464
rl_material_map* Original_mat_map; // 0x01E2B468

// ---- file statics
static base_array<cutscene_info> Cutscene_infos; // 0x0164BE98
static bool Async_cutscene_load = true; // 0x0164BD19
static bool Cutscene_ambient = true; // 0x0164BD1A, nothing writes it, so the "ambient" console commands never run
static int Cutscene_delay_count = -1; // 0x0164BE84
static void (*Cutscene_delay_function)();
static bool Saved_passive_ai; // 0x01E2B3F1
static bool Cutscene_is_playing; // 0x01E2B448
static bool Cutscene_is_exiting; // 0x01E2B449
static bool Cutscene_rendered_this_frame; // 0x01E2B44A
static bool Cs_load_first_frame; // 0x01E2B44B
static bool Cs_load_in_progress; // 0x01E2B44C
static bool Cs_load_finished; // 0x01E2B44D
static bool Cutscene_do_handshake_bink_cleanup; // 0x01E2B44F
static bool Cutscene_do_handshake_bink_launch; // 0x01E2B45C
static int Interface_mempool_mark; // 0x01E2B454
static int Interface_mempool_mark_gpu; // 0x01E2B458

// 0x00705C20
cutscene_slate::cutscene_slate()
{
}

// 0x00705440
cutscene_navpoint::cutscene_navpoint()
{
}

// 0x00705450
cutscene_npc::cutscene_npc()
{
}

// 0x00705460
cutscene_vehicle::cutscene_vehicle()
{
}

// 0x00705480
cutscene_play_params::cutscene_play_params()
{
}

// 0x00709980
cs_shot_char_tod::cs_shot_char_tod()
{
}

// 0x00709990
cs_shot_char_tod::~cs_shot_char_tod()
{
}

// 0x0070B000
cs_shot_char_tod& cs_shot_char_tod::operator=(const cs_shot_char_tod& __that) = default;

// 0x00709900
cs_shot_tod::cs_shot_tod()
{
}

// 0x00709910
cs_shot_tod::~cs_shot_tod()
{
}

// 0x0070B0A0
cs_shot_tod& cs_shot_tod::operator=(const cs_shot_tod& __that) = default;

// 0x00709810
cs_time_of_day::cs_time_of_day()
{
}

// 0x00709830
cs_time_of_day::~cs_time_of_day()
{
}

// 0x0070B0E0
cs_time_of_day& cs_time_of_day::operator=(const cs_time_of_day& __that) = default;

// 0x00709940
cs_shot_actor::cs_shot_actor()
{
}

// 0x00709960
cs_shot_actor::~cs_shot_actor()
{
}

// 0x0070B020
cs_shot_actor& cs_shot_actor::operator=(const cs_shot_actor& __that) = default;

// 0x00709A10
cs_shot_camera::cs_shot_camera()
{
}

// 0x0070D640
cutscene_shot::cutscene_shot()
{
}

// 0x007099E0
cutscene_shot::~cutscene_shot()
{
}

// 0x0070B140
cutscene_shot& cutscene_shot::operator=(const cutscene_shot& __that) = default;

// 0x00705310 (.h:302)
float cutscene_shot::shot_duration_sec() const
{
    return frames_to_seconds(duration_fm);
}

// 0x00705410 (:70)
float frames_to_seconds(unsigned int frames)
{
    // Magic number: cutscene frames run at 30 per second.
    return (float)frames / 30.0f;
}

// 0x00705490 (:186)
void cutscene_callback_bink_ended(int handle)
{
    Cutscene_do_handshake_bink_cleanup = true;
    if (Current_cutscene != nullptr)
    {
        Current_cutscene->bink_movie_handle = 0;
    }
}

// 0x0070DF00 (:194)
void unused_dcf_cutscene_play()
{
    if (Console_command == 0)
    {
        return;
    }
    // Magic numbers: argument type 2 is a string, 1 makes it optional.
    if (console_get_arg(2, 1) && cutscene_get_cutscene_info(Console_arg_org) != nullptr)
    {
        if (Cutscene_is_playing)
        {
            cutscene_stop();
        }
        Cutscene_skip_all = false;
        const player* const pp = player_get_local_player();
        cutscene_play(Console_arg_org, 0xFFFFFFFF, pp->pos, pp->orient);
    }
}

// 0x007054B0 (:233)
void cutscene_render_loading_screen()
{
}

// 0x0070BEC0 (:244)
cutscene_current_info::cutscene_current_info(cutscene_info* const info)
    : cs_info(info), m_light_set_name(nullptr), m_light_set(nullptr), cur_shot(nullptr), total_cutscene_duration(0.0f),
      player_vehicle_handle(-1), first_frame_fading(false)
{
}

// 0x00705510 (:261)
cs_depth_of_field cs_depth_of_field::operator-(const cs_depth_of_field& other)
{
    cs_depth_of_field result;
    result.max_coc_radius = max_coc_radius - other.max_coc_radius;
    result.preblur_radius = preblur_radius - other.preblur_radius;
    result.m_near = m_near - other.m_near;
    result.ideal_near = ideal_near - other.ideal_near;
    result.ideal_far = ideal_far - other.ideal_far;
    result.m_far = m_far - other.m_far;
    return result;
}

// 0x007055D0 (:276)
cs_depth_of_field cs_depth_of_field::operator+(const cs_depth_of_field& other)
{
    cs_depth_of_field result;
    result.max_coc_radius = other.max_coc_radius + max_coc_radius;
    result.preblur_radius = other.preblur_radius + preblur_radius;
    result.m_near = other.m_near + m_near;
    result.ideal_near = other.ideal_near + ideal_near;
    result.ideal_far = other.ideal_far + ideal_far;
    result.m_far = other.m_far + m_far;
    return result;
}

// 0x00705690 (:291)
cs_depth_of_field operator*(const float scale, const cs_depth_of_field& dof)
{
    cs_depth_of_field result;
    result.max_coc_radius = dof.max_coc_radius * scale;
    result.preblur_radius = dof.preblur_radius * scale;
    result.m_near = dof.m_near * scale;
    result.ideal_near = dof.ideal_near * scale;
    result.ideal_far = dof.ideal_far * scale;
    result.m_far = dof.m_far * scale;
    return result;
}

// 0x00708750 (:311)
unsigned int cutscene_npc_lookup(const char* name)
{
    if (v_stricmp(name, "#Player#") == 0)
    {
        return player_get_local_player()->get_handle();
    }
    for (int i = 0; i < Current_cutscene->cutscene_npcs.size(); i++)
    {
        if (v_stricmp(name, Current_cutscene->cutscene_npcs[i]->name) == 0)
        {
            return Current_cutscene->cutscene_npcs[i]->handle;
        }
    }
    return 0xFFFFFFFF;
}

// 0x007087C0 (:328)
unsigned int cutscene_vehicle_lookup(const char* name)
{
    for (int i = 0; i < Current_cutscene->cutscene_vehicles.size(); i++)
    {
        if (v_stricmp(name, Current_cutscene->cutscene_vehicles[i]->name) == 0)
        {
            return Current_cutscene->cutscene_vehicles[i]->handle;
        }
    }
    return 0xFFFFFFFF;
}

// The visibility of what the human holds; rfg.exe writes this out in cutscene_stop with true.
static void cs_set_weapons_visible(human* hp, const bool visible)
{
    const inventory_item* const item = hp->get_equipped_inv_item();
    if (item == nullptr)
    {
        return;
    }
    object* weapon_object = World.object_from_handle(item->weapon_handle);
    if (weapon_object != nullptr)
    {
        object_set_visibility_plus_children(weapon_object, visible);
    }
    object* const offhand_object = World.object_from_handle(hp->offhand_projectile_handle);
    if (offhand_object != nullptr)
    {
        object_set_visibility_plus_children(offhand_object, visible);
    }
}

// 0x00705730 (:343)
// LTCG: hp in ESI.
static void cutscene_hide_weapons(human* hp)
{
    cs_set_weapons_visible(hp, false);
}

// 0x0070BF70 (:362)
// LTCG: shot in ESI, rack_focus_repair dropped: nothing reads it.
static void cs_update_framebuffer_effects_in_shot(cutscene_shot& shot)
{
    const float elapsed = Cutscene_timer.get_shot_elapsed_sec();
    // Deviation from rfg.exe: get_start_time and get_end_time are accessors added to tweener for fields it reads
    // directly.
    if (shot.camera.orient.get_start_time() < elapsed && shot.camera.orient.is_keyed() && elapsed < shot.camera.orient.get_end_time())
    {
        // Original quirk kept: the result is dropped. Retail lost what it fed.
        shot.camera.orient.get_current_scale(elapsed);
    }
    const float remaining = Cutscene_timer.get_shot_remaining_sec();
    if (0.0f < shot.fade_out_duration_sec && remaining <= shot.fade_out_duration_sec)
    {
        gr_set_color(0, 0, 0, 0xFF);
        shot.fade_out_duration_sec = 0.0f;
    }
    // Magic number: the letterbox starts fading out a second before the shot ends.
    if (shot.fade_out_letterbox && remaining <= 1.0f)
    {
        interface_letterbox_fade_out(false);
        shot.fade_out_letterbox = false;
    }
}

// 0x00705780 (:411)
static void cs_set_cutscene_delay(void (*function)())
{
    if (Cutscene_delay_count < 0)
    {
        // Magic number: two frames, as both callers set a black screen first.
        Cutscene_delay_count = 2;
        Cutscene_delay_function = function;
    }
}

// 0x007057A0 (:425)
// LTCG: hp in ESI.
static void cs_start_for_human(human* hp)
{
    human_spinebend_disable(hp);
    human_update_collision_filter(hp, false);
    human_enable_gravity(hp, false);
    human_in_vehicle_transition(hp, nullptr, VI_INTERACTION_ENTER);
    human_in_vehicle_transition(hp, nullptr, VI_INTERACTION_EXIT);
}

// 0x00708820 (:455)
static void cs_start_for_npcs(const cutscene_current_info* const cur_cutscene)
{
    for (int i = 0; i < cur_cutscene->cutscene_npcs.size(); i++)
    {
        const unsigned int handle = cur_cutscene->cutscene_npcs[i]->handle;
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* const objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            human* const hp = (human*)objp;
            cs_start_for_human(hp);
            object_set_visibility_plus_children(hp, false);
        }
    }
}

// 0x007088B0 (:468)
// LTCG: cur_cutscene in EDI.
static void cs_start_for_vehicles(const cutscene_current_info* const cur_cutscene)
{
    for (int i = 0; i < cur_cutscene->cutscene_vehicles.size(); i++)
    {
        const unsigned int handle = cur_cutscene->cutscene_vehicles[i]->handle;
        if (handle == 0xFFFFFFFF)
        {
            continue;
        }
        const object* const objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            vehicle_disable_collisions(objp->get_handle(), true);
        }
    }
}

// 0x007057D0 (:481)
static void cs_exit_for_human(human* hp)
{
    human_spinebend_enable(hp);
    human_update_collision_filter(hp, true);
    human_enable_gravity(hp, true);
    // Magic number: clears bit 2 of the character instance's flags.
    hp->char_inst->ci_flags &= 0xFFFB;
}

// 0x00705800 (:495)
static void cutscene_load_mempools_set_thread_ownership()
{
    Common_mempool.set_thread_ownership();
    Cutscene_mempool.set_thread_ownership();
    Interface_mempool.set_thread_ownership();
    Level_mempool.set_thread_ownership();
    Stream_human_allocator.set_thread_ownership();
    Stream_player_allocator.set_thread_ownership();
    Stream_vehicle_allocator.set_thread_ownership();
    Stream_large_weapon_allocator.set_thread_ownership();
    Stream_small_weapon_allocator.set_thread_ownership();
    packfile_set_thread_ownership();
}

// 0x0070C050 (:514)
static void cs_initialize_shot_times(cutscene_current_info* const cur_cutscene)
{
    const c_cutscene_camera* const script = Rfg_camera.cscript_params.camera_script;
    float total = 0.0f;
    if (script != nullptr)
    {
        for (int i = 0; i < script->csc_num_shots; i++)
        {
            const c_cutscene_shot& shot = script->csc_shots[i];
            if (i < cur_cutscene->m_shots.size())
            {
                cur_cutscene->m_shots[i].start_time = shot.start_time;
            }
            total = (float)((double)shot.end_time - (double)shot.start_time + (double)total);
        }
    }
    // Original quirk kept: with no camera script the duration is zero, so the cutscene ends on its first frames.
    cur_cutscene->total_cutscene_duration = total;
}

// 0x0070FE90 (:538)
static void cs_play_internal()
{
    Cutscene_do_handshake_bink_cleanup = false;
    Cutscene_do_handshake_bink_launch = false;
    cutscene_info* const info = cutscene_get_cutscene_info(Cutscene_play_parameters.name);
    cs_manage_audio_start();
    Current_cutscene = new (Cutscene_mempool) cutscene_current_info(info);
    Current_cutscene->m_slate_handle = Cutscene_play_parameters.slate_handle != 0xFFFFFFFF
                                           ? Cutscene_play_parameters.slate_handle
                                           : cutscene_get_closest_slate_handle();
    Current_cutscene->post_cutscene_restore_pos = Cutscene_play_parameters.restore_pos;
    Current_cutscene->post_cutscene_restore_orient = Cutscene_play_parameters.restore_orient;
    Cutscene_is_playing = true;
    Cutscene_is_exiting = false;
    Num_cutscene_navpoints = 0;
    Num_cutscene_npcs = 0;
    Num_cutscene_vehicles = 0;
    game_render_thread_wait_for_prev_done(false);
    Saved_passive_ai = Passive_ai;
    Passive_ai = true;
    control_gameplay_reset_movement_controls();
    if (!Cutscene_ambient)
    {
        console_do_command("ambient off");
    }

    player* pp = player_get_local_player();
    if (human_is_ragdolled(pp))
    {
        human_ragdoll_get_up(pp, true);
        human_ragdoll_stop(pp, false);
    }
    human_set_stance(pp, HST_STAND);
    weapon* const equipped = pp->get_equipped_weapon();
    if (equipped != nullptr)
    {
        equipped->end_looping_effects(true);
    }
    player_firearm_attack_stop(false);
    if (human_in_vehicle(pp) != nullptr)
    {
        Current_cutscene->player_vehicle_handle = pp->vehicle_handle;
        vi_exit_data exit_data;
        exit_data.flags.teleport = 1;
        vehicle_interaction_request_exit(pp, &exit_data);
    }

    char spawn_data_name[64];
    sprintf(spawn_data_name, "cs_%s", Current_cutscene->cs_info->name);
    srm_mission_reservation(spawn_res_data_find(spawn_data_name));
    cutscene_load_current_data(Current_cutscene);
    cs_initialize_shot_times(Current_cutscene);
    projectile_destroy_all();
    World.stream_hard_wait();
    World.stream_pause(true);

    cs_start_for_human(player_get_local_player());
    cs_start_for_npcs(Current_cutscene);
    cs_start_for_vehicles(Current_cutscene);
    Current_cutscene->m_light_set = nullptr;
    // rfg.exe inlines this call.
    cs_audio_prepare_blocking(Current_cutscene);
    Cutscene_timer.start_playing(Current_cutscene->total_cutscene_duration);
    camera_start_cutscene_mode();
    Current_cutscene->first_shot_processed = false;
}

// 0x0070DF60 (:797)
// LTCG: shot_number in EAX.
static void cs_start_effects_for_shot(int shot_number)
{
    const cutscene_shot& shot = Current_cutscene->m_shots[shot_number];
    for (int i = 0; i < shot.m_effect_infos.size(); i++)
    {
        vector pos;
        matrix orient;
        if (Current_cutscene->slates.size() < 1)
        {
            pos = ZERO_VECTOR;
            orient = IDENTITY_MATRIX;
        }
        else
        {
            const cutscene_slate& slate = Current_cutscene->slates[Current_cutscene->current_slate];
            pos = slate.m_world_pos;
            orient = slate.m_rotation;
        }
        const unsigned int effect_handle = effects_play(shot.m_effect_infos[i].handle, pos, orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        if (effect_handle != 0xFFFFFFFF)
        {
            // rfg.exe inlines this call.
            cutscene_add_effect_object((int)effect_handle);
        }
    }
}

// 0x00705860 (:825)
// LTCG: shot in ESI, name dropped: rfg.exe passes only the handle.
static void cs_play_animation_on_character(int aim_handle, cs_shot_char_animation& shot)
{
    float start_time = (Cutscene_timer.get_shot_elapsed_sec() - frames_to_seconds(shot.get_shot_start_frame())) + frames_to_seconds(shot.start_frame);
    if (start_time < 0.0f)
    {
        start_time = 0.0f;
    }
    animlib_clear_non_looping_animations(aim_handle);
    // Magic number: the flags cutscene animations always play with.
    shot.anim_inst = animlib_play_animation(aim_handle, shot.anim_handle, 0x44, start_time, 1.0f, 1.0f, frames_to_seconds(shot.get_ramp_in()));
}

// 0x00708900 (:863)
// Starts and stops the actor's animations that are due. Original quirk kept: no null check of the actor's object.
static void cs_update_animations_in_shot_for_character(cs_shot_actor& character, unsigned int shot_frame)
{
    object* const objp = World.object_from_handle(character.handle);
    for (int i = 0; i < character.m_animations.size(); i++)
    {
        cs_shot_char_animation& animation = character.m_animations[i];
        if (animation.should_start(shot_frame))
        {
            if (objp->get_obj_type() == OT_HUMAN)
            {
                cs_play_animation_on_character(((human*)objp)->char_inst->aim_handle, animation);
            }
            else if (objp->get_obj_type() == OT_VEHICLE)
            {
                cs_play_animation_on_character(((vehicle*)objp)->aim_handle, animation);
            }
            else
            {
                keen::debug::nativePrintArgs("Unknown object type having animation played!");
            }
        }
        else if (animation.should_stop(shot_frame))
        {
            if (objp->get_obj_type() == OT_HUMAN)
            {
                vanim_ramp_out_action(((human*)objp)->char_inst->vp, animation.anim_inst, animation.get_ramp_out());
            }
            else if (objp->get_obj_type() == OT_VEHICLE)
            {
                vanim_ramp_out_action(((vehicle*)objp)->vp, animation.anim_inst, animation.get_ramp_out());
            }
            else
            {
                keen::debug::nativePrintArgs("Unknown object type having animation stopped!");
            }
            animation.anim_inst = INVALID_ANIM_INSTANCE_HANDLE;
        }
    }
}

// The human an actor's handle names, the local player counting only as a player. rfg.exe writes this out twice.
static human* cs_get_actor_human(const unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const bool is_local_player = object_is_local_player(handle);
    object* const objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || (is_local_player && objp->get_sub_type() != OT_SUB_HUMAN_PLAYER))
    {
        return nullptr;
    }
    return (human*)objp;
}

// The actor's position and orientation in the world. rfg.exe writes this out twice.
static void cs_get_actor_world_transform(const cs_shot_actor& actor, vector& pos, matrix& orient)
{
    pos = actor.position;
    const vector forward(0.0f, 0.0f, 1.0f);
    orient.make_quick(forward);
    orient.rotate_around_local_y(actor.orient_y);
    cutscene_transform_point(pos, pos);
    cutscene_rotate_orient(orient, orient);
}

// 0x0070C0E0 (:924)
static void cs_start_characters_for_shot(cutscene_shot& shot)
{
    for (int i = 0; i < shot.m_characters.size(); i++)
    {
        cs_shot_actor& character = shot.m_characters[i];
        if (character.handle == 0xFFFFFFFF)
        {
            character.handle = cutscene_npc_lookup(character.actor_name);
        }
        human* const hp = cs_get_actor_human(character.handle);
        if (hp == nullptr)
        {
            continue;
        }
        human_controller_set_vel(hp, ZERO_VECTOR);
        object_set_visibility_plus_children(hp, true);
        // rfg.exe inlines this call.
        cutscene_hide_weapons(hp);
        vector pos;
        matrix orient;
        cs_get_actor_world_transform(character, pos, orient);
        if (!character.options.dont_reposition)
        {
            hp->update_pos_and_orient(pos, orient, true);
        }
        character.cur_playing_anim = -1;
        cs_update_animations_in_shot_for_character(character, 0);
    }
}

// 0x0070C2B0 (:1006)
static void cs_start_vehicles_for_shot(cutscene_shot& shot)
{
    for (int i = 0; i < shot.m_vehicles.size(); i++)
    {
        cs_shot_actor& character = shot.m_vehicles[i];
        if (character.handle == 0xFFFFFFFF)
        {
            character.handle = cutscene_vehicle_lookup(character.actor_name);
        }
        object* const objp = character.handle != 0xFFFFFFFF ? World.object_from_handle(character.handle) : nullptr;
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
        {
            keen::debug::nativePrintArgs("Unable to find vehicle for cutscene.");
            continue;
        }
        vehicle* const vp = (vehicle*)objp;
        vector pos;
        matrix orient;
        cs_get_actor_world_transform(character, pos, orient);
        vp->update_pos_and_orient(pos, orient, true);
        vp->last_anim_transform = matrix43(vp->pos, vp->orient);
        cs_update_animations_in_shot_for_character(character, 0);
    }
}

// 0x00708A10 (:1054)
static void cs_update_animations_in_shot(cutscene_shot& shot, unsigned int shot_frame)
{
    for (int i = 0; i < shot.m_characters.size(); i++)
    {
        cs_shot_actor& character = shot.m_characters[i];
        if (character.handle == 0xFFFFFFFF)
        {
            character.handle = cutscene_npc_lookup(character.actor_name);
        }
        if (cs_get_actor_human(character.handle) != nullptr)
        {
            cs_update_animations_in_shot_for_character(character, shot_frame);
        }
    }
    for (int i = 0; i < shot.m_vehicles.size(); i++)
    {
        cs_shot_actor& character = shot.m_vehicles[i];
        if (character.handle == 0xFFFFFFFF)
        {
            // rfg.exe inlines this call.
            character.handle = cutscene_vehicle_lookup(character.actor_name);
        }
        const object* const objp = character.handle != 0xFFFFFFFF ? World.object_from_handle(character.handle) : nullptr;
        if (objp != nullptr && objp->get_obj_type() == OT_VEHICLE)
        {
            cs_update_animations_in_shot_for_character(character, shot_frame);
        }
    }
}

// 0x00705950 (:1100)
// LTCG: vp in ESI.
static void cs_kill_character_animations(v_animate* vp)
{
    if (vp != nullptr && vp->aim_handle != -1)
    {
        animlib_clear_non_looping_animations(vp->aim_handle);
        animlib_clear_looping_animations(vp->aim_handle, false);
    }
}

// 0x00705980 (:1115)
static void cutscene_load_enter(game_state prev_state)
{
    Cs_load_first_frame = true;
}

// 0x00705990 (:1122)
static void cutscene_load_exit(game_state new_state)
{
}

// 0x00710200 (:1129)
void cs_play_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    scheduler_get_this_context_controller();
    cutscene_load_mempools_set_thread_ownership();
    cs_play_internal();
    Cs_load_finished = true;
}

// 0x00710220 (:1146)
static void cutscene_start_load()
{
    cpu_job job;
    job.trigger = nullptr;
    job.flags = 0;
    job.scratch_buffer = nullptr;
    job.scratch_buffer_size = 0;
    job.thread_id = get_current_thread_id();
    job.job_func = cs_play_job;
    job.type = ccjt_havok;
    scheduler_add_job(nullptr, &job, nullptr);
}

// 0x00710270 (:1157)
static void cutscene_load_process()
{
    if (Cs_load_first_frame)
    {
        Cs_load_first_frame = false;
        return;
    }
    if (!Async_cutscene_load)
    {
        cs_play_internal();
        gameseq_pop_state();
        return;
    }
    if (!Cs_load_in_progress)
    {
        cutscene_start_load();
        Cs_load_in_progress = true;
        os_sleep(10);
        return;
    }
    if (!Cs_load_finished)
    {
        os_sleep(20);
        return;
    }
    Cs_load_first_frame = false;
    Cs_load_in_progress = false;
    Cs_load_finished = false;
    cutscene_load_mempools_set_thread_ownership();
    gameseq_pop_state();
}

// 0x007059A0 (:1206)
static void cutscene_load_render(rl_renderer*)
{
}

// 0x007102E0 (:1218)
void cutscene_system_init()
{
    cutscene_read_cutscene_list(Cutscene_infos, false);
    state_info info;
    info.enter = cutscene_load_enter;
    info.exit = cutscene_load_exit;
    info.process = cutscene_load_process;
    info.handshake = nullptr;
    info.render = cutscene_load_render;
    info.transparent = false;
    info.pause_beneath = false;
    info.hide_cursor = true;
    gameseq_register_state(GS_LOAD_CUTSCENE, info);
}

// 0x007059B0 (:1236)
void cutscene_level_init_pre()
{
}

// 0x0070E130 (:1240)
void cutscene_level_shutdown()
{
    if (Cutscene_is_playing)
    {
        cutscene_stop();
    }
    Rfg_mover_playing_bink = nullptr;
}

// 0x007059C0 (:1253)
const cutscene_current_info* cutscene_get_current_cutscene_info()
{
    return Current_cutscene;
}

// 0x007059D0 (:1260)
cutscene_current_info* cutscene_get_current()
{
    return Current_cutscene;
}

// 0x00708B90 (:1271)
cutscene_info* cutscene_get_cutscene_info(const char* name)
{
    for (int i = 0; i < Cutscene_infos.size(); i++)
    {
        if (v_stricmp(Cutscene_infos[i].name, name) == 0)
        {
            return &Cutscene_infos[i];
        }
    }
    return nullptr;
}

// 0x00708BD0 (:1290)
const cutscene_info* cutscene_get_cutscene_info(const checksum_stri& name)
{
    for (int i = 0; i < Cutscene_infos.size(); i++)
    {
        if (checksum_stri(Cutscene_infos[i].name, 0) == name)
        {
            return &Cutscene_infos[i];
        }
    }
    return nullptr;
}

// 0x00708C20 (:1303)
const cutscene_info* cutscene_get_cutscene_info(int index)
{
    // Original quirk kept: an index past the end answers the entry one past the last.
    if (index < 0)
    {
        index = 0;
    }
    else if (Cutscene_infos.size() < index)
    {
        index = Cutscene_infos.size();
    }
    return &Cutscene_infos.elements()[index];
}

// 0x00708C50 (:1312)
int cutscene_num_cutscenes()
{
    return Cutscene_infos.size();
}

// 0x007059E0 (:1327)
void cutscene_play(const char* name, unsigned int slate_handle, const vector& restore_pos, const matrix& restore_orient)
{
    const player* const pp = player_get_local_player();
    if (pp == nullptr || pp->is_killed())
    {
        return;
    }
    // Original quirk kept: no bound on the name, into a 32 byte field.
    strcpy(Cutscene_play_parameters.name, name);
    Cutscene_play_parameters.slate_handle = slate_handle;
    Cutscene_play_parameters.restore_pos = restore_pos;
    Cutscene_play_parameters.restore_orient = restore_orient;
    gameseq_push_state(GS_LOAD_CUTSCENE, false, false);
}

// 0x00705A90 (:1344)
void cutscene_finalize_do()
{
    if (Current_cutscene != nullptr)
    {
        player* const pp = player_get_local_player();
        if (pp != nullptr)
        {
            human_set_movement_mode(pp, HMM_NONE, HMSM_NONE);
        }
    }
}

// 0x00710340 (:1371)
void cutscene_player_request_skip()
{
    gr_set_color(0, 0, 0, 0xFF);
    cs_set_cutscene_delay(cutscene_stop);
}

// 0x0070E150 (:1379)
void cutscene_do_stop()
{
    gr_set_color(0, 0, 0, 0xFF);
    cs_set_cutscene_delay(cutscene_stop);
}

// 0x0070C440 (:1389)
void cutscene_stop()
{
    if (!Cutscene_is_playing)
    {
        return;
    }
    cs_kill_character_animations(player_get_local_player()->char_inst->vp);
    for (int i = 0; i < Current_cutscene->cutscene_npcs.size(); i++)
    {
        const npc* const np = World.object_from_handle<npc>(Current_cutscene->cutscene_npcs[i]->handle);
        if (np != nullptr)
        {
            cs_kill_character_animations(np->char_inst->vp);
        }
    }
    for (int i = 0; i < Current_cutscene->cutscene_vehicles.size(); i++)
    {
        const vehicle* const vp = World.object_from_handle<vehicle>(Current_cutscene->cutscene_vehicles[i]->handle);
        if (vp != nullptr)
        {
            cs_kill_character_animations(vp->vp);
        }
    }
    World.stream_pause(false);

    player* const pp = player_get_local_player();
    if (pp != nullptr)
    {
        if (Current_cutscene->post_cutscene_restore_pos != ZERO_VECTOR)
        {
            pp->update_pos_and_orient(Current_cutscene->post_cutscene_restore_pos, Current_cutscene->post_cutscene_restore_orient, true);
        }
        Current_cutscene->post_cutscene_restore_pos = ZERO_VECTOR;
        cs_exit_for_human(pp);
        animlib_clear_non_looping_animations(pp->char_inst->vp->aim_handle);
        animlib_clear_looping_animations(pp->char_inst->vp->aim_handle, false);
        cs_set_weapons_visible(pp, true);
    }
    if (Current_cutscene->bink_movie_handle != 0)
    {
        video_player_stop(Current_cutscene->bink_movie_handle);
    }
    game_render_thread_wait_for_prev_done(false);
    camera_stop_cutscene_mode();
    Cutscene_timer.clear();
    Cutscene_is_playing = false;

    for (int i = 0; i < Current_cutscene->m_effect_object_handles.size(); i++)
    {
        object_effect* const effect = World.object_from_handle<object_effect>(Current_cutscene->m_effect_object_handles[i]);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    cutscene_cancel_audio_prime(Current_cutscene);
    Current_cutscene->soundtrack.play_id = -1;
    for (int i = 0; i < Current_cutscene->cutscene_npcs.size(); i++)
    {
        const npc* const np = World.object_from_handle<npc>(Current_cutscene->cutscene_npcs[i]->handle);
        if (np != nullptr)
        {
            World.destroy_object(np->get_handle(), false);
        }
    }
    for (int i = 0; i < Current_cutscene->cutscene_vehicles.size(); i++)
    {
        const vehicle* const vp = World.object_from_handle<vehicle>(Current_cutscene->cutscene_vehicles[i]->handle);
        if (vp != nullptr)
        {
            World.destroy_object(vp->get_handle(), false);
        }
    }
    Current_cutscene->cutscene_npcs.init(nullptr, 0, 0);
    Current_cutscene->cutscene_vehicles.init(nullptr, 0, 0);
    cutscene_unload_data(Current_cutscene);
    srm_mission_release();
    for (int i = 0; i < Current_cutscene->m_shots.size(); i++)
    {
        const base_array<cutscene_effect>& effects = Current_cutscene->m_shots[i].m_effect_infos;
        for (int j = 0; j < effects.size(); j++)
        {
            // Original quirk kept: an effect whose handle was -1 had neither field set, so this reads uninitialized ones.
            if (!effects[j].preloaded)
            {
                stream2_release(effects[j].srid);
            }
        }
    }
    Passive_ai = Saved_passive_ai;
    hud_hide(false);
    interface_letterbox_fade_out(true);
    gr_set_color(0, 0, 0, 0xFF);
    if (!Cutscene_ambient)
    {
        console_do_command("ambient on");
    }
    Current_cutscene = nullptr;
    Cutscene_mempool.clear();
}

// 0x00710380 (:1615)
void cutscene_do_frame()
{
    if (!Cutscene_is_playing)
    {
        return;
    }
    // rfg.exe inlines this call.
    cutscene_hide_weapons(player_get_local_player());
    if (Cutscene_is_exiting)
    {
        Cutscene_is_exiting = false;
        return;
    }
    Cutscene_timer.advance(Frametime_real);
    Cutscene_rendered_this_frame = true;
    if (-1 < Cutscene_delay_count)
    {
        Cutscene_delay_count--;
        if (Cutscene_delay_count < 0)
        {
            Cutscene_delay_function();
            Cutscene_delay_function = nullptr;
        }
    }
    if (Cutscene_is_playing && Current_cutscene != nullptr && Cutscene_delay_function == nullptr && cutscene_is_finishing())
    {
        cutscene_do_stop();
    }
}

// 0x00705AB0 (:1672)
void cutscene_interface_render()
{
}

// 0x00705AC0 (:1678)
void cutscene_handshake()
{
}

// 0x00705AD0 (:1686)
bool cutscene_is_playing()
{
    return Cutscene_is_playing;
}

// 0x00708C60 (:1695)
bool cutscene_is_finishing()
{
    return Cutscene_is_playing && Cutscene_timer.has_cutscene_elapsed();
}

// 0x00705AE0 (:1710)
int cutscene_get_cur_shot()
{
    return Cutscene_is_playing ? Cutscene_timer.get_cur_shot_index() : -1;
}

// 0x00705B00 (:1724)
bool cutscene_get_no_ai_bbox(vector& bbox_min, vector& bbox_max)
{
    if (Cutscene_is_playing && Current_cutscene->bbox_min != ZERO_VECTOR && Current_cutscene->bbox_max != ZERO_VECTOR)
    {
        bbox_min = Current_cutscene->bbox_min;
        bbox_max = Current_cutscene->bbox_max;
        return true;
    }
    return false;
}

// 0x00705BE0 (:1741)
void cutscene_set_no_ai_bbox(const vector& bbox_min, const vector& bbox_max)
{
    if (Current_cutscene != nullptr)
    {
        Current_cutscene->bbox_min = bbox_min;
        Current_cutscene->bbox_max = bbox_max;
    }
}

// 0x00705C10 (:1751)
void cutscene_remove_objects()
{
}

// 0x0070C880 (:1866)
void cutscene_set_offset_and_rotation(const cutscene_navpoint& offset_nav, unsigned int slate_handle)
{
    cutscene_slate slate;
    slate.m_loaded = true;
    slate.m_cutscene_pos = offset_nav.pos;
    for (int i = 0; i < World.get_num_objects<object_dummy>(); i++)
    {
        const object_dummy* const dummy = World.get_object<object_dummy>(i);
        if (dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_CUTSCENE && dummy->get_handle() == slate_handle)
        {
            slate.m_object_handle = dummy->get_handle();
            slate.m_world_pos = dummy->pos;
            matrix rotation(offset_nav.orient);
            rotation.transpose();
            slate.m_rotation = rotation * dummy->orient;
            Current_cutscene->slates.add(slate);
            return;
        }
    }
    // No such slate: play it on the ground 5 m in front of the camera. Magic number: 0.05 m above the ground.
    slate.m_object_handle = 0xFFFFFFFF;
    slate.m_world_pos.set(Rfg_camera.real_orient.fvec.x * 5.0f + Rfg_camera.real_pos.x, Rfg_camera.real_orient.fvec.y * 5.0f + Rfg_camera.real_pos.y, Rfg_camera.real_orient.fvec.z * 5.0f + Rfg_camera.real_pos.z);
    havok_terrain_height(slate.m_world_pos.y, slate.m_world_pos.x, slate.m_world_pos.z, false, nullptr);
    slate.m_world_pos.y = (float)((double)slate.m_world_pos.y + 0.05);
    slate.m_rotation = offset_nav.orient;
    slate.m_rotation.transpose();
    Current_cutscene->slates.add(slate);
}

// The slate rotation applied to a point, the three columns weighted by its coordinates.
static vector cs_rotate_point(const matrix& m, const vector& p)
{
    return vector(m.rvec.x * p.x + m.uvec.x * p.y + m.fvec.x * p.z, m.rvec.y * p.x + m.uvec.y * p.y + m.fvec.y * p.z, m.rvec.z * p.x + m.uvec.z * p.y + m.fvec.z * p.z);
}

// A point offset and rotated by the current shot's fiddle, when there is a current shot.
static vector cs_apply_shot_fiddle(const vector& p)
{
    const cutscene_shot* const shot = Current_cutscene->cur_shot;
    if (shot == nullptr)
    {
        return p;
    }
    const vector offset(shot->slate_fiddle_pos.x + p.x, shot->slate_fiddle_pos.y + p.y, shot->slate_fiddle_pos.z + p.z);
    matrix fiddle(IDENTITY_MATRIX);
    fiddle.rotate_around_local_y(shot->slate_fiddle_orient);
    return cs_rotate_point(fiddle, offset);
}

// Whether the current slate can be read, as the two transforms check.
static bool cs_has_valid_slate()
{
    return Current_cutscene != nullptr && Current_cutscene->slates.size() != 0 && -1 < Current_cutscene->current_slate && Current_cutscene->current_slate < Current_cutscene->slates.size();
}

// 0x00708C90 (:1928)
void cutscene_transform_point(vector& new_pos, const vector& old_pos)
{
    if (!cs_has_valid_slate())
    {
        return;
    }
    const cutscene_slate& slate = Current_cutscene->slates[Current_cutscene->current_slate];
    const vector rotated = cs_rotate_point(slate.m_rotation, cs_apply_shot_fiddle(old_pos));
    new_pos.set(slate.m_world_pos.x + rotated.x, slate.m_world_pos.y + rotated.y, slate.m_world_pos.z + rotated.z);
}

// 0x007090A0 (:1961)
void cutscene_transform_point_for_camera(vector& new_pos, const vector& old_pos)
{
    if (!cs_has_valid_slate())
    {
        return;
    }
    const cutscene_slate& slate = Current_cutscene->slates[Current_cutscene->current_slate];
    // Around the slate's point in the cutscene, then moved to the slate in the world.
    const vector local(old_pos.x - slate.m_cutscene_pos.x, old_pos.y - slate.m_cutscene_pos.y, old_pos.z - slate.m_cutscene_pos.z);
    const vector rotated = cs_rotate_point(slate.m_rotation, cs_apply_shot_fiddle(local));
    new_pos.set(rotated.x + slate.m_world_pos.x, rotated.y + slate.m_world_pos.y, rotated.z + slate.m_world_pos.z);
}

// 0x00709580 (:1994)
void cutscene_rotate_orient(matrix& new_orient, const matrix& old_orient)
{
    if (Current_cutscene == nullptr || Current_cutscene->slates.size() == 0)
    {
        return;
    }
    new_orient = old_orient * Current_cutscene->slates[Current_cutscene->current_slate].m_rotation;
    if (Current_cutscene->cur_shot != nullptr)
    {
        matrix fiddle(IDENTITY_MATRIX);
        fiddle.rotate_around_local_y(Current_cutscene->cur_shot->slate_fiddle_orient);
        new_orient = new_orient * fiddle;
    }
}

// 0x00709670 (:2016)
bool cutscene_select_slate(unsigned int slate_handle, bool for_transform)
{
    for (int i = 0; i < Current_cutscene->slates.size(); i++)
    {
        if (Current_cutscene->slates[i].m_object_handle == slate_handle)
        {
            Current_cutscene->current_slate = i;
            return true;
        }
    }
    return false;
}

// 0x0070CB50 (:2044)
void cutscene_add_effect_object(int handle)
{
    if (Cutscene_is_playing && Current_cutscene->m_effect_object_handles.size() < Current_cutscene->m_effect_object_handles.capacity())
    {
        Current_cutscene->m_effect_object_handles.add(handle);
    }
}

// 0x0070E190 (:2058)
void cutscene_start_next_shot(int shot_number)
{
    // Original quirk kept: shot_number may equal the count, one past the last shot.
    if (Current_cutscene == nullptr || shot_number < 0 || Current_cutscene->m_shots.size() < shot_number)
    {
        return;
    }
    if (!Current_cutscene->first_shot_processed)
    {
        camera_start_cutscene_mode();
    }
    if (shot_number == 0)
    {
        hud_hide(true);
    }
    cutscene_shot& shot = Current_cutscene->m_shots.elements()[shot_number];
    Current_cutscene->cur_shot = &shot;
    // rfg.exe also prints "shot%u" with the number plus one to a local buffer it never reads.
    cs_kill_character_animations(player_get_local_player()->char_inst->vp);
    cutscene_select_slate(Current_cutscene->m_slate_handle, true);
    cs_start_characters_for_shot(shot);
    cs_start_vehicles_for_shot(shot);
    cs_start_effects_for_shot(shot_number);
    if (!Current_cutscene->first_shot_processed)
    {
        cutscene_audio_play(Current_cutscene);
    }
    cutscene_set_options_for_shot(shot);
    cs_update_framebuffer_effects_in_shot(shot);
    if (!Current_cutscene->first_shot_processed)
    {
        interface_letterbox_fade_in(true);
    }
    Current_cutscene->first_shot_processed = true;
}

// 0x0070CB80 (:2218)
void cutscene_update_shot(int shot_number)
{
    // Original quirk kept: as in cutscene_start_next_shot, the count passes.
    if (Current_cutscene == nullptr || shot_number < 0 || Current_cutscene->m_shots.size() < shot_number)
    {
        return;
    }
    cutscene_shot& shot = Current_cutscene->m_shots.elements()[shot_number];
    const float elapsed = Cutscene_timer.get_shot_elapsed_sec();
    // Magic number: 30 frames per second. Truncated: rfg.exe sets the x87 control word to chop for this.
    cs_update_animations_in_shot(shot, (unsigned int)(long long)((double)elapsed * 30.0));
    if (shot.camera.position.is_keyed() || shot.camera.dolly_in_pct.is_keyed())
    {
        // Original quirk kept: these three results are dropped. Retail lost what used them; the camera script drives the
        // camera in camera_cs_process.
        shot.camera.position.get_current_value(elapsed);
        shot.camera.dolly_in_pct.get_current_value(elapsed);
        if (Current_cutscene->camera_orientation.is_keyed())
        {
            Current_cutscene->camera_orientation.get_current_value(elapsed);
        }
    }
    cs_update_framebuffer_effects_in_shot(shot);
}

// 0x00705C30 (:2262)
bool cutscene_disallow_pause()
{
    // Magic number: no pausing in the first quarter of a second.
    return Current_cutscene != nullptr && Cutscene_timer.get_cutscene_elapsed_sec() <= 0.25f;
}

// 0x007096A0 (:2275)
vector cutscene_get_slate_position()
{
    if (Current_cutscene != nullptr && 0 < Current_cutscene->slates.size() && -1 < Current_cutscene->current_slate)
    {
        const cutscene_slate& slate = Current_cutscene->slates[Current_cutscene->current_slate];
        return slate.m_loaded ? slate.m_world_pos : slate.m_cutscene_pos;
    }
    return Rfg_camera.real_pos;
}

// 0x00705C60 (:2288)
bool cutscene_current_is_first_shot_processed()
{
    return Current_cutscene->first_shot_processed;
}

// 0x00705C70 (:2296)
char* str_new_copy(const char* const source, mempool_base& pool)
{
    if (source == nullptr)
    {
        return nullptr;
    }
    const unsigned int size = (unsigned int)strlen(source) + 1;
    char* const copy = (char*)pool.alloc(size, 4);
    if (copy == nullptr)
    {
        return nullptr;
    }
    memcpy(copy, source, size);
    return copy;
}

// 0x00709730 (:2311)
void cutscene_reset_mover_for_bink()
{
    if (Cloned_mat != nullptr)
    {
        rl_material_library_unregister(Cloned_mat);
        Cloned_mat->release();
        Cloned_mat = nullptr;
    }
    if (Cloned_mat_map != nullptr)
    {
        Cloned_mat_map->release();
        Cloned_mat_map = nullptr;
    }
    if (Rfg_mover_playing_bink != nullptr)
    {
        Rfg_mover_playing_bink->rdd.get_data()->model_instance_p->set_material_map(Original_mat_map);
        Rfg_mover_playing_bink = nullptr;
    }
}

// 0x0070CC70 (:2338)
void cutscene_setup_mover_for_bink(rfg_mover* rm)
{
    Original_mat_map = chunk_get_base_by_uid(rm->chunk_uid)->material_map_p.to_pointer();
    Cloned_mat_map = Original_mat_map->clone(&Cutscene_mempool);
    const int index = Cloned_mat_map->find_material_index(Current_cutscene->bink_material_name);
    if (index == -1)
    {
        return;
    }
    const unsigned int material = Cloned_mat_map->get_material(index);
    if (material == 0xFFFFFFFF)
    {
        return;
    }
    Cloned_mat = (rl_material*)rl_material_library_clone(material, &Cutscene_mempool);
    if (Cloned_mat == nullptr)
    {
        return;
    }
    int y_texture;
    int cb_texture;
    int cr_texture;
    if (!video_player_get_texture_handles(Current_cutscene->bink_movie_handle, y_texture, cb_texture, cr_texture))
    {
        Cloned_mat->release();
        Cloned_mat = nullptr;
        return;
    }
    Cloned_mat->set_texture_data("Y_Map", y_texture);
    Cloned_mat->set_texture_data("Cb_Map", cb_texture);
    Cloned_mat->set_texture_data("Cr_Map", cr_texture);
    Cloned_mat_map->set_material(index, rl_material_library_register(Cloned_mat));
    rm->rdd.get_data()->model_instance_p->set_material_map(Cloned_mat_map);
    Rfg_mover_playing_bink = rm;
}

// 0x0070CDD0 (:2394)
void cutscene_do_handshake()
{
    if (Cutscene_do_handshake_bink_cleanup)
    {
        cutscene_reset_mover_for_bink();
        // Magic number: pool flag 0x40, the interface pool's CPU and GPU sides are marked apart.
        if ((Interface_mempool.get_flags() & 0x40) == 0)
        {
            Interface_mempool.restore_to_mark(Interface_mempool_mark);
        }
        else
        {
            Interface_mempool.restore_to_mark_cpu(Interface_mempool_mark);
            Interface_mempool.restore_to_mark_gpu(Interface_mempool_mark_gpu);
        }
        hud_ui_reload_items();
        Cutscene_do_handshake_bink_cleanup = false;
    }
    if (Cutscene_do_handshake_bink_launch)
    {
        if (Current_cutscene == nullptr)
        {
            // Original quirk kept: this leaves the launch flag set and Cutscene_rendered_this_frame as it was.
            return;
        }
        hud_ui_unload_items();
        if ((Interface_mempool.get_flags() & 0x40) == 0)
        {
            Interface_mempool_mark = Interface_mempool.mark();
        }
        else
        {
            Interface_mempool_mark = Interface_mempool.mark_cpu();
            Interface_mempool_mark_gpu = Interface_mempool.mark_gpu();
        }
        // Magic numbers: a 10 by 10 window at the corner, silent; the picture is only used as a texture.
        Current_cutscene->bink_movie_handle = video_player_play(Bink_filename, VPM_LOOPING, 0, 0, 10, 10, 0.0f, true, cutscene_callback_bink_ended, &Interface_mempool, nullptr, nullptr, 0, false, false);
        for (int i = 0; i < World.get_num_objects<rfg_mover>(); i++)
        {
            rfg_mover* const rm = World.get_object<rfg_mover>(i);
            if (rm->chunk_uid == 0xFFFFFFFF)
            {
                continue;
            }
            const c_chunk_base* const chunk = chunk_get_base_by_uid(rm->chunk_uid);
            if (chunk == nullptr)
            {
                // Original quirk kept: this leaves the launch flag set, so it tries again next frame.
                return;
            }
            // Original quirk kept: tested against zero, not -1, so a material that is missing counts and the first
            // material of a map does not.
            if (chunk->material_map_p.to_pointer()->find_material_index(Current_cutscene->bink_material_name) != 0)
            {
                cutscene_setup_mover_for_bink(rm);
                break;
            }
        }
        Cutscene_do_handshake_bink_launch = false;
    }
    Cutscene_rendered_this_frame = false;
}

// 0x00705CC0 (:2460)
bool cutscene_start_bink_movie(const char* filename)
{
    // Original quirk kept: no bound on the name, into a 33 byte buffer.
    strcpy(Bink_filename, filename);
    Cutscene_do_handshake_bink_launch = true;
    return true;
}

// 0x00705CF0 (:2471)
void cutscene_set_bink_material(const char* material_name)
{
    // rfg.exe inlines this call.
    Current_cutscene->bink_material_name = str_new_copy(material_name, Cutscene_mempool);
}

// 0x0070CF70 (:2479)
unsigned int cutscene_get_closest_slate_handle()
{
    // Original quirk kept: despite the name, the first slate in the world's list, wherever it is.
    for (int i = 0; i < World.get_num_objects<object_dummy>(); i++)
    {
        const object_dummy* const dummy = World.get_object<object_dummy>(i);
        if (dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_CUTSCENE)
        {
            return dummy->get_handle();
        }
    }
    return 0xFFFFFFFF;
}

// 0x00705D60 (:2491)
bool cutscene_rendered_this_frame()
{
    return Cutscene_rendered_this_frame;
}

// 0x0070E2D0 (:2497)
npc* cutscene_spawn_npc(const human_info* h_info, const vector& pos, const matrix& orient)
{
    if (!Cutscene_is_playing || !srm_human_info_reserved(h_info))
    {
        return nullptr;
    }
    npc_spawn_param_list spawn_list(NPC_MANAGER_CUT_SCENE);
    spawn_list.spawn_pos = pos;
    spawn_list.spawn_orient = orient;
    npc_spawn_params params;
    params.reset();
    params.hum_info = h_info;
    spawn_list.npc_spawn_info.add(params);
    spawn_list.spawn_flags.static_spawn_placement = true;
    object_spawn_do_spawn(spawn_list);
    return spawn_list.npc_spawn_info[0].np;
}

// 0x00705D70 (:2518)
vehicle* cutscene_spawn_vehicle(const vehicle_info* v_info, const vector& pos, const matrix& orient)
{
    if (!Cutscene_is_playing || !srm_vehicle_info_reserved(v_info))
    {
        return nullptr;
    }
    vehicle_spawn_params params;
    params.reset();
    params.spawn_pos = pos;
    params.spawn_orient = orient;
    params.veh_spawn_flags.static_spawn_placement = 1;
    params.veh_spawn_flags.no_save = 1;
    params.veh_info = v_info;
    object_spawn_vehicle(params);
    return params.vp;
}
