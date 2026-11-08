// Original: volition/rfg/code/vehicle/vehicle_audio.cpp
//
// Vehicle sounds: the engine (RPM, load, peel, gear shift), chassis landings, horn, siren, megaphone, wheel foley and
// pass-bys. Compiled in the vehicle unity TU.

#include "volition/rfg/code/vehicle/vehicle_audio.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_effects.h"

// ---- file-static globals
static float Gear2_scalar = 0.88f; // 0x016BA4AC
static float Gear3_scalar = 0.76f; // 0x016BA4B0
static float Npc_rpm_scalar = 1.6f; // 0x016BA4B4
// from the tweak table
static float Vehicle_chassis_minimum_impact_low; // 0x0376F638
static float Vehicle_chassis_minimum_impact_high; // 0x0376F9E4
static float Vehicle_chassis_maximum_impact_high; // 0x0376F7B0
static float Vehicle_chassis_linear_stress_scale; // 0x03770004
static float Vehicle_chassis_angular_stress_scale; // 0x0376FFF8
static float Automobile_engine_peel_min_accelerator; // 0x0376F7A8
static float Automobile_engine_peel_accelerator_cutoff; // 0x0376F62C
static float Automobile_engine_peel_rpm_buffer; // 0x0376F670
static float Automobile_engine_peel_max_speed_sq; // 0x0376F9EC
static tweak_table_item Tweak_table_item_Vehicle_chassis_minimum_impact_low("Vehicle_chassis_minimum_impact_low", &Vehicle_chassis_minimum_impact_low);
static tweak_table_item Tweak_table_item_Vehicle_chassis_minimum_impact_high("Vehicle_chassis_minimum_impact_high", &Vehicle_chassis_minimum_impact_high);
static tweak_table_item Tweak_table_item_Vehicle_chassis_maximum_impact_high("Vehicle_chassis_maximum_impact_high", &Vehicle_chassis_maximum_impact_high);
static tweak_table_item Tweak_table_item_Vehicle_chassis_linear_stress_scale("Vehicle_chassis_linear_stress_scale", &Vehicle_chassis_linear_stress_scale);
static tweak_table_item Tweak_table_item_Vehicle_chassis_angular_stress_scale("Vehicle_chassis_angular_stress_scale", &Vehicle_chassis_angular_stress_scale);
static tweak_table_item Tweak_table_item_Automobile_engine_peel_min_accelerator("Automobile_engine_peel_min_accelerator", &Automobile_engine_peel_min_accelerator);
static tweak_table_item Tweak_table_item_Automobile_engine_peel_accelerator_cutoff("Automobile_engine_peel_accelerator_cutoff", &Automobile_engine_peel_accelerator_cutoff);
static tweak_table_item Tweak_table_item_Automobile_engine_peel_rpm_buffer("Automobile_engine_peel_rpm_buffer", &Automobile_engine_peel_rpm_buffer);
static tweak_table_item Tweak_table_item_Automobile_engine_peel_max_speed_sq("Automobile_engine_peel_max_speed_sq", &Automobile_engine_peel_max_speed_sq);

// 0x00B64110 (:169)
void vehicle_audio_level_init_pre()
{
}

// 0x00B8AC20 (:176)
void automobile_handle_chassis_sounds(automobile* ap)
{
    const base_vehicle_types classification = ap->info->vehicle_classification;
    if (classification == BVT_TANK || classification == BVT_ART_TANK) {
        return;
    }
    if (ap->m_chassis_sound_delay.valid() && !ap->m_chassis_sound_delay.elapsed()) {
        return;
    }
    vector velocity;
    vector angular_velocity;
    vehicle_get_velocity(ap, velocity);
    vehicle_get_angular_velocity(ap, angular_velocity);
    const vector angular_accel((angular_velocity.x - ap->last_angular_velocity.x) / Frametime, (angular_velocity.y - ap->last_angular_velocity.y) / Frametime, (angular_velocity.z - ap->last_angular_velocity.z) / Frametime);
    const float stress = sqrtf(angular_accel.x * angular_accel.x + angular_accel.y * angular_accel.y + angular_accel.z * angular_accel.z) * Vehicle_chassis_angular_stress_scale + fabsf((velocity.y - ap->last_velocity.y) / Frametime) * Vehicle_chassis_linear_stress_scale;
    const float last_stress = ap->prev_chassis_stress;
    // the stress just peaked: a landing
    if (stress < last_stress) {
        bool landed = true;
        float volume = 0.0f;
        if (last_stress < Vehicle_chassis_minimum_impact_high || 0.0f < ap->last_velocity.y) {
            if (last_stress < Vehicle_chassis_minimum_impact_low) {
                landed = false;
            }
            else {
                volume = last_stress / Vehicle_chassis_minimum_impact_high;
                if (volume - 0.2f < 0.0f) {
                    volume = 0.2f;
                }
                if (0.0f <= volume - 1.0f) {
                    volume = 1.0f;
                }
                ap->foley_chassis = audiolib_cue_play_3d(ap->info->m_foley_chassis_land_id[0], &ap->pos, NULL, NULL, NULL);
                vehicle_camera_shake_play_land_soft(ap, volume);
            }
        }
        else {
            volume = last_stress / Vehicle_chassis_maximum_impact_high;
            if (volume - 0.6f < 0.0f) {
                volume = 0.6f;
            }
            if (0.0f <= volume - 1.0f) {
                volume = 1.0f;
            }
            ap->foley_chassis = audiolib_cue_play_3d(ap->info->m_foley_chassis_land_id[1], &ap->pos, NULL, NULL, NULL);
            vehicle_camera_shake_play_land(ap, volume);
        }
        if (landed) {
            audiolib_play_set_volume(ap->foley_chassis, volume, 100);
            ap->m_chassis_sound_delay.set(150);
            ap->max_chassis_stress = last_stress;
        }
    }
    ap->prev_chassis_stress = stress;
}

// 0x00B64120 (:244)
void vehicle_audio_handle_player_enter(vehicle* vp)
{
    audiolib_vehicle_engine_audio_to_2d(vp->engine_inst);
}

// 0x00B75C40 (:251)
void vehicle_handle_enter_audio(vehicle* vp, bool player)
{
    if (player) {
        audiolib_cue_play(vp->info->foley_start, NULL, NULL);
        if (audiolib_vehicle_engine_is_playing(vp->engine_inst)) {
            audiolib_vehicle_engine_audio_to_2d(vp->engine_inst);
        }
        else {
            vp->engine_inst = audiolib_vehicle_engine_start(vp->info->engine_id, NULL, NULL);
        }
    }
    else {
        audiolib_cue_play_3d(vp->info->foley_start, &vp->pos, NULL, NULL, NULL);
        if (!audiolib_vehicle_engine_is_playing(vp->engine_inst)) {
            ::player* local = player_get_local_player(); // the parameter hides the class
            const bool local_inside = local != NULL && human_in_vehicle(local, vp->get_handle());
            vp->engine_inst = audiolib_vehicle_engine_start(vp->info->engine_id, local_inside ? NULL : &vp->pos, NULL);
        }
    }
    vp->engine_started_inst = vp->engine_inst;
    vp->engine_high_load_start_timer.set(7500); // the load ramp of automobile_audio_get_load_value
}

// 0x00B64140 (:287)
void vehicle_handle_exit_audio(vehicle* vp, bool stop_engine, bool player, bool streamout)
{
    if (player) {
        audiolib_vehicle_engine_audio_to_3d(vp->engine_inst, &vp->pos, NULL);
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_AUTO) {
        automobile* ap = (automobile*)vp;
        audiolib_att_loop_rel_stop(ap->honk_alr_id, 100);
        audiolib_play_fade_out(ap->foley_siren, 50);
        audiolib_play_stop(ap->foley_megaphone);
        audiolib_play_change_to_3d(ap->engine_peel, &vp->pos, NULL, NULL);
    }
    vp->m_flags.m_using_horn = 0;
    if (stop_engine) {
        audiolib_vehicle_engine_stop(vp->engine_inst);
        if (!streamout) {
            audiolib_cue_play_3d(vp->info->foley_off, &vp->pos, NULL, NULL, NULL);
        }
    }
    vp->engine_high_load_start_timer.invalidate();
}

// 0x00B967B0 (:319)
void vehicle_audio_engine_update(vehicle* const car)
{
    const float damage = vehicle_get_smoke_amount(car->m_damage_areas); // inlined in rfg.exe, with the 0.3 in double
    if (car->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        flyer* fp = (flyer*)car;
        if (car->engine_inst != 0 && car->engine_inst != -1) {
            havok_vehicle_ai_info info;
            info.flags = 0x16;
            vehicle_get_ai_info(car->get_handle(), info);
            audiolib_vehicle_engine_set_pos(car->engine_inst, &car->pos, NULL);
            audiolib_vehicle_engine_set_RPM(car->engine_inst, info.speed, fp->turbine_rpm, fp->turbine_load, damage);
        }
        return;
    }
    if (car->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        walker* wp = (walker*)car;
        if (car->engine_inst != 0 && car->engine_inst != -1) {
            audiolib_vehicle_engine_set_pos(car->engine_inst, &car->pos, NULL);
            const float speed = wp->actual_velocity_mag;
            float load;
            if (0.05f < wp->desired_move_speed - speed || 0.5f < wp->actual_velocity.y || wp->w_flags.jump_jet_on != 0) {
                load = Frametime + wp->engine_load;
                if (1.0f - load < 0.0f) {
                    load = 1.0f;
                }
            }
            else {
                load = wp->engine_load - Frametime;
                if (0.0f <= 0.0f - load) {
                    load = 0.0f;
                }
            }
            wp->engine_load = load;
            audiolib_vehicle_engine_set_RPM(car->engine_inst, speed, (speed * 10.0f) / 6.5f, wp->engine_load, damage);
        }
        return;
    }
    if (car->get_sub_type() != OT_SUB_VEHICLE_AUTO) {
        return;
    }
    automobile* ap = (automobile*)car;
    if (!ap->a_rdd.is_loaded()) {
        return;
    }
    hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    float rpm = instance->m_rpm;
    const float max_speed = automobile_moving_in_reverse(ap) ? instance->get_enforced_max_reverse_speed() : instance->get_enforced_max_speed();
    // held at top speed the engine climbs to its maximum over 6 s
    if (instance->calcMPH() < max_speed - 2.0f || ap->user_acc_input < 0.9f) {
        ap->time_at_max_speed.invalidate();
    }
    else if (ap->time_at_max_speed.valid()) {
        rpm = ((float)ap->time_at_max_speed.time_since() * (ap->info->max_rpm - rpm)) / 6000.0f + rpm;
    }
    else {
        ap->time_at_max_speed.set(0);
    }
    if (ap->m_flags.m_wheels_on_ground == 0) {
        // revving free in the air
        const float to_max = ap->info->max_rpm - rpm;
        float climb = ap->info->max_rpm * 0.25f;
        if (0.0f <= climb - to_max) {
            climb = to_max;
        }
        rpm += climb;
    }
    const float load = automobile_audio_get_load_value(ap);
    if (ap->engine_inst != 0 && ap->engine_inst != -1) {
        havok_vehicle_ai_info info;
        info.flags = 0x16;
        vehicle_get_ai_info(car->get_handle(), info);
        audiolib_vehicle_engine_set_pos(ap->engine_inst, &car->pos, NULL);
        const unsigned int gear = automobile_get_current_gear(ap);
        if (gear == 2) {
            rpm *= Gear2_scalar;
        }
        else if (gear == 3) {
            rpm *= Gear3_scalar;
        }
        if (!vehicle_player_in_vehicle(ap) && automobile_get_mph(ap) != 0) {
            rpm *= Npc_rpm_scalar;
            if (rpm < 500.0f) {
                rpm = 500.0f;
            }
        }
        audiolib_vehicle_engine_set_RPM(ap->engine_inst, info.speed, rpm, load, damage);
    }
    if (ap->info->foley_engine_peel != -1) {
        const float accelerator = automobile_get_accelerator_value(ap->get_handle());
        if (Automobile_engine_peel_accelerator_cutoff <= accelerator) {
            if (ap->engine_peel == -1) {
                if (rpm <= ap->info->min_rpm + Automobile_engine_peel_rpm_buffer && Automobile_engine_peel_min_accelerator <= accelerator && automobile_get_speed_squared(ap->get_handle()) < Automobile_engine_peel_max_speed_sq) {
                    player* local = player_get_local_player();
                    if (local != NULL && human_in_vehicle(local, ap)) {
                        ap->engine_peel = audiolib_cue_play(ap->info->foley_engine_peel, NULL, NULL);
                    }
                    else {
                        ap->engine_peel = audiolib_cue_play_3d(ap->info->foley_engine_peel, &ap->pos, NULL, NULL, NULL);
                    }
                }
            }
            else if (!audiolib_play_is_2d(ap->engine_peel)) {
                audiolib_play_set_3d_position(ap->engine_peel, &ap->pos, NULL, NULL);
            }
        }
        else if (ap->engine_peel != -1) {
            audiolib_play_fade_out(ap->engine_peel, 250);
            ap->engine_peel = -1;
        }
    }
    if (audiolib_is_playing(ap->gear_shift, true)) {
        if (!audiolib_play_is_2d(ap->gear_shift)) {
            audiolib_play_set_3d_position(ap->gear_shift, &ap->pos, NULL, NULL);
            ap->play_shift_foley = false;
        }
        return;
    }
    if (!ap->play_shift_foley) {
        ap->gear_shift = -1;
        return;
    }
    ap->play_shift_foley = false;
    player* local = player_get_local_player();
    // Original bug kept: the shift's play id is stored as the peel's, so gear_shift never plays.
    if (local != NULL && human_in_vehicle(local, ap)) {
        ap->engine_peel = audiolib_cue_play(ap->info->foley_shift, NULL, NULL);
    }
    else {
        ap->engine_peel = audiolib_cue_play_3d(ap->info->foley_shift, &ap->pos, NULL, NULL, NULL);
    }
}

// 0x00B75D40 (:482)
float automobile_audio_get_load_value(const automobile* const ap)
{
    static float percent_speed = 0.33f; // guarded local in rfg.exe, 1 - the accelerator's 0.67
    if (ap == NULL || !ap->a_rdd.is_loaded() || ap->m_flags.m_engine_running == 0) {
        return 0.0f;
    }
    hk_custom_vehicle_instance* instance = ap->a_rdd.get_data()->m_hk_vehicle_instance;
    // Original bug kept: a null instance reads the forward limit through null.
    const bool reversing = instance != NULL && instance->m_tryingToReverse;
    const float max_speed = reversing ? instance->get_enforced_max_reverse_speed() : instance->get_enforced_max_speed();
    const float mph = instance->calcMPH();
    float accel = ap->user_acc_input * 0.67f;
    if (reversing) {
        accel = -accel;
    }
    if (accel - 0.0f < 0.0f) {
        accel = 0.0f;
    }
    if (ap->m_flags.m_wheels_on_ground == 0) {
        accel *= 0.2f;
    }
    float speed_fraction = mph / max_speed;
    if (1.0f - speed_fraction < 0.0f) {
        speed_fraction = 1.0f;
    }
    const float load = speed_fraction * percent_speed + accel;
    // full load just after starting, fading over 7.5 s
    const float start_load = (float)ap->engine_high_load_start_timer.time_until() / 7500.0f;
    return load - start_load < 0.0f ? start_load : load;
}

// 0x00B64210 (:520)
void automobile_get_horn_world_pos(vector& pos, const vehicle* vp)
{
    const float offset = vp->info->spawn_box.max.z * 0.25f; // a quarter of the way to the front
    pos.x = vp->pos.x + vp->orient.fvec.x * offset;
    pos.y = vp->pos.y + vp->orient.fvec.y * offset;
    pos.z = vp->pos.z + vp->orient.fvec.z * offset;
}

// 0x00B8AFB0 (:525)
void automobile_audio_horn_update(automobile* const car)
{
    if (car->honk_alr_id != -1) {
        vector horn_pos;
        automobile_get_horn_world_pos(horn_pos, car); // inlined in rfg.exe
        audiolib_att_loop_rel_set_pos(car->honk_alr_id, &horn_pos, &car->orient.fvec);
    }
    if (car->horn_timeout.valid() && car->horn_timeout.elapsed()) {
        car->horn_timeout.invalidate();
        automobile_release_horn(car->get_handle());
    }
}

// 0x00B8B0F0 (:644)
void automobile_audio_siren_update(automobile* const car, human* const hp)
{
    vehicle_ai* ai = vehicle_get_vehicle_ai(car->get_handle()); // inlined in rfg.exe
    if (!audiolib_is_playing(car->foley_siren, true)) {
        return;
    }
    audiolib_play_set_3d_position(car->foley_siren, &car->pos, NULL, NULL);
    if (car->m_flags.m_siren_dying == 0) {
        // dies under 45 % of the hit points
        const unsigned int threshold = (unsigned int)(long long)((float)(unsigned int)car->m_damage_areas[0].m_max_hit_points * 0.45f + 0.5f);
        if ((unsigned int)car->m_damage_areas[0].m_hit_points < threshold) {
            audiolib_play_stop(car->foley_siren);
            car->m_flags.m_siren_dying = 1;
            return;
        }
    }
    // an ambulance's only while it has somewhere to go
    if (!object_is_player(hp) && car->m_flags.m_siren_dying == 0 && ai != NULL && car->info->vehicle_classification == BVT_AMBULANCE && ai->get_state() == (vehicle_ai_state)0) {
        audiolib_play_stop(car->foley_siren);
    }
}

// 0x00B64300 (:679)
void automobile_audio_megaphone_update(automobile* const car)
{
    if (audiolib_is_playing(car->foley_megaphone, true)) {
        if (car->is_killed()) {
            audiolib_play_stop(car->foley_megaphone);
            return;
        }
        vector horn_pos;
        automobile_get_horn_world_pos(horn_pos, car); // inlined in rfg.exe
        audiolib_play_set_3d_position(car->foley_megaphone, &horn_pos, &car->orient.fvec, NULL);
    }
}

// 0x00B64440 (:694)
void automobile_audio_chassis_sound_update(automobile* const car)
{
    if (audiolib_is_playing(car->foley_chassis, true)) {
        audiolib_play_set_3d_position(car->foley_chassis, &car->pos, NULL, NULL);
    }
}

// 0x00B75EF0 (:703)
void automobile_play_megaphone(voice_line_handle line_handle, human_voice_persona* hvpp, unsigned int a_handle)
{
    automobile* ap = a_handle != 0xFFFFFFFF ? World.object_from_handle<automobile>(a_handle) : NULL;
    if (ap != NULL && ap->foley_megaphone == -1 && !ap->is_killed()) {
        dialogue_manager_start_dialogue_megaphone(line_handle, hvpp, DIALOGUE_PRIORITY_AMBIENT, a_handle);
    }
}

// 0x00B75F50 (:728)
void automobile_honk_horn(unsigned int a_handle)
{
    automobile* ap = a_handle != 0xFFFFFFFF ? World.object_from_handle<automobile>(a_handle) : NULL; // Original bug kept: not checked for null
    if (ap->honk_alr_id == -1) {
        vector horn_pos;
        automobile_get_horn_world_pos(horn_pos, ap); // inlined in rfg.exe
        audiolib_att_loop_rel_start(ap->info->foley_honk, &ap->honk_alr_id, false, &horn_pos, &ap->orient.fvec);
        ap->m_flags.m_using_horn = 1;
        ap->m_flags.m_mp_honked_horn = 1;
        ap->horn_timeout.set(5000);
    }
}

// 0x00B760C0 (:776)
void automobile_release_horn(unsigned int a_handle)
{
    automobile* ap = a_handle != 0xFFFFFFFF ? World.object_from_handle<automobile>(a_handle) : NULL; // Original bug kept: not checked for null
    if (ap->honk_alr_id != -1) {
        audiolib_att_loop_rel_stop(ap->honk_alr_id, 100);
        ap->m_flags.m_using_horn = 0;
        ap->m_flags.m_mp_honked_horn = 0;
        ap->honk_alr_id = -1;
    }
}

// 0x00B76130 (:828)
void vehicle_audio_process_pass_by(vehicle* vp)
{
    if (vp->info->m_foley_pass_by == -1) {
        return;
    }
    audiolib_stats stats;
    audiolib_stats_get(&stats);
    player* local = player_get_local_player();
    vector listener;
    if (stats.initialized) {
        listener.set(stats.pos_x, stats.pos_y, stats.pos_z);
    }
    else if (local == NULL) {
        listener = Rfg_camera.real_pos;
    }
    else {
        listener = local->pos;
    }
    if (vp->get_sub_type() == OT_SUB_VEHICLE_FLYER) {
        // a flyer's height counts for at most 40 m
        float height = listener.y - vp->pos.y;
        if (height - -40.0f < 0.0f) {
            height = -40.0f;
        }
        if (0.0f <= height - 40.0f) {
            height = 40.0f;
        }
        listener.y -= height;
    }
    vector listener_velocity;
    listener_velocity.zero();
    if (local != NULL && local->vehicle_handle != 0xFFFFFFFF) {
        vehicle* local_vehicle = World.object_from_handle<vehicle>(local->vehicle_handle);
        listener_velocity = local_vehicle != NULL ? local_vehicle->last_velocity : local->velocity;
    }
    // over 5 m/s it is the speed relative to the listener that counts
    vector relative = vp->last_velocity;
    if (25.0f < relative.x * relative.x + relative.y * relative.y + relative.z * relative.z && local != NULL) {
        relative.x -= listener_velocity.x;
        relative.y -= listener_velocity.y;
        relative.z -= listener_velocity.z;
    }
    const float dist_sqr = vec_dist_squared(listener, vp->pos);
    bool just_passed = false;
    if (dist_sqr <= vp->pass_by_distance) {
        vp->m_flags.pass_by_getting_closer = 1;
    }
    else if (vp->m_flags.pass_by_getting_closer != 0) {
        just_passed = true;
        vp->m_flags.pass_by_getting_closer = 0;
    }
    vp->pass_by_distance = dist_sqr;
    if (vp->pass_by_play_id == -1) {
        // within 30 m, at over 10 m/s
        if (just_passed && dist_sqr < 900.0f && 100.0f < relative.x * relative.x + relative.y * relative.y + relative.z * relative.z && !player_occupied_vehicle(vp)) {
            vp->pass_by_play_id = audiolib_cue_play_3d(vp->info->m_foley_pass_by, &vp->pos, NULL, NULL, NULL);
        }
    }
    else if (900.0f < dist_sqr && audiolib_play_get_state(vp->pass_by_play_id, NULL) != AUDIOLIB_ERR_NONE) {
        vp->pass_by_play_id = -1;
    }
}

// 0x00B76490 (:918)
void automobile_process_wheel_foley(automobile* ap, int& play_id, bool play_foley, bool stop_foley, int cue, bool restart, float volume, const vector& pos, const vector& velocity)
{
    audiolib_cue_info cue_info;
    if (play_foley) {
        if (play_id == -1) {
            if ((int)audiolib_cue_get_info(cue, &cue_info) < 0) {
                return;
            }
            audiolib_params params;
            params.pos = pos;
            params.velocity = velocity;
            params.volume = volume;
            if (!cue_info.always_2d && !vehicle_player_in_vehicle(ap)) {
                params.flags = 1; // 3d
            }
            bool paused = false;
            audiolib_all_sounds_get_pause(&paused, NULL);
            if (!paused) {
                play_id = audiolib_cue_play(cue, &params, NULL);
            }
            return;
        }
        audiolib_play_set_volume(play_id, volume, 250);
    }
    else {
        if (play_id == -1) {
            return;
        }
        if (stop_foley) {
            // Original quirk kept: a reversing vehicle fades over 650 ms
            const bool reversing = ap->a_rdd.is_loaded() && ap->a_rdd.get_data()->m_hk_vehicle_instance != NULL && ap->a_rdd.get_data()->m_hk_vehicle_instance->m_tryingToReverse;
            audiolib_play_fade_out(play_id, reversing ? 650 : 500);
            play_id = -1;
            return;
        }
    }
    if (play_id == -1) {
        return;
    }
    if (audiolib_cue_get_info(cue, &cue_info) == AUDIOLIB_ERR_NONE && !cue_info.always_2d) {
        audiolib_play_set_3d_position(play_id, &pos, NULL, &velocity);
    }
    if (play_id != -1 && restart) {
        audiolib_play_fade_out(play_id, 500);
        play_id = -1;
    }
}
