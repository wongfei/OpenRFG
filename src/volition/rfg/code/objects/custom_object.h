// Original: volition/rfg/code/objects/custom_object.h
// Implementation: volition/rfg/code/objects/custom_object.cpp
//
// Scripted behaviour of special movers (computers, kiosks, power cores, fire barrels, excavator blades, objects that
// kill or stop effects when moved): custom_object_set_callbacks installs stream-in, damage, destroy and use callbacks
// by object name, and objects that need a per-frame callback are kept in a processing pool
// (custom_object_do_frame_callbacks).
#pragma once

#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/vlib/code/time/timestamp.h"

struct event_data;
struct notify_info;
class object_mover;

// size 0x10
struct custom_object_do_frame_callbacks
{
    custom_object_do_frame_callbacks(); // 0x00A05D80 (.h:29)

    unsigned int object_handle; // 0x00
    notify_info* event_handle; // 0x04
    timestamp timer; // 0x08
    void (*frame_process_callback)(int); // 0x0C
};

bool custom_object_set_callbacks(object_mover* obj); // 0x00AD9670 (:1213)
void custom_object_stop_custom_effects(object_mover* obj); // 0x00A2D060 (:1341)
event_result custom_object_mover_stream_in_callback(const event_data& in_event, notify_info* info); // 0x00A2D0C0 (:1359)
event_result custom_object_mover_damaged_callback(const event_data& in_event, notify_info* info); // 0x00A2D120 (:1382)
event_result custom_object_mover_destroyed_callback(const event_data& in_event, notify_info* info); // 0x00A2D180 (:1405)
event_result custom_object_use_action_callback(const event_data& in_event, notify_info* info); // 0x00A2D1E0 (:1428)
void custom_object_system_init(); // 0x00A05F40 (:1457)
void custom_object_system_shutdown(); // 0x00A05F50 (:1461)
void custom_object_level_init_pre(); // 0x00A2D260 (:1465)
void custom_object_level_shutdown(); // 0x00A05F60 (:1487)
void custom_object_do_frame(); // 0x00A2D2E0 (:1494)
