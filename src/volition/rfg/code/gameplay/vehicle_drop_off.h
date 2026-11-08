// Original: volition/rfg/code/gameplay/vehicle_drop_off.h
// Implementation: volition/rfg/code/gameplay/vehicle_drop_off.cpp
//
// Vehicle drop off: in single player, when the player has sprinted on foot for 10 s outside missions and activities
// (low alert, morale at least 30%, not in a safehouse or restricted area), a squad drops off a vehicle near the player;
// polled every 1-2 s, at most once a minute.
#pragma once

void vehicle_drop_off_level_init_pre(); // 0x0075F810 (:301)
void vehicle_drop_off_do_frame(); // 0x007B1170 (:307)
