// Original: volition/rfg/code/gameplay/activities/activity_mission_vrail.h
// Implementation: volition/rfg/code/gameplay/activities/activity_mission_vrail.cpp
//
// Mission vehicle rails: the scripted chase and patrol encounters missions start and cancel.
#pragma once


class object_patrol;
class object_squad;
class vector;
class vehicle;

bool activity_mission_vrail_start(int index, const object_patrol* patrol, const vector& drop_off_pos, object_squad* squad, vehicle* vp); // 0x007AE1F0 (:566)
bool activity_mission_vrail_start(int index, const object_patrol* patrol, const vector& drop_pos); // 0x007AE270 (:584)
void activity_mission_vrail_cancel(); // 0x007AE2F0 (:601)
