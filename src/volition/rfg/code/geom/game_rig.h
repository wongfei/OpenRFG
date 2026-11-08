// Original: volition/rfg/code/geom/game_rig.h
// Implementation: volition/rfg/code/geom/game_rig.cpp
//
// Rig lookup by file name and paging in of anim_lib rigs.
#pragma once


class anim_rig;

anim_rig* anim_rig_find(const char* filename); // 0x00808440 (:88)
void game_rig_system_init(); // 0x0080BF60 (:95)
void page_in_animlib_rig(const char* rig_name); // 0x0080B2B0 (:118)
