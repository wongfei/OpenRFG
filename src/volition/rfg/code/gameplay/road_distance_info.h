// Original: volition/rfg/code/gameplay/road_distance_info.h
// Implementation: volition/rfg/code/gameplay/road_distance_info.cpp
//
// Road distance infos: per road (and point index), the distance the player has travelled along it, collected at
// level_init_post.
#pragma once

#include "volition/vlib/code/util/varray.h"

// size 0xC
struct road_distance_info
{
    unsigned int road_handle; // 0x00
    float travel_distance; // 0x04
    unsigned short index; // 0x08
};

void road_info_level_init_post(); // 0x007901B0 (:304)
float road_info_player_travel_distance(unsigned int road_handle, unsigned short index); // 0x007901C0 (:313)
const farray<road_distance_info,256>& road_info_get_list(); // 0x00790220 (:339)
