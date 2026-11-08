// Original: volition/rfg/code/gameplay/activities/activity_courier_chase.h
// Implementation: volition/rfg/code/gameplay/activities/activity_courier_chase.cpp
//
// Courier chase activity: the courier (spawning, escapes, abandoning the vehicle, scripted escape) and its squad.
#pragma once


class npc;
class object_courier_end_point;
class vehicle;

void unused_dcf_make_courier_stuck(); // 0x00797000 (:140)
void activity_cc_courier_damaged_start(); // 0x00757E50 (:1430)
bool activity_cc_is_courier(const npc* np); // 0x00757E70 (:1437)
bool activity_cc_is_active_courier(const npc* np); // 0x00757EB0 (:1458)
bool activity_cc_most_recent_courier_escaped(); // 0x00757F00 (:1468)
bool activity_cc_abandon_vehicle(const vehicle* vp); // 0x00757F10 (:1474)
bool activity_cc_spawn_courier(const object_courier_end_point* end_point); // 0x007B2050 (:1489)
void activity_courier_spawned_this_frame(); // 0x00757F20 (:1499)
void activity_cc_scripted_escape(); // 0x00757FA0 (:1511)
unsigned int activity_cc_get_courier_squad_handle(); // 0x00757FC0 (:1569)
void activity_cc_get_rewards(float& control, float& morale); // 0x00757FD0 (:1578)
