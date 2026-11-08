// Original: volition/rfg/code/cutscene/cutscene_options.h
// Implementation: volition/rfg/code/cutscene/cutscene_options.cpp
//
// Shot options: named switches applied at the start or end of a shot (spawning peds and cars, level lights, shadows,
// camera movement, trees, PVS, shader LOD, far clip, fades, letterbox, framebuffer tint, time of day lights, vint
// functions, hidden player components, Bink movies), dispatched through a table by option type. Placement of
// e_cs_option_type inferred.
#pragma once

#include "volition/vlib/code/util/color.h"

struct cutscene_shot;
struct xml_element;

enum e_cs_option_type
{
    CSOPT_NONE = 0,
    CSOPT_BOOL = 1,
    CSOPT_FLOAT = 2,
    CSOPT_STRING = 3,
    CSOPT_COLOR = 4,
    CSOPT_3STATE = 5,
};

// size 0x20
struct cs_shot_option
{
    bool operator==(const char* const _param) const; // 0x00706D50 (:526)
    cs_shot_option(); // 0x00706D80

    const char* option_name; // 0x00
    e_cs_option_type option_type; // 0x04
    bool bParam1; // 0x08
    int nParam1; // 0x0C
    float flParam1; // 0x10
    float flParam2; // 0x14
    const char* szParam1; // 0x18
    color Color; // 0x1C
};

void cutscene_parse_shot_options(xml_element* const shot_xml, cutscene_shot& shot); // 0x0070D790 (:543)
void cutscene_set_options_for_shot(const cutscene_shot& shot); // 0x00709EC0 (:625)
