// Original: volition/rfg/code/gameplay/gameflow/gfm_structure_defense_coordinator.cpp
//
// gameflow.md GF2. Damage tracking reports for important EDF structures, sent on to the alert level manager four times a
// second. Movers create and destroy them.

#include "volition/rfg/code/gameplay/gameflow/gfm_structure_defense_coordinator.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_damage_report.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/time/timestamp.h"

// Magic number: 250 ms between rounds of reports.
static const int REPORT_PERIOD_MS = 250;

static object_pool<gfm_damage_report, 512, 1> Structure_damage_reports; // 0x022487D0
static timestamp Report_period; // 0x016576A0
static bool Should_report; // 0x02229648

// 0x0078BF00 (:41), rfgm_handle in EDI
static gfm_damage_report* find_report(unsigned int rfgm_handle)
{
    for (gfm_damage_report* report = Structure_damage_reports.get_first(0); report != nullptr; report = Structure_damage_reports.get_next(report))
    {
        if (report->handle == rfgm_handle)
        {
            return report;
        }
    }
    return nullptr;
}

// 0x007739E0 (:55)
static gfm_damage_report* find_report(const rfg_mover* rfgmp)
{
    gfm_damage_report* report = Structure_damage_reports.get_entry(rfgmp->damage_report_handle);
    return report != nullptr && report->handle == rfgmp->get_handle() ? report : nullptr;
}

// 0x007A5490 (:91)
void sdc_level_init_pre()
{
    Structure_damage_reports.clear();
    Report_period.set(0);
    Should_report = false;
}

// 0x0078BF70 (:99)
void sdc_do_frame()
{
    if (!Should_report || !Report_period.elapsed())
    {
        return;
    }
    Report_period.set(REPORT_PERIOD_MS);
    Should_report = false;
    for (gfm_damage_report* report = Structure_damage_reports.get_first(0); report != nullptr; report = Structure_damage_reports.get_next(report))
    {
        if (report->should_report())
        {
            if (report->is_dead())
            {
                alm_report_death(*report);
            }
            else
            {
                alm_report_damage(*report);
            }
        }
        if (Should_report || report->has_unreported_damage())
        {
            Should_report = true;
        }
    }
}

// 0x0078C080 (:134)
int sdc_create_damage_tracking_report(rfg_mover* rfgmp, object_importance_level importance_level, human_teams team)
{
    if (team != HUMAN_TEAM_EDF || (int)importance_level < 1)
    {
        return -1;
    }
    const unsigned int handle = rfgmp->get_handle();
    gfm_damage_report* report = find_report(handle);
    if (report == nullptr)
    {
        report = Structure_damage_reports.alloc(0);
        if (report == nullptr)
        {
            return -1;
        }
    }
    report->reset();
    report->type = OT_MOVER;
    report->importance_level = importance_level;
    report->handle = handle;
    return Structure_damage_reports.get_index(report);
}

// 0x00773A20 (:173)
void sdc_report_new_damage(rfg_mover* rfgmp, float percent_destroyed)
{
    gfm_damage_report* report = find_report(rfgmp);
    if (report == nullptr)
    {
        return;
    }
    report->report_new_damage(rfgmp->is_killed() ? 1.0f : percent_destroyed);
    Should_report = true;
}

// 0x0079CE50 (:193)
void sdc_destroy_report(const rfg_mover* rfgmp)
{
    gfm_damage_report* report = find_report(rfgmp);
    if (report != nullptr)
    {
        Structure_damage_reports.free(report);
    }
}

// 0x00773AB0 (:205)
gfm_damage_report* sdc_get_structure_damage_report(const rfg_mover* rfgmp)
{
    return find_report(rfgmp);
}
