// Original: volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.cpp
//
// The accumulation (camera) blur and the radial blur. The camera code turns the camera's motion blur on and adds to its
// percentage (capped); the radial blur of effects_play_radial_blur fades over its duration and with the camera's
// distance from its position. Both only reach game_render's motion blur parameters, which game_render_setup_frame
// copies into the main render camera.
//
// VFX.md FX5: ported whole. In rfg.exe nothing draws them: rl_motion_blur::composite, the only reader of the camera's
// motion blur parameters, is never called (rl_motion_blur.cpp).

#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// ---- globals (placement inferred: only this file uses them)
bool Motion_blur_enabled_accum = true; // 0x0165F089
float Motion_blur_scale = 1.0f; // 0x0165F1F8
float Fb_radial_blur_strength = 0.01f; // 0x0165F200
float Fb_radial_blur_distance_fade = 1.0f; // 0x0165F204
float Fb_radial_blur_time = 1.0f; // 0x0165F208
float Fb_radial_blur_radius = 1.0f; // 0x0165F20C

// ---- file-static globals
static float Radial_blur_distance_fade = 1.0f; // 0x0165F1FC
static bool Accum_blur_enabled; // 0x0253D3D0
static float Accum_blur_percentage; // 0x0253D3D4
static float Accum_blur_cap; // 0x0253D3D8
static float Radial_blur_strength_accum; // 0x0253D3DC
static float Radial_blur_radius_accum; // 0x0253D3E4
static float Radial_blur_time_start; // 0x0253D3E8
static float Radial_blur_time; // 0x0253D3EC
static vector Radial_blur_pos; // 0x0253D470

// Not in the binary: the game time in seconds as rfg.exe computes it (the unsigned milliseconds times 0.001 in x87
// double precision, stored as a float).
static float fb_accum_blur_time_seconds()
{
    // Magic number: milliseconds to seconds.
    return (float)((double)Frametime_total_milliseconds * 0.001);
}

// 0x007F0F50 (:29)
// The camera's motion blur on only while Motion_blur_enabled_accum allows it.
void fb_accum_blur_set_enable(bool enable)
{
    if (enable && Motion_blur_enabled_accum)
    {
        Accum_blur_enabled = true;
        game_render_set_motion_blur_enabled(true);
        return;
    }
    Accum_blur_enabled = false;
    game_render_set_motion_blur_enabled(false);
}

// 0x007F0F90 (:35)
// The scaled percentage, capped when a cap is set, as the camera's motion blur amount.
void fb_accum_blur_set_percentage(float percentage)
{
    Accum_blur_percentage = Motion_blur_scale * percentage;
    if (Accum_blur_cap != 0.0f && Accum_blur_cap < Accum_blur_percentage)
    {
        Accum_blur_percentage = Accum_blur_cap;
    }
    game_render_set_motion_blur_amount(Accum_blur_percentage);
}

// 0x007F1000 (:45)
// As fb_accum_blur_set_percentage, adding to the current percentage (the product and sum in double precision).
void fb_accum_blur_change_percentage(float delta_percentage)
{
    Accum_blur_percentage = (float)((double)Motion_blur_scale * (double)delta_percentage + (double)Accum_blur_percentage);
    if (Accum_blur_cap != 0.0f && Accum_blur_cap < Accum_blur_percentage)
    {
        Accum_blur_percentage = Accum_blur_cap;
    }
    game_render_set_motion_blur_amount(Accum_blur_percentage);
}

// 0x007F1070 (:55)
void fb_accum_blur_set_cap(float cap)
{
    Accum_blur_cap = cap;
}

// 0x007F1080 (:66)
// The radial blur's parameters for this frame (from the gameplay frame): its strength fades out over its duration
// (1 - elapsed fraction, 0 once nearly done) and with the camera's distance over the fade distance, scaled by the
// tunables; below 0.0001 it is off. The screen position follows the blur's world position while it projects (the last
// projected position otherwise).
//
// Notes (decompiled behaviour, kept as is): after fb_accum_blur_kill_radial_blur the elapsed fraction is t / 0 (+inf,
// or NaN on the frame it starts), which the clamps turn into a strength of 0.
void fb_accum_blur_update()
{
    // Magic numbers: the clamps to [0, 1] and the 0.0001 cut-off are rfg.exe's (compared in double precision).
    float t = (float)((double)(fb_accum_blur_time_seconds() - Radial_blur_time_start) / ((double)Fb_radial_blur_time * (double)Radial_blur_time));
    if (!(t - 0.0f >= 0.0f))
    {
        t = 0.0f;
    }
    if (t - 1.0f >= 0.0f)
    {
        t = 1.0f;
    }
    float time_fade;
    if ((double)t > 0.0001)
    {
        time_fade = 1.0f - t;
    }
    else
    {
        time_fade = 0.0f;
    }
    const float dx = Radial_blur_pos.x - Rfg_camera.real_pos.x;
    const float dy = Radial_blur_pos.y - Rfg_camera.real_pos.y;
    const float dz = Radial_blur_pos.z - Rfg_camera.real_pos.z;
    const float dist_sq = (float)((double)dy * (double)dy + (double)dx * (double)dx + (double)dz * (double)dz);
    const float dist = (float)sqrt((double)dist_sq);
    // Magic number: 1e-05 keeps the division finite with a zero fade distance.
    float d = (float)((double)dist / ((double)Fb_radial_blur_distance_fade * (double)Radial_blur_distance_fade + 1e-05));
    if (!(d - 0.0f >= 0.0f))
    {
        d = 0.0f;
    }
    if (d - 1.0f >= 0.0f)
    {
        d = 1.0f;
    }
    float strength = (float)((double)((1.0f - d) * time_fade) * ((double)Fb_radial_blur_strength * (double)Radial_blur_strength_accum));
    if (!((double)strength > 0.0001))
    {
        strength = 0.0f;
    }
    static vector current_screen_pos; // 0x0253D490 (guard $S1 0x0253D49C)
    vector screen;
    if (gr_screen_coords_from_world_coords(Radial_blur_pos, &screen.x, &screen.y, &screen.z, 1))
    {
        current_screen_pos = screen;
    }
    const vector2 position(current_screen_pos.x, current_screen_pos.y);
    game_render_set_radial_blur_params(position, strength, Fb_radial_blur_radius * Radial_blur_radius_accum);
}

// 0x007F1330 (:108)
// Starts a radial blur at a position that projects onto the screen (behind the eye it is ignored).
void fb_accum_blur_set_radial_blur(const vector& world_pos, float strength, float radius, float time, float distance_fade)
{
    float screen_x;
    float screen_y;
    float screen_z;
    if (!gr_screen_coords_from_world_coords(world_pos, &screen_x, &screen_y, &screen_z, 1))
    {
        return;
    }
    Radial_blur_pos = world_pos;
    Radial_blur_strength_accum = strength;
    Radial_blur_radius_accum = radius;
    Radial_blur_time_start = fb_accum_blur_time_seconds();
    Radial_blur_time = time;
    Radial_blur_distance_fade = distance_fade;
}

// 0x007F13D0 (:126)
// The playing blur's effect moved: its position follows while it projects.
void fb_accum_blur_update_radial_blur(const vector& world_pos)
{
    float screen_x;
    float screen_y;
    float screen_z;
    if (gr_screen_coords_from_world_coords(world_pos, &screen_x, &screen_y, &screen_z, 1))
    {
        Radial_blur_pos = world_pos;
    }
}

// 0x007F1410 (:137)
void fb_accum_blur_kill_radial_blur()
{
    Radial_blur_time = 0.0f;
    Radial_blur_time_start = 0.0f;
}

// 0x007F1430 (:144)
// Empty in rfg.exe.
void fb_accum_blur_system_init()
{
}
