// Original: volition/rfg/code/multi/multi_game/multi_game_flag_capture_zone.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_flag_capture_zone.cpp
//
// Flag capture zones (Capture the Flag): processing and the zone of a team.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"

class multi_object_marker;

void multi_object_flag_capture_zone_process(); // 0x009F0E20 (:14)
multi_object_marker* multi_object_flag_capture_zone_get_zone_by_team(human_teams team); // 0x00998FE0 (:33)
