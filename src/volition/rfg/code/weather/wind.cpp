// Original: volition/rfg/code/weather/wind.cpp
//
// The wind (decompiled; header comment in wind.h). While active, every 20 seconds the wind picks a new target speed
// within the zone's limits (at most 40 mph up or down from the current speed), ramps the destination to it over the
// first 10 seconds and eases the speed and direction towards their destinations at Wind_transition_rate per second.
// Without a local player nothing changes; the multiplayer modes start it with the level's intensity and direction.
//
// Split TU (VFX.md 2.6): compiled twice. The rfg library builds only weather_wind_get_speed (the undergrowth reads it
// in every program). The effect library (rfg_effects, OPENRFG_EFFECTS) builds the rest; the programs without it keep
// its stand-ins.

#include "volition/rfg/code/weather/weather.h"
#include "volition/rfg/code/weather/wind.h"

#ifndef OPENRFG_EFFECTS

// 0x00BEC9E0 (:323)
// The current wind speed (mph) and direction; either pointer may be null.
void weather_wind_get_speed(float* mph, float* dir)
{
    if (mph != nullptr)
    {
        *mph = Weather_stats.wind.wind_mph;
    }
    if (dir != nullptr)
    {
        *dir = Weather_stats.wind.wind_dir;
    }
}

#else // OPENRFG_EFFECTS

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"

#include <math.h>
#include <stdio.h>

static const int WIND_CYCLE_MS = 20000; // a new target speed every 20 s
static const float WIND_RAMP_MS = 10000.0f; // reached over the cycle's first 10 s
static const float WIND_MAX_CHANGE_MPH = 40.0f;
static const float WIND_DEFAULT_MIN_SPEED = 75.0f; // without a zone at the local player
static const float WIND_DEFAULT_MAX_SPEED = 85.0f;
static const float WIND_START_DIR = 90.0f;
static const float WIND_MAX_DIR = 360.0f;
static const unsigned int CONSOLE_ARG_WIND_ONOFF = 0xC1; // console_get_arg type: on, off or a boolean
static const unsigned int CONSOLE_ARG_FLOAT = 0x10;

// ---- file-static globals
static float Wind_min_speed = 75.0f; // 0x016EBEE8
static float Wind_max_speed = 85.0f; // 0x016EBEEC
static float Wind_dir_wind = 90.0f; // 0x016EBEF0, the last direction set from the console
static float Wind_transition_rate = 30.0f; // 0x016EBEF4
static float start_speed; // 0x038B30B4
static float end_speed; // 0x038B30B8
static bool Wind_debug; // 0x038B30BD
// The console commands print through vlib's thread-local block (TLS + 0x38).
static thread_local char Wind_console_buffer[2048];

// 0x00BEC670 (:24)
float weather_wind_get_max_speed()
{
    return Wind_max_speed;
}

// 0x00BEC680 (:33)
// The ramp times and the speed ranges (sorted by minimum speed).
bool weather_internal_parse_wind(xml_element* wind)
{
    int value;
    xml_table_get_req_int(value, wind, "Min_Ramp_Time");
    Weather_stats.wind.min_ramp_time = value;
    xml_table_get_req_int(value, wind, "Max_Ramp_Time");
    Weather_stats.wind.max_ramp_time = value;
    xml_element* ranges = xml_find(wind, "Wind_Ranges");
    if (ranges != nullptr)
    {
        Weather_stats.wind.num_wind_ranges = xml_table_count(ranges, "Range_Entry");
        wind_range_info* range_p = Weather_stats.wind.wind_ranges;
        for (xml_element* entry = xml_find(ranges, "Range_Entry"); entry != nullptr; entry = xml_find_next(ranges, entry, "Range_Entry"))
        {
            xml_table_get_req_float(range_p->min_speed, entry, "Min_Speed");
            xml_table_get_req_float(range_p->max_speed, entry, "Max_Speed");
            xml_table_get_req_int(range_p->min_time, entry, "Min_Time_In_Range");
            xml_table_get_req_int(range_p->max_time, entry, "Max_Time_In_Range");
            xml_table_get_req_float(range_p->skip_up_percent, entry, "Skip_Up_Percentage");
            xml_table_get_req_float(range_p->skip_down_percent, entry, "Skip_Down_Percentage");
            xml_table_get_req_float(range_p->direction_percent, entry, "Direction_Percentage");
            xml_table_get_req_float(range_p->drop_to_zero_percent, entry, "Drop_To_Zero_Percentage");
            ++range_p;
        }
    }
    // Exchange sort by min_speed.
    for (int i = 0; i < Weather_stats.wind.num_wind_ranges; ++i)
    {
        for (int j = i + 1; j < Weather_stats.wind.num_wind_ranges; ++j)
        {
            if (Weather_stats.wind.wind_ranges[j].min_speed < Weather_stats.wind.wind_ranges[i].min_speed)
            {
                const wind_range_info swap = Weather_stats.wind.wind_ranges[i];
                Weather_stats.wind.wind_ranges[i] = Weather_stats.wind.wind_ranges[j];
                Weather_stats.wind.wind_ranges[j] = swap;
            }
        }
    }
    return true;
}

// 0x00BED1B0 (:92)
// The "wind" console command: on, off or toggle.
void unused_dcf_wind()
{
    if (Console_command != 0)
    {
        if (!console_get_arg(CONSOLE_ARG_WIND_ONOFF, 1))
        {
            sprintf(Wind_console_buffer, "Invalid syntax");
        }
        else
        {
            if ((Console_arg_type & 0x40) != 0 || ((Console_arg_type & 0x80) == 0 && (Console_arg_type & 1) != 0 && !Weather_stats.wind.wind_active)) // 0x40 on, 0x80 off, 1 toggle
            {
                weather_wind_start();
            }
            else if ((Console_arg_type & 0x80) != 0 || (Console_arg_type & 1) != 0)
            {
                weather_wind_stop();
            }
            sprintf(Wind_console_buffer, "wind is now %s\n", Weather_stats.wind.wind_active ? "on" : "off");
        }
    }
    if (Console_status != 0)
    {
        sprintf(Wind_console_buffer, "wind is currently %s\n", Weather_stats.wind.wind_active ? "on" : "off");
    }
    if (Console_help != 0)
    {
        sprintf(Wind_console_buffer, "Usage: wind <on|off>\n");
    }
    console_output(Wind_console_buffer, nullptr, false);
}

// 0x00BECEE0 (:129)
// The "wind_speed" console command: sets the destination speed.
void unused_dcf_wind_speed()
{
    if (Console_command != 0 && console_get_arg(CONSOLE_ARG_FLOAT, 1))
    {
        Weather_stats.wind.wind_dest_mph = Console_arg_float;
    }
    sprintf(Wind_console_buffer, "Wind Speed: %3.2f Dir: %3.2f\n", (double)Weather_stats.wind.wind_mph, (double)Weather_stats.wind.wind_dir);
    console_output(Wind_console_buffer, nullptr, false);
    const bool help = Console_help != 0;
    Wind_console_buffer[0] = '\0';
    if (help)
    {
        console_output("Usage: wind_speed <value (0-100.0)>\n", nullptr, false);
    }
}

// 0x00BECF80 (:156)
// The "wind_dir" console command: holds the speed and sets the destination direction (clamped to [0, 360]).
void unused_dcf_Wind_dir_wind()
{
    if (Console_command != 0 && console_get_arg(CONSOLE_ARG_FLOAT, 1))
    {
        Weather_stats.wind.wind_dest_mph = Weather_stats.wind.wind_mph;
        Wind_dir_wind = Console_arg_float;
        if (0.0f <= Console_arg_float)
        {
            Weather_stats.wind.wind_dest_dir = Console_arg_float;
            if (Console_arg_float - 0.0f < 0.0f)
            {
                Weather_stats.wind.wind_dest_dir = 0.0f;
            }
            if (0.0f <= Weather_stats.wind.wind_dest_dir - WIND_MAX_DIR)
            {
                Weather_stats.wind.wind_dest_dir = WIND_MAX_DIR;
            }
        }
    }
    sprintf(Wind_console_buffer, "Wind Speed: %3.2f Dir: %3.2f\n", (double)Weather_stats.wind.wind_mph, (double)Weather_stats.wind.wind_dir);
    console_output(Wind_console_buffer, nullptr, false);
    const bool help = Console_help != 0;
    Wind_console_buffer[0] = '\0';
    if (help)
    {
        console_output("Usage: wind_dir <value (0-360.0)>\n", nullptr, false);
    }
}

// 0x00BEC850 (:188)
// The "zone_min_wind_speed" console command: the local player's zone's minimum wind speed.
void unused_dcf_zone_min_wind_speed()
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_output("Usage: zone_min_wind_speed <value>", nullptr, false);
        }
        return;
    }
    player* player_p = player_get_local_player();
    if (player_p == nullptr)
    {
        return;
    }
    obj_zone* zone_p = World.get_zone_by_pos(player_p->pos);
    if (zone_p == nullptr)
    {
        return;
    }
    if (!console_get_arg(CONSOLE_ARG_FLOAT, 1))
    {
        sprintf(Wind_console_buffer, "Zone min wind speed: %03f", (double)zone_p->wind_min_speed);
        console_printf(Wind_console_buffer);
        return;
    }
    zone_p->wind_min_speed = Console_arg_float;
}

// 0x00BEC900 (:213)
// The "zone_max_wind_speed" console command.
void unused_dcf_zone_max_wind_speed()
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_output("Usage: zone_max_wind_speed <value>", nullptr, false);
        }
        return;
    }
    player* player_p = player_get_local_player();
    if (player_p == nullptr)
    {
        return;
    }
    obj_zone* zone_p = World.get_zone_by_pos(player_p->pos);
    if (zone_p == nullptr)
    {
        return;
    }
    if (!console_get_arg(CONSOLE_ARG_FLOAT, 1))
    {
        sprintf(Wind_console_buffer, "Zone max wind speed: %03f", (double)zone_p->wind_max_speed);
        console_printf(Wind_console_buffer);
        return;
    }
    zone_p->wind_max_speed = Console_arg_float;
}

// 0x00BED070 (:269)
// Starts at the middle of the local player's zone's limits (75-85 mph without a zone), blowing at 90 degrees.
bool weather_wind_start()
{
    Weather_stats.wind.wind_active = true;
    player* player_p = player_get_local_player();
    obj_zone* zone_p = player_p != nullptr ? World.get_zone_by_pos(player_p->pos) : nullptr;
    if (zone_p == nullptr)
    {
        Wind_max_speed = WIND_DEFAULT_MAX_SPEED;
        Wind_min_speed = WIND_DEFAULT_MIN_SPEED;
    }
    else
    {
        Wind_min_speed = zone_p->wind_min_speed;
        Wind_max_speed = zone_p->wind_max_speed;
    }
    start_speed = (Wind_max_speed + Wind_min_speed) * 0.5f;
    Weather_stats.wind.wind_dest_mph = start_speed;
    Weather_stats.wind.wind_dest_dir = WIND_START_DIR;
    Weather_stats.wind.wind_mph = start_speed;
    return true;
}

// 0x00BED170 (:308)
void weather_wind_stop()
{
    Weather_stats.wind.wind_active = false;
    Weather_stats.wind.wind_dest_mph = 0.0f;
    Weather_stats.wind.wind_dest_dir = 0.0f;
}

// 0x00BEC9B0 (:315)
void weather_wind_reset()
{
    Weather_stats.wind.wind_mph = 0.0f;
    Weather_stats.wind.wind_dir = 0.0f;
    Weather_stats.wind.wind_dest_mph = 0.0f;
    Weather_stats.wind.wind_dest_dir = 0.0f;
}

// 0x00BECA10 (:334)
void weather_wind_set_limits(float min_speed, float max_speed)
{
    Wind_min_speed = min_speed;
    Wind_max_speed = max_speed;
}

// 0x00BECA30 (:341)
// The destination speed; a direction and rate only when not negative (the direction clamped to [0, 360]).
void weather_wind_set_speed(float mph, float dir, float transition_rate)
{
    Weather_stats.wind.wind_dest_mph = mph;
    if (0.0f <= dir)
    {
        if (dir - 0.0f < 0.0f)
        {
            dir = 0.0f;
        }
        Weather_stats.wind.wind_dest_dir = dir;
        if (0.0f <= dir - WIND_MAX_DIR)
        {
            Weather_stats.wind.wind_dest_dir = WIND_MAX_DIR;
        }
    }
    if (0.0f <= transition_rate)
    {
        Wind_transition_rate = transition_rate;
    }
}

// 0x00BECAA0 (:362)
void weather_wind_update()
{
    static timestamp t0; // 0x038B3150 (guard $S5 0x038B3154), invalid from its constructor
    player* player_p = player_get_local_player();
    if (player_p == nullptr)
    {
        return;
    }
    if (!Weather_stats.wind.wind_active)
    {
        Weather_stats.wind.wind_mph = 0.0f;
        Weather_stats.wind.wind_dest_dir = 0.0f;
        Weather_stats.wind.wind_dest_mph = 0.0f;
        start_speed = 0.0f;
        t0.invalidate();
        Weather_stats.wind.wind_dir = 0.0f;
        return;
    }
    if (Wind_transition_rate <= 0.0f)
    {
        Weather_stats.wind.wind_mph = Weather_stats.wind.wind_dest_mph;
        Weather_stats.wind.wind_dir = Weather_stats.wind.wind_dest_dir;
        return;
    }
    if (t0.valid())
    {
        if ((float)t0.time_until() <= WIND_RAMP_MS)
        {
            if (t0.elapsed())
            {
                t0.invalidate();
            }
        }
        else
        {
            const float ramp = ((float)WIND_CYCLE_MS - (float)t0.time_until()) / WIND_RAMP_MS;
            weather_wind_set_speed(fabsf(ramp * (end_speed - start_speed) + start_speed), -1.0f, -1.0f);
        }
    }
    else
    {
        t0.set(WIND_CYCLE_MS);
        obj_zone* zone_p = World.get_zone_by_pos(player_p->pos);
        if (zone_p == nullptr)
        {
            return;
        }
        Wind_min_speed = zone_p->wind_min_speed;
        Wind_max_speed = zone_p->wind_max_speed;
        if (Wind_max_speed < Wind_min_speed)
        {
            const float swap = Wind_max_speed;
            Wind_max_speed = Wind_min_speed;
            Wind_min_speed = swap;
        }
        float up = Wind_max_speed - Weather_stats.wind.wind_mph;
        start_speed = Weather_stats.wind.wind_mph;
        if (up - 0.0f < 0.0f)
        {
            up = 0.0f;
        }
        if (0.0f <= up - WIND_MAX_CHANGE_MPH)
        {
            up = WIND_MAX_CHANGE_MPH;
        }
        float down = Wind_min_speed - Weather_stats.wind.wind_mph;
        if (down - -WIND_MAX_CHANGE_MPH < 0.0f)
        {
            down = -WIND_MAX_CHANGE_MPH;
        }
        float minimum = 0.0f;
        if (down - 0.0f < 0.0f)
        {
            minimum = down;
        }
        end_speed = fl_rand_range(minimum, up) + start_speed;
    }
    const float rate = Frametime * Wind_transition_rate;
    float speed_blend = 1.0f;
    if (0.0f <= 1.0f - rate)
    {
        speed_blend = rate;
    }
    Weather_stats.wind.wind_mph = (Weather_stats.wind.wind_dest_mph - Weather_stats.wind.wind_mph) * speed_blend + Weather_stats.wind.wind_mph;
    const float dir_blend = 1.0f - rate < 0.0f ? 1.0f : rate;
    Weather_stats.wind.wind_dir = (Weather_stats.wind.wind_dest_dir - Weather_stats.wind.wind_dir) * dir_blend + Weather_stats.wind.wind_dir;
}

#endif // OPENRFG_EFFECTS
