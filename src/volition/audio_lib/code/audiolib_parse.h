// Original: volition/audio_lib/code/audiolib_parse.h
// Implementation: volition/audio_lib/code/audiolib_parse.cpp
//
// The settings table parse (audiolib_settings.xtbl into Audiolib_settings).
#pragma once

#include "volition/audio_lib/code/audiolib_enums.h"

struct xml_element;

audiolib_result audiolib_internal_parse_settings(xml_element* root); // 0x00488C60 (:45)
audiolib_result audiolib_internal_parse_near_far_settings(xml_element* root); // 0x004891E0 (:250)
audiolib_result audiolib_parse_settings_xtbl(const char* filename, bool first_pass); // 0x00489350 (:297)
