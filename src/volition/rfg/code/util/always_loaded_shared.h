// Original: volition/rfg/code/util/always_loaded_shared.h
// Implementation: volition/rfg/code/util/always_loaded_shared.cpp
//
// The always-loaded asset tables: pegs and files that stay resident for the whole game, combined per platform.
#pragma once

#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

struct peg_header;

str_data_hash_table<unsigned char,1024,64,unsigned short>* always_loaded_get_tables_combined(vlib_platform plat); // 0x00B611B0 (:26)
void always_loaded_add_peg(peg_header* header, vlib_platform plat); // 0x00B62780 (:45)
void always_loaded_add_file(const char* filename); // 0x00B628B0 (:57)
