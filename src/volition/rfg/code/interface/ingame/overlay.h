// Original: volition/rfg/code/interface/ingame/overlay.h
// Implementation: volition/rfg/code/interface/ingame/overlay.cpp
//
// The overlay stack: open, close and the active overlays.
#pragma once


class ovl_base;

void overlay_system_init(); // 0x008AA750 (:73)
void overlay_level_init_post(); // 0x008880C0 (:78)
void overlay_level_shutdown(); // 0x008880D0 (:82)
bool overlay_open(const ovl_base* op); // 0x008BF460 (:88)
bool overlay_is_open(); // 0x008AA760 (:109)
bool overlay_is_open(const ovl_base* op); // 0x008AA770 (:117)
bool overlay_is_top(const ovl_base* op); // 0x008AA7A0 (:124)
void overlay_close(const ovl_base* op); // 0x008BF4A0 (:132)
void overlay_process(); // 0x008BF4F0 (:142)
void overlay_render(); // 0x008AA7C0 (:155)
