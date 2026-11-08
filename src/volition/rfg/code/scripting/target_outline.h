// Original: volition/rfg/code/scripting/target_outline.h
// Implementation: volition/rfg/code/scripting/target_outline.cpp
//
// Target outlines: objects outlined by scripts and the local player's targeting and locking (rl_outline_compositor).
#pragma once


class vector;

void target_outline_level_init(); // 0x00B2EB80 (:79)
void target_outline_system_init(); // 0x00B24280 (:84)
void target_outline_add_object(unsigned int handle); // 0x00B35390 (:91)
void target_outline_remove_object(unsigned int handle); // 0x00B353C0 (:104)
void target_outline_remove_all(); // 0x00B2EB90 (:113)
void target_outline_set_local_player_targeting_int(int targeting_int); // 0x00B242A0 (:118)
void target_outline_set_local_player_locking_int(int locking_int); // 0x00B242B0 (:123)
void outline_specific_object(unsigned int handle, vector cam_orient_offset_ul, vector cam_orient_offset_ur, float size_override); // 0x00B353E0 (:129)
void target_outline_render(); // 0x00B35BA0 (:250)
