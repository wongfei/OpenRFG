// Original: volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.cpp
//
// The colour correction of the main scene: tint and saturation, each with a modulation on top (stub_impl.md S3). The
// values live in game_render.cpp; rfg.exe inlines game_render's setters and getters here (the tint is copied into
// Scene_main_tint directly), the calls below are those inlined functions.

#include "volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/rfg/code/gamewide/game_render.h"

// 0x007F1440 (:9)
void fb_color_correct_set_tint(const rl_color_float& tint)
{
    game_render_set_main_scene_tint(tint);
}

// 0x007F1450 (:14)
void fb_color_correct_set_saturation(float saturation)
{
    game_render_set_main_scene_saturation(saturation);
}

// 0x007F1470 (:19)
const rl_color_float& fb_color_correct_get_tint()
{
    return game_render_get_main_scene_tint();
}

// 0x007F1480 (:24)
float fb_color_correct_get_saturation()
{
    return game_render_get_main_scene_saturation();
}

// 0x007F1490 (:30)
void fb_color_correct_modulate_tint(const rl_color_float& tint)
{
    game_render_set_main_scene_modulate_tint(tint);
}

// 0x007F14A0 (:36)
void fb_color_correct_modulate_saturation(float saturation)
{
    game_render_set_main_scene_modulate_saturation(saturation);
}

// 0x007F14C0 (:41)
void fb_color_correct_bloom_exposure_factor(float exposure_factor)
{
    game_render_set_bloom_exposure_factor(exposure_factor);
}
