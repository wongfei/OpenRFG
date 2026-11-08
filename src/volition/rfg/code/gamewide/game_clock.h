// Original: volition/rfg/code/gamewide/game_clock.h
// Implementation: volition/rfg/code/gamewide/game_clock.cpp
//
// The game clock: the in-game date and time (Game_clock, advanced by game_clock_update during gameplay and saved with the
// game), the time of day derived from it, game time triggers (a script function called at a given game time), and
// pausing the game. Gamewide_game_clock is the read-only pointer the time of day reads. Placement of the trigger and the
// partitioned time of day inferred.
#pragma once


struct cfile;

enum game_clock_time_of_day
{
    GC_INVALID = -1,
    GC_DAY = 0,
    GC_NIGHT = 1,
    GC_TIME_OF_DAY_COUNT = 2,
};

enum game_clock_tod_partitioned
{
    GC_EARLY_DAY = 0,
    GC_MID_DAY = 1,
    GC_LATE_DAY = 2,
    GC_EARLY_NIGHT = 3,
    GC_MID_NIGHT = 4,
    GC_LATE_NIGHT = 5,
    NUM_TOD_PARTITIONS = 6,
};

// size 0x10
struct game_clock
{
    unsigned short m_year; // 0x00
    unsigned char m_month; // 0x02
    unsigned char m_day; // 0x03
    unsigned char m_hour; // 0x04
    unsigned char m_minutes; // 0x05
    unsigned char m_seconds; // 0x06
    unsigned char m_day_of_week; // 0x07
    float m_time_scale; // 0x08
    unsigned int m_curr_day_ticks; // 0x0C
};

// size 0x14
struct game_time_trigger
{
    game_clock m_trigger_time; // 0x00
    unsigned short func_handle; // 0x10
};

extern const game_clock* Gamewide_game_clock; // 0x0165CBE4

void unused_dcf_game_clock_set(); // 0x007CD4D0 (:126)
void game_clock_update_day_ticks_from_time_of_day(); // 0x007C0B30 (:190)
void game_clock_init(); // 0x007C11C0 (:425)
void game_clock_set(const game_clock* game_clock_state); // 0x007C1460 (:509)
const game_clock* game_clock_get(); // 0x007C1480 (:516)
void game_clock_update(float delta_time, bool delta_is_game_time); // 0x007D9930 (:526)
void game_clock_increment_seconds(game_clock& clock, float num_game_seconds); // 0x007CD5E0 (:548)
void game_clock_increment_days(game_clock& clock, int num_days); // 0x007C1490 (:582)
int game_clock_compare_dates(const game_clock* clock1, const game_clock* clock2); // 0x007C1560 (:624)
const char* game_clock_get_day_of_week_str(); // 0x007C15A0 (:646)
int game_clock_get_time_hour(); // 0x007C15B0 (:654)
game_clock_tod_partitioned game_clock_get_tod_partitioned(); // 0x007C15C0 (:665)
game_clock_time_of_day game_clock_get_time_of_day(); // 0x007C15E0 (:688)
bool game_time_trigger_set(const game_clock& trigger_t, unsigned short func_handle); // 0x007C1640 (:712)
extern volatile int Game_paused_count; // 0x02533880, game_pause nesting count
bool game_is_paused(); // 0x007C1690 (:733)
void game_pause(bool mute_music); // 0x007C16A0 (:743)
void game_unpause(); // 0x007C16D0 (:762)
void game_clock_save(cfile* save_file); // 0x007C1700 (:775)
void game_clock_load(cfile* save_file); // 0x007C1740 (:783)
void game_clock_set_time(int hours, int minutes, int seconds); // 0x007C1770 (:792)
void game_clock_freeze_ingame_time(bool freeze); // 0x007C17C0 (:808)
