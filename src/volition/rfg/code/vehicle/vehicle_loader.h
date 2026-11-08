// Original: volition/rfg/code/vehicle/vehicle_loader.h
// Implementation: volition/rfg/code/vehicle/vehicle_loader.cpp
//
// Loads vehicle_subinfo files (GSP_VEHICLE load callback): patches the pointers, creates the collision models, MOPPs,
// constraints and render data, and registers the subinfo by file name (vehicle_subinfo_find).
#pragma once

class vehicle_subinfo;

bool vehicle_subinfo_load(const char* filename, unsigned int srid, unsigned char* cpu_data, int cpu_size, unsigned char* gpu_data, int gpu_data_size); // 0x00B98E20 (:643)
void vehicle_subinfo_unload(const char* filename, unsigned int srid); // 0x00B91710 (:788)
vehicle_subinfo* vehicle_subinfo_find(const char* filename); // 0x00B91880 (:831)
void vehicle_loader_system_init(); // 0x00B68B90 (:838)
