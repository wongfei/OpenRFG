// Original: volition/rfg/code/gameplay/marauder_raid.h
// Implementation: volition/rfg/code/gameplay/marauder_raid.cpp
//
// Marauder raids: a marauder squad raid, cancelled when its patrol squad reaches the last node (marauder_raid_report_patrol
// is a global event callback); the howl delay is a random 30-180 s timer.
#pragma once

#include "volition/rfg/code/scripting/action_script/global_event.h"

void marauder_raid_level_init_post(); // 0x007A5840 (:370)
event_result marauder_raid_report_patrol(const event_data& in_event, notify_info* info); // 0x0079D6B0 (:390)
void marauder_raid_do_frame(); // 0x007B49D0 (:419)
void marauder_raid_set_howl_delay(); // 0x0075E980 (:445)
