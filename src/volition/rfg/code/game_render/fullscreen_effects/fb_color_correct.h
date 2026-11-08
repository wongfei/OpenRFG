// Original: volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.h
// Implementation: volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.cpp
//
// Color correction full-screen effect.
#pragma once


class rl_color_float;

void fb_color_correct_set_tint(const rl_color_float& tint); // 0x007F1440 (:9)
void fb_color_correct_set_saturation(float saturation); // 0x007F1450 (:14)
const rl_color_float& fb_color_correct_get_tint(); // 0x007F1470 (:19)
float fb_color_correct_get_saturation(); // 0x007F1480 (:24)
void fb_color_correct_modulate_tint(const rl_color_float& tint); // 0x007F1490 (:30)
void fb_color_correct_modulate_saturation(float saturation); // 0x007F14A0 (:36)
void fb_color_correct_bloom_exposure_factor(float exposure_factor); // 0x007F14C0 (:41)
