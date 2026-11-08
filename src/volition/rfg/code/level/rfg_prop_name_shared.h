// Original: volition/rfg/code/level/rfg_prop_name_shared.h
//
// Name of a property in a prop block: the string and its case-insensitive CRC (crc_stri), computed once in the
// constructor. Property names are global prop_name constants (OBJ_PROP_*, ZONE_PROP_*, ...).
#pragma once

// size 0x8
class prop_name
{
public:
    prop_name(const char* _str); // 0x00A05450 (.h:27)

    const char* str; // 0x00
    unsigned int crc; // 0x04
};
