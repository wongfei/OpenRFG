// Original: volition/rfg/code/weather/wind.h
// Implementation: volition/rfg/code/weather/wind.cpp
//
// Wind: speed and direction changing over time within wind_range_info limits (used by foliage and particles).
// Placement of wind_range_info and rfg_wind_info inferred.
#pragma once


struct xml_element;

// size 0x20
struct wind_range_info
{
    float min_speed; // 0x00
    float max_speed; // 0x04
    int min_time; // 0x08
    int max_time; // 0x0C
    float skip_up_percent; // 0x10
    float skip_down_percent; // 0x14
    float direction_percent; // 0x18
    float drop_to_zero_percent; // 0x1C
};

// size 0x124
// The wind state: the speed (mph) and direction moving to their destinations over the ramp time, within the
// ranges of weather.xtbl.
struct rfg_wind_info
{
    bool wind_active; // 0x00
    float wind_mph; // 0x04
    float wind_dest_mph; // 0x08
    float wind_dir; // 0x0C
    float wind_dest_dir; // 0x10
    int min_ramp_time; // 0x14
    int max_ramp_time; // 0x18
    int ramp_time; // 0x1C
    int num_wind_ranges; // 0x20
    wind_range_info wind_ranges[8]; // 0x24
};

float weather_wind_get_max_speed(); // 0x00BEC670 (:24)
bool weather_internal_parse_wind(xml_element* wind); // 0x00BEC680 (:33)
void unused_dcf_wind(); // 0x00BED1B0 (:92)
void unused_dcf_wind_speed(); // 0x00BECEE0 (:129)
void unused_dcf_Wind_dir_wind(); // 0x00BECF80 (:156)
void unused_dcf_zone_min_wind_speed(); // 0x00BEC850 (:188)
void unused_dcf_zone_max_wind_speed(); // 0x00BEC900 (:213)
bool weather_wind_start(); // 0x00BED070 (:269)
void weather_wind_stop(); // 0x00BED170 (:308)
void weather_wind_reset(); // 0x00BEC9B0 (:315)
void weather_wind_get_speed(float* mph, float* dir); // 0x00BEC9E0 (:323)
void weather_wind_set_limits(float min_speed, float max_speed); // 0x00BECA10 (:334)
void weather_wind_set_speed(float mph, float dir, float transition_rate); // 0x00BECA30 (:341)
void weather_wind_update(); // 0x00BECAA0 (:362)
