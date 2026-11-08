// Original: volition/rfg/code/level/territory_shared.h
// Implementation: volition/rfg/code/level/territory_shared.cpp
//
// Territories and their districts, parsed from territory xtbl tables (territory_shared_system_init, plus a DLC table)
// into Territories / Dlc_territories (file static str_data_hash_tables keyed by territory name). A t_district lists the
// grid cells it covers (grid_info: grid id and flags, at most 21), its control/morale defaults and presentation data;
// the districts of a territory form a list (next/prev). The vertical and horizontal mask zone lists name the cells whose
// streaming is masked in the v/h direction (stream_grid, zone_v_mask/zone_h_mask). The runtime district object is
// district (object_district.h, Tier 6).
#pragma once

#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/varray.h"

class mempool_base;

// size 0x1
struct district_flags
{
    bool allow_cough : 1; // 0x00:0
    bool allow_amb_edf_civilian_dump : 1; // 0x00:1
    bool play_capstone_unlocked_lines : 1; // 0x00:2
    bool disable_morale_change : 1; // 0x00:3
    bool disable_control_change : 1; // 0x00:4
};

// size 0x1
struct grid_flags
{
    bool play_marauder_howl : 1; // 0x00:0
    bool marauder_zone : 1; // 0x00:1
};

// size 0x4
struct grid_info
{
    unsigned short gid; // 0x00
    grid_flags flags; // 0x02
};

// size 0xCC
struct t_district
{
    t_district(); // 0x0092B4A0

    const char* name; // 0x00
    const char* liberated_radio_line; // 0x04
    unsigned int localized_name_hash; // 0x08
    unsigned int localized_name_hash_no_ar; // 0x0C
    float control; // 0x10
    float control_max; // 0x14
    float morale; // 0x18
    float morale_max; // 0x1C
    float tech_level; // 0x20
    int marauder_level; // 0x24
    int edf_level; // 0x28
    int edf_progression_level; // 0x2C
    bool liberated; // 0x30
    district_flags flags; // 0x31
    color col; // 0x32
    color vfx_tint; // 0x36
    int disc_layer; // 0x3C
    unsigned char index; // 0x40
    const char* thumbnail; // 0x44
    const char* load_screen_images[5]; // 0x48
    unsigned char num_load_screen_images; // 0x5C
    farray<grid_info,21> list; // 0x60
    t_district* next; // 0xC4
    t_district* prev; // 0xC8
};

// size 0x24
struct territory
{
    territory(); // 0x00919CE0 (.h:116)
    t_district* find_district(unsigned short gid); // 0x00922E30 (:437)
    bool zone_v_mask(unsigned short gid); // 0x0091BDC0 (:453)
    bool zone_h_mask(unsigned short gid); // 0x0091BDF0 (:467)

    const char* name; // 0x00
    float tech_level_default; // 0x04
    float tech_level_max; // 0x08
    unsigned char index; // 0x0C
    t_district* districts; // 0x10
    unsigned short* vertical_mask_zones; // 0x14
    int num_v_mask_zones; // 0x18
    unsigned short* horiz_mask_zones; // 0x1C
    int num_h_mask_zones; // 0x20
};

void territory_shared_system_dlc_shutdown(); // 0x00922E80 (:487)
bool territory_shared_system_init(mempool_base* pool, int dlc_id, unsigned char* table_data); // 0x009327E0 (:500)
t_district* territory_shared_find_district(int territory_index, int district_index); // 0x009329E0 (:580)
territory* territory_shared_find(const char* name); // 0x0092B4C0 (:615)
