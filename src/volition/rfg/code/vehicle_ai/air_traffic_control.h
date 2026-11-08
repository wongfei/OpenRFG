// Original: volition/rfg/code/vehicle_ai/air_traffic_control.h
// Implementation: volition/rfg/code/vehicle_ai/air_traffic_control.cpp
//
// Air traffic control: keeps AI flyers apart. Placement inferred.
#pragma once

#include "volition/vlib/code/util/varray.h"

// size 0x34
class air_traffic_control_object
{
public:
    air_traffic_control_object(); // 0x00BCBF00 (:97)
    float request_altitude_clearance(unsigned int flyer_handle, float current_altitude); // 0x00BCBF30 (:106)
    float request_speed_clearance(unsigned int flyer_handle); // 0x00BC8C80 (:159)
private:
    void clear_stale_reservations(); // 0x00BC8B90 (:57)
    void update_reservation_offset(); // 0x00BC8C50 (:84)
public:
    ~air_traffic_control_object(); // 0x00BC8B80

private:
    farray<unsigned int,8> altitude_reservations; // 0x00
    int reservation_offset; // 0x30
};

extern air_traffic_control_object Air_traffic_controller; // 0x016EB8B4
