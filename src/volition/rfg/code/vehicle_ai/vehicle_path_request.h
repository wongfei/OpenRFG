// Original: volition/rfg/code/vehicle_ai/vehicle_path_request.h
// Implementation: volition/rfg/code/vehicle_ai/vehicle_path_request.cpp
//
// Vehicle nav mesh path requests, serviced a few per frame.
#pragma once


class vehicle;

void vehicle_navmesh_path_requests_level_init_pre(); // 0x00BC7580 (:82)
bool vehicle_navmesh_path_can_pathfind(unsigned int vehicle_handle); // 0x00BC75A0 (:94)
void vehicle_navmesh_path_requests_process(); // 0x00BCA860 (:102)
void vehicle_navmesh_path_add_request(const vehicle* vp); // 0x00BC75B0 (:150)
void vehicle_navmesh_path_service_request(unsigned int vehicle_handle); // 0x00BC75D0 (:159)
