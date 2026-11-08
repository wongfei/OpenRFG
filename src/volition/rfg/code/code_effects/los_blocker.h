// Original: volition/rfg/code/code_effects/los_blocker.h
// Implementation: volition/rfg/code/code_effects/los_blocker.cpp
//
// Line of sight blockers: temporary spheres (smoke, dust clouds; LOS_blocker_pool, 25) that block AI vision by a
// percentage.
#pragma once

#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

// size 0x20
class los_blocker
{
public:
    void get_pos(vector& real_pos); // 0x006E4470 (:45)
    los_blocker(); // 0x006E5E20

    vector pos; // 0x00
    float radius; // 0x0C
    float opacity; // 0x10
    int flags; // 0x14
    timestamp life_span; // 0x18
    unsigned int parent_handle; // 0x1C
};

void los_blocker_system_init(); // 0x006E4530 (:63)
void los_blocker_level_init_pre(); // 0x006F6150 (:68)
los_blocker* los_blocker_create(const vector& pos, const float radius, const float opacity, const int life_span, unsigned int parent_handle); // 0x006ED480 (:87)
void los_blocker_destroy(los_blocker* lbp); // 0x006F1290 (:122)
void los_blocker_do_frame(); // 0x006F12A0 (:129)
float los_blocker_percent_blocked(const vector& start, const vector& end); // 0x006ED5A0 (:148)
void los_blocker_render_debug(); // 0x006ED760 (:172)
