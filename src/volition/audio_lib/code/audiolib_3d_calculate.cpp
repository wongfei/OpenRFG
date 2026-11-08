// Original: volition/audio_lib/code/audiolib_3d_calculate.cpp
//
// Volume and placement of a play (SFX.md SX2, 1.4 step 6): rolloff curves (a logarithmic curve, type 0, and a linear
// one, type 1, both from audiolib_3d_system_init), distance, distance low pass, occlusion, reverb send and doppler
// (audiolib_3d_calculate), the emitter cone and listener angle scales, the base volume, and the speaker matrix of a
// sound (front/back/left/right weights from the linear curve). Merged and public address multi-sounds sum or take the
// loudest of their positions. The keen port gives keen only the play's volume_scale and position (keen pans 3D sounds
// itself); the matrix is computed and not used (audiolib_hw_3d_update). Float rule as in fx8: float expressions are
// evaluated in double and rounded where rfg.exe stores them; chains without a store are written in double.
// Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_internal.h"

#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/stub/audiolib_stub.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <math.h>
#include <string.h>

// ---- public globals (rfg.pdb)
audiolib_rolloff_curve_data* audiolib_rolloff_curve_data_list[74]; // 0x01A79960
int audiolib_curve_list_num_curves; // 0x01A79A8C
// The built-in curves: (distance fraction, scale) pairs.
float audiolib_rolloff_curve_linear_entries[2][2] = {{0.0f, 0.0f}, {1.0f, 1.0f}}; // 0x014F9124
float audiolib_rolloff_curve_logarythmic_entries[11][2] = { // 0x014F9138
    {0.0f, 1.0f}, {0.1f, 0.699f}, {0.2f, 0.5229f}, {0.3f, 0.398f}, {0.4f, 0.3011f}, {0.5f, 0.2219f},
    {0.6f, 0.155f}, {0.7f, 0.097f}, {0.8f, 0.046f}, {0.9f, 0.02f}, {1.0f, 0.0f},
};

// ---- file-static globals
// Debug switches, never set in rfg.exe.
static bool s_disableRFGVolumeAdjustmentByDistance; // 0x01A7AEE3
static bool s_disableRFGVolumeEffects; // 0x01A7AEE4

static const float Audiolib_pi = 3.1415927f;

// A value clamped to [min, max] as rfg.exe writes it (value - min < 0, then 0 <= value - max).
static float audiolib_3d_clamp(float value, float min, float max)
{
    if (value - min < 0.0f)
    {
        value = min;
    }
    if (0.0f <= value - max)
    {
        value = max;
    }
    return value;
}

// 0x00483930 (:354)
audiolib_rolloff_curve_data* audiolib_rolloff_curve_find_appropriate_curve(signed char curve_type, const char* curve_name)
{
    // Type 1 is the linear curve; type 0 (or no name) the logarithmic one, which is also the fallback.
    if (curve_type == 1)
    {
        return audiolib_rolloff_curve_data_list[1];
    }
    if (curve_type == 0 || curve_name == nullptr)
    {
        return audiolib_rolloff_curve_data_list[0];
    }
    const unsigned int name_crc = crc_str(curve_name, 0);
    for (int i = 0; i < audiolib_curve_list_num_curves; ++i)
    {
        audiolib_rolloff_curve_data* curve = audiolib_rolloff_curve_data_list[i];
        if (curve->type == curve_type && curve->name_crc == name_crc)
        {
            return curve;
        }
    }
    return audiolib_rolloff_curve_data_list[0];
}

// 0x004839B0 (:391)
bool audiolib_create_rolloff_curve(signed char type, const char* name, int num_entries, float* rolloff_list)
{
    if (num_entries == 0 || rolloff_list == nullptr || num_entries > 16)
    {
        return false;
    }
    // Original: 8 bytes per entry on top of the whole struct (which holds 16 already).
    const unsigned int size = num_entries * 8 + sizeof(audiolib_rolloff_curve_data);
    audiolib_rolloff_curve_data* curve = (audiolib_rolloff_curve_data*)Audiolib_state.pool->alloc(size, 0);
    if (curve != nullptr)
    {
        memset(curve, 0, size);
        curve->type = type;
        strncpy(curve->name, name, sizeof(curve->name));
        curve->name_crc = crc_str(name, 0);
        curve->num_entries = num_entries;
        for (int i = 0; i < num_entries; ++i)
        {
            curve->curve[i][0] = rolloff_list[i * 2];
            curve->curve[i][1] = rolloff_list[i * 2 + 1];
        }
    }
    // Original: a failed allocation still takes a list slot.
    audiolib_rolloff_curve_data_list[audiolib_curve_list_num_curves] = curve;
    ++audiolib_curve_list_num_curves;
    return curve != nullptr;
}

// 0x00483AC0 (:426)
float audiolib_calc_scaled_value(signed char curve_type, audiolib_rolloff_curve_data* curve_data, float min_dist, float max_dist, float val_at_min, float val_at_max, float distance)
{
    if (distance < min_dist)
    {
        return val_at_min;
    }
    if (max_dist <= distance)
    {
        return val_at_max;
    }
    if (curve_data == nullptr)
    {
        curve_data = curve_type == 1 ? audiolib_rolloff_curve_data_list[1] : audiolib_rolloff_curve_data_list[0];
        if (curve_data == nullptr)
        {
            return 0.0f;
        }
    }
    float fraction = (distance - min_dist) / (max_dist - min_dist);
    // The linear curve is read from the far end.
    if (curve_type == 1)
    {
        fraction = 1.0f - fraction;
    }
    float scale = 1.0f;
    for (int i = 1; i < curve_data->num_entries; ++i)
    {
        if (fraction < curve_data->curve[i][0])
        {
            const float x0 = curve_data->curve[i - 1][0];
            const float y0 = curve_data->curve[i - 1][1];
            const float dx = curve_data->curve[i][0] - x0;
            const float dy = curve_data->curve[i][1] - y0;
            float t = 0.5f;
            if (dx > 0.0f)
            {
                t = (fraction - x0) / dx;
            }
            scale = (float)((double)t * (double)dy + (double)y0);
            break;
        }
    }
    if (val_at_min <= val_at_max)
    {
        return (float)(((double)val_at_max - (double)val_at_min) * (double)scale + (double)val_at_min);
    }
    return (float)(((double)val_at_min - (double)val_at_max) * (double)scale + (double)val_at_max);
}

// A weight from the linear curve over 0..2: 1 at 0, 0.1 at 2 (the speaker weights, inlined in rfg.exe).
static float audiolib_3d_speaker_weight(float position)
{
    return audiolib_calc_scaled_value(1, nullptr, 0.0f, 2.0f, 1.0f, 0.1f, position);
}

// 0x00483CA0 (:495)
bool audiolib_calculate_speaker_volumes(audiolib_play_data* play, audiolib_channel_to_speaker_map* output_map)
{
    const float center_scalar = Audiolib_settings.center_speaker_scalar;
    const float low_freq_scalar = Audiolib_settings.low_freq_speaker_scalar;
    if (play == nullptr || output_map == nullptr)
    {
        return false;
    }
    // The 2D weights, once (function statics with the $S1 guard 0x01AC3734).
    static float f_scale_2d = audiolib_3d_speaker_weight(0.5f); // 0x01AC3730
    static float b_scale_2d = audiolib_3d_speaker_weight(1.5f); // 0x01AC372C
    static float lr_scale_2d = audiolib_3d_speaker_weight(1.0f); // 0x01AC3728
    static float f_scale_music = audiolib_3d_speaker_weight(0.9f); // 0x01AC3724
    static float b_scale_music = audiolib_3d_speaker_weight(1.1f); // 0x01AC3720
    memset(output_map, 0, sizeof(*output_map));
    output_map->num_src_channels = play->foley_data.input_channels;
    output_map->num_dst_channels = Audiolib_state.speaker_count;
    float front;
    float back;
    float left;
    float right;
    if ((play->params.flags & 1) == 0)
    {
        // 2D: panned between left and right.
        left = lr_scale_2d;
        right = lr_scale_2d;
        if (play->curr_pan != 0.0f)
        {
            left = audiolib_3d_speaker_weight(1.0f - play->curr_pan);
            right = audiolib_3d_speaker_weight(play->curr_pan + 1.0f);
        }
        if (play->category_id == Audiolib_state.music_cat_id)
        {
            front = f_scale_music;
            back = b_scale_music;
        }
        else
        {
            front = f_scale_2d;
            back = b_scale_2d;
        }
    }
    else
    {
        // 3D: the direction to the sound in the listener's frame.
        const vector to_sound(play->curr_pos.x - Audiolib_state.listener_pos.x, play->curr_pos.y - Audiolib_state.listener_pos.y,
                              play->curr_pos.z - Audiolib_state.listener_pos.z);
        vector direction = to_sound.copy_normalize_safe();
        const float heading = (float)atan2((double)Audiolib_state.listener_front_orient.z, (double)Audiolib_state.listener_front_orient.x);
        // + pi/2 as a float (x87 in rfg.exe, stored as a float).
        direction.rotate_around_y_axis((float)((double)heading + (double)1.5707964f));
        // Stereo has no front weight.
        front = Audiolib_state.speaker_type == AUDIOLIB_SPEAKER_TYPE_STEREO ? 1.0f : audiolib_3d_speaker_weight(direction.z + 1.0f);
        back = audiolib_3d_speaker_weight(1.0f - direction.z);
        left = audiolib_3d_speaker_weight(1.0f - direction.x);
        right = audiolib_3d_speaker_weight(direction.x + 1.0f);
    }
    const float center = (back + front) * 0.5f;
    const float volume = play->volume_scale;
    // The cross-feed between the sides at a tenth of the volume.
    const float cross_volume = volume * 0.1f;
    float* matrix = output_map->matrix;
    const int channels = output_map->num_src_channels;
    if (channels == 2)
    {
        const float front_left = front * left;
        const float front_right = front * right;
        matrix[0] = volume * front_left;
        matrix[1] = cross_volume * front_right;
        matrix[2] = cross_volume * front_left;
        matrix[3] = volume * front_right;
        matrix[4] = center_scalar * front_left * play->volume_scale;
        matrix[5] = center_scalar * front_right * play->volume_scale;
        matrix[6] = left * low_freq_scalar * play->volume_scale;
        matrix[7] = right * low_freq_scalar * play->volume_scale;
        matrix[8] = volume * back * left;
        matrix[9] = cross_volume * back * right;
        matrix[10] = cross_volume * back * left;
        matrix[11] = volume * back * right;
        matrix[12] = volume * center * left;
        matrix[13] = cross_volume * center * right;
        matrix[14] = cross_volume * center * left;
        matrix[15] = volume * center * right;
        return true;
    }
    if (channels == 6)
    {
        for (int i = 0; i < 8; ++i)
        {
            matrix[i] = volume;
        }
        return true;
    }
    // Mono (and any other count): one column of 8 speakers.
    matrix[0] = front * left * play->volume_scale;
    matrix[1] = front * right * play->volume_scale;
    matrix[2] = front * center_scalar * play->volume_scale;
    matrix[3] = play->volume_scale * low_freq_scalar;
    matrix[4] = back * left * play->volume_scale;
    matrix[5] = back * right * play->volume_scale;
    matrix[6] = center * left * play->volume_scale;
    matrix[7] = center * right * play->volume_scale;
    return true;
}

// 0x0048DD10 (:665)
float audiolib_3d_scale_orientation_position_volume(audiolib_play_data* play)
{
    // The emitter cone: quieter behind a sound with an orientation (category cone angle and scale; 90 and 0.8
    // without a category).
    float cone_scale = 1.0f;
    if (play->curr_orient.x != ZERO_VECTOR.x || play->curr_orient.y != ZERO_VECTOR.y || play->curr_orient.z != ZERO_VECTOR.z)
    {
        float cone_angle = 90.0f;
        float cone_min_scale = 0.8f;
        bool use_cone = true;
        if (play->category_id >= 0 && play->category_id < Audiolib_category_list.count)
        {
            cone_angle = Audiolib_category_list.category[play->category_id].emitter_cone_angle;
            cone_min_scale = Audiolib_category_list.category[play->category_id].emitter_cone_min_scale;
            use_cone = cone_angle > 0.0f;
        }
        if (use_cone)
        {
            const float facing = Audiolib_pi - fabsf(vec_delta_ang(Audiolib_state.listener_front_orient, play->curr_orient, UP_VECTOR));
            cone_scale = audiolib_calc_scaled_value(1, nullptr, cone_angle, Audiolib_pi, 1.0f, cone_min_scale, facing);
        }
    }
    // The listener angle: quieter beside and behind the listener.
    float angle_scale = 1.0f;
    if (Audiolib_settings.listener_angle_volume_dropoff > 0.0f)
    {
        vector to_sound(play->curr_pos.x - Audiolib_state.listener_pos.x, play->curr_pos.y - Audiolib_state.listener_pos.y,
                        play->curr_pos.z - Audiolib_state.listener_pos.z);
        if (to_sound.x != 0.0f || to_sound.z != 0.0f)
        {
            to_sound.flatten_normalize();
            const float angle = fabsf(vec_delta_ang(Audiolib_state.listener_front_orient, to_sound, UP_VECTOR));
            angle_scale = audiolib_calc_scaled_value(1, nullptr, Audiolib_settings.listener_angle_volume_dropoff, Audiolib_pi, 1.0f,
                                                     Audiolib_settings.listener_angle_volume_min, angle);
        }
    }
    return audiolib_3d_clamp(angle_scale * cone_scale, 0.0f, 1.0f);
}

// 0x00485100 (:721), play in ESI
static audiolib_result audiolib_3d_calculate_one(audiolib_play_data* play)
{
    if (s_disableRFGVolumeEffects)
    {
        return AUDIOLIB_ERR_NONE;
    }
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    const unsigned short flags = play->params.flags;
    // Params flags: 1 3D, 0x10 no low pass, 0x20 no reverb. DSP type bits: 1 own low pass, 2 own reverb.
    if ((flags & 1) == 0 && !play->affected_by_environ && !play->affected_by_health)
    {
        return AUDIOLIB_ERR_NONE;
    }
    if (!play->affected_by_environ)
    {
        play->curr_low_pass = 0.0f;
        play->new_low_pass = 0.0f;
        play->reverb_scale = 0.0f;
        play->doppler_factor = 0.0f;
        play->dist = 0.0f;
        return AUDIOLIB_ERR_NONE;
    }
    if ((flags & 1) == 0)
    {
        play->dist = 0.0f;
    }
    else if (Audiolib_state.position_valid)
    {
        play->dist = vec_dist(Audiolib_state.listener_pos, play->curr_pos);
    }
    else
    {
        play->dist = 10000.0f;
    }
    const float old_low_pass = play->new_low_pass;
    if ((flags & 0x10) == 0)
    {
        if ((play->params.dsp_type & 1) == 0)
        {
            // Distance low pass: none up close, distance_lpf_value_at_max from distance_lpf_max on.
            const float distance = play->dist;
            play->new_low_pass = 0.0f;
            if (distance > 0.0f)
            {
                play->new_low_pass = audiolib_calc_scaled_value(1, nullptr, Audiolib_settings.distance_lpf_min, Audiolib_settings.distance_lpf_max, 0.0f,
                                                                Audiolib_settings.distance_lpf_value_at_max, distance);
            }
        }
        else
        {
            play->new_low_pass = play->params.dsp.low_pass.effect;
        }
    }
    else
    {
        play->new_low_pass = 0.0f;
        play->curr_low_pass = 0.0f;
    }
    if (play->affected_by_health)
    {
        play->new_low_pass = play->new_low_pass + Audiolib_state.low_health_low_pass_adjust;
    }
    if ((flags & 1) != 0 && !play->foley_data.do_not_apply_occlusion && (flags & 0x10) == 0)
    {
        const float occlusion = audiolib_3d_clamp(play->occlusion, 0.0f, 1.0f);
        play->new_low_pass = (float)((double)Audiolib_settings.occlusion_lpf_add * (double)occlusion + (double)play->new_low_pass);
    }
    play->new_low_pass = audiolib_3d_clamp(play->new_low_pass, 0.0f, 1.0f);
    play->new_low_pass = play->new_low_pass * Audiolib_settings.low_pass_platform_factor;
    // Actions 0x40/0x4000 (a fade in/out): the low pass only eases off, 0.05 per pass.
    if ((play->action & 0x4040) != 0 && play->new_low_pass > 0.0f)
    {
        float eased = old_low_pass - 0.05f;
        if (eased - 0.0f < 0.0f)
        {
            eased = 0.0f;
        }
        play->new_low_pass = eased;
    }
    // Action 0x1000: the hardware low pass needs an update.
    if (play->new_low_pass > 0.0f || play->new_low_pass != play->curr_low_pass)
    {
        play->action |= 0x1000;
    }
    if ((flags & 0x20) == 0)
    {
        if ((play->params.dsp_type & 2) == 0)
        {
            // Reverb of the listener's room, for mono and stereo sounds in range while the listener sees enough sky.
            play->reverb_type = Audiolib_state.room_dimensions.reverb_type;
            play->reverb_in_use = true;
            play->reverb_scale = 0.0f;
            const unsigned int channels = play->foley_data.input_channels;
            if (Audiolib_settings.minimum_reverb_fov < Audiolib_state.listener_fov && play->dist < Audiolib_settings.reverb_maximum_distance &&
                (channels == 1 || channels == 2))
            {
                play->reverb_scale = audiolib_calc_scaled_value(1, nullptr, Audiolib_settings.reverb_minimum_distance, Audiolib_settings.reverb_maximum_distance,
                                                                1.0f, 0.0f, play->dist);
                play->reverb_scale = play->reverb_scale * Audiolib_settings.max_reverb_value;
                play->reverb_scale = audiolib_3d_clamp(play->reverb_scale, 0.0f, Audiolib_settings.max_reverb_value);
            }
        }
        else
        {
            play->reverb_scale = play->params.dsp.reverb.effect;
            play->reverb_type = play->params.dsp.reverb.room_type;
            play->reverb_in_use = true;
        }
    }
    else
    {
        play->reverb_scale = 0.0f;
        play->reverb_in_use = false;
    }
    // Doppler from the change of distance since the last pass, -1..1.
    const float approach = play->dist - play->prev_dist;
    play->doppler_factor = 0.0f;
    if (fabsf(approach) > 0.0f)
    {
        play->doppler_factor = Audiolib_settings.speed_of_sound;
        play->doppler_factor = Audiolib_settings.speed_of_sound - Audiolib_settings.speed_of_sound * approach;
        play->doppler_factor = play->doppler_factor / Audiolib_settings.speed_of_sound - 1.0f;
        play->doppler_factor = play->doppler_factor * Audiolib_settings.doppler_scale;
        play->doppler_factor = audiolib_3d_clamp(play->doppler_factor, -1.0f, 1.0f);
    }
    play->prev_dist = play->dist;
    return AUDIOLIB_ERR_NONE;
}

// 0x0048E2F0 (:862)
float audiolib_internal_calculate_base_volume(audiolib_play_data* play)
{
    float volume = 1.0f;
    audiolib_internal_get_category_volume(play->category_id, &volume, true, play);
    volume = play->curr_vol * volume;
    if (play->affected_by_health)
    {
        volume = Audiolib_state.low_health_volume_adjust * volume;
    }
    // Volume scalar 6: the effects volume.
    return Audiolib_settings.overall_volume * volume * Audiolib_state.volume_scalars[6];
}

// 0x0048E390 (:887), play in ESI
static audiolib_result audiolib_3d_create_volume_matrix_one(audiolib_play_data* play, audiolib_channel_to_speaker_map* output_map)
{
    // audiolib_internal_calculate_base_volume, inlined with the scalars multiplied first.
    float volume = 1.0f;
    audiolib_internal_get_category_volume(play->category_id, &volume, true, play);
    volume = play->curr_vol * volume;
    if (play->affected_by_health)
    {
        volume = Audiolib_state.low_health_volume_adjust * volume;
    }
    volume = Audiolib_state.volume_scalars[6] * Audiolib_settings.overall_volume * volume;
    const float max_volume = Audiolib_settings.overall_volume * 2.0f;
    float result;
    if ((play->params.flags & 1) == 0)
    {
        // A 2D sound sits on the listener.
        play->curr_pos = Audiolib_state.listener_pos;
        play->curr_orient = Audiolib_state.listener_front_orient;
        play->dist = 1e-06f;
        result = volume;
    }
    else
    {
        audiolib_3d_calculate(play);
        bool check_curve_2 = true;
        if (play->affected_by_environ)
        {
            if (!s_disableRFGVolumeAdjustmentByDistance)
            {
                play->dist_scale = audiolib_calc_scaled_value(play->params.rolloff_curve, nullptr, play->dist_min, play->dist_max, 1.0f, 0.0f, play->dist);
                volume = play->dist_scale * volume;
            }
            else
            {
                play->dist_scale = 1.0f;
            }
            if (play->occlusion > 0.0f)
            {
                // Occlusion takes off occlusion_vol_adjust per unit, at most occlusion_percentage of the volume.
                float occlusion_cut = play->occlusion * Audiolib_settings.occlusion_vol_adjust;
                if (occlusion_cut - 0.0f < 0.0f)
                {
                    occlusion_cut = 0.0f;
                }
                float cut = Audiolib_settings.occlusion_percentage * volume;
                if (occlusion_cut - Audiolib_settings.occlusion_percentage * volume < 0.0f)
                {
                    cut = occlusion_cut;
                }
                volume = volume - cut;
            }
            if (!s_disableRFGVolumeAdjustmentByDistance)
            {
                volume = audiolib_3d_scale_orientation_position_volume(play) * volume;
            }
            else
            {
                check_curve_2 = false;
            }
        }
        result = volume;
        if (check_curve_2 && !s_disableRFGVolumeAdjustmentByDistance && play->params.rolloff_curve == 2)
        {
            // Curve 2: silent below the minimum distance, rising to the volume at the curve midpoint.
            const float distance = play->dist;
            if (play->dist_min <= distance)
            {
                if (distance < play->params.curve_midpoint)
                {
                    result = audiolib_calc_scaled_value(2, nullptr, play->dist_min, play->params.curve_midpoint, 0.0f, volume, distance);
                }
            }
            else
            {
                result = 0.0f;
            }
        }
        if (result < 0.0f || result > max_volume)
        {
            float clamped = 0.0f;
            if (0.0f <= result - 0.0f)
            {
                clamped = result;
            }
            result = max_volume;
            if (clamped - max_volume < 0.0f)
            {
                result = clamped;
            }
        }
    }
    // Without a listener a 3D sound is nearly silent.
    if (!Audiolib_state.position_valid && (play->params.flags & 1) != 0)
    {
        result = 0.01f;
    }
    // At most 0.1 change per pass.
    const float current = play->volume_scale;
    if (fabsf(result - current) > 0.1f)
    {
        if (result > current)
        {
            result = current + 0.1f;
        }
        else if (result < current)
        {
            result = current - 0.1f;
        }
    }
    play->volume_scale = result;
    if (!Audiolib_state.enabled)
    {
        play->volume_scale = 0.0f;
    }
    audiolib_calculate_speaker_volumes(play, output_map);
    return AUDIOLIB_ERR_NONE;
}

// 0x0048E770 (:1011)
audiolib_result audiolib_3d_create_volume_matrix(audiolib_play_data* play, audiolib_channel_to_speaker_map* output_map)
{
    if (play == nullptr || output_map == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_multi_sound_info_struct* multi_info = play->multi_info;
    if (multi_info != nullptr)
    {
        play->curr_pos = multi_info->data[0].pos;
        play->curr_vol = multi_info->data[0].volume;
    }
    const bool merged = play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_MERGED || play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS;
    if (!merged || multi_info == nullptr)
    {
        // Original: the matrix size is not set on these paths (audiolib_calculate_speaker_volumes sets it).
        audiolib_3d_create_volume_matrix_one(play, output_map);
        if (!merged)
        {
            return AUDIOLIB_ERR_NONE;
        }
    }
    else
    {
        // One matrix per emitter, then their mean (merged) or maximum (public address).
        const vector saved_pos = play->curr_pos;
        const float saved_vol = play->curr_vol;
        audiolib_channel_to_speaker_map maps[16];
        memset(maps, 0, sizeof(maps));
        memset(output_map, 0, sizeof(*output_map));
        int count = 0;
        for (int i = 0; i < 16; ++i)
        {
            maps[count].num_src_channels = play->foley_data.input_channels;
            maps[count].num_dst_channels = Audiolib_state.speaker_count;
            if (play->multi_info->data[i].in_use)
            {
                play->curr_pos = play->multi_info->data[i].pos;
                play->curr_vol = play->multi_info->data[i].volume;
                audiolib_3d_create_volume_matrix_one(play, &maps[count]);
                ++count;
            }
        }
        play->curr_pos = saved_pos;
        play->curr_vol = saved_vol;
        const int entries = maps[0].num_dst_channels * maps[0].num_src_channels;
        if (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_MERGED)
        {
            for (int j = 0; j < entries; ++j)
            {
                float sum = output_map->matrix[j];
                for (int k = 0; k < count; ++k)
                {
                    sum = maps[k].matrix[j] + sum;
                }
                output_map->matrix[j] = sum;
                // Original: no check for zero emitters.
                output_map->matrix[j] = output_map->matrix[j] / (float)count;
            }
        }
        if (play->multi_cue_type == AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS)
        {
            for (int j = 0; j < entries; ++j)
            {
                for (int k = 0; k < count; ++k)
                {
                    if (output_map->matrix[j] < maps[k].matrix[j])
                    {
                        output_map->matrix[j] = maps[k].matrix[j];
                    }
                }
            }
        }
    }
    output_map->num_src_channels = play->foley_data.input_channels;
    output_map->num_dst_channels = Audiolib_state.speaker_count;
    return AUDIOLIB_ERR_NONE;
}

// 0x004858F0 (:1128)
audiolib_result audiolib_3d_calculate(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    if (play->multi_info == nullptr ||
        (play->multi_cue_type != AUDIOLIB_MULTICUE_TYPE_MERGED && play->multi_cue_type != AUDIOLIB_MULTICUE_TYPE_PUBLIC_ADDRESS))
    {
        return audiolib_3d_calculate_one(play);
    }
    // A multi-sound is placed at its nearest emitter (each emitter's distance is kept). Original: 5000 starts both
    // the nearest distance and the low pass, so they stay 5000 without an emitter in use.
    float nearest_dist = 5000.0f;
    float nearest_low_pass = 5000.0f;
    int nearest = 0;
    for (int i = 0; i < 16; ++i)
    {
        audiolib_multi_sound_instance& emitter = play->multi_info->data[i];
        if (!emitter.in_use)
        {
            continue;
        }
        play->curr_pos = emitter.pos;
        audiolib_3d_calculate_one(play);
        emitter.dist = play->dist;
        if (play->dist < nearest_dist)
        {
            nearest_low_pass = play->new_low_pass;
            nearest = i;
            nearest_dist = play->dist;
        }
    }
    play->curr_pos = play->multi_info->data[nearest].pos;
    play->dist = nearest_dist;
    play->new_low_pass = nearest_low_pass;
    return AUDIOLIB_ERR_NONE;
}

// 0x00494F20 (:1189)
audiolib_result audiolib_3d_setup_lowpass_and_reverb(audiolib_play_data* play)
{
    if (play == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_internal_dsp_hw_setup(play);
    return AUDIOLIB_ERR_NONE;
}

// 0x00485A10 (:1197)
bool audiolib_3d_system_init()
{
    audiolib_curve_list_num_curves = 0;
    memset(audiolib_rolloff_curve_data_list, 0, sizeof(audiolib_rolloff_curve_data_list));
    // Slots 0 and 1, which the rest of the library takes by index.
    audiolib_create_rolloff_curve(0, "Logarythmic", 11, audiolib_rolloff_curve_logarythmic_entries[0]);
    audiolib_create_rolloff_curve(1, "Linear", 2, audiolib_rolloff_curve_linear_entries[0]);
    return true;
}

// 0x00485A60 (:1213)
void audiolib_3d_system_shutdown()
{
    audiolib_curve_list_num_curves = 0;
    memset(audiolib_rolloff_curve_data_list, 0, sizeof(audiolib_rolloff_curve_data_list));
}
