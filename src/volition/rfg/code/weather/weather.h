// Original: volition/rfg/code/weather/weather.h
// Implementation: volition/rfg/code/weather/weather.cpp
//
// Weather: dust storms and the weather state blended into fog, wind and particle effects. Placement of
// rfg_weather_info inferred.
#pragma once

#include "volition/rfg/code/weather/wind.h"

// size 0x124
struct rfg_weather_info
{
    rfg_wind_info wind; // 0x00
};

extern rfg_weather_info Weather_stats; // 0x038B2F90

bool weather_parse_xtbl(const char* filename); // 0x00BECE20 (:42)
void weather_level_init_pre(); // 0x00BEC630 (:73)
void weather_level_shutdown(); // 0x00BEC640 (:79)
bool weather_system_init(); // 0x00BECEA0 (:85)
void weather_system_shutdown(); // 0x00BEC650 (:103)
void weather_update(); // 0x00BECED0 (:115)
