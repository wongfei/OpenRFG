// Original: volition/rfg/code/gameplay/activities/activity_tanks_a_lot.h
// Implementation: volition/rfg/code/gameplay/activities/activity_tanks_a_lot.cpp
//
// Tank activity ("tanks a lot"): whether the local player is in a tank, and the building damage it counts.
#pragma once

bool player_local_in_tank(); // 0x0075A170 (:43)
void activity_tal_building_damaged(unsigned int building_handle, float percent_damaged); // 0x00770950 (:73)
