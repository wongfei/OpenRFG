// Original: volition/rfg/code/vehicle/vehicle_table_parser.h
// Implementation: volition/rfg/code/vehicle/vehicle_table_parser.cpp
//
// Parses vehicles.xtbl (defaults, groups, then one vehicle_info per entry) and the vehicle cover table.
#pragma once

#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"

class mempool;
struct xml_element;
struct vehicle_cover_info;

extern vehicle_info Vehicle_group_info[16]; // 0x0386C0B8
extern int Num_vehicle_groups; // 0x0377006C
extern string_pool Vehicle_info_string_pool; // 0x0381A5E0
extern const char* INVALID_VEHICLE_FAMILY_ID; // 0x0377002C
extern farray<vehicle_cover_info*,5> Dlc_vehicle_cover_info_list; // 0x0165E288
extern unsigned int Bone_indicies[74]; // 0x0376F680

const char* get_xml_filename(xml_element* xml_filename_root); // 0x00B68F70 (:208)
bool vehicle_table_parsed(); // 0x00B6D370 (:2920)
bool vehicle_parse_table(farray<vehicle_info,163>& vehicle_info_pool, mempool* pool, int dlc_id); // 0x00BAD240 (:2928)
bool vehicle_parse_cover_table(object_pool<vehicle_cover_info,163,1>& vehicle_cover_pool, int dlc_id); // 0x00BA2F90 (:2973)
void unused_dcf_vehicle_slots(); // 0x00B6D380 (:3019)
