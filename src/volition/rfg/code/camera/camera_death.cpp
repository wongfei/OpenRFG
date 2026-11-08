// Original: volition/rfg/code/camera/camera_death.cpp
//
// The death camera mode (stub_impl.md S5): single player only, the camera turns toward the dead player's ragdoll while
// the screen fades to a red tint, desaturates and blurs. Compiled in rfg_camera_unity.cpp.
// Multiple-op scalar expressions are computed in double and rounded once, as the SSE2 code does; vector operators
// round per operation.

#include "volition/rfg/code/camera/camera_death.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/render_lib/code/rl_color.h"

static float Player_death_camera_ragdoll_speed = 0.05f; // 0x0164816C, radians per frame

// 0x006C39E0 (:26)
// The camera stays where it is and turns toward the ragdoll (or the player), at most the ragdoll speed per call.
void camera_death_follow_ragdoll()
{
    player* pp = player_get_local_player();
    vector target = pp->pos;
    if (human_is_ragdolled(pp))
    {
        havok_ragdoll_get_pos(pp->ragdoll_idx, target, RAGDOLL_BODY);
    }
    vector to_target = target - Rfg_camera.real_pos;
    const float dist = to_target.mag_and_normalize_safe();
    // The miss: from the point the camera looks at, at the target's distance, to the target.
    vector miss = target - (Rfg_camera.real_pos + Rfg_camera.real_orient.fvec * dist);
    miss.normalize_safe();
    vector axis;
    axis.x = (float)((double)miss.y * (double)to_target.z - (double)miss.z * (double)to_target.y);
    axis.y = (float)((double)miss.z * (double)to_target.x - (double)miss.x * (double)to_target.z);
    axis.z = (float)((double)to_target.y * (double)miss.x - (double)miss.y * (double)to_target.x);
    axis.normalize();
    float ang = vec_delta_ang(to_target, Rfg_camera.real_orient.fvec, axis);
    float step = -Player_death_camera_ragdoll_speed;
    if (ang - -Player_death_camera_ragdoll_speed >= 0.0f)
    {
        step = ang;
    }
    if (step - Player_death_camera_ragdoll_speed >= 0.0f)
    {
        step = Player_death_camera_ragdoll_speed;
    }
    matrix orient(Rfg_camera.real_orient);
    orient.rotate_around_local_vector(axis, -step);
    Rfg_camera.ideal_orient = orient;
}

// 0x006C3D10 (:59)
// Until the player's dead time runs out: follow the ragdoll, and blend the tint, saturation and blur with the time.
void camera_death_process(float step_time, bool last_step_of_frame)
{
    if (Multiplayer)
    {
        return;
    }
    timestamp_percent& dead = player_get_local_player()->dead_timestamp;
    if (dead.valid() && dead.elapsed())
    {
        return;
    }
    camera_death_follow_ragdoll();
    const float pct = dead.percent();
    static const vector damage_tint(1.0f, 0.5f, 0.5f); // a function static in rfg.exe (guard _S1)
    rl_color_float tint;
    tint.green = (float)(((double)damage_tint.y - 1.0) * (double)pct + 1.0);
    tint.red = (float)(((double)damage_tint.x - 1.0) * (double)pct + 1.0);
    tint.alpha = 1.0f;
    tint.blue = (float)(((double)damage_tint.z - 1.0) * (double)pct + 1.0);
    fb_color_correct_modulate_tint(tint);
    // Magic numbers: the saturation goes from 1 to 0.25, the blur from 0.9 to 0.5 (-0.4f is a float constant).
    fb_color_correct_modulate_saturation((float)((double)pct * -0.75 + 1.0));
    if (!game_is_paused())
    {
        game_render_set_camera_frame_override_blur((float)((double)pct * (double)-0.4f + 0.9));
    }
}
