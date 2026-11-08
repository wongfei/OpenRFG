// Original: volition/rfg/code/gameplay/activities/activity_convoy.h
// Implementation: volition/rfg/code/gameplay/activities/activity_convoy.cpp
//
// Convoy activity: the convoy end nodes, the convoy's vehicles and their state for the interface (destination, target
// distance and health), and the hooks vehicles and squads call (vehicle destroyed, squad left, player hit).
#pragma once

#include "volition/rfg/code/objects/object_convoy.h"

class object_convoy_end_point;
class vector;
class vehicle;

const object_convoy_end_point* find_nearest_convoy_end(int desired_level); // 0x00780990 (:236)
void activity_convoy_at_destination(const object_convoy* convoy); // 0x007AD520 (:2319)
object_convoy* get_activity_convoy_object(); // 0x0076BDD0 (:2327)
convoy_type activity_convoy_get_type(); // 0x00757B90 (:2332)
float activity_convoy_get_nearest_target_dist(); // 0x00781430 (:2337)
float activity_convoy_get_nearest_target_health(); // 0x0076BE00 (:2342)
int activity_convoy_get_total_num_start_vehicles(); // 0x0076BE70 (:2347)
int activity_convoy_get_num_current_vehicles(); // 0x0076BE90 (:2352)
int activity_convoy_get_level(); // 0x00757BB0 (:2357)
bool activity_convoy_start(const object_convoy_end_point* end_point); // 0x007A0F10 (:2363)
bool activity_convoy_vehicle_destroy(const vehicle* vp); // 0x00757BC0 (:2387)
bool activity_convoy_vehicle_in_convoy(const vehicle* vp); // 0x00757C10 (:2404)
bool activity_convoy_player_in_vehicle(); // 0x00757C40 (:2409)
void activity_convoy_squad_left_convoy(unsigned int squad_handle); // 0x00757C50 (:2415)
void activity_convoy_record_player_hit(const vector& hit_location, unsigned int rfg_mover_handle); // 0x0076BEA0 (:2421)
