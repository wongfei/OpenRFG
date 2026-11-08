// Original: volition/rfg/code/gamewide/game_time_of_day.h
// Implementation: volition/rfg/code/gamewide/game_time_of_day.cpp
//
// The time-of-day system: the current time and its speed, blending of the global and district parameter sets
// (TOD_blend_info) into the sky, sun light, ambient, fog, HDR and color correction of the main scene, and the time of
// day light. Placement of tod_district_info inferred.
#pragma once

#include "volition/rfg/code/gamewide/time_of_day_shared.h"

struct horizon_mountain_parameters;
class object_effect;
class rl_color_float;
class rl_renderer;
struct rl_skybox_orbital_params;
class vector;
class vector4;

// size 0x24
struct TOD_blend_info
{
    TOD_blend_info(); // 0x007BFF40 (.h:60)

    tod_district_parameters* sources[4]; // 0x00
    float blend_weights[4]; // 0x10
    bool blend_valid; // 0x20
};

// size 0x7600
struct tod_district_info
{
    tod_district_info(); // 0x007DFA00

    unsigned int m_district_handle; // 0x00
    tod_district_parameters m_params; // 0x10
};

extern float Current_time_of_day; // 0x0165CC80, hours

void unused_dcf_tod_light_color(); // 0x007D9AD0 (:152)
void unused_dcf_TOD_light_on_off(); // 0x007C52E0 (:193)
void unused_dcf_reload_tod(); // 0x007EFF20 (:222)
void unused_dcf_tod_ambient(); // 0x007D9FB0 (:251)
void unused_dcf_tod_back_ambient(); // 0x007DA560 (:288)
void unused_dcf_tod_fog_color(); // 0x007DAB10 (:326)
void unused_dcf_tod_fog_ground(); // 0x007DB0C0 (:364)
void unused_dcf_tod_fog_atmosphere_scale(); // 0x007DB220 (:390)
void unused_dcf_tod_fog_density(); // 0x007DB380 (:416)
void unused_dcf_tod_fog_density_offset(); // 0x007DB4E0 (:441)
void unused_dcf_tod_light_angles(); // 0x007C54F0 (:466)
void unused_dcf_snap_tod_light_to_camera(); // 0x007C55F0 (:494)
void unused_dcf_tod_enabled(); // 0x007C5620 (:505)
void unused_dcf_tod_cloud_horizon_strenghts(); // 0x007DB640 (:532)
void unused_dcf_tod_cloud_overhead_strenghts(); // 0x007DBA60 (:577)
void game_time_of_day_render_update(void*, rl_renderer*); // 0x007DBE80 (:883)
void game_time_of_day_system_init(); // 0x007DF9A0 (:1046)
void game_time_of_day_level_init_pre(); // 0x007E9260 (:1053)
void game_time_of_day_level_init_post(); // 0x007EEAA0 (:1120)
void game_time_of_day_level_shutdown(); // 0x007C5810 (:1149)
void game_time_of_day_get_skybox_tod_colors(rl_color_float* colors); // 0x007DFA10 (:1181)
void game_time_of_day_do_frame(); // 0x007E27B0 (:1199)
bool game_time_of_day_load_global(const char* filename); // 0x007EEB50 (:1272)
void game_time_of_day_render_debug(); // 0x007E9760 (:1294)
void game_time_of_day_get_cloud_lighting(rl_color_float& horizon_front_ambient, rl_color_float& horizon_back_ambient, rl_color_float& overhead_front_ambient, rl_color_float& overhead_back_ambient, float& backlight_strength, float& backlight_power, vector& light_direction); // 0x007E31B0 (:1358)
void get_time_of_day_get_horizon_mountain_params(horizon_mountain_parameters& parameters); // 0x007E4590 (:1376)
void game_time_of_day_get_weather_settings(vector4& horizon_strengths, vector4& overhead_strengths); // 0x007C5880 (:1390)
int game_time_of_day_get_num_orbitals(); // 0x007D20C0 (:1400)
void game_time_of_day_get_orbital_params(int idx, rl_skybox_orbital_params& params); // 0x007E5400 (:1410)
object_effect* game_time_of_day_get_orbital_effect(int idx); // 0x007D20D0 (:1439)
void game_time_of_day_get_cloud_scroll_values(float& layer01_scroll, float& layer23_scroll); // 0x007C58C0 (:1463)
bool game_time_of_day_load_district(unsigned int district_handle); // 0x007E9BE0 (:1475)
void game_time_of_day_set_tutorial_tod(); // 0x007C58F0 (:1550)
