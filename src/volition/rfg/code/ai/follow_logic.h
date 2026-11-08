// Original: volition/rfg/code/ai/follow_logic.h
// Implementation: volition/rfg/code/ai/follow_logic.cpp
//
// Follow logic: the per-frame movement of a follower on foot or in a vehicle towards its formation slot behind the
// leader.
#pragma once

#include "volition/rfg/code/objects/humans/human_enums.h"
#include "volition/vlib/code/math/vector3.h"

class npc;
class vehicle;

// size 0x14
struct follow_parameters
{
    follow_parameters(); // 0x005E93F0 (:29)

    vector desired_dir; // 0x00
    float desired_speed; // 0x0C
    float cur_speed; // 0x10
};

bool ai_do_follow_frame(npc* np, unsigned int formation_handle, human_stances stance); // 0x006A0100 (:968)
bool ai_vehicle_do_follow_frame(npc* np, vehicle* vp, unsigned int formation_handle); // 0x006A07E0 (:1177)
