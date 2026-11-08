// Original: volition/render_lib/code/rl_morph_data.h
// Implementation: volition/render_lib/code/rl_morph_data.cpp
//
// rl_morph_data, serialized morph target data.
#pragma once

#include "volition/vlib/code/util/endian_type.h"

struct cfile;

// size 0x14
struct rl_morph_data
{
    int signature; // 0x00
    int version; // 0x04
    int target_format; // 0x08
    int num_targets; // 0x0C
    et_ptr_offset<unsigned char,0> targets; // 0x10
};

rl_morph_data* rl_morph_data_load(unsigned char* cpu_buf, unsigned char* gpu_buf, int& cpu_offset, int& gpu_offset); // 0x0053FA80 (:41)
bool rl_morph_data_write(cfile* cpu_file, cfile* gpu_file, rl_morph_data* data); // 0x004E9A50 (:173)
