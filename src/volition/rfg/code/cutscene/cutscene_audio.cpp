// Original: volition/rfg/code/cutscene/cutscene_audio.cpp
//
// Cutscene audio: the soundtrack is started paused so that it can prime, then unpaused on the first shot.

#include "volition/rfg/code/cutscene/cutscene_audio.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/cutscene/cutscene_internal.h"
#include "volition/vlib/code/os/os.h"

// ---- file statics (one translation unit with cutscene.cpp in rfg.exe)
static bool Cutscene_audio_is_playing; // 0x01E2B45D
static bool Cutscene_audio_prepared; // 0x01E2B45E

// 0x00705E20 (:82)
// Nothing in rfg.exe calls this; cutscene_cancel_audio_prime does the same unpause.
static void cs_manage_audio_end()
{
    audiolib_all_sounds_unpause(true, true);
}

// 0x00705E30 (:89)
static void cutscene_audio_prepared_callback(AUDIOLIB_CUE_CALLBACK_DATA* cue_cb_data)
{
    // Magic number: action 4 is the callback flag cutscene_prime_audio asks for, the cue being prepared.
    if (cue_cb_data->action == 4)
    {
        Cutscene_audio_prepared = true;
    }
}

// 0x00705E50 (:106)
void cs_manage_audio_start()
{
    audiolib_all_sounds_pause(true, true);
}

// 0x00705E60 (:112)
bool cutscene_prime_audio(cutscene_current_info* info)
{
    if (info->soundtrack.cue_id == -1 || info->soundtrack.play_id != -1)
    {
        return false;
    }
    Cutscene_audio_prepared = false;
    audiolib_params params;
    // Magic numbers: flags 6 start the cue paused, callback flag 4 reports when it is prepared.
    params.flags = 6;
    params.callback_flags = 4;
    params.callback_func = cutscene_audio_prepared_callback;
    info->soundtrack.play_id = audiolib_cue_play(info->soundtrack.cue_id, &params, nullptr);
    Cutscene_audio_is_playing = false;
    return true;
}

// 0x00705F00 (:140)
void cutscene_cancel_audio_prime(cutscene_current_info* info)
{
    // Original quirk kept: the sounds cs_manage_audio_start paused stay paused for a cutscene with no soundtrack.
    if (info->soundtrack.cue_id != -1)
    {
        audiolib_play_stop(info->soundtrack.play_id);
        Cutscene_audio_is_playing = false;
        audiolib_all_sounds_unpause(true, true);
    }
}

// 0x00705F30 (:155)
void cutscene_audio_pause()
{
}

// 0x00705F40 (:163)
void cutscene_audio_unpause()
{
}

// 0x00705F50 (:172)
void cutscene_audio_play(cutscene_current_info* cur_cutscene)
{
    if (cutscene_is_playing() && !Cutscene_audio_is_playing)
    {
        audiolib_play_set_pause(cur_cutscene->soundtrack.play_id, false);
        Cutscene_audio_is_playing = true;
    }
}

// 0x00705F80 (:188)
void cs_audio_prepare_blocking(cutscene_current_info* cur_cutscene)
{
    if (!cutscene_prime_audio(cur_cutscene))
    {
        return;
    }
    while (audiolib_get_sound_enabled())
    {
        if (Cutscene_audio_prepared)
        {
            return;
        }
        os_sleep(1);
    }
}
