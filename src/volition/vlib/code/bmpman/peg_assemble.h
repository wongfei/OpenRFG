// Original: volition/vlib/code/bmpman/peg_assemble.h
// Implementation: volition/vlib/code/bmpman/peg_assemble.cpp
//
// Peg assembly: packing bitmaps into peg texture files (tool-side code kept in the game; pasm_data is the assembler
// state).
#pragma once

#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

class mempool_base;
struct peg_header;

// size 0x10
struct pasm_data
{
    pasm_data(); // 0x00563E00 (.h:31)

    unsigned char* cpu_data; // 0x00
    int cpu_size; // 0x04
    unsigned char* gpu_data; // 0x08
    int gpu_size; // 0x0C
};

bool peg_asm_start(vlib_platform plat, mempool_base* temp_pool, mempool_base* final_pool); // 0x005C77A0 (:258)
void peg_asm_add_exclusions(str_data_hash_table<unsigned char,1024,64,unsigned short>* const exclusions); // 0x00563E20 (:292)
void peg_asm_set_error_callback(void (*error_cb)(const char*, ...)); // 0x00563E30 (:297)
bool peg_asm_add(peg_header* peg); // 0x005B5580 (:302)
bool peg_asm_add(const char* filename); // 0x005CE7E0 (:366)
bool peg_asm_add_from_slw(const char* slw_filename, vlib_platform plat); // 0x005CE810 (:387)
bool peg_asm_find_bitmap(const char* filename); // 0x005B5870 (:464)
peg_header* peg_asm_finish(bool pack_data_pointers); // 0x005CFFB0 (:489)
bool peg_asm_finish(pasm_data* data, bool pack_data_pointers); // 0x005CEBA0 (:498)
void peg_asm_abort(); // 0x005C2060 (:672)
