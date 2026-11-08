// Original: volition/rfg/code/gameplay/gameflow/gfm_damage_report.h
// Implementation: volition/rfg/code/gameplay/gameflow/gfm_damage_report.cpp
//
// The damage report of a structure the EDF defends: the damage level, the part already reported and the report delay.
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/time/timestamp.h"

// size 0x1C
class gfm_damage_report
{
public:
    gfm_damage_report(); // 0x00768F40 (.h:17)
    void reset(); // 0x007542B0 (.h:24)
    void update_reported_damage(); // 0x007738C0 (:24)
    void report_new_damage(float old_damage_level, float new_damage_level); // 0x0075DBC0 (:45)
    void report_new_damage(float new_damage_level); // 0x0075DBA0 (:35)
    int get_time_since_damage() const; // 0x0075DBE0 (:56)
    void reset_report_delay(); // 0x0075DC00 (:68)
    bool has_unreported_damage() const; // 0x007542E0 (.h:48)
    bool should_report() const; // 0x00754300 (.h:51)
    bool is_dead() const; // 0x00754330 (.h:54)
    float get_damage_level() const; // 0x00754350 (.h:57)
    float get_reported_damage_level() const; // 0x00754360 (.h:58)
    float get_damage_delta() const; // 0x00754370 (.h:59)

    unsigned int handle; // 0x00
    object_type type; // 0x04
    object_importance_level importance_level; // 0x08
private:
    float damage_level; // 0x0C
    float damage_level_reported; // 0x10
    timestamp_percent can_report; // 0x14
};
