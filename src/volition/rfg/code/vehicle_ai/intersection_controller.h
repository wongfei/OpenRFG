// Original: volition/rfg/code/vehicle_ai/intersection_controller.h
// Implementation: volition/rfg/code/vehicle_ai/intersection_controller.cpp
//
// Intersection controllers: queue AI vehicles at intersections and let them through in turn, adjusting their
// acceleration.
#pragma once


class vehicle;

// size 0x38
struct intersection_controller
{
    unsigned short next; // 0x00
    unsigned short prev; // 0x02
    unsigned int road1_handle; // 0x04
    unsigned int road2_handle; // 0x08
    unsigned char road1_point; // 0x0C
    unsigned char road2_point; // 0x0D
    unsigned char queue_size; // 0x0E
    unsigned int vehicle_queue[10]; // 0x10
};

void intersection_controller_system_init(); // 0x00BC3360 (:694)
void intersection_controller_level_init(); // 0x00BC3370 (:708)
void intersection_controller_do_frame(); // 0x00BCC980 (:722)
void intersection_controller_process_vehicle(vehicle* vp); // 0x00BCCA20 (:746)
void intersection_controller_handle_vehicle_destroyed(vehicle* vp); // 0x00BCCAB0 (:782)
void intersection_controller_update_vehicle_accel_decel(const vehicle* vp, float& overall_speed, float& overall_decel); // 0x00BCCAF0 (:798)
bool intersection_controller_vehicle_at_top_of_queue(const vehicle* vp); // 0x00BC36C0 (:893)
