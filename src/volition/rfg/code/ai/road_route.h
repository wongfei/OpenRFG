// Original: volition/rfg/code/ai/road_route.h
// Implementation: volition/rfg/code/ai/road_route.cpp
//
// Road routes: a route over road splines (road segments with their points), created by road pathfinding from a start
// to a destination.
#pragma once

#include "volition/vlib/code/util/varray.h"

class object_path_road;
class vector;

// size 0x8
struct road_spline_identifier
{
    object_path_road* road_spline; // 0x00
    unsigned short point; // 0x04
};

// size 0x8
class road_segment_info
{
public:
    road_segment_info(unsigned int new_road_handle, unsigned short new_start_index, unsigned short new_end_index); // 0x005F1420 (:250)
    road_segment_info() : road_handle(0xFFFFFFFF), start_index(0xFFFF), end_index(0xFFFF) {} // 0x005FB660 (.h:19)
    void reset(); // 0x005F1440 (:258)

    unsigned int road_handle; // 0x00
    unsigned short start_index; // 0x04
    unsigned short end_index; // 0x06
};

bool extract_road_segment(road_segment_info& new_segment, farray<road_spline_identifier,512>& road_waypoint_list); // 0x00641180 (:134)
bool road_route_create(const road_spline_identifier& start_road_id, const road_spline_identifier& destination_road_id, base_array<road_segment_info>& route_result); // 0x0066AAB0 (:272)
bool road_route_create(const vector& start_pos, const vector& end_pos, base_array<road_segment_info>& route_result); // 0x0066AB70 (:308)
