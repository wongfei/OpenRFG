// Original: volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h
// Implementation: volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.cpp
//
// Accumulation blur full-screen effect (frame blending, used for camera effects).
#pragma once


class vector;

void fb_accum_blur_set_enable(bool enable); // 0x007F0F50 (:29)
void fb_accum_blur_set_percentage(float percentage); // 0x007F0F90 (:35)
void fb_accum_blur_change_percentage(float delta_percentage); // 0x007F1000 (:45)
void fb_accum_blur_set_cap(float cap); // 0x007F1070 (:55)
void fb_accum_blur_update(); // 0x007F1080 (:66)
void fb_accum_blur_set_radial_blur(const vector& world_pos, float strength, float radius, float time, float distance_fade); // 0x007F1330 (:108)
void fb_accum_blur_update_radial_blur(const vector& world_pos); // 0x007F13D0 (:126)
void fb_accum_blur_kill_radial_blur(); // 0x007F1410 (:137)
void fb_accum_blur_system_init(); // 0x007F1430 (:144)
