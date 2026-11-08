// Original: volition/rfg/code/control/control_spectator.cpp
//
// The multiplayer spectator controls (the CM_SPECTATOR control mode, stub_impl.md S5): the free camera's spectator
// submode follows a player, the buttons cycle the viewed player and lock the camera, and the camera transform goes into
// the local player's network state. Compiled in rfg_control_unity.cpp (control.cpp's Active_control_mode).

#include "volition/rfg/code/control/control_spectator.h"

#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_lookaround.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_spectator.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/vehicle/vehicle_interaction_utilities.h"
#include "volition/vlib/code/os/frametime.h"

static unsigned int Spectated_object_handle = 0xFFFFFFFF; // 0x0164B698
static timestamp Switch_from_dead_spectator_timestamp; // 0x0164BBF4 (.data -1: invalid)
static bool Spectator_camera_locked; // 0x01E2A9D2

// The camera's target if it is a player (inlined in several functions of the binary).
static object* control_spectator_target_player()
{
    if (Rfg_camera.target_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(Rfg_camera.target_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return nullptr;
    }
    return objp;
}

// 0x00702BD0 (:52)
// The free camera in its spectator submode, on the player the camera already follows if that one can be spectated.
void control_spectator_enter()
{
    const unsigned int target_handle = Rfg_camera.target_handle;
    if (Spectator_camera_locked)
    {
        Rfg_camera.flags |= 8;
    }
    if (camera_mode_is_on_stack(CAMERA_DEATH_MODE))
    {
        camera_pop_mode(CAMERA_DEATH_MODE);
    }
    camera_set_level_mode(CAMERA_FREE_MODE, true);
    // Magic numbers: a smooth 0.3 s transition.
    camera_free_set_submode(FREE_CAMERA_SUBMODE_SPECTATOR, 1.0f, 0.3f);
    if (multi_spectator_target_is_valid(target_handle))
    {
        control_spectator_set_viewed_player(target_handle);
    }
    else
    {
        control_spectator_next_viewed_player(false);
    }
    Switch_from_dead_spectator_timestamp.invalidate();
}

// 0x006FB390 (:81)
void control_spectator_exit()
{
    Rfg_camera.flags &= ~8u;
    camera_free_set_submode(FREE_CAMERA_SUBMODE_EXTERIOR, 1.0f, 0.3f);
    Cartographer.set_target(player_get_local_player()->get_handle());
}

// 0x00702C70 (:91)
// A dead viewed player is followed for 3 seconds before the next one; the local player's movement input is cleared
// and its camera state is the spectator camera's.
void control_spectator_process()
{
    if (game_is_paused())
    {
        return;
    }
    player* pp = player_get_local_player();
    object* objp = World.object_from_handle(Rfg_camera.target_handle);
    object* hp = (objp != nullptr && objp->get_obj_type() == OT_HUMAN) ? objp : nullptr;
    bool next = objp == nullptr;
    if (objp != nullptr && hp != nullptr && hp->is_killed())
    {
        if (!Switch_from_dead_spectator_timestamp.valid())
        {
            Switch_from_dead_spectator_timestamp.set(3000); // Magic number: 3 s on a dead player
        }
        next = Switch_from_dead_spectator_timestamp.elapsed();
    }
    if (next)
    {
        control_spectator_next_viewed_player(true);
    }
    pp->mp_current_command.forward = 0.0f;
    pp->mp_current_command.sideways = 0.0f;
    pp->mp_camera_orient = Rfg_camera.real_orient;
    pp->mp_camera_pos = Rfg_camera.real_pos;
    float p;
    float b;
    float h;
    Rfg_camera.real_orient.extract_angles(p, b, h);
    float last_p;
    float last_h;
    Rfg_camera.last_orient.extract_angles(last_p, b, last_h);
    pp->mp_camera_angular_velocity.x = (p - last_p) / Frametime;
    pp->mp_camera_angular_velocity.y = (h - last_h) / Frametime;
    if (!Spectator_camera_locked)
    {
        camera_free_set_swing_rate(1.0f);
        const float rot = controls_get_analog(CAA_CAMERA_ROTATE, Player_controller_index);
        const float elev = controls_get_analog(CAA_CAMERA_ELEVATE, Player_controller_index);
        if (Rfg_camera.mode == CAMERA_FREE_MODE)
        {
            camera_free_set_rotation(-rot, CAA_CAMERA_ROTATE);
            camera_free_set_elevation(elev, CAA_CAMERA_ELEVATE);
        }
        else if (Rfg_camera.mode == CAMERA_LOOKAROUND_MODE || Rfg_camera.mode == CAMERA_THIRD_PERSON_MODE)
        {
            camera_lookaround_set_rotation(-elev, rot);
        }
    }
    else
    {
        Rfg_camera.flags |= 8;
    }
    if (controls_process_button_just_pressed(CBA_SPECTATOR_PREV_TARGET, Player_controller_index))
    {
        control_spectator_prev_viewed_player(false); // inlined in the binary
    }
    else if (controls_process_button_just_pressed(CBA_SPECTATOR_NEXT_TARGET, Player_controller_index))
    {
        control_spectator_next_viewed_player(false);
    }
    if (controls_process_button_just_pressed(CBA_SPECTATOR_UNLOCK_CAMERA, Player_controller_index))
    {
        Rfg_camera.flags &= ~8u;
        Spectator_camera_locked = false;
    }
    else if (controls_process_button_just_pressed(CBA_SPECTATOR_LOCK_CAMERA, Player_controller_index))
    {
        Rfg_camera.flags |= 8;
        Spectator_camera_locked = true;
        lookaround_mode_params& look = Rfg_camera.lookaround_params;
        look.rot_h = 0.0f;
        look.rot_p = 0.0f;
        look.rotvel_p = 0.0f;
        look.rotvel_h = 0.0f;
        look.rotvel_added_p = 0.0f;
        look.rotvel_added_h = 0.0f;
    }
}

// 0x006FB3E0 (:193)
// Empty in rfg.exe (retail strips the debug draw).
void control_spectator_render_debug()
{
}

// 0x00700360 (:199)
// The camera follows the player (the local one if the handle is not a player), on foot or in its vehicle.
void control_spectator_set_viewed_player(unsigned int player_handle)
{
    player* pp = nullptr;
    if (player_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(player_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            pp = (player*)objp;
        }
    }
    if (pp == nullptr)
    {
        pp = player_get_local_player();
    }
    camera_level_init_post();
    Spectated_object_handle = pp->get_handle();
    camera_free_set_target(pp);
    vehicle* vp = human_in_vehicle(pp);
    if (vp == nullptr)
    {
        vi_utilities::switch_to_on_foot_camera(pp);
    }
    else
    {
        vi_utilities::switch_to_vehicle_camera(pp, vp);
    }
    camera_snap_to_default_pos();
    // Original: the minimap follows the handle passed in, even when it was not a player.
    Cartographer.set_target(player_handle);
    Switch_from_dead_spectator_timestamp.invalidate();
}

// 0x007003E0 (:234)
// The next player to spectate; without one, the local player when forced or when the camera is not on a player.
void control_spectator_next_viewed_player(bool force)
{
    player* pp = multi_find_next_spectator_target();
    if (pp == nullptr)
    {
        if (!force && control_spectator_target_player() != nullptr)
        {
            return;
        }
        control_spectator_set_viewed_player(player_get_local_player()->get_handle());
        return;
    }
    if (pp->get_handle() == Rfg_camera.target_handle)
    {
        return;
    }
    if (human_in_vehicle(pp) != nullptr && pp->vehicle_handle == Rfg_camera.target_handle)
    {
        return;
    }
    control_spectator_set_viewed_player(pp->get_handle());
}

// 0x00700470 (:276)
void control_spectator_prev_viewed_player(bool force)
{
    player* pp = multi_find_prev_spectator_target();
    if (pp != nullptr)
    {
        if (pp->get_handle() != Rfg_camera.target_handle)
        {
            control_spectator_set_viewed_player(pp->get_handle());
        }
        return;
    }
    if (!force && control_spectator_target_player() != nullptr)
    {
        return;
    }
    control_spectator_set_viewed_player(player_get_local_player()->get_handle());
}

// 0x006FB3F0 (:306)
bool control_spectator_is_spectating_object(const object* target)
{
    return Active_control_mode == CM_SPECTATOR && Spectated_object_handle != 0xFFFFFFFF && target->get_handle() == Spectated_object_handle;
}

// 0x006FB420 (:311)
bool control_spectator_camera_is_locked()
{
    return Spectator_camera_locked;
}
