// Original: volition/rfg/code/gameplay/gameflow/gfm_damage_report.cpp
//
// gameflow.md GF2. A damage report: what an object has taken, what was already reported and when the next report may go.

#include "volition/rfg/code/gameplay/gameflow/gfm_damage_report.h"

// Magic number: a report goes out once a second at most.
static const int REPORT_DELAY_MS = 1000;

// 0x00768F40 (gfm_damage_report.h:17)
gfm_damage_report::gfm_damage_report()
{
    reset();
}

// 0x007542B0 (gfm_damage_report.h:24)
void gfm_damage_report::reset()
{
    handle = 0xFFFFFFFF;
    importance_level = OBJECT_IMPORTANCE_LEVEL_UNKNOWN;
    damage_level = 0.0f;
    damage_level_reported = 0.0f;
    can_report.set(0);
    type = OT_UNDEFINED;
}

// 0x007738C0 (:24)
void gfm_damage_report::update_reported_damage()
{
    damage_level_reported = damage_level;
    reset_report_delay();
}

// 0x0075DBA0 (:35)
void gfm_damage_report::report_new_damage(float new_damage_level)
{
    if (damage_level < new_damage_level)
    {
        damage_level = new_damage_level;
    }
}

// 0x0075DBC0 (:45)
void gfm_damage_report::report_new_damage(float old_damage_level, float new_damage_level)
{
    damage_level = new_damage_level;
    damage_level_reported = old_damage_level;
}

// 0x0075DBE0 (:56)
int gfm_damage_report::get_time_since_damage() const
{
    return damage_level <= 0.0f ? -1 : can_report.time_since_set();
}

// 0x0075DC00 (:68)
void gfm_damage_report::reset_report_delay()
{
    if (can_report.elapsed())
    {
        can_report.set(REPORT_DELAY_MS);
    }
}

// 0x007542E0 (gfm_damage_report.h:48)
bool gfm_damage_report::has_unreported_damage() const
{
    return damage_level_reported < damage_level;
}

// 0x00754300 (gfm_damage_report.h:51)
bool gfm_damage_report::should_report() const
{
    return damage_level_reported < damage_level && can_report.elapsed();
}

// 0x00754330 (gfm_damage_report.h:54)
bool gfm_damage_report::is_dead() const
{
    return 1.0f <= damage_level;
}

// 0x00754350 (gfm_damage_report.h:57)
float gfm_damage_report::get_damage_level() const
{
    return damage_level;
}

// 0x00754360 (gfm_damage_report.h:58)
float gfm_damage_report::get_reported_damage_level() const
{
    return damage_level_reported;
}

// 0x00754370 (gfm_damage_report.h:59)
float gfm_damage_report::get_damage_delta() const
{
    return damage_level - damage_level_reported;
}
