// Original: volition/rfg/code/gamewide/time_of_day_shared.cpp
//
// Partial: the time-of-day data and its loading. tod_interpolated_var<T> keeps a value for each half hour (48 keys),
// fills the unkeyed half hours by interpolating between the keyed ones (wrapping around midnight) and blends the two
// keys around a time. The district parameters (.dtodx) and the global parameters (.gtodx, the orbitals) are read from
// xml. Also the sun direction and orbital position at a time, and the HDR/LDR color conversion. Not done: the xml
// writers (tod_write_xml_helper, the save_* functions), which only the tools use.

#include "volition/rfg/code/gamewide/time_of_day_shared.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/math/vector4.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/xml_util.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <string.h>

static const int TOD_NUM_INTERVALS = 48; // one key per half hour
static const float TOD_DEG_TO_RAD = 0.017453f; // the original's degree factor (a float, not exactly pi / 180)
static const float TOD_PI = 3.1415927f;

// ---- globals
bool Tod_hdr_enabled = true; // 0x0165E0AC
float TOD_intensity_scale = 1.0f; // 0x0165E0B4
static float Hdr_to_ldr_range_min; // 0x02533948
static float Hdr_to_ldr_range_max = 32.0f; // 0x0165E0B0
static float light_offset = 0.53f; // 0x0165EB68, the depth of the sun's arc (z component at noon)
static float daytime_start = 7.5f; // 0x0165EB6C, the hour the sun rises in the east

// ---- tod_interpolated_var<T> (.h)
// <float> 0x007C9EC0, <vector4> 0x007C9CA0 (.h:72)
template<typename T>
void tod_interpolated_var<T>::interpolate_value(T& interpolated, const T& val0, const T& val1, float percent) const
{
    if (percent - 0.0f < 0.0f)
    {
        percent = 0.0f;
    }
    if (0.0f <= percent - 1.0f)
    {
        percent = 1.0f;
    }
    interpolated = val0 * (1.0f - percent) + val1 * percent;
}

// <float> 0x007C9F40, <vector4> 0x007C9E20 (.h:79)
template<typename T>
void tod_interpolated_var<T>::get_value_set(bool** keys, T** values)
{
    *keys = good_keyed;
    *values = good_values;
}

// <float> 0x007C9F60, <vector4> 0x007C9E40 (.h:93): the first keyed interval from start_val on, wrapping around.
template<typename T>
int tod_interpolated_var<T>::find_next_keyed_value(int start_val)
{
    for (int i = start_val; i < TOD_NUM_INTERVALS; i++)
    {
        if (good_keyed[i])
        {
            return i;
        }
    }
    for (int i = 0; i < start_val; i++)
    {
        if (good_keyed[i])
        {
            return i;
        }
    }
    return -1;
}

// <float> 0x007C9FA0, <vector4> 0x007C9E80 (.h:117): the last keyed interval at or before start_val, wrapping around.
template<typename T>
int tod_interpolated_var<T>::find_prev_keyed_value(int start_val)
{
    for (int i = start_val; i >= 0; i--)
    {
        if (good_keyed[i])
        {
            return i;
        }
    }
    for (int i = TOD_NUM_INTERVALS - 1; i > start_val; i--)
    {
        if (good_keyed[i])
        {
            return i;
        }
    }
    return -1;
}

// <float> 0x007D6C30, <vector4> 0x007D6680 (.h:141)
template<typename T>
tod_interpolated_var<T>::tod_interpolated_var()
{
    memset(good_keyed, 0, sizeof(good_keyed));
}

// <float> 0x007C96A0, <vector4> 0x007C9660 (.h:147)
template<typename T>
void tod_interpolated_var<T>::init()
{
    memset(good_keyed, 0, sizeof(good_keyed));
}

// <float> 0x007C96C0, <vector4> 0x007C9680 (.h:185)
template<typename T>
void tod_interpolated_var<T>::set_default_value(const T& def_val)
{
    default_value = def_val;
}

// <float> 0x007D6C50, <vector4> 0x007D66A0 (.h:201): the blend of the half-hour values below and above time. The upper
// index wraps to 0 at midnight; the lower one is floor(time * 2), which stays below 48 for times below 24.
template<typename T>
T tod_interpolated_var<T>::get_value_at_time(float time) const
{
    const float half_hours = time * 2.0f;
    const int prev_index = (int)floorf(half_hours);
    const int next_index = (int)ceilf(half_hours) % TOD_NUM_INTERVALS;
    float percent = fmodf(half_hours, 1.0f);
    if (percent - 0.0f < 0.0f)
    {
        percent = 0.0f;
    }
    if (0.0f <= percent - 1.0f)
    {
        percent = 1.0f;
    }
    return good_values[prev_index] * (1.0f - percent) + good_values[next_index] * percent;
}

// <float> 0x007D6D90, <vector4> 0x007D6910 (.h:216): every unkeyed interval gets the blend of the keyed intervals
// around it (the only key when there is one, the default when there is none).
template<typename T>
void tod_interpolated_var<T>::build_unkeyed_values()
{
    int prev_key = find_prev_keyed_value(0);
    int next_key = find_next_keyed_value(1);
    for (int i = 0; i < TOD_NUM_INTERVALS; i++)
    {
        if (good_keyed[i])
        {
            prev_key = i;
            next_key = find_next_keyed_value(i + 1);
            continue;
        }
        if (prev_key == -1 && next_key == -1)
        {
            good_values[i] = default_value;
            continue;
        }
        if (prev_key != -1 && prev_key == next_key)
        {
            good_values[i] = good_values[next_key];
            continue;
        }
        int span;
        int offset;
        if (next_key < prev_key)
        {
            // Across midnight. The original's span is 47 intervals short of a day, not 48. Kept as in the binary.
            span = next_key - prev_key + 47;
            offset = prev_key < i ? i - prev_key : i + span - next_key;
        }
        else
        {
            span = next_key - prev_key;
            offset = i - prev_key;
        }
        float percent = (float)offset / (float)span;
        if (percent - 0.0f < 0.0f)
        {
            percent = 0.0f;
        }
        if (0.0f <= percent - 1.0f)
        {
            percent = 1.0f;
        }
        good_values[i] = good_values[prev_key] * (1.0f - percent) + good_values[next_key] * percent;
    }
}

static bool read_tod_key_value(xml_element* key_el, float& value)
{
    return read_xml_float(key_el, "value", value, 0.0f);
}

static bool read_tod_key_value(xml_element* key_el, vector4& value)
{
    return read_xml_vector4(key_el, "value", value, ZERO_VECTOR4);
}

// <float> 0x007CA4A0, <vector4> 0x007CA360 (.h:264): the <key> children of root, each a <time> (half-hour index) and a
// <value>.
template<typename T>
bool tod_interpolated_var<T>::load(xml_element* root)
{
    memset(good_keyed, 0, sizeof(good_keyed));
    for (xml_element* key_el = xml_find(root, "key"); key_el != nullptr; key_el = xml_find_next(root, key_el, "key"))
    {
        int index;
        read_xml_int(key_el, "time", index, 0);
        good_keyed[index] = true;
        read_tod_key_value(key_el, good_values[index]);
    }
    return true;
}

#pragma warning(disable : 4661) // the const get_value_set overload has no body: nothing calls it
#pragma warning(push)
template class tod_interpolated_var<float>;
template class tod_interpolated_var<vector4>;
#pragma warning(pop)

// The load sequence the original repeats for every variable (inlined): clear the keys, read them if the element
// exists, then fill the unkeyed intervals (with the default when nothing is keyed).
template<typename T>
static void load_tod_var(xml_element* root, const char* tag, tod_interpolated_var<T>& var)
{
    xml_element* var_el = xml_find(root, tag);
    var.init();
    if (var_el != nullptr)
    {
        var.load(var_el);
    }
    var.build_unkeyed_values();
}

// ---- tod_orbital_object
// 0x007E8F60 (.h:324)
tod_orbital_object::tod_orbital_object()
{
    init();
}

// 0x007D8900 (.h:330)
void tod_orbital_object::init()
{
    plane_rot_x = 0.0f;
    plane_rot_y = 0.0f;
    distance_off_plane = 0.0f;
    orbit_time_offset = 0.0f;
    memcpy(map_name, "missing", 8);
    tint.set_default_value(vector4(1.0f, 1.0f, 1.0f, 1.0f));
    alpha_blend = false;
    scale.set_default_value(10.0f);
    opacity.set_default_value(1.0f);
    tint.init();
    scale.init();
    opacity.init();
    tint.build_unkeyed_values();
    scale.build_unkeyed_values();
    opacity.build_unkeyed_values();
}

// ---- tod_global_parameters
// 0x007EDA70 (.h:361)
tod_global_parameters::tod_global_parameters()
{
    modified = false;
    horizon_mountain_enabled = true;
    fog_camera_follow = false;
}

// 0x007D9AC0
tod_global_parameters::~tod_global_parameters()
{
}

// 0x007BFF60 (.h:366)
void tod_global_parameters::set_defaults()
{
    horizon_mountain_enabled = true;
    fog_camera_follow = false;
}

// ---- tod_district_parameters
// 0x007D89F0 (.h:465)
tod_district_parameters::tod_district_parameters()
{
    set_defaults();
    modified = false;
    strcpy(filename, "NONE");
}

// 0x007CCD50 (.h:626): the defaults of the unkeyed variables (magic numbers: the original tuning values).
void tod_district_parameters::set_defaults()
{
    for (int i = 0; i < 4; i++)
    {
        west_hemisphere[i].set_default_value(vector4(0.858f, 0.819f, 0.658f, 3.0f));
        east_hemisphere[i].set_default_value(vector4(0.858f, 0.819f, 0.658f, 3.0f));
    }
    zenith.set_default_value(vector4(0.858f, 0.819f, 0.658f, 3.0f));
    tod_light_color.set_default_value(vector4(1.0f, 0.94f, 0.78f, 7.0f));
    ambient_color.set_default_value(vector4(0.54f, 0.51f, 0.427f, 2.0f));
    back_ambient_color.set_default_value(vector4(0.23f, 0.1f, 0.54f, 2.0f));
    fog_color.set_default_value(vector4(1.0f, 0.75f, 0.44f, 1.0f));
    fog_atmosphere_scale.set_default_value(0.001f);
    fog_ground.set_default_value(0.0f);
    fog_density.set_default_value(0.007f);
    fog_density_offset.set_default_value(0.0f);
    bloom_slope_B.set_default_value(0.1f);
    luminance_max.set_default_value(4.0f);
    luminance_min.set_default_value(0.001f);
    eye_adaption_base.set_default_value(0.2f);
    particle_ambient_color.set_default_value(vector4(1.0f, 1.0f, 1.0f, 1.0f));
    eye_adaption_amount.set_default_value(0.3f);
    brightpass_offset.set_default_value(6.0f);
    particle_tod_color.set_default_value(vector4(1.0f, 1.0f, 1.0f, 1.0f));
    tonemap_lum_offset.set_default_value(10.0f);
    fog_matte_offset.set_default_value(10.0f);
    color_correct_tint.set_default_value(vector4(1.0f, 1.0f, 1.0f, 1.0f));
    ldr_max.set_default_value(32.0f);
    clouds_front_light_color.set_default_value(vector4(1.0f, 1.0f, 1.0f, 2.0f));
    bloom_exposure.set_default_value(0.5f);
    bloom_theta.set_default_value(1.15f);
    clouds_rear_light_color.set_default_value(vector4(1.0f, 1.0f, 1.0f, 2.0f));
    bloom_slope_A.set_default_value(-0.01f);
    iris_rate.set_default_value(3.0f);
    brightpass_threshold.set_default_value(3.0f);
    tonemap_lum_range.set_default_value(3.0f);
    fog_camera_max.set_default_value(3.0f);
    ldr_min.set_default_value(0.0f);
    luminance_mask_max.set_default_value(1.0f);
    eye_fade_min.set_default_value(1.0f);
    eye_fade_max.set_default_value(1.0f);
    bloom_amount.set_default_value(1.0f);
    fog_camera_min.set_default_value(-1000.0f);
    fog_skybox_offset.set_default_value(100.0f);
    fog_skybox_scale.set_default_value(100.0f);
    fog_clouds_offset.set_default_value(100.0f);
    fog_clouds_scale.set_default_value(100.0f);
    fog_matte_scale.set_default_value(100.0f);
    color_correct_saturation.set_default_value(1.0f);
    clouds_horizon_front_light_color.set_default_value(vector4(1.0f, 1.0f, 1.0f, 2.0f));
    horizon_layer2_strength.set_default_value(0.0f);
    horizon_layer3_strength.set_default_value(0.0f);
    overhead_layer2_strength.set_default_value(0.0f);
    overhead_layer3_strength.set_default_value(0.0f);
    horizon_mountain_color_front.set_default_value(vector4(1.0f, 0.75f, 0.44f, 1.0f));
    clouds_horizon_rear_light_color.set_default_value(vector4(1.0f, 1.0f, 1.0f, 2.0f));
    horizon_mountain_color_back.set_default_value(vector4(1.0f, 0.75f, 0.44f, 1.0f));
    horizon_mountain_fog_color.set_default_value(vector4(1.0f, 0.75f, 0.44f, 1.0f));
    horizon_layer0_strength.set_default_value(1.0f);
    horizon_layer1_strength.set_default_value(1.0f);
    overhead_layer0_strength.set_default_value(1.0f);
    overhead_layer1_strength.set_default_value(1.0f);
    horizon_mountain_fog_density.set_default_value(1.0f);
    cloud_backlight_strength.set_default_value(100.0f);
    cloud_backlight_power.set_default_value(2.0f);
    cloud_layer01_speed.set_default_value(0.125f);
    cloud_layer23_speed.set_default_value(0.125f);
}

// 0x007D8E70 (.h:718)
void tod_district_parameters::build_all_unkeyed_values()
{
    for (int i = 0; i < 4; i++)
    {
        west_hemisphere[i].build_unkeyed_values();
        east_hemisphere[i].build_unkeyed_values();
    }
    zenith.build_unkeyed_values();
    tod_light_color.build_unkeyed_values();
    ambient_color.build_unkeyed_values();
    back_ambient_color.build_unkeyed_values();
    fog_color.build_unkeyed_values();
    particle_ambient_color.build_unkeyed_values();
    particle_tod_color.build_unkeyed_values();
    fog_ground.build_unkeyed_values();
    fog_atmosphere_scale.build_unkeyed_values();
    fog_density.build_unkeyed_values();
    fog_density_offset.build_unkeyed_values();
    ldr_min.build_unkeyed_values();
    ldr_max.build_unkeyed_values();
    bloom_exposure.build_unkeyed_values();
    iris_rate.build_unkeyed_values();
    luminance_max.build_unkeyed_values();
    luminance_min.build_unkeyed_values();
    luminance_mask_max.build_unkeyed_values();
    eye_adaption_base.build_unkeyed_values();
    eye_adaption_amount.build_unkeyed_values();
    eye_fade_min.build_unkeyed_values();
    eye_fade_max.build_unkeyed_values();
    brightpass_threshold.build_unkeyed_values();
    brightpass_offset.build_unkeyed_values();
    bloom_amount.build_unkeyed_values();
    bloom_theta.build_unkeyed_values();
    bloom_slope_A.build_unkeyed_values();
    bloom_slope_B.build_unkeyed_values();
    tonemap_lum_range.build_unkeyed_values();
    tonemap_lum_offset.build_unkeyed_values();
    color_correct_saturation.build_unkeyed_values();
    color_correct_tint.build_unkeyed_values();
    clouds_front_light_color.build_unkeyed_values();
    clouds_rear_light_color.build_unkeyed_values();
    clouds_horizon_front_light_color.build_unkeyed_values();
    clouds_horizon_rear_light_color.build_unkeyed_values();
    horizon_layer0_strength.build_unkeyed_values();
    horizon_layer1_strength.build_unkeyed_values();
    horizon_layer2_strength.build_unkeyed_values();
    horizon_layer3_strength.build_unkeyed_values();
    overhead_layer0_strength.build_unkeyed_values();
    overhead_layer1_strength.build_unkeyed_values();
    overhead_layer2_strength.build_unkeyed_values();
    overhead_layer3_strength.build_unkeyed_values();
    horizon_mountain_color_front.build_unkeyed_values();
    horizon_mountain_color_back.build_unkeyed_values();
    horizon_mountain_fog_color.build_unkeyed_values();
    horizon_mountain_fog_density.build_unkeyed_values();
    cloud_backlight_power.build_unkeyed_values();
    cloud_backlight_strength.build_unkeyed_values();
    cloud_layer01_speed.build_unkeyed_values();
    cloud_layer23_speed.build_unkeyed_values();
    fog_camera_min.build_unkeyed_values();
    fog_camera_max.build_unkeyed_values();
    fog_skybox_offset.build_unkeyed_values();
    fog_skybox_scale.build_unkeyed_values();
    fog_clouds_offset.build_unkeyed_values();
    fog_clouds_scale.build_unkeyed_values();
    fog_matte_offset.build_unkeyed_values();
    fog_matte_scale.build_unkeyed_values();
}

// ---- functions
// 0x007C8B70 (:92)
bool tod_read_xml_helper(xml_element* parent, const char* tag, vector& val)
{
    return read_xml_vector(parent, tag, val, ZERO_VECTOR);
}

// 0x007C8B90 (:97)
bool tod_read_xml_helper(xml_element* parent, const char* tag, float& val)
{
    return read_xml_float(parent, tag, val, 0.0f);
}

// 0x007C8BC0 (:102)
bool tod_read_xml_helper(xml_element* parent, const char* tag, vector4& val)
{
    return read_xml_vector4(parent, tag, val, ZERO_VECTOR4);
}

// 0x007E65B0 (:117, LTCG): the district file's variables, in the file's tag names (some differ from the members).
static bool load_tod_params_for_weather(tod_district_parameters& params, xml_element* root)
{
    load_tod_var(root, "east0", params.east_hemisphere[0]);
    load_tod_var(root, "east1", params.east_hemisphere[1]);
    load_tod_var(root, "east2", params.east_hemisphere[2]);
    load_tod_var(root, "east3", params.east_hemisphere[3]);
    load_tod_var(root, "west0", params.west_hemisphere[0]);
    load_tod_var(root, "west1", params.west_hemisphere[1]);
    load_tod_var(root, "west2", params.west_hemisphere[2]);
    load_tod_var(root, "west3", params.west_hemisphere[3]);
    load_tod_var(root, "zenith", params.zenith);
    load_tod_var(root, "fog_ground", params.fog_ground);
    load_tod_var(root, "fog_atmosphere_scale", params.fog_atmosphere_scale);
    load_tod_var(root, "fog_density_new", params.fog_density);
    load_tod_var(root, "fog_density_offset", params.fog_density_offset);
    load_tod_var(root, "fog_color", params.fog_color);
    load_tod_var(root, "ambient_color", params.ambient_color);
    load_tod_var(root, "back_ambient_color", params.back_ambient_color);
    load_tod_var(root, "tod_light_color", params.tod_light_color);
    load_tod_var(root, "particle_ambient_color", params.particle_ambient_color);
    load_tod_var(root, "particle_tod_light_color", params.particle_tod_color);
    load_tod_var(root, "ldr_min", params.ldr_min);
    load_tod_var(root, "ldr_max", params.ldr_max);
    load_tod_var(root, "bloom_exposure", params.bloom_exposure);
    load_tod_var(root, "iris_rate", params.iris_rate);
    load_tod_var(root, "luminance_max", params.luminance_max);
    load_tod_var(root, "luminance_min", params.luminance_min);
    load_tod_var(root, "luminance_mask_max", params.luminance_mask_max);
    load_tod_var(root, "eye_adaption_base", params.eye_adaption_base);
    load_tod_var(root, "eye_adaption_amount", params.eye_adaption_amount);
    load_tod_var(root, "eye_fade_min", params.eye_fade_min);
    load_tod_var(root, "eye_fade_max", params.eye_fade_max);
    load_tod_var(root, "brightpass_threshold_new", params.brightpass_threshold);
    load_tod_var(root, "brightpass_offset_new", params.brightpass_offset);
    load_tod_var(root, "bloom_amount", params.bloom_amount);
    load_tod_var(root, "bloom_theta", params.bloom_theta);
    load_tod_var(root, "bloom_slope_A", params.bloom_slope_A);
    load_tod_var(root, "bloom_slope_B", params.bloom_slope_B);
    load_tod_var(root, "tonemap_lum_range", params.tonemap_lum_range);
    load_tod_var(root, "tonemap_lum_offset", params.tonemap_lum_offset);
    load_tod_var(root, "fog_camera_min", params.fog_camera_min);
    load_tod_var(root, "fog_camera_max", params.fog_camera_max);
    load_tod_var(root, "fog_skybox_offset", params.fog_skybox_offset);
    load_tod_var(root, "fog_skybox_scale", params.fog_skybox_scale);
    load_tod_var(root, "fog_clouds_offset", params.fog_clouds_offset);
    load_tod_var(root, "fog_clouds_scale", params.fog_clouds_scale);
    load_tod_var(root, "fog_matte_offset", params.fog_matte_offset);
    load_tod_var(root, "fog_matte_scale", params.fog_matte_scale);
    load_tod_var(root, "color_correct_saturation", params.color_correct_saturation);
    load_tod_var(root, "color_correct_tint", params.color_correct_tint);
    load_tod_var(root, "clouds_front_light_color", params.clouds_front_light_color);
    load_tod_var(root, "clouds_rear_light_color", params.clouds_rear_light_color);
    load_tod_var(root, "clouds_horizon_front_light_color", params.clouds_horizon_front_light_color);
    load_tod_var(root, "clouds_horizon_rear_light_color", params.clouds_horizon_rear_light_color);
    load_tod_var(root, "horizon_mountain_color", params.horizon_mountain_color_front);
    load_tod_var(root, "horizon_mountain_color_back", params.horizon_mountain_color_back);
    load_tod_var(root, "horizon_mountain_fog_color", params.horizon_mountain_fog_color);
    load_tod_var(root, "horizon_mountain_fog_density", params.horizon_mountain_fog_density);
    load_tod_var(root, "horizon_layer0_strength", params.horizon_layer0_strength);
    load_tod_var(root, "horizon_layer1_strength", params.horizon_layer1_strength);
    load_tod_var(root, "horizon_layer2_strength", params.horizon_layer2_strength);
    load_tod_var(root, "horizon_layer3_strength", params.horizon_layer3_strength);
    load_tod_var(root, "overhead_layer0_strength", params.overhead_layer0_strength);
    load_tod_var(root, "overhead_layer1_strength", params.overhead_layer1_strength);
    load_tod_var(root, "overhead_layer2_strength", params.overhead_layer2_strength);
    load_tod_var(root, "overhead_layer3_strength", params.overhead_layer3_strength);
    load_tod_var(root, "cloud_backlight_strength", params.cloud_backlight_strength);
    load_tod_var(root, "cloud_backlight_power", params.cloud_backlight_power);
    load_tod_var(root, "cloud_layer01_speed", params.cloud_layer01_speed);
    load_tod_var(root, "cloud_layer23_speed", params.cloud_layer23_speed);
    return true;
}

// 0x007EADB0 (:211, LTCG): the sun, moon and planets of the sky (at most 15).
static bool load_tod_orbital_objects(tod_global_parameters& params, xml_element* root)
{
    params.orbital_objects.clear();
    xml_element* orbitals_el = xml_find(root, "orbitals");
    if (orbitals_el == nullptr)
    {
        return true;
    }
    for (xml_element* orbital_el = xml_find(orbitals_el, "orbital"); orbital_el != nullptr && params.orbital_objects.size() < 15; orbital_el = xml_find_next(orbitals_el, orbital_el, "orbital"))
    {
        tod_orbital_object* orbital = params.orbital_objects.add_in_place();
        orbital->init();
        orbital->plane_rot_x = xml_get_float(xml_find(orbital_el, "plane_rot_x"), 0.0f);
        orbital->plane_rot_y = xml_get_float(xml_find(orbital_el, "plane_rot_y"), 0.0f);
        orbital->distance_off_plane = xml_get_float(xml_find(orbital_el, "distance_off_plane"), 0.0f);
        orbital->orbit_time_offset = xml_get_float(xml_find(orbital_el, "orbit_time_offset"), 0.0f);
        orbital->alpha_blend = xml_get_bool(xml_find(orbital_el, "alpha_blend"), false);
        // strncpy with the full buffer sizes: a name of the maximum length is not terminated (kept as in the binary).
        strncpy(orbital->map_name, xml_get_string(xml_find(orbital_el, "map_name"), nullptr), sizeof(orbital->map_name));
        strncpy(orbital->name, xml_get_string(xml_find(orbital_el, "name"), nullptr), sizeof(orbital->name));
        const char* effect_name = xml_get_string(xml_find(orbital_el, "effect_name"), nullptr);
        if (effect_name == nullptr)
        {
            orbital->m_effect_info_handle = 0xFFFFFFFF;
        }
        else
        {
            // Copied with the name's length limit (30), not effect_name's (64).
            strncpy(orbital->effect_name, effect_name, sizeof(orbital->name));
            orbital->m_effect_info_handle = effects_get_handle(effect_name);
        }
        orbital->m_effect_handle = 0xFFFFFFFF;
        load_tod_var(orbital_el, "scale", orbital->scale);
        load_tod_var(orbital_el, "tint", orbital->tint);
        load_tod_var(orbital_el, "opacity", orbital->opacity);
    }
    return true;
}

// 0x007E7560 (:283)
bool tod_shared_load_district_parameters(tod_district_parameters& params, const char* tod_filename, mempool_base* parse_mempool)
{
    if (parse_mempool == nullptr)
    {
        keen::debug::nativePrintArgs("tod_shared_load_district_parameters: failed parse_mempool == nullptr\n");
        return false;
    }
    mempool_mark_scope scope(parse_mempool);
    xml_element* root = xml_parse(tod_filename, parse_mempool);
    if (root == nullptr)
    {
        keen::debug::nativePrintArgs("tod_shared_load_district_parameters: root == nullptr. tod_filename: %s\n", tod_filename);
        params.build_all_unkeyed_values();
        return false;
    }
    if (!load_tod_params_for_weather(params, root))
    {
        keen::debug::nativePrintArgs("tod_shared_load_district_parameters: load_tod_params_for_weather() failed\n");
        return false;
    }
    char name[64];
    cf_drop_path(name, sizeof(name), tod_filename);
    strcpy(params.filename, name);
    return true;
}

// 0x007EB060 (:321)
bool tod_shared_load_global_parameters(tod_global_parameters& params, const char* tod_filename, mempool_base* parse_mempool)
{
    if (parse_mempool == nullptr)
    {
        return false;
    }
    mempool_mark_scope scope(parse_mempool);
    xml_element* root = xml_parse(tod_filename, parse_mempool);
    if (root == nullptr)
    {
        return false;
    }
    if (!load_tod_orbital_objects(params, root))
    {
        return false;
    }
    params.horizon_mountain_enabled = xml_get_bool(xml_find(root, "horizon_mountain_enabled"), true);
    params.fog_camera_follow = xml_get_bool(xml_find(root, "fog_camera_follow"), false);
    return true;
}

// 0x007C8BE0 (:540): the direction to the sun. It rises at daytime_start and crosses the sky in a half circle over
// 12 hours (x from -1 to 1, z bulging by light_offset); its height never drops below min_angle_to_horizon, so the
// light keeps shining from above at night. rot_around_y turns the whole path.
vector tod_shared_calcaulate_light_direction_at_time(float time, float rot_around_y, float min_angle_to_horizon)
{
    const float min_height = sinf(min_angle_to_horizon * TOD_DEG_TO_RAD);
    float hours = time - daytime_start;
    if (hours < 0.0f)
    {
        hours = hours + 24.0f;
    }
    const float angle = (hours / 12.0f - 1.0f) * TOD_PI;
    const float c = cosf(angle);
    const float s = sinf(angle);
    float height = fabsf(s);
    if (height - min_height < 0.0f)
    {
        height = min_height;
    }
    if (0.0f <= height - 1.0f)
    {
        height = 1.0f;
    }
    vector dir;
    dir.x = -c;
    dir.y = height;
    dir.z = light_offset * s;
    dir.normalize_safe();
    dir.rotate_around_y_axis(rot_around_y * TOD_DEG_TO_RAD);
    return dir;
}

// 0x007C8D80 (:579)
vector tod_shared_calcaulate_light_position_at_time(float time, float light_distance, float rot_around_y, float min_angle_to_horizon)
{
    const vector dir = tod_shared_calcaulate_light_direction_at_time(time, rot_around_y, min_angle_to_horizon);
    return vector(dir.x * light_distance, dir.y * light_distance, dir.z * light_distance);
}

// 0x007C8E20 (:594): val wrapped into [min_val, max_val). A value below min_val is mirrored ((max - min) - (val - min))
// rather than shifted up. Kept as in the binary.
float wrap_around_range(float val, float min_val, float max_val)
{
    const float range = max_val - min_val;
    float offset = val - min_val;
    if (offset < 0.0f)
    {
        offset = range - offset;
    }
    return fmodf(offset, range) + min_val;
}

// 0x007C8EB0 (:611): a point on the orbital's unit circle. The circle lies distance_off_plane above the plane (so its
// radius is sin(acos(distance))), one turn per day from orbit_time_offset, then tilted by the plane rotations.
void tod_shared_calculate_orbital_position(vector& pos, const tod_orbital_object& orbital_obj, float time)
{
    const float radius = sinf(fl_acos(orbital_obj.distance_off_plane));
    const float day_time = wrap_around_range(orbital_obj.orbit_time_offset + time, 0.0f, 24.0f);
    const float angle = day_time / 24.0f * 2.0f * TOD_PI;
    vector local;
    local.x = cosf(angle) * radius;
    local.y = orbital_obj.distance_off_plane;
    local.z = sinf(angle) * radius;
    local.rotate_around_x_axis(orbital_obj.plane_rot_x * TOD_DEG_TO_RAD);
    local.rotate_around_y_axis(orbital_obj.plane_rot_y * TOD_DEG_TO_RAD);
    pos = local;
}

// 0x007D62C0 (:630): rgb times the intensity in w (with HDR), or times w mapped from the LDR range to 0..1. The alpha is
// the intensity factor.
void hdr_color_from_vector4(rl_color_float& hdr_color, const vector4& source_color)
{
    float intensity;
    if (Tod_hdr_enabled)
    {
        intensity = source_color.w * TOD_intensity_scale;
    }
    else
    {
        intensity = (source_color.w - Hdr_to_ldr_range_min) / Hdr_to_ldr_range_max;
        if (intensity - 0.0f < 0.0f)
        {
            intensity = 0.0f;
        }
        if (!(intensity - 1.0f < 0.0f))
        {
            intensity = 1.0f;
        }
    }
    hdr_color.red = source_color.x * intensity;
    hdr_color.green = source_color.y * intensity;
    hdr_color.blue = source_color.z * intensity;
    hdr_color.alpha = intensity;
}

// 0x007C9020 (:647)
void ldr_color_from_vector4(rl_color_float& ldr_color, const vector4& source_color)
{
    float intensity = (source_color.w - Hdr_to_ldr_range_min) / Hdr_to_ldr_range_max;
    if (intensity - 0.0f < 0.0f)
    {
        intensity = 0.0f;
    }
    if (!(intensity - 1.0f < 0.0f))
    {
        intensity = 1.0f;
    }
    ldr_color.red = source_color.x * intensity;
    ldr_color.green = source_color.y * intensity;
    ldr_color.blue = source_color.z * intensity;
    ldr_color.alpha = intensity;
}

// 0x007C9120 (:663)
void set_hdr_to_ldr_conversion_range(float min_intensity, float max_intensity)
{
    Hdr_to_ldr_range_min = min_intensity;
    Hdr_to_ldr_range_max = max_intensity;
}

// 0x007C9140 (:669)
void tod_set_hdr_enabled(bool enabled)
{
    Tod_hdr_enabled = enabled;
}

// 0x007C9150 (:674)
bool tod_get_hdr_enabled()
{
    return Tod_hdr_enabled;
}
