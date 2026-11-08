// Original: volition/rfg/code/vehicle/vehicle_interaction_utilities.h
// Implementation: volition/rfg/code/vehicle/vehicle_interaction_utilities.cpp
//
// Helpers of the vehicle interaction code: seat and waypoint lookups and the camera switches on entering and exiting.
#pragma once

#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"

class human;
class vehicle;
class vi_enter_data;

// size 0x1
class vi_utilities
{
public:
    static bool can_human_enter_vehicles(human* hp); // 0x00B689A0 (:62)
    static bool vehicle_is_useable(human*, vi_enter_data*, const vehicle*);
    static bool vehicle_seat_is_valid(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B83880 (:87)
    static int waypoint_index_from_seat_index(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B68A00 (:105)
    static bool seat_is_available(const vehicle*, vehicle_seat_index, human*);
    static void switch_to_vehicle_camera(human* hp, vehicle* target_vp); // 0x00B838C0 (:146)
    static void switch_to_on_foot_camera(human* new_target); // 0x00B68A50 (:264)

};
