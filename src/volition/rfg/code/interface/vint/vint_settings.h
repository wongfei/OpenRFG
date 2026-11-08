// Original: volition/rfg/code/interface/vint/vint_settings.h
//
// The game's vint project settings: the include pegs and include sheets ui_init allocates (10 each) and the mouse scale
// and offset from the project's resolution mode.
#pragma once

#include "volition/vlib/code/util/varray.h"

// size 0x20
struct Vint_settings
{
    Vint_settings(); // 0x008B4060
    ~Vint_settings(); // 0x008B4070

    base_array<char*> include_bmp_pegs; // 0x00
    base_array<char*> include_bmp_sheets; // 0x10
};

extern Vint_settings* gVint_settings; // 0x01664B48
