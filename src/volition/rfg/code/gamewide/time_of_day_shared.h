// Original: volition/rfg/code/gamewide/time_of_day_shared.h
// Implementation: volition/rfg/code/gamewide/time_of_day_shared.cpp
//
// Time-of-day data shared with the tools: tod_interpolated_var<T>, a value keyed at 48 intervals of the day and
// interpolated between keys, and the global and per-district parameter sets (sky, sun/moon orbitals, light colors, fog,
// HDR, color correction) read from and written to xml. tod_interpolated_var<T> is written by hand; the addresses are of
// the <float> instantiation (<vector4> is the other one).
#pragma once

#include "volition/vlib/code/math/vector4.h"
#include "volition/vlib/code/util/varray.h"

struct _iobuf;
class mempool_base;
class rl_color_float;
class vector;
struct xml_element;

// size 0xF4 (<float>), 0x340 (<vector4>)
template<typename T>
class tod_interpolated_var
{
    void interpolate_value(T& interpolated, const T& val0, const T& val1, float percent) const; // 0x007C9EC0 (.h:72)
    void get_value_set(const bool**, const T**) const;
    void get_value_set(bool** keys, T** values); // 0x007C9F40 (.h:79)
    int find_next_keyed_value(int start_val); // 0x007C9F60 (.h:93)
    int find_prev_keyed_value(int start_val); // 0x007C9FA0 (.h:117)
public:
    tod_interpolated_var(); // 0x007D6C30 (.h:141)
    void init(); // 0x007C96A0 (.h:147)
    bool is_keyed_at_interval(int) const;
    void set_is_keyed_at_interval(int, bool);
    const T& get_value_at_interval(int) const;
    void set_default_value(const T& def_val); // 0x007C96C0 (.h:185)
    void set_value_at_interval(int, const T&);
    T get_value_at_time(float time) const; // 0x007D6C50 (.h:201)
    void build_unkeyed_values(); // 0x007D6D90 (.h:216)
    bool load(xml_element* root); // 0x007CA4A0 (.h:264)
    bool write(_iobuf* f, const char* white_space) const; // 0x007CA530 (.h:286)
protected:
    T good_values[48]; // 0x00
    bool good_keyed[48]; // 0xC0 (<float>)
    T default_value; // 0xF0 (<float>)
};

// size 0x5D0
struct tod_orbital_object
{
    tod_orbital_object(const tod_orbital_object& __that); // 0x007E3090
    tod_orbital_object(); // 0x007E8F60 (.h:324)
    void init(); // 0x007D8900 (.h:330)

    float plane_rot_x; // 0x00
    float plane_rot_y; // 0x04
    float distance_off_plane; // 0x08
    char map_name[44]; // 0x0C
    float orbit_time_offset; // 0x38
    tod_interpolated_var<vector4> tint; // 0x40
    tod_interpolated_var<float> scale; // 0x380
    tod_interpolated_var<float> opacity; // 0x474
    bool alpha_blend; // 0x568
    char name[30]; // 0x569
    char effect_name[64]; // 0x587
    unsigned int m_effect_info_handle; // 0x5C8
    unsigned int m_effect_handle; // 0x5CC
};

// size 0x5750
struct tod_global_parameters
{
    tod_global_parameters(); // 0x007EDA70 (.h:361)
    void set_defaults(); // 0x007BFF60 (.h:366)
    ~tod_global_parameters(); // 0x007D9AC0

    farray<tod_orbital_object,15> orbital_objects; // 0x00
    bool horizon_mountain_enabled; // 0x5740
    bool fog_camera_follow; // 0x5741
    bool modified; // 0x5742
};

// size 0x75F0
struct tod_district_parameters
{
    tod_district_parameters(); // 0x007D89F0 (.h:465)
    bool any_keyed_at_interval(int);
    void set_defaults(); // 0x007CCD50 (.h:626)
    void build_all_unkeyed_values(); // 0x007D8E70 (.h:718)

    tod_interpolated_var<vector4> west_hemisphere[4]; // 0x00
    tod_interpolated_var<vector4> east_hemisphere[4]; // 0xD00
    tod_interpolated_var<vector4> zenith; // 0x1A00
    tod_interpolated_var<vector4> tod_light_color; // 0x1D40
    tod_interpolated_var<vector4> ambient_color; // 0x2080
    tod_interpolated_var<vector4> back_ambient_color; // 0x23C0
    tod_interpolated_var<vector4> fog_color; // 0x2700
    tod_interpolated_var<vector4> particle_ambient_color; // 0x2A40
    tod_interpolated_var<vector4> particle_tod_color; // 0x2D80
    tod_interpolated_var<float> fog_ground; // 0x30C0
    tod_interpolated_var<float> fog_atmosphere_scale; // 0x31B4
    tod_interpolated_var<float> fog_density; // 0x32A8
    tod_interpolated_var<float> fog_density_offset; // 0x339C
    tod_interpolated_var<float> ldr_min; // 0x3490
    tod_interpolated_var<float> ldr_max; // 0x3584
    tod_interpolated_var<float> bloom_exposure; // 0x3678
    tod_interpolated_var<float> iris_rate; // 0x376C
    tod_interpolated_var<float> luminance_max; // 0x3860
    tod_interpolated_var<float> luminance_min; // 0x3954
    tod_interpolated_var<float> luminance_mask_max; // 0x3A48
    tod_interpolated_var<float> eye_adaption_base; // 0x3B3C
    tod_interpolated_var<float> eye_adaption_amount; // 0x3C30
    tod_interpolated_var<float> eye_fade_min; // 0x3D24
    tod_interpolated_var<float> eye_fade_max; // 0x3E18
    tod_interpolated_var<float> brightpass_threshold; // 0x3F0C
    tod_interpolated_var<float> brightpass_offset; // 0x4000
    tod_interpolated_var<float> bloom_amount; // 0x40F4
    tod_interpolated_var<float> bloom_theta; // 0x41E8
    tod_interpolated_var<float> bloom_slope_A; // 0x42DC
    tod_interpolated_var<float> bloom_slope_B; // 0x43D0
    tod_interpolated_var<float> tonemap_lum_range; // 0x44C4
    tod_interpolated_var<float> tonemap_lum_offset; // 0x45B8
    tod_interpolated_var<float> color_correct_saturation; // 0x46AC
    tod_interpolated_var<vector4> color_correct_tint; // 0x47A0
    tod_interpolated_var<vector4> clouds_front_light_color; // 0x4AE0
    tod_interpolated_var<vector4> clouds_rear_light_color; // 0x4E20
    tod_interpolated_var<vector4> clouds_horizon_front_light_color; // 0x5160
    tod_interpolated_var<vector4> clouds_horizon_rear_light_color; // 0x54A0
    tod_interpolated_var<float> horizon_layer0_strength; // 0x57E0
    tod_interpolated_var<float> horizon_layer1_strength; // 0x58D4
    tod_interpolated_var<float> horizon_layer2_strength; // 0x59C8
    tod_interpolated_var<float> horizon_layer3_strength; // 0x5ABC
    tod_interpolated_var<float> overhead_layer0_strength; // 0x5BB0
    tod_interpolated_var<float> overhead_layer1_strength; // 0x5CA4
    tod_interpolated_var<float> overhead_layer2_strength; // 0x5D98
    tod_interpolated_var<float> overhead_layer3_strength; // 0x5E8C
    tod_interpolated_var<vector4> horizon_mountain_color_front; // 0x5F80
    tod_interpolated_var<vector4> horizon_mountain_color_back; // 0x62C0
    tod_interpolated_var<vector4> horizon_mountain_fog_color; // 0x6600
    tod_interpolated_var<float> horizon_mountain_fog_density; // 0x6940
    tod_interpolated_var<float> cloud_backlight_power; // 0x6A34
    tod_interpolated_var<float> cloud_backlight_strength; // 0x6B28
    tod_interpolated_var<float> cloud_layer01_speed; // 0x6C1C
    tod_interpolated_var<float> cloud_layer23_speed; // 0x6D10
    tod_interpolated_var<float> fog_camera_min; // 0x6E04
    tod_interpolated_var<float> fog_camera_max; // 0x6EF8
    tod_interpolated_var<float> fog_skybox_offset; // 0x6FEC
    tod_interpolated_var<float> fog_skybox_scale; // 0x70E0
    tod_interpolated_var<float> fog_clouds_offset; // 0x71D4
    tod_interpolated_var<float> fog_clouds_scale; // 0x72C8
    tod_interpolated_var<float> fog_matte_offset; // 0x73BC
    tod_interpolated_var<float> fog_matte_scale; // 0x74B0
    bool modified; // 0x75A4
    char filename[64]; // 0x75A5
};

extern bool Tod_hdr_enabled; // 0x0165E0AC
extern float TOD_intensity_scale; // 0x0165E0B4

bool tod_write_xml_helper(_iobuf* f, const char* tag_name, char* white_space, const vector& val); // 0x007C8B00 (:77)
bool tod_write_xml_helper(_iobuf* f, const char* tag_name, char* white_space, const float& val); // 0x007C8B20 (:82)
bool tod_write_xml_helper(_iobuf* f, const char* tag_name, char* white_space, const vector4& val); // 0x007C8B50 (:87)
bool tod_read_xml_helper(xml_element* parent, const char* tag, vector& val); // 0x007C8B70 (:92)
bool tod_read_xml_helper(xml_element* parent, const char* tag, float& val); // 0x007C8B90 (:97)
bool tod_read_xml_helper(xml_element* parent, const char* tag, vector4& val); // 0x007C8BC0 (:102)
bool tod_shared_load_district_parameters(tod_district_parameters& params, const char* tod_filename, mempool_base* parse_mempool); // 0x007E7560 (:283)
bool tod_shared_load_global_parameters(tod_global_parameters& params, const char* tod_filename, mempool_base* parse_mempool); // 0x007EB060 (:321)
bool tod_shared_save_district_parameters(const tod_district_parameters& params, const char* tod_filename); // 0x007D61E0 (:500)
bool tod_shared_save_global_parameters(const tod_global_parameters& params, const char* tod_filename); // 0x007D6230 (:518)
vector tod_shared_calcaulate_light_direction_at_time(float time, float rot_around_y, float min_angle_to_horizon); // 0x007C8BE0 (:540)
vector tod_shared_calcaulate_light_position_at_time(float time, float light_distance, float rot_around_y, float min_angle_to_horizon); // 0x007C8D80 (:579)
float wrap_around_range(float val, float min_val, float max_val); // 0x007C8E20 (:594)
void tod_shared_calculate_orbital_position(vector& pos, const tod_orbital_object& orbital_obj, float time); // 0x007C8EB0 (:611)
void hdr_color_from_vector4(rl_color_float& hdr_color, const vector4& source_color); // 0x007D62C0 (:630)
void ldr_color_from_vector4(rl_color_float& ldr_color, const vector4& source_color); // 0x007C9020 (:647)
void set_hdr_to_ldr_conversion_range(float min_intensity, float max_intensity); // 0x007C9120 (:663)
void tod_set_hdr_enabled(bool enabled); // 0x007C9140 (:669)
bool tod_get_hdr_enabled(); // 0x007C9150 (:674)
