// Original: volition/audio_lib/code/audiolib_parse.cpp
//
// The audio settings table (foley_settings.xtbl, <global_settings>) into Audiolib_settings (SFX.md SX2). The first pass
// reads the general values, distances, occlusion, reverb, field of view weights and the don't-duck and satellite cue
// lists; the second pass (after the foley data is loaded) the near/far cue variants. Compiled in audio_lib_unity.cpp.

#include "volition/audio_lib/code/audiolib_parse.h"

#include "volition/audio_lib/code/audiolib_foley_category.h"
#include "volition/audio_lib/code/audiolib_internal.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

#include <string.h>

// 0x00488C60 (:45)
audiolib_result audiolib_internal_parse_settings(xml_element* root)
{
    xml_element* element = xml_find(root, "general_settings");
    if (element != nullptr)
    {
        xml_table_get_opt_float(Audiolib_settings.overall_volume, element, "Overall_volume_PC");
        xml_table_get_opt_float(Audiolib_settings.speed_of_sound, element, "speed_of_sound");
        xml_table_get_opt_float(Audiolib_settings.default_dist_min, element, "default_dist_min");
        xml_table_get_opt_float(Audiolib_settings.default_dist_max, element, "default_dist_max");
        xml_table_get_opt_float(Audiolib_settings.default_volume, element, "default_volume");
        xml_table_get_opt_int(Audiolib_settings.default_fade_out, element, "default_fade_out");
        xml_table_get_opt_float(Audiolib_settings.doppler_scale, element, "DopplerScale");
        xml_table_get_opt_float(Audiolib_settings.duck_volume, element, "Duck_volume");
        xml_table_get_opt_int(Audiolib_settings.duck_fade_time, element, "Duck_fade_time");
    }
    element = xml_find(root, "low_pass_distance");
    if (element != nullptr)
    {
        // The Xbox 360 factor is the one read on PC.
        xml_table_get_opt_float(Audiolib_settings.low_pass_platform_factor, element, "low_pass_factor_XBOX2");
        xml_table_get_opt_float(Audiolib_settings.distance_lpf_min, element, "distance_lpf_min");
        xml_table_get_opt_float(Audiolib_settings.distance_lpf_max, element, "distance_lpf_max");
        xml_table_get_opt_float(Audiolib_settings.distance_lpf_value_at_max, element, "distance_lpf_value_at_max");
        xml_table_get_opt_float(Audiolib_settings.distance_lpf_min_freq, element, "distance_lpf_min_freq");
    }
    element = xml_find(root, "Occlusion");
    if (element != nullptr)
    {
        xml_table_get_opt_int(Audiolib_settings.max_occlusions, element, "max_occlusions");
        xml_table_get_opt_float(Audiolib_settings.occlusion_lpf_add, element, "occlusion_lpf_add");
        xml_table_get_opt_float(Audiolib_settings.occlusion_vol_adjust, element, "volume_factor");
        xml_table_get_opt_float(Audiolib_settings.occlusion_percentage, element, "max_vol_occlusion");
    }
    element = xml_find(root, "ListenerAngles");
    if (element != nullptr)
    {
        xml_table_get_opt_float(Audiolib_settings.listener_angle_volume_dropoff, element, "FrontAngle");
        // Degrees to radians with a float pi: (double)3.14159274f / 180.0.
        Audiolib_settings.listener_angle_volume_dropoff = (float)((double)Audiolib_settings.listener_angle_volume_dropoff * 0.017453293005625408);
        xml_table_get_opt_float(Audiolib_settings.listener_angle_volume_min, element, "MinimumVolume");
    }
    element = xml_find(root, "Reverb_Distance_Rolloff");
    if (element != nullptr)
    {
        xml_table_get_opt_float(Audiolib_settings.reverb_minimum_distance, element, "Reverb_Minimum_Distance");
        xml_table_get_opt_float(Audiolib_settings.reverb_maximum_distance, element, "Reverb_Maximum_Distance");
        xml_table_get_opt_float(Audiolib_settings.max_reverb_value, element, "Reverb_Maximum_Effect");
    }
    element = xml_find(root, "listener_field_of_view");
    if (element != nullptr)
    {
        xml_table_get_opt_float(Audiolib_settings.minimum_reverb_fov, element, "minimum_reverb_fov");
        xml_table_get_opt_float(Audiolib_settings.minimum_wind_fov, element, "minimum_wind_fov");
        xml_table_get_opt_float(Audiolib_settings.maximum_wind_fov, element, "maximum_wind_fov");
        xml_table_get_opt_float(Audiolib_settings.maximum_wind_lowpass, element, "maximum_wind_lowpass");
        xml_table_get_opt_float(Audiolib_settings.maximum_distance_fov, element, "maximum_distance_fov");
        xml_element* weights = xml_find(element, "FOV_Weights");
        if (weights != nullptr)
        {
            xml_table_get_opt_float(Audiolib_settings.fov_weights.up_weight, weights, "Up");
            xml_table_get_opt_float(Audiolib_settings.fov_weights.hz_weight, weights, "Horizontal");
            xml_table_get_opt_float(Audiolib_settings.fov_weights.dn_weight, weights, "Down");
            xml_element* angle = xml_find(weights, "Up_Angle");
            if (angle != nullptr)
            {
                xml_table_get_opt_float(Audiolib_settings.fov_weights.up_angle_angle, angle, "Angle");
                xml_table_get_opt_float(Audiolib_settings.fov_weights.up_angle_weight, angle, "Weight");
            }
            angle = xml_find(weights, "Down_Angle");
            if (angle != nullptr)
            {
                xml_table_get_opt_float(Audiolib_settings.fov_weights.dn_angle_angle, angle, "Angle");
                xml_table_get_opt_float(Audiolib_settings.fov_weights.dn_angle_weight, angle, "Weight");
            }
        }
    }
    element = xml_find(root, "Surround_Speaker");
    if (element != nullptr && (element = xml_find(element, "XBOX")) != nullptr)
    {
        xml_table_get_opt_float(Audiolib_settings.center_speaker_scalar, element, "Center_Scalar");
        xml_table_get_opt_float(Audiolib_settings.low_freq_speaker_scalar, element, "Low_Freq_Scalar");
    }
    unsigned short count = 0;
    Audiolib_settings.num_min_max_distance_entries = 0;
    element = xml_find(root, "Min_Max_Distances");
    if (element != nullptr && (element = xml_find(element, "Entries")) != nullptr)
    {
        // At most 200 entries.
        for (xml_element* entry = xml_find(element, "Min_Max"); entry != nullptr; entry = xml_find_next(element, entry, "Min_Max"))
        {
            audiolib_min_max_entry& distances = Audiolib_settings.min_max_distance_entries[count];
            xml_table_get_req_float(distances.min, entry, "Min_Distance");
            xml_table_get_req_float(distances.max, entry, "Max_Distance");
            xml_table_get_req_float(distances.min_ext, entry, "Min_Dist_Ext");
            xml_table_get_req_float(distances.max_ext, entry, "Max_Dist_Ext");
            if (++count > 199)
            {
                break;
            }
        }
    }
    Audiolib_settings.num_min_max_distance_entries = count;
    // Original: no bound on the cue lists (100 and 30 slots).
    memset(Audiolib_settings.dont_duck_foley_crcs, 0, sizeof(Audiolib_settings.dont_duck_foley_crcs));
    Audiolib_settings.num_dont_duck_foleys = 0;
    element = xml_find(root, "DoNotDuck");
    if (element != nullptr && (element = xml_find(element, "DoNotDuckTheseFoleys")) != nullptr)
    {
        Audiolib_settings.num_dont_duck_foleys = xml_count(element, "Foley");
        unsigned int* crc = Audiolib_settings.dont_duck_foley_crcs;
        for (xml_element* foley = xml_find(element, "Foley"); foley != nullptr; foley = xml_find_next(element, foley, "Foley"))
        {
            *crc++ = crc_str(foley->text, 0);
        }
    }
    memset(Audiolib_settings.satellite_foley_crcs, 0, sizeof(Audiolib_settings.satellite_foley_crcs));
    Audiolib_settings.num_satellite_foleys = 0;
    element = xml_find(root, "SatelliteMode");
    if (element != nullptr)
    {
        xml_table_get_req_float(Audiolib_settings.satellite_play_distance, element, "SatelliteDistance");
        element = xml_find(element, "SatelliteModeFoleys");
        if (element != nullptr)
        {
            Audiolib_settings.num_satellite_foleys = xml_count(element, "Foley");
            unsigned int* crc = Audiolib_settings.satellite_foley_crcs;
            for (xml_element* foley = xml_find(element, "Foley"); foley != nullptr; foley = xml_find_next(element, foley, "Foley"))
            {
                *crc++ = crc_str(foley->text, 0);
            }
        }
    }
    return AUDIOLIB_ERR_NONE;
}

// 0x004891E0 (:250)
audiolib_result audiolib_internal_parse_near_far_settings(xml_element* root)
{
    int count = 0;
    Audiolib_settings.num_near_far_entries = 0;
    xml_element* element = xml_find(root, "FarVariants");
    xml_element* foleys = element != nullptr ? xml_find(element, "Foleys") : nullptr;
    if (foleys != nullptr)
    {
        char name[64];
        for (xml_element* variants = xml_find(foleys, "NearFarVariants"); variants != nullptr; variants = xml_find_next(foleys, variants, "NearFarVariants"))
        {
            xml_table_get_req_string(name, sizeof(name), variants, "NearFoley");
            const unsigned int near_crc = crc_str(name, 0);
            xml_table_get_req_string(name, sizeof(name), variants, "FarFoley");
            const unsigned int far_crc = crc_str(name, 0);
            audiolib_foley_data* near_foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(near_crc, nullptr));
            audiolib_foley_data* far_foley = audiolib_get_foley_data_pointer(audiolib_foley_hash_table_get(far_crc, nullptr));
            // Original: an entry with an unknown cue still takes a slot (left as it was).
            if (near_foley != nullptr && far_foley != nullptr)
            {
                // Caller cue ids (index | address << 16, audiolib_foley_category.cpp).
                Audiolib_settings.near_far_entries[count].near_cue = (int)((unsigned int)(near_foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)near_foley << 16);
                Audiolib_settings.near_far_entries[count].far_cue = (int)((unsigned int)(far_foley - Audiolib_foley_list.foley) & 0xFFFF) | (int)((unsigned int)(uintptr_t)far_foley << 16);
                near_foley->near_far_variant = true;
            }
            // At most 256 entries.
            if (++count > 255)
            {
                break;
            }
        }
    }
    Audiolib_settings.num_near_far_entries = count;
    return AUDIOLIB_ERR_NONE;
}

// 0x00489350 (:297)
audiolib_result audiolib_parse_settings_xtbl(const char* filename, bool first_pass)
{
    if (filename == nullptr)
    {
        return AUDIOLIB_ERR_BAD_PARAM;
    }
    audiolib_result result = AUDIOLIB_ERR_NONE;
    char path[256];
    if (Audiolib_state.packfile_build)
    {
        strncpy(path, filename, sizeof(path));
    }
    else
    {
        strncpy(path, Audiolib_state.root_dir, sizeof(path));
        strncat(path, Audiolib_state.table_dir, sizeof(path));
        strncat(path, filename, sizeof(path));
    }
    xml_element* root = xml_table_parse(path);
    if (root != nullptr)
    {
        xml_element* settings = xml_table_find_opt(root, "global_settings");
        if (settings != nullptr)
        {
            result = first_pass ? audiolib_internal_parse_settings(settings) : audiolib_internal_parse_near_far_settings(settings);
        }
        xml_table_close(root);
    }
    return result;
}
