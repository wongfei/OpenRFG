// Original: volition/rfg/code/gamewide/game_clock.cpp
//
// The game clock and pausing (game_clock.h), whole (multiplayer.md MP1). rfg_gamewide_unity.obj is a unity build:
// Game_clock and Ingame_time_should_update are gamestate_gameplay.cpp's statics (rfg_gamewide_unity.cpp includes it
// first).
//
// Precision (from the asm): the time-of-day conversions are evaluated in double per statement and rounded to float, and
// their whole parts are an inlined floor (in double) truncated to an integer; game_clock_increment_seconds uses the x87
// with truncation.
//
// Notes (decompiled behaviour, kept as is):
// - game_clock_increment_seconds converts the whole days to float as an unsigned value (a negative count of days adds
//   2^32), and a negative time step adds those days as a huge positive count.
// - free_game_time_trigger moves the following triggers down 4 bytes at a time over (count - index) * 20 bytes.

#include "volition/rfg/code/gamewide/game_clock.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <intrin.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

const game_clock* Gamewide_game_clock = &Game_clock; // 0x0165CBE4
volatile int Game_paused_count; // 0x02533880

// ---- file-static globals
static const unsigned char Num_days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; // 0x012E80B0
static const char* const Days_of_the_week[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"}; // 0x012E80BC
static game_time_trigger Game_time_triggers[32]; // 0x02532910
static int Num_game_time_triggers; // 0x02533884

// Magic number: 2^32, the day ticks of one day.
static const double Ticks_per_day = 4294967296.0;

// ---- internal functions
static bool game_clock_is_leap_year(unsigned short year); // 0x007C0BD0 (:205)
static unsigned char game_clock_num_days_in_month(unsigned char month, unsigned short year); // 0x007C0C00 (:239)
static void game_clock_update_time_of_day_from_day_ticks(game_clock& clock); // 0x007C0C50 (:262)
static int i_mod(int x, int y); // 0x007C0E70 (:315)
static game_time_trigger* allocate_game_time_trigger(); // 0x007C0F00 (:322)
static void free_game_time_trigger(int index); // 0x007C0F20 (:333)
static void game_time_triggers_process(); // 0x007C0F60 (:348)

// The inlined floor of the conversions: floor in double of the float, truncated.
static int clock_floor(float f)
{
    return (int)(float)floor((double)f);
}

// 0x007CD4D0 (:126)
// Console command game_clock_set [hours minutes [seconds]]: sets the time, then prints the date and time.
void unused_dcf_game_clock_set()
{
    if (Console_command != 0 && console_get_arg(8, true))
    {
        const int hours = Console_arg_int;
        console_get_arg(8, false);
        const int minutes = Console_arg_int;
        int seconds = 0;
        if (console_get_arg(8, true))
        {
            seconds = Console_arg_int;
        }
        // Magic numbers: 23 hours, 59 minutes.
        if ((unsigned int)hours > 23)
        {
            console_output("hours must be from 0 to 23", nullptr, false);
            return;
        }
        if ((unsigned int)minutes > 59)
        {
            console_output("minutes must be from 0 to 59", nullptr, false);
            return;
        }
        game_clock_set_time(hours, minutes, seconds);
    }
    unsigned int hour = Gamewide_game_clock->m_hour;
    const char* am_pm = "AM";
    if (hour > 11)
    {
        am_pm = "PM";
        hour -= 12;
    }
    if (hour == 0)
    {
        hour = 12;
    }
    // Original: a thread-local scratch buffer; a static one here.
    static char text[128];
    sprintf(text, "%s %d/%d/%d %d:%02d.%02d %s", Days_of_the_week[Game_clock.m_day_of_week], Gamewide_game_clock->m_month + 1, (unsigned int)Gamewide_game_clock->m_day,
            (unsigned int)Gamewide_game_clock->m_year, hour, (unsigned int)Gamewide_game_clock->m_minutes, (unsigned int)Gamewide_game_clock->m_seconds, am_pm);
    console_output(text, nullptr, false);
}

// 0x007C0B30 (:190)
void game_clock_update_day_ticks_from_time_of_day()
{
    Game_clock.m_curr_day_ticks = fl_uround((float)((double)(float)Game_clock.m_hour / 24.0 * Ticks_per_day));
    Game_clock.m_curr_day_ticks += fl_uround((float)((double)(float)Game_clock.m_minutes / 1440.0 * Ticks_per_day));
    Game_clock.m_curr_day_ticks += fl_uround((float)((double)(float)Game_clock.m_seconds / 86400.0 * Ticks_per_day));
}

// 0x007C0BD0 (:205)
static bool game_clock_is_leap_year(unsigned short year)
{
    if ((year & 3) != 0)
    {
        return false;
    }
    if (year % 100 != 0)
    {
        return true;
    }
    return year % 400 == 0;
}

// 0x007C0C00 (:239)
// LTCG: month in AL, year in EDX.
static unsigned char game_clock_num_days_in_month(unsigned char month, unsigned short year)
{
    const unsigned char days = Num_days_per_month[month];
    if (month != 1)
    {
        return days;
    }
    return game_clock_is_leap_year(year) ? (unsigned char)(days + 1) : days;
}

// 0x007C0C50 (:262)
// LTCG: clock in EAX.
static void game_clock_update_time_of_day_from_day_ticks(game_clock& clock)
{
    const float ticks = (float)clock.m_curr_day_ticks;
    const int hour = clock_floor((float)((double)ticks * 24.0 * (1.0 / Ticks_per_day)));
    clock.m_hour = (unsigned char)hour;
    const float day_minutes = (float)((double)ticks * 1440.0 * (1.0 / Ticks_per_day));
    const float hour_minutes = (float)((double)(hour & 0xFF) * 60.0);
    if (hour_minutes > day_minutes)
    {
        clock.m_minutes = 0;
    }
    else
    {
        clock.m_minutes = (unsigned char)clock_floor((float)((double)day_minutes - (double)hour_minutes));
    }
    const float day_seconds = (float)((double)ticks * 86400.0 * (1.0 / Ticks_per_day));
    const float hour_and_minute_seconds = (float)((double)clock.m_minutes * 60.0 + (double)(hour & 0xFF) * 3600.0);
    unsigned char seconds = 0;
    if (!(hour_and_minute_seconds > day_seconds))
    {
        seconds = (unsigned char)clock_floor((float)((double)day_seconds - (double)hour_and_minute_seconds));
    }
    clock.m_seconds = seconds;
}

// 0x007C0E70 (:315)
// LTCG: x in EAX, y in ECX. x mod y, floored (the quotient in float).
static int i_mod(int x, int y)
{
    return x - clock_floor((float)x / (float)y) * y;
}

// 0x007C0F00 (:322)
static game_time_trigger* allocate_game_time_trigger()
{
    // Magic number: 32, the size of Game_time_triggers.
    if (Num_game_time_triggers < 32)
    {
        return &Game_time_triggers[Num_game_time_triggers++];
    }
    return nullptr;
}

// 0x007C0F20 (:333)
static void free_game_time_trigger(int index)
{
    Num_game_time_triggers--;
    if (index < Num_game_time_triggers)
    {
        memmove(&Game_time_triggers[index], &Game_time_triggers[index + 1], (size_t)(Num_game_time_triggers - index) * sizeof(game_time_trigger));
    }
}

// 0x007C0F60 (:348)
// Removes each trigger whose time has come (the seconds from now to it are not positive). The script call of the
// trigger (func_handle) is not made: rfg.exe compiles it out.
static void game_time_triggers_process()
{
    for (int i = 0; i < Num_game_time_triggers; i++)
    {
        const game_clock& t = Game_time_triggers[i].m_trigger_time;
        // The days from now to the trigger's date: whole years, then months, then days.
        const int year_step = t.m_year < Game_clock.m_year ? -1 : 1;
        int days = 0;
        for (unsigned int year = Game_clock.m_year; year != t.m_year; year += year_step)
        {
            days += year_step * 365;
            if (year_step < 0 && game_clock_is_leap_year((unsigned short)(year - 1)))
            {
                days -= 1;
            }
            else if (year_step > 0 && game_clock_is_leap_year((unsigned short)year))
            {
                days += 1;
            }
        }
        const int month_step = t.m_month < Game_clock.m_month ? -1 : 1;
        for (unsigned int month = Game_clock.m_month; month != t.m_month; month += month_step)
        {
            // Notes (decompiled behaviour, kept as is): the month lengths use the trigger's year.
            if (month_step < 0)
            {
                days -= game_clock_num_days_in_month((unsigned char)(month - 1), t.m_year);
            }
            else
            {
                days += game_clock_num_days_in_month((unsigned char)month, t.m_year);
            }
        }
        days += ((int)t.m_day - (int)Game_clock.m_day) * month_step;
        const int seconds = (((days * 24 + t.m_hour - Game_clock.m_hour) * 60 + t.m_minutes - Game_clock.m_minutes) * 60 + t.m_seconds) - Game_clock.m_seconds;
        if (seconds < 1)
        {
            free_game_time_trigger(i);
            i--;
        }
    }
}

// 0x007C11C0 (:425)
// The start date: 13 May 2004 (the month counts from 0, the day from 1), 11:00, 48 game
// seconds per real second. The day of the week is (days since 1 January 2004 + 3) mod 7, an index of
// Days_of_the_week (Monday first; 1 January 2004 was a Thursday).
void game_clock_init()
{
    // Magic numbers: the start date and time and the time scale.
    Game_clock.m_year = 2004;
    Game_clock.m_month = 4;
    Game_clock.m_day = 13;
    Game_clock.m_hour = 11;
    Game_clock.m_minutes = 0;
    Game_clock.m_seconds = 0;
    Game_clock.m_time_scale = 48.0f;
    game_clock_update_day_ticks_from_time_of_day();
    // Magic number: 2004, the reference year of the day of the week.
    const unsigned short reference_year = 2004;
    if (Game_clock.m_year >= reference_year)
    {
        int days = Game_clock.m_day - 1;
        for (unsigned short year = reference_year; year < Game_clock.m_year; year++)
        {
            days += game_clock_is_leap_year(year) ? 366 : 365;
        }
        for (unsigned char month = 0; month < Game_clock.m_month; month++)
        {
            days += game_clock_num_days_in_month(month, Game_clock.m_year);
        }
        Game_clock.m_day_of_week = (unsigned char)((unsigned long long)(unsigned int)(days + 3) % 7);
        return;
    }
    // Before the reference year: the days from the date to the reference year's start, counted backwards.
    int days = (int)game_clock_num_days_in_month(Game_clock.m_month, Game_clock.m_year) - (int)Game_clock.m_day + 1;
    for (unsigned short year = (unsigned short)(Game_clock.m_year + 1); year < reference_year; year++)
    {
        days += game_clock_is_leap_year(year) ? 366 : 365;
    }
    for (unsigned char month = (unsigned char)(Game_clock.m_month + 1); month < 12; month++)
    {
        days += game_clock_num_days_in_month(month, Game_clock.m_year);
    }
    Game_clock.m_day_of_week = (unsigned char)i_mod(3 - days, 7);
}

// 0x007C1460 (:509)
void game_clock_set(const game_clock* game_clock_state)
{
    Game_clock = *game_clock_state;
}

// 0x007C1480 (:516)
const game_clock* game_clock_get()
{
    return &Game_clock;
}

// 0x007D9930 (:526)
void game_clock_update(float delta_time, bool delta_is_game_time)
{
    if (Ingame_time_should_update)
    {
        const float scale = delta_is_game_time ? 1.0f : Game_clock.m_time_scale;
        game_clock_increment_seconds(Game_clock, scale * delta_time);
        game_time_triggers_process();
    }
}

// 0x007CD5E0 (:548)
void game_clock_increment_seconds(game_clock& clock, float num_game_seconds)
{
    // Magic number: 86400 seconds per day.
    const float day_fraction = (float)((double)num_game_seconds / 86400.0);
    int num_days = (int)(long long)(float)floor((double)day_fraction);
    const unsigned int old_ticks = clock.m_curr_day_ticks;
    const float whole_days = (float)(unsigned int)num_days; // see the file comment
    const float rest = (float)((double)num_game_seconds - (double)whole_days * 86400.0);
    clock.m_curr_day_ticks += fl_uround((float)((double)rest / 86400.0 * Ticks_per_day));
    game_clock_update_time_of_day_from_day_ticks(clock);
    if (clock.m_curr_day_ticks < old_ticks)
    {
        num_days++;
    }
    game_clock_increment_days(clock, num_days);
}

// 0x007C1490 (:582)
void game_clock_increment_days(game_clock& clock, int num_days)
{
    for (int i = 0; i < num_days; i++)
    {
        clock.m_day++;
        clock.m_day_of_week++;
        if (clock.m_day_of_week > 6)
        {
            clock.m_day_of_week = 0;
        }
        if (clock.m_day > game_clock_num_days_in_month(clock.m_month, clock.m_year))
        {
            clock.m_day = 1;
            clock.m_month++;
            if (clock.m_month > 11)
            {
                clock.m_month = 0;
                clock.m_year++;
            }
        }
    }
}

// 0x007C1560 (:624)
int game_clock_compare_dates(const game_clock* clock1, const game_clock* clock2)
{
    int diff = (int)clock1->m_year - (int)clock2->m_year;
    if (diff == 0)
    {
        diff = (int)clock1->m_month - (int)clock2->m_month;
        if (diff == 0)
        {
            diff = (int)clock1->m_day - (int)clock2->m_day;
            if (diff == 0)
            {
                diff = (int)(clock1->m_curr_day_ticks - clock2->m_curr_day_ticks);
            }
        }
    }
    return diff;
}

// 0x007C15A0 (:646)
const char* game_clock_get_day_of_week_str()
{
    return Days_of_the_week[Game_clock.m_day_of_week];
}

// 0x007C15B0 (:654)
int game_clock_get_time_hour()
{
    return Game_clock.m_hour;
}

// 0x007C15C0 (:665)
// Four-hour parts of the day starting at 6:00.
game_clock_tod_partitioned game_clock_get_tod_partitioned()
{
    int hours = Game_clock.m_hour - 6;
    if (hours < 0)
    {
        hours = Game_clock.m_hour + 18;
    }
    return (game_clock_tod_partitioned)((unsigned int)hours >> 2);
}

// 0x007C15E0 (:688)
// Day from 6:00 to 18:00, night from 18:00 to 6:00.
game_clock_time_of_day game_clock_get_time_of_day()
{
    switch (game_clock_get_tod_partitioned())
    {
    case GC_EARLY_DAY:
    case GC_MID_DAY:
    case GC_LATE_DAY:
        break;
    case GC_EARLY_NIGHT:
    case GC_MID_NIGHT:
    case GC_LATE_NIGHT:
        return GC_NIGHT;
    default:
        VLIB_CRITICAL_FAILURE();
        break;
    }
    return GC_DAY;
}

// 0x007C1640 (:712)
bool game_time_trigger_set(const game_clock& trigger_t, unsigned short func_handle)
{
    game_time_trigger* trigger = allocate_game_time_trigger();
    if (trigger == nullptr)
    {
        return false;
    }
    trigger->m_trigger_time = trigger_t;
    trigger->func_handle = func_handle;
    return true;
}

// 0x007C1690 (:733)
bool game_is_paused()
{
    return Game_paused_count > 0;
}

// 0x007C16A0 (:743)
void game_pause(bool mute_music)
{
    if (_InterlockedIncrement((volatile long*)&Game_paused_count) == 1)
    {
        timestamp_pause();
        frametime_pause_animation_timer();
        audiolib_all_sounds_pause(true, mute_music);
    }
}

// 0x007C16D0 (:762)
void game_unpause()
{
    if (_InterlockedDecrement((volatile long*)&Game_paused_count) == 0)
    {
        timestamp_unpause();
        frametime_unpause_animation_timer();
        audiolib_all_sounds_unpause(true, true);
    }
}

// 0x007C1700 (:775)
void game_clock_save(cfile* save_file)
{
    cf_write_int8(Game_clock.m_hour, save_file);
    cf_write_int8(Game_clock.m_minutes, save_file);
    cf_write_int8(Game_clock.m_seconds, save_file);
}

// 0x007C1740 (:783)
void game_clock_load(cfile* save_file)
{
    Game_clock.m_hour = (unsigned char)cf_read_int8(save_file);
    Game_clock.m_minutes = (unsigned char)cf_read_int8(save_file);
    Game_clock.m_seconds = (unsigned char)cf_read_int8(save_file);
    game_clock_update_day_ticks_from_time_of_day();
}

// 0x007C1770 (:792)
void game_clock_set_time(int hours, int minutes, int seconds)
{
    Game_clock.m_hour = (unsigned char)hours;
    Game_clock.m_minutes = (unsigned char)minutes;
    Game_clock.m_seconds = (unsigned char)seconds;
    game_clock_update_day_ticks_from_time_of_day();
}

// 0x007C17C0 (:808)
void game_clock_freeze_ingame_time(bool freeze)
{
    Ingame_time_should_update = !freeze;
}
