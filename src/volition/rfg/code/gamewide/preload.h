// Original: volition/rfg/code/gamewide/preload.h
// Implementation: volition/rfg/code/gamewide/preload.cpp
//
// Preloading: subsystems register a loader per file extension (preload_register_extension); preload_all walks the
// preload lists of the base game or a DLC bundle and hands every listed file to the loader of its extension.
#pragma once

int get_ext_index(const char* in_ext); // 0x007C59A0 (:56)
void preload_all(int dlc_id); // 0x007C5BF0 (:150)
void preload_register_extension(const char* in_ext, void (*loader)(const char*)); // 0x007C5CF0 (:186)
