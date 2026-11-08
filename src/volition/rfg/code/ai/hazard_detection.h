// Original: volition/rfg/code/ai/hazard_detection.h
// Implementation: volition/rfg/code/ai/hazard_detection.cpp
//
// Hazard detection: the physics information (position, velocity, bounds, time to impact) of movers and other objects
// that threaten an npc, used to dodge debris, vehicles and projectiles.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class npc;
class object;

// size 0x38
struct hazard_physics_info
{
    hazard_physics_info() {} // 0x005DD480

    vector velocity; // 0x00
    vector obj_to_npc; // 0x0C
    float distance; // 0x18
    float speed; // 0x1C
    float time_to_pass; // 0x20
    vector pass_point; // 0x24
    float distance_at_pass; // 0x30
    bool will_impact; // 0x34
};

// size 0x10
struct hazard_mover_physics_info
{
    float mass; // 0x00
    float kinetic_energy; // 0x04
    float potential_energy; // 0x08
    float total_energy; // 0x0C
};

void get_hazard_mover_physics_info(hazard_mover_physics_info& info, const npc* np, const object* obj); // 0x005EB3A0 (:713)
void get_hazard_physics_info(hazard_physics_info& info, const npc* np, const object* obj); // 0x00661CA0 (:732)
