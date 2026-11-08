// Original: volition/rfg/code/destruction/rfg_stress.h
// Implementation: volition/rfg/code/destruction/rfg_stress.cpp
//
// Structural stress evaluation of buildings: one mover at a time is evaluated over several frames (states idle, init
// stage 1 and 2, collect objects, process object, process siblings, finish; driven by rfg_do_frame_pre). Movers are
// queued by stress_evaluation_start (high or low priority queue, guarded by Stress_mover_queue_wrapper).
// stress_evaluation_info accumulates the supported mass, centre of mass and force per subpiece (linked by index).
// Placement of stress_evaluation_info inferred.
#pragma once

#include "volition/vlib/code/math/vector3.h"

class rfg_mover;

// size 0x1C
struct stress_evaluation_info
{
    stress_evaluation_info(); // 0x00718090

    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    vector total_com; // 0x04
    float total_mass; // 0x10
    float total_force; // 0x14
    int total_yield; // 0x18
};

void stress_evaluation_system_init(); // 0x007212E0 (:1539)
void stress_evaluation_level_init_pre(); // 0x00729E90 (:1554)
bool stress_evaluation_start(rfg_mover* mover, bool sort_links, bool priority); // 0x007212F0 (:1578)
bool stress_evaluation_stop(rfg_mover* mover); // 0x0072A200 (:1675)
bool stress_evaluation_poke(rfg_mover* mover); // 0x00721600 (:1707)
bool stress_evaluation_is_active(unsigned int handle); // 0x0072A3D0 (:1738)
bool stress_evaluation_is_active(rfg_mover* mover); // 0x00721860 (:1743)
void stress_evaluation_do_frame(); // 0x00737BB0 (:1775)
bool stress_evaluation_set_mode(const char* mode_name); // 0x00718710 (:1858)
