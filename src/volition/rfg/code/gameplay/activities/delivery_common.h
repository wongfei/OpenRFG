// Original: volition/rfg/code/gameplay/activities/delivery_common.h
// Implementation: volition/rfg/code/gameplay/activities/delivery_common.cpp
//
// The delivery definitions (vehicles, routes and times) shared by the delivery activities, read from the delivery table.
#pragma once

#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"

class base_activity_data;
class vehicle_info;
struct xml_element;

static const int NUM_DELIVERY_RADIO_LINE_ENDINGS = 3; // the size of radio_lines

// size 0x4C
class base_delivery_definition_data
{
public:
    base_delivery_definition_data(); // 0x007690B0 (.h:48)
    void reset(); // 0x0075AF10 (:177)
    bool load(xml_element* xml_root); // 0x00786BE0 (:198)

    const char* delivery_name; // 0x00
    unsigned int name_crc; // 0x04
    const char* subtitle; // 0x08
    const vehicle_info* v_info; // 0x0C
    float vehicle_speed; // 0x10
    int vehicle_max_hitpoints; // 0x14
    float vehicle_health; // 0x18
    float end_node_radius; // 0x1C
    int time_to_complete; // 0x20
    int time_to_fail; // 0x24
    int par_time; // 0x28
    gfm_alert_levels alert_level; // 0x2C
    activity_standard_rewards standard_rewards; // 0x30
    voice_line_handle radio_lines[3]; // 0x3C
    const base_activity_data* base_data; // 0x48
};

void load_delivery_data(int dlc_id); // 0x0079A660 (:98)
void delivery_data_unload_dlc(); // 0x0079A860 (:160)
void delivery_common_system_init(); // 0x0079A950 (:315)
const base_delivery_definition_data* delivery_get_def_by_name(const char* delivery_name); // 0x00786E90 (:321)
