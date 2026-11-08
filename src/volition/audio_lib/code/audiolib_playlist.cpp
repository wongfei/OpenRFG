// Original: volition/audio_lib/code/audiolib_playlist.cpp
//
// The play list (SFX.md SX2, 1.4 step 5): every cue play is an audiolib_play_data in Play_instances, kept in the list
// of its state (play_state, a audiolib_hw_play_state). audiolib_process_playlist runs on the Main Audio Thread every
// ~10 ms and walks the lists from the end of life back to the start: destroyed and stopped plays are freed, stopping
// plays wait for the stream thread, playing/stalling/paused plays get their volume, pitch, low pass and 3D updates,
// ready plays start, loaded plays are prepared and pending plays are range-checked and loaded. A play id packs the
// pool index (12 bits), the emitter of a merged sound (4 bits) and the slot's rolling counter (high half).
// Action bits: 2 start, 4 pause, 8 unpause, 0x10 drop, 0x20 volume, 0x40 fade then drop (no stop), 0x80 pan,
// 0x100 position, 0x200 pitch, 0x400 to 3D, 0x800 to 2D, 0x1000 low pass, 0x2000 stall fade, 0x4000 fade then stop,
// 0x8000 destroy. Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_internal.h"

#include "keen/sound/soundsystem.hpp"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/audio_lib/code/audiolib_callbacks.h"
#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_hardware.h"
#include "volition/audio_lib/code/audiolib_markers.h"
#include "volition/audio_lib/code/audiolib_music.h"
#include "volition/audio_lib/code/audiolib_soundbank.h"
#include "volition/audio_lib/code/audiolib_stream.h"
#include "volition/audio_lib/code/audiolib_thread.h"
#include "volition/audio_lib/code/audiolib_vehicle_engine.h"
#include "volition/audio_lib/code/stub/audiolib_stub.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/timer.h"
#include "volition/vlib/code/time/timestamp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---- public globals (rfg.pdb; no header declares them)
keen::SoundSystem::System* m_pSoundSystem; // 0x01A7AF00

// ---- file-static globals
// The public play state of each internal hardware play state.
static audiolib_play_state play_state_lookup[15] = { // 0x014F9428
    AUDIOLIB_PLAY_STATE_UNUSED, AUDIOLIB_PLAY_STATE_PENDING_FREE_CHANNEL, AUDIOLIB_PLAY_STATE_LOADING,
    AUDIOLIB_PLAY_STATE_LOADING, AUDIOLIB_PLAY_STATE_LOADING, AUDIOLIB_PLAY_STATE_LOADING, AUDIOLIB_PLAY_STATE_READY,
    AUDIOLIB_PLAY_STATE_PLAYING, AUDIOLIB_PLAY_STATE_PLAYING, AUDIOLIB_PLAY_STATE_PLAYING, AUDIOLIB_PLAY_STATE_PLAYING,
    AUDIOLIB_PLAY_STATE_PAUSED, AUDIOLIB_PLAY_STATE_STOPPED, AUDIOLIB_PLAY_STATE_STOPPED, AUDIOLIB_PLAY_STATE_STOPPED,
};
// Set 30 ms ahead when a cue above medium priority starts; lower priority pending cues wait for it.
static timestamp audiolib_cue_start; // 0x014F9C58, invalid (-1) from the constructor
// audiolib_interactive_music_data Audiolib_Music (0x01A789C8) is used here too; placed in audiolib.cpp.
// bool Audiolib_markers_inited (0x01A79A8B) is used here too; placed in audiolib.cpp.
// char debug_decoded_name[8][64] (0x01A79CD0) is used here too; placed in audiolib_foley_category.cpp.
// The dropped-sound report switch (never set; audiolib_list_active_foley prints nothing in rfg.exe).
static bool audiolib_show_dropped_calls; // 0x01A7AEE7
// sync_event Audiolib_Stream_Trigger (0x01A7AF1C) is used here too; placed in audiolib.cpp.
// object_pool<audiolib_internal_eventqueue_data,250,9> EventQueue_list (0x01A7E530) is used here too; placed in audiolib.cpp.
// cs_wrapper Audiolib_EventQueue_list_cs (0x01A811EC) is used here too; placed in audiolib.cpp.
static audiolib_multi_sound_info_struct audiolib_multi_sound_info[26]; // 0x01A81278
// object_pool<audiolib_soundbank_data,252,5> Soundbank_list (0x01A83AD0) is used here too; placed in audiolib.cpp.
// cs_wrapper Audiolib_marker_cs (0x01A988A0) is used here too; placed in audiolib_markers.cpp.
// object_pool<audiolib_alr_data,100,1> ALR_list (0x01A988F0) is used here too; placed in audiolib_att_loop_rel.cpp.
// object_pool<audiolib_vehicle_engine_instance,30,4> Active_engine_list (0x01A9BA20) is used here too; placed in audiolib.cpp.
static object_pool<audiolib_play_data,200,15> Play_instances(nullptr, &audiolib_play_data::drop, 0x47); // 0x01A9D120

// The play time of the play's current variation (audiolib_internal_get_variation_pointer, inlined).
static unsigned int audiolib_playlist_variation_play_time(audiolib_play_data* play)
{
    return audiolib_internal_get_variation_pointer(&play->foley_data, play->curr_variant)->play_time;
}

// Moves a play to a state list (the move of each audiolib_internal_new_playstate_*).
static void audiolib_playlist_move_to(audiolib_play_data* play, audiolib_hw_play_state state)
{
    if (play->play_state != state)
    {
        Audiolib_play_cs.enter();
        Play_instances.move(play, state);
        play->play_state = state;
        Audiolib_play_cs.leave();
    }
}

// A play the hardware finished (or that never loaded) ends: only the drop bit stays, and a destroy is asked for
// (inlined in audiolib_process_playlist).
static void audiolib_playlist_end_play(audiolib_play_data* play, int time)
{
    play->action = (play->action & 0x10) | 0x8000;
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_STOPPED);
    audiolib_internal_callback_event_add(play, 0x80, time, nullptr);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x00489480 (:136)
void audiolib_play_data::invalidate()
{
    const int counter = rolling_counter;
    memset(this, 0, sizeof(*this));
    rolling_counter = counter;
}

// 0x004894B0 (:145)
void audiolib_play_data::drop()
{
    // Play_instances' release callback: the slot's next ids no longer match the old ones (12-bit counter).
    ++rolling_counter;
    if (rolling_counter > 0xFFF)
    {
        rolling_counter = 1;
    }
    const int counter = rolling_counter;
    memset(this, 0, sizeof(*this));
    rolling_counter = counter;
}

// 0x0048CEF0 (audiolib_internal.h:907)
audiolib_play_data::audiolib_play_data()
{
    hw_info.streamingWaveData.pData = nullptr;
    rolling_counter &= 0xFFF;
    if (rolling_counter == 0)
    {
        rolling_counter = 1;
    }
    // Everything else zero (the params constructor's defaults are overwritten too).
    const int counter = rolling_counter;
    memset(this, 0, sizeof(*this));
    rolling_counter = counter;
}

// 0x00490E60 (:169)
audiolib_play_data* audiolib_internal_playstate_id_to_ptr(int play_id)
{
    if (play_id == 0 || play_id == -1)
    {
        return nullptr;
    }
    const unsigned int index = play_id & 0xFFF;
    if (index > 199)
    {
        return nullptr;
    }
    Audiolib_play_cs.enter();
    audiolib_play_data* play = Play_instances.get_entry((int)index);
    if (play != nullptr)
    {
        if ((play_id >> 16) != play->rolling_counter)
        {
            Audiolib_play_cs.leave();
            return nullptr;
        }
        // A play id of a merged sound is valid while its emitter is (a public address sound while any is).
        if (play->reference_count > 1 && play->multi_info != nullptr)
        {
            bool in_use = true;
            if (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_MERGED)
            {
                in_use = play->multi_info->data[(play_id >> 12) & 0xF].in_use;
            }
            else if (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS)
            {
                in_use = play->multi_info->in_use;
            }
            if (!in_use)
            {
                Audiolib_play_cs.leave();
                return nullptr;
            }
        }
    }
    Audiolib_play_cs.leave();
    return play;
}

// 0x00490F20 (:226)
char* audiolib_internal_debug_playid_name(int play_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return nullptr;
    }
    // audiolib_internal_debug_foley_name, inlined.
    char* name = audiolib_internal_debug_foley_name(play->foley_data.name_crc);
    Audiolib_play_cs.leave();
    return name;
}

// 0x00495980 (:260)
void audiolib_list_active_foley(bool everything, const char* why)
{
    (void)everything;
    (void)why;
    // The report is compiled out: only the walk over every list is left.
    if (!audiolib_show_dropped_calls)
    {
        return;
    }
    for (int state = 0; state < NUM_AUDIOLIB_HW_PLAY_STATES; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
        }
        Play_instances.thread_access.leave();
    }
}

// The lowest priority play below play's among the ready, playing (and paused) ones that are not being dropped;
// priority_ok picks which plays count (the walk of both priority kill functions).
template<typename Filter>
static void audiolib_playlist_scan_kill_candidates(audiolib_play_data* play, Filter filter)
{
    static const int states[3] = {AUDIOLIB_HW_PLAY_STATE_READY, AUDIOLIB_HW_PLAY_STATE_PLAYING, AUDIOLIB_HW_PLAY_STATE_PAUSED};
    for (int i = 0; i < 3; ++i)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* other = Play_instances.get_first(states[i]); other != nullptr; other = Play_instances.get_next(other))
        {
            filter(other, states[i]);
        }
        Play_instances.thread_access.leave();
    }
    (void)play;
}

// 0x00495A20 (:575)
static void audiolib_force_priority_kill(audiolib_play_data* play)
{
    audiolib_play_data* lowest = nullptr;
    audiolib_playlist_scan_kill_candidates(play, [&](audiolib_play_data* other, int) {
        if ((other->action & 0x8010) == 0 && other->params.priority < play->params.priority &&
            (lowest == nullptr || other->params.priority < lowest->params.priority))
        {
            lowest = other;
        }
    });
    if (lowest != nullptr)
    {
        audiolib_internal_playlist_fade_out(lowest->play_id, 0);
        ++Audiolib_state.frame_stats.dropped_sounds;
        audiolib_list_active_foley(false, "killed by higher priority sound");
    }
}

// 0x00495BB0 (:625)
static bool audiolib_check_for_priority_kills(audiolib_play_data* play)
{
    audiolib_play_data* lowest = nullptr;
    audiolib_play_data* farthest = nullptr;
    bool lowest_priority = false;
    bool killed = false;
    // A 2D play counts as 1000 m away.
    const float distance = (play->params.flags & 1) == 0 ? 1000.0f : play->dist;
    if (play->params.priority > 4)
    {
        audiolib_cue_start.set(30);
    }
    // Params flag 0x100: never waits for a channel.
    if ((play->params.flags & 0x100) != 0)
    {
        return true;
    }
    short weather_category = 0;
    if (Audiolib_state.initialized)
    {
        weather_category = audiolib_internal_get_category_id("AMBIENCE_WEATHER");
    }
    // Candidates: one-shots, and low priority weather loops among the ready and playing plays (paused plays: one-shots
    // only). The lowest priority one below play's, and the farthest low priority 3D one (beyond 30 m) farther than play.
    audiolib_playlist_scan_kill_candidates(play, [&](audiolib_play_data* other, int state) {
        const bool weather_loop = state != AUDIOLIB_HW_PLAY_STATE_PAUSED && other->params.priority < 5 && other->category_id == weather_category;
        if ((other->foley_data.looping && !weather_loop) || (other->action & 0x8010) != 0)
        {
            return;
        }
        const unsigned char priority = other->params.priority;
        if (priority < play->params.priority && (lowest == nullptr || priority < lowest->params.priority))
        {
            lowest = other;
        }
        if (priority < 5 && distance < other->dist && (other->params.flags & 1) != 0 && other->dist > 30.0f &&
            (farthest == nullptr || farthest->dist < other->dist))
        {
            farthest = other;
        }
    });
    audiolib_play_data* victim = nullptr;
    const char* why = nullptr;
    if (lowest != nullptr && play->params.priority > lowest->params.priority)
    {
        victim = lowest;
        why = "killed by higher priority sound";
    }
    else if (farthest != nullptr)
    {
        victim = farthest;
        why = "killed by distance";
    }
    if (victim != nullptr)
    {
        // A loop is dropped, a one-shot faded out.
        if (victim->foley_data.looping)
        {
            victim->action |= 0x10;
        }
        else
        {
            audiolib_internal_playlist_fade_out(victim->play_id, 0);
        }
        ++Audiolib_state.frame_stats.dropped_sounds;
        audiolib_list_active_foley(false, why);
        killed = true;
    }
    const unsigned char priority = play->params.priority;
    if (priority == 0 || (lowest != nullptr && priority < lowest->params.priority))
    {
        lowest_priority = true;
    }
    if (!killed)
    {
        if (lowest_priority)
        {
            ++Audiolib_state.frame_stats.dropped_sounds;
            audiolib_list_active_foley(false, "killed by being a low priority sound");
            ++play->waiting_for_channel;
            return lowest_priority;
        }
        if (priority < 5 && (play->params.flags & 1) != 0 && !play->foley_data.looping && play->dist > 30.0f)
        {
            ++Audiolib_state.frame_stats.dropped_sounds;
            audiolib_list_active_foley(false, "killed by being a 3D medium priority sound");
            ++play->waiting_for_channel;
            return false;
        }
        // Waiting more than 50 passes above medium priority: force a kill.
        if (play->waiting_for_channel > 50 && priority > 4)
        {
            audiolib_force_priority_kill(play);
            play->waiting_for_channel = 0;
        }
    }
    ++play->waiting_for_channel;
    return lowest_priority;
}

// 0x004894F0 (:819), play in EDX
static bool audiolib_playlist_allocate_multi_data(audiolib_play_data* play)
{
    if (play->multi_info != nullptr)
    {
        return true;
    }
    for (int i = 0; i < 26; ++i)
    {
        audiolib_multi_sound_info_struct& info = audiolib_multi_sound_info[i];
        if (!info.in_use)
        {
            // Only the first emitter's dist, volume, position and in_use are cleared (8-byte stores up to 0x1C).
            info.data[0].dist = 0.0f;
            info.data[0].volume = 0.0f;
            info.data[0].pos.x = 0.0f;
            info.data[0].pos.y = 0.0f;
            memset(&info.data[0].pos.z, 0, 8);
            play->multi_info = &info;
            info.in_use = true;
            return true;
        }
    }
    return false;
}

// 0x00489550 (:842), play in ESI
static int audiolib_find_free_multi_data(audiolib_play_data* play)
{
    for (int i = 0; i < 16; ++i)
    {
        audiolib_multi_sound_instance& emitter = play->multi_info->data[i];
        if (!emitter.in_use)
        {
            memset(&emitter, 0, sizeof(emitter));
            emitter.in_use = true;
            return i;
        }
    }
    return -1;
}

// 0x004895A0 (:859), multi_index in EAX, play in ECX
static bool audiolib_merged_data_clean_one_merged_sound(audiolib_play_data* play, int multi_index)
{
    if (multi_index > 15)
    {
        return false;
    }
    memset(&play->multi_info->data[multi_index], 0, sizeof(audiolib_multi_sound_instance));
    return true;
}

// 0x004895D0 (:871), play in ESI
static bool audiolib_playlist_free_merge_data(audiolib_play_data* play)
{
    for (int i = 0; i < 26; ++i)
    {
        if (audiolib_multi_sound_info[i].in_use && play->multi_info == &audiolib_multi_sound_info[i])
        {
            memset(&audiolib_multi_sound_info[i], 0, sizeof(audiolib_multi_sound_info[i]));
            return true;
        }
    }
    return false;
}

// 0x00489630 (:886), multi_index in EAX, play in ECX, params in ESI
static bool audiolib_playlist_copy_params_to_multi(audiolib_play_data* play, audiolib_params* params, int multi_index)
{
    if (play == nullptr || play->multi_info == nullptr || params == nullptr)
    {
        return false;
    }
    audiolib_multi_sound_instance& emitter = play->multi_info->data[multi_index];
    emitter.pos = params->pos;
    emitter.volume = params->volume;
    emitter.dist = play->dist;
    emitter.in_use = true;
    return true;
}

// 0x0049C1E0 (:903)
void audiolib_internal_playlist_pause(bool pause_sound, bool pause_music)
{
    if (pause_music)
    {
        audiolib_interactive_music_set_pause(true);
    }
    if (!pause_sound)
    {
        return;
    }
    Audiolib_play_cs.enter();
    for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_STOPPED; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            if (play->category_id == Audiolib_state.music_cat_id)
            {
                continue;
            }
            if (play->foley_data.do_not_kill_when_pausing || play->foley_data.looping)
            {
                // Loops and kept sounds pause.
                if ((play->action & 0xC050) == 0)
                {
                    play->action = (play->action & 0xFFF7) | 4;
                }
            }
            else if (state == AUDIOLIB_HW_PLAY_STATE_PLAYING)
            {
                // A playing one-shot fades out over 500 ms and is dropped.
                play->action |= 0x60;
                play->new_vol = 0.0f;
                play->fade_time = 500;
            }
            else
            {
                play->action |= 0x8000;
            }
        }
        Play_instances.thread_access.leave();
    }
    Audiolib_play_cs.leave();
}

// 0x0049C340 (:944)
void audiolib_internal_playlist_unpause(bool unpause_sound, bool unpause_music)
{
    if (unpause_music)
    {
        audiolib_interactive_music_set_pause(false);
    }
    if (!unpause_sound)
    {
        return;
    }
    Audiolib_play_cs.enter();
    for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_PAUSED; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            if (play->category_id != Audiolib_state.music_cat_id && (play->action & 0xC050) == 0)
            {
                play->action |= 8;
            }
        }
        Play_instances.thread_access.leave();
    }
    Audiolib_play_cs.leave();
}

// 0x004A6360 (:969)
void audiolib_internal_playlist_stop(bool stop_sound, bool stop_music)
{
    if (stop_music)
    {
        audiolib_interactive_music_stop(Audiolib_Music.inited ? Audiolib_Music.current_music_id : -1, 0);
    }
    if (!stop_sound)
    {
        return;
    }
    audiolib_internal_att_loop_rel_kill_all();
    audiolib_internal_vehicle_engine_kill_all();
    Audiolib_play_cs.enter();
    for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_DESTROYED; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            if (play->category_id != Audiolib_state.music_cat_id || stop_music)
            {
                if (state == AUDIOLIB_HW_PLAY_STATE_PLAYING && !Audiolib_panicking)
                {
                    // Playing sounds fade out over 500 ms and stop.
                    play->action |= 0x4020;
                    play->new_vol = 0.0f;
                    play->fade_time = 500;
                }
                else
                {
                    play->action |= 0x8000;
                }
            }
        }
        Play_instances.thread_access.leave();
    }
    Audiolib_play_cs.leave();
}

// 0x00495FE0 (:1006)
void audiolib_internal_playlist_stop_by_category(short category_id)
{
    Audiolib_play_cs.enter();
    for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_STOPPED; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            // Original: every category but the given one fades out.
            if (play->category_id != category_id)
            {
                audiolib_internal_playlist_fade_out(play->play_id, 0);
            }
        }
        Play_instances.thread_access.leave();
    }
    Audiolib_play_cs.leave();
}

// 0x00489800 (:1957), play in ECX, time_step in EDX
static void audiolib_internal_playlist_change_volume(audiolib_play_data* play, int time_step)
{
    const float current = play->curr_vol;
    float volume = play->new_vol;
    if (time_step >= 0)
    {
        // The share of the fade time this step covers, eased so no step exceeds 0.1.
        const int fade_time = play->fade_time;
        float share = 1.0f;
        if (fade_time > 0 && time_step < fade_time)
        {
            share = (float)time_step / (float)fade_time;
        }
        float step;
        while (fabsf(step = share * (volume - current)) > 0.1f)
        {
            share = share * 0.9f;
        }
        volume = step + current;
        play->fade_time = time_step < fade_time ? fade_time - time_step : 0;
    }
    unsigned short action = play->action;
    if ((action & 0x4000) != 0)
    {
        // Fading to a stop: destroyed once inaudible.
        if (volume <= 0.01f)
        {
            play->action = (action & 0x3FFF) | 0x8000;
            volume = 0.01f;
        }
    }
    else if ((action & 0x40) != 0)
    {
        // Fading to a drop.
        if (volume < 1e-05f)
        {
            volume = 0.0f;
            play->action = (action & 0xFFBF) | 0x8010;
        }
    }
    else if ((action & 0x2000) != 0 && volume <= 0.01f)
    {
        // A stall fade ends in a pause request.
        play->action = (action & 0xDFFF) | 4;
        volume = 0.01f;
    }
    if (volume != current)
    {
        // The emitters of a multi-sound scale with the play.
        const float scale = volume / current;
        play->curr_vol = volume;
        if (play->multi_info != nullptr)
        {
            for (int i = 0; i < 16; ++i)
            {
                audiolib_multi_sound_instance& emitter = play->multi_info->data[i];
                if (emitter.in_use)
                {
                    emitter.volume = scale * emitter.volume;
                }
            }
        }
    }
    if (play->curr_vol == play->new_vol)
    {
        play->action &= 0xFFDF;
    }
}

// 0x00491010 (:2017), time_step in EAX, play in ESI
static void audiolib_internal_playlist_change_pitch(audiolib_play_data* play, int time_step)
{
    const float current = play->curr_pitch;
    float pitch = play->new_pitch;
    if (time_step >= 0)
    {
        // Original: the share is the time step itself, capped at 1 (any step of 1 ms or more is the whole change).
        float share = 1.0f;
        if (time_step < 1)
        {
            share = (float)time_step;
        }
        float step;
        while (fabsf(step = share * (pitch - current)) > 0.1f)
        {
            share = share * 0.9f;
        }
        pitch = step + current;
        if (play->doppler_factor > 0.0f)
        {
            pitch = play->doppler_factor * pitch;
        }
        if (pitch - -1.0f < 0.0f)
        {
            pitch = -1.0f;
        }
        if (0.0f <= pitch - 1.0f)
        {
            pitch = 1.0f;
        }
    }
    if (pitch != current && audiolib_hw_set_pitch(play, pitch) == AUDIOLIB_ERR_NONE)
    {
        play->curr_pitch = pitch;
    }
    if (play->curr_pitch == play->new_pitch)
    {
        play->action &= 0xFDFF;
    }
}

// 0x00489B90 (:2055), time_step in EAX, play in ECX
static void audiolib_internal_playlist_change_lowpass(audiolib_play_data* play, int time_step)
{
    const float target = play->new_low_pass;
    float low_pass;
    if (time_step < 0)
    {
        // At most 0.05 towards the target.
        const float lowest = play->curr_low_pass - 0.05f;
        float clamped = target;
        if (target - lowest < 0.0f)
        {
            clamped = lowest;
        }
        low_pass = play->curr_low_pass + 0.05f;
        if (clamped - low_pass < 0.0f)
        {
            low_pass = clamped;
        }
    }
    else
    {
        // Over 500 ms, eased so no step exceeds 0.1, at most 0.05 per call.
        const float current = play->curr_low_pass;
        float share = 1.0f;
        if (time_step < 500)
        {
            share = (float)time_step / 500.0f;
        }
        float step;
        while (fabsf(step = share * (target - current)) > 0.1f)
        {
            share = share * 0.9f;
        }
        low_pass = step + current;
        if (low_pass - (current - 0.05f) < 0.0f)
        {
            low_pass = current - 0.05f;
        }
        if (0.0f <= low_pass - (current + 0.05f))
        {
            low_pass = current + 0.05f;
        }
    }
    play->curr_low_pass = low_pass;
    if (low_pass == target)
    {
        play->action &= 0xEFFF;
    }
}

// 0x004A1CA0 (:1028), play in ECX, time_step in EDI
static audiolib_result audiolib_update_calculations_for_output(audiolib_play_data* play, int time, int time_step)
{
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    bool changed = false;
    if (play->action != 0)
    {
        if ((play->action & 0x6020) != 0)
        {
            audiolib_internal_playlist_change_volume(play, time_step);
            // A stall fade that ended pauses now.
            if (play->play_state == AUDIOLIB_HW_PLAY_STATE_STALLING && (play->action & 4) != 0)
            {
                play->action &= 0xFFFB;
                audiolib_internal_new_playstate_stalled(play, time);
            }
            changed = true;
        }
        if ((play->action & 0x80) != 0)
        {
            play->curr_pan = play->new_pan;
            play->action &= 0xFF7F;
            changed = true;
        }
        if ((play->action & 0x100) != 0)
        {
            play->action &= 0xFEFF;
            play->curr_pos = play->new_pos;
            changed = true;
        }
        if ((play->action & 0x200) != 0)
        {
            audiolib_internal_playlist_change_pitch(play, time_step);
            changed = true;
        }
        if ((play->action & 0x1000) != 0)
        {
            audiolib_internal_playlist_change_lowpass(play, time_step);
            changed = true;
        }
        if ((play->action & 0x400) != 0)
        {
            play->params.flags |= 1;
            play->action &= 0xFBFF;
            changed = true;
        }
        if ((play->action & 0x800) != 0)
        {
            // To 2D: no low pass and no reverb, no own DSP.
            play->action &= 0xF7FF;
            play->params.flags = (play->params.flags & 0xFFFE) | 0x30;
            play->params.dsp_type = 0;
            changed = true;
        }
    }
    // Occlusion and 3D once per game frame; the output when anything changed.
    if (play->output_framecount < Framecount)
    {
        audiolib_internal_fov_compute_occlusion(play);
        audiolib_3d_calculate(play);
    }
    else if (!changed)
    {
        return AUDIOLIB_ERR_NONE;
    }
    audiolib_internal_dsp_hw_setup(play);
    audiolib_channel_to_speaker_map output_map;
    audiolib_3d_create_volume_matrix(play, &output_map);
    audiolib_hw_3d_update(play, &output_map);
    play->output_framecount = Framecount;
    return AUDIOLIB_ERR_NONE;
}

// 0x0049C440 (:1118), play in ESI, new_play_state in EDI
static bool audiolib_internal_playlist_move(audiolib_play_data* play, audiolib_hw_play_state new_play_state)
{
    if (play == nullptr)
    {
        return false;
    }
    audiolib_playlist_move_to(play, new_play_state);
    return true;
}

// 0x00490FC0 (:1140)
bool audiolib_internal_get_callback_info(AUDIOLIB_CUE_CALLBACK_DATA* callback_event)
{
    if (callback_event == nullptr)
    {
        return false;
    }
    if (callback_event->function == nullptr)
    {
        Audiolib_play_cs.enter();
        audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(callback_event->play_id);
        if (play != nullptr)
        {
            callback_event->function = play->params.callback_func;
            callback_event->user_data = play->params.callback_data;
        }
        Audiolib_play_cs.leave();
    }
    return true;
}

// 0x0049C480 (:1159)
void audiolib_internal_new_playstate_pending(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    // A shared multi-sound play is restarted only once nothing else uses it.
    const bool shared = (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_MERGED || play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS) &&
                        play->reference_count >= 2 && play->play_state != AUDIOLIB_HW_PLAY_STATE_UNUSED &&
                        !(play->play_state > AUDIOLIB_HW_PLAY_STATE_STOPPING && play->play_state < NUM_AUDIOLIB_HW_PLAY_STATES);
    if (shared)
    {
        return;
    }
    switch (play->hw_play_state)
    {
    case AUDIOLIB_HW_PLAY_STATE_UNUSED:
    case AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL:
    case AUDIOLIB_HW_PLAY_STATE_STOPPED:
    case AUDIOLIB_HW_PLAY_STATE_DESTROYED:
        Audiolib_play_cs.enter();
        play->cue_time = time;
        play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL;
        Audiolib_play_cs.leave();
        audiolib_internal_playlist_move(play, AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL);
        audiolib_internal_callback_event_add(play, 1, time, nullptr);
        break;
    default:
        break;
    }
}

// 0x004A64B0 (:1218)
void audiolib_internal_load_request(audiolib_play_data* play, int time)
{
    audiolib_3d_calculate(play);
    if (audiolib_stream_start_load(play, play->params.offset_ms) == AUDIOLIB_ERR_NONE)
    {
        audiolib_internal_callback_event_add(play, 2, time, nullptr);
        play->load_requested = true;
    }
    else
    {
        audiolib_internal_new_playstate_stop(play, time);
    }
    ++Audiolib_state.frame_stats.action_count;
}

// 0x004A1E70 (:1282)
void audiolib_internal_new_playstate_loaded(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    audiolib_internal_fov_compute_occlusion(play);
    audiolib_3d_calculate(play);
    if (audiolib_stream_prepare(play, play->params.offset_ms) == AUDIOLIB_ERR_NONE)
    {
        audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_LOADED);
        audiolib_internal_callback_event_add(play, 4, time, nullptr);
    }
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049C530 (:1305)
void audiolib_internal_new_playstate_preparing(audiolib_play_data* play, int time)
{
    (void)time;
    if (play == nullptr)
    {
        return;
    }
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_PREPARING);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049C580 (:1324)
void audiolib_internal_new_playstate_ready(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    if ((play->params.flags & 1) == 0)
    {
        // 2D: on the listener.
        play->new_pos = Audiolib_state.listener_pos;
        play->new_orient = Audiolib_state.listener_front_orient;
        play->new_velocity = vector(0.0f, 0.0f, 0.0f);
    }
    else
    {
        play->new_pos = play->params.pos;
        play->new_pan = play->params.pan;
        play->new_orient = play->params.orient;
        play->new_velocity = play->params.velocity;
    }
    play->new_pitch = play->params.pitch;
    // Callback flag 0x40: marker callbacks.
    if ((play->params.callback_flags & 0x40) != 0 && (!Audiolib_markers_inited || audiolib_marker_add_marker_to_process_list(play) < 0))
    {
        play->marker_id = -1;
    }
    // Params flag 2: start paused (no start request).
    if ((play->params.flags & 2) == 0)
    {
        play->action |= 2;
    }
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_READY);
    audiolib_internal_callback_event_add(play, 8, time, nullptr);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x004A6510 (:1387)
void audiolib_internal_new_playstate_play(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    audiolib_update_calculations_for_output(play, time, 10);
    if (audiolib_stream_play(play) < 0)
    {
        audiolib_internal_new_playstate_stop(play, time);
        return;
    }
    play->action &= 0xFFF5;
    play->start_time = time;
    play->end_time = (int)audiolib_playlist_variation_play_time(play) + time;
    audiolib_marker_pause_marker_data(play, false);
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_PLAYING);
    audiolib_internal_callback_event_add(play, 0x10, time, nullptr);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049C6C0 (:1436)
void audiolib_internal_new_playstate_stalling(audiolib_play_data* play, int time)
{
    (void)time;
    if (play == nullptr)
    {
        return;
    }
    // Faded to 0.01 over 100 ms, then paused; the louder of the target and current volume comes back after.
    float volume = play->new_vol;
    if (play->new_vol - play->curr_vol < 0.0f)
    {
        volume = play->curr_vol;
    }
    play->action |= 0x2020;
    play->pre_stalled_volume = volume;
    play->new_vol = 0.01f;
    play->fade_time = 100;
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_STALLING);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049C770 (:1460)
void audiolib_internal_new_playstate_stalled(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    play->new_vol = 0.01f;
    play->curr_vol = 0.01f;
    // audiolib_stream_pause(play, true), inlined.
    const audiolib_hw_play_state state = play->hw_play_state;
    if (state < AUDIOLIB_HW_PLAY_STATE_STOPPING)
    {
        if (state == AUDIOLIB_HW_PLAY_STATE_PLAYING)
        {
            if (audiolib_hw_pause(play, true) == AUDIOLIB_ERR_NONE)
            {
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PAUSED;
            }
        }
        else if (state == AUDIOLIB_HW_PLAY_STATE_STALLING || state == AUDIOLIB_HW_PLAY_STATE_STALLED)
        {
            if (audiolib_hw_pause(play, true) == AUDIOLIB_ERR_NONE)
            {
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STALLED;
            }
        }
        else
        {
            audiolib_hw_pause(play, true);
        }
    }
    play->stalled_time = time;
    // audiolib_marker_pause_marker_data(play, true), inlined (the pool is defined later in the unity TU).
    audiolib_marker_pause_marker_data(play, true);
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_STALLED);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049C890 (:1492)
void audiolib_internal_new_playstate_unstall(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    play->new_vol = play->pre_stalled_volume;
    play->action = (play->action & 0xDFFF) | 0x20;
    // The stall does not count as play time.
    const int stalled = time - play->stalled_time;
    play->start_time += stalled;
    play->end_time += stalled;
    play->fade_time = 100;
    play->stalled_time = 0;
    const audiolib_hw_play_state state = play->hw_play_state;
    if (state < AUDIOLIB_HW_PLAY_STATE_STOPPING && state != AUDIOLIB_HW_PLAY_STATE_PLAYING)
    {
        if (state == AUDIOLIB_HW_PLAY_STATE_PAUSED || state == AUDIOLIB_HW_PLAY_STATE_UNSTALLED)
        {
            if (audiolib_hw_pause(play, false) == AUDIOLIB_ERR_NONE)
            {
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PLAYING;
            }
        }
        else
        {
            audiolib_hw_pause(play, false);
        }
    }
    audiolib_marker_pause_marker_data(play, false);
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_PLAYING);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049C970 (:1532)
void audiolib_internal_new_playstate_pause(audiolib_play_data* play, int time)
{
    if (play == nullptr || play->hw_play_state <= AUDIOLIB_HW_PLAY_STATE_READY)
    {
        return;
    }
    // audiolib_stream_pause(play, true), inlined.
    const audiolib_hw_play_state state = play->hw_play_state;
    if (state < AUDIOLIB_HW_PLAY_STATE_STOPPING)
    {
        if (state == AUDIOLIB_HW_PLAY_STATE_PLAYING)
        {
            if (audiolib_hw_pause(play, true) == AUDIOLIB_ERR_NONE)
            {
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PAUSED;
            }
        }
        else if (state == AUDIOLIB_HW_PLAY_STATE_STALLING || state == AUDIOLIB_HW_PLAY_STATE_STALLED)
        {
            if (audiolib_hw_pause(play, true) == AUDIOLIB_ERR_NONE)
            {
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_STALLED;
            }
        }
        else
        {
            audiolib_hw_pause(play, true);
        }
    }
    play->action &= 0xFFFB;
    play->paused_time = time;
    audiolib_marker_pause_marker_data(play, true);
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_PAUSED);
    audiolib_internal_callback_event_add(play, 0x20, time, nullptr);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x004A1EF0 (:1569)
void audiolib_internal_new_playstate_unpause(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    play->action &= 0xFFF1;
    audiolib_update_calculations_for_output(play, time, 10);
    // audiolib_stream_pause(play, false), inlined; a failure ends the play.
    const audiolib_hw_play_state state = play->hw_play_state;
    bool resumed = true;
    if (state < AUDIOLIB_HW_PLAY_STATE_STOPPING && state != AUDIOLIB_HW_PLAY_STATE_PLAYING)
    {
        if (state == AUDIOLIB_HW_PLAY_STATE_PAUSED || state == AUDIOLIB_HW_PLAY_STATE_UNSTALLED)
        {
            resumed = audiolib_hw_pause(play, false) == AUDIOLIB_ERR_NONE;
            if (resumed)
            {
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PLAYING;
            }
        }
        else
        {
            resumed = audiolib_hw_pause(play, false) == AUDIOLIB_ERR_NONE;
        }
    }
    if (!resumed)
    {
        audiolib_internal_new_playstate_stopped(play, time);
        ++Audiolib_state.frame_stats.action_count;
        return;
    }
    const int paused = time - play->paused_time;
    play->start_time += paused;
    play->end_time += paused;
    play->paused_time = 0;
    audiolib_marker_pause_marker_data(play, false);
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_PLAYING);
    audiolib_internal_callback_event_add(play, 0x10, time, nullptr);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x004A1FF0 (:1608)
void audiolib_internal_new_playstate_stop(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    audiolib_stream_stop(play);
    play->end_time = time;
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_STOPPING);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049CA50 (:1641)
void audiolib_internal_new_playstate_stopped(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    play->action &= 0x8010;
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_STOPPED);
    audiolib_internal_callback_event_add(play, 0x80, time, nullptr);
    ++Audiolib_state.frame_stats.action_count;
}

// 0x0049CAC0 (:1660)
void audiolib_internal_new_playstate_destroy(audiolib_play_data* play, int time)
{
    if (play == nullptr)
    {
        return;
    }
    audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_DESTROYED);
    audiolib_internal_callback_event_add(play, 0x100, time, nullptr);
}

// 0x0049CB20 (:1684), play in EAX
static audiolib_result audiolib_free_and_destroy_play_instance(audiolib_play_data* play, int time)
{
    const unsigned short flags = play->params.flags;
    // A distance-kept 3D loop (params flag 0x40 or a max distance override) that was not destroyed on purpose
    // restarts as pending instead (params flag 0x80: a public address sound, never kept).
    bool free = true;
    if ((flags & 1) != 0 && (play->foley_data.max_dist_override || (flags & 0x40) != 0) && play->foley_data.looping &&
        (flags & 0x80) == 0 && (play->action & 0x8000) == 0 && !Audiolib_state.in_shutdown)
    {
        free = false;
    }
    Audiolib_play_cs.enter();
    if ((play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_MERGED || play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS) &&
        play->reference_count > 0)
    {
        const short references = play->reference_count;
        play->reference_count = (short)(references - 1);
        if (free)
        {
            if ((play->action & 0x8000) != 0)
            {
                play->reference_count = 0;
            }
            if (play->reference_count > 0)
            {
                // Another emitter still uses it.
                Audiolib_play_cs.leave();
                return AUDIOLIB_ERR_NONE;
            }
        }
        else
        {
            play->reference_count = references;
        }
    }
    if (play->hw_play_state > AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL || Audiolib_state.in_shutdown)
    {
        audiolib_hw_destroy(play);
    }
    if (play->marker_id != -1 && Audiolib_markers_inited)
    {
        audiolib_marker_remove_marker_from_process_list(play);
    }
    if (!free)
    {
        audiolib_internal_new_playstate_pending(play, time);
        play->action = 0;
        play->volume_scale = 0.0f;
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    audiolib_internal_category_instance_ended(play->category_id);
    if (play->foley_data.curr_simul != 0)
    {
        audiolib_foley_data* foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(play->foley_data.name_crc, nullptr));
        if (foley != nullptr)
        {
            foley->curr_simul = foley->curr_simul - 1;
        }
    }
    audiolib_playlist_free_merge_data(play);
    Play_instances.free(play);
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00489690 (:1818)
void audiolib_gather_stats_start()
{
    // The CPU times survive the reset.
    const int total_cpu_usage = Audiolib_state.frame_stats.total_cpu_usage;
    const int cpu_usage = Audiolib_state.frame_stats.cpu_usage;
    Audiolib_state.last_frame_stats = Audiolib_state.frame_stats;
    memset(&Audiolib_state.frame_stats, 0, sizeof(Audiolib_state.frame_stats));
    Audiolib_state.frame_stats.total_cpu_usage = total_cpu_usage;
    Audiolib_state.frame_stats.cpu_usage = cpu_usage;
}

// 0x004896E0 (:1840)
bool audiolib_stalled()
{
    return Audiolib_state.frame_stats.stalled > 0;
}

// 0x0049CC90 (:1845)
void audiolib_gather_stats_end()
{
    audiolib_gather_frame_stats& stats = Audiolib_state.frame_stats;
    stats.free_instances = Play_instances.get_free();
    stats.total_waiting = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL);
    stats.loading = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_LOADING);
    stats.ready = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_READY);
    stats.playing = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_PLAYING);
    stats.stalling = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_STALLING);
    stats.stalled = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_STALLED);
    stats.paused = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_PAUSED);
    stats.stopped = Play_instances.get_count(AUDIOLIB_HW_PLAY_STATE_STOPPED);
    // Loading up to playing.
    for (int state = AUDIOLIB_HW_PLAY_STATE_LOADING; state <= AUDIOLIB_HW_PLAY_STATE_PLAYING; ++state)
    {
        Play_instances.thread_access.enter();
        audiolib_play_data* next = Play_instances.get_first(state);
        while (next != nullptr)
        {
            audiolib_play_data* play = next;
            next = Play_instances.get_next(play);
            const int streaming = play->foley_data.wavebank_type == 1 ? 1 : 0;
            if (play->foley_data.looping)
            {
                ++stats.loops;
            }
            if (streaming != 0)
            {
                ++stats.streaming;
            }
            ++stats.mem_stats.num_loading[streaming];
            ++stats.mem_stats.num_playing[streaming];
            ++stats.mem_stats.num_paused[streaming];
            // Original: the DSP counts read the next play (null at the end of a list).
            if (next == nullptr)
            {
                break;
            }
            if (next->lpf_in_use)
            {
                if (next->reverb_in_use)
                {
                    ++stats.mem_stats.num_wet_dry_dsp[streaming];
                }
                else
                {
                    ++stats.mem_stats.num_dry_dsp[streaming];
                }
            }
            else if (next->reverb_in_use)
            {
                ++stats.mem_stats.num_wet_dsp[streaming];
            }
        }
        Play_instances.thread_access.leave();
    }
    stats.alrs = (int)ALR_list.get_capacity() - ALR_list.get_free();
    stats.engines = (int)Active_engine_list.get_capacity() - Active_engine_list.get_free();
    audiolib_gather_frame_stats& peak = Audiolib_state.peak_stats;
    peak.streaming = stats.streaming > peak.streaming ? stats.streaming : peak.streaming;
    peak.started_at_once = stats.started_at_once > peak.started_at_once ? stats.started_at_once : peak.started_at_once;
    peak.stopped = stats.stopped > peak.stopped ? stats.stopped : peak.stopped;
    peak.playing = stats.playing > peak.playing ? stats.playing : peak.playing;
    peak.stalling = stats.stalling > peak.stalling ? stats.stalling : peak.stalling;
    peak.stalled = stats.stalled > peak.stalled ? stats.stalled : peak.stalled;
    peak.total_waiting = stats.total_waiting > peak.total_waiting ? stats.total_waiting : peak.total_waiting;
    peak.waiting_on_memory = stats.waiting_on_memory > peak.waiting_on_memory ? stats.waiting_on_memory : peak.waiting_on_memory;
    peak.waiting_on_channel = stats.waiting_on_channel > peak.waiting_on_channel ? stats.waiting_on_channel : peak.waiting_on_channel;
    peak.waiting_on_dist = stats.waiting_on_dist > peak.waiting_on_dist ? stats.waiting_on_dist : peak.waiting_on_dist;
    peak.loading = stats.loading > peak.loading ? stats.loading : peak.loading;
    peak.alrs = stats.alrs > peak.alrs ? stats.alrs : peak.alrs;
    peak.engines = stats.engines > peak.engines ? stats.engines : peak.engines;
}

// 0x004896F0 (:1930)
void audiolib_throttle_scalar(float* current, float* desired, int num_steps, int this_step)
{
    if (current == nullptr || desired == nullptr)
    {
        return;
    }
    float share = 1.0f;
    if (num_steps > 0 && this_step < num_steps)
    {
        share = (float)this_step / (float)num_steps;
    }
    float step;
    while (fabsf(step = share * (*desired - *current)) > 0.1f)
    {
        share = share * 0.9f;
    }
    *current = *current + step;
}

// A play that is being dropped or destroyed stops (audiolib_internal_new_playstate_stop, inlined in the list walks).
static void audiolib_playlist_stop_dropped(audiolib_play_data* play, int time)
{
    audiolib_internal_new_playstate_stop(play, time);
}

// 0x004A65F0 (:2097)
bool audiolib_process_playlist(void* user_data, int time, int time_step)
{
    (void)user_data;
    Audiolib_play_cs.enter();
    audiolib_gather_stats_start();
    audiolib_play_data* next;
    // Destroyed and stopped plays are freed.
    static const int freed_states[2] = {AUDIOLIB_HW_PLAY_STATE_DESTROYED, AUDIOLIB_HW_PLAY_STATE_STOPPED};
    for (int state : freed_states)
    {
        Play_instances.thread_access.enter();
        next = Play_instances.get_first(state);
        while (next != nullptr)
        {
            audiolib_play_data* play = next;
            next = Play_instances.get_next(play);
            audiolib_free_and_destroy_play_instance(play, time);
        }
        Play_instances.thread_access.leave();
    }
    // Stopping plays end when the hardware is done.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_STOPPING);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        audiolib_stream_get_play_state(play);
        if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_STOPPED)
        {
            audiolib_internal_new_playstate_stopped(play, time);
        }
        if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_DESTROYED)
        {
            audiolib_internal_new_playstate_stopped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Playing plays: stalls, range drops, pauses, the output, or the stop.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_PLAYING);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        audiolib_stream_get_play_state(play);
        const audiolib_hw_play_state state = play->hw_play_state;
        if (state < AUDIOLIB_HW_PLAY_STATE_STOPPED)
        {
            if (state == AUDIOLIB_HW_PLAY_STATE_STALLING)
            {
                audiolib_internal_new_playstate_stalling(play, time);
                continue;
            }
            if (state == AUDIOLIB_HW_PLAY_STATE_STALLED)
            {
                audiolib_internal_new_playstate_stalled(play, time);
                continue;
            }
        }
        else
        {
            // The hardware is done with it.
            play->action |= 0x10;
        }
        // 2 m past the maximum distance a 3D play is dropped.
        if ((play->params.flags & 1) != 0 && (double)play->dist_max + 2.0 < (double)play->dist)
        {
            play->action |= 0x10;
        }
        const unsigned short action = play->action;
        if ((action & 0x8010) == 0)
        {
            if ((action & 4) == 0)
            {
                audiolib_update_calculations_for_output(play, time, time_step);
            }
            else
            {
                audiolib_internal_new_playstate_pause(play, time);
            }
        }
        else
        {
            if ((action & 0x4000) != 0)
            {
                play->action = action | 0x8000;
            }
            play->action &= 0x8010;
            audiolib_playlist_stop_dropped(play, time);
        }
        // A loop's end time moves on by its play time. Original: the raw variation union is added (the list pointer
        // for a multi-variation cue).
        if (play->foley_data.looping && play->end_time < time)
        {
            play->end_time = (int)(uintptr_t)play->foley_data.variation_list + play->end_time;
        }
    }
    Play_instances.thread_access.leave();
    // Stalling plays.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_STALLING);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        audiolib_stream_get_play_state(play);
        const audiolib_hw_play_state state = play->hw_play_state;
        if (state > AUDIOLIB_HW_PLAY_STATE_STOPPING)
        {
            play->action |= 0x10;
        }
        if (state == AUDIOLIB_HW_PLAY_STATE_PLAYING)
        {
            audiolib_internal_new_playstate_unstall(play, time);
        }
        else if (state == AUDIOLIB_HW_PLAY_STATE_STALLED)
        {
            audiolib_internal_new_playstate_stalled(play, time);
        }
        else if ((play->action & 0x8010) == 0)
        {
            audiolib_update_calculations_for_output(play, time, time_step);
        }
        else
        {
            audiolib_playlist_stop_dropped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Stalled plays.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_STALLED);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        audiolib_stream_get_play_state(play);
        const audiolib_hw_play_state state = play->hw_play_state;
        if (state > AUDIOLIB_HW_PLAY_STATE_STOPPING)
        {
            play->action |= 0x10;
        }
        if (state == AUDIOLIB_HW_PLAY_STATE_UNSTALLED)
        {
            audiolib_internal_new_playstate_unstall(play, time);
        }
        else if ((play->action & 0x8010) != 0)
        {
            audiolib_playlist_stop_dropped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Unstalled plays resume.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_UNSTALLED);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        if ((play->action & 0x8010) == 0)
        {
            audiolib_stream_get_play_state(play);
            audiolib_internal_new_playstate_unstall(play, time);
        }
        else
        {
            audiolib_playlist_stop_dropped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Paused plays wait for an unpause (or start) request.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_PAUSED);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        if ((play->action & 0x8010) == 0)
        {
            if ((play->action & 10) != 0)
            {
                audiolib_internal_new_playstate_unpause(play, time);
            }
        }
        else
        {
            audiolib_playlist_stop_dropped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Ready plays start once asked to and past their delay.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_READY);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        audiolib_stream_get_play_state(play);
        // Params flag 0x100: load only (a precache), never played.
        if ((play->params.flags & 0x100) != 0)
        {
            play->action = 0x8000;
        }
        if ((play->action & 0x8010) == 0)
        {
            if ((play->action & 10) != 0 && play->delay_start <= time)
            {
                audiolib_internal_new_playstate_play(play, time);
            }
        }
        else
        {
            audiolib_playlist_stop_dropped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Preparing plays become ready.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_PREPARING);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        if ((play->action & 0x8010) == 0)
        {
            audiolib_stream_get_play_state(play);
            if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_READY)
            {
                audiolib_internal_new_playstate_ready(play, time);
            }
            if (play->hw_play_state > AUDIOLIB_HW_PLAY_STATE_STOPPING)
            {
                audiolib_playlist_end_play(play, time);
            }
        }
        else
        {
            audiolib_playlist_stop_dropped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Loaded plays prepare.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_LOADED);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        if ((play->action & 0x8010) == 0)
        {
            audiolib_stream_get_play_state(play);
            if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_PREPARING)
            {
                audiolib_internal_new_playstate_preparing(play, time);
            }
            if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_READY)
            {
                audiolib_internal_new_playstate_ready(play, time);
            }
            if (play->hw_play_state > AUDIOLIB_HW_PLAY_STATE_STOPPING)
            {
                audiolib_playlist_end_play(play, time);
            }
        }
        else
        {
            audiolib_playlist_stop_dropped(play, time);
        }
    }
    Play_instances.thread_access.leave();
    // Loading plays: the load is asked for once, then the play waits for it.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_LOADING);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        if ((play->action & 0x8010) != 0)
        {
            audiolib_playlist_stop_dropped(play, time);
            continue;
        }
        if (play->load_requested)
        {
            audiolib_stream_get_play_state(play);
            if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_LOADED)
            {
                audiolib_internal_new_playstate_loaded(play, time);
            }
            else if (play->hw_play_state > AUDIOLIB_HW_PLAY_STATE_STOPPING)
            {
                audiolib_playlist_end_play(play, time);
            }
            continue;
        }
        // audiolib_internal_load_request, inlined with the current time.
        audiolib_internal_load_request(play, timer_get(TIMER_MILLISECONDS));
    }
    Play_instances.thread_access.leave();
    // Pending plays: dropped when they waited too long or are out of range, otherwise loaded.
    Play_instances.thread_access.enter();
    next = Play_instances.get_first(AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL);
    while (next != nullptr)
    {
        audiolib_play_data* play = next;
        next = Play_instances.get_next(play);
        play->output_framecount = 0;
        if (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS && play->params.priority < 7)
        {
            // A public address sound joins late at its offset; past its end it ends.
            const unsigned int offset = (unsigned int)(time - play->cue_time);
            play->params.offset_ms = offset;
            if (audiolib_playlist_variation_play_time(play) < offset)
            {
                audiolib_playlist_end_play(play, time);
                continue;
            }
        }
        if (play->waiting_for_channel > 0 && !play->foley_data.looping && play->params.priority <= 6)
        {
            // A one-shot that waited for a channel: twice its play time under a second, half over two seconds, at
            // most five seconds.
            int wait = (int)audiolib_playlist_variation_play_time(play);
            if (wait < 1000)
            {
                wait = wait * 2;
            }
            else if (wait > 1999)
            {
                wait = wait / 2;
            }
            if (wait > 5000)
            {
                wait = 5000;
            }
            if (time > (int)(play->cue_time + play->params.offset_ms + wait) && (play->params.flags & 0x100) == 0)
            {
                audiolib_playlist_end_play(play, time);
                ++Audiolib_state.frame_stats.dropped_sounds;
                audiolib_list_active_foley(false, "Dropped sound - too long before start");
                continue;
            }
        }
        bool destroy = (play->action & 0x8010) != 0;
        if (!destroy)
        {
            if (play->params.priority < 5 && audiolib_cue_start.time_until() > 0)
            {
                continue;
            }
            const unsigned short flags = play->params.flags;
            if ((flags & 0x100) == 0)
            {
                if ((play->action & 10) != 0)
                {
                    play->action &= 0xFFF3;
                }
                if ((play->action & 4) != 0)
                {
                    continue;
                }
                if ((flags & 1) != 0)
                {
                    play->curr_pos = play->new_pos;
                    audiolib_3d_calculate(play);
                    if (play->dist_max < play->dist)
                    {
                        // Out of range: kept waiting when distance-kept (and inside max_distance_to_keep_sounds in a
                        // level with a listener), otherwise dropped.
                        const bool keep_far = Audiolib_state.audio_game_state != AUDIOLIB_GAME_STATE_IN_LEVEL || Audiolib_state.listener_pos.x == 0.0f ||
                                              Audiolib_state.listener_pos.y == 0.0f || Audiolib_state.listener_pos.z == 0.0f ||
                                              play->dist <= Audiolib_state.max_distance_to_keep_sounds;
                        if (keep_far && ((play->params.flags & 0x40) != 0 || play->foley_data.max_dist_override))
                        {
                            ++Audiolib_state.frame_stats.waiting_on_dist;
                            if ((play->params.flags & 2) == 0)
                            {
                                continue;
                            }
                        }
                        else
                        {
                            play->action |= 0x8000;
                            ++Audiolib_state.frame_stats.dropped_sounds;
                            audiolib_list_active_foley(false, "Dropped sound - too far");
                            audiolib_internal_new_playstate_destroy(play, time);
                            continue;
                        }
                    }
                    else if ((play->params.flags & 0x40) != 0 || play->foley_data.max_dist_override)
                    {
                        audiolib_stream_get_play_state(play);
                    }
                }
            }
            // audiolib_stream_get_channel, inlined: a channel moves the play to loading.
            if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL && play->stream_info.stream_buffer_index == -1 &&
                play->stream_info.cache_index == -1)
            {
                play->stream_info.data_location = audiolib_internal_soundbank_is_bank_in_RAM(play->foley_data.wavebank_id)
                                                      ? AUDIOLIB_STREAM_DATA_IN_RAM
                                                      : AUDIOLIB_STREAM_DATA_NEEDS_STREAMING;
                play->load_requested = false;
                audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_LOADING);
                continue;
            }
        }
        audiolib_internal_new_playstate_destroy(play, time);
    }
    Play_instances.thread_access.leave();
    audiolib_gather_stats_end();
    Audiolib_play_cs.leave();
    // Distance-kept engine loops (params flag 0x800) that no vehicle engine uses any more are destroyed.
    Audiolib_engine_cs.enter();
    Audiolib_play_cs.enter();
    for (int state = AUDIOLIB_HW_PLAY_STATE_READY; state <= AUDIOLIB_HW_PLAY_STATE_PAUSED; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            const unsigned short flags = play->params.flags;
            if (play->foley_data.looping && (flags & 0x800) != 0 && (flags & 0x100) == 0 && (play->action & 0xC050) == 0 &&
                !audiolib_internal_check_sound_used_in_engine(play->play_id))
            {
                play->action |= 0x8000;
            }
        }
        Play_instances.thread_access.leave();
    }
    Audiolib_play_cs.leave();
    Audiolib_engine_cs.leave();
    return false;
}

// 0x004A86C0 (:2767)
bool audiolib_internal_playlist_system_init()
{
    Play_instances.clear();
    memset(audiolib_multi_sound_info, 0, sizeof(audiolib_multi_sound_info));
    for (vector& direction : audiolib_raycast_direction_vectors)
    {
        direction.normalize();
    }
    audiolib_cue_start.set(0);
    return audiolib_thread_function_register(audiolib_process_playlist, "audiolib_playlist_process", nullptr) == AUDIOLIB_ERR_NONE;
}

// 0x004A8750 (:2791)
void audiolib_internal_playlist_system_shutdown()
{
    // Every play is stopped, then the process waits (1 ms passes) until the lists are empty.
    for (;;)
    {
        Audiolib_play_cs.enter();
        bool empty = true;
        for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_STOPPED; ++state)
        {
            Play_instances.thread_access.enter();
            for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
            {
                empty = false;
                audiolib_playlist_move_to(play, AUDIOLIB_HW_PLAY_STATE_STOPPED);
            }
            Play_instances.thread_access.leave();
        }
        Audiolib_play_cs.leave();
        os_sleep(1);
        if (empty)
        {
            audiolib_thread_function_unregister(audiolib_process_playlist);
            Play_instances.clear();
            return;
        }
    }
}

// 0x00489D80 (:2824)
audiolib_result audiolib_internal_set_listener(const vector* position, const matrix* orient, const vector* velocity)
{
    matrix orientation;
    if (orient != nullptr)
    {
        orientation = *orient;
        orientation.fvec.normalize();
        orientation.uvec.normalize();
        orientation.rvec.normalize();
        orientation.fix();
    }
    Audiolib_play_cs.enter();
    if (position != nullptr)
    {
        Audiolib_state.listener_pos = *position;
    }
    if (velocity != nullptr)
    {
        Audiolib_state.listener_velocity = *velocity;
    }
    if (orient != nullptr)
    {
        Audiolib_state.orient_matrix = orientation;
        Audiolib_state.listener_front_orient = orientation.fvec;
        Audiolib_state.listener_up_orient = orientation.uvec;
        Audiolib_state.listener_right_orient = orientation.rvec;
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004960B0 (:2867)
audiolib_play_data* audiolib_internal_playlist_alloc(audiolib_foley_data* foley_data, vector& pos, audiolib_params* params, int& multi_index)
{
    if (foley_data == nullptr)
    {
        return nullptr;
    }
    Audiolib_play_cs.enter();
    multi_index = -1;
    // Merging: a category that allows it, not refused by params flag 0x200, a 3D position. Public address: params flag
    // 0x80 with a position list.
    bool merge = audiolib_category_is_merged_ok(foley_data->category_id);
    bool public_address = (params->flags & 0x80) != 0;
    if ((params->flags & 0x200) != 0)
    {
        merge = false;
    }
    if (foley_data->always_2d || (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f))
    {
        merge = false;
    }
    if (foley_data->always_2d || params->num_positions == 0)
    {
        public_address = false;
    }
    if (merge)
    {
        // A play of the same cue in range (but not within 1 cm) takes this one as another emitter.
        for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_STOPPED; ++state)
        {
            Play_instances.thread_access.enter();
            for (audiolib_play_data* other = Play_instances.get_first(state); other != nullptr; other = Play_instances.get_next(other))
            {
                if (other->foley_data.name_crc != foley_data->name_crc || other->reference_count >= 16)
                {
                    continue;
                }
                const float distance = vec_dist(other->curr_pos, pos);
                if (distance < 0.01f || other->dist_max <= distance)
                {
                    continue;
                }
                if (!audiolib_playlist_allocate_multi_data(other))
                {
                    break;
                }
                multi_index = audiolib_find_free_multi_data(other);
                if (multi_index < 0)
                {
                    continue;
                }
                if (multi_index == 0)
                {
                    // The first merge also records the play's own emitter.
                    other->dist = vec_dist(other->curr_pos, pos);
                    audiolib_playlist_copy_params_to_multi(other, params, multi_index);
                    multi_index = audiolib_find_free_multi_data(other);
                }
                other->dist = vec_dist(other->curr_pos, pos);
                audiolib_playlist_copy_params_to_multi(other, params, multi_index);
                ++other->reference_count;
                other->multi_cue_type = AUDIOLIB_MULTICUE_TYPE_MERGED;
                Play_instances.thread_access.leave();
                Audiolib_play_cs.leave();
                return other;
            }
            Play_instances.thread_access.leave();
        }
    }
    audiolib_play_data* play = Play_instances.alloc(AUDIOLIB_HW_PLAY_STATE_UNUSED);
    if (play != nullptr)
    {
        play->index = Play_instances.get_index(play);
        play->reference_count = 1;
        play->volume_scale = 0.0f;
        if (public_address && audiolib_playlist_allocate_multi_data(play))
        {
            // One emitter per listed position, starting at the first.
            play->multi_cue_type = AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS;
            play->curr_pos = params->position_list[0];
            play->new_pos = params->position_list[0];
            for (int i = 0; i < params->num_positions; ++i)
            {
                params->pos = params->position_list[i];
                audiolib_playlist_copy_params_to_multi(play, params, i);
            }
            play->reference_count = params->num_positions;
            multi_index = 0;
        }
        audiolib_stream_init_play_instance(play);
        // The cue's index in its XACT soundbank (the high half of the internal cue id).
        play->hw_cue_id = (short)(play->foley_data.internal_cue_id >> 16);
        play->hw_cue_id = (short)(foley_data->internal_cue_id >> 16);
        if ((merge || public_address) && multi_index < 0)
        {
            multi_index = 0;
        }
        play->play_id = (play->rolling_counter << 16) | (play->index & 0xFFF);
    }
    Audiolib_play_cs.leave();
    return play;
}

// 0x00489EC0 (:3004)
int audiolib_internal_choose_variation(audiolib_foley_data* foley_data)
{
    // Usage counts are 4-bit nibbles, two per byte. A variation is chosen at random weighted by 11 minus its usage,
    // never the last one played.
    const unsigned int variants = foley_data->num_variants;
    if (variants == 1)
    {
        foley_data->last_variation = 0;
        return 0;
    }
    int weights[8];
    int total_weight = 0;
    int total_usage = 0;
    const unsigned int last = foley_data->last_variation;
    for (unsigned int i = 0; i < variants; ++i)
    {
        const unsigned int usage = (foley_data->variation_usage[i >> 1] >> ((i & 1) * 4)) & 0xF;
        total_usage += (i & 1) == 0 ? (foley_data->variation_usage[i >> 1] & 0xF) : usage;
        int weight = 0;
        if (i != last)
        {
            weight = 11 - (int)usage;
            if (weight < 0)
            {
                weight = 0;
            }
        }
        weights[i] = weight;
        total_weight += weight;
    }
    int chosen = -1;
    // CRT rand() on the caller's thread (SFX.md 2.5). Original: no check for a zero total.
    const int roll = rand() % total_weight + 1;
    int cumulative = 0;
    for (unsigned int i = 0; i < foley_data->num_variants; ++i)
    {
        cumulative += weights[i];
        if (roll <= cumulative)
        {
            chosen = (int)i;
            break;
        }
    }
    if (total_usage > 10)
    {
        // Original: only the odd nibbles are cleared.
        for (unsigned int i = 0; i < foley_data->num_variants; ++i)
        {
            foley_data->variation_usage[i >> 1] &= 0xF;
        }
    }
    if (chosen < 0)
    {
        chosen = 0;
    }
    foley_data->last_variation = (unsigned int)chosen;
    unsigned char& usage = foley_data->variation_usage[chosen / 2];
    // Original: the usage count is never increased; an even variation clears its odd neighbour instead.
    if ((chosen & 1) == 0)
    {
        usage = usage & 0xF;
    }
    return chosen;
}

// 0x004911D0 (:3088)
audiolib_result audiolib_internal_playlist_get_variation_and_play_time(int play_id, int* play_time, int* variation)
{
    if (play_time != nullptr)
    {
        *play_time = -1;
    }
    if (variation != nullptr)
    {
        *variation = -1;
    }
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->play_state > AUDIOLIB_HW_PLAY_STATE_STOPPING)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NOT_PLAYING;
    }
    const audiolib_variation_info* info = audiolib_internal_get_variation_pointer(&play->foley_data, play->curr_variant);
    if (info == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_VARIATION_NOT_KNOWN;
    }
    if (play_time != nullptr)
    {
        *play_time = (int)info->play_time;
    }
    if (variation != nullptr)
    {
        *variation = play->curr_variant;
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004912B0 (:3118)
audiolib_result audiolib_internal_playlist_get_caller_cue_id(int play_id, int* cue_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (cue_id != nullptr)
    {
        // Original: a cue no longer in the foley list gives an id built from a null entry.
        const audiolib_foley_data* foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(play->foley_data.name_crc, nullptr));
        *cue_id = (int)((unsigned int)(foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)foley << 16);
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491330 (:3150)
audiolib_result audiolib_internal_playlist_get_state(int play_id, audiolib_play_state* curr_state)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (curr_state != nullptr)
    {
        *curr_state = play_state_lookup[play->play_state];
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491380 (:3165)
audiolib_result audiolib_internal_playlist_add_emitter(int play_id, const vector* pos, const vector* orientation)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (!audiolib_playlist_allocate_multi_data(play))
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_MULTI_CUE_NO_SLOTS;
    }
    int multi_index = audiolib_find_free_multi_data(play);
    if (multi_index == 0)
    {
        // The play's own emitter first. Original: with the play's params, as for a merge.
        audiolib_playlist_copy_params_to_multi(play, &play->params, multi_index);
        multi_index = audiolib_find_free_multi_data(play);
    }
    if (multi_index < 0)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_MULTI_CUE_NO_ROOM;
    }
    audiolib_params params;
    params.pos = *pos;
    params.orient = *orientation;
    audiolib_playlist_copy_params_to_multi(play, &params, multi_index);
    ++play->reference_count;
    play->multi_cue_type = AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004914A0 (:3207)
audiolib_result audiolib_internal_playlist_get_pause(int play_id, bool* pause)
{
    if (pause == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    *pause = play->play_state == AUDIOLIB_HW_PLAY_STATE_PAUSED;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004914F0 (:3219)
audiolib_result audiolib_internal_playlist_set_pause(int play_id, bool pause)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    unsigned short action = play->action;
    if ((action & 0x10) != 0)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    // A play not started yet keeps the pause in its params (flag 2). Started states get pause (4) or unpause (8) requests,
    // and unpausing a play that is ready also starts it (2).
    switch (play->play_state)
    {
    case AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL:
    case AUDIOLIB_HW_PLAY_STATE_LOAD_REQUEST:
    case AUDIOLIB_HW_PLAY_STATE_LOADING:
    case AUDIOLIB_HW_PLAY_STATE_LOADED:
    case AUDIOLIB_HW_PLAY_STATE_PREPARING:
        if (pause)
        {
            play->params.flags |= 2;
        }
        else if ((play->params.flags & 2) != 0)
        {
            play->params.flags &= 0xFFFD;
            play->action = action | 2;
        }
        break;
    case AUDIOLIB_HW_PLAY_STATE_READY:
        if (pause)
        {
            play->action = (action & 0xFFF7) | 4;
        }
        else
        {
            play->params.flags &= 0xFFF9;
            play->action = (action & 0xFFFB) | 10;
        }
        break;
    case AUDIOLIB_HW_PLAY_STATE_PLAYING:
    case AUDIOLIB_HW_PLAY_STATE_STALLING:
    case AUDIOLIB_HW_PLAY_STATE_STALLED:
    case AUDIOLIB_HW_PLAY_STATE_UNSTALLED:
        if (pause)
        {
            play->action = (action & 0xFFF7) | 4;
        }
        break;
    case AUDIOLIB_HW_PLAY_STATE_PAUSED:
        if (!pause)
        {
            play->action = (action & 0xFFFB) | 8;
        }
        break;
    default:
        break;
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491600 (:3296)
bool audiolib_internal_cue_is_fading(int play_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    const bool fading = play != nullptr && play->fade_time != 0 && (play->action & 0xC050) != 0;
    Audiolib_play_cs.leave();
    return fading;
}

// 0x00491650 (:3307)
audiolib_result audiolib_internal_playlist_fade_out(int play_id, const int fade_time_in_ms)
{
    const int now = timer_get(TIMER_MILLISECONDS);
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    // At least 100 ms.
    const int fade_time = fade_time_in_ms < 100 ? 100 : fade_time_in_ms;
    if ((play->action & 0xC050) != 0)
    {
        // Already ending: the new fade time, stopping from playing, destroying otherwise.
        play->fade_time = fade_time;
        play->vol_fade_start = now;
        play->action |= play->play_state == AUDIOLIB_HW_PLAY_STATE_PLAYING ? 0x4000 : 0x8000;
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    const short references = play->reference_count;
    if (references == 1)
    {
        if (Audiolib_markers_inited)
        {
            audiolib_marker_remove_marker_from_process_list(play);
        }
        if (play->play_state != AUDIOLIB_HW_PLAY_STATE_PLAYING)
        {
            play->action |= 0x8000;
            Audiolib_play_cs.leave();
            return AUDIOLIB_ERR_NONE;
        }
        play->action |= 0x4020;
        play->fade_time = fade_time;
        play->vol_fade_start = now;
        play->new_vol = 0.0f;
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    if (play->multi_info != nullptr)
    {
        if (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_MERGED)
        {
            // One emitter of a merged sound leaves.
            const int multi_index = (play_id >> 12) & 0xF;
            if (play->multi_info->data[multi_index].in_use)
            {
                audiolib_merged_data_clean_one_merged_sound(play, multi_index);
                --play->reference_count;
            }
        }
        else if (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS)
        {
            // All emitters but one leave, then the sound fades out and is dropped.
            int remaining = references - 1;
            for (int i = 0; remaining > 0 && i < references; ++i)
            {
                if (play->multi_info->data[i].in_use)
                {
                    audiolib_merged_data_clean_one_merged_sound(play, i);
                    --play->reference_count;
                    --remaining;
                }
            }
            audiolib_marker_remove_marker_data(play);
            play->action |= 0x60;
            play->fade_time = fade_time;
            play->vol_fade_start = now;
            play->new_vol = 0.0f;
        }
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x0049CF40 (:3394)
void audiolib_internal_playlist_kill_sounds_from_bank(short bank_id)
{
    if (audiolib_internal_soundbank_id_to_ptr(bank_id, false) == nullptr)
    {
        return;
    }
    Audiolib_play_cs.enter();
    for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_DESTROYED; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            if (play->foley_data.wavebank_id == bank_id || play->foley_data.soundbank_id == bank_id)
            {
                play->action = 0x8010;
            }
        }
        Play_instances.thread_access.leave();
    }
    Audiolib_play_cs.leave();
}

// 0x0049D060 (:3429)
bool audiolib_internal_playlist_sounds_from_bank_killed(short bank_id)
{
    if (audiolib_internal_soundbank_id_to_ptr(bank_id, false) == nullptr)
    {
        return true;
    }
    Audiolib_play_cs.enter();
    for (int state = AUDIOLIB_HW_PLAY_STATE_PENDING_FREE_CHANNEL; state <= AUDIOLIB_HW_PLAY_STATE_STOPPED; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            if (play->foley_data.wavebank_id == bank_id || play->foley_data.soundbank_id == bank_id)
            {
                // Still there: marked playing so the next pass asks the hardware and ends it.
                play->hw_play_state = AUDIOLIB_HW_PLAY_STATE_PLAYING;
                Play_instances.thread_access.leave();
                Audiolib_play_cs.leave();
                return false;
            }
        }
        Play_instances.thread_access.leave();
    }
    Audiolib_play_cs.leave();
    return true;
}

// 0x00491870 (:3461)
audiolib_result audiolib_internal_playlist_get_volume(int play_id, float* volume)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    *volume = play->new_vol;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004918C0 (:3471)
audiolib_result audiolib_internal_playlist_set_volume(int play_id, float volume, const int fade_time_ms)
{
    const int now = timer_get(TIMER_MILLISECONDS);
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (play->hw_play_state == AUDIOLIB_HW_PLAY_STATE_STALLING)
    {
        // Applied after the stall.
        play->pre_stalled_volume = volume;
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    // Original: a change during a fade waits for the fade to end.
    if (play->new_vol != volume && play->fade_time == 0)
    {
        play->fade_time = fade_time_ms;
        play->vol_fade_start = now;
        play->new_vol = volume;
    }
    play->action |= 0x20;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491980 (:3498)
audiolib_result audiolib_internal_playlist_get_low_pass(int play_id, float* low_pass)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    *low_pass = play->curr_low_pass;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x004919D0 (:3507)
audiolib_result audiolib_internal_playlist_set_low_pass(int play_id, const float low_pass)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    play->action |= 0x1000;
    play->new_low_pass = low_pass;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491A30 (:3518)
audiolib_result audiolib_internal_playlist_get_pitch(int play_id, float* pitch)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    *pitch = play->curr_pitch;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491A80 (:3528)
audiolib_result audiolib_internal_playlist_set_pitch(int play_id, const float pitch)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (!play->foley_data.pitch_changes_allowed)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    // audiolib_internal_foley_get_pitch_min_max, inlined: -1..1.
    float min_pitch = 0.0f;
    float max_pitch = 0.0f;
    if (play->foley_data.pitch_changes_allowed)
    {
        min_pitch = -1.0f;
        max_pitch = 1.0f;
    }
    float clamped = min_pitch;
    if (0.0f <= pitch - min_pitch)
    {
        clamped = pitch;
    }
    if (0.0f <= clamped - max_pitch)
    {
        clamped = max_pitch;
    }
    if (play->new_pitch != clamped)
    {
        play->action |= 0x200;
        play->new_pitch = clamped;
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491B70 (:3555)
audiolib_result audiolib_internal_playlist_get_pan(int play_id, float* pan)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr || pan == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    *pan = play->curr_pan;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491BC0 (:3565)
audiolib_result audiolib_internal_playlist_set_pan(int play_id, float pan)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    play->action |= 0x80;
    play->new_pan = pan;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491C20 (:3576)
bool audiolib_internal_playlist_is_2d(int play_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    const bool is_2d = play != nullptr && (play->params.flags & 1) == 0;
    Audiolib_play_cs.leave();
    return is_2d;
}

// 0x00491C60 (:3586)
audiolib_result audiolib_internal_playlist_change_to_2d(int play_id, const float volume)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        // Original: a bad cue id, not a bad play id.
        return AUDIOLIB_ERR_BAD_CUE_ID;
    }
    play->action |= 0x800;
    play->new_vol = volume;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491CC0 (:3597)
audiolib_result audiolib_internal_playlist_change_to_3d(int play_id, const vector* pos, const vector* orientation, const vector* velocity)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (pos != nullptr)
    {
        play->new_pos = *pos;
    }
    if (orientation != nullptr)
    {
        play->new_orient = *orientation;
    }
    if (velocity != nullptr)
    {
        play->new_velocity = *velocity;
    }
    play->action |= 0x400;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491D60 (:3610)
audiolib_result audiolib_internal_playlist_get_3d_position(int play_id, vector* pos, vector* orient, vector* velocity)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr || (play->params.flags & 1) == 0)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (pos != nullptr)
    {
        *pos = play->curr_pos;
    }
    if (orient != nullptr)
    {
        *orient = play->curr_orient;
    }
    if (velocity != nullptr)
    {
        *velocity = play->curr_velocity;
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00491E00 (:3624)
audiolib_result audiolib_internal_playlist_set_3d_position(int play_id, const vector* pos, const vector* orientation, const vector* velocity)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if ((play->params.flags & 1) == 0)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NOT_3D;
    }
    // Non-finite values are ignored.
    if (pos != nullptr && !(fl_is_finite(pos->x) && fl_is_finite(pos->y) && fl_is_finite(pos->z)))
    {
        pos = nullptr;
    }
    if (orientation != nullptr && !(fl_is_finite(orientation->x) && fl_is_finite(orientation->y) && fl_is_finite(orientation->z)))
    {
        orientation = nullptr;
    }
    if (velocity != nullptr && !(fl_is_finite(velocity->x) && fl_is_finite(velocity->y) && fl_is_finite(velocity->z)))
    {
        velocity = nullptr;
    }
    play->new_pos = play->curr_pos;
    play->new_orient = play->curr_orient;
    play->new_velocity = play->curr_velocity;
    if (pos != nullptr)
    {
        play->new_pos = *pos;
    }
    if (orientation != nullptr && !orientation->is_null())
    {
        play->new_orient = *orientation;
        play->new_orient.flatten_normalize();
    }
    if (velocity != nullptr)
    {
        play->new_velocity = *velocity;
    }
    play->action |= 0x100;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00492020 (:3690)
audiolib_result audiolib_internal_playlist_remove_emitter(int play_id, const vector& pos)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    // Params flag 0x80: a public address sound.
    if ((play->params.flags & 0x80) == 0)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NO_MULT_EMITTERS;
    }
    if (play->reference_count < 2)
    {
        play->action |= 0x8000;
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_NONE;
    }
    // Original: 26 entries are scanned (the array has 16).
    for (int i = 0; i < 26; ++i)
    {
        audiolib_multi_sound_instance& emitter = play->multi_info->data[i];
        if (emitter.in_use && vec_dist(emitter.pos, pos) < 1.0f)
        {
            emitter.in_use = false;
            emitter.pos.zero();
            --play->reference_count;
            Audiolib_play_cs.leave();
            return AUDIOLIB_ERR_NONE;
        }
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_BAD_PLAY_ID;
}

// 0x00492130 (:3744)
audiolib_result audiolib_internal_playlist_get_audible_range(int play_id, float* min, float* max)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    if (min != nullptr)
    {
        *min = play->dist_min;
    }
    if (max != nullptr)
    {
        *max = play->dist_max;
    }
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00492190 (:3756)
audiolib_result audiolib_internal_playlist_set_audible_range(int play_id, float min, float max)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    if (play == nullptr)
    {
        Audiolib_play_cs.leave();
        return AUDIOLIB_ERR_BAD_PLAY_ID;
    }
    play->dist_min = min;
    play->dist_max = max;
    Audiolib_play_cs.leave();
    return AUDIOLIB_ERR_NONE;
}

// 0x00496450 (:3768)
void audiolib_internal_playlist_streaming_sound_unload()
{
    // Every play holding streamed wave data is destroyed (no Audiolib_play_cs in rfg.exe).
    for (int state = 0; state < NUM_AUDIOLIB_HW_PLAY_STATES; ++state)
    {
        Play_instances.thread_access.enter();
        for (audiolib_play_data* play = Play_instances.get_first(state); play != nullptr; play = Play_instances.get_next(play))
        {
            if (play->hw_info.streamingWaveData.dataSize != 0)
            {
                play->action |= 0x8000;
            }
        }
        Play_instances.thread_access.leave();
    }
}

// 0x004921E0 (:3784)
int audiolib_internal_playlist_get_marker_id(int play_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    const int marker_id = play != nullptr ? play->marker_id : -1;
    Audiolib_play_cs.leave();
    return marker_id;
}

// 0x00492220 (:3793)
unsigned short audiolib_internal_playlist_get_callback_flags(int play_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    const unsigned short flags = play != nullptr ? play->params.callback_flags : 0;
    Audiolib_play_cs.leave();
    return flags;
}

// 0x00492260 (:3802)
void (*audiolib_internal_playlist_get_callback_function(int play_id))(AUDIOLIB_CUE_CALLBACK_DATA*)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    void (*function)(AUDIOLIB_CUE_CALLBACK_DATA*) = play != nullptr ? play->params.callback_func : nullptr;
    Audiolib_play_cs.leave();
    return function;
}

// 0x004922A0 (:3811)
void* audiolib_internal_playlist_get_callback_user_data(int play_id)
{
    Audiolib_play_cs.enter();
    audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
    void* user_data = play != nullptr ? play->params.callback_data : nullptr;
    Audiolib_play_cs.leave();
    return user_data;
}

// 0x0048A080 (:3820)
void audiolib_internal_play_cs_enter()
{
    Audiolib_play_cs.enter();
}

// 0x0048A090 (:3825)
void audiolib_internal_play_cs_leave()
{
    Audiolib_play_cs.leave();
}

// 0x004922E0 (:3830)
audiolib_result audiolib_play_get_remaining_play_time(int play_id, int* play_time)
{
    if (play_time != nullptr)
    {
        Audiolib_play_cs.enter();
        audiolib_play_data* play = audiolib_internal_playstate_id_to_ptr(play_id);
        if (play == nullptr)
        {
            Audiolib_play_cs.leave();
            return AUDIOLIB_ERR_NOT_PLAYING;
        }
        *play_time = play->end_time - timer_get(TIMER_MILLISECONDS);
        Audiolib_play_cs.leave();
    }
    return AUDIOLIB_ERR_NONE;
}
