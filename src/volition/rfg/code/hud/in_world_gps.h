// Original: volition/rfg/code/hud/in_world_gps.h
// Implementation: volition/rfg/code/hud/in_world_gps.cpp
//
// The in-world GPS arrows (single player).
#pragma once


struct path_info;
class rl_renderer;

void in_world_gps_system_init(); // 0x008772B0 (:115)
void in_world_gps_register_path(path_info* gps_path); // 0x0084BCA0 (:138)
void in_world_gps_update_render(void* ignored, rl_renderer*); // 0x00869790 (:248)
