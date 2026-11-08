// Original: volition/rfg/code/gameplay/activities/activity_air_strike_defense_internal.h
//
// Air strike defense: the activity definitions read from the air strike defense table (Asd_definitions) and the radio
// lines it plays. The activity's code was folded into other files (no functions with line info). Placement inferred.
#pragma once

#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"

class base_activity_data;
class vehicle_rail_definition;

enum asd_radio_line_type
{
    ASD_RADIO_LINE_NONE = -1,
    ASD_RADIO_LINE_START = 0,
    NUM_ASD_RADIO_LINES = 1,
};

// size 0x28
class asd_definition
{
public:
    const char* name; // 0x00
    char* caption; // 0x04
    const base_activity_data* base_data; // 0x08
    const vehicle_rail_definition* vrail_def; // 0x0C
    int num_bombers_to_kill; // 0x10
    float global_moral_change_win; // 0x14
    float global_moral_change_loss; // 0x18
    float global_moral_change_ignored; // 0x1C
    voice_line_handle radio_lines[2]; // 0x20
};
