// Original: volition/rfg/code/vehicle/vehicle_cover.h
// Implementation: volition/rfg/code/vehicle/vehicle_cover.cpp
//
// Cover around vehicles: stopped vehicles register for cover checks, which create temporary cover nodes along the
// vehicle from the cover node definitions of its mesh (vehicle_cover_info).
#pragma once

#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/varray.h"

class vehicle;

// size 0x2C
struct vehicle_cover_node_definition
{
    vehicle_cover_node_definition(); // 0x00B6D1B0

    vector offset; // 0x00
    vector relative_fvec; // 0x0C
    cover_node_flags cover_flags; // 0x18
    float def_left_arc; // 0x1C
    float def_right_arc; // 0x20
    float off_left_arc; // 0x24
    float off_right_arc; // 0x28
};

// size 0x1CC
struct vehicle_cover_info
{
    vehicle_cover_info(); // 0x00B953F0

    const char* mesh_name; // 0x00
    farray<vehicle_cover_node_definition,10> node_info; // 0x04
};

// stub_impl.md S5 (downloadable_content.cpp frees a DLC's vehicle cover infos; placement inferred).
extern object_pool<vehicle_cover_info,163,1> Vehicle_cover_info_pool; // 0x0383D3A0

void vehicle_cover_process_start(); // 0x00B66300 (:105)
bool vehicle_cover_can_process(); // 0x00B66310 (:110)
bool vehicle_cover_can_check(); // 0x00B66320 (:114)
void vehicle_cover_register_check(); // 0x00B66330 (:119)
void vehicle_cover_clear_registrations(); // 0x00B66340 (:124)
int vehicle_cover_num_vehicles_checking(); // 0x00B66360 (:130)
void vehicle_cover_regenerate_all(); // 0x00B8EA70 (:137)
void vehicle_cover_info_init(int dlc_id); // 0x00BAAA60 (:152)
const vehicle_cover_info* vehicle_cover_get_info_for_mesh(const char* mesh_name); // 0x00B8EB00 (:166)
unsigned int vehicle_cover_create_node(const vehicle* vp, const vehicle_cover_node_definition& def); // 0x00B7C050 (:187)
void vehicle_cover_check_node(vehicle* vp, const vehicle_cover_node_definition& def); // 0x00B8EB70 (:251)
