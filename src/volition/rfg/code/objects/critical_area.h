// Original: volition/rfg/code/objects/critical_area.h
// Implementation: volition/rfg/code/objects/critical_area.cpp
//
// The critical area of a mission: a circle around an object (one at a time, saved with the game) the player must
// stay in while a mission is active. Leaving it plays a warning (a voice line of the radio persona or a HUD message)
// and starts an 8 second script timer; when the timer runs out the mission fails.
#pragma once

#include "volition/vlib/code/time/timestamp.h"

struct cfile;

// size 0x14
struct critical_area
{
    critical_area(); // 0x00A05AC0

    unsigned int handle; // 0x00
    float radius; // 0x04
    unsigned int flags; // 0x08
    unsigned int tmr_handle; // 0x0C
    timestamp voice_line_delay; // 0x10
};

void critical_area_level_init(); // 0x00A05AD0 (:67)
void critical_area_timer_create(); // 0x00A05B00 (:77)
void critical_area_timer_delete(); // 0x00A05B20 (:88)
void critical_area_delete_internal(); // 0x00A05B40 (:99)
void critical_area_delete(); // 0x00A05B80 (:110)
bool critical_area_create(unsigned int handle, float radius); // 0x00A05BD0 (:125)
void critical_area_activate(); // 0x00A05C70 (:148)
void critical_area_do_frame(); // 0x00A7A950 (:162)
void critical_area_save(cfile* f); // 0x00A05CB0 (:258)
void critical_area_load(cfile* f); // 0x00A05D00 (:267)
void critical_area_level_shutdown(); // 0x00A05D40 (:275)
