// Original: volition/rfg/code/geom/decal_info.h
// Implementation: volition/rfg/code/geom/decal_info.cpp
//
// Decal definitions from decal_info.xtbl (decal_info_shared.h parses them) and the name lookups.
// decal_info::flags: 0x1 preloaded, 0x2 material loaded (decal_info_load), 0x4 double sided. A decal is addressed by
// index (decal_info_find) or name hash (decal_info_find_by_hash).
#pragma once

#include "volition/vlib/code/util/hash_table.h"

struct material;
class rl_decal_parameters;

// size 0xB0
struct decal_info
{
    char name[65]; // 0x00
    char material_name[65]; // 0x41
    int life_time; // 0x84
    int fade_time; // 0x88
    float width; // 0x8C
    float length; // 0x90
    float depth; // 0x94
    float depth_fade_start_pct; // 0x98
    float depth_fade_end_pct; // 0x9C
    float slope_fade_start; // 0xA0
    float slope_fade_end; // 0xA4
    material* mat; // 0xA8
    int flags; // 0xAC
};

extern decal_info Decal_info[32]; // 0x025DEDC0
extern int Num_decal_infos; // 0x025EF8DC
extern str_data_hash_table<decal_info*,32,64,unsigned char> Decal_info_name_hash; // 0x025F5FD8

void unused_dcf_decal_info_reload(); // 0x007F2EA0 (:65)
void decal_info_load(unsigned int srid); // 0x0080BDC0 (:98)
void decal_info_unload(unsigned int srid); // 0x00807F90 (:109)
void decal_info_system_init(); // 0x0080BE70 (:122)
void decal_info_system_shutdown(); // 0x007F2F00 (:154)
void decal_info_get(rl_decal_parameters* out_info, const char* decal_info_name); // 0x0080AB60 (:165)
void decal_info_get_by_hash(rl_decal_parameters* out_info, unsigned int decal_info_hash); // 0x0080AB80 (:171)
void decal_info_get(rl_decal_parameters* out_info, int decal_info_id); // 0x007FBE00 (:177)
int decal_info_find(const char* decal_info_name); // 0x00807FF0 (:200)
int decal_info_find_by_hash(unsigned int decal_info_name_hash); // 0x00808050 (:215)
