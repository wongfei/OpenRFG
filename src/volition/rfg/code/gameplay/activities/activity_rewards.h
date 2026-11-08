// Original: volition/rfg/code/gameplay/activities/activity_rewards.h
// Implementation: volition/rfg/code/gameplay/activities/activity_rewards.cpp
//
// The standard activity rewards (control and morale of the district, salvage), read from an activity's table entry
// and applied to the district when the activity is won.
#pragma once


class district;
struct xml_element;

// size 0xC
struct activity_standard_rewards
{
    float control; // 0x00
    float morale; // 0x04
    int salvage; // 0x08
};

bool load_standard_rewards(activity_standard_rewards& rewards_out, xml_element* xml_root); // 0x0075A010 (:36)
void apply_standard_rewards(const activity_standard_rewards& rewards_in, district* dp); // 0x007B2E30 (:72)
