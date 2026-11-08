// Original: volition/rfg/code/hud/minimap_icon_info.h
// Implementation: volition/rfg/code/hud/minimap_icon_info.cpp
//
// The minimap icon table (minimap_icon_info.xtbl): bitmap, scale, flags and colors of each map element.
#pragma once

#include "volition/rfg/code/hud/minimap.h"

// size 0x1
struct minimap_icon_info_flags
{
    minimap_icon_info_flags(); // 0x0083F540 (.h:105)
    void set(const minimap_icon_info_flags& other); // 0x0083F550 (.h:114)

    unsigned char visible_on_fs_map : 1; // 0x00:0
    unsigned char visible_over_fow : 1; // 0x00:1
    unsigned char is_activity_icon : 1; // 0x00:2
    unsigned char is_objective_icon : 1; // 0x00:3
    unsigned char is_target_icon : 1; // 0x00:4
    unsigned char display_caption : 1; // 0x00:5
    unsigned char visible_when_zoomed : 1; // 0x00:6
};

// size 0xC8
struct minimap_icon_info
{
    void init(); // 0x0084E640 (:87)
    minimap_icon_info(); // 0x0084E630

    unsigned int bitmap_id; // 0x00
    minimap_icon_info_flags flags; // 0x04
    char icon_name[32]; // 0x05
    char caption[32]; // 0x25
    char description[128]; // 0x45
};

extern minimap_icon_info Minimap_icon_infos[59]; // 0x02BD03D8

unsigned int get_minimap_bitmap_id(int which); // 0x0084E660 (:96)
const char* get_minimap_bitmap_icon_name(int which); // 0x0084E680 (:104)
bool minimap_is_icon_mission_start(minimap_elements element); // 0x0084E6A0 (:112)
const char* get_minimap_bitmap_caption(int which); // 0x0084E6B0 (:118)
const char* get_minimap_bitmap_description(int which); // 0x0084E6D0 (:126)
const char* get_unknown_mission_caption(); // 0x0084E6F0 (:136)
const char* get_unknown_mission_description(); // 0x0084E700 (:142)
void refresh_minimap_icon_bitmap_ids(); // 0x0084E7E0 (:192)
void minimap_load_icon_table(int dlc_id); // 0x0084E810 (:201)
bool minimap_is_directional_icon(minimap_elements element); // 0x0084EB10 (:305)
bool minimap_element_visible_on_fs_map(minimap_elements element); // 0x0084EB20 (:321)
bool minimap_element_is_visible_when_zoomed(minimap_elements element); // 0x0084EB40 (:331)
bool minimap_element_visible_over_fow(minimap_elements element); // 0x0084EB60 (:342)
bool minimap_element_display_caption(minimap_elements element); // 0x0084EB80 (:352)
bool minimap_element_is_activity_icon(minimap_elements element); // 0x0084EBA0 (:364)
bool minimap_element_is_objective_icon(minimap_elements element); // 0x0084EBC0 (:375)
bool minimap_element_is_target_icon(minimap_elements element); // 0x0084EBE0 (:386)
