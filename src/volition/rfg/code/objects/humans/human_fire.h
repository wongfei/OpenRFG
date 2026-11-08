// Original: volition/rfg/code/objects/humans/human_fire.h
// Implementation: volition/rfg/code/objects/humans/human_fire.cpp
//
// Humans on fire: a fixed set of burning slots (human_fire), ignited by fire damage and updated every frame until
// the fire stops or the human dies.
#pragma once


class human;

// size 0x14
struct human_fire
{
    unsigned int object_handle; // 0x00
    unsigned int killer_handle; // 0x04
    unsigned int left_leg_fire_handle; // 0x08
    unsigned int right_leg_fire_handle; // 0x0C
    unsigned int torso_fire_handle; // 0x10
};

void human_fire_stop(unsigned int object_handle); // 0x00A55B20 (:156)
void human_fire_system_init(); // 0x00A0A4F0 (:183)
bool human_fire_ignite(human* hp_to_ignite, human* killer_hp); // 0x00AD46F0 (:205)
void human_fire_do_frame(); // 0x00AD4A70 (:340)
bool human_is_on_fire(human* hp); // 0x00A0A580 (:424)
