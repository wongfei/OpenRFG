// Original: volition/rfg/code/scripting/managed_script_group.h
// Implementation: volition/rfg/code/scripting/managed_script_group.cpp
//
// Managed script groups: named groups of scripts that are activated and terminated together (handles by name).
#pragma once

#include "volition/vlib/code/util/hash_table.h"

// size 0x8
struct managed_script_group_data
{
    unsigned char flags; // 0x00
    unsigned int mark; // 0x04
};

// stub_impl.md S5 (downloadable_content.cpp removes a DLC's script groups; placement inferred).
extern int Managed_script_handle_free; // 0x03611248
extern str_data_hash_table<int,106,32,unsigned char>* Managed_script_table; // 0x03611414

void managed_script_system_init(); // 0x00B3DA10 (:24)
int managed_script_create_handle(const char* group_name); // 0x00B33640 (:39)
int managed_script_get_handle(const char* group_name); // 0x00B336C0 (:59)
const char* managed_script_get_name(int group_handle); // 0x00B3A6B0 (:70)
