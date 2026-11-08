// Original: volition/vlib/code/util/rule_file.h
// Implementation: volition/vlib/code/util/rule_file.cpp
//
// Build rule files (sources and targets per platform, written by tools).
#pragma once

#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/vlib_core/vlib_core.h"

class string_pool;

bool rule_open(const char* filename, const char* data_type, bool self_referencing); // 0x005CD630 (:58)
bool rule_start_platform(vlib_platform plat); // 0x005B1C00 (:105)
bool rule_add_source(const char* filename, const char* extension_override); // 0x005B1D40 (:149)
bool rule_add_sources(string_pool* files, const char* extension_override, const char* second_extension_override); // 0x005B1E40 (:183)
bool rule_add_target(const char* filename, const char* extension_override); // 0x005B1FB0 (:211)
bool rule_end_platform(); // 0x00591D60 (:243)
bool rule_close(); // 0x005CD710 (:256)
bool slw_get_textures(const char* filename, str_data_hash_table<unsigned char,1024,64,unsigned short>& dst, vlib_platform plat, bool strip_extensions); // 0x005CD770 (:288)
