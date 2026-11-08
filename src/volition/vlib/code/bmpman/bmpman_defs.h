// Original: volition/vlib/code/bmpman/bmpman_defs.h
//
// Texture (peg) structures. A peg file (.cpeg_pc header and entries, .gpeg_pc pixel data; .cvbm_pc/.gvbm_pc for single
// bitmaps) is a peg_header followed by num_bitmaps peg_entry records and their file names; the loader patches the
// et_ptr_offsets in place (bm_fixup_peg_header). bitmap_entry is bmpman's handle slot: the entry it currently shows
// and the animation frame. Placement of peg_header and peg_entry inferred.
#pragma once

#include "volition/vlib/code/util/endian_type.h"

// size 0x18
struct peg_header
{
    int signature; // 0x00
    short version; // 0x04
    short platform; // 0x06
    int dir_block_size; // 0x08
    int data_block_size; // 0x0C
    short num_bitmaps; // 0x10
    short flags; // 0x12
    short total_entries; // 0x14
    short align_value; // 0x16
};

// size 0x30
struct peg_entry
{
    // size 0x2
    struct peg_flag_and_size_union
    {
        unsigned short combined; // 0x00
    };

    peg_entry(const peg_entry& __that); // 0x005C76B0
    peg_entry& operator=(const peg_entry& __that); // 0x005C7890

    et_ptr_offset<unsigned char,0> data; // 0x00
    unsigned short width; // 0x04
    unsigned short height; // 0x06
    unsigned short bm_fmt; // 0x08
    peg_flag_and_size_union source_width; // 0x0A
    unsigned short anim_tiles_width; // 0x0C
    unsigned short anim_tiles_height; // 0x0E
    unsigned short num_frames; // 0x10
    unsigned short flags; // 0x12
    et_ptr_offset<char,1> filename; // 0x14
    peg_flag_and_size_union source_height; // 0x18
    unsigned char fps; // 0x1A
    unsigned char mip_levels; // 0x1B
    unsigned int frame_size; // 0x1C
    et_ptr_offset<peg_entry,1> next; // 0x20
    et_ptr_offset<peg_entry,1> prev; // 0x24
    unsigned int cache[2]; // 0x28
};

// size 0xC
struct peg_file
{
    const char* filename; // 0x00
    int data_size; // 0x04
    peg_header* header; // 0x08
};

// size 0xC
struct bitmap_entry_serialized
{
    et_ptr_offset<char,1> filename_ptr; // 0x00
    et_ptr_offset<peg_entry,1> current_peg_entry; // 0x04
    unsigned short frame_number; // 0x08
    unsigned short user_data; // 0x0A
};

// size 0xC
struct bitmap_entry
{
    void copy_cache_data(const bitmap_entry* src); // 0x00563B80 (.h:226)
    void fill(const bitmap_entry_serialized* src); // 0x005B4FB0 (.h:235)

    const char* filename_ptr; // 0x00
    peg_entry* current_peg_entry; // 0x04
    unsigned short frame_number; // 0x08
    unsigned short user_data; // 0x0A
};
