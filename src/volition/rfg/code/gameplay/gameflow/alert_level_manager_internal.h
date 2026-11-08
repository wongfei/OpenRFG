// Original: volition/rfg/code/gameplay/gameflow/alert_level_manager_internal.h
//
// Alert level tables: the alert value parameters per reported event and the base values per alert level.
#pragma once

#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/objects/object_type.h"

struct xml_element;

// size 0x20
class alert_value_parameters
{
public:
    alert_value_parameters(object_type type_, const char* type_name_); // 0x00772EC0 (.h:45)
    void reset(); // 0x0075D130 (alert_level_manager.cpp:283)
    void load(xml_element* xml_root); // 0x0075D160 (alert_level_manager.cpp:294)

    const object_type type; // 0x00
    const char* type_name; // 0x04
    float imp_multipliers[4]; // 0x08
    float damage_mult; // 0x18
    float death_value; // 0x1C
};

// size 0x24
class base_alert_level_values
{
public:
    base_alert_level_values(gfm_alert_levels level_, const char* min_value_tag_, const char* max_value_tag_, const char* decay_rate_tag_); // 0x00772E80 (.h:10)
    void reset(); // 0x0075D030 (.h:23)
    void load(xml_element* xml_root); // 0x0075D050 (alert_level_manager.cpp:252)

    const gfm_alert_levels level; // 0x00
    float min_value; // 0x04
    float max_value; // 0x08
    float lower_bound; // 0x0C
    float upper_bound; // 0x10
    float decay_rate; // 0x14
private:
    const char* min_value_tag; // 0x18
    const char* max_value_tag; // 0x1C
    const char* decay_rate_tag; // 0x20
};

// Deviation from rfg.exe: external for the courier and convoy files, which reach it through its unity build.
extern base_alert_level_values Alert_level_ranges[NUM_ALERT_LEVELS]; // 0x01657598
