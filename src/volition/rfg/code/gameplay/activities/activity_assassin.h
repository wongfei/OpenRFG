// Original: volition/rfg/code/gameplay/activities/activity_assassin.h
// Implementation: volition/rfg/code/gameplay/activities/activity_assassin.cpp
//
// Assassination activity: the bodyguard query used by the AI.
#pragma once


class npc;

npc* activity_assassin_get_closest_bodyguard(const npc* bureaucrat, float* dist_sq); // 0x00796710 (:60)
